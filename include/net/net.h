#ifndef FLUXMAX_NET_H
#define FLUXMAX_NET_H
#include <stddef.h>
#include <stdint.h>
#include "net/ipv4.h"
#include "net/stack.h"
typedef struct {
    int nic_present;
    int ipv4_configured;
    const char *driver_name;
    fluxmax_ipv4_addr_t address, netmask, gateway, dns_server;
    uint8_t mac[6];
    uint64_t nic_rx, nic_tx;
    uint32_t stack_rx, stack_tx, dropped, arp_replies, icmp_echo_replies, udp_datagrams;
} fluxmax_net_status_t;
int net_init(void);
int net_is_online(void); /* Means NIC+IPv4 configured; it does not prove internet reachability. */
void fluxmax_net_poll(void);
void fluxmax_net_task(void *argument);
int fluxmax_net_configure(const char *address, const char *netmask, const char *gateway);
int fluxmax_net_start_dhcp(void);
int fluxmax_net_set_dns(const char *server);
int fluxmax_net_resolve_ipv4(const char *hostname, uint64_t timeout_ticks, fluxmax_ipv4_addr_t *address_out);
int fluxmax_net_ping_address(fluxmax_ipv4_addr_t destination, uint64_t timeout_ticks);
void fluxmax_net_get_status(fluxmax_net_status_t *status);
#endif
