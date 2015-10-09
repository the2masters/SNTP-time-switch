#ifndef _RESOURCES_H_
#define _RESOURCES_H_

#include <stdint.h>
#include <time.h>

#define CIDR		24
#define IP_OWN		192, 168, 200, 40
#define IP_ROUTER	192, 168, 200, 3
#define IP_SNTP		192, 168, 200, 3
#define MAC_OWN		0x02, 0x00, 0x00, 0x00, 0x00, 0x40

#define ETHERNET_FRAME_SIZE	590
#define ETHERNET_FRAME_SIZE_MIN	(14+28) // Ethernet ohne CRC: 14 + ARP/IP+UDP/IP+ICMP: 28

typedef uint32_t IP_Address_t;
//Damit mir LUFA RNDIS nicht dazwischen funkt
#define _RNDIS_CLASS_H_
typedef struct
{
        uint8_t         Octets[6];
} __attribute__((packed)) MAC_Address_t;

extern uint8_t Packet[ETHERNET_FRAME_SIZE];
extern volatile uint16_t PacketLength;

extern const MAC_Address_t OwnMACAddress;
extern const IP_Address_t OwnIPAddress;
extern const IP_Address_t BroadcastIPAddress;
extern const IP_Address_t RouterIPAddress;
extern const IP_Address_t SNTPIPAddress;

#if CIDR >= 24
	typedef uint8_t IP_Hostpart_t;
#elif CIDR >= 16
	typedef uint16_t IP_Hostpart_t;
#elif CIDR >= 8
	typedef __uint24 IP_Hostpart_t;
#else
	#warning is CIDR really < 8 ?
	typedef IP_Address_t IP_Hostpart_t;
#endif

#define NETMASK_BE CPU_TO_BE32(~(uint32_t)(_BV(32 - CIDR) - 1))

extern time_t reloadtime;
extern time_t nextCalc;

#endif //_RESOURCES_H_
