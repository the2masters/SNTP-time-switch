#ifndef _SNTP_H_
#define _SNTP_H_
#include <stdint.h>
#include <time.h>
#include <LUFA/Common/Common.h>
#include "resources.h"

time_t SNTP_ProcessPacket(uint8_t packet[], uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
int8_t SNTP_GenerateRequest(uint8_t packet[], const IP_Address_t *destinationIP, UDP_Port_t destinationPort) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);
#endif
