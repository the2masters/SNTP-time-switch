#include "timer1.h"
#include "resources.h"

#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <util/atomic.h>

//#include "rules.h"

#include "PacketBuffer.h"
#include "USB.h"

void processNetworkPackets(void)
{
	Packet_t *packet;
	ATOMIC_BLOCK(ATOMIC_FORCEON)
		packet = Packet_GetInput();
	if(packet) {
// TODO: if there is one packet, there could be more packets
		sleep_disable();
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			Packet_ReattachOutput(packet);
			USB_EnableTransmitter();
		}
	}
}

int main(void)
{
	timer1_init(F_CPU);
	wdt_enable(WDTO_2S);
	set_sleep_mode(SLEEP_MODE_IDLE);

//TODO: Irgendwo muss DDR gesetzt werden, hier ists eher nicht so cool
//	DDRC |= (_BV(4) | _BV(5));
	DDRD = 0xFF;

	sei();

	for (;;)
	{
		// From now on, if anything happens in interrupt context which requires a new run of the main loop, it has to call sleep_disable();
		sleep_enable();

		processNetworkPackets();

//		checkRules();

//		sendChangedRules();

		sleep_cpu();
		wdt_reset();
	}
}
