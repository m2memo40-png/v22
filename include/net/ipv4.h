#ifndef FLUXMAX_NET_IPV4_H
#define FLUXMAX_NET_IPV4_H
#include <stddef.h>
#include <stdint.h>
typedef struct { uint8_t octet[4]; } fluxmax_ipv4_addr_t;
typedef struct { fluxmax_ipv4_addr_t network; uint8_t prefix; uint32_t metric; uint32_t interface_id; } fluxmax_ipv4_route_t;
uint16_t fluxmax_ipv4_checksum(const void *bytes, size_t length);
int fluxmax_ipv4_parse(const char *text, fluxmax_ipv4_addr_t *out);
int fluxmax_ipv4_prefix_match(fluxmax_ipv4_addr_t a, fluxmax_ipv4_addr_t b, uint8_t prefix);
uint32_t fluxmax_ipv4_to_u32(fluxmax_ipv4_addr_t address);
fluxmax_ipv4_addr_t fluxmax_ipv4_from_u32(uint32_t value);
#endif
