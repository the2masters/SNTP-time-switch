#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <avr/power.h>
#include <avr/sleep.h>
//#include <avr/cpufunc.h>
#include <util/atomic.h>

#define _RNDIS_CLASS_H_
#include <LUFA/Drivers/USB/USB.h>

#include "PacketBuffer.h"
#include "helper.h"
#include "Descriptors.h"
#include "resources.h"

#include "IPcheck.c"

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


static inline void USB_EnableReceiver(void)
{
	Endpoint_SelectEndpoint(CDC_RX_EPADDR);
	USB_INT_Enable(USB_INT_RXOUTI);
}

static inline void USB_EnableTransmitter(void)
{
	Endpoint_SelectEndpoint(CDC_TX_EPADDR);
	USB_INT_Enable(USB_INT_TXINI);
}

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
				packet = Buffer_GetOutput();
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
					Buffer_ReleaseOutput(packet);
					packet = NULL;
					enableRX = true;
				}
			}
		}

		// Receive packets into input queue
		if(enableRX && (Endpoint_SelectEndpoint(CDC_RX_EPADDR), Endpoint_IsOUTReceived()))
		{
			static Packet_t *packet;
			static volatile uint8_t *writer;
			static uint16_t bytesRemaining;
			static enum {
				WAITING   = 0,
				NEEDSPACE = 1 << 0,
				READING   = 1 << 1,
			} state = WAITING;

#warning: Passe Buffer_Extend an: ueberge, wie viel groesser gemacht werden soll
#warning: Passe Buffer_Extend an: Funktioniere auch, wenn dazwischen andere Buffer_new aufgerufen wurden
#warning: Passe Paket an: data als volatile uint8_t[]

			const uint8_t usbLen = Endpoint_BytesInEndpoint();

			if(state == NEEDSPACE)
			{
				Packet_t *newPacket = Buffer_Extend(packet, bytesRemaining);
				if(newPacket)
				{
					writer += (uintptr_t)newPacket - (uintptr_t)packet;
					packet = newPacket;
					state = READING;
				} else {
					enableRX = false;
				}
			}
			else if(state == WAITING)
			{
				if(usbLen >= PACKET_LEN_MIN)
				{
					packet = Buffer_New(usbLen);
					if(packet)
					{
						state = READING;
						uint16_t fullLength = USB_Read24Byte_Check_GetLength(packet->data);
						if(fullLength > usbLen)
						{
							if(usbLen == CDC_TXRX_EPSIZE) // There will be more USB frames
							{
								Packet_t *newPacket = Buffer_Extend(packet, fullLength - usbLen);
								if(newPacket)
								{
									packet = newPacket;
								} else { // Try to extend buffer when next frame arrives
									state |= NEEDSPACE;
								}
							} else { // IP Header told us about a larger packet
								error(&errRXIPlong);
								fullLength = 0;
							}
						}

						if(fullLength > 0)
						{
							writer = packet->data + 24;
							bytesRemaining = fullLength - 24;
						} else { // Packet is not for us
							error(&errRXIPdontcare);
							Buffer_ReleaseInput(packet);
							packet = NULL;
							bytesRemaining = 0;
						}
					} else { // No space in PacketBuffer
						enableRX = false;
					}
				} else { // usbLen < PACKET_LEN_MIN => cannot be a valid packet
					error(&errRXShort);
					Endpoint_ClearOUT();
				}
			}

			if(state & READING)
			{
				uint8_t readLength = (uint8_t)MIN((uint16_t)usbLen, bytesRemaining);
				bytesRemaining -= readLength;
				while(readLength--)
					*writer++ = Endpoint_Read_8();

				if(usbLen != CDC_TXRX_EPSIZE)
				{
					if(bytesRemaining)
					{
						error(&errRXShort);
						if(packet) Buffer_ReleaseInput(packet);
						bytesRemaining = 0;
					} else {
						if(packet) Buffer_PutInput(packet);
						sleep_disable();
					}
					state = WAITING;
				}
				Endpoint_ClearOUT();
			}
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

		if(!Endpoint_ConfigureEndpoint(CDC_TX_EPADDR, EP_TYPE_BULK, CDC_TXRX_EPSIZE, 1))
			return;
		USB_INT_Enable(USB_INT_TXINI);

		if(!Endpoint_ConfigureEndpoint(CDC_RX_EPADDR, EP_TYPE_BULK, CDC_TXRX_EPSIZE, 1))
			return;
		USB_INT_Enable(USB_INT_RXOUTI);
	} else {
		Endpoint_SelectEndpoint(CDC_RX_EPADDR);
		USB_INT_Disable(USB_INT_RXOUTI);
		Endpoint_DisableEndpoint();
		Endpoint_SelectEndpoint(CDC_TX_EPADDR);
		USB_INT_Disable(USB_INT_TXINI);
		Endpoint_DisableEndpoint();
		Endpoint_SelectEndpoint(CDC_NOTIFICATION_EPADDR);
		USB_INT_Disable(USB_INT_TXINI);
		Endpoint_DisableEndpoint();

		if(USB_Device_ConfigurationNumber == 2)
			exit(EXIT_SUCCESS);
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
