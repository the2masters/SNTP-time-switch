#ifndef _ARP_H_
#define _ARP_H_
#include <stdint.h>
#include <LUFA/Common/Common.h>
#include "resources.h"

bool ARP_ProcessPacket(uint8_t packet[], uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
const MAC_Address_t* ARP_searchMAC(const IP_Address_t *IP) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
uint8_t ARP_GenerateBroadcastRequest(uint8_t packet[], const IP_Address_t *destinationIP) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1, 2);

#endif

