#ifndef _USB_H_
#define _USB_H_
#include <stdint.h>
#include <LUFA/Common/Common.h>

typedef struct
{
	uint8_t *data;
	uint16_t len;
} Packet_t;

bool USB_prepareTS(Packet_t *packet) ATTR_WARN_UNUSED_RESULT;
void USB_Send(Packet_t packet);
bool USB_Received(Packet_t *packet) ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(1);
void USB_EnableReceiver(void);
bool USB_isReady(void) ATTR_WARN_UNUSED_RESULT ATTR_PURE;

#endif //_USB_H_
