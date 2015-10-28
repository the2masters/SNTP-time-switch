#ifndef _UDP_H_
#define _UDP_H_
#include <stdint.h>
#include "resources.h"

bool UDP_ProcessPacket(uint8_t packet[], const IP_Address_t *sourceIP, uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);

int8_t UDP_GenerateUnicast(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort, uint16_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);
uint8_t UDP_GenerateBroadcast(uint8_t packet[], UDP_Port_t sourcePort, uint16_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
#endif

