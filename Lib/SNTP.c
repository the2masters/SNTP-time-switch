#include "SNTP.h"
#include <time.h>
#include <avr/io.h>
#include <string.h>
#include <avr/cpufunc.h>
#include <util/atomic.h>
#include "helper.h"

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
uint16_t SNTP_ProcessPacket(void *packet, uint16_t length)
{
	if(length != sizeof(SNTP_FullPacket_t))
		return 0;

	SNTP_Packet_t* SNTP  = &((SNTP_FullPacket_t *)packet)->SNTP;

	if ((SNTP->VersionMode & 0x3F) != SNTP_VERSIONMODESERVER || (SNTP->VersionMode & 0xC0) == 0xC0 || (SNTP->TransmitTimestampSec == 0 && SNTP->TransmitTimestampSub == 0))
		return 0;

#ifndef SNTP_CALCULATE_ROUNDTRIP
	TCNT1 = frac2timer(be32_to_cpu(SNTP->TransmitTimestampSub));
	time_t neueZeit = be32_to_cpu(SNTP->TransmitTimestampSec) - NTP_OFFSET;

#else
	uint32_t DestinationTimestampSub = TCNT1;
	time_t DestinationTimestampSec = time(NULL);

	if(SNTP->OriginateTimestampSec > DestinationTimestampSec ||
	  (SNTP->OriginateTimestampSec == DestinationTimestampSec && SNTP->OriginateTimestampSub > DestinationTimestampSub) ||
	   SNTP->OriginateTimestampSec < DestinationTimestampSec - MaxRoundTripSec)
		return 0;

	const uint16_t timerlength = (OCR1A + 1);
	DestinationTimestampSub -= SNTP->OriginateTimestampSub;
	while(DestinationTimestampSec != SNTP->OriginateTimestampSec)
	{
		DestinationTimestampSub += timerlength;
		DestinationTimestampSec--;
	}
	DestinationTimestampSub /= 2; // half roundtrip delay

	// Add it to TransmitTimestamp
	time_t neueZeit = be32_to_cpu(SNTP->TransmitTimestampSec) - NTP_OFFSET;
	DestinationTimestampSub += frac2timer(be32_to_cpu(SNTP->TransmitTimestampSub));
	while(DestinationTimestampSub >= timerlength)
	{
		neueZeit++;
		DestinationTimestampSub -= timerlength;
	}
	TCNT1 = (uint16_t)DestinationTimestampSub;
#endif

	set_system_time(neueZeit);
	reloadtime = neueZeit + ONE_HOUR;
	nextCalc = 0;
	return 0;
}

uint16_t SNTP_GeneratePacket(void *packet)
{
	SNTP_Packet_t *SNTP_Packet = &((SNTP_FullPacket_t *)packet)->SNTP;

	memset(SNTP_Packet, 0, sizeof(SNTP_Packet_t));
	SNTP_Packet->VersionMode = SNTP_VERSIONMODECLIENT;

#ifndef SNTP_CALCULATE_ROUNDTRIP
	SNTP_Packet->TransmitTimestampSec = time(NULL);
	SNTP_Packet->TransmitTimestampSub = TCNT1;
#endif

	return UDP_GenerateHeader(packet, &SNTPIPAddress, UDP_PORT_NTP, sizeof(SNTP_Packet_t));
}

