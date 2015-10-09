#include "ICMP.h"
#include "Ethernet.h"

uint16_t ICMP_ProcessPacket(uint8_t packet[], uint16_t length)
{
	// Length is already checked
	ICMP_Header_t *ICMP = (ICMP_Header_t *)packet;

	if(ICMP->Code != ICMP_ECHO_Code)
		return 0;

	switch ((ICMP_Type_t)ICMP->Type)
	{
		case ICMP_Echo_Request:
			// Leave Packet as is, only replace Type
			ICMP->Type = ICMP_Echo_Reply;
			// Adjust Checksum, only difference is Request(8)-Reply(0) at high byte
			Ethernet_ChecksumAdd(&ICMP->Checksum, CPU_TO_BE16((ICMP_Echo_Request - ICMP_Echo_Reply) << 8));

			return length;

		case ICMP_Echo_Reply:
		default:
			return 0;
	}
}
