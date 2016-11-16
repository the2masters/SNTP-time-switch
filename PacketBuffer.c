#include "PacketBuffer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <util/atomic.h>

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
/// - There can be multiple concurrent writer and reader.
/// - It is possible to resize a packet in the queue.
/// - Output packets need to be released after being send successfully.
/// - The length of a packet is stored in the first word of a packet in the queue, the packet data
///   follows. New packets are aligned to 4 bytes on machines where uint32 needs to be aligned to
///   4 bytes. Packet data then starts with offset of 2 bytes. This layout is beneficial for
///   storing Ethernet packets, as an Ethernet header is 14, 18 or 22 bytes long and following
///   headers contain uint32 values.
/// - The length field is also used to store a number of flags and called `state`. You have to use
///   `getLen(packet->state)` to get the length of a packet.

/// Ring buffer layout:
/// - As the packets should be stored continuous in memory, we leave unused space in the end of
///   the underlying buffer if there is not enough space to store the next packet. The beginning
///   of the unused space is marked with an empty packet with the flag `StartOver`. The next packet
///   will be stored in the beginning of the underlying buffer (if there is enough space).
/// - To optimize reading packets we even set the `StartOver` flag if there is no unused space (in
///   case the last written packet was exactly as large as the unused space). To be able to set
///   this flag we reserve an empty packet after the end of the underlaying buffer.
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

static Packet_t * volatile NextWriter = RingBuffer.Start;
static Packet_t * volatile InputReader = RingBuffer.Start;
static Packet_t * volatile OutputReader = RingBuffer.Start;


/// Strip flags from state to get length
ATTR_ALWAYS_INLINE static inline uint16_t getLen(uint16_t state)
{
	return state & ~Skip;
}

/// Calculate pointer to next packet in memory (without taking bounds into account)
ATTR_ALWAYS_INLINE static inline Packet_t *getNextPacket(Packet_t *packet, uint16_t len)
{
	return &packet[DIV_ROUND_UP(len + offsetof(Packet_t, data[0]), sizeof(Packet_t))];
}

// TODO: Document me
ATTR_ALWAYS_INLINE static inline Packet_t *_buffer_new(Packet_t *writer, Packet_t *lastReader, uint16_t len, bool resize, Packet_t *packet, uint16_t oldLen)
{
	Packet_t *nextPacket = getNextPacket(writer, len);

	// Check if we can and have to jump to the beginning of the RingBuffer
	if(writer >= lastReader)
	{
		if(nextPacket > RingBuffer.End)
		{
			nextPacket = getNextPacket(RingBuffer.Start, len);

			// If RingBuffer is empty reset all readers and start from the beginning
			if(writer == lastReader)
			{
				assert(nextPacket <= RingBuffer.End);
				InputReader = RingBuffer.Start;
				OutputReader = RingBuffer.Start;
			}
			// If there is enough space in the beginning of the RingBuffer,
			// mark current end of RingBuffer with StartOver
			else if(nextPacket < lastReader)
			{
				writer->state = StartOver;
			} else {
				return NULL;	// Ringbuffer full
			}

			writer = RingBuffer.Start;
		}
	}
	// We are in the beginning of the RingBuffer already, check if there is enough
	// space before old entries at the end of the RingBuffer
	else if(nextPacket >= lastReader)
	{
		return NULL;
	}

	writer->state = len;

	// Next steps are only needed in case of packet resize
	if(resize)
	{
		packet->state = Skip | oldLen;	// before memmove, as memmove may override memory of old packet (correct)

		// Copy old packet into new buffer (underlying memory could overlap)
		memmove((void *)writer->data, (void *)packet->data, oldLen);
	}

	nextPacket->state = EndOfRing;	// after memmove, as this may override memory which is only free after memmove
	NextWriter = nextPacket;

	return writer;
}

/// Get a new packet from RingBuffer (not threadsafe)
Packet_t *Buffer_New_unsafe(uint16_t len)
{
	return _buffer_new(NextWriter, OutputReader, len, false, NULL, 0);
}

/// Get a new packet from RingBuffer
inline Packet_t *Buffer_New(uint16_t len)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		return Buffer_New_unsafe(len);

	__builtin_unreachable();
}

/// Resize a packet recently gotten from `Buffer_New()` (not threadsafe)
Packet_t *Buffer_Resize_unsafe(Packet_t *packet, uint16_t len)
{
	uint16_t oldLen = packet->state;
	assert((oldLen & Skip) == 0);

	Packet_t *nextPacket = getNextPacket(packet, len);
	Packet_t *oldNextPacket = getNextPacket(packet, oldLen);

	// Packet shrink, need to do something with remainder
	if(oldNextPacket > nextPacket)
	{
		if(NextWriter == oldNextPacket)	// Last element in ring, adjust next write position
			NextWriter = nextPacket;
		else				// Otherwise mark remainder as Skip
			nextPacket->state = Skip | ((uintptr_t)oldNextPacket - (uintptr_t)nextPacket->data);
	}

	// Packet shrink or same size
	if(oldNextPacket >= nextPacket)
	{
		// even if oldNextPacket == nextPacket it's possible that len != oldLen
		packet->state = len;
		return packet;
	}

	// Packet extend
	Packet_t *lastReader = OutputReader;
	Packet_t *writer = NextWriter;

	// Check if packet is last element in RingBuffer and we have enough space to append the extra bytes
	if(oldNextPacket == writer && ((packet < lastReader) ? (nextPacket < lastReader) : (nextPacket <= RingBuffer.End)))
	{
		nextPacket->state = EndOfRing;
		packet->state = len;
		NextWriter = nextPacket;
		return packet;
	}

	// We cannot append, create new packet of full length
	return _buffer_new(writer, lastReader, len, true, packet, oldLen);
}

/// Resize a packet recently gotten from `Buffer_New()`
inline Packet_t *Buffer_Resize(Packet_t *packet, uint16_t len)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON)
		return Buffer_Resize_unsafe(packet, len);

	__builtin_unreachable();
}


inline void Buffer_PutInput(Packet_t *packet)
{
	assert((packet->state & Skip) == 0);
	packet->state |= Input;
}

inline void Buffer_PutOutput(Packet_t *packet)
{
	assert((packet->state & Skip) == 0);
	packet->state |= Output;
}

void Buffer_ReattachOutput(Packet_t *packet)
{
	assert((packet->state & Skip) == Input);

	uint16_t len = getLen(packet->state);
	packet->state = len | Output;
	Packet_t *nextPacket = getNextPacket(packet, len);

	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
		InputReader = nextPacket;
	}
}

void Buffer_ReleaseInput(Packet_t *packet)
{
	Packet_t *nextPacket = getNextPacket(packet, getLen(packet->state));

	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
		InputReader = nextPacket;
		if(OutputReader == packet)
			OutputReader = nextPacket;
		else
			packet->state |= Skip;
	}
}

void Buffer_ReleaseOutput_unsafe(Packet_t *packet)
{
	Packet_t *nextPacket = getNextPacket(packet, getLen(packet->state));
	if(InputReader == packet)
		InputReader = nextPacket;
	OutputReader = nextPacket;
}

inline void Buffer_ReleaseOutput(Packet_t *packet)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON)
		Buffer_ReleaseOutput_unsafe(packet);
}

Packet_t *Buffer_GetInput(void)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
		Packet_t *reader = InputReader;
		bool skipInput = false, skipOutput = (OutputReader == reader);
		uint16_t state;

		// Skip entries of type Output or Skip
		while((state = reader->state) & Output)
		{
			skipInput = true;
			// Skip Output pointer entries only for skip entries
			if(skipOutput && !(state & Input))
			{
				OutputReader = reader;
				skipOutput = false;
			}

			if(state == StartOver)
				reader = RingBuffer.Start;
			else
				reader = getNextPacket(reader, getLen(state));
		}

		if(skipInput)
		{
			InputReader = reader;
			if(skipOutput)
				OutputReader = reader;
		}
		return (state & Input) ? reader : NULL;
	}
	__builtin_unreachable();
}

Packet_t *Buffer_GetOutput_unsafe(void)
{
	Packet_t *reader = OutputReader, *input = InputReader;
	bool skipOutput = false, skipInput = false;
	uint16_t state;

	// Skip entries of type Skip
	while(((state = reader->state) & Skip) == Skip)
	{
		skipOutput = true;
		if(!skipInput && input == reader)
			skipInput = true;

		if(state == StartOver)
			reader = RingBuffer.Start;
		else
			reader = getNextPacket(reader, getLen(state));
	}

	if(skipOutput)
	{
		OutputReader = reader;
		if(skipInput)
			InputReader = reader;
	}
	return (state & Output) ? reader : NULL;
}

inline Packet_t *Buffer_GetOutput(void)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON)
		return Buffer_GetOutput_unsafe();
	__builtin_unreachable();
}
