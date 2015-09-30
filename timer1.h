#ifndef timer1_h
#define timer1_h

#include <stdint.h>
#include <avr/io.h>
#include <avr/power.h>
#include <time.h>
#include <avr/interrupt.h>

#ifdef __cplusplus
extern "C"
{
#endif

static inline void timer1_init(uint32_t ticks)
{
	power_timer1_enable();
	TCCR1B = 0;			// Timer ausschalten
	TCCR1A = 0;			// CTC Mode steht in TCCR1B
	TCNT1 = 0;			// Timer Reset
	TIFR1 = 0xff;			// Interrupt Flags zurücksetzen

	uint16_t max;
	uint8_t  prescaler;

	if(ticks <= 65536) {
		max = (uint16_t)(ticks - 1);
		prescaler = _BV(CS10);
	}
	else if (ticks <= (8*65536LL)) {
		//staticassert(ticks % 8 == 0, "precision not supported");
		max = (uint16_t)(ticks / 8 - 1);
		prescaler = _BV(CS11);
	}
	else if (ticks <= (64*65536LL)) {
		//staticassert(ticks % 64 == 0, "precision not supported");
		max = (uint16_t)(ticks / 64 - 1);
		prescaler = _BV(CS11) | _BV(CS10);
	}
	else if (ticks <= (256*65536LL)) {
		//staticassert(ticks % 256 == 0, "precision not supported");
		max = (uint16_t)(ticks / 256 - 1);
		prescaler = _BV(CS12);
	}
	else if (ticks <= (1024*65536LL)) {
		//_Static_assert(ticks % 1024 == 0, "precision not supported");
		max = (uint16_t)(ticks / 1024 - 1);
		prescaler = _BV(CS12) | _BV(CS10);
	}
	//else {
	//	staticassert(ticks > (1024*256LL), "ticks value to large");
	//}

#ifdef TIMER1_USE_ICR
	ICR1   = max;			// CTC Ende
	TIMSK1 = (1 << ICIE1);		// Interrupt einschalten
	TCCR1B = (_BV(WGM13) | _BV(WGM12) | prescaler;
#else
	OCR1A  = max;			// CTC Ende
	TIMSK1 = (1 << OCIE1A);		// Interrupt einschalten
	TCCR1B = _BV(WGM12) | prescaler;
#endif
}

__attribute__((destructor)) static void timer1_stop(void)
{
	TCCR1B = 0;
	power_timer1_disable();
}


#ifdef TIMER1_USE_ICR
ISR(TIMER1_CAPT_vect, ISR_NAKED)
#else
ISR(TIMER1_COMPA_vect, ISR_NAKED)
#endif
{
	system_tick();
	reti();
}

#ifdef __cplusplus
}
#endif

#endif // timer1_h
