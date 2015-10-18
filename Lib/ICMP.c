#include "ICMP.h"
//TODO: put Ethernet_checksum to own file
#include "Ethernet.h"

typedef struct
{
	uint8_t		Type;
	uint8_t		Code;
	uint16_t	Checksum;

	uint8_t		data[];
} ATTR_PACKED ICMP_Header_t;

typedef enum {
        ICMP_Echo_Reply = 0,
        ICMP_Echo_Request = 8,
} ICMP_Type_t;
#define ICMP_ECHO_Code 0

bool ICMP_ProcessPacket(uint8_t packet[], uint16_t length)
{
	// Length is already checked
	ICMP_Header_t *ICMP = (ICMP_Header_t *)packet;

	if(ICMP->Code != ICMP_ECHO_Code)
		return false;

	switch ((ICMP_Type_t)ICMP->Type)
	{
		case ICMP_Echo_Request:
			// Leave Packet as is, only replace Type
			ICMP->Type = ICMP_Echo_Reply;
			// Adjust Checksum, only difference is Request(8)-Reply(0) at high byte
			Ethernet_ChecksumAdd(&ICMP->Checksum, CPU_TO_BE16((ICMP_Echo_Request - ICMP_Echo_Reply) << 8));

			return true;

		case ICMP_Echo_Reply:
		default:
			return false;
	}
}
