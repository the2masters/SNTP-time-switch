#include <stdint.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <assert.h>
#include <avr/interrupt.h>

__attribute__ ((naked, used, section (".init3")))
static void init3(void)
{
	MCUSR &= ~_BV(WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_2);

	// Disable all peripherals
	power_all_disable();
	ACSR &= ~_BV(ACD);
}

//#define BOOTLOADERWORDS	1024
__attribute__ ((naked, used, section (".fini3")))
static void fini3(void)
{
	power_all_enable();
	wdt_disable();
	cli();
	uint16_t bootloaderwords;
	#ifdef BOOTLOADERWORDS
		bootloaderwords = BOOTLOADERWORDS;
	#else
		// Get bootloadersize from fuses: Select which FUSE contains BOOTSZ0/1 and set BOOTLOADERWORDS to the lowest possible bootloader size in words
		#if defined(__AVR_AT90USB1287__)
		#define FUSE GET_HIGH_FUSE_BITS
		#define BOOTLOADERMINWORDS 512
		#elif defined(__AVR_AT90USB162__) || defined(__AVR_ATmega32U2__) || defined(__AVR_ATmega32U4__)
		#define FUSE GET_HIGH_FUSE_BITS
		#define BOOTLOADERMINWORDS 256
		#else
		#error unknown chip, please add code to read bootloader size from your Atmega or set BOOTLOADERWORDS
		#endif

		uint8_t shift = ((uint8_t)~boot_lock_fuse_bits_get(FUSE) & ((uint8_t)~FUSE_BOOTSZ0 | (uint8_t)~FUSE_BOOTSZ1)) / (uint8_t)~FUSE_BOOTSZ0;
		bootloaderwords = (uint16_t)BOOTLOADERMINWORDS << shift;
	#endif

	typedef void (*AppPtr_t)(void) __attribute__ ((noreturn));
	AppPtr_t bootloader = (AppPtr_t)((uint16_t)((uint32_t)FLASHEND / 2) - bootloaderwords + 1);

	bootloader();
}
