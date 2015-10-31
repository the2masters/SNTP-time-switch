#ifndef _RESOURCES_H_
#define _RESOURCES_H_

#include <stdint.h>
#include <time.h>
#include <LUFA/Common/Common.h>

#define CIDR		24
#define IP_OWN		192, 168, 200, 40
#define IP_ROUTER	192, 168, 200, 3
#define IP_SNTP		192, 168, 200, 3
#define MAC_OWN		0x02, 0x00, 0x00, 0x00, 0x00, 0x40
#define UDP_PORT	65432

#define ETHERNET_FRAME_SIZE	590
#define ETHERNET_FRAME_SIZE_MIN	(14+28) // Ethernet ohne CRC: 14 + ARP/IP+UDP/IP+ICMP: 28

//TODO: Change to ONE_DAY
#define SNTP_TimeBetweenQueries 30

typedef uint32_t IP_Address_t;
typedef uint16_t UDP_Port_t;
//Damit mir LUFA RNDIS nicht dazwischen funkt
#define _RNDIS_CLASS_H_
typedef struct
{
        uint8_t         Octets[6];
} __attribute__((packed)) MAC_Address_t;

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

bool IP_compareNet(const IP_Address_t *a, const IP_Address_t *b) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2) ATTR_PURE;

ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1) ATTR_PURE ATTR_ALWAYS_INLINE
static inline IP_Hostpart_t IP_getHost(const IP_Address_t *ip)
{
	return (IP_Hostpart_t)be32_to_cpu(*ip);
}

#endif //_RESOURCES_H_
