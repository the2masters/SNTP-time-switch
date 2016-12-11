#include "network.h"

uint32_t ip = OwnIPAddress;

inline uint16_t USB_Read24Byte_Check_GetLength(volatile uint8_t destinationBuffer[])
{
	uint8_t data;

	// Destination MAC
	*destinationBuffer++ = data = UEDATX;
	if(data == GETBYTE_ARRAY(0, MAC_OWN))			// Unicast
	{
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE_ARRAY(1, MAC_OWN)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE_ARRAY(2, MAC_OWN)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE_ARRAY(3, MAC_OWN)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE_ARRAY(4, MAC_OWN)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE_ARRAY(5, MAC_OWN)) return 0;
	}
	else if(data == GETBYTE_ARRAY(0, MAC_BROADCAST))	// Broadcast
	{
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE_ARRAY(1, MAC_BROADCAST)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE_ARRAY(2, MAC_BROADCAST)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE_ARRAY(3, MAC_BROADCAST)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE_ARRAY(4, MAC_BROADCAST)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE_ARRAY(5, MAC_BROADCAST)) return 0;
	}
	else return 0;

	// Source MAC
	REPEAT(6, *destinationBuffer++ = UEDATX);

	// EtherType
	_Static_assert(GETBYTE(1, ETHERTYPE_IPV4) == GETBYTE(1, ETHERTYPE_ARP), "Ethertypes don't share identical high byte");
	*destinationBuffer++ = data = UEDATX;
	if(data != GETBYTE(1, ETHERTYPE_IPV4)) return 0;
	*destinationBuffer++ = data = UEDATX;
	if(data == GETBYTE(0, ETHERTYPE_IPV4))		// IPv4
	{	// Version_IHL
		*destinationBuffer++ = data = UEDATX;
		if(data != IP_VERSION_IHL) return 0;	// Only support Packets without options

		// TypOfService
		*destinationBuffer++ = UEDATX;

		// TotalLength
		*destinationBuffer++ = data = UEDATX;
		uint16_t iplength = (uint16_t)data << 8;// Length is in BE16 format
		*destinationBuffer++ = data = UEDATX;
		iplength += data;
		iplength += 14;	// TODO: sizeof(Ethernet)
		if(iplength < (uint16_t)PACKET_LEN_MIN || iplength > (uint16_t)PACKET_LEN_MAX) return 0;

		// Identification
		REPEAT(2, *destinationBuffer++ = UEDATX);

		// Flags_FragmentOffset
		*destinationBuffer++ = data = UEDATX;	// Don't support fragmented packets, but allow don't fragment flag
		if(data & ~GETBYTE(1, IP_FLAGS_DONTFRAGMENT)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data & ~GETBYTE(0, IP_FLAGS_DONTFRAGMENT)) return 0;

		// TimeToLive
		*destinationBuffer++ = UEDATX;

		// Protocol
		*destinationBuffer++ = data = UEDATX;
		if(data != IP_PROTOCOL_ICMP &&		// Support only ICMP and UDP
		   data != IP_PROTOCOL_UDP) return 0;
		return iplength;
	}

	else if(data == GETBYTE(0, ETHERTYPE_ARP))	// ARP
	{	// HardwareType
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE(1, ARP_HARDWARE_ETHERNET)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE(0, ARP_HARDWARE_ETHERNET)) return 0;

		// ProtocolType
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE(1, ETHERTYPE_IPV4)) return 0;
		*destinationBuffer++ = data = UEDATX;
		if(data != GETBYTE(0, ETHERTYPE_IPV4)) return 0;

		// HardwareLen
		*destinationBuffer++ = data = UEDATX;
		if(data != 6) return 0;	// TODO: sizeof(MAC-Address)

		// ProtocolLen
		*destinationBuffer++ = data = UEDATX;
		if(data != 4) return 0;	// TODO: sizeof(IP-Address)

		// Operation
		REPEAT(2, *destinationBuffer++ = UEDATX);

		// SenderMAC (first 2 bytes)
		REPEAT(2, *destinationBuffer++ = UEDATX);

		return 28 + 14; // sizeof(ARP) + sizeof(Ethernet)
	}
	else return 0;
}
