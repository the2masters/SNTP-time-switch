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
	ARP_Packet_t *ARP_Packet = &((ARP_FullPacket_t *)packet)->ARP;

	ARP_Packet->HardwareType	= ARP_HARDWARE_ETHERNET;
	ARP_Packet->ProtocolType	= ETHERTYPE_IPV4;
	ARP_Packet->HLEN		= sizeof(MAC_Address_t);
	ARP_Packet->PLEN		= sizeof(IP_Address_t);
	ARP_Packet->Operation		= direction;
	ARP_Packet->TargetMAC		= *targetMAC;	// Has to be placed before SenderMAC
	ARP_Packet->TargetIP		= *targetIP;	// Has to be placed before SenderIP
	ARP_Packet->SenderMAC		= OwnMACAddress;
	ARP_Packet->SenderIP		= OwnIPAddress;

	return Ethernet_GenerateHeader(packet, &ARP_Packet->TargetMAC, ETHERTYPE_ARP, sizeof(ARP_Packet_t));
}

uint16_t ARP_ProcessPacket(void *packet, uint16_t length)
{
	if(length != sizeof(ARP_FullPacket_t))
		return 0;

	ARP_Packet_t* ARP_Packet = &((ARP_FullPacket_t *)packet)->ARP;

	switch(ARP_Packet->Operation)
	{
		case ARP_OPERATION_REQUEST:
			if(ARP_Packet->TargetIP != OwnIPAddress) break;
			return ARP_GeneratePacket(packet, ARP_OPERATION_REPLY, &ARP_Packet->SenderMAC, &ARP_Packet->SenderIP);

		case ARP_OPERATION_REPLY:
			if(!IP_compareNet(&OwnIPAddress, &ARP_Packet->SenderIP)) break;

			bool IPneu = true;
			const IP_Hostpart_t IP_Hostpart = IP_getHost(&ARP_Packet->SenderIP);

			for(uint8_t i = 0; i < ARRAY_SIZE(ARP_Table); i++)
				if(ARP_Table[i].IP == IP_Hostpart)
				{
					ARP_Table[i].MAC = ARP_Packet->SenderMAC;
					IPneu = false;
					break;
				}
			if(IPneu)
			{
				static uint8_t writePosition = 0;
				ARP_Table[writePosition].IP = IP_Hostpart;
				ARP_Table[writePosition].MAC = ARP_Packet->SenderMAC;
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
