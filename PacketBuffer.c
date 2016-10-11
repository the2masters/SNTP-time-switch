#include "PacketBuffer.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "helper.h"
#include "resources.h"

/// PacketBuffer: Combined queue for variable length packets flowing into two directions.
/// =====================================================================================
/// - A single ring buffer is used to store input and output packets.
/// - Packets are always stored continuous in memory, to allow zero copy processing of packets.
/// - Input packets can be read from the queue independent of output packets in queue.
/// - Input packets are prioritized over output packets, to allow modifying input packets in place
///   and reuse them as output packet. This has a side effect, input packets in the queue block
///   reading of output packets located in the ring buffer after the input packets.
/// - There can be multiple concurrent writer.
/// - It is possible to enlarge a packet in the queue. This is only possible, if you are the last
///   writer to the queue. To ensure this, you have to implement a lock against other writer
///   or something similar.
/// - The length of a packet is stored in the first word of a packet in the queue, the packet data
///   follows. New packets are aligned to 4 bytes, thus the packet data starts on an uneven word
///   address (2 byte aligned). This layout is beneficial for storing Ethernet packets, as an
///   Ethernet header is 14, 18 or 22 bytes long. Therefore the Ethernet payload (e.g. IP-packets)
///   are aligned to 4 bytes.
/// - The length field is also used to store a number of flags and called `state`. You have to use
///   `Packet_getLen(packet->state)` to get the length of a packet.

/// Ring buffer layout:
/// - As the packets should be stored continuous in memory, we leave unused space in the end of
///   the underlying buffer if there is not enough space to store the next packet. The beginning
///   of the unused space is marked with an empty packet with the flag `StartOver`. The next packet
///   will be stored in the beginning of the underlying buffer (if there is enough space).
/// - To optimize reading packets we even set the `StartOver` flag if there is no unused space (in
///   case the last written packet was exactly as large as the unused space). To be able to set
///   this flag we reserve an empty packet after the end of the underlaying buffer.
/// - Current writer position is managed locally by `Buffer_New()`
/// - If a new packet is written, the next packet after this new packet is marked as `EndOfRing`.
///   Until the new packet is finished, its state is `EndOfRing` | lenght. There can be multiple
///   concurrent unfinished packets, the `EndOfRing` flag is cleared, if the packet is finished.
/// - `Buffer_New()` checks for enough empty space in the RingBuffer. There is always at least
///   one empty packet between the last reader and the fastest writer. This empty packet is marked
///   as `EndOfRing`, see above.
/// - There is a flag for input and output packets. Input reader skips packet with flag `Output`,
///   but output reader blocks reading packets with flag `Input`. Packets marked as `Skip` are
///   skipped by both readers. Both readers are blocked if they reach a packet marked `EndOfRing`.
/// - After an input packet is processed, it needs to be released or resend in the output queue.
///   This changes the packet flags to `Skip` or `Output` and unblocks the output reader.
/// - Output packets need to be released after being send successfully.
/// - The last reader position is saved in the LastReader variable. This variable is advanced,
///   if the oldest packet is released from the queue.

_Static_assert(ROUND_UP(PACKETBUFFER_LEN, sizeof(Packet_t)) >=
		(2 * ROUND_UP(PACKET_LEN_MAX + PacketHeaderLen, sizeof(Packet_t))),
		 "PACKETBUFFER_LEN needs to be at least twice as large as PACKET_LEN_MAX");

/// the state field of a packet includes 2 flag bits (MSB). This is designed to optimize
/// conditional testing of the flags.
#define EndOfRing 0x0000
#define Input     0x4000
#define Output    0x8000
#define Skip      0xC000
#define StartOver 0xFFFF

static struct {
	Packet_t Start[DIV_ROUND_UP(PACKETBUFFER_LEN, sizeof(Packet_t))];
	Packet_t End[1];	///< Used to store StartOver flag in case of full ring
} __attribute__((packed, may_alias)) RingBuffer = {.Start[0].state = EndOfRing};

/// Pointer to the end of the RingBuffer
static Packet_t * volatile LastReader = RingBuffer.Start;


/// Calculate pointer to next packet in memory (without taking bounds into account)
static inline Packet_t *getNextPacket(Packet_t *packet, uint16_t len)
{
	return &packet[DIV_ROUND_UP(len + PacketHeaderLen, sizeof(Packet_t))];
}

/// Get a new packet from RingBuffer
Packet_t *Buffer_New(uint16_t len)
{
	static Packet_t *Writer = RingBuffer.Start;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		Packet_t *packet = Writer;
		Packet_t *nextPacket = getNextPacket(packet, len);
		Packet_t *lastReader = LastReader;

		// Check if we have to jump to the beginning of the RingBuffer
		if(packet >= lastReader && nextPacket > RingBuffer.End)
		{
			nextPacket = getNextPacket(RingBuffer.Start, len);
			if(nextPacket >= lastReader)
				return NULL;

			// Mark this position as end of ring, mark start of ring as end
			packet->state = StartOver;
			packet = RingBuffer.Start;
		} else {
			// If we turned arround, check if there is enough space before the Reader
			if(packet < lastReader && nextPacket >= lastReader)
				return NULL;
		}

		nextPacket->state = EndOfRing;
		packet->state = len;
		Writer = nextPacket;
		return packet;
	}
}

/// Extend a packet recently gotten from `Buffer_New()`. This assumes packet is the last packet
/// from Buffer_New() and there are no other concurrent calls from other threads.
Packet_t *Buffer_Extend(Packet_t *packet, uint16_t newlen)
{
	assert((packet->state & Skip) == 0);
	Packet_t *oldNextPacket = getNextPacket(packet, packet->state);

	// Check if old packet has enough free space in padding
	uint16_t oldlen = (uint16_t)(((uintptr_t)oldNextPacket - (uintptr_t)packet) - PacketHeaderLen);
	if(newlen <= oldlen)
	{
		ATOMIC_WRITE packet->state = newlen;
		return packet;
	}

	// Check if RingBuffer has enough space to append the extra bytes
	Packet_t *nextPacket = getNextPacket(packet, newlen);
	bool append = (packet < LastReader || nextPacket <= RingBuffer.End);
	uint16_t getlen = newlen;
	if(append)
	{	// We have enough space to append the needed bytes to the packet,
		// just get a new packet for the remaining bytes
		getlen -= oldlen;

		// If we enlarge an existing packet we don't need a new header. We can only get
		// packets with full header from RingBuffer, so subtract header length.
		if(getlen >= PacketHeaderLen)
			getlen -= PacketHeaderLen;
		// Special case: len - old < sizeof(packet_header) => getlen would be negative
	}

	Packet_t *newPacket = Buffer_New(getlen);
	if(newPacket == NULL)
		return NULL;

	// packets adjacent?
	if(newPacket == oldNextPacket)
	{	// append == false should not happen here. This would mean we thought we couldn't
		// append, but we actually could.
		if(!append)
		{
			assert(append);
			ATOMIC_WRITE packet->state |= Skip;
			packet = newPacket;
		}
		ATOMIC_WRITE packet->state = newlen;
		return packet;
	}
	else	// packets not adjacent
	{	// append == true should not happen here. This would mean we thought we can get
		// adjacent packets, but we couldn't.
		if(append)
		{
			assert(!append);
			ATOMIC_WRITE newPacket->state |= Skip;
			return NULL;
		}
		// Copy old packet data to new packet
		memcpy((void *)newPacket->data, (void *)packet->data, packet->state);
		ATOMIC_WRITE packet->state = StartOver; // Could be |= Skip, but this is faster
		return newPacket;
	}
}

void Buffer_PutInput(Packet_t *packet)
{
	assert((packet->state & Output) == 0);
	ATOMIC_WRITE packet->state |= Input;
}

void Buffer_PutOutput(Packet_t *packet)
{
	// Input flag can be set, if received packet is answered in place
	ATOMIC_WRITE packet->state = (uint16_t)((packet->state & ~Input) | Output);
}

Packet_t *Buffer_GetInput(void)
{
	static Packet_t *inputReader = RingBuffer.Start;
	uint16_t state;

	// Skip entries of type Output or Skip
	while((state = inputReader->state) & Output)
	{
		Packet_t *old = inputReader;

		if(state == StartOver)
			inputReader = RingBuffer.Start;
		else
			inputReader = getNextPacket(inputReader, Packet_getLen(state));

		// Advance Output reader, if there's nothing to read for them
		if((state & Input) && LastReader == old)	// same as state == Skip or StartOver
			ATOMIC_WRITE LastReader = inputReader;
	}

	if(state & Input)
	{
		Packet_t *retVal = inputReader;
		inputReader = getNextPacket(inputReader, Packet_getLen(state));
		return retVal;
	}

	return NULL;
}

Packet_t *Buffer_GetOutput(void)
{
	Packet_t *outputReader = LastReader;
	uint16_t state;

	// Skip entries of type Skip
	while(((state = outputReader->state) & Skip) == Skip)
	{
		if(state == StartOver)
			outputReader = RingBuffer.Start;
		else
			outputReader = getNextPacket(outputReader, Packet_getLen(state));

		ATOMIC_WRITE LastReader = outputReader;
	}

	if(state & Output)
	{
		return outputReader;
	}

	return NULL;
}

void Buffer_ReleaseInput(Packet_t *packet)
{
	if(packet == LastReader)
		Buffer_ReleaseOutput(packet);
	else
		ATOMIC_WRITE packet->state |= Skip;
}

void Buffer_ReleaseOutput(Packet_t *packet)
{
	ATOMIC_WRITE LastReader = getNextPacket(packet, Packet_getLen(packet->state));
}
