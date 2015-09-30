#include <avr/io.h>
#include <avr/power.h>
#include <avr/wdt.h>

__attribute__ ((naked, used, section (".init3")))
static void init3(void)
{
	MCUSR &= ~_BV(WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	// Disable all peripherals
	power_all_disable();
	ACSR &= ~_BV(ACD);
}

#define BOOTLOADERSIZE	4096
__attribute__ ((naked, used, section (".fini3")))
static void fini3(void)
{
	power_all_enable();
	wdt_disable();
	/* compute the address of the beginning of the bootloader section and
	 * convert to word address */
	uint32_t bladdr = (FLASHEND - BOOTLOADERSIZE) / 2 + 1;
	__asm__ __volatile__(
		"mov r30, %0" "\n\t"
		"mov r31, %1" "\n\t"
		"ijmp"
		:
		: "r" ((bladdr >> 0) & 0xff),
		  "r" ((bladdr >> 8) & 0xff)
		: "r30", "r31");
}
