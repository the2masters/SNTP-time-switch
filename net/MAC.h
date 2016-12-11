#include <stdint.h>
#include "../endianness.h"

#ifndef MAC_OWN
#error MAC_OWN not defined
#endif

class MAC {
	uint16_t be16[3];

public:
	__attribute__((always_inline))
	constexpr MAC(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) : be16{ByteToBE16(a, b), ByteToBE16(c, d), ByteToBE16(e, f)} {}

	__attribute__((always_inline))
	constexpr bool operator==(const MAC& other) const { return be16[0] == other.be16[0] && be16[1] == other.be16[1] && be16[2] == other.be16[2]; }
	__attribute__((always_inline))
	constexpr bool operator!=(const MAC& other) const { return !(*this == other); }
	__attribute__((always_inline))
	constexpr MAC& operator=(const MAC& other) { be16[0] = other.be16[0]; be16[1] = other.be16[1]; be16[2] = other.be16[2]; return *this; }

	__attribute__((always_inline))
	static constexpr MAC Own() { return MAC(MAC_OWN); }

};
