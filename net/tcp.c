#include "net/tcp.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/pit.h"
#include "lib/mem.h"
#include <stdint.h>

#define TCP_FIN 0x01u
#define TCP_SYN 0x02u
#define TCP_RST 0x04u
#define TCP_PSH 0x08u
#define TCP_ACK 0x10u
#define TCP_WINDOW 4096u
#define TCP_RETRY_TICKS 500u
#define TCP_MAX_RETRIES 5u
#define TCP_FLAG_DATA_OFFSET 0x50u

typedef enum { TCP_CLOSED=0, TCP_SYN_SENT, TCP_ESTABLISHED, TCP_FIN_WAIT, TCP_FIN_WAIT2, TCP_CLOSE_WAIT,
               TCP_LAST_ACK, TCP_FAILED } tcp_state_t;

typedef struct {
    volatile tcp_state_t state;
    fluxmax_net_stack_t *stack;
    fluxmax_ipv4_addr_t remote_ip;
    uint16_t local_port, remote_port;
    uint32_t snd_una, snd_nxt, rcv_nxt;
    uint32_t pending_seq;
    uint8_t pending_flags, pending_valid, pending_retries;
    size_t pending_length;
    uint8_t pending_data[FLUXMAX_TCP_MSS];
    uint64_t pending_deadline;
    volatile uint8_t remote_fin;
    volatile int error;
    uint8_t rx[FLUXMAX_TCP_RX_CAPACITY];
    volatile uint32_t rx_head, rx_tail;
} tcp_control_block_t;

static tcp_control_block_t tcb;
static uint32_t seq_seed=0x54595255u;

static uint16_t get16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0]<<8)|p[1]); }
static uint32_t get32(const uint8_t *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static uint32_t sequence_cost(uint8_t flags,size_t n) { return (uint32_t)n+((flags&TCP_SYN)?1u:0u)+((flags&TCP_FIN)?1u:0u); }
/* CLOSE_WAIT is a valid TCP half-closed state: the peer has finished sending,
   but our send side remains open until we send our own FIN. A fast HTTP peer
   may ACK the request and deliver its response+FIN before the sender resumes. */
static int tcp_data_path_open(void) {
    return tcb.state==TCP_ESTABLISHED || tcb.state==TCP_CLOSE_WAIT;
}
static uint32_t rx_used(void) { return tcb.rx_head-tcb.rx_tail; }
static uint16_t receive_window(void) { uint32_t free_space=FLUXMAX_TCP_RX_CAPACITY-rx_used(); return (uint16_t)(free_space>65535u?65535u:free_space); }

static int send_segment(uint32_t seq,uint32_t ack,uint8_t flags,const uint8_t *data,size_t length) {
    if(!tcb.stack) return -1;
    return fluxmax_net_stack_send_tcp(tcb.stack,tcb.remote_ip,tcb.local_port,tcb.remote_port,
                                     seq,ack,flags,receive_window(),data,length);
}
static void send_ack(void) { (void)send_segment(tcb.snd_nxt,tcb.rcv_nxt,TCP_ACK,0,0); }
static int send_pending(void) {
    if(!tcb.pending_valid) return 0;
    /* Publish a future retry deadline BEFORE transmitting. The network polling
       task can preempt the foreground shell on this single CPU; a deadline of
       zero/stale from the previous segment must not trigger an overlapping
       retransmission while the first transmit is still in progress. */
    tcb.pending_deadline=pit_ticks()+TCP_RETRY_TICKS;
    int rc=send_segment(tcb.pending_seq,tcb.rcv_nxt,tcb.pending_flags,
                        tcb.pending_length?tcb.pending_data:0,tcb.pending_length);
    if(rc==1) tcb.pending_deadline=pit_ticks()+100u;
    if(rc<0 && tcb.pending_valid) {
        tcb.error=-1; /* local transmit/driver failure */
        tcb.state=TCP_FAILED;
        return -1;
    }
    return rc;
}
static int start_pending(uint8_t flags,const void *payload,size_t length) {
    if(length>FLUXMAX_TCP_MSS || (!payload&&length) || tcb.pending_valid) return -1;
    /* Fill every field observed by fluxmax_tcp_tick() before setting the
       pending_valid publication flag. Previously the poll task could see a
       half-initialized pending segment and retransmit it using a stale timer. */
    tcb.pending_seq=tcb.snd_nxt; tcb.pending_flags=flags; tcb.pending_length=length;
    if(length) memcpy(tcb.pending_data,payload,length);
    tcb.pending_retries=0;
    tcb.snd_nxt+=sequence_cost(flags,length);
    tcb.pending_deadline=pit_ticks()+TCP_RETRY_TICKS;
    tcb.pending_valid=1;
    return send_pending();
}
static void clear_pending_if_acked(uint32_t ack) {
    if(!tcb.pending_valid) return;
    uint32_t end=tcb.pending_seq+sequence_cost(tcb.pending_flags,tcb.pending_length);
    /* This single-flight sender only accepts the exact end of the outstanding segment.
       A future/forged ACK must not discard pending data. */
    if(ack==end) {
        tcb.snd_una=ack; tcb.pending_valid=0; tcb.pending_length=0; tcb.pending_retries=0;
        if(tcb.state==TCP_FIN_WAIT) tcb.state=tcb.remote_fin?TCP_CLOSED:TCP_FIN_WAIT2;
        else if(tcb.state==TCP_LAST_ACK) tcb.state=TCP_CLOSED;
    }
}

void fluxmax_tcp_init(fluxmax_net_stack_t *stack) {
    memset(&tcb,0,sizeof(tcb)); tcb.stack=stack; tcb.state=TCP_CLOSED;
    if(stack) fluxmax_net_stack_set_tcp_handler(stack,fluxmax_tcp_receive,&tcb);
    uint32_t lo,hi; __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
    seq_seed=lo^hi^0x54595255u; if(!seq_seed) seq_seed=0x54595255u;
}

void fluxmax_tcp_tick(uint64_t now) {
    if(!tcb.pending_valid || tcb.state==TCP_CLOSED || tcb.state==TCP_FAILED || now<tcb.pending_deadline) return;
    if(tcb.pending_retries>=TCP_MAX_RETRIES) { tcb.error=-2; tcb.state=TCP_FAILED; tcb.pending_valid=0; return; }
    ++tcb.pending_retries; (void)send_pending();
}

void fluxmax_tcp_receive(fluxmax_ipv4_addr_t source,fluxmax_ipv4_addr_t destination,
                        const uint8_t *segment,size_t length,void *context) {
    (void)context;
    if(!segment || length<20u || tcb.state==TCP_CLOSED || tcb.state==TCP_FAILED) return;
    size_t hdr=(size_t)(segment[12]>>4)*4u;
    if(hdr<20u || hdr>length || get16(segment)!=tcb.remote_port || get16(segment+2)!=tcb.local_port ||
       fluxmax_ipv4_to_u32(source)!=fluxmax_ipv4_to_u32(tcb.remote_ip) ||
       !tcb.stack || fluxmax_ipv4_to_u32(destination)!=fluxmax_ipv4_to_u32(tcb.stack->address)) return;
    uint32_t seq=get32(segment+4), ack=get32(segment+8);
    uint8_t flags=segment[13];
    const uint8_t *payload=segment+hdr; size_t payload_len=length-hdr;
    if(flags&TCP_RST) { tcb.error=-3; tcb.state=TCP_FAILED; tcb.pending_valid=0; return; }

    if(tcb.state==TCP_SYN_SENT) {
        if((flags&(TCP_SYN|TCP_ACK))!=(TCP_SYN|TCP_ACK) || ack!=tcb.snd_nxt) return;
        tcb.snd_una=ack; tcb.pending_valid=0; tcb.pending_length=0;
        tcb.rcv_nxt=seq+1u; tcb.error=0; tcb.state=TCP_ESTABLISHED; send_ack(); return;
    }
    if(flags&TCP_ACK) clear_pending_if_acked(ack);
    if(tcb.state==TCP_FAILED || tcb.state==TCP_CLOSED) return;

    if(payload_len) {
        if(seq==tcb.rcv_nxt && payload_len<=FLUXMAX_TCP_RX_CAPACITY-rx_used()) {
            for(size_t i=0;i<payload_len;++i) tcb.rx[(tcb.rx_head++)%FLUXMAX_TCP_RX_CAPACITY]=payload[i];
            tcb.rcv_nxt+=(uint32_t)payload_len;
        }
        /* ACK current receive edge, also advertising the currently available window. */
        send_ack();
    }
    if(flags&TCP_FIN) {
        uint32_t fin_seq=seq+(uint32_t)payload_len;
        if(fin_seq==tcb.rcv_nxt) { ++tcb.rcv_nxt; tcb.remote_fin=1; if(tcb.state==TCP_ESTABLISHED)tcb.state=TCP_CLOSE_WAIT; else if(tcb.state==TCP_FIN_WAIT2)tcb.state=TCP_CLOSED; }
        send_ack();
    }
}

int fluxmax_tcp_connect(fluxmax_ipv4_addr_t destination,uint16_t remote_port,
                       uint16_t local_port,uint64_t timeout_ticks) {
    if(!tcb.stack || !tcb.stack->configured || !remote_port || !local_port || !timeout_ticks ||
       tcb.state!=TCP_CLOSED) return -1;
    memset(tcb.rx,0,sizeof(tcb.rx)); tcb.rx_head=tcb.rx_tail=0; tcb.remote_fin=0; tcb.error=0;
    tcb.remote_ip=destination; tcb.remote_port=remote_port; tcb.local_port=local_port;
    seq_seed=seq_seed*1664525u+1013904223u;
    tcb.snd_una=seq_seed; tcb.snd_nxt=seq_seed; tcb.rcv_nxt=0; tcb.state=TCP_SYN_SENT;
    if(start_pending(TCP_SYN,0,0)<0) {
        tcb.pending_valid=0; tcb.state=TCP_CLOSED;
        return -1;
    }
    uint64_t start=pit_ticks();
    while(tcb.state==TCP_SYN_SENT && pit_ticks()-start<timeout_ticks) cpu_pause();
    if(tcb.state!=TCP_ESTABLISHED) { tcb.state=TCP_CLOSED; tcb.pending_valid=0; return -1; }
    return 0;
}

int fluxmax_tcp_send(const void *data,size_t length,uint64_t timeout_ticks) {
    if((!data&&length) || !timeout_ticks || !tcp_data_path_open()) return -1;
    const uint8_t *p=(const uint8_t *)data; size_t sent=0;
    while(sent<length) {
        size_t chunk=length-sent; if(chunk>FLUXMAX_TCP_MSS)chunk=FLUXMAX_TCP_MSS;
        uint64_t start=pit_ticks();
        /* A peer FIN transitions ESTABLISHED -> CLOSE_WAIT, but must not turn
           an already ACKed request into a send failure. Continue waiting for
           the outstanding ACK in either writable state. */
        while(tcb.pending_valid && pit_ticks()-start<timeout_ticks && tcp_data_path_open()) cpu_pause();
        if(!tcp_data_path_open() || tcb.pending_valid) {
            if(tcp_data_path_open() && tcb.pending_valid) {
                tcb.error=-4;
                tcb.pending_valid=0;
                tcb.state=TCP_FAILED;
            }
            return -1;
        }
        if(start_pending(TCP_ACK|TCP_PSH,p+sent,chunk)<0) return -1;
        start=pit_ticks();
        while(tcb.pending_valid && pit_ticks()-start<timeout_ticks && tcp_data_path_open()) cpu_pause();
        if(!tcp_data_path_open() || tcb.pending_valid) {
            if(tcp_data_path_open() && tcb.pending_valid) {
                tcb.error=-4; /* caller's ACK deadline elapsed */
                tcb.pending_valid=0;
                tcb.state=TCP_FAILED;
            }
            return -1;
        }
        sent+=chunk;
    }
    return (int)sent;
}

int fluxmax_tcp_receive_data(void *buffer,size_t capacity,uint64_t timeout_ticks) {
    if(!buffer || !capacity || !timeout_ticks) return -1;
    uint64_t start=pit_ticks();
    for(;;) {
        uint32_t used=rx_used();
        if(used) {
            size_t n=used; if(n>capacity)n=capacity;
            uint8_t *out=(uint8_t *)buffer;
            for(size_t i=0;i<n;++i)out[i]=tcb.rx[(tcb.rx_tail++)%FLUXMAX_TCP_RX_CAPACITY];
            return (int)n;
        }
        if(tcb.error || tcb.state==TCP_FAILED) return -1;
        if(tcb.remote_fin || tcb.state==TCP_CLOSED) return 0;
        if(pit_ticks()-start>=timeout_ticks) return -1;
        cpu_pause();
    }
}

int fluxmax_tcp_close(uint64_t timeout_ticks) {
    if(!timeout_ticks) return -1;
    if(tcb.state==TCP_CLOSED) return 0;
    if(tcb.state==TCP_FAILED) { tcb.state=TCP_CLOSED; tcb.pending_valid=0; return -1; }
    uint64_t start=pit_ticks();
    while(tcb.pending_valid && pit_ticks()-start<timeout_ticks) cpu_pause();
    if(tcb.pending_valid) { tcb.state=TCP_CLOSED; tcb.pending_valid=0; return -1; }
    if(tcb.state==TCP_ESTABLISHED || tcb.state==TCP_CLOSE_WAIT) {
        tcb.state=tcb.remote_fin?TCP_LAST_ACK:TCP_FIN_WAIT;
        if(start_pending(TCP_FIN|TCP_ACK,0,0)<0) { tcb.state=TCP_CLOSED; return -1; }
        start=pit_ticks();
        while(tcb.state!=TCP_CLOSED && tcb.state!=TCP_FAILED && pit_ticks()-start<timeout_ticks) cpu_pause();
    }
    int ok=tcb.state==TCP_CLOSED; tcb.state=TCP_CLOSED; tcb.pending_valid=0;
    return ok?0:-1;
}
int fluxmax_tcp_is_connected(void) { return tcp_data_path_open(); }

int fluxmax_tcp_last_error(void) { return tcb.error; }
