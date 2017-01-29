#include <stdint.h>
#include "../endianness.h"

#ifndef MAC_OWN
#error MAC_OWN not defined
#endif

class MACAddress {
	BE<uint16_t> word1, word2, word3;

public:
	__attribute__((always_inline))
	constexpr MACAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) : word1(FromBytes(a, b)), word2(FromBytes(c, d)), word3(FromBytes(e, f)) {}

	__attribute__((always_inline))
	constexpr bool operator==(const MACAddress& other) const { return word1 == other.word1 && word2 == other.word2 && word3 == other.word3; }
	__attribute__((always_inline))
	constexpr bool operator!=(const MACAddress& other) const { return !(*this == other); }

	static constexpr MACAddress Own = MACAddress(MAC_OWN);

};

static_assert(sizeof(MACAddress) == 6, "Class MACAddress has wrong size");
