#ifndef _ETHERNET_H_
#define _ETHERNET_H_

#include <stdint.h>
#include <LUFA/Common/Common.h>
#include "resources.h"

typedef enum
{
	ETHERTYPE_IPV4 = CPU_TO_BE16(0x0800),
	ETHERTYPE_ARP = CPU_TO_BE16(0x0806)
} Ethertype_t;

typedef struct
{
	MAC_Address_t Destination;
	MAC_Address_t Source;
	uint16_t      EtherType;
} ATTR_PACKED Ethernet_Header_t;

uint16_t Ethernet_ProcessPacket(void *packet, uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
void Ethernet_ChecksumAdd(uint16_t *checksum, uint16_t word) ATTR_NON_NULL_PTR_ARG(1);
uint16_t Ethernet_Checksum(const void *data, uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1) ATTR_PURE;
uint16_t Ethernet_GenerateHeaderIP(void *packet, const IP_Address_t *destinationIP, Ethertype_t ethertype, uint16_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);
uint16_t Ethernet_GenerateHeader(void* packet, const MAC_Address_t *destinationMAC, Ethertype_t ethertype, uint16_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);

extern const MAC_Address_t BroadcastMACAddress;

#endif

