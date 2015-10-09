#include <time.h>
#include <util/eu_dst.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

#include "helper.h"
#include "timer1.h"
#include "relay.h"
#include "Lib/Ethernet.h"
#include "Lib/SNTP.h"
#include "USB.h"
#include "resources.h"
#include "Lib/ARP.h"

// List of all installed Relais
typedef enum {
	Relay1,
	Relay2,
	RelayCount
} Relay_t;

// Extracted from curcuit board
define_relay(Relay1, PORTC, _BV(5))
define_relay(Relay2, PORTC, _BV(4))


// Here you can select the times when a relay should be switched on
typedef struct {
	Relay_t relay;
	int8_t starthour;
	int8_t startmin;
	int8_t stophour;
	int8_t stopmin;
	uint8_t day; // Bits: 7 .. 0: unused, saturday, friday, thursday, wednesday, tuesday, monday, sunday
} Order_t;
static const Order_t Orders[] = {
	{ .relay = Relay1, .starthour = 20, .startmin =  41, .stophour = 24, .stopmin =  0, .day = _BV(MONDAY) | _BV(TUESDAY) | _BV(WEDNESDAY) | _BV(THURSDAY) | _BV(FRIDAY) | _BV(SATURDAY) | _BV(SUNDAY) },
};

static inline void relay_set(Relay_t num, bool onoff)
{
	switch(num)
	{
		case Relay1: Relay1_set(onoff); break;
		case Relay2: Relay2_set(onoff); break;
		case RelayCount:
		default: __builtin_unreachable();
	}
}

time_t nextCalc = UINT32_MAX;	// Wait until first received SNTP-Packet
time_t reloadtime = 5;		// Start 2s after bootup sending SNTP-Packets

// This is mktime without changing it's arguments but recalculate dst each time
// { struct tm tm = *time; tm.tm_isdst = -1; return mktime(&tm); }
static time_t my_mktime(const struct tm *time)
{
	// extract internals out of time.h implementation
	extern int32_t __utc_offset;
	extern int16_t (*__dst_ptr)(const time_t *, int32_t *);

	// Convert localtime as UTC and correct it by UTC offset
	time_t retVal = mk_gmtime(time) - __utc_offset;
	// Additionally correct time by DST offset (if DST rules are configured)
	// Orders scheduled in the lost hour in spring are executed one hour early
	// Orders scheduled in the repeated hour in autumn are executed only one time
	// after the time shift
	if(__dst_ptr)
		return retVal - __dst_ptr(&retVal, &__utc_offset);
	else
		return retVal;
}

// Calculate which relay should be switched on or off
void calc(time_t time)
{
	struct tm tm;
	localtime_r(&time, &tm);
	tm.tm_sec = 0;				// ignore seconds

	tm.tm_hour = 23, tm.tm_min = 60;
	nextCalc = my_mktime(&tm);		// recalculate not later than next midnight

	// Default: Every Relay switched off
	bool relayState[RelayCount] = {0};

	for(uint8_t i = 0; i < ARRAY_SIZE(Orders); i++)
	{
		// Skip this order, if it's written for another weekday
		if((Orders[i].day & _BV(tm.tm_wday)) == 0)
			continue;

		// Calculate Start timestamp
		tm.tm_hour = Orders[i].starthour, tm.tm_min = Orders[i].startmin;
		time_t start = my_mktime(&tm);
		if(time < start)		// if it's before start time of this order
		{				// recalculate not later than switch on time
			if(nextCalc > start) nextCalc = start;
		} else {
			// Calculate Stop timestamp
			tm.tm_hour = Orders[i].stophour, tm.tm_min = Orders[i].stopmin;
			time_t stop = my_mktime(&tm);
			if(time < stop)		// if it's between start and stop of this order
			{			// recalculate not later then switch off time
				if(nextCalc > stop) nextCalc = stop;
				// Set Flag to set Relay
				relayState[Orders[i].relay] = true;
			}
		}
	}
	// Set each relay to it's calculated state
	for(Relay_t i = 0; i < ARRAY_SIZE(relayState); i++)
	{
		relay_set(i, relayState[i]);
	}
}

uint16_t UDP_Callback(uint8_t packet[], uint16_t length, const IP_Address_t *sourceIP, uint16_t sourcePort)
{
	if(sourcePort == CPU_TO_BE16(UDP_PORT_NTP))
	{
		time_t now = SNTP_ProcessPacket(packet, length);
		if(now == 0) return 0;
		set_system_time(now);
		reloadtime = now + ONE_HOUR;
		nextCalc = 0;
	}
	return 0;
}

int main(void)
{
	timer1_init(F_CPU);
	set_zone(1 * ONE_HOUR);
	set_dst(eu_dst);
	wdt_enable(WDTO_2S);
	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();

	GlobalInterruptEnable();

	for (;;)
	{
		time_t now = time(NULL);

		if(now >= nextCalc)
		{
			calc(now);
		}

		if(USB_isReady())
		{
			// reloadtime reflects the time a new SNTP-Packet should be sent
			if(now >= reloadtime)
			{
				if(USB_prepareTS())
				{
					reloadtime = now + 2;	// Solange keine Antwort kommt sende das alle 2 Sekunden
					int8_t length = SNTP_GeneratePacket(Packet);
					if(length < 0)
					{
						length = -length;
						reloadtime = now + 1;
					}
					PacketLength = length;

					USB_Send();

				}
			}
			if(USB_Received())
			{
				PacketLength = Ethernet_ProcessPacket(Packet, PacketLength);
				if(PacketLength)
					USB_Send();
				else
					USB_EnableReceiver();
			}
		}
		sleep_cpu();
		wdt_reset();
	}
}
