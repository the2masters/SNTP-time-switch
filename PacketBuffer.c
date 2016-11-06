#include "PacketBuffer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#ifdef __AVR_ARCH__
	#include <avr/sleep.h>
	#include <util/atomic.h>
#else
	#define sleep_disable()
	#define ATOMIC_BLOCK(x)
	#define NONATOMIC_BLOCK(x)
#endif

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
///   follows. New packets are aligned to 4 bytes on machines where uint32 needs to be aligned to
///   4 bytes. Packet data then starts with offset of 2 bytes. This layout is beneficial for
///   storing Ethernet packets, as an Ethernet header is 14, 18 or 22 bytes long an following
///   headers contain uint32 values.
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

#define PacketHeaderLen offsetof(Packet_t, data[0])
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
static Packet_t * volatile NextWriter = RingBuffer.Start;
/// Pointer to the end of the RingBuffer
static Packet_t * volatile InputReader = RingBuffer.Start;
/// Pointer to the end of the RingBuffer
static Packet_t * volatile OutputReader = RingBuffer.Start;


/// Calculate pointer to next packet in memory (without taking bounds into account)
static inline Packet_t *getNextPacket(Packet_t *packet, uint16_t len)
{
	return &packet[DIV_ROUND_UP(len + PacketHeaderLen, sizeof(Packet_t))];
}

/// Get a new packet from RingBuffer
Packet_t *Buffer_New(uint16_t len)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		Packet_t *writer = NextWriter;
		Packet_t *nextPacket = getNextPacket(writer, len);
		Packet_t *lastReader = OutputReader;

		// Check if we have to jump to the beginning of the RingBuffer
		if(writer >= lastReader && nextPacket > RingBuffer.End)
		{
			nextPacket = getNextPacket(RingBuffer.Start, len);
			if(nextPacket >= lastReader)
				return NULL;

			// Mark this position as end of ring, mark start of ring as end
			writer->state = StartOver;
			writer = RingBuffer.Start;
		} else {
			// If we turned arround, check if there is enough space before the Reader
			if(writer < lastReader && nextPacket >= lastReader)
				return NULL;
		}

		nextPacket->state = EndOfRing;
		writer->state = len;
		NextWriter = nextPacket;
		return writer;
	}
	__builtin_unreachable();
}

/// Extend a packet recently gotten from `Buffer_New()`. This assumes packet is the last packet
/// from Buffer_New() and there are no other concurrent calls from other threads.
Packet_t *Buffer_Extend(Packet_t *packet, uint16_t len)
{
	uint16_t oldLen = packet->state;
	assert((oldLen & Skip) == 0);
	Packet_t *oldNextPacket = getNextPacket(packet, oldLen);

	// there could be unused padding space in packet
	oldLen = ((oldLen + PacketHeaderLen - 1) | (sizeof(Packet_t) - 1)) - PacketHeaderLen + 1;

	// Alternative:
	// oldLen = (uint16_t)(((uintptr_t)oldNextPacket - (uintptr_t)packet) - PacketHeaderLen);

	if(len <= oldLen)
	{
		packet->state = len;
		return packet;
	}

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		Packet_t *nextPacket = getNextPacket(packet, len);
		Packet_t *inputReader = InputReader;
		Packet_t *lastReader = OutputReader;
		Packet_t *writer = NextWriter;

		// Check if RingBuffer has enough space to append the extra bytes
		if(oldNextPacket == writer && ((packet < lastReader) ? (nextPacket < lastReader) : (nextPacket <= RingBuffer.End)))
		{
			nextPacket->state = EndOfRing;
			packet->state = len;
			NextWriter = nextPacket;
			return packet;
		}

		// Test if RingBuffer is empty, Reset RingBuffer in this case
		if(len <= PACKET_LEN_MAX && writer == inputReader && writer == lastReader)
		{
			NextWriter = RingBuffer.Start;
			InputReader = RingBuffer.Start;
			OutputReader = RingBuffer.Start;
			writer = RingBuffer.Start;
			lastReader = RingBuffer.Start;
		}

		nextPacket = getNextPacket(writer, len);
		if(writer >= lastReader && nextPacket > RingBuffer.End)
		{
			nextPacket = getNextPacket(RingBuffer.Start, len);
			if(nextPacket >= lastReader)
				return NULL;

			memcpy((void *)RingBuffer.Start->data, (void *)packet->data, packet->state);

			// Mark this position as end of ring, mark start of ring as end
			writer->state = StartOver;
			writer = RingBuffer.Start;
		} else {
			// If we turned arround, check if there is enough space before the Reader
			if(writer < lastReader && nextPacket >= lastReader)
				return NULL;

			memcpy((void *)RingBuffer.Start->data, (void *)packet->data, packet->state);
		}

		nextPacket->state = EndOfRing;
		writer->state = len;
		NextWriter = nextPacket;
		return writer;
	}
	__builtin_unreachable();
}

void Buffer_PutInput(Packet_t *packet)
{
	assert((packet->state & Skip) == 0);
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		packet->state |= Input;

	sleep_disable();
}

void Buffer_PutOutput(Packet_t *packet)
{
	assert((packet->state & Skip) == 0);
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		packet->state |= Output;
}

void Buffer_ReattachOutput(Packet_t *packet)
{
	assert((packet->state & Skip) == Input);

	uint16_t len = Packet_getLen(packet->state);
	Packet_t *nextPacket = getNextPacket(packet, len);

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		InputReader = nextPacket;
		packet->state = len | Output;
	}
}

void Buffer_ReleaseInput(Packet_t *packet)
{
	Packet_t *nextPacket = getNextPacket(packet, Packet_getLen(packet->state));

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		InputReader = nextPacket;
		if(OutputReader == packet)
			OutputReader = nextPacket;
		else
			packet->state |= Skip;
	}
}

void Buffer_ReleaseOutput(Packet_t *packet)
{
	Packet_t *nextPacket = getNextPacket(packet, Packet_getLen(packet->state));
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(InputReader == packet)
			InputReader = nextPacket;
		OutputReader = nextPacket;
	}
}


Packet_t *Buffer_GetInput(void)
{
	Packet_t *reader = InputReader;
	uint16_t state;

	// Skip entries of type Output or Skip
	while((state = reader->state) & Output)
	{
		Packet_t *old = reader;

		if(state == StartOver)
			reader = RingBuffer.Start;
		else
			reader = getNextPacket(reader, Packet_getLen(state));

		InputReader = reader;
		// Advance Output reader, if there's nothing to read for them
		if((state & Input) && OutputReader == old)	// same as state == Skip or StartOver
			OutputReader = reader;
	}

	return (state & Input) ? reader : NULL;
}

Packet_t *Buffer_GetOutput(void)
{
	Packet_t *reader = OutputReader;
	uint16_t state;

	// Skip entries of type Skip
	while(((state = reader->state) & Skip) == Skip)
	{
		Packet_t *old = reader;

		if(state == StartOver)
			reader = RingBuffer.Start;
		else
			reader = getNextPacket(reader, Packet_getLen(state));

		// Advance InputReader, if there's nothing to read for them
		if(InputReader == old)
			InputReader = reader;

		OutputReader = reader;
	}

	return (state & Output) ? reader : NULL;
}