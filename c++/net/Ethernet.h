#ifndef _ETHERNET_H_
#define _ETHERNET_H_
#include <stdint.h>
#include "endianness.h"
//#include "resources.h"

#ifndef MAC_OWN
#error MAC_OWN not defined
#endif

#include <stdlib.h>
void * operator new(size_t size)
{
	return malloc(size);
}
void operator delete(void *ptr)
{
	free(ptr);
}
void operator delete(void *ptr,  __attribute__((unused)) size_t size)
{
	operator delete(ptr);
}

struct Packet {
	void* operator new(size_t size, size_t count = 0) { return ::operator new(size + count); }
//	void operator delete( void* p ) { ::operator delete ( p ); };
};

class Ethernet : public Packet
{
// Types:
public:
	class Address {
public:
		BE<uint16_t> word1, word2, word3;
	public:
		__attribute__((always_inline))
		constexpr Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) : word1(FromBytes(a, b)), word2(FromBytes(c, d)), word3(FromBytes(e, f)) {}
		__attribute__((always_inline))
		constexpr bool operator==(const Address& other) const { return word1 == other.word1 && word2 == other.word2 && word3 == other.word3; }
		__attribute__((always_inline))
		constexpr bool operator!=(const Address& other) const { return !(*this == other); }
	};

	enum class Protocol : uint16_t
	{
		IPV4 = 0x0800,
		ARP = 0x0806,
	};

// Ethernet Header
private:
public:
	Address		destination;
	Address		source;
	BE<Protocol>	protocol;

// Methods
	constexpr Ethernet() : destination(0, 0, 0, 0, 0, 0), source(0, 0, 0, 0, 0, 0), protocol(BE<Protocol>(Protocol::IPV4)) {}

// Static Methods
	static constexpr Address OwnAddress() { return Address(MAC_OWN); }

	static bool ProcessPacket(uint8_t packet[], uint16_t length);
//	static int8_t GenerateUnicast(uint8_t packet[], IP::Address destinationIP, Ethertype ethertype);
	static uint8_t GenerateBroadcast(uint8_t packet[], Protocol ethertype);
};


static_assert(sizeof(Ethernet::Address) == 6, "Class Ethernet::Address has wrong size");
static_assert(sizeof(Ethernet) == 14, "Class Ethernet has wrong size");


#endif // _ETHERNET_H_
