#ifndef _UDP_H_
#define _UDP_H_
#include <stdint.h>
#include <LUFA/Common/Common.h>
#include "resources.h"

#define UDP_PORT_AUTOMAT CPU_TO_BE16(1024)

bool UDP_ProcessPacket(uint8_t packet[], const IP_Address_t *sourceIP, uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);
ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2) ATTR_WEAK bool UDP_Callback_Request(uint8_t packet[], const IP_Address_t *sourceIP, UDP_Port_t destinationPort, uint16_t length) {return false; }
ATTR_NON_NULL_PTR_ARG(1, 2) ATTR_WEAK void UDP_Callback_Reply(uint8_t packet[], const IP_Address_t *sourceIP, UDP_Port_t sourcePort, uint16_t length) {}

int8_t UDP_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort, uint16_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);
uint8_t UDP_GenerateBroadcastReply(uint8_t packet[], UDP_Port_t sourcePort, uint16_t payloadLength) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
#endif

