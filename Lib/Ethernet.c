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

void Ethernet_ChecksumAdd(uint16_t *checksum, uint16_t word)
{
#if __GNUC__ < 5
       *checksum += word;
       if(*checksum < word)
               (*checksum)++;
#else
       if(__builtin_uadd_overflow(*checksum, word, checksum))
               (*checksum)++;
#endif
}

uint16_t Ethernet_Checksum(const void *data, uint16_t length)
{
	uint16_t *Words = (uint16_t *)data;
	uint16_t length16 = length / 2;
	uint16_t Checksum = 0;

	while(length16--)
		Ethernet_ChecksumAdd(&Checksum, *(Words++));

	if(length & 1)
		Ethernet_ChecksumAdd(&Checksum, *Words & CPU_TO_BE16(0xFF00));

	return ~Checksum;
}

static uint8_t Ethernet_WriteHeader(uint8_t packet[], const MAC_Address_t *destinationMAC, Ethertype_t ethertype)
{
	Ethernet_Header_t *Ethernet = (Ethernet_Header_t *)packet;

	Ethernet->Destination	= *destinationMAC;
	Ethernet->Source	= OwnMACAddress;
	Ethernet->EtherType	= ethertype;

	return sizeof(Ethernet_Header_t);
}

int8_t Ethernet_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP, Ethertype_t ethertype)
{
	const MAC_Address_t *MAC = ARP_searchMAC(destinationIP);
	if(MAC == NULL)
		return -ARP_GenerateBroadcastRequest(packet, destinationIP);

	return Ethernet_WriteHeader(packet, MAC, ethertype);
}

uint8_t Ethernet_GenerateBroadcastReply(uint8_t packet[], Ethertype_t ethertype)
{
	return Ethernet_WriteHeader(packet, &BroadcastMACAddress, ethertype);
}
