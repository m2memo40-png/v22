#include "net/dns.h"
#include "lib/mem.h"

static uint16_t be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0]<<8)|p[1]); }
static void put16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v; }
int fluxmax_dns_build_a_query(const char *name,uint16_t id,uint8_t *out,size_t cap,size_t *out_len) {
    if(!name || !out || !out_len || cap<17u) return -1;
    size_t n=0;while(name[n] && n<=253u)++n;
    if(!n || n>253u) return -1;
    memset(out,0,12);put16(out,id);put16(out+2,0x0100u);put16(out+4,1u);size_t pos=12,start=0;
    while(start<n) {
        size_t end=start;while(end<n && name[end]!='.')++end;
        size_t label_len=end-start;if(!label_len || label_len>63u || pos+1u+label_len+5u>cap) return -1;
        out[pos++]=(uint8_t)label_len;
        for(size_t i=start;i<end;++i) {
            unsigned char c=(unsigned char)name[i];
            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-')) return -1;
            out[pos++]=c;
        }
        if(end==n) { start=n; break; }
        start=end+1u;if(start==n)return -1;
    }
    out[pos++]=0;put16(out+pos,1u);pos+=2;put16(out+pos,1u);pos+=2;*out_len=pos;return 0;
}
/* Skip a possibly compressed DNS name, retaining the end of the encoded name. */
static int skip_name(const uint8_t *p,size_t n,size_t *offset) {
    size_t pos=*offset,return_pos=0,steps=0;int jumped=0;
    for(;;) {
        if(pos>=n || ++steps>n) return -1;
        uint8_t c=p[pos++];
        if(c==0) { if(!jumped)return_pos=pos;*offset=return_pos;return 0; }
        if((c&0xC0u)==0xC0u) {
            if(pos>=n)return -1;
            size_t target=((size_t)(c&0x3Fu)<<8)|p[pos++];if(target>=n)return -1;
            if(!jumped){return_pos=pos;jumped=1;}pos=target;continue;
        }
        if(c&0xC0u || c>63u || (size_t)c>n-pos)return -1;
        pos+=(size_t)c;
    }
}
int fluxmax_dns_parse_a_response(const uint8_t *p,size_t n,uint16_t id,fluxmax_ipv4_addr_t *out) {
    if(!p || !out || n<12u || be16(p)!=id) return -1;
    uint16_t flags=be16(p+2),questions=be16(p+4),answers=be16(p+6);
    if(!(flags&0x8000u) || (flags&0x0200u) || (flags&0x000Fu)!=0u) return -2;
    if(questions>32u || answers>256u) return -1;
    size_t pos=12;
    for(uint16_t i=0;i<questions;++i) { if(skip_name(p,n,&pos)!=0 || pos+4u>n)return -1;pos+=4u; }
    for(uint16_t i=0;i<answers;++i) {
        if(skip_name(p,n,&pos)!=0 || pos+10u>n)return -1;
        uint16_t type=be16(p+pos),klass=be16(p+pos+2),rdlen=be16(p+pos+8);pos+=10u;
        if((size_t)rdlen>n-pos)return -1;
        if(type==1u && klass==1u && rdlen==4u) { memcpy(out->octet,p+pos,4);return 0; }
        pos+=(size_t)rdlen;
    }
    return -3;
}
