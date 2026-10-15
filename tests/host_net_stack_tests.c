#include "net/stack.h"
#include "lib/mem.h"
#include <stdio.h>
#include <stdint.h>

static unsigned passed, failed, tx_count, udp_calls, tcp_calls;
static uint8_t last_tx[FLUXMAX_NET_FRAME_MAX];
static size_t last_tx_len;
static uint16_t last_udp_sport,last_udp_dport;
static uint8_t last_udp_data[64]; static size_t last_udp_len;
static uint16_t last_tcp_sport,last_tcp_dport; static size_t last_tcp_len; static uint8_t last_tcp_data[32];
static void check(const char *name,int ok) { printf("[%s] %s\n",ok?"PASS":"FAIL",name); if(ok)++passed;else ++failed; }
static uint16_t be16(const uint8_t*p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static void w16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static int send_capture(const uint8_t *f,size_t n,void *ctx){(void)ctx;if(n>sizeof(last_tx))return -1;memcpy(last_tx,f,n);last_tx_len=n;++tx_count;return 0;}
static void udp_capture(fluxmax_ipv4_addr_t src,uint16_t sp,uint16_t dp,const uint8_t*p,size_t n,void*ctx){(void)src;(void)ctx;++udp_calls;last_udp_sport=sp;last_udp_dport=dp;last_udp_len=n;if(n>sizeof(last_udp_data))last_udp_len=sizeof(last_udp_data);memcpy(last_udp_data,p,last_udp_len);}
static void tcp_capture(fluxmax_ipv4_addr_t src,fluxmax_ipv4_addr_t dst,const uint8_t*p,size_t n,void*ctx){(void)src;(void)dst;(void)ctx;++tcp_calls;last_tcp_sport=be16(p);last_tcp_dport=be16(p+2);last_tcp_len=n;if(n>sizeof(last_tcp_data))last_tcp_len=sizeof(last_tcp_data);memcpy(last_tcp_data,p,last_tcp_len);}
static uint16_t tcp_checksum(fluxmax_ipv4_addr_t s,fluxmax_ipv4_addr_t d,const uint8_t*p,size_t n){uint32_t sum=0;for(int i=0;i<4;i+=2)sum+=((uint16_t)s.octet[i]<<8)|s.octet[i+1];for(int i=0;i<4;i+=2)sum+=((uint16_t)d.octet[i]<<8)|d.octet[i+1];sum+=6;sum+=(uint16_t)n;for(size_t i=0;i<n;i+=2){sum+=(uint16_t)p[i]<<8;if(i+1<n)sum+=p[i+1];while(sum>>16)sum=(sum&0xffff)+(sum>>16);}while(sum>>16)sum=(sum&0xffff)+(sum>>16);return (uint16_t)~sum;}
static fluxmax_ipv4_addr_t ip4(unsigned a,unsigned b,unsigned c,unsigned d){fluxmax_ipv4_addr_t x={{a,b,c,d}};return x;}
static size_t make_arp(uint8_t*f,const uint8_t dstmac[6],const uint8_t srcmac[6],uint16_t op,fluxmax_ipv4_addr_t src,fluxmax_ipv4_addr_t target){memset(f,0,42);memcpy(f,dstmac,6);memcpy(f+6,srcmac,6);w16(f+12,0x0806);w16(f+14,1);w16(f+16,0x0800);f[18]=6;f[19]=4;w16(f+20,op);memcpy(f+22,srcmac,6);memcpy(f+28,src.octet,4);memcpy(f+32,dstmac,6);memcpy(f+38,target.octet,4);return 42;}
static size_t make_ipv4(uint8_t*f,const uint8_t dstmac[6],const uint8_t srcmac[6],fluxmax_ipv4_addr_t src,fluxmax_ipv4_addr_t dst,uint8_t proto,const uint8_t*payload,size_t plen){size_t n=14+20+plen;memset(f,0,n);memcpy(f,dstmac,6);memcpy(f+6,srcmac,6);w16(f+12,0x0800);uint8_t*ip=f+14;ip[0]=0x45;w16(ip+2,(uint16_t)(20+plen));w16(ip+4,7);w16(ip+6,0x4000);ip[8]=64;ip[9]=proto;memcpy(ip+12,src.octet,4);memcpy(ip+16,dst.octet,4);w16(ip+10,fluxmax_ipv4_checksum(ip,20));memcpy(ip+20,payload,plen);return n;}
static uint16_t udp_checksum(fluxmax_ipv4_addr_t s,fluxmax_ipv4_addr_t d,const uint8_t*u,size_t n){uint32_t sum=0;const uint8_t*p=s.octet;for(int i=0;i<4;i+=2)sum+=((uint16_t)p[i]<<8)|p[i+1];p=d.octet;for(int i=0;i<4;i+=2)sum+=((uint16_t)p[i]<<8)|p[i+1];sum+=17;sum+=(uint16_t)n;p=u;for(size_t i=0;i<n;i+=2){sum+=(uint16_t)p[i]<<8;if(i+1<n)sum+=p[i+1];while(sum>>16)sum=(sum&0xffff)+(sum>>16);}while(sum>>16)sum=(sum&0xffff)+(sum>>16);return (uint16_t)~sum;}
int main(void){
 const uint8_t ours[6]={0x52,0x54,0,0x12,0x34,0x56}, peer[6]={0x52,0x54,0,0x65,0x43,0x21}, broadcast[6]={255,255,255,255,255,255};
 fluxmax_net_stack_t s; fluxmax_net_stack_init(&s,ours,send_capture,0); fluxmax_net_stack_configure(&s,ip4(192,168,1,10),ip4(255,255,255,0),ip4(192,168,1,1)); fluxmax_net_stack_set_udp_handler(&s,udp_capture,0); fluxmax_net_stack_set_tcp_handler(&s,tcp_capture,0);
 uint8_t f[1514]; size_t n=make_arp(f,broadcast,peer,1,ip4(192,168,1,20),ip4(192,168,1,10));
 check("ARP request for local IP sends reply",fluxmax_net_stack_receive(&s,f,n)==0 && tx_count==1 && be16(last_tx+20)==2 && memcmp(last_tx,peer,6)==0 && memcmp(last_tx+28,s.address.octet,4)==0);
 uint8_t cached[6]={0}; check("ARP sender added to neighbor cache",fluxmax_net_stack_arp_lookup(&s,ip4(192,168,1,20),cached)==0 && memcmp(cached,peer,6)==0);
 unsigned before=tx_count; n=make_arp(f,broadcast,peer,1,ip4(192,168,1,20),ip4(192,168,1,99));
 check("ARP request for another host does not get a reply",fluxmax_net_stack_receive(&s,f,n)==0 && tx_count==before);
 uint8_t icmp[16]={8,0,0,0,0x12,0x34,0,1,'t','e','s','t','-','p','i','n'};w16(icmp+2,fluxmax_ipv4_checksum(icmp,sizeof(icmp)));
 n=make_ipv4(f,ours,peer,ip4(192,168,1,20),s.address,1,icmp,sizeof(icmp));
 check("Valid ICMP echo request gets echo reply",fluxmax_net_stack_receive(&s,f,n)==0 && be16(last_tx+12)==0x0800 && memcmp(last_tx,peer,6)==0 && last_tx[34]==0 && fluxmax_ipv4_checksum(last_tx+14,20)==0 && fluxmax_ipv4_checksum(last_tx+34,16)==0 && memcmp(last_tx+26,s.address.octet,4)==0 && memcmp(last_tx+30,ip4(192,168,1,20).octet,4)==0);
 icmp[2]^=1;n=make_ipv4(f,ours,peer,ip4(192,168,1,20),s.address,1,icmp,sizeof(icmp));before=s.rx_dropped;
 check("Bad ICMP checksum is dropped",fluxmax_net_stack_receive(&s,f,n)!=0 && s.rx_dropped==before+1);
 icmp[2]^=1;n=make_ipv4(f,ours,peer,ip4(192,168,1,20),s.address,1,icmp,sizeof(icmp));w16(f+20,0x2000);w16(f+24,0);w16(f+24,fluxmax_ipv4_checksum(f+14,20));before=s.rx_dropped;
 check("IPv4 fragments are rejected safely",fluxmax_net_stack_receive(&s,f,n)!=0 && s.rx_dropped==before+1);
 uint8_t udp[16]={0};w16(udp,12345);w16(udp+2,9000);w16(udp+4,sizeof(udp));memcpy(udp+8,"udp-test",8);
 n=make_ipv4(f,ours,peer,ip4(192,168,1,20),s.address,17,udp,sizeof(udp));
 check("UDP datagram validated and delivered to registered handler",fluxmax_net_stack_receive(&s,f,n)==0 && udp_calls==1 && last_udp_sport==12345 && last_udp_dport==9000 && last_udp_len==8 && memcmp(last_udp_data,"udp-test",8)==0);
 w16(udp+6,1);n=make_ipv4(f,ours,peer,ip4(192,168,1,20),s.address,17,udp,sizeof(udp));before=s.rx_dropped;
 check("UDP packet with invalid nonzero checksum is rejected",fluxmax_net_stack_receive(&s,f,n)!=0 && s.rx_dropped==before+1);
 w16(udp+6,0);w16(udp+4,3);n=make_ipv4(f,ours,peer,ip4(192,168,1,20),s.address,17,udp,sizeof(udp));before=s.rx_dropped;
 check("UDP length smaller than header is rejected",fluxmax_net_stack_receive(&s,f,n)!=0 && s.rx_dropped==before+1);
 int urc=fluxmax_net_stack_send_udp(&s,ip4(192,168,1,20),1111,2222,"hello",5);
 check("UDP transmit builds IPv4 Ethernet frame with a valid checksum",urc==0 && be16(last_tx+12)==0x0800 && be16(last_tx+34)==1111 && be16(last_tx+36)==2222 && fluxmax_ipv4_checksum(last_tx+14,20)==0 && udp_checksum(ip4(192,168,1,10),ip4(192,168,1,20),last_tx+34,13)==0);
 int prc=fluxmax_net_stack_ping(&s,ip4(192,168,1,20),0x5459,2);
 check("ICMP ping transmit uses cached ARP neighbor",prc==0 && s.ping_waiting && last_tx[34]==8 && fluxmax_ipv4_checksum(last_tx+34,32)==0);
 uint8_t tcp[20]={0}; w16(tcp,80); w16(tcp+2,49152); tcp[4]=0;tcp[5]=0;tcp[6]=0;tcp[7]=12; tcp[8]=0;tcp[9]=0;tcp[10]=0;tcp[11]=1; tcp[12]=0x50;tcp[13]=0x10;w16(tcp+14,4096); w16(tcp+16,tcp_checksum(ip4(192,168,1,20),s.address,tcp,sizeof(tcp)));
 n=make_ipv4(f,ours,peer,ip4(192,168,1,20),s.address,6,tcp,sizeof(tcp));
 check("Valid TCP segment passes pseudo-header checksum and reaches handler",fluxmax_net_stack_receive(&s,f,n)==0 && tcp_calls==1 && last_tcp_sport==80 && last_tcp_dport==49152 && last_tcp_len==20 && s.tcp_segments==1);
 tcp[16]^=1;n=make_ipv4(f,ours,peer,ip4(192,168,1,20),s.address,6,tcp,sizeof(tcp));before=s.rx_dropped;
 check("TCP segment with bad checksum is dropped",fluxmax_net_stack_receive(&s,f,n)!=0 && s.rx_dropped==before+1 && tcp_calls==1);
 int trc=fluxmax_net_stack_send_tcp(&s,ip4(192,168,1,20),49152,80,0x12345678u,0x10203040u,0x18u,4096,"hello",5);
 check("TCP transmit emits ports, sequence, ACK and valid checksum",trc==0 && be16(last_tx+34)==49152 && be16(last_tx+36)==80 && fluxmax_ipv4_checksum(last_tx+14,20)==0 && tcp_checksum(s.address,ip4(192,168,1,20),last_tx+34,25)==0 && last_tx[47]==0x18 && memcmp(last_tx+54,"hello",5)==0);
 check("TCP rejects segment payload exceeding the MTU-safe MSS",fluxmax_net_stack_send_tcp(&s,ip4(192,168,1,20),49152,80,1,1,0x10,4096,last_tcp_data,1461)<0);
 check("Malformed Ethernet frame safely rejected",fluxmax_net_stack_receive(&s,f,8)!=0);
 fluxmax_net_stack_t invalid_mask; fluxmax_net_stack_init(&invalid_mask,ours,send_capture,0);
 fluxmax_net_stack_configure(&invalid_mask,ip4(192,168,1,10),ip4(0,0,0,0),ip4(192,168,1,1)); before=tx_count;
 check("Zero netmask is rejected for routed UDP transmit",fluxmax_net_stack_send_udp(&invalid_mask,ip4(8,8,8,8),1111,53,"x",1)<0 && tx_count==before);
 printf("\nNetwork stack tests: %u passed, %u failed.\n",passed,failed);return failed?1:0;
}
