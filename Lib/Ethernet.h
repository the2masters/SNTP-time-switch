#ifndef _ETHERNET_H_
#define _ETHERNET_H_
#include <stdint.h>
#include "resources.h"

typedef enum
{
	ETHERTYPE_IPV4 = CPU_TO_BE16(0x0800),
	ETHERTYPE_ARP = CPU_TO_BE16(0x0806)
} Ethertype_t;

bool Ethernet_ProcessPacket(uint8_t packet[], uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
int8_t Ethernet_GenerateUnicast(uint8_t packet[], const IP_Address_t *destinationIP, Ethertype_t ethertype) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);
uint8_t Ethernet_GenerateBroadcast(uint8_t packet[], Ethertype_t ethertype) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);

extern const MAC_Address_t BroadcastMACAddress;

#endif

