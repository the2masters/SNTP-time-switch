#ifndef _IP_H_
#define _IP_H_

#include <stdint.h>
#include "../endianness.h"
#include "../helper.h"
//#include "../resources.h"

#ifndef NETMASK
#error NETMASK not defined
#endif

#ifndef IP_OWN
#error IP_OWN not defined
#endif

class IP : Ethernet {
// Types:
public:
	typedef BE<uint32_t> Address;
	#if GETBYTE_ARRAY(3, NETMASK) != 0 || GETBYTE_ARRAY(2, NETMASK) == 255
	typedef BE<uint8_t> Hostpart;
	#elif GETBYTE_ARRAY(2, NETMASK) != 0 || GETBYTE_ARRAY(1, NETMASK) == 255
	typedef BE<uint16_t> Hostpart;
	#else
	typedef BE<uint32_t> Hostpart;
	#endif

	enum class Protocol : uint8_t
	{
		ICMP = 1,
		UDP = 17,
	};

	class Checksum {
		uint16_t checksum;
	public:
//		constexpr void add(uint16_t word) { if(__builtin_add_overflow(*checksum, word, checksum)) (*checksum)++; }
//		constexpr checksum(const uint8_t data[], uint8_t length) :
	};

// IP Header
private:
	BE<uint8_t>	Version_IHL;
	BE<uint8_t>	TypeOfService;
	BE<uint16_t>	Length;

	BE<uint16_t>	Identification;
	BE<uint16_t>	FlagsFragment;

	BE<uint8_t>	TTL;
	BE<Protocol>	Protocol;
	Checksum	Checksum;

	Address	SourceAddress;
	Address	DestinationAddress;

// Methods
public:

// Static Methods
	static constexpr Address OwnAddress() { return Address(FromBytes(IP_OWN)); }
	static constexpr Address Subnet() { return OwnAddress() & Address(FromBytes(NETMASK)); }

//	static constexpr uint8_t HeaderLength = Ethernet::HeaderLength + 20;

//	static bool ProcessPacket(uint8_t packet[], uint16_t length);
//	static Packet_t *GenerateUnicast(Protocol protocol, Address destinationIP, uint16_t payloadLength);
//	static Packet_t *GenerateBroadcast(Protocol protocol, uint16_t payloadLength);
};

static_assert(sizeof(IP::Address) == 4, "Class IP::Address has wrong size");
static_assert(sizeof(IP) - sizeof(Ethernet) == 20, "Class IP has wrong size");

#endif // _IP_H_

