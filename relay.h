#ifndef relay_h
#define relay_h
#include "helper.h"

#define define_relay(name, port, pin)			\
	__attribute__((constructor)) static void init_ ## name (void) 		\
	{						\
		port &= ~pin;				\
		*(CONVERT_PORT_TO_DDR(&port)) |= pin;	\
	}						\
	__attribute__((always_inline)) static inline void name ## _set(bool onoff) \
	{						\
		if(onoff) port |= pin;			\
		else port &= ~pin;			\
	}

#endif // relay_h
