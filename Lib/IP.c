#include "IP.h"
#include "UDP.h"
#include "ICMP.h"
#include <avr/cpufunc.h>
#include "helper.h"

#define IP_HEADERLENGTHVERSION		(0x40 | sizeof(IP_Header_t)/4)
#define DEFAULT_TTL			128
#define IP_FLAGS_DONTFRAGMENT		CPU_TO_BE16(0x4000)


uint16_t IP_ProcessPacket(void *packet, uint16_t length)
{
	IP_Header_t *IP_Header = &((IP_FullHeader_t *)packet)->IP;

	if(IP_Header->HeaderLengthVersion != IP_HEADERLENGTHVERSION ||
	   IP_Header->TotalLength != cpu_to_be16(length - sizeof(Ethernet_Header_t)) ||
	  (IP_Header->FlagsFragment & CPU_TO_BE16(0x3FFF)))
		return 0;

	if(IP_Header->DestinationAddress != OwnIPAddress && IP_Header->DestinationAddress != BroadcastIPAddress)
		return 0;

	switch (IP_Header->Protocol)
	{
		case IP_PROTOCOL_ICMP:
			return ICMP_ProcessPacket(packet, length);
		case IP_PROTOCOL_UDP:
			return UDP_ProcessPacket(packet, length);
	}
	return 0;
}

uint16_t IP_GenerateHeader(void *packet, IP_Protocol_t protocol, const IP_Address_t *destinationIP, uint16_t payloadLength)
{
	IP_Header_t *IP_Header = &((IP_FullHeader_t *)packet)->IP;

	IP_Header->HeaderLengthVersion	= IP_HEADERLENGTHVERSION;
	IP_Header->TypeOfService	= 0;
	IP_Header->TotalLength		= cpu_to_be16(sizeof(IP_Header_t) + payloadLength);
	IP_Header->Identification	= 0;
	IP_Header->FlagsFragment	= IP_FLAGS_DONTFRAGMENT;
	IP_Header->TTL			= DEFAULT_TTL;
	IP_Header->Protocol		= protocol;
	IP_Header->HeaderChecksum	= 0;		// First set it to 0, later calculate correct checksum
	IP_Header->SourceAddress	= OwnIPAddress;
	IP_Header->DestinationAddress	= *destinationIP;
	IP_Header->HeaderChecksum	= Ethernet_Checksum(IP_Header, sizeof(IP_Header_t));

	if(!IP_compareNet(&OwnIPAddress, destinationIP))
		destinationIP = &RouterIPAddress;

	return Ethernet_GenerateHeaderIP(packet, destinationIP, ETHERTYPE_IPV4, sizeof(IP_Header_t) + payloadLength);
}
