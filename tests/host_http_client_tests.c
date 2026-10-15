#include "browser/browser.h"
#include "browser/http_client.h"
#include "net/net.h"
#include "net/tcp.h"
#include "kernel/log.h"
#include "mm/heap.h"
#include "gui/console.h"
#include "drivers/keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static unsigned passed, failed, connect_calls, close_calls;
static const char *fake_response;
static size_t fake_response_len, fake_offset;
static char request_seen[2048], output[32768];
static size_t output_len;
static uint16_t connected_port;
static uint32_t connected_ip;
static int fake_send_error, fake_tcp_error;
typedef struct { char *buffer; size_t capacity, length; } output_capture_t;
static void capture_output_char(char ch, void *context) {
    output_capture_t *capture=(output_capture_t *)context;
    if (!capture || !capture->buffer || capture->length + 1u >= capture->capacity) return;
    capture->buffer[capture->length++]=ch; capture->buffer[capture->length]='\0';
}
static void check(const char *name,int ok) { printf("[%s] %s\n",ok?"PASS":"FAIL",name); if(ok)++passed;else ++failed; }
static int contains(const char *hay,const char *needle) { return strstr(hay,needle)!=0; }
static void reset_output(void) { output_len=0; output[0]='\0'; }
static void set_response(const char *response) { fake_response=response; fake_response_len=strlen(response);fake_offset=0; }
int net_is_online(void) { return 1; }
int fluxmax_net_resolve_ipv4(const char *host,uint64_t timeout,fluxmax_ipv4_addr_t *out) {
    (void)timeout; if(!host||!out||strcmp(host,"example.test")!=0)return -1;
    out->octet[0]=203;out->octet[1]=0;out->octet[2]=113;out->octet[3]=5;return 0;
}
int fluxmax_tcp_connect(fluxmax_ipv4_addr_t dst,uint16_t remote,uint16_t local,uint64_t timeout) {
    (void)local;(void)timeout; ++connect_calls;connected_ip=((uint32_t)dst.octet[0]<<24)|((uint32_t)dst.octet[1]<<16)|((uint32_t)dst.octet[2]<<8)|dst.octet[3];connected_port=remote;return 0;
}
int fluxmax_tcp_send(const void *data,size_t n,uint64_t timeout) { (void)timeout;if(fake_send_error)return -1;if(n>=sizeof(request_seen))return -1;memcpy(request_seen,data,n);request_seen[n]='\0';return (int)n; }
int fluxmax_tcp_receive_data(void *buffer,size_t capacity,uint64_t timeout) {
    (void)timeout;if(fake_offset>=fake_response_len)return 0;
    size_t n=fake_response_len-fake_offset;if(n>capacity)n=capacity;if(n>13u)n=13u;
    memcpy(buffer,fake_response+fake_offset,n);fake_offset+=n;return (int)n;
}
int fluxmax_tcp_close(uint64_t timeout) { (void)timeout;++close_calls;return 0; }
int fluxmax_tcp_is_connected(void) { return 1; }
int fluxmax_tcp_last_error(void) { return fake_tcp_error; }
void *kmalloc(size_t n) { return malloc(n); }
void kfree(void *p) { free(p); }
void fluxmax_display_console_putc(char c) { if(output_len+1u<sizeof(output)){output[output_len++]=c;output[output_len]='\0';} }
void fluxmax_display_console_write(const char *s) { if(s)while(*s)fluxmax_display_console_putc(*s++); }
void fluxmax_display_console_clear(void) { reset_output(); }
void serial_write_char(char c) { (void)c; }
void serial_write(const char *s) { (void)s; }
int serial_try_read_char(char *out) { (void)out;return 0; }
int ps2_keyboard_read_char(char *out) { (void)out;return 0; }

int main(void) {
    static const char html_response[]="HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n<!doctype html><html><body><h1>Welcome</h1><p>Hello &amp; world.</p></body></html>";
    static const char chunked_response[]="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n5\r\nHello\r\n7\r\n world!\r\n0\r\n\r\n";
    static const char redirect_response[]="HTTP/1.0 302 Found\r\nLocation: https://example.test/\r\nContent-Length: 0\r\n\r\n";
    static const char truncated_response[]="HTTP/1.0 200 OK\r\nContent-Length: 99\r\nContent-Type: text/plain\r\n\r\nshort";
    fluxmax_browser_http_init();

    reset_output(); set_response(html_response); size_t before=connect_calls;
    int rc=browser_open_url("http://example.test/hello");
    check("HTTP GET can read a response split across small TCP reads",rc==0 && connect_calls==before+1 && contains(output,"Hello & world."));
    check("HTTP request contains path, host and close semantics",contains(request_seen,"GET /hello HTTP/1.0\r\n") && contains(request_seen,"Host: example.test\r\n") && contains(request_seen,"Connection: close\r\n"));
    check("DNS-resolved host connects to the returned IPv4 address and port 80",connected_ip==0xCB007105u && connected_port==80u);

    reset_output(); set_response(chunked_response);
    rc=browser_open_url("http://example.test:8080/chunked");
    check("Chunked HTTP body is decoded before rendering",rc==0 && contains(output,"Hello world!"));
    check("Non-default port is included in Host header and transport tuple",connected_port==8080u && contains(request_seen,"Host: example.test:8080\r\n"));

    before=connect_calls; reset_output();
    rc=browser_open_url("https://example.test/private");
    check("HTTPS is safely refused until TLS and certificate validation exist",rc==-1 && connect_calls==before);
    before=connect_calls; rc=browser_open_url("http:///missing-host");
    check("Malformed HTTP URL is rejected before opening a connection",rc==-1 && connect_calls==before);
    rc=browser_open_url("http://x:0/");
    check("Zero port is rejected as an invalid URL",rc==-1);

    reset_output(); set_response(redirect_response);
    rc=browser_open_url("http://example.test/redirect");
    check("Redirect responses are not silently treated as page content",rc==-1 && contains(output,"HTTP status 302"));

    reset_output(); set_response(truncated_response);
    rc=browser_open_url("http://example.test/truncated");
    check("Truncated Content-Length response is rejected",rc==-1 && contains(output,"truncated HTTP response body"));

    check("TCP close path is requested after each opened HTTP connection",close_calls>=4u);

    char redirected[32768]; output_capture_t capture={redirected,sizeof(redirected),0}; redirected[0]='\0';
    reset_output(); set_response(html_response); browser_set_output(capture_output_char,&capture);
    rc=browser_open_url("http://example.test/window"); browser_set_output(0,0);
    check("browser output can be redirected into a GUI window sink",rc==0 && contains(redirected,"Hello & world.") && contains(redirected,"FLUXMAX WEB VIEW"));
    check("redirected browser output bypasses the text console",output_len==0);

    reset_output(); fake_send_error=1; fake_tcp_error=-2; set_response(html_response);
    rc=browser_open_url("http://example.test/send-failure");
    check("TCP send failure reports its ACK/retransmit cause",rc==-1 && contains(output,"TCP error -2: ACK/retransmit timeout"));
    fake_send_error=0; fake_tcp_error=0;

    printf("\nHTTP client tests: %u passed, %u failed.\n",passed,failed);
    return failed?1:0;
}
