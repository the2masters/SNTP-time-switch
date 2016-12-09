#ifndef _RESOURCES_H_
#define _RESOURCES_H_

#include <stdint.h>
#include <stdbool.h>
#include "helper.h"

#define CIDR		24
#define IP_OWN		192, 168, 200, 40
#define IP_ROUTER	192, 168, 200, 3
#define IP_SNTP		192, 168, 200, 3
#define MAC_OWN		0x02, 0x00, 0x00, 0x00, 0x00, 0x40
#define MAC_BROADCAST	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
#define UDP_PORT	65432

#define PACKET_LEN_MAX	(14+576)
#define PACKET_LEN_MIN	(14+28) // Ethernet ohne CRC: 14 + ARP/IP+UDP/IP+ICMP: 28
//#define PACKET_LEN_MAX 12
//#define PACKET_LEN_MIN 4

#define PACKETBUFFER_LEN PACKET_LEN_MAX

//TODO: Change to ONE_DAY
#define SNTP_TimeBetweenQueries 300

typedef uint32_t IP_Address_t;
typedef uint16_t UDP_Port_t;

//Damit mir LUFA RNDIS nicht dazwischen funkt
#define _RNDIS_CLASS_H_
typedef struct
{
        uint8_t         Octets[6];
} __attribute__((packed)) MAC_Address_t;

#if CIDR >= 32
	#error CIDR too large
#elif CIDR >= 24
	typedef uint8_t IP_Hostpart_t;
#elif CIDR >= 16
	typedef uint16_t IP_Hostpart_t;
#else
	#warning is CIDR really < 16 ?
	typedef IP_Address_t IP_Hostpart_t;
#endif

#define NETMASK (~(uint32_t)(_BV(32 - CIDR) - 1))

ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2) ATTR_PURE ATTR_ALWAYS_INLINE
static inline bool IP_compareNet(const IP_Address_t *a, const IP_Address_t *b)
{
        return (*a & CPU_TO_BE32(NETMASK)) == (*b & CPU_TO_BE32(NETMASK));
}


ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1) ATTR_PURE ATTR_ALWAYS_INLINE
static inline IP_Hostpart_t IP_getHost(const IP_Address_t *ip)
{
	return (IP_Hostpart_t)be32_to_cpu(*ip);
}

#endif //_RESOURCES_H_
