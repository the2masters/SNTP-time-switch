#ifndef helper_h
#define helper_h

#if !defined ARCH_LITTLE_ENDIAN && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ARCH_LITTLE_ENDIAN 1
#elif !defined ARCH_BIG_ENDIAN && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ARCH_BIG_ENDIAN 1
#endif

#if !defined __has_include || __has_include(<LUFA/Common/Common.h>)
#define __INCLUDE_FROM_COMMON_H
#include <LUFA/Common/CompilerSpecific.h>
#include <LUFA/Common/Attributes.h>
#include <LUFA/Common/Endianness.h>
#endif

#undef MIN
#define MIN(x, y) ({typeof(x) _min1 = (x); typeof(y) _min2 = (y); (void) (&_min1 == &_min2); _min1 < _min2 ? _min1 : _min2; })
#undef MAX
#define MAX(x, y) ({typeof(x) _max1 = (x); typeof(y) _max2 = (y); (void) (&_max1 == &_max2); _max1 > _max2 ? _max1 : _max2; })

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define DIV_ROUND_DOWN(n, d) ((n) / (d))
#define ROUND_UP(n, d) (DIV_ROUND_UP(n, d) * (d))
#define ROUND_DOWN(n, d) (DIV_ROUND_DOWN(n, d) * (d))

#if 0 // The following ROUND_UP MACROS are untested, check for speed later
#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define ROUND_UP(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define ROUND_DOWN(x, y) ((x) & ~__round_mask(x, y))
#endif

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// Get a value from a initializer listing
#define GETBYTE_ARRAY(array, num) ((const uint8_t[]){array}[num])
// Get high or low byte from a word in big endian
#define GETBYTE_BE16(value, num) (num ? (uint8_t)BE16_TO_CPU(value) : (uint8_t)(BE16_TO_CPU(value) >> 8))

// Repeat an expression n times
#define REPEAT2(arg) do {arg; arg;} while(0)
#define REPEAT3(arg) do {arg; arg; arg;} while(0)
#define REPEAT4(arg) do {arg; arg; arg; arg;} while(0)
#define REPEAT5(arg) do {arg; arg; arg; arg; arg;} while(0)
#define REPEAT6(arg) do {arg; arg; arg; arg; arg; arg;} while(0)
#define REPEAT(n, arg) REPEAT ## n(arg)


#ifdef __AVR_ARCH__
// Force indirect access of pointer
#undef GCC_FORCE_POINTER_ACCESS
#define GCC_FORCE_POINTER_ACCESS(_ptr) __asm__ __volatile__("" : "=e" (_ptr) : "0" (_ptr))

#include <stdint.h>
typedef volatile uint8_t * HWADDR;

#ifdef __cplusplus
extern "C"
{
#endif

// Convert PORTx to DDRx
inline HWADDR CONVERT_PORT_TO_DDR(const HWADDR port)
{
	return port - 1;
}

// Convert PORTx to PINx
inline HWADDR CONVERT_PORT_TO_PIN(const HWADDR port)
{
	return port - 2;
}
#ifdef __cplusplus
}
#endif

#endif // __AVR_ARCH__

#endif // helper_h
