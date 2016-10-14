#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "helper.h"

#include <LUFA/Common/Common.h>

#define CIDR            24
#define IP_OWN          192, 168, 200, 40
#define IP_ROUTER       192, 168, 200, 3
#define IP_SNTP         192, 168, 200, 3
#define IP_BROADCAST	192, 168, 200, 255
#define MAC_OWN         0x02, 0x00, 0x00, 0x00, 0x00, 0x40
#define MAC_BROADCAST	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
#define UDP_PORT        65432

#define ETHERNET_FRAME_SIZE_MAX (14+576)
#define ETHERNET_FRAME_SIZE_MIN (14+28) // Ethernet ohne CRC: 14 + ARP/IP+UDP/IP+ICMP: 28

#define GETBYTE_ARRAY(array, num) ((const uint8_t[]){array}[num])
#define GETBYTE_BE16(value, num) (num ? (uint8_t)BE16_TO_CPU(value) : (uint8_t)(BE16_TO_CPU(value) >> 8))

#define REPEAT1(arg) arg
#define REPEAT2(arg) do {REPEAT1(arg); REPEAT1(arg);} while(0)
#define REPEAT3(arg) do {REPEAT2(arg); REPEAT1(arg);} while(0)
#define REPEAT4(arg) REPEAT2(REPEAT2(arg))
#define REPEAT6(arg) REPEAT3(REPEAT2(arg))
#define REPEAT24(arg) REPEAT6(REPEAT4(arg))
#define REPEAT(n, arg) REPEAT ## n(arg)

#include "PacketBuffer.h"
#include <avr/io.h>

typedef enum
{
        ETHERTYPE_IPV4 = CPU_TO_BE16(0x0800),
        ETHERTYPE_ARP = CPU_TO_BE16(0x0806)
} Ethertype_t;

typedef enum
{
        ARP_OPERATION_REQUEST   = CPU_TO_BE16(0x0001),
        ARP_OPERATION_REPLY     = CPU_TO_BE16(0x0002),
} ARP_Operation_t;

#define ARP_HARDWARE_ETHERNET   CPU_TO_BE16(0x0001)

typedef enum
{
        IP_PROTOCOL_ICMP = 1,
        IP_PROTOCOL_UDP = 17,
	IP_PROTOCOL_UDPLITE = 136,
} IP_Protocol_t;
#define IP_VERSION_IHL          0x45	// TODO: 0x40 | sizeof(IP_HEADER)
#define IP_FLAGS_DONTFRAGMENT           CPU_TO_BE16(0x4000)

inline uint16_t USB_Read24Byte_Check_GetLength(uint8_t destinationBuffer[])
{
	uint8_t data;
	// Read USB Register indirect. Saves about 80 Bytes
	HWADDR usbRegister = &UEDATX;
	FIX_POINTER(usbRegister);

	// Destination MAC
	*destinationBuffer++ = data = *usbRegister;
	if(data == GETBYTE_ARRAY(MAC_OWN, 0))			// Unicast
	{
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_ARRAY(MAC_OWN, 1)) return 0;
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_ARRAY(MAC_OWN, 2)) return 0;
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_ARRAY(MAC_OWN, 3)) return 0;
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_ARRAY(MAC_OWN, 4)) return 0;
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_ARRAY(MAC_OWN, 5)) return 0;
	}
	else if(data == GETBYTE_ARRAY(MAC_BROADCAST, 0))	// Broadcast
	{
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_ARRAY(MAC_BROADCAST, 1)) return 0;
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_ARRAY(MAC_BROADCAST, 2)) return 0;
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_ARRAY(MAC_BROADCAST, 3)) return 0;
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_ARRAY(MAC_BROADCAST, 4)) return 0;
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_ARRAY(MAC_BROADCAST, 5)) return 0;
	}
	else return 0;

	// Source MAC
	REPEAT(6, *destinationBuffer++ = *usbRegister);

	// EtherType
	*destinationBuffer++ = data = *usbRegister;
	if(data != GETBYTE_BE16(ETHERTYPE_IPV4, 0)) return 0;	// ETHERTYPE_IPV4 and ARP share same high byte
	*destinationBuffer++ = data = *usbRegister;
	if(data == GETBYTE_BE16(ETHERTYPE_IPV4, 1))		// IPv4
	{	// Version_IHL
		*destinationBuffer++ = data = *usbRegister;
		if((data & 0xF0) != (IP_VERSION_IHL & 0xF0)) return 0;

		// TypOfService
		*destinationBuffer++ = *usbRegister;

		// TotalLength
		*destinationBuffer++ = data = *usbRegister;
		uint16_t iplength = (uint16_t)data << 8;	// Length is in BE16 format
		*destinationBuffer++ = data = *usbRegister;
		iplength += data;
		if(iplength <= (PACKET_LEN_MIN - 14) || iplength > (PACKET_LEN_MAX - 14)) return 0;	// TODO: sizeof(Ethernet)

		// Identification
		REPEAT(2, *destinationBuffer++ = *usbRegister);	// TODO: FIELD_SIZEOF(IP_Packet, Identification)

		// Flags_FragmentOffset
		*destinationBuffer++ = data = *usbRegister;		// Don't support fragmented packets,
		if(data & ~GETBYTE_BE16(IP_FLAGS_DONTFRAGMENT, 0)) return 0;			// but allow don't fragment flag
		*destinationBuffer++ = data = *usbRegister;
		if(data & ~GETBYTE_BE16(IP_FLAGS_DONTFRAGMENT, 1)) return 0;

		// TimeToLive
		*destinationBuffer++ = *usbRegister;

		// Protocol
		*destinationBuffer++ = data = *usbRegister;
		if(data != IP_PROTOCOL_ICMP &&			// Support only ICMP and UDP(-lite)
		   data != IP_PROTOCOL_UDP &&
		   data != IP_PROTOCOL_UDPLITE) return 0;
		return iplength + 14;	// TODO: sizeof(Ethernet)
	}

	else if(data == GETBYTE_BE16(ETHERTYPE_ARP, 1))	// ARP
	{	// HardwareType
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_BE16(ARP_HARDWARE_ETHERNET, 0)) return 0;
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_BE16(ARP_HARDWARE_ETHERNET, 1)) return 0;

		// ProtocolType
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_BE16(ETHERTYPE_IPV4, 0)) return 0;
		*destinationBuffer++ = data = *usbRegister;
		if(data != GETBYTE_BE16(ETHERTYPE_IPV4, 1)) return 0;

		// HardwareLen
		*destinationBuffer++ = data = *usbRegister;
		if(data != 6) return 0;	// TODO: sizeof(MAC-Address)

		// ProtocolLen
		*destinationBuffer++ = data = *usbRegister;
		if(data != 4) return 0;	// TODO: sizeof(IP-Address)

		// Operation
		REPEAT(2, *destinationBuffer++ = *usbRegister);

		// SenderMAC (first 2 bytes)
		REPEAT(2, *destinationBuffer++ = *usbRegister);

		return 28 + 14; // sizeof(ARP) + sizeof(Ethernet)
	}
	else return 0;
}

int main(void)
{
uint8_t buffer[42];

uint16_t retVal = USB_Read24Byte_Check_GetLength(buffer);

//printf("ret=%d", retVal);
	return buffer[retVal];

}
