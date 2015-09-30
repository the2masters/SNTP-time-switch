#include "ICMP.h"
#include "Ethernet.h"

uint16_t ICMP_ProcessPacket(void *packet, uint16_t length)
{
	ICMP_Packet_t *ICMP_Packet = &((ICMP_FullPacket_t *)packet)->ICMP;
	IP_Header_t *IP_Header = &((ICMP_FullPacket_t *)packet)->IP.IP;

	switch ((ICMP_Type_t)ICMP_Packet->Type)
	{
		case ICMP_Echo_Request:
			// Leave Packet as is, only replace Type
			ICMP_Packet->Type = ICMP_Echo_Reply;
			// Adjust Checksum, only difference is Request(8)-Reply(0) at high byte
			Ethernet_ChecksumAdd(&ICMP_Packet->Checksum, CPU_TO_BE16((ICMP_Echo_Request - ICMP_Echo_Reply) << 8));

			// Get destinationIP and recreate IP Header
			IP_Address_t destinationIP = IP_Header->SourceAddress;
			return IP_GenerateHeader(packet, IP_PROTOCOL_ICMP, &destinationIP, length - sizeof(IP_FullHeader_t));
		case ICMP_Echo_Reply:
		default:
			return 0;
	}
}
