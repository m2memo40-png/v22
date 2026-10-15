#include "net/tcp.h"
#include "arch/x86_64/pit.h"
#include <stdio.h>
#include <string.h>

static fluxmax_net_tcp_fn receive_fn;
static void *receive_ctx;
static fluxmax_net_stack_t *bound_stack;
static fluxmax_ipv4_addr_t local_ip={{192,0,2,10}}, remote_ip={{192,0,2,80}};
static uint16_t local_port=49153, remote_port=80;
static uint32_t server_seq=0x12345678u;
static uint64_t ticks;
static unsigned scheduled_reply;
static uint32_t ack_delta;
static int inject_data, inject_fin, combine_response_fin;
static char last_payload[2048];
static size_t last_payload_len;
static uint8_t last_flags;
static uint32_t last_seq, client_next_ack;
static unsigned send_count;

static void put16(uint8_t *p,uint16_t x){p[0]=(uint8_t)(x>>8);p[1]=(uint8_t)x;}
static void put32(uint8_t *p,uint32_t x){p[0]=(uint8_t)(x>>24);p[1]=(uint8_t)(x>>16);p[2]=(uint8_t)(x>>8);p[3]=(uint8_t)x;}

static void inject_segment(uint8_t flags,uint32_t seq,uint32_t ack,const void *payload,size_t n) {
    uint8_t seg[20+1200];
    if(n>1200 || !receive_fn) return;
    memset(seg,0,20+n); put16(seg,remote_port);put16(seg+2,local_port);put32(seg+4,seq);put32(seg+8,ack);
    seg[12]=0x50;seg[13]=flags;put16(seg+14,4096);
    if(n)memcpy(seg+20,payload,n);
    receive_fn(remote_ip,local_ip,seg,20+n,receive_ctx);
}

uint64_t pit_ticks(void) {
    ++ticks;
    unsigned reply=scheduled_reply; scheduled_reply=0;
    if(reply==1) {
        inject_segment(0x12,server_seq,client_next_ack,0,0); /* SYN-ACK */
        ++server_seq; /* account for SYN sequence-space byte */
    }
    if(reply==2) inject_segment(0x10,server_seq,client_next_ack+ack_delta,0,0);
    if(reply==3) {
        static const char msg[]="fast response";
        inject_segment(0x19,server_seq,client_next_ack,msg,sizeof(msg)-1u); /* ACK|PSH|FIN in one packet */
        server_seq+=(uint32_t)(sizeof(msg)-1u)+1u;
    }
    if(inject_data) {
        inject_data=0; static const char msg[]="server response";
        inject_segment(0x18,server_seq,client_next_ack,msg,sizeof(msg)-1u);
        server_seq+=(uint32_t)(sizeof(msg)-1u);
    }
    if(inject_fin) { inject_fin=0; inject_segment(0x11,server_seq,client_next_ack,0,0); ++server_seq; }
    return ticks;
}

void fluxmax_net_stack_set_tcp_handler(fluxmax_net_stack_t *s,fluxmax_net_tcp_fn fn,void *ctx) {
    bound_stack=s;receive_fn=fn;receive_ctx=ctx;
}
int fluxmax_net_stack_send_tcp(fluxmax_net_stack_t *s,fluxmax_ipv4_addr_t dst,uint16_t sport,uint16_t dport,
                              uint32_t seq,uint32_t ack,uint8_t flags,uint16_t window,const void *payload,size_t n) {
    (void)s;(void)dst;(void)window;
    ++send_count; last_flags=flags;last_seq=seq;last_payload_len=n;
    if(n>=sizeof(last_payload)) return -1;
    if (n) memcpy(last_payload,payload,n);
    last_payload[n]='\0';
    if((flags&0x02u) || ((flags&0x08u) && n) || (flags&0x01u)) {
        client_next_ack=seq+(uint32_t)n+((flags&0x02u)?1u:0u)+((flags&0x01u)?1u:0u);
    }
    if(flags&0x02u) { /* Client SYN: arrange a matching server SYN-ACK. */
        remote_port=dport;local_port=sport; scheduled_reply=1; ack_delta=0;
        (void)ack;
    } else if((flags&0x08u) && n) {
        if(combine_response_fin) { scheduled_reply=3; combine_response_fin=0; }
        else scheduled_reply=2;
    }
    else if(flags&0x01u) { scheduled_reply=2; }
    return 0;
}

/* The normal kernel stack registers the callback via this setter; the host fake
   provides only the TCP transport boundary so this test exercises the TCB state machine. */
static unsigned passed,failed;
#define CHECK(label, condition) do { printf("[%s] %s\n",(condition)?"PASS":"FAIL",(label));if(condition)++passed;else ++failed;} while(0)

int main(void) {
    fluxmax_net_stack_t stack;memset(&stack,0,sizeof(stack));stack.address=local_ip;stack.configured=1;
    fluxmax_tcp_init(&stack);
    /* SYN-ACK is injected on the first PIT tick after SYN transmission. */
    /* Replace the generic injector with the current SYN sequence acknowledged. */
    CHECK("initial TCP state is disconnected",!fluxmax_tcp_is_connected());

    /* Start the handshake; pit_ticks() uses last_seq for exact SYN acknowledgement. */
    /* scheduled_reply=1 was set by the send mock and the first tick will deliver it. */
    int rc=fluxmax_tcp_connect(remote_ip,80,49153,20);
    CHECK("TCP three-way handshake reaches ESTABLISHED",rc==0 && fluxmax_tcp_is_connected());
    if(rc==0) {
        static const char request[]="GET / HTTP/1.0\r\n\r\n";
        rc=fluxmax_tcp_send(request,sizeof(request)-1u,20);
        CHECK("TCP sender transmits and waits for exact ACK of request",rc==(int)(sizeof(request)-1u));
        CHECK("last outbound data segment contains request",(last_flags&0x08u)!=0 && strstr(last_payload,"GET / HTTP/1.0")!=0);
        inject_data=1; char response[64]; int n=fluxmax_tcp_receive_data(response,sizeof(response),20);
        CHECK("TCP receiver buffers in-order remote payload",n==15 && memcmp(response,"server response",15)==0);
        inject_fin=1; n=fluxmax_tcp_receive_data(response,sizeof(response),20);
        CHECK("TCP receiver returns EOF after remote FIN",n==0);
        CHECK("TCP close acknowledges and completes LAST-ACK",fluxmax_tcp_close(20)==0);
    }

    /* Regression: servers can ACK a request and deliver response data+FIN in
       the same segment. That transitions the TCB to CLOSE_WAIT during send().
       The acknowledged request must still be reported as sent successfully. */
    scheduled_reply=0;inject_data=0;inject_fin=0;combine_response_fin=0;
    server_seq=0x33445566u;ack_delta=0;
    fluxmax_tcp_init(&stack);
    rc=fluxmax_tcp_connect(remote_ip,80,49154,20);
    CHECK("fast-response connection establishes",rc==0);
    if(rc==0) {
        static const char fast_request[]="GET / HTTP/1.0\r\n\r\n";
        combine_response_fin=1;
        rc=fluxmax_tcp_send(fast_request,sizeof(fast_request)-1u,20u);
        CHECK("ACK plus response+FIN during send does not falsely fail",rc==(int)(sizeof(fast_request)-1u));
        CHECK("peer FIN leaves readable response in CLOSE_WAIT",fluxmax_tcp_is_connected());
        char fast_response[64];
        int n=fluxmax_tcp_receive_data(fast_response,sizeof(fast_response),20u);
        CHECK("fast response data remains readable after FIN",n==13 && memcmp(fast_response,"fast response",13)==0);
        n=fluxmax_tcp_receive_data(fast_response,sizeof(fast_response),20u);
        CHECK("combined response FIN produces EOF after buffered data",n==0);
        CHECK("close from CLOSE_WAIT completes LAST-ACK",fluxmax_tcp_close(20u)==0);
    }

    /* Reset the static TCB and ensure a future ACK cannot free the current segment. */
    scheduled_reply=0;inject_data=0;inject_fin=0;combine_response_fin=0;server_seq=0x22334455u;ack_delta=9;
    fluxmax_tcp_init(&stack);
    rc=fluxmax_tcp_connect(remote_ip,80,49155,20);
    CHECK("second test connection establishes",rc==0);
    if(rc==0) {
        ack_delta=9;
        rc=fluxmax_tcp_send("X",1u,4u);
        CHECK("future ACK is not accepted as acknowledgement of pending data",rc==-1);
        (void)fluxmax_tcp_close(2u);
    }
    printf("TCP transport tests: %u passed, %u failed.\n",passed,failed);
    return failed?1:0;
}
