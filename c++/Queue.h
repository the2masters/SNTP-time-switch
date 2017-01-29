#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <assert.h>
#include <stdint.h>
#include "helper.h"
#include "Packet.h"

class RingBuffer
{
protected:
	static struct ringBuffer {
		Packet start[/*DIV_ROUND_UP(PACKETBUFFER_LEN, sizeof(Packet))*/30];
		Packet end[1];	///< Used to store StartOver flag in case of full ring
	} ringBuffer;

	static Packet* volatile nextWriter;
	static Packet* volatile inputReader;
	static Packet* volatile outputReader;

public:
	static Packet* Get(uint16_t length);
	static void Release(Packet *packet);
};

template<Packet::State queue>
class Queue : RingBuffer
{
public:
	/// Mark packet ready to be processed by chain.
	Packet* Pop();
	void Push(Packet *packet);
};

typedef Queue<Packet::State::Input> InputQueue;
typedef Queue<Packet::State::Output> OutputQueue;

#endif
