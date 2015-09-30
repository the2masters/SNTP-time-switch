#ifndef timer0_h
#define timer0_h

#include <stdint.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

static inline void timer0_init(uint32_t ticks)
{
	power_timer0_enable();
        TCCR0B = 0;			// Timer beenden
	TCCR0A = _BV(WGM01);		// CTC Mode
	TCNT0 = 0;			// Timer Reset
	TIFR0 = 0xff;			// Interrupt Flags zurücksetzen
	TIMSK0 = (1 << OCIE0A);		// Interrupt einschalten

	if(ticks <= 256) {
		OCR0A = (uint8_t)(ticks - 1);
		TCCR0B = _BV(CS00);
	}
	else if (ticks <= (8*256LL)) {
		//staticassert(ticks % 8 == 0, "precision not supported");
		OCR0A = (uint8_t)(ticks / 8 - 1);
		TCCR0B = _BV(CS01);
	}
	else if (ticks <= (64*256LL)) {
		//staticassert(ticks % 64 == 0, "precision not supported");
		OCR0A = (uint8_t)(ticks / 64 - 1);
		TCCR0B = _BV(CS01) | _BV(CS00);
	}
	else if (ticks <= (256*256LL)) {
		//staticassert(ticks % 256 == 0, "precision not supported");
		OCR0A = (uint8_t)(ticks / 256 - 1);
		TCCR0B = _BV(CS02);
	}
	else if (ticks <= (1024*256LL)) {
		//staticassert(ticks % 1024 == 0, "precision not supported");
		OCR0A = (uint8_t)(ticks / 1024 - 1);
		TCCR0B = _BV(CS02) | _BV(CS00);
	}
	//else {
	//	staticassert(ticks > (1024*256LL), "ticks value to large");
	//}
}

__attribute__((destructor)) static void timer0_stop(void)
{
        TCCR0B = 0;
        power_timer1_disable();
}

ISR(TIMER0_COMPA_vect, ISR_NAKED)
{
	system_tick();
	reti();
}

#ifdef __cplusplus
}
#endif

#endif // timer0_h
