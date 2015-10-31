#include "UDP.h"
#include "IP.h"
#include "helper.h"

#define UDP_PORT_AUTOMAT CPU_TO_BE16(UDP_PORT)

typedef struct
{
        uint16_t SourcePort;
        uint16_t DestinationPort;
        uint16_t Length;
        uint16_t Checksum;

        uint8_t data[];
} ATTR_PACKED UDP_Header_t;

static uint8_t UDP_WriteHeader(uint8_t packet[], UDP_Port_t sourcePort, UDP_Port_t destinationPort, uint8_t payloadLength)
{
	UDP_Header_t *UDP = (UDP_Header_t *)packet;

	UDP->SourcePort = sourcePort;
	UDP->DestinationPort = destinationPort;
	UDP->Length = cpu_to_be16(sizeof(UDP_Header_t) + payloadLength);
	UDP->Checksum = 0;
	return sizeof(UDP_Header_t);
}


ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1) ATTR_WEAK
bool UDP_Callback_Request(uint8_t packet[] ATTR_MAYBE_UNUSED, UDP_Port_t destinationPort ATTR_MAYBE_UNUSED, uint16_t length ATTR_MAYBE_UNUSED)
{
	return false;
}

ATTR_NON_NULL_PTR_ARG(1, 2) ATTR_WEAK
void UDP_Callback_Reply(uint8_t packet[] ATTR_MAYBE_UNUSED, const IP_Address_t *sourceIP ATTR_MAYBE_UNUSED, UDP_Port_t sourcePort ATTR_MAYBE_UNUSED, uint16_t length ATTR_MAYBE_UNUSED)
{
}

bool UDP_ProcessPacket(uint8_t packet[], const IP_Address_t *sourceIP, uint16_t length)
{
	// Length is already checked
	UDP_Header_t *UDP = (UDP_Header_t *)packet;

        if(be16_to_cpu(UDP->Length) > length)
                return false;

	length -= sizeof(UDP_Header_t);

	if(UDP->SourcePort == UDP_PORT_AUTOMAT)		// This is a request
	{
		if(UDP_Callback_Request(UDP->data, be16_to_cpu(UDP->DestinationPort), length))
		{
			UDP_WriteHeader(packet, UDP->DestinationPort, UDP->SourcePort, length);
			return true;
		}
	}
	else if (UDP->DestinationPort == UDP_PORT_AUTOMAT)	// This is a reply
	{
		UDP_Callback_Reply(UDP->data, sourceIP, be16_to_cpu(UDP->SourcePort), length);
	}
	return false;
}

int8_t UDP_GenerateUnicast(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort, uint16_t payloadLength)
{
	int8_t offset = IP_GenerateUnicast(packet, IP_PROTOCOL_UDP, destinationIP, sizeof(UDP_Header_t) + payloadLength);
	if(offset <= 0)
		return offset;

	return offset + UDP_WriteHeader(packet + offset, UDP_PORT_AUTOMAT, cpu_to_be16(destinationPort), payloadLength);
}

uint8_t UDP_GenerateBroadcast(uint8_t packet[], UDP_Port_t sourcePort, uint16_t payloadLength)
{
	uint8_t offset = IP_GenerateBroadcast(packet, IP_PROTOCOL_UDP, sizeof(UDP_Header_t) + payloadLength);

	return offset + UDP_WriteHeader(packet + offset, cpu_to_be16(sourcePort), UDP_PORT_AUTOMAT, payloadLength);
}
