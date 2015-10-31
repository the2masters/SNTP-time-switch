#include "ASCII.h"
#include "UDP.h"

bool ASCII_ProcessReply(uint8_t packet[], uint16_t length, bool *value)
{
	if(length != 1)
		return false;

	switch(*packet)
	{
		case '0': *value = false; return true;
		case '1': *value = true; return true;
		default: return false;
	}
}

bool ASCII_ProcessRequest(uint8_t packet[], uint16_t length, bool value)
{
	if(length != 1)
		return false;

	*packet = value ? '1' : '0';
	return true;
}

int8_t ASCII_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort)
{
        int8_t offset = UDP_GenerateUnicast(packet, destinationIP, destinationPort, 1);
	if(offset < 0)
		return offset;

	*(packet + offset) = '?';

	return offset + 1;
}

uint8_t ASCII_GenerateBroadcast(uint8_t packet[], UDP_Port_t sourcePort, bool value)
{
        uint8_t offset = UDP_GenerateBroadcast(packet, sourcePort, 1);

	*(packet + offset) = value ? '1' : '0';

	return offset + 1;
}
