#ifndef _SNTP_H_
#define _SNTP_H_

#include <time.h>
#include <LUFA/Common/Common.h>
#include "UDP.h"
#include "resources.h"

/* Macros: */
#define SNTP_VERSIONMODECLIENT		0x1B
#define SNTP_VERSIONMODESERVER		0x1C

typedef struct
{
	uint8_t VersionMode;
	uint8_t Stratum;
	uint8_t Poll;
	uint8_t Precision;
	uint32_t RootDelay;
	uint32_t RootDispersion;
	uint32_t ReferenceIdentifier;
	uint32_t ReferenceTimestampSec;
	uint32_t ReferenceTimestampSub;
	uint32_t OriginateTimestampSec;
	uint32_t OriginateTimestampSub;
	uint32_t ReceiveTimestampSec;
	uint32_t ReceiveTimestampSub;
	uint32_t TransmitTimestampSec;
	uint32_t TransmitTimestampSub;
} ATTR_PACKED SNTP_Header_t;

typedef struct
{
	Ethernet_Header_t Ethernet;
	IP_Header_t IP;
	UDP_Header_t UDP;
	SNTP_Header_t SNTP;
} ATTR_PACKED SNTP_Packet_t;

uint16_t SNTP_ProcessPacket(void *packet, uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
uint16_t SNTP_GeneratePacket(void *packet) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);

extern time_t reloadtime;
#endif
