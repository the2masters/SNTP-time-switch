#ifndef _USB_H_
#define _USB_H_
#include <stdint.h>
#include <LUFA/Drivers/USB/USB.h>
#include "Descriptors.h"

// Should be called, after a packet is put into Output chain.
static inline void USB_EnableTransmitter(void)
{
        Endpoint_SelectEndpoint(CDC_TX_EPADDR);
        USB_INT_Enable(USB_INT_TXINI);
}

// Should be called, after a packet is released from PacketBuffer.
static inline void USB_EnableReceiver(void)
{
        Endpoint_SelectEndpoint(CDC_RX_EPADDR);
        USB_INT_Enable(USB_INT_RXOUTI);
}

#endif //_USB_H_
