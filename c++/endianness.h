#ifndef _ENDIANNESS_H_
#define _ENDIANNESS_H_

#include <stdint.h>

__attribute__((always_inline))
static constexpr uint8_t FromBytes(uint8_t a) { return a; }
__attribute__((always_inline))
static constexpr uint16_t FromBytes(uint8_t a, uint8_t b)
{
	return (uint16_t)a << 8 | (uint16_t)b;
}
__attribute__((always_inline))
static constexpr uint32_t FromBytes(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 | (uint32_t)d;
}
__attribute__((always_inline))
static constexpr uint64_t FromBytes(uint8_t a, uint8_t b, uint8_t c, uint8_t d, int8_t e, uint8_t f, uint8_t g, uint8_t h)
{
	return (uint64_t)a << 56 | (uint64_t)b << 48 | (uint64_t)c << 40 | (uint64_t)d << 32 | (uint64_t)e << 24 | (uint64_t)f << 16 | (uint64_t)g << 8 | (uint64_t)h;
}

template <typename basetype>
class ByteSwap {
	basetype value;

	__attribute__((always_inline))
	static constexpr basetype byteswap(basetype value)
	{
		static_assert(sizeof(basetype) == 1 || sizeof(basetype) == 2 || sizeof(basetype) == 4 || sizeof(basetype) == 8);
		if     (sizeof(basetype) == 2) return (basetype)__builtin_bswap16((uint16_t)value);
		else if(sizeof(basetype) == 4) return (basetype)__builtin_bswap32((uint32_t)value);
		else if(sizeof(basetype) == 8) return (basetype)__builtin_bswap64((uint64_t)value);
		else return value;
	}
public:
	__attribute__((always_inline))
	constexpr explicit ByteSwap(basetype value) : value(byteswap(value)) {}

	template <typename sourcetype>
	__attribute__((always_inline))
	constexpr explicit ByteSwap(const ByteSwap<sourcetype> &value) : ByteSwap((basetype)value) {}
	template <typename targettype>
	__attribute__((always_inline))
	constexpr explicit operator targettype() const { return byteswap(value); }

	__attribute__((always_inline))
	constexpr bool operator==(const ByteSwap& other) const { return value == other.value; }
	__attribute__((always_inline))
	constexpr bool operator!=(const ByteSwap& other) const { return !(*this == other); }

	__attribute__((always_inline))
	constexpr ByteSwap& operator&=(const ByteSwap& other) { value &= other.value; return *this; }
	__attribute__((always_inline))
	constexpr ByteSwap& operator|=(const ByteSwap& other) { value |= other.value; return *this; }
	__attribute__((always_inline))
	constexpr ByteSwap& operator^=(const ByteSwap& other) { value ^= other.value; return *this; }
	__attribute__((always_inline))
	friend constexpr ByteSwap operator~(ByteSwap rhs) { rhs.value = ~rhs.value; return rhs; }
	__attribute__((always_inline))
	friend constexpr ByteSwap operator&(ByteSwap lhs, const ByteSwap& rhs) { lhs &= rhs; return lhs; }
	__attribute__((always_inline))
	friend constexpr ByteSwap operator|(ByteSwap lhs, const ByteSwap& rhs) { lhs |= rhs; return lhs; }
	__attribute__((always_inline))
	friend constexpr ByteSwap operator^(ByteSwap lhs, const ByteSwap& rhs) { lhs ^= rhs; return lhs; }
};

#ifndef __BYTE_ORDER__
#error Cannot check Byte order
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
template <typename basetype> using LE = basetype;
template <typename basetype> using BE = ByteSwap<basetype>;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
template <typename basetype> using LE = ByteSwap<basetype>;
template <typename basetype> using BE = basetype;
#else
#error Unknown byte order
#endif

#endif // _ENDIANNESS_H_
