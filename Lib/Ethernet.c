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
	Ethernet_Header_t *Ethernet_Header = (Ethernet_Header_t *)packet;

	if(!(MAC_compare(&Ethernet_Header->Destination, &OwnMACAddress) ||
	     MAC_compare(&Ethernet_Header->Destination, &BroadcastMACAddress)))
		return 0;

	switch (Ethernet_Header->EtherType)
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

uint16_t Ethernet_GenerateHeaderIP(void *packet, const IP_Address_t *destinationIP, Ethertype_t ethertype, uint16_t payloadLength)
{
	const MAC_Address_t *MAC = ARP_searchMAC(destinationIP);
	if(MAC == NULL)
		return ARP_GenerateRequest(packet, destinationIP);

	return Ethernet_GenerateHeader(packet, MAC, ethertype, payloadLength);
}

uint16_t Ethernet_GenerateHeader(void* packet, const MAC_Address_t *destinationMAC, Ethertype_t ethertype, uint16_t payloadLength)
{
	Ethernet_Header_t *Ethernet_Header = (Ethernet_Header_t *)packet;

	Ethernet_Header->Destination	= *destinationMAC;
	Ethernet_Header->Source		= OwnMACAddress;
	Ethernet_Header->EtherType	= ethertype;

	return sizeof(Ethernet_Header_t) + payloadLength;
}
