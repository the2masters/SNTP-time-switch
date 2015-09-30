#include "UDP.h"
#include "SNTP.h"

uint16_t UDP_ProcessPacket(void *packet, uint16_t length)
{
	UDP_Header_t *UDP_Header = &((UDP_FullHeader_t *)packet)->UDP;

        if(UDP_Header->Length != cpu_to_be16(length - sizeof(IP_FullHeader_t)))
                return 0;

	switch (UDP_Header->SourcePort)
	{
		case UDP_PORT_NTP:
			return SNTP_ProcessPacket(packet, length);
	}
	return 0;
}

uint16_t UDP_GenerateHeader(void *packet, const IP_Address_t *destinationIP, UDP_Port_t destinationPort, uint16_t payloadLength)
{
	UDP_Header_t *UDP_Header = &((UDP_FullHeader_t *)packet)->UDP;

	static uint16_t sourcePortNum = 1024;
	if(sourcePortNum < 65535)
		sourcePortNum++;
	else
		sourcePortNum = 1024;
	UDP_Header->SourcePort = cpu_to_be16(sourcePortNum);
	UDP_Header->DestinationPort = destinationPort;
	UDP_Header->Length = cpu_to_be16(sizeof(UDP_Header_t) + payloadLength);
	UDP_Header->Checksum = 0;

	return IP_GenerateHeader(packet, IP_PROTOCOL_UDP, destinationIP, sizeof(UDP_Header_t) + payloadLength);
}

