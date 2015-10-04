#ifndef _ARP_H_
#define _ARP_H_

#include <LUFA/Common/Common.h>
#include "Ethernet.h"
#include "resources.h"

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
	Ethernet_Header_t Ethernet;
	ARP_Header_t ARP;
} ATTR_PACKED ARP_Packet_t;

const MAC_Address_t* ARP_searchMAC(const IP_Address_t *IP) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
uint16_t ARP_ProcessPacket(void* packet, uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
uint16_t ARP_GenerateRequest(void* packet, const IP_Address_t *destinationIP) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);

#endif

