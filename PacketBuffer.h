#ifndef _PACKETBUFFER_H_
#define _PACKETBUFFER_H_

#include <stdalign.h> // alignof
#include <stdint.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
///
typedef struct {
	/// length of a packet in bytes and flag bits in the 2 MSBs.
	volatile uint16_t state; ///< stores the length of a packet in lower bytes, and to flag bits
	volatile uint8_t data[];
} __attribute__((packed, may_alias, aligned(alignof(uint32_t) > 2 ? alignof(uint32_t) : 2))) Packet_t;
#pragma GCC diagnostic pop

/// Get length from the state field of a packet
static inline uint16_t Packet_getLen(uint16_t state)
{
	// Mask 2 MSBs
	return state & 0x3FFF;
}

/// Allocate memory for a new packet in FIFO buffer. (not threadsafe)
///
/// The operation could fail if there is not enough free memory in FIFO buffer.
/// New packets are marked unfinished until calling Packet_PutInput or Packet_PutOutput
/// to put them into the Input or Output chain.
/// @param[in] len Length of new packet in byte.
/// @returns Pointer to packet, whose data array is `len` byte long.
Packet_t *Packet_New(uint16_t len);

/// Resize packet in FIFO buffer. (not threadsafe)
///
/// If size is increased it could copy the packet to a new larger packet.
/// If size in decreased it could create dummy packets of type Skip from remainder of old packet.
///
/// If the operation succeeds, the pointer `packet` is invalid. If the size increses, the operation
/// could fail if there is not enough free memory in FIFO buffer. Then NULL is returned and the
/// old pointer `packet` is still valid.
/// @param[in] packet Pointer to packet, which should be resized.
/// @param[in] newlen New length of packet in byte.
/// @returns Pointer to packet, whose data array is `newlen` byte long.
Packet_t *Packet_Resize(Packet_t *packet, uint16_t newlen);

/// Mark packet ready to be processed by Input chain. (threadsafe)
void Packet_PutInput(Packet_t *packet);
/// Mark packet ready to be processed by Output chain. (threadsafe)
void Packet_PutOutput(Packet_t *packet);

/// Get a packet from the Input chain. If there is no ready packet, returns NULL. (not threadsafe)
Packet_t *Packet_GetInput(void);
/// Get a packet from the Output chain. If there is no ready packet, returns NULL. (not threadsafe)
Packet_t *Packet_GetOutput(void);

/// Release packet from Input chain, free memory. (not threadsafe)
void Packet_ReleaseInput(Packet_t *packet);
/// Release packet from Input chain, free memory. (not threadsafe)
void Packet_ReleaseOutput(Packet_t *packet);

/// Reattach packet from the Input chain to Output chain. (not threadsafe)
void Packet_ReattachOutput(Packet_t *packet);

#endif //_PACKETBUFFER_H_
