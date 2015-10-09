#include "IP.h"
#include "UDP.h"
#include "ICMP.h"
#include <avr/cpufunc.h>
#include "helper.h"

#define IP_HEADERLENGTHVERSION		(0x40 | sizeof(IP_Header_t)/4)
#define DEFAULT_TTL			128
#define IP_FLAGS_DONTFRAGMENT		CPU_TO_BE16(0x4000)


uint16_t IP_ProcessPacket(uint8_t packet[], uint16_t length)
{
	// Length is already checked
	IP_Header_t *IP = (IP_Header_t *)packet;

	if(IP->HeaderLengthVersion != IP_HEADERLENGTHVERSION ||
	   be16_to_cpu(IP->Length) != length ||
	  (IP->FlagsFragment & CPU_TO_BE16(0x3FFF)))
		return 0;

	if(IP->DestinationAddress != OwnIPAddress && IP->DestinationAddress != BroadcastIPAddress)
		return 0;

	length -= sizeof(IP_Header_t);
	switch (IP->Protocol)
	{
		case IP_PROTOCOL_ICMP:
			length = ICMP_ProcessPacket(IP->data, length);
			break;
		case IP_PROTOCOL_UDP:
			length = UDP_ProcessPacket(IP->data, length, &IP->SourceAddress);
			break;
		default:
			return 0;
	}

	if(length)
	{
		length += sizeof(IP_Header_t);

		IP->Length		= cpu_to_be16(length);
		IP->TTL			= DEFAULT_TTL;
		IP->Checksum		= 0;
		IP->DestinationAddress	= IP->SourceAddress;
		IP->SourceAddress	= OwnIPAddress;
		IP->Checksum		= Ethernet_Checksum(IP, sizeof(IP_Header_t));
	}
	return length;
}

int8_t IP_GenerateHeader(uint8_t packet[], IP_Protocol_t protocol, const IP_Address_t *destinationIP, uint16_t payloadLength)
{
	const IP_Address_t *routerIP = destinationIP;
	if(!(IP_compareNet(&OwnIPAddress, destinationIP)))
		routerIP = &RouterIPAddress;

	int8_t offset = Ethernet_GenerateHeaderIP(packet, routerIP, ETHERTYPE_IPV4);
	if(offset <= 0)
		return offset;

	IP_Header_t *IP = (IP_Header_t *)(packet + offset);

	IP->HeaderLengthVersion	= IP_HEADERLENGTHVERSION;
	IP->TypeOfService	= 0;
	IP->Length		= cpu_to_be16(sizeof(IP_Header_t) + payloadLength);
	IP->Identification	= 0;
	IP->FlagsFragment	= IP_FLAGS_DONTFRAGMENT;
	IP->TTL			= DEFAULT_TTL;
	IP->Protocol		= protocol;
	IP->Checksum		= 0;		// First set it to 0, later calculate correct checksum
	IP->SourceAddress	= OwnIPAddress;
	IP->DestinationAddress	= *destinationIP;
	IP->Checksum		= Ethernet_Checksum(IP, sizeof(IP_Header_t));

	return offset + sizeof(IP_Header_t);
}
