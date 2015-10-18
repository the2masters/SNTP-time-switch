#ifndef _IP_H_
#define _IP_H_
#include <stdint.h>
#include <LUFA/Common/Common.h>
#include "resources.h"

typedef enum
{
	IP_PROTOCOL_ICMP = 1,
	IP_PROTOCOL_UDP = 17,
} IP_Protocol_t;

bool IP_ProcessPacket(uint8_t packet[], uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
int8_t IP_GenerateRequest(uint8_t packet[], IP_Protocol_t protocol, const IP_Address_t *destinationIP, uint8_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 3);
uint8_t IP_GenerateBroadcastReply(uint8_t packet[], IP_Protocol_t protocol, uint8_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);

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
