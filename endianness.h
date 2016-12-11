#ifndef _ENDIANNESS_H_
#define _ENDIANNESS_H_

#ifndef __BYTE_ORDER__
#error Cannot check Byte order
#endif
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error Currently there is only support for little endian architectures
#endif

#ifndef __cplusplus
#define constexpr
#endif

#include <stdint.h>

__attribute__((always_inline, const))
static inline constexpr uint16_t ByteToBE16(uint8_t a, uint8_t b)
{
	return ((uint16_t)a << 0) | ((uint16_t)b << 8);
}

__attribute__((always_inline, const))
static inline constexpr uint32_t ByteToBE32(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	return ((uint32_t)a << 0) | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}

__attribute__((always_inline, const))
static inline constexpr uint16_t BE32ToBE16(uint32_t be32)
{
	return be32 >> 16;
}

__attribute__((always_inline, const))
static inline constexpr uint8_t BE32ToBE8(uint32_t be32)
{
	return be32 >> 24;
}



#endif //_ENDIANNESS_H_
