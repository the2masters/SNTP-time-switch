#include <stdint.h>
#include "../endianness.h"
#include "../helper.h"

#ifndef NETMASK
#error NETMASK not defined
#endif

#ifndef IP_OWN
#error IP_OWN not defined
#endif

class IPAddress {
	uint32_t be32;

	explicit constexpr IPAddress(uint32_t be32) : be32{be32} {}
public:
	constexpr IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : be32{ByteToBE32(a, b, c, d)} {}

	constexpr bool operator==(const IPAddress& other) const { return be32 == other.be32; }
	constexpr bool operator!=(const IPAddress& other) const { return !(*this == other); }
	constexpr IPAddress& operator=(const IPAddress& other) { be32 = other.be32; return *this; }

	constexpr IPAddress subnet(void) const { return IPAddress(be32 & IPAddress(NETMASK).be32); }

	static constexpr IPAddress Own() { return IPAddress(IP_OWN); }

	class Hostpart {
	#if GETBYTE_ARRAY(3, NETMASK) != 0 || GETBYTE_ARRAY(2, NETMASK) == 255
		uint8_t hostpart;
	public:
		explicit constexpr Hostpart(IPAddress ip) : hostpart(BE32ToBE8(ip.be32) & BE32ToBE8(~IPAddress(NETMASK).be32)) {}
	#elif GETBYTE_ARRAY(2, NETMASK) != 0 || GETBYTE_ARRAY(1, NETMASK) == 255
		uint16_t hostpart;
	public:
		explicit constexpr Hostpart(IPAddress ip) : hostpart(BE32ToBE16(ip.be32) & BE32ToBE16(~IPAddress(NETMASK).be32)) {}
	#else
		uint32_t hostpart;
	public:
		explicit constexpr Hostpart(IPAddress ip) : hostpart(ip.be32 & ~IPAddress(NETMASK).be32) {}
	#endif
		constexpr bool operator==(const Hostpart& other) const { return hostpart == other.hostpart; }
		constexpr bool operator!=(const Hostpart& other) const { return !(*this == other); }
		constexpr Hostpart& operator=(const Hostpart& other) { hostpart = other.hostpart; return *this; }
	};
	constexpr Hostpart hostpart(void) const { return Hostpart(*this); }
};

class IP {
public:
	enum class Protocol : uint8_t
	{
		ICMP = 1,
		UDP = 17,
	};
};

