#include "IP.h"
#include "UDP.h"
#include "ICMP.h"
#include "Ethernet.h"

#define IP_VERSION_IHL			(0x40 | sizeof(IP_Header_t)/4)
#define DEFAULT_TTL			64
#define IP_FLAGS_DONTFRAGMENT		0x4000

typedef struct
{
	uint8_t		Version_IHL;
	uint8_t		TypeOfService;
	uint16_t	Length;

	uint16_t	Identification;
	uint16_t	FlagsFragment;

	uint8_t		TTL;
	uint8_t		Protocol;
	uint16_t	Checksum;

	IP_Address_t	SourceAddress;
	IP_Address_t	DestinationAddress;

	uint8_t		data[];
} ATTR_PACKED IP_Header_t;

void IP_ChecksumAdd(uint16_t *checksum, uint16_t word)
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

uint16_t IP_Checksum(const void *data, uint16_t length)
{
	const uint16_t *Words = (const uint16_t *)data;
	uint16_t length16 = length / 2;
	uint16_t Checksum = 0;

	while(length16--)
		IP_ChecksumAdd(&Checksum, *(Words++));

	if(length & 1)
		IP_ChecksumAdd(&Checksum, *Words & CPU_TO_BE16(0xFF00));

	return ~Checksum;
}

static uint8_t IP_WriteHeader(uint8_t packet[], IP_Protocol_t protocol, const IP_Address_t *destinationIP, uint16_t payloadLength)
{
	IP_Header_t *IP = (IP_Header_t *)packet;

	IP->Version_IHL		= IP_VERSION_IHL;
	IP->TypeOfService	= 0;
	IP->Length		= cpu_to_be16(sizeof(IP_Header_t) + payloadLength);
	IP->Identification	= 0;
	IP->FlagsFragment	= CPU_TO_BE16(IP_FLAGS_DONTFRAGMENT);
	IP->TTL			= DEFAULT_TTL;
	IP->Protocol		= protocol;
	IP->DestinationAddress	= *destinationIP;	// Can be an alias of IP->SourceAddress
	IP->SourceAddress	= OwnIPAddress;
	IP->Checksum		= 0;			// First set it to 0, then calculate correct checksum
	IP->Checksum		= IP_Checksum(IP, sizeof(IP_Header_t));

	return sizeof(IP_Header_t);
} 

bool IP_ProcessPacket(uint8_t packet[], uint16_t length)
{
	// Minimum length is already checked
	IP_Header_t *IP = (IP_Header_t *)packet;

	// Remove optional padding
	uint16_t ip_length = be16_to_cpu(IP->Length);

	if(IP->Version_IHL != IP_VERSION_IHL ||
	  (IP->FlagsFragment & CPU_TO_BE16(0x3FFF) ||
	   ip_length > length))
		return false;

	if(IP->DestinationAddress != OwnIPAddress && IP->DestinationAddress != BroadcastIPAddress)
		return false;

	length = ip_length - sizeof(IP_Header_t);
	bool reflect;
	switch (IP->Protocol)
	{
		case IP_PROTOCOL_ICMP:
			reflect = ICMP_ProcessPacket(IP->data);
			break;
		case IP_PROTOCOL_UDP:
			reflect = UDP_ProcessPacket(IP->data, &IP->SourceAddress, length);
			break;
		default:
			return false;
	}

	if(reflect)	// rewrite Header, replace destination with sourceAddress
	{
		IP_WriteHeader(packet, IP->Protocol, &IP->SourceAddress, length);
		return true;
	} else {
		return false;
	}
}

int8_t IP_GenerateUnicast(uint8_t packet[], IP_Protocol_t protocol, const IP_Address_t *destinationIP, uint8_t payloadLength)
{
	const IP_Address_t *routerIP = destinationIP;
	if(!(IP_compareNet(&OwnIPAddress, destinationIP)))
		routerIP = &RouterIPAddress;

	int8_t offset = Ethernet_GenerateUnicast(packet, routerIP, CPU_TO_BE16(ETHERTYPE_IPV4));
	if(offset <= 0)
		return offset;

	return offset + IP_WriteHeader(packet + offset, protocol, destinationIP, payloadLength);
}

uint8_t IP_GenerateBroadcast(uint8_t packet[], IP_Protocol_t protocol, uint8_t payloadLength)
{
	uint8_t offset = Ethernet_GenerateBroadcast(packet, CPU_TO_BE16(ETHERTYPE_IPV4));

	return offset + IP_WriteHeader(packet + offset, protocol, &BroadcastIPAddress, payloadLength);
}
