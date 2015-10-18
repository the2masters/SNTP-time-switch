#ifndef _ICMP_H_
#define _ICMP_H_
#include <stdint.h>
#include <LUFA/Common/Common.h>

bool ICMP_ProcessPacket(uint8_t packet[], uint16_t length) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);

#endif
