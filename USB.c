#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/cpufunc.h>
#include <util/atomic.h>

#define _RNDIS_CLASS_H_
#include <LUFA/Drivers/USB/USB.h>
#include "helper.h"
#include "Descriptors.h"
#include "resources.h"
#include "USB.h"

static volatile bool RequestTS = false;
static uint8_t Buffer[ETHERNET_FRAME_SIZE];
#define BufferEnd (Buffer + ARRAY_SIZE(Buffer))
static Packet_t current = {.data = Buffer, .len = 0};

static volatile enum {
	S_Ready,
	S_Sending,
	S_Sended,
	S_Receiving,
	S_Received,
} State = S_Ready;

static void USB_SendNextFragment(void)
{
	#if CDC_TXRX_EPSIZE > 255
	#  error change sendLength to uint16_t
	#endif

	for(uint8_t blocklen = MIN(current.len, CDC_TXRX_EPSIZE); blocklen; --blocklen)
		Endpoint_Write_8(*(current.data++));
	Endpoint_ClearIN();

	if(current.len < CDC_TXRX_EPSIZE)
	{
		State = S_Sended;
	} else {
		current.len -= CDC_TXRX_EPSIZE;
		_MemoryBarrier();
	}
}

static volatile uint8_t ConnectionStateIndex;
static const __flash struct {
	USB_Request_Header_t header;
	uint32_t data[2];
} ATTR_PACKED ConnectionState[] = {
	{
		.header = {
			.bmRequestType = (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE),
			.bRequest      = CDC_NOTIF_ConnectionSpeedChange,
			.wValue        = CPU_TO_LE16(0),
			.wIndex        = CPU_TO_LE16(INTERFACE_ID_CDC_CCI),
			.wLength       = CPU_TO_LE16(8),
		},
		.data = {CPU_TO_LE32(1000000), CPU_TO_LE32(1000000)}
	},
	{
		.header = {
			.bmRequestType = (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE),
			.bRequest      = CDC_NOTIF_NetworkConnection,
			.wValue        = CPU_TO_LE16(1),
			.wIndex        = CPU_TO_LE16(INTERFACE_ID_CDC_CCI),
			.wLength       = CPU_TO_LE16(0),
		}
	}
};

void EVENT_USB_Endpoint_Interrupt(void)
{
	if(Endpoint_HasEndpointInterrupted(CDC_TX_EPADDR))
	{
		Endpoint_SelectEndpoint(CDC_TX_EPADDR);
		if(USB_INT_IsEnabled(USB_INT_TXINI))
		{
			USB_INT_Clear(USB_INT_TXINI);

			if(State == S_Sending)
			{
				USB_INT_Disable(USB_INT_TXINI);

				NONATOMIC_BLOCK(NONATOMIC_FORCEOFF)
				{
					_MemoryBarrier();
					USB_SendNextFragment();
				}

				USB_INT_Enable(USB_INT_TXINI);
			} else { // State == S_Sended
				USB_EnableReceiver();
			}
		}
	}
	if(Endpoint_HasEndpointInterrupted(CDC_RX_EPADDR))
	{
		Endpoint_SelectEndpoint(CDC_RX_EPADDR);
		if(USB_INT_IsEnabled(USB_INT_RXOUTI))
		{
			USB_INT_Disable(USB_INT_RXOUTI);
			State = S_Receiving;
			GlobalInterruptEnable();

			uint8_t readLength = Endpoint_BytesInEndpoint();

			if(readLength > BufferEnd - current.data)
			{
				readLength = BufferEnd - current.data;
			}

			for(uint8_t i = readLength; i; --i)
				*(current.data++) = Endpoint_Read_8();
			Endpoint_ClearOUT();
			current.len += readLength;

			if(readLength < CDC_TXRX_EPSIZE)
			{
				State = S_Received;
				sleep_disable();

				_MemoryBarrier();
			} else {
				GlobalInterruptDisable();
				USB_INT_Enable(USB_INT_RXOUTI);
			}
		}
	}
	if(Endpoint_HasEndpointInterrupted(CDC_NOTIFICATION_EPADDR))
	{
		Endpoint_SelectEndpoint(CDC_NOTIFICATION_EPADDR);
		if(USB_INT_IsEnabled(USB_INT_TXINI))
		{
			USB_INT_Disable(USB_INT_TXINI);

			uint8_t index = --ConnectionStateIndex;

			_Static_assert(CDC_NOTIFICATION_EPSIZE >= sizeof(ConnectionState[0]), "CDC_NOTIFICATION_EPSIZE to small");

			NONATOMIC_BLOCK(NONATOMIC_FORCEOFF)
			{
				__flash const uint8_t *data = (__flash const uint8_t *)&ConnectionState[index];
				for(uint8_t i = (sizeof(USB_Request_Header_t) + le16_to_cpu(ConnectionState[index].header.wLength)); i; --i)
					Endpoint_Write_8(*(data++));

				Endpoint_ClearIN();
			}

			if(index)
				USB_INT_Enable(USB_INT_TXINI);
		}
	}
}

bool USB_isReady(void)
{
	return (USB_DeviceState == DEVICE_STATE_Configured);
}

bool USB_prepareTS(Packet_t *packet)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON)
		if(State == S_Ready)
		{
			Endpoint_SelectEndpoint(CDC_RX_EPADDR);
			USB_INT_Disable(USB_INT_RXOUTI);
			*packet = current;
			return true;
		} else {
			RequestTS = true;
			return false;
		}

	 //Silence warning
	__builtin_unreachable();
}

void USB_Send(Packet_t packet)
{
	State = S_Sending;
	current = packet;

	Endpoint_SelectEndpoint(CDC_TX_EPADDR);
	USB_INT_Disable(USB_INT_TXINI);

	USB_SendNextFragment();

	USB_INT_Enable(USB_INT_TXINI);
}

bool USB_Received(Packet_t *packet)
{
	if(State == S_Received)
	{
		_MemoryBarrier();
		//TODO: Mehrere Packete gleichzeitig?
		packet->data = Buffer;
		packet->len = current.len;
		return true;	
	} else {
		return false;
	}
}

void USB_EnableReceiver(void)
{
	State = S_Ready;
	if(RequestTS)
	{
		RequestTS = false;
	} else {
		ACCESS_ONCE(current.data) = Buffer;
		ACCESS_ONCE(current.len) = 0;
		Endpoint_SelectEndpoint(CDC_RX_EPADDR);
		USB_INT_Enable(USB_INT_RXOUTI);
	}
}

__attribute__((constructor)) static void init_USB(void)
{
	power_usb_enable();
	USB_Init();
}

__attribute__((destructor)) static void stop_USB(void)
{
	USB_Disable();
	power_usb_disable();
}

static void stopEndpoints(void)
{
	Endpoint_SelectEndpoint(CDC_RX_EPADDR);
	USB_INT_Disable(USB_INT_RXOUTI);
	Endpoint_DisableEndpoint();
	Endpoint_SelectEndpoint(CDC_TX_EPADDR);
	USB_INT_Disable(USB_INT_TXINI);
	Endpoint_DisableEndpoint();
	Endpoint_SelectEndpoint(CDC_NOTIFICATION_EPADDR);
	Endpoint_DisableEndpoint();
}

void EVENT_USB_Device_ConfigurationChanged(void)
{
	switch(USB_Device_ConfigurationNumber)
	{
	case 1:
		if(!Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPADDR, EP_TYPE_INTERRUPT, CDC_NOTIFICATION_EPSIZE, 1))
			return;
		ConnectionStateIndex = ARRAY_SIZE(ConnectionState);
		USB_INT_Enable(USB_INT_TXINI);

		if(!Endpoint_ConfigureEndpoint(CDC_TX_EPADDR, EP_TYPE_BULK, CDC_TXRX_EPSIZE, 1))
			return;
		USB_INT_Enable(USB_INT_TXINI);

		if(!Endpoint_ConfigureEndpoint(CDC_RX_EPADDR, EP_TYPE_BULK, CDC_TXRX_EPSIZE, 1))
			return;
		USB_INT_Enable(USB_INT_RXOUTI);

		return;
	case 2:
		stopEndpoints();
		exit(EXIT_SUCCESS);
		return;
	default:
		stopEndpoints();
		return;
	}
}

void EVENT_USB_Device_ControlRequest(void)
{
	switch (USB_ControlRequest.bRequest)
	{
		case CDC_REQ_SetEthernetPacketFilter:
			Endpoint_ClearSETUP();
			Endpoint_ClearStatusStage();
			break;
	}
}

