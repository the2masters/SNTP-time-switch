#ifndef _PACKET_H_
#define _PACKET_H_

#include <stddef.h> // size_t
#include <stdint.h>

class alignas(uint32_t) Packet
{
public:
	enum class State : uint16_t
	{
		Length		= 0x3FFF,
		Flags		= 0xC000,
// Flags
		EndOfRing	= 0,
		Input		= 0x4000,
		Output		= 0x8000,
		Skip		= 0xC000,
		StartOver	= 0xFFFF
	} state;
	friend constexpr State operator &(State l, State r) { return (State)((uint16_t)l & (uint16_t)r); }
	friend constexpr State operator |(State l, State r) { return (State)((uint16_t)l | (uint16_t)r); }
	friend constexpr State& operator &=(State& l, State r) { return (l = l & r); }
	friend constexpr State& operator |=(State& l, State r) { return (l = l | r); }

/*	friend class Queue;
	friend class InputQueue;
	friend class OutputQueue;*/
public:
	constexpr uint16_t getLen() const { return (uint16_t)(state & State::Length); }
	void* operator new(size_t sizeOfClass, size_t extraBytes = 0);
	void operator delete(void *packet);
};

#endif
