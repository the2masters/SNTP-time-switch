#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <avr/power.h>
#include <avr/cpufunc.h>
#include <util/atomic.h>

#define _RNDIS_CLASS_H_
#include <LUFA/Drivers/USB/USB.h>
#include "helper.h"
#include "Descriptors.h"
#include "resources.h"
#include "USB.h"

static volatile bool RequestTS = false;
static volatile uint16_t USBPos = 0;

static volatile enum {
	S_Ready,
	S_Sending,
	S_Sended,
	S_Receiving,
	S_Received,
} State = S_Ready;

static void USB_SendNextFragment(void)
{
	_MemoryBarrier();
	uint8_t sendLength;
	if(PacketLength < CDC_TXRX_EPSIZE)
	{
		sendLength = (uint8_t)PacketLength;
		State = S_Sended;
	} else {
		sendLength = CDC_TXRX_EPSIZE;
	}
	uint16_t USBPosTemp = USBPos;
	for(uint8_t i = sendLength; i; --i)
		Endpoint_Write_8(Packet[USBPosTemp++]);
	Endpoint_ClearIN();

	USBPos = USBPosTemp;
	PacketLength -= sendLength;
}

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
					USB_SendNextFragment();

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

			#if CDC_TXRX_EPSIZE < ETHERNET_FRAME_SIZE_MIN
			#error CDC_TXRX_EPSIZE to small
			#endif

			uint16_t posTemp = USBPos;
			if(readLength > ARRAY_SIZE(Packet) - posTemp)
			{
				readLength = ARRAY_SIZE(Packet) - posTemp;
			}
			if(posTemp || readLength >= ETHERNET_FRAME_SIZE_MIN)
			{
				for(uint8_t i = readLength; i; --i)
					Packet[posTemp++] = Endpoint_Read_8();
			}
			Endpoint_ClearOUT();

			if(readLength < CDC_TXRX_EPSIZE)
			{
				USBPos = 0;
				PacketLength = posTemp;
				State = S_Received;
				_MemoryBarrier();
			} else {
				USBPos = posTemp;
				GlobalInterruptDisable();
				USB_INT_Enable(USB_INT_RXOUTI);
			}
		}
	}
}

bool USB_isReady(void)
{
	return (USB_DeviceState == DEVICE_STATE_Configured);
}

bool USB_prepareTS(void)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON)
		if(State == S_Ready)
		{
			Endpoint_SelectEndpoint(CDC_RX_EPADDR);
			USB_INT_Disable(USB_INT_RXOUTI);
			return true;
		} else {
			RequestTS = true;
			return false;
		}

	// Silence warning
	__builtin_unreachable();
}

void USB_Send(void)
{
	State = S_Sending;
	USBPos = 0;

	Endpoint_SelectEndpoint(CDC_TX_EPADDR);
	USB_INT_Disable(USB_INT_TXINI);

	USB_SendNextFragment();

	USB_INT_Enable(USB_INT_TXINI);
}

bool USB_Received(void)
{
	_MemoryBarrier();
	return (State == S_Received);
}

void USB_EnableReceiver(void)
{
	USBPos = 0;
	State = S_Ready;
	if(RequestTS)
	{
		RequestTS = false;
	} else {
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

