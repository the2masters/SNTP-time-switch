#ifndef _PACKETBUFFER_H_
#define _PACKETBUFFER_H_

#include <stdalign.h> // alignof
#include <stdint.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
typedef struct {
	volatile uint16_t state;
	volatile uint8_t data[];
} __attribute__((packed, may_alias, aligned(alignof(uint32_t) > 2 ? alignof(uint32_t) : 2))) Packet_t;
#pragma GCC diagnostic pop

// Create a Buffer
Packet_t *Buffer_New(uint16_t len);
Packet_t *Buffer_New_unsafe(uint16_t len);
// Resize a buffer
Packet_t *Buffer_Resize(Packet_t *packet, uint16_t newlen);
Packet_t *Buffer_Resize_unsafe(Packet_t *packet, uint16_t newlen);

// Mark entry ready to be processed
void Buffer_PutInput(Packet_t *packet);
void Buffer_PutOutput(Packet_t *packet);

// Receiving out of Buffer
Packet_t *Buffer_GetInput(void);
Packet_t *Buffer_GetOutput(void);
Packet_t *Buffer_GetOutput_unsafe(void);

// Release Buffer
void Buffer_ReleaseInput(Packet_t *packet);
void Buffer_ReleaseOutput(Packet_t *packet);
void Buffer_ReleaseOutput_unsafe(Packet_t *packet);

#endif //_PACKETBUFFER_H_
