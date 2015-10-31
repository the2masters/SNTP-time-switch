#include <time.h>
#include <util/eu_dst.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

#include "timer1.h"
#include "resources.h"

#include "rules.h"







/*
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
}*/

int main(void)
{
	timer1_init(F_CPU);
	set_zone(1 * ONE_HOUR);
	set_dst(eu_dst);
	wdt_enable(WDTO_2S);
	set_sleep_mode(SLEEP_MODE_IDLE);

	GlobalInterruptEnable();

	for (;;)
	{
		// From now on, if anything happens in interrupt context which requires a new run of the main loop, it has to sleep_disable();
		sleep_enable();

		processNetworkPackets();

		checkRules();

		sendChangedRules();

		sleep_cpu();
		wdt_reset();
	}
}
