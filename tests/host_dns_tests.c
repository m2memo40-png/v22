#include "net/dns.h"
#include "lib/mem.h"
#include <stdio.h>
#include <stdint.h>
static unsigned passed,failed;
static void check(const char *n,int ok){printf("[%s] %s\n",ok?"PASS":"FAIL",n);if(ok)++passed;else ++failed;}
static uint16_t be16(const uint8_t*p){return(uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static void w16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static size_t make_response(uint8_t*p,uint16_t id){memset(p,0,128);w16(p,id);w16(p+2,0x8180);w16(p+4,1);w16(p+6,1);size_t n=12;
 const char*labels[]={"www","example","com"};for(unsigned i=0;i<3;++i){size_t l=0;while(labels[i][l])++l;p[n++]=(uint8_t)l;memcpy(p+n,labels[i],l);n+=l;}p[n++]=0;w16(p+n,1);n+=2;w16(p+n,1);n+=2;
 p[n++]=0xC0;p[n++]=0x0C;w16(p+n,1);n+=2;w16(p+n,1);n+=2;p[n++]=0;p[n++]=0;p[n++]=0;p[n++]=60;w16(p+n,4);n+=2;p[n++]=93;p[n++]=184;p[n++]=216;p[n++]=34;return n;}
int main(void){uint8_t buf[512];size_t len=0;check("Build recursive A query",fluxmax_dns_build_a_query("www.example.com",0x1234,buf,sizeof(buf),&len)==0&&be16(buf)==0x1234&&be16(buf+2)==0x0100&&be16(buf+4)==1&&len==33);
 check("Query ends with A/IN question",len==33&&buf[len-4]==0&&buf[len-3]==1&&buf[len-2]==0&&buf[len-1]==1);
 check("Reject empty hostname and empty labels",fluxmax_dns_build_a_query("",1,buf,sizeof(buf),&len)!=0&&fluxmax_dns_build_a_query("www..example.com",1,buf,sizeof(buf),&len)!=0&&fluxmax_dns_build_a_query("www.example.",1,buf,sizeof(buf),&len)!=0);
 char long_label[80];for(unsigned i=0;i<70;++i)long_label[i]='a';long_label[70]='.';long_label[71]='x';long_label[72]=0;
 check("Reject DNS label longer than 63 octets",fluxmax_dns_build_a_query(long_label,1,buf,sizeof(buf),&len)!=0);
 check("Reject too-small destination buffer",fluxmax_dns_build_a_query("www.example.com",1,buf,16,&len)!=0);
 uint8_t response[128];size_t n=make_response(response,0x1234);fluxmax_ipv4_addr_t ip={{0}};
 check("Parse compressed A answer",fluxmax_dns_parse_a_response(response,n,0x1234,&ip)==0&&ip.octet[0]==93&&ip.octet[1]==184&&ip.octet[2]==216&&ip.octet[3]==34);
 check("Reject mismatched response identifier",fluxmax_dns_parse_a_response(response,n,0x9999,&ip)!=0);
 response[3]=3;check("Reject DNS error response",fluxmax_dns_parse_a_response(response,n,0x1234,&ip)==-2);response[3]=0x80;
 check("Reject truncated DNS resource record",fluxmax_dns_parse_a_response(response,n-2,0x1234,&ip)!=0);
 response[2]=0x01;check("Reject DNS packet that is not a response",fluxmax_dns_parse_a_response(response,n,0x1234,&ip)==-2);
 printf("\nDNS tests: %u passed, %u failed.\n",passed,failed);return failed?1:0;}
