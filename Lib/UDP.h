#ifndef _UDP_H_
#define _UDP_H_

#include <LUFA/Common/Common.h>
#include "IP.h"
#include "resources.h"

typedef uint16_t UDP_Port_t;
enum {
	UDP_PORT_NTP		= CPU_TO_BE16(123),
	UDP_PORT_AUTOMAT	= CPU_TO_BE16(1023),
};

typedef struct
{
	uint16_t SourcePort;
	uint16_t DestinationPort;
	uint16_t Length;
	uint16_t Checksum;

	uint8_t		data[];
} ATTR_PACKED UDP_Header_t;

uint16_t UDP_ProcessPacket(uint8_t packet[], uint16_t length, const IP_Address_t *sourceIP) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 3);
int8_t UDP_GenerateHeader(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort, uint16_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);
extern uint16_t UDP_Callback(uint8_t packet[], uint16_t length, const IP_Address_t *sourceIP, UDP_Port_t sourcePort, uint16_t destinationPort) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 3);

#endif

