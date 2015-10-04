#ifndef _UDP_H_
#define _UDP_H_

#include <LUFA/Common/Common.h>
#include "IP.h"
#include "resources.h"

// TODO: Lösch mich
typedef enum {
	UDP_PORT_NTP		= 123,
} UDP_Port_t;

typedef struct
{
	uint16_t SourcePort;
	uint16_t DestinationPort;
	uint16_t Length;
	uint16_t Checksum;
} ATTR_PACKED UDP_Header_t;

typedef struct
{
	Ethernet_Header_t Ethernet;
	IP_Header_t IP;
	UDP_Header_t UDP;
	uint8_t data[];
} ATTR_PACKED UDP_Packet_t;

uint16_t UDP_ProcessPacket(void *packet, uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
int8_t UDP_GenerateHeader(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t sourcePort, UDP_Port_t destinationPort, uint16_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);

#endif

