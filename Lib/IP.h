#ifndef _IP_H_
#define _IP_H_

#include <LUFA/Common/Common.h>
#include "Ethernet.h"
#include "resources.h"

typedef enum
{
	IP_PROTOCOL_ICMP = 1,
	IP_PROTOCOL_UDP = 17,
} IP_Protocol_t;

typedef struct
{
	uint8_t      HeaderLengthVersion;
	uint8_t      TypeOfService;
	uint16_t     TotalLength;

	uint16_t     Identification;
	uint16_t     FlagsFragment;

	uint8_t      TTL;
	uint8_t      Protocol;
	uint16_t     HeaderChecksum;

	IP_Address_t SourceAddress;
	IP_Address_t DestinationAddress;
} ATTR_PACKED IP_Header_t;

typedef struct
{
	Ethernet_Header_t Ethernet;
	IP_Header_t IP;
	uint8_t data[];

} ATTR_PACKED IP_Packet_t;

uint16_t IP_ProcessPacket(void *packet, uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
uint16_t IP_GenerateHeader(void *packet, IP_Protocol_t protocol, const IP_Address_t *destinationIP, uint16_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 3);

ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2) ATTR_PURE ATTR_ALWAYS_INLINE
static inline bool IP_compareNet(const IP_Address_t *a, const IP_Address_t *b)
{
	return (*a & NETMASK_BE) == (*b & NETMASK_BE);
}

ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1) ATTR_PURE ATTR_ALWAYS_INLINE
static inline IP_Hostpart_t IP_getHost(const IP_Address_t *ip)
{
	return (IP_Hostpart_t)be32_to_cpu(*ip);
}

#endif
