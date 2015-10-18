#include "UDP.h"
#include "IP.h"

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

bool UDP_ProcessPacket(uint8_t packet[], const IP_Address_t *sourceIP, uint16_t length)
{
	// Length is already checked
	UDP_Header_t *UDP = (UDP_Header_t *)packet;

        if(be16_to_cpu(UDP->Length) > length)
                return false;

	length -= sizeof(UDP_Header_t);

	if(UDP->SourcePort == UDP_PORT_AUTOMAT)		// This is a request
	{
		if(UDP_Callback_Request(UDP->data, sourceIP, be16_to_cpu(UDP->DestinationPort), length))
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

int8_t UDP_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort, uint16_t payloadLength)
{
	int8_t offset = IP_GenerateRequest(packet, IP_PROTOCOL_UDP, destinationIP, sizeof(UDP_Header_t) + payloadLength);
	if(offset <= 0)
		return offset;

	return offset + UDP_WriteHeader(packet + offset, UDP_PORT_AUTOMAT, cpu_to_be16(destinationPort), payloadLength);
}

uint8_t UDP_GenerateBroadcastReply(uint8_t packet[], UDP_Port_t sourcePort, uint16_t payloadLength)
{
	uint8_t offset = IP_GenerateBroadcastReply(packet, IP_PROTOCOL_UDP, sizeof(UDP_Header_t) + payloadLength);

	return offset + UDP_WriteHeader(packet + offset, cpu_to_be16(sourcePort), UDP_PORT_AUTOMAT, payloadLength);
}
