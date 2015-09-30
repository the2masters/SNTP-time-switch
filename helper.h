#ifndef helper_h
#define helper_h

#ifdef __cplusplus
extern "C"
{
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

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
