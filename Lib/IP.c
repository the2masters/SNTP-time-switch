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
	IP_Header_t *IP = &((IP_Packet_t *)packet)->IP;

	if(IP->HeaderLengthVersion != IP_HEADERLENGTHVERSION ||
	   IP->TotalLength != cpu_to_be16(length - sizeof(Ethernet_Header_t)) ||
	  (IP->FlagsFragment & CPU_TO_BE16(0x3FFF)))
		return 0;

	if(IP->DestinationAddress != OwnIPAddress && IP->DestinationAddress != BroadcastIPAddress)
		return 0;

	switch (IP->Protocol)
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
	IP_Header_t *IP = &((IP_Packet_t *)packet)->IP;

	IP->HeaderLengthVersion	= IP_HEADERLENGTHVERSION;
	IP->TypeOfService	= 0;
	IP->TotalLength		= cpu_to_be16(sizeof(IP_Header_t) + payloadLength);
	IP->Identification	= 0;
	IP->FlagsFragment	= IP_FLAGS_DONTFRAGMENT;
	IP->TTL			= DEFAULT_TTL;
	IP->Protocol		= protocol;
	IP->HeaderChecksum	= 0;		// First set it to 0, later calculate correct checksum
	IP->SourceAddress	= OwnIPAddress;
	IP->DestinationAddress	= *destinationIP;
	IP->HeaderChecksum	= Ethernet_Checksum(IP, sizeof(IP_Header_t));

	if(!IP_compareNet(&OwnIPAddress, destinationIP))
		destinationIP = &RouterIPAddress;

	return Ethernet_GenerateHeaderIP(packet, destinationIP, ETHERTYPE_IPV4, sizeof(IP_Header_t) + payloadLength);
}
