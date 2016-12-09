
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <avr/power.h>
#include <avr/sleep.h>
#include <util/atomic.h>

#define _RNDIS_CLASS_H_
#include <LUFA/Drivers/USB/USB.h>

#include "PacketBuffer.h"
#include "helper.h"
#include "Descriptors.h"
#include "resources.h"

#include "USB.h"

#include "net/PacketCheck.c"

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


static inline void error(volatile uint8_t *counter)
{
#ifndef NDEBUG
	uint8_t copy = *counter;
	if(!++copy) --copy;
	*counter = copy;
#endif
}
static volatile uint8_t errRXShort = 0;
static volatile uint8_t errRXIPlong = 0;
static volatile uint8_t errRXIPdontcare = 0;

void EVENT_USB_Endpoint_Interrupt(void)
{
	// Disable all USB endpoint interrupts, then enable global interrupts
	Endpoint_SelectEndpoint(CDC_NOTIFICATION_EPADDR);
	bool enableNO = USB_INT_IsEnabled(USB_INT_TXINI);
	if(enableNO) USB_INT_Disable(USB_INT_TXINI);

	Endpoint_SelectEndpoint(CDC_RX_EPADDR);
	bool enableRX = USB_INT_IsEnabled(USB_INT_RXOUTI);
	if(enableRX) USB_INT_Disable(USB_INT_RXOUTI);

	Endpoint_SelectEndpoint(CDC_TX_EPADDR);
	bool enableTX = USB_INT_IsEnabled(USB_INT_TXINI);
	if(enableTX) USB_INT_Disable(USB_INT_TXINI);

	NONATOMIC_BLOCK(NONATOMIC_FORCEOFF)
	{
		// Send packets from output queue
		if(enableTX && Endpoint_IsINReady())
		{
			static Packet_t *packet = NULL;
			static volatile uint8_t *reader;
			static uint16_t bytesRemaining;

			if(!packet)
			{
				packet = Packet_GetOutput();
				if(packet)
				{
					reader = packet->data;
					bytesRemaining = Packet_getLen(packet->state);
				} else {
					enableTX = false;
				}
			}

			if(packet)
			{
				bool last = (bytesRemaining < CDC_TXRX_EPSIZE);
				uint8_t writeLength = last ? bytesRemaining : CDC_TXRX_EPSIZE;
				bytesRemaining -= writeLength;
				while(writeLength--)
                                        Endpoint_Write_8(*reader++);
				Endpoint_ClearIN();

				if(last)
				{
					Packet_ReleaseOutput(packet);
					packet = NULL;
					enableRX = true;
				}
			}
		}

		// Receive packets into input queue
		if(enableRX && (Endpoint_SelectEndpoint(CDC_RX_EPADDR), Endpoint_IsOUTReceived()))
		{
			PORTD |= 0x01;

			static Packet_t *packet;
			static volatile uint8_t *writer;
			static uint8_t receiveBuffer[24];
			static uint16_t bytesRemaining;
			static bool last;
			static enum {
				NEEDSPACE = 0,
				WAITING = 1,
				READING = 2,
			} state = WAITING;

			_Static_assert(CDC_TXRX_EPSIZE <= UINT8_MAX, "Change usbLen to uint16_t");
			uint8_t usbLen = Endpoint_BytesInEndpoint();
			if(state) // READING || WAITING
				last = (usbLen < CDC_TXRX_EPSIZE);

			if(state == WAITING)
			{
				_Static_assert(CDC_TXRX_EPSIZE >= PACKET_LEN_MIN, "CDC_TXRX_EPSIZE to small");
				if(usbLen >= PACKET_LEN_MIN)
				{
					bytesRemaining = USB_Read24Byte_Check_GetLength(receiveBuffer);
					if(bytesRemaining > (uint16_t)usbLen && last)
					{	// IP Header told us about a larger packet, drop it
						error(&errRXIPlong);
						Endpoint_ClearOUT();
					}
					else if(bytesRemaining > 0)
					{	// Accept Packet
						state = NEEDSPACE;
						usbLen -= 24;
					}
					else
					{ // Packet is not for us, read all parts of it
						error(&errRXIPdontcare);
						state = READING;
					}
				} else { // usbLen < PACKET_LEN_MIN => cannot be a valid packet
					error(&errRXShort);
					Endpoint_ClearOUT();
				}
			}

			if(state == NEEDSPACE)
			{
				packet = Packet_New(bytesRemaining);
				if(packet)
				{
					writer = packet->data;
					uint8_t *reader = receiveBuffer;
					REPEAT(24, *writer++ = *reader++);
					bytesRemaining -= 24;
					state = READING;
				} else { // No space in PacketBuffer
					enableRX = false;
				}
			}

			if(state == READING)
			{
				uint8_t readLength = (uint8_t)MIN((uint16_t)usbLen, bytesRemaining);
				bytesRemaining -= readLength;
				while(readLength--)
					*writer++ = Endpoint_Read_8();

				if(last)
				{
					if(packet)
					{
						if(bytesRemaining)
						{
							error(&errRXShort);
							Packet_ReleaseInput(packet);
						} else {
							sleep_disable();
							Packet_PutInput(packet);
						}
						packet = NULL;
					}
					state = WAITING;
				}
				Endpoint_ClearOUT();
			}

			PORTD &= ~0x01;
		}

		if(enableNO && (Endpoint_SelectEndpoint(CDC_NOTIFICATION_EPADDR), Endpoint_IsINReady()))
		{
			uint8_t index = --ConnectionStateIndex;
			if(!index)
				enableNO = false;

			_Static_assert(CDC_NOTIFICATION_EPSIZE >= sizeof(ConnectionState[0]), "CDC_NOTIFICATION_EPSIZE to small");

			__flash const uint8_t *data = (__flash const uint8_t *)&ConnectionState[index];
			for(uint8_t i = (sizeof(USB_Request_Header_t) + le16_to_cpu(ConnectionState[index].header.wLength)); i; --i)
				Endpoint_Write_8(*(data++));

			Endpoint_ClearIN();
		}
	}

	if(enableNO) USB_INT_Enable(USB_INT_TXINI);
	if(enableRX) USB_EnableReceiver();
	if(enableTX) USB_EnableTransmitter();
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

void EVENT_USB_Device_ConfigurationChanged(void)
{
	if(USB_Device_ConfigurationNumber == 1)
	{
		if(!Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPADDR, EP_TYPE_INTERRUPT, CDC_NOTIFICATION_EPSIZE, 1))
			return;
		ConnectionStateIndex = ARRAY_SIZE(ConnectionState);
		USB_INT_Enable(USB_INT_TXINI);

		if(!Endpoint_ConfigureEndpoint(CDC_TX_EPADDR, EP_TYPE_BULK, CDC_TXRX_EPSIZE, CDC_TXRX_BANKS))
			return;
		USB_INT_Enable(USB_INT_TXINI);

		if(!Endpoint_ConfigureEndpoint(CDC_RX_EPADDR, EP_TYPE_BULK, CDC_TXRX_EPSIZE, CDC_TXRX_BANKS))
			return;
		USB_INT_Enable(USB_INT_RXOUTI);
	} else {
		Endpoint_ClearEndpoints();
		if(USB_Device_ConfigurationNumber == 2)
		{
			exit(EXIT_SUCCESS);
		}
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
