#ifndef _PACKETBUFFER_H_
#define _PACKETBUFFER_H_

#include <stddef.h> // offsetof
#include <stdint.h>

typedef struct {
	uint16_t state;
	uint16_t data[1];	// used as flexible array member
} __attribute__((packed, may_alias)) Packet_t;

#define PacketHeaderLen offsetof(Packet_t, data[0])

// Create a Buffer
Packet_t *Buffer_New(uint16_t len);
// Extend a buffer, only possible, if packet == last created buffer
Packet_t *Buffer_Extend(Packet_t *packet, uint16_t newlen);

// Mark entry ready to be processed
void Buffer_PutInput(Packet_t *packet);
void Buffer_PutOutput(Packet_t *packet);

// Receiving out of Buffer
Packet_t *Buffer_GetInput(void);
Packet_t *Buffer_GetOutput(void);

// Release Buffer
void Buffer_ReleaseInput(Packet_t *packet);
void Buffer_ReleaseOutput(Packet_t *packet);

__attribute__((const)) inline uint16_t Packet_getLen(uint16_t state)
{
        return state & 0x3FFF;
}
#endif //_PACKETBUFFER_H_
