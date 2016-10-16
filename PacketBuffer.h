#ifndef _PACKETBUFFER_H_
#define _PACKETBUFFER_H_

#include <stdalign.h> // alignof
#include <stdint.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
typedef struct {
	volatile uint16_t state;
	volatile uint8_t data[];
} __attribute__((packed, may_alias, aligned(alignof(uint32_t) == 4 ? 4 : 2))) Packet_t;
#pragma GCC diagnostic pop

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
