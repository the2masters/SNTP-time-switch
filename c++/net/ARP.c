#include "ARP.h"
#include "Ethernet.h"
#include "helper.h"

typedef struct
{
	uint16_t	HardwareType;
	uint16_t	ProtocolType;

	uint8_t		HLEN;
	uint8_t		PLEN;
	uint16_t	Operation;

	MAC_Address_t	SenderMAC;
	IP_Address_t	SenderIP;
	MAC_Address_t	TargetMAC;
	IP_Address_t	TargetIP;
} ATTR_PACKED ARP_Header_t;

typedef struct
{
	IP_Hostpart_t IP;
	MAC_Address_t MAC;
} ATTR_PACKED ARP_TableEntry;

static ARP_TableEntry ARP_Table[10] = {{0}};

static uint8_t ARP_WriteHeader(uint8_t packet[], ARP_Operation_t operation, const MAC_Address_t *destinationMAC, const IP_Address_t *destinationIP)
{
	ARP_Header_t *ARP = (ARP_Header_t *)packet;

	ARP->HardwareType	= CPU_TO_BE16(ARP_HARDWARE_ETHERNET);
	ARP->ProtocolType	= CPU_TO_BE16(ETHERTYPE_IPV4);
	ARP->HLEN		= sizeof(MAC_Address_t);
	ARP->PLEN		= sizeof(IP_Address_t);
	ARP->Operation		= operation;
	ARP->TargetMAC		= *destinationMAC;	// Can be an alias of ARP->SenderMAC
	ARP->TargetIP		= *destinationIP;	// Can be an alias of ARP->SenderIP
	COPY_ARRAY(6, ARP->SenderMAC, MAC_OWN);
	COPY_ARRAY(6, ARP->SenderIP, IP_OWN);

	return sizeof(ARP_Header_t);
}

bool ARP_ProcessPacket(uint8_t packet[], uint16_t length)
{
	ARP_Header_t *ARP = (ARP_Header_t *)packet;

	if(ARP->TargetIP != OwnIPAddress)
		return false;

	switch(ARP->Operation)
	{
		case CPU_TO_BE16(ARP_OPERATION_REQUEST):
			ARP->Operation	= CPU_TO_BE16(ARP_OPERATION_REPLY);
			ARP->TargetMAC	= ARP->SenderMAC;
			ARP->TargetIP	= ARP->SenderIP;
			ARP->SenderMAC	= OwnMACAddress;
			ARP->SenderIP	= OwnIPAddress;
			return true;

		case CPU_TO_BE16(ARP_OPERATION_REPLY):
			if(!IP_compareNet(&ARP->SenderIP, &OwnIPAddress))
				return false;

			bool IPneu = true;
			const IP_Hostpart_t IP_Hostpart = IP_getHost(&ARP->SenderIP);

			for(uint8_t i = 0; i < ARRAY_SIZE(ARP_Table); i++)
			{
				if(ARP_Table[i].IP == IP_Hostpart)
				{
					ARP_Table[i].MAC = ARP->SenderMAC;
					IPneu = false;
					break;
				}
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
			return false;
		default:
			return false;

	}
}

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

uint8_t ARP_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP)
{
	uint8_t offset = Ethernet_GenerateBroadcast(packet, CPU_TO_BE16(ETHERTYPE_ARP));

	return offset + ARP_WriteHeader(packet + offset, CPU_TO_BE16(ARP_OPERATION_REQUEST), &BroadcastMACAddress, destinationIP);
}
