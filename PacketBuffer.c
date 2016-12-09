#include "PacketBuffer.h"

#include <stddef.h>	// offsetof
#include <stdint.h>
#include <stdbool.h>
#include <string.h>	// memmove
#include <assert.h>

#include "helper.h"	// DIV_ROUND_UP
#include "resources.h"	// PACKETBUFFER_LEN

/// PacketBuffer: Combined queue for variable length packets flowing into two directions.
/// =====================================================================================
/// - A single ring buffer is used to store input and output packets.
/// - Packets are always stored continuous in memory, to allow zero copy processing of packets.
/// - Input packets can be read from the queue independent of output packets in queue.
/// - Input packets are prioritized over output packets, to allow modifying input packets in place
///   and reuse them as output packet. This has a side effect, input packets in the queue block
///   reading of output packets located in the ring buffer after the input packets.
/// - Output packets need to be released after being send successfully.
/// - It is possible to resize a packet in the queue.
/// - The length of a packet is stored in the first word of a packet in the queue, the packet data
///   follows. New packets are aligned to 4 bytes on machines where uint32 needs to be aligned to
///   4 bytes. Packet data then starts with offset of 2 bytes. This layout is beneficial for
///   storing Ethernet packets, as an Ethernet header is 14, 18 or 22 bytes long and following
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
/// - If a new packet is written, the next packet after this new packet is marked as `EndOfRing`.
///   Until the new packet is finished, its state is `EndOfRing` | lenght. There can be multiple
///   concurrent unfinished packets, the `EndOfRing` flag is cleared, if the packet is finished.
/// - `Packet_New()` checks for enough empty space in the RingBuffer. There is always at least
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

/// Calculate pointer to next packet in memory (without taking bounds into account)
__attribute__((always_inline)) static inline Packet_t *getNextPacket(Packet_t *packet, uint16_t len)
{
	return &packet[DIV_ROUND_UP(len + offsetof(Packet_t, data[0]), sizeof(Packet_t))];
}

/// Allocate memory for a new packet in FIFO buffer. This is used by Packet_New and Packet_Resize.
///
/// Can optionally copy memory from old packet (used for Packet_Resize), in this case the last 3 parameters are needed.
/// @param[in] writer Should be NextWriter.
/// @param[in] lastReader Should be OutputReader
/// @param[in] len Length of new packet in byte.
/// @param[in] resize True if memory from `packet` should be copied (Packet_Resize), otherwise false (Packet_New)
/// @param[in] packet Ignored if `resize`==false, old packet whose memory should be copied to new memory
/// @param[in] oldLen Ignored if `resize`==false, length of memory to be copied from old packet
__attribute__((always_inline)) static inline Packet_t *allocateNew(Packet_t *writer, Packet_t *lastReader, uint16_t len, bool resize, Packet_t *packet, uint16_t oldLen)
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

/// Allocate memory for a new packet in FIFO buffer.
Packet_t *Packet_New(uint16_t len)
{
	return allocateNew(NextWriter, OutputReader, len, false, NULL, 0);
}

/// Resize packet in FIFO buffer.
Packet_t *Packet_Resize(Packet_t *packet, uint16_t len)
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
	return allocateNew(writer, lastReader, len, true, packet, oldLen);
}

/// Mark packet ready to be processed by Input chain.
void Packet_PutInput(Packet_t *packet)
{
	assert((packet->state & Skip) == 0);
	packet->state |= Input;
}

/// Mark packet ready to be processed by Output chain.
void Packet_PutOutput(Packet_t *packet)
{
	assert((packet->state & Skip) == 0);
	packet->state |= Output;
}

/// Get a packet from the Input chain. If there is no ready packet, returns NULL.
Packet_t *Packet_GetInput(void)
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
			reader = getNextPacket(reader, Packet_getLen(state));
	}

	if(skipInput)
	{
		InputReader = reader;
		if(skipOutput)
			OutputReader = reader;
	}
	return (state & Input) ? reader : NULL;
}

/// Get a packet from the Output chain. If there is no ready packet, returns NULL.
Packet_t *Packet_GetOutput(void)
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
			reader = getNextPacket(reader, Packet_getLen(state));
	}

	if(skipOutput)
	{
		OutputReader = reader;
		if(skipInput)
			InputReader = reader;
	}
	return (state & Output) ? reader : NULL;
}

/// Release packet from Input chain, free memory.
void Packet_ReleaseInput(Packet_t *packet)
{
	Packet_t *nextPacket = getNextPacket(packet, Packet_getLen(packet->state));
	InputReader = nextPacket;
	if(OutputReader == packet)
		OutputReader = nextPacket;
	else
		packet->state |= Skip;
}

/// Release packet from Input chain, free memory.
void Packet_ReleaseOutput(Packet_t *packet)
{
	Packet_t *nextPacket = getNextPacket(packet, Packet_getLen(packet->state));
	if(InputReader == packet)
		InputReader = nextPacket;
	OutputReader = nextPacket;
}

/// Reattach packet from the Input chain to Output chain.
void Packet_ReattachOutput(Packet_t *packet)
{
	assert((packet->state & Skip) == Input);

	uint16_t len = Packet_getLen(packet->state);
	packet->state = len | Output;
	InputReader = getNextPacket(packet, len);
}

