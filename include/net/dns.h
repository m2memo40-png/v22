#ifndef FLUXMAX_NET_DNS_H
#define FLUXMAX_NET_DNS_H
#include <stddef.h>
#include <stdint.h>
#include "net/ipv4.h"
/* Creates a standard recursive A/IN question. Returns 0 on success. */
int fluxmax_dns_build_a_query(const char *hostname, uint16_t identifier,
                             uint8_t *buffer, size_t capacity, size_t *length_out);
/* Parses a matching DNS response and returns the first answer A/IN record. */
int fluxmax_dns_parse_a_response(const uint8_t *packet, size_t length,
                                uint16_t identifier, fluxmax_ipv4_addr_t *address_out);
#endif
