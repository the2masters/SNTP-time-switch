#include "ICMP.h"
#include "IP.h"

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

bool ICMP_ProcessPacket(uint8_t packet[])
{
	// Length is already checked
	ICMP_Header_t *ICMP = (ICMP_Header_t *)packet;

	if(ICMP->Type != ICMP_Echo_Request || ICMP->Code != ICMP_ECHO_Code)
		return false;

	// Answer Request: Leave Packet as is, only replace Type
	ICMP->Type = ICMP_Echo_Reply;
	// Adjust Checksum, only difference is Request(8)-Reply(0) at high byte
	IP_ChecksumAdd(&ICMP->Checksum, CPU_TO_BE16((ICMP_Echo_Request - ICMP_Echo_Reply) << 8));

	return true;
}
