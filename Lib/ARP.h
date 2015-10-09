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

const MAC_Address_t* ARP_searchMAC(const IP_Address_t *IP) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
uint8_t ARP_ProcessPacket(uint8_t packet[], uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
uint8_t ARP_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);

#endif

