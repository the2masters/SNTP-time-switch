#ifndef helper_h
#define helper_h

#ifdef __cplusplus
extern "C"
{
#endif

#define MIN(x, y) ({typeof(x) _min1 = (x); typeof(y) _min2 = (y); (void) (&_min1 == &_min2); _min1 < _min2 ? _min1 : _min2; })
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

#define ATOMIC_WRITE ATOMIC_BLOCK(ATOMIC_RESTORESTATE)

#define ATTR_MAYBE_UNUSED __attribute__((unused))

#include <stdint.h>
typedef volatile uint8_t * HWADDR;

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

#endif // helper_h
