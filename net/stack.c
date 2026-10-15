#include "net/stack.h"
#include "lib/mem.h"
#include <stdint.h>

#define ETH_HEADER 14u
#define ETH_ARP 0x0806u
#define ETH_IPV4 0x0800u
#define ARP_BYTES 28u
#define IP_MIN_HEADER 20u
#define IP_PROTO_ICMP 1u
#define IP_PROTO_TCP 6u
#define IP_PROTO_UDP 17u
#define ICMP_HEADER 8u
#define UDP_HEADER 8u
#define DHCP_BROADCAST_MAC 0xFFu

static uint16_t get_be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static void put_be16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static int ip_equal(fluxmax_ipv4_addr_t a,fluxmax_ipv4_addr_t b) { return fluxmax_ipv4_to_u32(a)==fluxmax_ipv4_to_u32(b); }
static int ip_zero(fluxmax_ipv4_addr_t a) { return fluxmax_ipv4_to_u32(a)==0; }
static int mac_equal(const uint8_t *a,const uint8_t *b) { return memcmp(a,b,6)==0; }
static int ip_is_multicast_or_broadcast(fluxmax_ipv4_addr_t a) { uint8_t first=a.octet[0]; return first>=224u || fluxmax_ipv4_to_u32(a)==0xFFFFFFFFu; }
static void ethernet_header(uint8_t *out,const uint8_t destination[6],const uint8_t source[6],uint16_t type) {
    memcpy(out,destination,6); memcpy(out+6,source,6); put_be16(out+12,type);
}
static int transmit_frame(fluxmax_net_stack_t *s,const uint8_t *frame,size_t length) {
    if(!s || !s->transmit || !frame || length<ETH_HEADER || length>FLUXMAX_NET_FRAME_MAX) return -1;
    if(s->transmit(frame,length,s->transmit_context)!=0) return -1;
    ++s->tx_frames; return 0;
}
static void arp_learn(fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t ip,const uint8_t mac[6]) {
    if(!s || ip_zero(ip) || ip_is_multicast_or_broadcast(ip)) return;
    for(unsigned i=0;i<FLUXMAX_NET_ARP_ENTRIES;++i) if(s->arp[i].valid && ip_equal(s->arp[i].ip,ip)) {
        memcpy(s->arp[i].mac,mac,6); return;
    }
    unsigned slot=s->arp_next++%FLUXMAX_NET_ARP_ENTRIES;
    s->arp[slot].ip=ip; memcpy(s->arp[slot].mac,mac,6); s->arp[slot].valid=1;
}
int fluxmax_net_stack_arp_lookup(const fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t ip,uint8_t out[6]) {
    if(!s || !out) return -1;
    for(unsigned i=0;i<FLUXMAX_NET_ARP_ENTRIES;++i) if(s->arp[i].valid && ip_equal(s->arp[i].ip,ip)) {
        memcpy(out,s->arp[i].mac,6); return 0;
    }
    return -1;
}
void fluxmax_net_stack_init(fluxmax_net_stack_t *s,const uint8_t mac[6],fluxmax_net_tx_fn tx,void *ctx) {
    if(!s) return;
    memset(s,0,sizeof(*s)); if(mac) memcpy(s->mac,mac,6); s->transmit=tx; s->transmit_context=ctx;
    s->ipv4_identification=1; s->ping_identifier=0x5459u;
}
void fluxmax_net_stack_configure(fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t ip,fluxmax_ipv4_addr_t mask,fluxmax_ipv4_addr_t gateway) {
    if(!s) return;
    s->address=ip; s->netmask=mask; s->gateway=gateway; s->configured=(uint8_t)(!ip_zero(ip));
}
void fluxmax_net_stack_set_udp_handler(fluxmax_net_stack_t *s,fluxmax_net_udp_fn fn,void *ctx) { if(s){s->udp_handler=fn;s->udp_context=ctx;} }
void fluxmax_net_stack_set_tcp_handler(fluxmax_net_stack_t *s,fluxmax_net_tcp_fn fn,void *ctx) { if(s){s->tcp_handler=fn;s->tcp_context=ctx;} }
int fluxmax_net_stack_send_arp_request(fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t target) {
    uint8_t f[ETH_HEADER+ARP_BYTES]; static const uint8_t broadcast[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    if(!s || !s->transmit || ip_zero(target)) return -1;
    memset(f,0,sizeof(f));
    ethernet_header(f,broadcast,s->mac,ETH_ARP);
    put_be16(f+14,1); put_be16(f+16,ETH_IPV4); f[18]=6; f[19]=4; put_be16(f+20,1);
    memcpy(f+22,s->mac,6); memcpy(f+28,s->address.octet,4); memset(f+32,0,6); memcpy(f+38,target.octet,4);
    return transmit_frame(s,f,sizeof(f));
}
static int send_arp_reply(fluxmax_net_stack_t *s,const uint8_t remote_mac[6],fluxmax_ipv4_addr_t remote_ip) {
    uint8_t f[ETH_HEADER+ARP_BYTES]; memset(f,0,sizeof(f)); ethernet_header(f,remote_mac,s->mac,ETH_ARP);
    put_be16(f+14,1); put_be16(f+16,ETH_IPV4); f[18]=6; f[19]=4; put_be16(f+20,2);
    memcpy(f+22,s->mac,6); memcpy(f+28,s->address.octet,4); memcpy(f+32,remote_mac,6); memcpy(f+38,remote_ip.octet,4);
    if(transmit_frame(s,f,sizeof(f))==0) { ++s->arp_replies; return 0; } return -1;
}
static uint32_t checksum_add(uint32_t sum,const uint8_t *b,size_t len) {
    while(len>=2) { sum+=((uint16_t)b[0]<<8)|b[1]; b+=2; len-=2; sum=(sum&0xFFFFu)+(sum>>16); }
    if(len) sum+=(uint16_t)b[0]<<8;
    while(sum>>16) sum=(sum&0xFFFFu)+(sum>>16);
    return sum;
}
static uint16_t udp_checksum(fluxmax_ipv4_addr_t src,fluxmax_ipv4_addr_t dst,const uint8_t *udp,size_t len) {
    uint32_t sum=0; sum=checksum_add(sum,src.octet,4); sum=checksum_add(sum,dst.octet,4);
    uint8_t pseudo[4]={0,IP_PROTO_UDP,(uint8_t)(len>>8),(uint8_t)len}; sum=checksum_add(sum,pseudo,sizeof(pseudo));
    sum=checksum_add(sum,udp,len); return (uint16_t)~sum;
}
static uint16_t tcp_checksum(fluxmax_ipv4_addr_t src,fluxmax_ipv4_addr_t dst,const uint8_t *tcp,size_t len) {
    uint32_t sum=0; sum=checksum_add(sum,src.octet,4); sum=checksum_add(sum,dst.octet,4);
    uint8_t pseudo[4]={0,IP_PROTO_TCP,(uint8_t)(len>>8),(uint8_t)len}; sum=checksum_add(sum,pseudo,sizeof(pseudo));
    sum=checksum_add(sum,tcp,len); return (uint16_t)~sum;
}
static int send_ipv4_l2(fluxmax_net_stack_t *s,const uint8_t dst_mac[6],fluxmax_ipv4_addr_t src,fluxmax_ipv4_addr_t dst,
                        uint8_t protocol,const uint8_t *payload,size_t payload_len) {
    uint8_t frame[FLUXMAX_NET_FRAME_MAX]; size_t ip_len=IP_MIN_HEADER+payload_len;
    if(!s || !dst_mac || !payload || ip_len>1500u || ETH_HEADER+ip_len>FLUXMAX_NET_FRAME_MAX) return -1;
    ethernet_header(frame,dst_mac,s->mac,ETH_IPV4); uint8_t *ip=frame+ETH_HEADER; memset(ip,0,IP_MIN_HEADER);
    ip[0]=0x45u; put_be16(ip+2,(uint16_t)ip_len); put_be16(ip+4,(uint16_t)s->ipv4_identification++); put_be16(ip+6,0x4000u);
    ip[8]=64u; ip[9]=protocol; memcpy(ip+12,src.octet,4); memcpy(ip+16,dst.octet,4); put_be16(ip+10,fluxmax_ipv4_checksum(ip,IP_MIN_HEADER));
    memcpy(ip+IP_MIN_HEADER,payload,payload_len); return transmit_frame(s,frame,ETH_HEADER+ip_len);
}
static int mask_prefix(fluxmax_ipv4_addr_t mask,uint8_t *out);
static int next_hop(fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t dst,fluxmax_ipv4_addr_t *next) {
    uint8_t prefix=0;
    if(!s || !s->configured || !next || mask_prefix(s->netmask,&prefix)!=0) return -1;
    if(fluxmax_ipv4_prefix_match(s->address,dst,prefix)) *next=dst;
    else *next=s->gateway;
    return ip_zero(*next)?-1:0;
}
/* Prefix length from a contiguous netmask; rejects non-contiguous masks. */
static int mask_prefix(fluxmax_ipv4_addr_t mask,uint8_t *out) {
    uint32_t m=fluxmax_ipv4_to_u32(mask); int zero_seen=0; uint8_t n=0;
    if(m==0) return -1;
    for(int bit=31;bit>=0;--bit) { int one=(m&(1u<<bit))!=0; if(one && zero_seen) return -1; if(one) ++n; else zero_seen=1; }
    *out=n; return 0;
}
int fluxmax_net_stack_send_tcp(fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t dst,uint16_t sport,uint16_t dport,
                              uint32_t seq,uint32_t ack,uint8_t flags,uint16_t window,
                              const void *payload,size_t payload_len) {
    enum { TCP_HEADER=20u };
    if(!s || !s->configured || (!payload && payload_len) || payload_len+TCP_HEADER>1460u) return -1;
    uint8_t packet[1480], dst_mac[6]; size_t length=TCP_HEADER+payload_len;
    memset(packet,0,TCP_HEADER); put_be16(packet,sport); put_be16(packet+2,dport);
    packet[4]=(uint8_t)(seq>>24); packet[5]=(uint8_t)(seq>>16); packet[6]=(uint8_t)(seq>>8); packet[7]=(uint8_t)seq;
    packet[8]=(uint8_t)(ack>>24); packet[9]=(uint8_t)(ack>>16); packet[10]=(uint8_t)(ack>>8); packet[11]=(uint8_t)ack;
    packet[12]=0x50u; packet[13]=flags; put_be16(packet+14,window);
    if(payload_len) memcpy(packet+TCP_HEADER,payload,payload_len);
    put_be16(packet+16,tcp_checksum(s->address,dst,packet,length));
    fluxmax_ipv4_addr_t hop; if(next_hop(s,dst,&hop)!=0) return -1;
    if(fluxmax_net_stack_arp_lookup(s,hop,dst_mac)!=0) { (void)fluxmax_net_stack_send_arp_request(s,hop); return 1; }
    return send_ipv4_l2(s,dst_mac,s->address,dst,IP_PROTO_TCP,packet,length);
}
int fluxmax_net_stack_send_udp_broadcast(fluxmax_net_stack_t *s,uint16_t sport,uint16_t dport,const void *payload,size_t payload_len) {
    if(!s || (!payload && payload_len) || payload_len+UDP_HEADER>1472u) return -1;
    uint8_t packet[1472]; size_t length=UDP_HEADER+payload_len;
    put_be16(packet,sport); put_be16(packet+2,dport); put_be16(packet+4,(uint16_t)length); put_be16(packet+6,0);
    if(payload_len) memcpy(packet+UDP_HEADER,payload,payload_len);
    fluxmax_ipv4_addr_t zero={{0,0,0,0}}, broadcast={{255,255,255,255}};
    /* IPv4 UDP checksum is optional; leaving it zero is valid and interoperable for DHCP. */
    uint8_t dst_mac[6]; memset(dst_mac,DHCP_BROADCAST_MAC,6);
    return send_ipv4_l2(s,dst_mac,zero,broadcast,IP_PROTO_UDP,packet,length);
}
int fluxmax_net_stack_send_udp(fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t dst,uint16_t sport,uint16_t dport,
                              const void *payload,size_t payload_len) {
    if(!s || !s->configured || (!payload && payload_len) || payload_len+UDP_HEADER>1472u) return -1;
    uint8_t packet[1472], dst_mac[6]; size_t length=UDP_HEADER+payload_len;
    put_be16(packet,sport); put_be16(packet+2,dport); put_be16(packet+4,(uint16_t)length); put_be16(packet+6,0);
    if(payload_len) memcpy(packet+UDP_HEADER,payload,payload_len);
    uint16_t c=udp_checksum(s->address,dst,packet,length); if(!c) c=0xFFFFu; put_be16(packet+6,c);
    fluxmax_ipv4_addr_t hop;
    if(next_hop(s,dst,&hop)!=0) return -1;
    if(fluxmax_net_stack_arp_lookup(s,hop,dst_mac)!=0) { (void)fluxmax_net_stack_send_arp_request(s,hop); return 1; }
    return send_ipv4_l2(s,dst_mac,s->address,dst,IP_PROTO_UDP,packet,length);
}
int fluxmax_net_stack_ping(fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t dst,uint16_t identifier,uint16_t sequence) {
    if(!s || !s->configured || ip_zero(dst) || ip_is_multicast_or_broadcast(dst)) return -1;
    fluxmax_ipv4_addr_t hop; uint8_t mac_dst[6];
    if(next_hop(s,dst,&hop)!=0) return -1;
    if(fluxmax_net_stack_arp_lookup(s,hop,mac_dst)!=0) { (void)fluxmax_net_stack_send_arp_request(s,hop); return 1; }
    uint8_t payload[32]; memset(payload,0,sizeof(payload)); payload[0]=8; payload[1]=0;
    put_be16(payload+4,identifier); put_be16(payload+6,sequence);
    static const uint8_t marker[]="Fluxmax ping"; memcpy(payload+8,marker,sizeof(marker)-1u);
    put_be16(payload+2,fluxmax_ipv4_checksum(payload,sizeof(payload)));
    s->ping_identifier=identifier; s->ping_sequence=sequence; s->ping_destination=dst; s->ping_waiting=1;
    if(send_ipv4_l2(s,mac_dst,s->address,dst,IP_PROTO_ICMP,payload,sizeof(payload))!=0) { s->ping_waiting=0; return -1; }
    return 0;
}
static int process_arp(fluxmax_net_stack_t *s,const uint8_t *f,size_t n) {
    if(n<ETH_HEADER+ARP_BYTES) return -1;
    const uint8_t *a=f+ETH_HEADER;
    if(get_be16(a)!=1u || get_be16(a+2)!=ETH_IPV4 || a[4]!=6u || a[5]!=4u) return -1;
    uint16_t op=get_be16(a+6); if(op!=1u && op!=2u) return -1;
    if(!mac_equal(a+8,f+6)) return -1;
    fluxmax_ipv4_addr_t sender; memcpy(sender.octet,a+14,4); arp_learn(s,sender,a+8);
    if(op==1u && s->configured && memcmp(a+24,s->address.octet,4)==0) {
        fluxmax_ipv4_addr_t sender_ip; memcpy(sender_ip.octet,a+14,4);
        if(ip_zero(sender_ip)) return 0;
        return send_arp_reply(s,a+8,sender_ip);
    }
    return 0;
}
static int process_icmp(fluxmax_net_stack_t *s,const uint8_t *ethernet_src,fluxmax_ipv4_addr_t src,
                        fluxmax_ipv4_addr_t dst,const uint8_t *ip,const uint8_t *icmp,size_t length,
                        size_t ip_header_length) {
    if(length<ICMP_HEADER || fluxmax_ipv4_checksum(icmp,length)!=0) return -1;
    if(icmp[0]==0u && icmp[1]==0u) {
        if(s->ping_waiting && ip_equal(src,s->ping_destination) && get_be16(icmp+4)==s->ping_identifier &&
           get_be16(icmp+6)==s->ping_sequence) { ++s->icmp_echo_replies; s->ping_waiting=0; }
        return 0;
    }
    if(icmp[0]!=8u || icmp[1]!=0u || !s->configured || !ip_equal(dst,s->address) || ip_zero(src)) return 0;
    uint8_t reply[FLUXMAX_NET_FRAME_MAX]; size_t reply_ip_len=ip_header_length+length;
    if(ETH_HEADER+reply_ip_len>FLUXMAX_NET_FRAME_MAX) return -1;
    ethernet_header(reply,ethernet_src,s->mac,ETH_IPV4);
    uint8_t *out_ip=reply+ETH_HEADER; memcpy(out_ip,ip,ip_header_length+length);
    memcpy(out_ip+12,dst.octet,4); memcpy(out_ip+16,src.octet,4); out_ip[8]=64u;
    put_be16(out_ip+10,0); put_be16(out_ip+10,fluxmax_ipv4_checksum(out_ip,ip_header_length));
    uint8_t *out_icmp=out_ip+ip_header_length; out_icmp[0]=0; out_icmp[2]=0; out_icmp[3]=0;
    put_be16(out_icmp+2,fluxmax_ipv4_checksum(out_icmp,length));
    return transmit_frame(s,reply,ETH_HEADER+reply_ip_len);
}
static int process_udp(fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t src,fluxmax_ipv4_addr_t dst,
                       const uint8_t *udp,size_t length) {
    if(length<UDP_HEADER) return -1;
    uint16_t udp_len=get_be16(udp+4), check=get_be16(udp+6);
    if(udp_len<UDP_HEADER || udp_len>length) return -1;
    if(check && udp_checksum(src,dst,udp,udp_len)!=0) return -1;
    if(!s->udp_handler) return 0;
    s->udp_handler(src,get_be16(udp),get_be16(udp+2),udp+UDP_HEADER,udp_len-UDP_HEADER,s->udp_context);
    ++s->udp_datagrams; return 0;
}
static int process_tcp(fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t src,fluxmax_ipv4_addr_t dst,
                       const uint8_t *tcp,size_t length) {
    if(length<20u) return -1;
    size_t header=(size_t)(tcp[12]>>4)*4u;
    if(header<20u || header>length || tcp_checksum(src,dst,tcp,length)!=0) return -1;
    if(s->tcp_handler) s->tcp_handler(src,dst,tcp,length,s->tcp_context);
    ++s->tcp_segments; return 0;
}
int fluxmax_net_stack_receive(fluxmax_net_stack_t *s,const uint8_t *f,size_t n) {
    if(!s || !f || n<ETH_HEADER || n>FLUXMAX_NET_FRAME_MAX) return -1;
    ++s->rx_frames;
    uint16_t type=get_be16(f+12);
    if(type==ETH_ARP) {
        if(process_arp(s,f,n)!=0) { ++s->rx_dropped; return -1; }
        return 0;
    }
    if(type!=ETH_IPV4) return 0;
    if(n<ETH_HEADER+IP_MIN_HEADER) { ++s->rx_dropped; return -1; }
    const uint8_t *ip=f+ETH_HEADER;
    uint8_t version=ip[0]>>4, ihl=(uint8_t)((ip[0]&0x0Fu)*4u);
    uint16_t total=get_be16(ip+2), frag=get_be16(ip+6);
    if(version!=4u || ihl<IP_MIN_HEADER || ihl>60u || total<ihl || total>n-ETH_HEADER ||
       fluxmax_ipv4_checksum(ip,ihl)!=0 || (frag&0x3FFFu)!=0) { ++s->rx_dropped; return -1; }
    fluxmax_ipv4_addr_t src,dst; memcpy(src.octet,ip+12,4); memcpy(dst.octet,ip+16,4);
    if(!ip_zero(src)) arp_learn(s,src,f+6);
    if(s->configured && !ip_equal(dst,s->address) && fluxmax_ipv4_to_u32(dst)!=0xFFFFFFFFu) return 0;
    const uint8_t *payload=ip+ihl; size_t payload_len=(size_t)total-ihl;
    int rc=0;
    if(ip[9]==IP_PROTO_ICMP) rc=process_icmp(s,f+6,src,dst,ip,payload,payload_len,ihl);
    else if(ip[9]==IP_PROTO_UDP) rc=process_udp(s,src,dst,payload,payload_len);
    else if(ip[9]==IP_PROTO_TCP) rc=process_tcp(s,src,dst,payload,payload_len);
    if(rc!=0) { ++s->rx_dropped; return -1; }
    return 0;
}
