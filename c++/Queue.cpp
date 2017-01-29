#include "Queue.h"
#include "helper.h"
#include "assert.h"

struct RingBuffer::ringBuffer RingBuffer::ringBuffer;

Packet* volatile RingBuffer::nextWriter = ringBuffer.start;
Packet* volatile RingBuffer::inputReader = ringBuffer.start;
Packet* volatile RingBuffer::outputReader = ringBuffer.start;


/// Calculate pointer to next packet in memory (without taking bounds into account)
__attribute__((always_inline)) static constexpr Packet* getNextPacket(Packet* packet, uint16_t len)
{
	return &packet[DIV_ROUND_UP(len, sizeof(Packet))];
}
__attribute__((always_inline)) static constexpr Packet* getNextPacket(Packet* packet)
{
	return getNextPacket(packet, packet->getLen());
}

/// Allocate memory for a new packet in FIFO buffer.
///
/// @param[in] len Length of new packet in byte.
Packet* RingBuffer::Get(uint16_t len)
{
	Packet *writer = nextWriter;
	Packet *lastReader = outputReader;
	Packet *nextPacket = getNextPacket(writer, len);

	// Check if we can and have to jump to the beginning of the RingBuffer
	if(writer >= lastReader)
	{
		if(nextPacket > ringBuffer.end)
		{
			nextPacket = getNextPacket(ringBuffer.start, len);

			// If RingBuffer is empty reset all readers and start from the beginning
			if(writer == lastReader)
			{
				assert(nextPacket <= ringBuffer.end);
				inputReader = ringBuffer.start;
				outputReader = ringBuffer.start;
			}
			// If there is enough space in the beginning of the RingBuffer,
			// mark current end of RingBuffer with StartOver
			else if(nextPacket < lastReader)
			{
				writer->state = Packet::State::StartOver;
			} else {
				return NULL;	// Ringbuffer full
			}

			writer = ringBuffer.start;
		}
	}
	// We are in the beginning of the RingBuffer already, check if there is enough
	// space before old entries at the end of the RingBuffer
	else if(nextPacket >= lastReader)
	{
		return NULL;
	}

	nextPacket->state = Packet::State::EndOfRing;
	writer->state = (Packet::State)len;
	nextWriter = nextPacket;

	return writer;
}

/// Release packet from Input chain, free memory.
void RingBuffer::Release(Packet *packet)
{
	Packet *nextPacket = getNextPacket(packet);
	if(inputReader == packet)
		inputReader = nextPacket;
	if(outputReader == packet)
		outputReader = nextPacket;
	else
		packet->state |= Packet::State::Skip;
}

template<>
void InputQueue::Push(Packet *packet) {
	assert((packet->state & Packet::State::Output) == Packet::State::Output);
	packet->state |= Packet::State::Input;
}

template<>
void OutputQueue::Push(Packet *packet) {
	uint16_t len = packet->getLen();
	if((packet->state & Packet::State::Input) == Packet::State::Input)
		inputReader = getNextPacket(packet, len);
	packet->state = (Packet::State)len | Packet::State::Input;
}

/// Get a packet from the Input chain. If there is no ready packet, returns NULL.
template<>
Packet* InputQueue::Pop()
{
	Packet *reader = inputReader;
	bool skipInput = false, skipOutput = (outputReader == reader);
	Packet::State state;

	// Skip entries of type Output or Skip
	while(((state = reader->state) & Packet::State::Output) == Packet::State::Output)
	{
		skipInput = true;
		// Skip Output pointer entries only for skip entries
		if(skipOutput && (state & Packet::State::Input) != Packet::State::Input)
		{
			outputReader = reader;
			skipOutput = false;
		}

		if(state == Packet::State::StartOver)
			reader = ringBuffer.start;
		else
			reader = getNextPacket(reader);
	}

	if(skipInput)
	{
		inputReader = reader;
		if(skipOutput)
			outputReader = reader;
	}
	return (state & Packet::State::Input) == Packet::State::Input ? reader : NULL;
}

/// Get a packet from the Input chain. If there is no ready packet, returns NULL.
template<>
Packet* OutputQueue::Pop()
{
	Packet *reader = outputReader, *input = inputReader;
	bool skipInput = false, skipOutput = false;
	Packet::State state;

	// Skip entries of type Skip
	while(((state = reader->state) & Packet::State::Skip) == Packet::State::Skip)
	{
		skipOutput = true;
		if(!skipInput && input == reader)
			skipInput = true;

		if(state == Packet::State::StartOver)
			reader = ringBuffer.start;
		else
			reader = getNextPacket(reader);
	}

	if(skipOutput)
	{
		outputReader = reader;
		if(skipInput)
			inputReader = reader;
	}
	return (state & Packet::State::Output) == Packet::State::Output ? reader : NULL;
}
