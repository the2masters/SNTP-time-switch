#ifndef _ASCII_H_
#define _ASCII_H_
#include <stdint.h>
#include "resources.h"

bool ASCII_ProcessReply(uint8_t packet[], uint16_t length, bool *value) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 3);
bool ASCII_ProcessRequest(uint8_t packet[], uint16_t length, bool value) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);

int8_t ASCII_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);
uint8_t ASCII_GenerateBroadcast(uint8_t packet[], UDP_Port_t sourcePort, bool value) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
#endif
