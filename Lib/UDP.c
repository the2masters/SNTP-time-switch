#include "UDP.h"
#include "SNTP.h"

uint16_t UDP_ProcessPacket(void *packet, uint16_t length)
{
	UDP_Header_t *UDP = &((UDP_Packet_t *)packet)->UDP;

        if(UDP->Length != cpu_to_be16(length - sizeof(IP_Packet_t)))
                return 0;

	switch (UDP->SourcePort)
	{
		case UDP_PORT_NTP:
			return SNTP_ProcessPacket(packet, length);
	}
	return 0;
}

int8_t UDP_GenerateHeader(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t sourcePort, UDP_Port_t destinationPort, uint16_t payloadLength)
{
	int8_t offset = Ethernet_GenerateHeaderIP(packet, destinationIP, ETHERTYPE_IPV4);
	if(offset < 0)
		return offset;

	UDP_Header_t *UDP = (UDP_Header_t *)(packet + offset);

	UDP->SourcePort = cpu_to_be16(sourcePort);
	UDP->DestinationPort = cpu_to_be16(destinationPort);
	UDP->Length = cpu_to_be16(sizeof(UDP_Header_t) + payloadLength);
	UDP->Checksum = 0;

	return offset + sizeof(UDP_Header_t);
}

