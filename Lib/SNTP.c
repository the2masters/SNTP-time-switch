#include "SNTP.h"
#include <time.h>
#include <avr/io.h>
#include <string.h>
#include "UDP.h"

#define SNTP_VERSIONMODECLIENT	0x1B
#define SNTP_VERSIONMODESERVER	0x1C

typedef struct
{
	uint8_t		VersionMode;
	uint8_t		Stratum;
	uint8_t		Poll;
	uint8_t		Precision;
	uint32_t	RootDelay;
	uint32_t	RootDispersion;
	uint32_t	ReferenceIdentifier;
	uint32_t	ReferenceTimestampSec;
	uint32_t	ReferenceTimestampSub;
	uint32_t	OriginateTimestampSec;
	uint32_t	OriginateTimestampSub;
	uint32_t	ReceiveTimestampSec;
	uint32_t	ReceiveTimestampSub;
	uint32_t	TransmitTimestampSec;
	uint32_t	TransmitTimestampSub;
} ATTR_PACKED SNTP_Header_t;

// Passt den Zahlenraum NTP an den Zahlenraum Timer an
// 32 Bit werden die Zahlen 0-62498 abgebildet. Mathematisch:
// Runden(fract * 62498 / 2^32)
// Eigentlich muesste es auf 0-62499 abgebildet werden, aber dann
// bin ich mir nicht sicher, dass der Interrupt ausloest
// Optimiert auf 32Bit Schiebe und Addier-Operationen
static inline uint16_t frac2timer(uint32_t fract)
{
        uint32_t retVal = 0;
        for (uint8_t i = 15; i; --i)
        {
                fract >>= 1;
                switch(i) {     // Je gesetztem Bit von 62500 addieren
                case 1:
                case 5:
                case 10:
                case 12:
                case 13:
                case 14:
                case 15: retVal += fract;
                }
        }

        return (retVal + 32768) >> 16;
}

#define MaxRoundTripSec 2
time_t SNTP_ProcessPacket(uint8_t packet[], uint16_t length)
{
	if(length != sizeof(SNTP_Header_t))
		return 0;

	SNTP_Header_t* SNTP  = (SNTP_Header_t *)packet;

	if ((SNTP->VersionMode & 0x3F) != SNTP_VERSIONMODESERVER || (SNTP->VersionMode & 0xC0) == 0xC0 ||
	    (SNTP->TransmitTimestampSec == 0 && SNTP->TransmitTimestampSub == 0))
		return 0;

	TCNT1 = frac2timer(be32_to_cpu(SNTP->TransmitTimestampSub));
	time_t newTime = be32_to_cpu(SNTP->TransmitTimestampSec) - NTP_OFFSET;
	set_system_time(newTime);

	return newTime;
}

int8_t SNTP_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort)
{
	int8_t offset = UDP_GenerateUnicast(packet, destinationIP, destinationPort, sizeof(SNTP_Header_t));
	if(offset < 0)
		return offset;

	SNTP_Header_t *SNTP = (SNTP_Header_t *)(packet + offset);

	memset(SNTP, 0, sizeof(SNTP_Header_t));
	SNTP->VersionMode = SNTP_VERSIONMODECLIENT;
#ifdef DEBUG
	SNTP->TransmitTimestampSec = cpu_to_be32(time(NULL) + (uint32_t)NTP_OFFSET);
	SNTP->TransmitTimestampSub = cpu_to_be32(TCNT1 * (uint32_t)68721);
#endif

	return offset + sizeof(SNTP_Header_t);
}

