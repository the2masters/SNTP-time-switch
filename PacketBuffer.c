#include "PacketBuffer.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "helper.h"
#include "resources.h"

static uint32_t RingBuffer[DIV_ROUND_UP(USB_BUFFER_LEN, sizeof(uint32_t))] = {0};
#define RingBufferStart ((Packet_t *)RingBuffer)
#define RingBufferEnd ((Packet_t *)&RingBuffer[ARRAY_SIZE(RingBuffer)])
#define RingBufferBlockSize 2

_Static_assert(sizeof(RingBuffer) >= (2*ROUND_UP(MAX_PACKET_LEN + sizeof(Packet_t), RingBufferBlockSize) + ROUND_UP(sizeof(Packet_t), RingBufferBlockSize)), "USB_BUFFER_LEN too small");

static Packet_t * volatile LastReader = RingBufferStart;

// the state field of a packet includes 2 flag bits (MSB)
#define EndOfRing 0x0000
#define Ethernet  0x4000
#define Usb       0x8000
#define Skip      0xC000
#define StartOver 0xFFFF

static inline Packet_t *getNextPacket(const Packet_t *packet, uint16_t len)
{
	// round up bytes to RingBuffer BlockSize
	return (Packet_t *)&packet->data[DIV_ROUND_UP(len, RingBufferBlockSize)];
}



Packet_t *Buffer_New(uint16_t len)
{
	static Packet_t *Writer = RingBufferStart;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		Packet_t *packet = Writer;
		Packet_t *nextPacket = getNextPacket(packet, len);
		Packet_t *lastReader = LastReader;

		// Check if we have to jump to the beginning of the RingBuffer
		if(packet >= lastReader && nextPacket > (RingBufferEnd - 1))
		{
			nextPacket = getNextPacket(RingBufferStart, len);
			if(nextPacket > (lastReader - 1))
				return NULL;

			// Mark this position as end of ring, mark start of ring as end
			packet->state = StartOver;
			packet = RingBufferStart;
		} else {
			// If we turned arround, check if there is enough space before the Reader
			if(packet < lastReader && nextPacket > (lastReader - 1))
				return NULL;
		}

		nextPacket->state = EndOfRing;
		packet->state = len;
		Writer = nextPacket;
		return packet;
	}
}

Packet_t *Buffer_Extend(Packet_t *packet, uint16_t newlen)
{
	Packet_t *oldNextPacket = getNextPacket(packet, Packet_getLen(packet->state));

	// Check if old packet has enough free space in padding
	uint16_t oldlen = (uint16_t)((oldNextPacket->data - packet->data) * (uint16_t)sizeof(*packet->data) - (uint16_t)sizeof(Packet_t));
	if(newlen <= oldlen)
	{
		ATOMIC_WRITE packet->state = newlen;
		return packet;
	}

	// Check if RingBuffer has enough space to append the extra bytes
	Packet_t *nextPacket = getNextPacket(packet, newlen);
	bool append = (packet < LastReader || nextPacket <= (RingBufferEnd - 1));
	uint16_t getlen = newlen;
	if(append)
	{	// We have enough space to append the needed bytes to the packet, just get a new packet for the remaining bytes
		getlen -= oldlen;

		// If we enlarge an existing packet we don't need a new header. We can only get
		// packets with full header from RingBuffer, so subtract header length.
		if(getlen >= sizeof(Packet_t))
			getlen -= sizeof(Packet_t);
		// Special case: len - old < sizeof(packet_header) => getlen would be negative
		else
			getlen = 0;
	}

	Packet_t *newPacket = Buffer_New(getlen);
	if(newPacket == NULL)
		return NULL;

	// packets adjacent?
	if(newPacket == oldNextPacket)
	{
		assert(append); // !append should not happen here, would mean packet is too big
		ATOMIC_WRITE packet->state = append ? newlen : newlen + oldlen + (uint16_t)sizeof(Packet_t);
		return packet;
	// packets not adjacent
	} else {
		assert(!append);
		if(!append)
		{
			memcpy((void *)newPacket->data, (void *)packet->data, packet->state);
			ATOMIC_WRITE packet->state |= Skip;
			return newPacket;
		} else { // we wanted to concat packets, but got not adjacent packets: This should not happen
			ATOMIC_WRITE newPacket->state |= Skip;
			return NULL;
		}
	}
}

void Buffer_PutForUSB(Packet_t *packet)
{
	// Ethernet can respond to packets, in this case Ethernet flag is already set
	ATOMIC_WRITE packet->state = (uint16_t)((packet->state & ~Ethernet) | Usb);
// TODO: Enable USB Send Interrupt
}

void Buffer_PutForEthernet(Packet_t *packet)
{
	assert((packet->state & Usb) == 0); // Usb flag cannot be set
	ATOMIC_WRITE packet->state |= Ethernet;
}

// Keep in sync with Buffer_GetForUSB()
Packet_t *Buffer_GetForEthernet(void)
{
	static Packet_t *ethernetReader = RingBufferStart;
	uint16_t state;

	// Skip entries of type Usb or Skip
	while((state = ethernetReader->state) & Usb)
	{
		if(state == StartOver)
			ethernetReader = RingBufferStart;
		else
			ethernetReader = getNextPacket(ethernetReader, Packet_getLen(state));
	}

	if(state & Ethernet)
	{
		Packet_t *retVal = ethernetReader;
		ethernetReader = getNextPacket(ethernetReader, Packet_getLen(state));
		return retVal;
	}

	return NULL;
}

// Keep in sync with Ethernet_Receive
Packet_t *Buffer_GetForUSB(void)
{
	Packet_t *usbReader = LastReader;
	uint16_t state;

	// Skip entries of type Skip
	while(((state = usbReader->state) & Skip) == Skip)
	{
		if(state == StartOver)
			usbReader = RingBufferStart;
		else
			usbReader = getNextPacket(usbReader, Packet_getLen(state));

		ATOMIC_WRITE LastReader = usbReader;
	}

	if(state & Usb)
	{
		return usbReader;
	}

	return NULL;
}

void Buffer_ReleaseEthernet(Packet_t *packet)
{
	if(packet == LastReader)
		Buffer_ReleaseUSB(packet);
	else
		ATOMIC_WRITE packet->state |= Skip;
// Enable USB Reader
}

void Buffer_ReleaseUSB(Packet_t *packet)
{
	ATOMIC_WRITE LastReader = getNextPacket(packet, Packet_getLen(packet->state));
// TODO: disable USB Send Interrupt when LastReader->state & Skip = EndOfRing
}
