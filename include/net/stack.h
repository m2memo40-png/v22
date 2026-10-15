#ifndef FLUXMAX_NET_STACK_H
#define FLUXMAX_NET_STACK_H
#include <stddef.h>
#include <stdint.h>
#include "net/ipv4.h"
#define FLUXMAX_NET_FRAME_MAX 1514u
#define FLUXMAX_NET_ARP_ENTRIES 16u
typedef int (*fluxmax_net_tx_fn)(const uint8_t *frame, size_t length, void *context);
typedef void (*fluxmax_net_udp_fn)(fluxmax_ipv4_addr_t source, uint16_t source_port,
                                  uint16_t destination_port, const uint8_t *payload,
                                  size_t payload_length, void *context);
typedef void (*fluxmax_net_tcp_fn)(fluxmax_ipv4_addr_t source, fluxmax_ipv4_addr_t destination,
                                  const uint8_t *segment, size_t segment_length, void *context);
typedef struct {
    uint8_t mac[6];
    fluxmax_ipv4_addr_t address, netmask, gateway;
    uint8_t configured;
    uint8_t reserved[3];
    uint32_t rx_frames, tx_frames, rx_dropped, arp_replies, icmp_echo_replies, udp_datagrams, tcp_segments;
    uint32_t ipv4_identification;
    uint16_t ping_identifier, ping_sequence;
    fluxmax_ipv4_addr_t ping_destination;
    uint8_t ping_waiting;
    uint8_t padding[3];
    struct { fluxmax_ipv4_addr_t ip; uint8_t mac[6]; uint8_t valid; } arp[FLUXMAX_NET_ARP_ENTRIES];
    uint32_t arp_next;
    fluxmax_net_tx_fn transmit;
    void *transmit_context;
    fluxmax_net_udp_fn udp_handler;
    void *udp_context;
    fluxmax_net_tcp_fn tcp_handler;
    void *tcp_context;
} fluxmax_net_stack_t;
void fluxmax_net_stack_init(fluxmax_net_stack_t *stack, const uint8_t mac[6],
                           fluxmax_net_tx_fn transmit, void *transmit_context);
void fluxmax_net_stack_configure(fluxmax_net_stack_t *stack, fluxmax_ipv4_addr_t address,
                                fluxmax_ipv4_addr_t netmask, fluxmax_ipv4_addr_t gateway);
void fluxmax_net_stack_set_udp_handler(fluxmax_net_stack_t *stack, fluxmax_net_udp_fn handler, void *context);
void fluxmax_net_stack_set_tcp_handler(fluxmax_net_stack_t *stack, fluxmax_net_tcp_fn handler, void *context);
/* Sends one IPv4 TCP segment. Returns 0 sent, 1 after requesting ARP resolution, -1 on error. */
int fluxmax_net_stack_send_tcp(fluxmax_net_stack_t *stack, fluxmax_ipv4_addr_t destination,
                              uint16_t source_port, uint16_t destination_port,
                              uint32_t sequence, uint32_t acknowledgement,
                              uint8_t flags, uint16_t window,
                              const void *payload, size_t payload_length);
int fluxmax_net_stack_receive(fluxmax_net_stack_t *stack, const uint8_t *frame, size_t length);
int fluxmax_net_stack_send_arp_request(fluxmax_net_stack_t *stack, fluxmax_ipv4_addr_t target);
int fluxmax_net_stack_send_udp(fluxmax_net_stack_t *stack, fluxmax_ipv4_addr_t destination,
                              uint16_t source_port, uint16_t destination_port,
                              const void *payload, size_t payload_length);
int fluxmax_net_stack_send_udp_broadcast(fluxmax_net_stack_t *stack,
                                        uint16_t source_port, uint16_t destination_port,
                                        const void *payload, size_t payload_length);
/* Returns 0 when sent, 1 when ARP resolution was requested, and -1 on error. */
int fluxmax_net_stack_ping(fluxmax_net_stack_t *stack, fluxmax_ipv4_addr_t destination,
                          uint16_t identifier, uint16_t sequence);
int fluxmax_net_stack_arp_lookup(const fluxmax_net_stack_t *stack, fluxmax_ipv4_addr_t ip, uint8_t mac_out[6]);
#endif
