#include "Ethernet.h"
#include "IP.h"
#include "ARP.h"

typedef struct
{
	MAC_Address_t	Destination;
	MAC_Address_t	Source;
	uint16_t	EtherType;
	uint8_t		data[];
} ATTR_PACKED Ethernet_Header_t;

const MAC_Address_t BroadcastMACAddress = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

__attribute__((pure, always_inline))
static inline bool MAC_compare(const MAC_Address_t *a, const MAC_Address_t *b)
{
	return memcmp(a, b, sizeof(MAC_Address_t)) == 0;
}

bool Ethernet_ProcessPacket(uint8_t packet[], uint16_t length)
{
	if(length < ETHERNET_FRAME_SIZE_MIN)
		return false;

	Ethernet_Header_t *Ethernet = (Ethernet_Header_t *)packet;

	if(!(MAC_compare(&Ethernet->Destination, &OwnMACAddress) ||
	     MAC_compare(&Ethernet->Destination, &BroadcastMACAddress)))
		return false;

	length -= sizeof(Ethernet_Header_t);
	bool reflect;
	switch (Ethernet->EtherType)
	{
		case ETHERTYPE_ARP:
			reflect = ARP_ProcessPacket(Ethernet->data, length);
			break;
		case ETHERTYPE_IPV4:
			reflect = IP_ProcessPacket(Ethernet->data, length);
			break;
		default:
			return false;
	}

	if(reflect)
	{
		Ethernet->Destination = Ethernet->Source;
		Ethernet->Source = OwnMACAddress;
		return true;
	} else {
		return false;
	}
}

static uint8_t Ethernet_WriteHeader(uint8_t packet[], const MAC_Address_t *destinationMAC, Ethertype_t ethertype)
{
	Ethernet_Header_t *Ethernet = (Ethernet_Header_t *)packet;

	Ethernet->Destination	= *destinationMAC;
	Ethernet->Source	= OwnMACAddress;
	Ethernet->EtherType	= ethertype;

	return sizeof(Ethernet_Header_t);
}

int8_t Ethernet_GenerateUnicast(uint8_t packet[], const IP_Address_t *destinationIP, Ethertype_t ethertype)
{
	const MAC_Address_t *MAC = ARP_searchMAC(destinationIP);
	if(MAC != NULL)
	{
		return Ethernet_WriteHeader(packet, MAC, ethertype);
	} else {
		return -ARP_GenerateRequest(packet, destinationIP);
	}
}

uint8_t Ethernet_GenerateBroadcast(uint8_t packet[], Ethertype_t ethertype)
{
	return Ethernet_WriteHeader(packet, &BroadcastMACAddress, ethertype);
}
