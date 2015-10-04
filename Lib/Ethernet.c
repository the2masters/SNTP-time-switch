#include "Ethernet.h"
#include "ARP.h"
#include "IP.h"

const MAC_Address_t BroadcastMACAddress = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

__attribute__((pure, always_inline))
static inline bool MAC_compare(const MAC_Address_t *a, const MAC_Address_t *b)
{
	return memcmp(a, b, sizeof(MAC_Address_t)) == 0;
}

uint16_t Ethernet_ProcessPacket(void *packet, uint16_t length)
{
	Ethernet_Header_t *Ethernet = &((Ethernet_Packet_t *)packet)->Ethernet;

	if(!(MAC_compare(&Ethernet->Destination, &OwnMACAddress) ||
	     MAC_compare(&Ethernet->Destination, &BroadcastMACAddress)))
		return 0;

	switch (Ethernet->EtherType)
	{
		case ETHERTYPE_ARP:
			return ARP_ProcessPacket(packet, length);
		case ETHERTYPE_IPV4:
			return IP_ProcessPacket(packet, length);
	}
	return 0;
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

int8_t Ethernet_GenerateHeaderIP(uint8_t packet[], const IP_Address_t *destinationIP, Ethertype_t ethertype)
{
	const MAC_Address_t *MAC = ARP_searchMAC(destinationIP);
	if(MAC == NULL)
		return -ARP_GenerateRequest(packet, destinationIP);

	return Ethernet_GenerateHeader(packet, MAC, ethertype);
}

uint8_t Ethernet_GenerateHeader(uint8_t packet[], const MAC_Address_t *destinationMAC, Ethertype_t ethertype)
{
	Ethernet_Header_t *Ethernet = (Ethernet_Header_t *)packet;

	Ethernet->Destination	= *destinationMAC;
	Ethernet->Source	= OwnMACAddress;
	Ethernet->EtherType	= ethertype;

	return sizeof(Ethernet_Header_t);
}
