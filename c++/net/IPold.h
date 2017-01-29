#ifndef _IP_H_
#define _IP_H_
#include <stdint.h>
#include "resources.h"

typedef enum
{
	IP_PROTOCOL_ICMP = 1,
	IP_PROTOCOL_UDP = 17,
} IP_Protocol_t;

bool IP_ProcessPacket(uint8_t packet[], uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
int8_t IP_GenerateUnicast(uint8_t packet[], IP_Protocol_t protocol, const IP_Address_t *destinationIP, uint8_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 3);
uint8_t IP_GenerateBroadcast(uint8_t packet[], IP_Protocol_t protocol, uint8_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);

void IP_ChecksumAdd(uint16_t *checksum, uint16_t word) ATTR_NON_NULL_PTR_ARG(1);
uint16_t IP_Checksum(const void *data, uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1) ATTR_PURE;

#endif
