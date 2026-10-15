#include "browser/browser.h"
#include "browser/html.h"
#include "net/net.h"
#include "net/tcp.h"
#include "kernel/log.h"
#include "mm/heap.h"
#include "lib/mem.h"
#include "gui/console.h"
#include "drivers/keyboard.h"
#include "arch/x86_64/io.h"
#include <stddef.h>
#include <stdint.h>

#define HTTP_RESPONSE_LIMIT (128u*1024u)
#define HTTP_HEADER_LIMIT 8192u
#define HTTP_HOST_LIMIT 255u
#define HTTP_PATH_LIMIT 1024u

static fluxmax_browser_putc_fn browser_output_sink;
static void *browser_output_context;

void browser_set_output(fluxmax_browser_putc_fn output, void *context) {
    browser_output_sink = output;
    browser_output_context = context;
}

static int lower(int c) { return c>='A'&&c<='Z'?c+('a'-'A'):c; }
static int span_eq_ci(const char *a,size_t n,const char *b) {
    size_t i=0; while(b[i]) { if(i>=n || lower((unsigned char)a[i])!=lower((unsigned char)b[i])) return 0; ++i; } return i==n;
}
static void browser_putc(char c,void *ctx) { (void)ctx; if (browser_output_sink) browser_output_sink(c, browser_output_context); else fluxmax_display_console_putc(c); serial_write_char(c); }
static void print_both(const char *s) { if (!s) return; if (browser_output_sink) { for (size_t i=0; s[i]; ++i) browser_output_sink(s[i], browser_output_context); } else fluxmax_display_console_write(s); serial_write(s); }
static int find_bytes(const char *haystack,size_t hay_len,const char *needle,size_t needle_len) {
    if(!haystack || !needle || !needle_len || needle_len>hay_len) return -1;
    for(size_t i=0;i<=hay_len-needle_len;++i) { size_t j=0; while(j<needle_len && haystack[i+j]==needle[j])++j; if(j==needle_len)return (int)i; }
    return -1;
}
static int parse_url(const char *url,char *host,size_t host_cap,uint16_t *port,char *path,size_t path_cap) {
    static const char prefix[]="http://";
    if(!url || !host || !port || !path || host_cap<2 || path_cap<2) return -1;
    for(size_t i=0;i<sizeof(prefix)-1u;++i) if(!url[i] || url[i]!=prefix[i]) return -1;
    const char *p=url+sizeof(prefix)-1u, *host_start=p;
    while(*p && *p!='/' && *p!='?' && *p!='#') ++p;
    size_t host_len=(size_t)(p-host_start); if(!host_len || host_len>=host_cap || host_len>HTTP_HOST_LIMIT) return -1;
    size_t name_len=host_len; *port=80;
    for(size_t i=0;i<host_len;++i) if(host_start[i]==':') {
        if(i==0 || i+1>=host_len) return -1;
        unsigned n=0; for(size_t j=i+1;j<host_len;++j) { if(host_start[j]<'0'||host_start[j]>'9') return -1; n=n*10u+(unsigned)(host_start[j]-'0'); if(n>65535u)return -1; }
        if(!n) return -1;
        *port=(uint16_t)n; name_len=i; break;
    }
    if(!name_len || name_len>=host_cap) return -1;
    for(size_t i=0;i<name_len;++i) { char c=host_start[i]; if(!( (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='-')) return -1; host[i]=c; }
    host[name_len]='\0';
    if(!*p || *p=='#') { path[0]='/'; path[1]='\0'; }
    else {
        size_t n=0;
        if(*p=='?') path[n++]='/';
        while(*p && *p!='#' && n+1<path_cap) path[n++]=*p++;
        if(*p && *p!='#') return -1;
        path[n]='\0';
    }
    return 0;
}
static int header_value(const char *headers,size_t n,const char *key,char *out,size_t cap) {
    size_t key_len=0; while(key[key_len]) ++key_len;
    size_t pos=0;
    while(pos<n) {
        size_t end=pos; while(end+1<n && !(headers[end]=='\r'&&headers[end+1]=='\n')) ++end;
        if(end==pos) break;
        const char *colon=0; for(size_t i=pos;i<end;++i) if(headers[i]==':') { colon=headers+i; break; }
        if(colon && span_eq_ci(headers+pos,(size_t)(colon-(headers+pos)),key)) {
            size_t b=(size_t)(colon-headers)+1u; while(b<end && (headers[b]==' '||headers[b]=='\t')) ++b;
            size_t e=end; while(e>b && (headers[e-1]==' '||headers[e-1]=='\t')) --e;
            size_t l=e-b; if(!cap || l>=cap)return -1; for(size_t i=0;i<l;++i)out[i]=headers[b+i]; out[l]='\0'; return (int)l;
        }
        pos=end+2u;
    }
    return 0;
}
static int parse_u32_decimal(const char *s,uint32_t *out) {
    if(!s || !*s || !out) return -1;
    uint32_t v=0;
    for(size_t i=0;s[i];++i) { if(s[i]<'0'||s[i]>'9')return -1; if(v>(0xFFFFFFFFu-(uint32_t)(s[i]-'0'))/10u)return -1; v=v*10u+(uint32_t)(s[i]-'0'); }
    *out=v; return 0;
}
/* De-chunk an RFC 7230 transfer body in-place. Returns decoded length, -1 on malformed input. */
static int decode_chunked(char *body,size_t length,size_t *decoded) {
    size_t r=0,w=0;
    while(r<length) {
        size_t line=r; while(line+1<length && !(body[line]=='\r'&&body[line+1]=='\n')) ++line;
        if(line+1>=length || line-r>16u) return -1;
        uint32_t chunk=0; size_t i=r; int digits=0;
        while(i<line && body[i]!=';') {
            char c=body[i++]; unsigned d;
            if(c>='0'&&c<='9')d=(unsigned)(c-'0'); else if(c>='a'&&c<='f')d=(unsigned)(c-'a'+10); else if(c>='A'&&c<='F')d=(unsigned)(c-'A'+10); else return -1;
            if(chunk>(0xFFFFFFFFu-d)/16u) return -1;
            chunk=chunk*16u+d; ++digits;
        }
        if(!digits)return -1;
        r=line+2u;
        if(chunk==0) { *decoded=w; return 0; }
        if((size_t)chunk>length-r || length-r-(size_t)chunk<2u || body[r+chunk]!='\r'||body[r+chunk+1u]!='\n') return -1;
        for(size_t j=0;j<(size_t)chunk;++j) body[w++]=body[r+j];
        r+=(size_t)chunk+2u;
    }
    return -1;
}
static int http_open_impl(const char *url) {
    char host[HTTP_HOST_LIMIT+1u], path[HTTP_PATH_LIMIT+1u]; uint16_t port;
    if(parse_url(url,host,sizeof(host),&port,path,sizeof(path))!=0) {
        print_both("Fluxmax browser: invalid URL. Supported format: http://hostname/path (HTTPS is not supported yet).\n"); return -1;
    }
    if(!net_is_online()) { print_both("Fluxmax browser: network is not configured. Check RTL8139 and DHCP using 'net'.\n"); return -1; }
    fluxmax_ipv4_addr_t address;
    if(fluxmax_ipv4_parse(host,&address)!=0 && fluxmax_net_resolve_ipv4(host,5000u,&address)!=0) {
        print_both("Fluxmax browser: DNS lookup failed. Check 'net' and 'dns'.\n"); return -1;
    }
    char request[1600]; size_t rn=0;
    static const char a[]="GET ", b[]=" HTTP/1.0\r\nHost: ", c[]="\r\nUser-Agent: Fluxmax/0.6\r\nAccept: text/html,text/plain,*/*\r\nConnection: close\r\n\r\n";
#define APPEND_LITERAL(x) do { size_t z=sizeof(x)-1u; if(rn+z>=sizeof(request))return -1; memcpy(request+rn,x,z); rn+=z; } while(0)
    APPEND_LITERAL(a);
    for(size_t i=0;path[i];++i) { if(rn+1>=sizeof(request))return -1; request[rn++]=path[i]; }
    APPEND_LITERAL(b);
    for(size_t i=0;host[i];++i) { if(rn+1>=sizeof(request))return -1; request[rn++]=host[i]; }
    if(port!=80u) { char digits[5];size_t n=0;uint16_t p=port;do{digits[n++]=(char)('0'+p%10u);p/=10u;}while(p&&n<sizeof(digits));if(rn+2+n>=sizeof(request))return -1;request[rn++]=':';while(n)request[rn++]=digits[--n]; }
    APPEND_LITERAL(c);
#undef APPEND_LITERAL
    static uint16_t local_port=49152u; if(++local_port<49152u)local_port=49152u;
    print_both("Fluxmax browser: connecting to "); print_both(host); print_both("...\n");
    if(fluxmax_tcp_connect(address,port,local_port,8000u)!=0) { print_both("Fluxmax browser: TCP connect failed or timed out.\n"); return -1; }
    if(fluxmax_tcp_send(request,rn,5000u)!=(int)rn) {
        int tcp_error=fluxmax_tcp_last_error();
        (void)fluxmax_tcp_close(1000u);
        print_both("Fluxmax browser: request send failed (TCP error ");
        char error_code[16]; size_t en=0; unsigned magnitude=(unsigned)(tcp_error<0?-tcp_error:tcp_error);
        if(tcp_error<0) error_code[en++]='-';
        char digits[10]; size_t dn=0; do { digits[dn++]=(char)('0'+magnitude%10u); magnitude/=10u; } while(magnitude && dn<sizeof(digits));
        while(dn) { error_code[en++]=digits[--dn]; }
        error_code[en]='\0';
        print_both(error_code);
        if(tcp_error==-1) print_both(": local transmit failure");
        else if(tcp_error==-2) print_both(": ACK/retransmit timeout");
        else if(tcp_error==-3) print_both(": peer reset");
        else if(tcp_error==-4) print_both(": send deadline elapsed");
        else print_both(": unspecified transport state");
        print_both(").\n"); return -1;
    }
    char *response=(char *)kmalloc(HTTP_RESPONSE_LIMIT); if(!response) { (void)fluxmax_tcp_close(1000u); print_both("Fluxmax browser: out of memory.\n"); return -1; }
    size_t used=0; int timed_out=0, ended=0;
    while(used<HTTP_RESPONSE_LIMIT) {
        int n=fluxmax_tcp_receive_data(response+used,HTTP_RESPONSE_LIMIT-used,4000u);
        if(n>0) { used+=(size_t)n; continue; }
        if(n==0) { ended=1; break; }
        timed_out=1; break;
    }
    (void)fluxmax_tcp_close(2500u);
    if(!used) { kfree(response); print_both("Fluxmax browser: no HTTP response was received.\n"); return -1; }
    int split=find_bytes(response,used,"\r\n\r\n",4u);
    if(split<0 || (size_t)split>HTTP_HEADER_LIMIT) { kfree(response); print_both("Fluxmax browser: response header is missing or too large.\n"); return -1; }
    size_t header_len=(size_t)split+2u, body_start=(size_t)split+4u, body_len=used-body_start;
    int status=0;
    if(used<12u || response[0]!='H'||response[1]!='T'||response[2]!='T'||response[3]!='P'||response[4]!='/') { kfree(response); print_both("Fluxmax browser: malformed HTTP status line.\n"); return -1; }
    size_t pos=0;while(pos<used && response[pos]!=' ')++pos;if(pos+4u>used){kfree(response);return -1;}
    if(response[pos+1]<'0'||response[pos+1]>'9'||response[pos+2]<'0'||response[pos+2]>'9'||response[pos+3]<'0'||response[pos+3]>'9'){kfree(response);return -1;}
    status=(response[pos+1]-'0')*100+(response[pos+2]-'0')*10+(response[pos+3]-'0');
    if(status<200 || status>=300) {
        print_both("Fluxmax browser: HTTP status "); char code[4]={(char)('0'+status/100),(char)('0'+(status/10)%10),(char)('0'+status%10),'\0'}; print_both(code);
        if(status>=300&&status<400) print_both(" (redirect not followed automatically; HTTPS redirects are unsupported)");
        print_both("\n"); kfree(response); return -1;
    }
    char value[64]; uint32_t content_length=0; int has_length=header_value(response,header_len,"Content-Length",value,sizeof(value));
    if(has_length<0 || (has_length>0 && parse_u32_decimal(value,&content_length)!=0)) { kfree(response); print_both("Fluxmax browser: invalid Content-Length.\n"); return -1; }
    int transfer_len=header_value(response,header_len,"Transfer-Encoding",value,sizeof(value));
    int chunked=0; if(transfer_len>0) {
        for(size_t i=0;i+7u<=(size_t)transfer_len;++i) if(span_eq_ci(value+i,7u,"chunked")) { chunked=1;break; }
    }
    if(has_length>0 && content_length>body_len && (ended || used>=HTTP_RESPONSE_LIMIT)) {
        kfree(response); print_both("Fluxmax browser: truncated HTTP response body.\n"); return -1;
    }
    if(has_length>0 && content_length<body_len) body_len=content_length;
    if(chunked) { size_t decoded=0; if(decode_chunked(response+body_start,body_len,&decoded)!=0) { kfree(response); print_both("Fluxmax browser: malformed chunked response.\n"); return -1; } body_len=decoded; }
    if(timed_out && !(has_length>0 && content_length<=body_len)) { kfree(response); print_both("Fluxmax browser: response timed out before completion.\n"); return -1; }
    if(!ended && body_len==HTTP_RESPONSE_LIMIT-body_start) print_both("Fluxmax browser: response reached the 128 KiB safety limit.\n");
    print_both("\n--- FLUXMAX WEB VIEW: "); print_both(url); print_both(" ---\n\n");
    char content_type[128]; int type_len=header_value(response,header_len,"Content-Type",content_type,sizeof(content_type));
    int html=type_len==0;
    if(type_len>0) for(size_t i=0;i+9u<=(size_t)type_len;++i) if(span_eq_ci(content_type+i,9u,"text/html")) html=1;
    int rc=0;
    if(html) rc=fluxmax_browser_render_html_text(response+body_start,body_len,browser_putc,0);
    else for(size_t i=0;i<body_len;++i) browser_putc(response[body_start+i],0);
    if(body_len && response[body_start+body_len-1u]!='\n') browser_putc('\n',0);
    print_both("\n--- END PAGE ---\n");
    kfree(response);
    return rc;
}

void fluxmax_browser_http_init(void) { browser_set_url_handler(http_open_impl); }

int browser_launch_ui(void) {
    char url[HTTP_HOST_LIMIT+HTTP_PATH_LIMIT+16u]; size_t n=0;
    fluxmax_display_console_clear();
    print_both("FLUXMAX TEXT WEB BROWSER\nEnter an http:// URL (HTTPS is not supported).\nURL: ");
    for(;;) {
        char c=0; if(!ps2_keyboard_read_char(&c)) { if(!serial_try_read_char(&c)) { cpu_pause(); continue; } }
        if(c=='\r') continue;
        if(c=='\n') { browser_putc('\n',0); break; }
        if(c=='\b') { if(n) {--n; browser_putc('\b',0);} continue; }
        if((unsigned char)c<32u || n+1u>=sizeof(url)) continue;
        url[n++]=c; browser_putc(c,0);
    }
    url[n]='\0';
    return browser_open_url(url);
}
