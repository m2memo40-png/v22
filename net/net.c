#include "net/net.h"
#include "net/dns.h"
#include "net/tcp.h"
#include "drivers/rtl8139.h"
#include "drivers/e1000.h"
#include "kernel/log.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/pit.h"
#include "lib/mem.h"
#include <stdint.h>

static fluxmax_net_stack_t stack;
static int stack_ready;
#define NET_NIC_NONE 0
#define NET_NIC_RTL8139 1
#define NET_NIC_E1000 2
static int active_nic;
static uint16_t ping_sequence;
static uint32_t dhcp_xid;
static uint32_t dhcp_server_id;
static fluxmax_ipv4_addr_t dhcp_offered_ip, dhcp_dns;
static uint64_t dhcp_retry_tick, dhcp_bound_tick, dhcp_lease_ticks;
static uint16_t dns_query_id;
static uint8_t dns_query_waiting, dns_query_done;
static int dns_query_result_code;
static fluxmax_ipv4_addr_t dns_query_result;
static unsigned dhcp_state; /* 0=discover, 1=request, 2=bound, 3=static configuration */
static fluxmax_ipv4_addr_t ip4(unsigned a,unsigned b,unsigned c,unsigned d);
static int parse_netmask(fluxmax_ipv4_addr_t mask);
#define DHCP_CLIENT_PORT 68u
#define DHCP_SERVER_PORT 67u
#define DHCP_DISCOVER 1u
#define DHCP_OFFER 2u
#define DHCP_REQUEST 3u
#define DHCP_ACK 5u
#define DHCP_NAK 6u

static void dhcp_append(uint8_t *p,size_t *n,uint8_t code,const void *value,uint8_t length) {
    p[(*n)++]=code; p[(*n)++]=length; if(length){memcpy(p+*n,value,length);*n+=length;}
}
static void dhcp_send_discover(void) {
    uint8_t p[300]; memset(p,0,sizeof(p)); p[0]=1;p[1]=1;p[2]=6;
    p[4]=(uint8_t)(dhcp_xid>>24);p[5]=(uint8_t)(dhcp_xid>>16);p[6]=(uint8_t)(dhcp_xid>>8);p[7]=(uint8_t)dhcp_xid;
    p[10]=0x80u; memcpy(p+28,stack.mac,6);p[236]=99;p[237]=130;p[238]=83;p[239]=99;
    size_t n=240;uint8_t type=DHCP_DISCOVER;dhcp_append(p,&n,53,&type,1);
    static const uint8_t request_options[4]={1,3,6,51};dhcp_append(p,&n,55,request_options,sizeof(request_options));
    uint8_t client_id[7]={1,0,0,0,0,0,0};memcpy(client_id+1,stack.mac,6);dhcp_append(p,&n,61,client_id,sizeof(client_id));p[n++]=255;
    (void)fluxmax_net_stack_send_udp_broadcast(&stack,DHCP_CLIENT_PORT,DHCP_SERVER_PORT,p,n);
}
static void dhcp_send_request(void) {
    uint8_t p[300];memset(p,0,sizeof(p));p[0]=1;p[1]=1;p[2]=6;
    p[4]=(uint8_t)(dhcp_xid>>24);p[5]=(uint8_t)(dhcp_xid>>16);p[6]=(uint8_t)(dhcp_xid>>8);p[7]=(uint8_t)dhcp_xid;
    p[10]=0x80u;memcpy(p+28,stack.mac,6);p[236]=99;p[237]=130;p[238]=83;p[239]=99;
    size_t n=240;uint8_t type=DHCP_REQUEST;dhcp_append(p,&n,53,&type,1);
    dhcp_append(p,&n,50,dhcp_offered_ip.octet,4);
    uint8_t server[4]={(uint8_t)(dhcp_server_id>>24),(uint8_t)(dhcp_server_id>>16),(uint8_t)(dhcp_server_id>>8),(uint8_t)dhcp_server_id};
    dhcp_append(p,&n,54,server,4);static const uint8_t request_options[4]={1,3,6,51};dhcp_append(p,&n,55,request_options,sizeof(request_options));
    uint8_t client_id[7]={1,0,0,0,0,0,0};memcpy(client_id+1,stack.mac,6);dhcp_append(p,&n,61,client_id,sizeof(client_id));p[n++]=255;
    (void)fluxmax_net_stack_send_udp_broadcast(&stack,DHCP_CLIENT_PORT,DHCP_SERVER_PORT,p,n);
}
static int ip_value_is_zero(const fluxmax_ipv4_addr_t *a){return fluxmax_ipv4_to_u32(*a)==0;}
static void dhcp_udp_receive(fluxmax_ipv4_addr_t source,uint16_t source_port,uint16_t destination_port,
                             const uint8_t *p,size_t n,void *context) {
    (void)context;
    if(source_port==53u && destination_port==53000u && dns_query_waiting &&
       fluxmax_ipv4_to_u32(source)==fluxmax_ipv4_to_u32(dhcp_dns)) {
        int dr=fluxmax_dns_parse_a_response(p,n,dns_query_id,&dns_query_result);
        if(dr==0 || dr==-3 || dr==-2) { dns_query_result_code=dr; dns_query_done=1; dns_query_waiting=0; }
        return;
    }
    if(source_port!=DHCP_SERVER_PORT || destination_port!=DHCP_CLIENT_PORT || !p || n<240u ||
       p[0]!=2u || p[1]!=1u || p[2]<6u || p[236]!=99u || p[237]!=130u || p[238]!=83u || p[239]!=99u) return;
    uint32_t xid=((uint32_t)p[4]<<24)|((uint32_t)p[5]<<16)|((uint32_t)p[6]<<8)|p[7];
    if(xid!=dhcp_xid || memcmp(p+28,stack.mac,6)!=0) return;
    fluxmax_ipv4_addr_t yiaddr;memcpy(yiaddr.octet,p+16,4);
    uint8_t message=0; fluxmax_ipv4_addr_t mask={{255,255,255,0}},router=source,dns={{0,0,0,0}};
    uint32_t server=fluxmax_ipv4_to_u32(source),lease=3600u;
    size_t i=240;
    while(i<n) {
        uint8_t code=p[i++]; if(code==0) continue; if(code==255) break; if(i>=n) return;
        uint8_t len=p[i++]; if(len>n-i) return; const uint8_t *v=p+i;
        if(code==53u && len==1u) message=v[0];
        else if(code==54u && len==4u) server=((uint32_t)v[0]<<24)|((uint32_t)v[1]<<16)|((uint32_t)v[2]<<8)|v[3];
        else if(code==1u && len==4u) memcpy(mask.octet,v,4);
        else if(code==3u && len>=4u) memcpy(router.octet,v,4);
        else if(code==6u && len>=4u) memcpy(dns.octet,v,4);
        else if(code==51u && len==4u) lease=((uint32_t)v[0]<<24)|((uint32_t)v[1]<<16)|((uint32_t)v[2]<<8)|v[3];
        i+=len;
    }
    if(message==DHCP_OFFER && (dhcp_state==0u || dhcp_state==1u)) {
        if(ip_value_is_zero(&yiaddr)) return;
        dhcp_offered_ip=yiaddr;dhcp_server_id=server;dhcp_state=1u;dhcp_retry_tick=pit_ticks()+5000u;dhcp_send_request();
    } else if(message==DHCP_ACK && dhcp_state==1u) {
        if(!ip_value_is_zero(&yiaddr)) dhcp_offered_ip=yiaddr;
        if(parse_netmask(mask)!=0) mask=ip4(255,255,255,0);
        if(ip_value_is_zero(&router)) router=fluxmax_ipv4_from_u32(dhcp_server_id);
        fluxmax_net_stack_configure(&stack,dhcp_offered_ip,mask,router);dhcp_dns=dns;
        dhcp_state=2u;dhcp_bound_tick=pit_ticks();dhcp_lease_ticks=(uint64_t)(lease?lease:3600u)*1000u;
        log_write(LOG_INFO,"dhcp","Lease ACK accepted; IPv4/netmask/gateway configured (verify Internet reachability separately)");
    } else if(message==DHCP_NAK) { stack.configured=0;dhcp_state=0u;dhcp_retry_tick=0; }
}
static void dhcp_tick(void) {
    if(!stack_ready || dhcp_state==3u) return;
    uint64_t now=pit_ticks();
    if(dhcp_state==2u && dhcp_lease_ticks && now-dhcp_bound_tick>=dhcp_lease_ticks/2u) {
        dhcp_state=0u; dhcp_retry_tick=0; /* Refresh lease through a new discovery exchange. */
    }
    if((dhcp_state==0u || dhcp_state==1u) && now>=dhcp_retry_tick) {
        if(dhcp_state==1u) dhcp_state=0u;
        dhcp_send_discover(); dhcp_retry_tick=now+5000u;
    }
}

static int tx_bridge(const uint8_t *frame,size_t length,void *context) {
    (void)context;
    if (active_nic == NET_NIC_E1000) return e1000_send_frame(frame, length);
    if (active_nic == NET_NIC_RTL8139) return rtl8139_send_frame(frame, length);
    return -1;
}
static void rx_bridge(const uint8_t *frame,size_t length,void *context) {
    fluxmax_net_stack_t *s=(fluxmax_net_stack_t *)context;
    (void)fluxmax_net_stack_receive(s,frame,length);
}
static fluxmax_ipv4_addr_t ip4(unsigned a,unsigned b,unsigned c,unsigned d) { fluxmax_ipv4_addr_t x={{(uint8_t)a,(uint8_t)b,(uint8_t)c,(uint8_t)d}}; return x; }
static int parse_netmask(fluxmax_ipv4_addr_t mask) {
    uint32_t m=fluxmax_ipv4_to_u32(mask); int zero_seen=0;
    if(m==0) return -1;
    for(int bit=31;bit>=0;--bit) {
        int one=(m&(1u<<(unsigned)bit))!=0;
        if(one && zero_seen) return -1;
        if(one) continue;
        zero_seen=1;
    }
    return 0;
}
int net_init(void) {
    stack_ready=0;
    active_nic=NET_NIC_NONE;
    if(e1000_probe()==0) active_nic=NET_NIC_E1000;
    else if(rtl8139_probe()==0) active_nic=NET_NIC_RTL8139;
    else return -1;
    const uint8_t *mac = active_nic==NET_NIC_E1000 ? e1000_mac_address() : rtl8139_mac_address();
    if(!mac) { active_nic=NET_NIC_NONE; return -1; }
    fluxmax_net_stack_init(&stack,mac,tx_bridge,0);
    fluxmax_net_stack_set_udp_handler(&stack,dhcp_udp_receive,0);
    fluxmax_tcp_init(&stack);
    uint32_t tsc_low,tsc_high; __asm__ volatile("rdtsc" : "=a"(tsc_low), "=d"(tsc_high));
    dhcp_xid=tsc_low ^ tsc_high ^ ((uint32_t)mac[2]<<24) ^ ((uint32_t)mac[5]<<8) ^ 0x54595255u;
    if(!dhcp_xid) dhcp_xid=0x54595255u;
    dhcp_state=0u; dhcp_retry_tick=0; dhcp_dns=ip4(0,0,0,0);
    if(active_nic==NET_NIC_E1000) {
        e1000_set_rx_handler(rx_bridge,&stack);
        log_write(LOG_INFO,"net","Intel E1000 network path initialized: Ethernet/ARP/IPv4/ICMP/UDP, DHCP/DNS and plain HTTP over initial TCP");
    } else {
        rtl8139_set_rx_handler(rx_bridge,&stack);
        log_write(LOG_INFO,"net","Realtek RTL8139 network path initialized: Ethernet/ARP/IPv4/ICMP/UDP, DHCP/DNS and plain HTTP over initial TCP");
    }
    stack_ready=1;
    return 0;
}
void fluxmax_net_poll(void) {
    if(!stack_ready) return;
    if(active_nic==NET_NIC_E1000) e1000_poll();
    else if(active_nic==NET_NIC_RTL8139) rtl8139_poll();
    dhcp_tick(); fluxmax_tcp_tick(pit_ticks());
}
void fluxmax_net_task(void *argument) {
    (void)argument;
    if(!stack_ready) return;
    for(;;) { fluxmax_net_poll(); cpu_hlt(); }
}
int net_is_online(void) { return stack_ready && stack.configured; }
int fluxmax_net_configure(const char *address,const char *netmask,const char *gateway) {
    fluxmax_ipv4_addr_t ip,mask,gw;
    if(!stack_ready || fluxmax_ipv4_parse(address,&ip)!=0 || fluxmax_ipv4_parse(netmask,&mask)!=0 ||
       fluxmax_ipv4_parse(gateway,&gw)!=0 || parse_netmask(mask)!=0 ||
       fluxmax_ipv4_to_u32(ip)==0 || fluxmax_ipv4_to_u32(ip)==0xFFFFFFFFu) return -1;
    fluxmax_net_stack_configure(&stack,ip,mask,gw);
    dhcp_dns=ip4(0,0,0,0); dhcp_state=3u;
    log_write(LOG_INFO,"net","Static IPv4 settings applied; reachability must be tested separately");
    return 0;
}
int fluxmax_net_set_dns(const char *server) {
    fluxmax_ipv4_addr_t parsed;
    if(!stack_ready || fluxmax_ipv4_parse(server,&parsed)!=0 || fluxmax_ipv4_to_u32(parsed)==0 ||
       fluxmax_ipv4_to_u32(parsed)==0xFFFFFFFFu) return -1;
    dhcp_dns=parsed; return 0;
}
int fluxmax_net_resolve_ipv4(const char *hostname,uint64_t timeout_ticks,fluxmax_ipv4_addr_t *out) {
    uint8_t query[512];size_t query_len=0;
    if(!stack_ready || !stack.configured || !out || !timeout_ticks || fluxmax_ipv4_to_u32(dhcp_dns)==0) return -1;
    dns_query_id=(uint16_t)(dns_query_id+1u);if(!dns_query_id)dns_query_id=1u;
    if(fluxmax_dns_build_a_query(hostname,dns_query_id,query,sizeof(query),&query_len)!=0) return -1;
    dns_query_waiting=1;dns_query_done=0;dns_query_result_code=-1;
    uint64_t start=pit_ticks(),next_send=0;
    while(pit_ticks()-start<timeout_ticks) {
        if(dns_query_done) { int result=dns_query_result_code; if(result==0)*out=dns_query_result;dns_query_done=0;return result; }
        uint64_t elapsed=pit_ticks()-start;
        if(elapsed>=next_send) {
            int rc=fluxmax_net_stack_send_udp(&stack,dhcp_dns,53000u,53u,query,query_len);
            if(rc<0) { dns_query_waiting=0;return -1; }
            next_send=elapsed+(rc==0?1000u:100u);
        }
        cpu_pause();
    }
    dns_query_waiting=0;return -1;
}
int fluxmax_net_start_dhcp(void) {
    if(!stack_ready) return -1;
    stack.configured=0; dhcp_dns=ip4(0,0,0,0); dhcp_state=0u; dhcp_retry_tick=0; dhcp_lease_ticks=0; dhcp_bound_tick=0;
    return 0;
}
int fluxmax_net_ping_address(fluxmax_ipv4_addr_t destination,uint64_t timeout_ticks) {
    if(!stack_ready || !stack.configured || !timeout_ticks) return -1;
    uint32_t old_replies=stack.icmp_echo_replies;
    uint64_t start=pit_ticks(), last_try=0;
    uint16_t seq=++ping_sequence;
    for(;;) {
        if(stack.icmp_echo_replies!=old_replies) return 0;
        uint64_t now=pit_ticks();
        if(now-start>=timeout_ticks) { stack.ping_waiting=0; return -1; }
        if(now-start>=last_try) {
            int rc=fluxmax_net_stack_ping(&stack,destination,0x5459u,seq);
            if(rc==0) {
                /* An echo request is now outstanding; wait for its matching reply. */
                while(stack.ping_waiting) {
                    if(pit_ticks()-start>=timeout_ticks) { stack.ping_waiting=0; return -1; }
                    cpu_pause();
                }
                return stack.icmp_echo_replies!=old_replies?0:-1;
            }
            if(rc<0) return -1;
            last_try=now-start+100u; /* Rate-limit ARP retries to 100 ms at a 1 kHz PIT. */
        }
        cpu_pause();
    }
}
void fluxmax_net_get_status(fluxmax_net_status_t *out) {
    if(!out) return;
    memset(out,0,sizeof(*out));
    out->nic_present=stack_ready; out->ipv4_configured=stack_ready && stack.configured;
    out->driver_name = active_nic==NET_NIC_E1000 ? "Intel E1000" :
                       (active_nic==NET_NIC_RTL8139 ? "Realtek RTL8139" : "No supported NIC");
    if(stack_ready) {
        memcpy(out->mac,stack.mac,6); out->address=stack.address; out->netmask=stack.netmask; out->gateway=stack.gateway; out->dns_server=dhcp_dns;
        out->stack_rx=stack.rx_frames; out->stack_tx=stack.tx_frames; out->dropped=stack.rx_dropped;
        out->arp_replies=stack.arp_replies; out->icmp_echo_replies=stack.icmp_echo_replies; out->udp_datagrams=stack.udp_datagrams;
        if(active_nic==NET_NIC_E1000) { out->nic_rx=e1000_rx_count(); out->nic_tx=e1000_tx_count(); }
        else { out->nic_rx=rtl8139_rx_count(); out->nic_tx=rtl8139_tx_count(); }
    }
}
