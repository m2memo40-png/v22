#ifndef FLUXMAX_TCP_H
#define FLUXMAX_TCP_H
#include <stddef.h>
#include <stdint.h>
#include "net/ipv4.h"
#include "net/stack.h"

/* Single-connection, outbound TCP transport for early user-facing tools.
 * It deliberately does not claim a complete BSD sockets implementation. */
#define FLUXMAX_TCP_MSS 1200u
#define FLUXMAX_TCP_RX_CAPACITY 16384u
void fluxmax_tcp_init(fluxmax_net_stack_t *stack);
void fluxmax_tcp_tick(uint64_t ticks);
void fluxmax_tcp_receive(fluxmax_ipv4_addr_t source, fluxmax_ipv4_addr_t destination,
                        const uint8_t *segment, size_t length, void *context);
int fluxmax_tcp_connect(fluxmax_ipv4_addr_t destination, uint16_t remote_port,
                       uint16_t local_port, uint64_t timeout_ticks);
int fluxmax_tcp_send(const void *data, size_t length, uint64_t timeout_ticks);
/* Returns bytes read, 0 for orderly remote close, and -1 for timeout/error. */
int fluxmax_tcp_receive_data(void *buffer, size_t capacity, uint64_t timeout_ticks);
int fluxmax_tcp_close(uint64_t timeout_ticks);
int fluxmax_tcp_is_connected(void);
/* Last transport failure: -1 local TX failure, -2 ACK/retransmit timeout,
 * -3 peer reset, -4 caller send timeout. Zero means no recorded failure. */
int fluxmax_tcp_last_error(void);
#endif
