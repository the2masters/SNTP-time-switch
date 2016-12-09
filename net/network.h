#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>

typedef enum
{
	ETHERTYPE_IPV4 = 0x0800,
	ETHERTYPE_ARP = 0x0806,
} Ethertype_t;

typedef enum
{
	ARP_OPERATION_REQUEST	= 0x0001,
	ARP_OPERATION_REPLY	= 0x0002,
} ARP_Operation_t;

#define ARP_HARDWARE_ETHERNET   0x0001

typedef enum
{
	IP_PROTOCOL_ICMP = 1,
	IP_PROTOCOL_UDP = 17,
} IP_Protocol_t;

// Default IP Header without options:
#define IP_VERSION_IHL			0x45
#define IP_DEFAULT_TTL			64
#define IP_FLAGS_DONTFRAGMENT		0x4000

bool ARP_ProcessPacket(uint8_t packet[], uint16_t length);
const MAC_Address_t* ARP_searchMAC(const IP_Address_t *IP);
uint8_t ARP_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP);

bool Ethernet_ProcessPacket(uint8_t packet[], uint16_t length);
int8_t Ethernet_GenerateUnicast(uint8_t packet[], const IP_Address_t *destinationIP, Ethertype_t ethertype);
uint8_t Ethernet_GenerateBroadcast(uint8_t packet[], Ethertype_t ethertype);

bool ICMP_ProcessPacket(uint8_t packet[]);

bool IP_ProcessPacket(uint8_t packet[], uint16_t length);
int8_t IP_GenerateUnicast(uint8_t packet[], IP_Protocol_t protocol, const IP_Address_t *destinationIP, uint8_t payloadLength);
uint8_t IP_GenerateBroadcast(uint8_t packet[], IP_Protocol_t protocol, uint8_t payloadLength);

void IP_ChecksumAdd(uint16_t *checksum, uint16_t word);
uint16_t IP_Checksum(const void *data, uint16_t length);

#include <time.h>
time_t SNTP_ProcessPacket(uint8_t packet[], uint16_t length);
int8_t SNTP_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort);

bool UDP_ProcessPacket(uint8_t packet[], const IP_Address_t *sourceIP, uint16_t length);

int8_t UDP_GenerateUnicast(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort, uint16_t payloadLength);
uint8_t UDP_GenerateBroadcast(uint8_t packet[], UDP_Port_t sourcePort, uint16_t payloadLength);

#endif
