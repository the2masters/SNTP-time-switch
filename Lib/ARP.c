#include "ARP.h"
#include "IP.h"
#include "helper.h"
#include <avr/cpufunc.h>
#include <string.h>
#include <avr/io.h>

typedef enum
{
	ARP_OPERATION_REQUEST	= CPU_TO_BE16(0x0001),
	ARP_OPERATION_REPLY	= CPU_TO_BE16(0x0002),
} ARP_Operation_t;

#define ARP_HARDWARE_ETHERNET	CPU_TO_BE16(0x0001)

typedef struct
{
	IP_Hostpart_t IP;
	MAC_Address_t MAC;
} ATTR_PACKED ARP_TableEntry;

static ARP_TableEntry ARP_Table[10] = {{0}};

const MAC_Address_t* ARP_searchMAC(const IP_Address_t *IP)
{
	const MAC_Address_t *retVal = NULL;
	for(uint8_t i = 0; i < ARRAY_SIZE(ARP_Table); i++)
		if(ARP_Table[i].IP == IP_getHost(IP))
		{
			retVal = &ARP_Table[i].MAC;
			break;
		}
	return retVal;
}

static uint16_t ARP_GeneratePacket(void *packet, ARP_Operation_t direction, const MAC_Address_t *targetMAC, const IP_Address_t *targetIP)
{
	ARP_Header_t *ARP = &((ARP_Packet_t *)packet)->ARP;

	ARP->HardwareType	= ARP_HARDWARE_ETHERNET;
	ARP->ProtocolType	= ETHERTYPE_IPV4;
	ARP->HLEN		= sizeof(MAC_Address_t);
	ARP->PLEN		= sizeof(IP_Address_t);
	ARP->Operation		= direction;
	ARP->SenderMAC		= OwnMACAddress;
	ARP->SenderIP		= OwnIPAddress;
	ARP->TargetMAC		= *targetMAC;
	ARP->TargetIP		= *targetIP;

	return Ethernet_GenerateHeader(packet, targetMAC, ETHERTYPE_ARP, sizeof(ARP_Header_t));
}

uint16_t ARP_ProcessPacket(void *packet, uint16_t length)
{
	if(length != sizeof(ARP_Packet_t))
		return 0;

	ARP_Header_t* ARP = &((ARP_Packet_t *)packet)->ARP;

	switch(ARP->Operation)
	{
		case ARP_OPERATION_REQUEST:
			if(ARP->TargetIP != OwnIPAddress) break;

			// Get destination MAC and IP and recreate ARP Header
			MAC_Address_t destinationMAC = ARP->SenderMAC;
			IP_Address_t destinationIP = ARP->SenderIP;
			return ARP_GeneratePacket(packet, ARP_OPERATION_REPLY, &destinationMAC, &destinationIP);

		case ARP_OPERATION_REPLY:
			if(!IP_compareNet(&OwnIPAddress, &ARP->SenderIP)) break;

			bool IPneu = true;
			const IP_Hostpart_t IP_Hostpart = IP_getHost(&ARP->SenderIP);

			for(uint8_t i = 0; i < ARRAY_SIZE(ARP_Table); i++)
				if(ARP_Table[i].IP == IP_Hostpart)
				{
					ARP_Table[i].MAC = ARP->SenderMAC;
					IPneu = false;
					break;
				}
			if(IPneu)
			{
				static uint8_t writePosition = 0;
				ARP_Table[writePosition].IP = IP_Hostpart;
				ARP_Table[writePosition].MAC = ARP->SenderMAC;
				if(writePosition < ARRAY_SIZE(ARP_Table))
					writePosition++;
				else
					writePosition = 0;
			}
			break;

	}
	return 0;
}

uint16_t ARP_GenerateRequest(void *packet, const IP_Address_t *destinationIP)
{
	return ARP_GeneratePacket(packet, ARP_OPERATION_REQUEST, &BroadcastMACAddress, destinationIP);
}
