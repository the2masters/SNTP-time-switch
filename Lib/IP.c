#include "IP.h"
#include "UDP.h"
#include "ICMP.h"
#include <avr/cpufunc.h>
#include "helper.h"
//TODO: Checksum sollte ausgegliedert werden
#include "Ethernet.h"

#define IP_HEADERLENGTHVERSION		(0x40 | sizeof(IP_Header_t)/4)
#define DEFAULT_TTL			64
#define IP_FLAGS_DONTFRAGMENT		CPU_TO_BE16(0x4000)

typedef struct
{
	uint8_t		HeaderLengthVersion;
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

static uint8_t IP_WriteHeader(uint8_t packet[], IP_Protocol_t protocol, const IP_Address_t *destinationIP, uint16_t payloadLength)
{
	IP_Header_t *IP = (IP_Header_t *)packet;

	IP->HeaderLengthVersion	= IP_HEADERLENGTHVERSION;
	IP->TypeOfService	= 0;
	IP->Length		= cpu_to_be16(sizeof(IP_Header_t) + payloadLength);
	IP->Identification	= 0;
	IP->FlagsFragment	= IP_FLAGS_DONTFRAGMENT;
	IP->TTL			= DEFAULT_TTL;
	IP->Protocol		= protocol;
	IP->DestinationAddress	= *destinationIP;	// Can be an alias of IP->SourceAddress
	IP->SourceAddress	= OwnIPAddress;
	IP->Checksum		= 0;			// First set it to 0, then calculate correct checksum
	IP->Checksum		= Ethernet_Checksum(IP, sizeof(IP_Header_t));

	return sizeof(IP_Header_t);
} 

bool IP_ProcessPacket(uint8_t packet[], uint16_t length)
{
	// Minimum length is already checked
	IP_Header_t *IP = (IP_Header_t *)packet;

	// Remove optional padding
	uint16_t ip_length = be16_to_cpu(IP->Length);

	if(IP->HeaderLengthVersion != IP_HEADERLENGTHVERSION ||
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
			reflect = ICMP_ProcessPacket(IP->data, length);
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

int8_t IP_GenerateRequest(uint8_t packet[], IP_Protocol_t protocol, const IP_Address_t *destinationIP, uint8_t payloadLength)
{
	const IP_Address_t *routerIP = destinationIP;
	if(!(IP_compareNet(&OwnIPAddress, destinationIP)))
		routerIP = &RouterIPAddress;

	int8_t offset = Ethernet_GenerateRequest(packet, routerIP, ETHERTYPE_IPV4);
	if(offset <= 0)
		return offset;

	return offset + IP_WriteHeader(packet + offset, protocol, destinationIP, sizeof(IP_Header_t) + payloadLength);
}

uint8_t IP_GenerateBroadcastReply(uint8_t packet[], IP_Protocol_t protocol, uint8_t payloadLength)
{
	uint8_t offset = Ethernet_GenerateBroadcastReply(packet, ETHERTYPE_IPV4);

	return offset + IP_WriteHeader(packet + offset, protocol, &BroadcastIPAddress, payloadLength);
}
