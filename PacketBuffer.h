#ifndef _PACKETBUFFER_H_
#define _PACKETBUFFER_H_

#include <stdint.h>

typedef struct
{
        volatile uint16_t state;
        volatile uint16_t data[];
} __attribute__((__packed__, __may_alias__)) Packet_t;

// Create a Buffer
Packet_t *Buffer_New(uint16_t len);
// Extend a buffer, only possible, if packet == last created buffer
Packet_t *Buffer_Extend(Packet_t *packet, uint16_t newlen);

// Mark entry ready to be processed
void Buffer_PutForUSB(Packet_t *packet);
void Buffer_PutForEthernet(Packet_t *packet);

// Receiving out of Buffer
Packet_t *Buffer_GetForEthernet(void);
Packet_t *Buffer_GetForUSB(void);

// Release Buffer
void Buffer_ReleaseEthernet(Packet_t *packet);
void Buffer_ReleaseUSB(Packet_t *packet);

__attribute__((const)) inline uint16_t Packet_getLen(uint16_t state)
{
        return state & 0x3FFF;
}
#endif //_PACKETBUFFER_H_
