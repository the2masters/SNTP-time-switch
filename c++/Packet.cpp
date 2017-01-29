#include "Packet.h"
#include "Queue.h"

#include <util/atomic.h>

void* Packet::operator new(size_t sizeOfClass, size_t extraBytes)
{
	Packet *packet;
	ATOMIC_BLOCK(ATOMIC_FORCEON)
		packet = RingBuffer::Get(sizeOfClass + extraBytes);
	if(packet)
		packet->state = (Packet::State)(sizeOfClass + extraBytes - sizeof(Packet));
	return packet;
}

void Packet::operator delete(void *packet)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON)
		RingBuffer::Release((Packet *)packet);
}
