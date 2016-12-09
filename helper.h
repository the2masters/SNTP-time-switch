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




#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))






#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif



// Macro tricks:
// Call a macro indirectly for parameter expansion
#define INDIRECT(_NAME, ...) _NAME(__VA_ARGS__)

// Count number of arguments to a variadic macro
#define VA_NARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) N
#define VA_NARGS(...) VA_NARGS_IMPL(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)



// Repeat a command n times (like a compile time for loop)
// Can be called with 2, 3 or 4 arguments
#define REPEAT(...) INDIRECT(REPEAT_IMPL, VA_NARGS(__VA_ARGS__), __VA_ARGS__)
#define REPEAT_IMPL(nargs, ...) REPEAT_IMPL ## nargs(__VA_ARGS__)
// 2 args: call cmd n times
#define REPEAT_IMPL2(n, cmd) REPEAT ## n(cmd)
// 3 args: init some variable, call cmd n times
#define REPEAT_IMPL3(n, init, cmd) do {init; REPEAT_IMPL2(n, cmd);} while (0)
// 4 args: init some variable, call cmd and cmd2 n times
// example: REPEAT(6, int i = 0, printf("%d\n", i), i++)
#define REPEAT_IMPL4(n, init, cmd, cmd2) REPEAT_IMPL3(n, init, do {cmd; cmd2;} while (0))

// Dumb way to call something n times, it's impossible to forcefully unroll a for loop
#define REPEAT1(arg) do {arg;} while(0)
#define REPEAT2(arg) do {arg; arg;} while(0)
#define REPEAT3(arg) do {arg; arg; arg;} while(0)
#define REPEAT4(arg) do {arg; arg; arg; arg;} while(0)
#define REPEAT5(arg) do {arg; arg; arg; arg; arg;} while(0)
#define REPEAT6(arg) do {arg; arg; arg; arg; arg; arg;} while(0)
#define REPEAT7(arg) do {arg; arg; arg; arg; arg; arg; arg;} while(0)
#define REPEAT8(arg) do {arg; arg; arg; arg; arg; arg; arg; arg;} while(0)
#define REPEAT24(arg) do {REPEAT3(REPEAT8(arg));} while(0)



// Copy all values from a initializer listing into array dest
#define COPY_ARRAY(length, dest, ...) REPEAT(length, size_t i = 0, dest[i] = GETBYTE_ARRAY(i, __VA_ARGS__), i++)

// Get a byte from a bigger value (bytes counted from LSB)
#define GETBYTE(num, value) ((uint8_t)(value >> (num * 8)))
// Get a value from a initializer listing
#define GETBYTE_ARRAY(num, ...) ((const uint8_t[]){__VA_ARGS__}[num])



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
static inline HWADDR CONVERT_PORT_TO_DDR(const HWADDR port)
{
	return port - 1;
}

// Convert PORTx to PINx
static inline HWADDR CONVERT_PORT_TO_PIN(const HWADDR port)
{
	return port - 2;
}
#ifdef __cplusplus
}
#endif

#endif // __AVR_ARCH__

#endif // helper_h
