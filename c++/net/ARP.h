#ifndef _ARP_H_
#define _ARP_H_

#include <stdint.h>
#include "../endianness.h"
#include "Ethernet.h"
#include "IP.h"
//#include "resources.h"

class ARP : public Ethernet {
// Types:
public:
	enum class Hardware : uint16_t
	{
		Ethernet = 0x0001
	};
	typedef Ethernet::Protocol Protocol;

	enum class Operation : uint16_t
	{
		Request = 0x0001,
		Reply = 0x0002
	};

// ARP Packet
private:
	BE<Hardware>		hardware;
	BE<Protocol>		protocol;

	BE<uint8_t>		hardwareLength;
	BE<uint8_t>		protocolLength;
	BE<Operation>		operation;

	Ethernet::Address	senderMAC;
	IP::Address		senderIP;
	Ethernet::Address	targetMAC;
	IP::Address		targetIP;

// Methods

// Static Methods

//	bool ARP_ProcessPacket(uint8_t packet[], uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
//	const MAC_Address_t* ARP_searchMAC(const IP_Address_t *IP) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
//	uint8_t ARP_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);

};

static_assert(sizeof(ARP) - sizeof(Ethernet) == 28);

#endif // _ARP_H_
