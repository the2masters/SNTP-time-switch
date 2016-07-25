#include <time.h>
#include <util/eu_dst.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

#include "timer1.h"
#include "resources.h"

#include "rules.h"

int main(void)
{
	timer1_init(F_CPU);
	set_zone(1 * ONE_HOUR);
	set_dst(eu_dst);
	wdt_enable(WDTO_2S);
	set_sleep_mode(SLEEP_MODE_IDLE);

//TODO: Irgendwo muss DDR gesetzt werden, hier ists eher nicht so cool
	DDRC |= (_BV(4) | _BV(5));

	GlobalInterruptEnable();

	for (;;)
	{
		// From now on, if anything happens in interrupt context which requires a new run of the main loop, it has to call sleep_disable();
		sleep_enable();

		processNetworkPackets();

		checkRules();

		sendChangedRules();

		sleep_cpu();
		wdt_reset();
	}
}
