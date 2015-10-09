#include "UDP.h"
#include "SNTP.h"

uint16_t UDP_ProcessPacket(uint8_t packet[], const IP_Address_t *sourceIP, uint16_t length)
{
	// Length is already checked
	UDP_Header_t *UDP = (UDP_Header_t *)packet;

        if(be16_to_cpu(UDP->Length) != length)
                return 0;

	length -= sizeof(UDP_Header_t);
	switch (UDP->DestinationPort)
	{
		case CPU_TO_BE16(UDP_PORT_NTP):
			length = SNTP_ProcessPacket(UDP->data, length);
			break;
		default:
			return 0;
	}

	if(length)
	{
		length += sizeof(UDP_Header_t);

		// Exchange source and destinationPort
		uint16_t temp = UDP->SourcePort;
		UDP->SourcePort = UDP->DestinationPort;
		UDP->DestinationPort = temp;

		// Set length and no checksum
		UDP->Length = cpu_to_be16(length);
		UDP->Checksum = 0;		
	}
	return length;
}

int8_t UDP_GenerateHeader(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t sourcePort, UDP_Port_t destinationPort, uint16_t payloadLength)
{
	int8_t offset = IP_GenerateHeader(packet, IP_PROTOCOL_UDP, destinationIP, payloadLength + sizeof(UDP_Header_t));
	if(offset <= 0)
		return offset;

	UDP_Header_t *UDP = (UDP_Header_t *)(packet + offset);

	UDP->SourcePort = cpu_to_be16(sourcePort);
	UDP->DestinationPort = cpu_to_be16(destinationPort);
	UDP->Length = cpu_to_be16(sizeof(UDP_Header_t) + payloadLength);
	UDP->Checksum = 0;

	return offset + sizeof(UDP_Header_t);
}

