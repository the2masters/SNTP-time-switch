#include "Ethernet.h"
#include "IP.h"
#include "ARP.h"

Ethernet* Ethernet::Prepare(IP::Address destination, uint16_t length)
{
	const Address *mac = ARP::get(destination);
	if(mac)
	{
		Packet *packet = Packet_New(length);
		return (Ethernet *)(packet);
		
		length = sizeof(ARP);
	Packet *packet = Packet_New(length);
}

void Ethernet::Send()
{
	





typedef struct
{
	MAC_Address_t	Destination;
	MAC_Address_t	Source;
	uint16_t	EtherType;
	uint8_t		data[];
}  __attribute__((packed, may_alias)) Ethernet_Header_t;

bool Ethernet_ProcessPacket(Packet_t *packet)
{
	Ethernet_Header_t *Ethernet = (Ethernet_Header_t *)packet->data;

	bool reflect;
	switch (Ethernet->EtherType)
	{
		case CPU_TO_BE16(ETHERTYPE_ARP):
			reflect = ARP_ProcessPacket((ARP_Header_t *)__builtin_assume_aligned(Ethernet->data, alignof(Packet_t));
			break;
		case CPU_TO_BE16(ETHERTYPE_IPV4):
			reflect = IP_ProcessPacket((IP_Header_t *)__builtin_assume_aligned(Ethernet->data, alignof(Packet_t));
			break;
		default:
			return false;
	}

	if(reflect)
	{
		Ethernet->Destination = Ethernet->Source;
		MAC_Address_t ourMAC = {{MAC_OWN}};
		Ethernet->Source = ourMAC;
		return true;
	} else {
		return false;
	}
}

static uint8_t Ethernet_WriteHeader(uint8_t packet[], const MAC_Address_t *destinationMAC, Ethertype_t ethertype)
{
	Ethernet_Header_t *Ethernet = (Ethernet_Header_t *)packet;

	Ethernet->Destination	= *destinationMAC;
	Ethernet->Source	= OwnMACAddress;
	Ethernet->EtherType	= ethertype;

	return sizeof(Ethernet_Header_t);
}

int8_t Ethernet_GenerateUnicast(uint8_t packet[], const IP_Address_t *destinationIP, Ethertype_t ethertype)
{
	const MAC_Address_t *MAC = ARP_searchMAC(destinationIP);
	if(MAC != NULL)
	{
		return Ethernet_WriteHeader(packet, MAC, ethertype);
	} else {
		return -ARP_GenerateRequest(packet, destinationIP);
	}
}

uint8_t Ethernet_GenerateBroadcast(uint8_t packet[], Ethertype_t ethertype)
{
	return Ethernet_WriteHeader(packet, &BroadcastMACAddress, ethertype);
}
