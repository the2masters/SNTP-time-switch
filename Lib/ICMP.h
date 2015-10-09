#ifndef _ICMP_H_
#define _ICMP_H_
#include <LUFA/Common/Common.h>
#include "IP.h"

typedef enum {
	ICMP_Echo_Reply = 0,
	ICMP_Echo_Request = 8,
} ICMP_Type_t;

#define ICMP_ECHO_Code 0

typedef struct
{
	uint8_t		Type;
	uint8_t		Code;
	uint16_t	Checksum;
	uint8_t		data[];
} ATTR_PACKED ICMP_Header_t;

uint16_t ICMP_ProcessPacket(uint8_t packet[], uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);

#endif
