#include "drivers/rtl8139.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/config.h"
#include "arch/x86_64/io.h"
#include "kernel/log.h"
#include "mm/vmm.h"
#include "lib/mem.h"
#include <stddef.h>
#include <stdint.h>

#define RTL_VENDOR 0x10ECu
#define RTL_DEVICE 0x8139u
#define RX_RING_SIZE 8192u
#define RX_ALLOC_SIZE (RX_RING_SIZE + 16u + 1536u)
#define TX_BUFFER_SIZE 1536u
#define TX_SLOTS 4u
#define RX_BUDGET 32u
#define POLL_LIMIT 1000000u
#define REG_IDR0 0x00u
#define REG_TSD0 0x10u
#define REG_TSAD0 0x20u
#define REG_RBSTART 0x30u
#define REG_CMD 0x37u
#define REG_CAPR 0x38u
#define REG_IMR 0x3Cu
#define REG_ISR 0x3Eu
#define REG_TCR 0x40u
#define REG_RCR 0x44u
#define REG_CONFIG1 0x52u
#define CMD_BUFE 0x01u
#define CMD_TE 0x04u
#define CMD_RE 0x08u
#define CMD_RESET 0x10u
#define TX_TOK 0x8000u
#define TX_TUN 0x4000u

static uint8_t rx_ring[RX_ALLOC_SIZE] __attribute__((aligned(4096)));
static uint8_t tx_buffers[TX_SLOTS][TX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t rx_frame[TX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t mac[6];
static uint16_t io_base;
static uint32_t rx_offset;
static uint32_t tx_slot;
static uint64_t rx_frames, tx_frames, rx_dropped;
static int driver_up;
static fluxmax_rtl8139_rx_fn rx_handler;
static void *rx_context;

typedef struct { int found; uint8_t bus, device, function; } rtl_candidate_t;
static int find_rtl(const fluxmax_pci_device_t *d, void *context) {
    rtl_candidate_t *c=(rtl_candidate_t *)context;
    if(d->vendor_id==RTL_VENDOR && d->device_id==RTL_DEVICE) {
        c->found=1; c->bus=d->bus; c->device=d->device; c->function=d->function; return 1;
    }
    return 0;
}
static uint32_t pci_read_bridge(uint8_t b,uint8_t d,uint8_t f,uint8_t o,void *ctx) {
    return fluxmax_pci_read_config32(b,d,f,o,ctx);
}
static int dma_address(const void *ptr, uint32_t *out) {
    uint64_t pa=0;
    if(!ptr || !out || vmm_translate((uint64_t)(uintptr_t)ptr,&pa)!=0 || pa>0xFFFFFFFFULL) return -1;
    /* The PCI RTL8139 only accepts 32-bit physical DMA addresses. */
    *out=(uint32_t)pa; return 0;
}
static int dma_region_contiguous(const void *ptr, size_t length, uint32_t *out) {
    uint32_t first; uint64_t pa;
    if(!length || dma_address(ptr,&first)!=0) return -1;
    uintptr_t start=(uintptr_t)ptr, end=start+length-1u;
    if(end<start) return -1;
    uintptr_t page=(start & ~(uintptr_t)0xFFFu)+0x1000u;
    while(page<=end) {
        if(vmm_translate((uint64_t)page,&pa)!=0 || pa>0xFFFFFFFFULL || pa!=(uint64_t)first+(page-start)) return -1;
        if(page>UINTPTR_MAX-0x1000u) return -1;
        page+=0x1000u;
    }
    if(vmm_translate((uint64_t)end,&pa)!=0 || pa>0xFFFFFFFFULL || pa!=(uint64_t)first+(end-start)) return -1;
    *out=first; return 0;
}
int rtl8139_init(uint8_t bus,uint8_t device,uint8_t function) {
    if(driver_up) return 0;
    uint32_t bar=fluxmax_pci_read_config32(bus,device,function,0x10u,0);
    if(bar==0xFFFFFFFFu || !(bar&1u)) { log_write(LOG_WARN,"rtl8139","Device BAR0 is not an I/O-port BAR; driver declined initialization"); return -1; }
    uint32_t ports=bar & ~3u;
    if(!ports || ports>0xFFFFu-0x60u) return -1;
    io_base=(uint16_t)ports;
    uint32_t command=fluxmax_pci_read_config32(bus,device,function,0x04u,0);
    command=(command & 0xFFFFu) | 0x0005u; /* Enable I/O space and PCI bus mastering. */
    if(fluxmax_pci_write_config32(bus,device,function,0x04u,command,0)!=0) return -1;
    outb((uint16_t)(io_base+REG_CONFIG1),0x00u); /* Power up. */
    outb((uint16_t)(io_base+REG_CMD),CMD_RESET);
    uint32_t spins=0;
    while((inb((uint16_t)(io_base+REG_CMD))&CMD_RESET) && ++spins<POLL_LIMIT) cpu_pause();
    if(spins>=POLL_LIMIT) { log_write(LOG_ERROR,"rtl8139","Controller reset timed out"); return -1; }
    uint32_t rb_phys;
    if(dma_region_contiguous(rx_ring,sizeof(rx_ring),&rb_phys)!=0 || (rb_phys&0xFFu)) {
        log_write(LOG_ERROR,"rtl8139","Receive ring is not physically contiguous, aligned, or DMA-addressable"); return -1;
    }
    for(uint32_t i=0;i<TX_SLOTS;++i) {
        uint32_t tx_phys;
        if(dma_region_contiguous(tx_buffers[i],sizeof(tx_buffers[i]),&tx_phys)!=0 || (tx_phys&0xFu)) {
            log_write(LOG_ERROR,"rtl8139","Transmit buffer is not DMA-addressable"); return -1;
        }
        outl((uint16_t)(io_base+REG_TSAD0+i*4u),tx_phys);
    }
    for(uint32_t i=0;i<6u;++i) mac[i]=inb((uint16_t)(io_base+REG_IDR0+i));
    memset(rx_ring,0,sizeof(rx_ring)); memset(tx_buffers,0,sizeof(tx_buffers));
    outl((uint16_t)(io_base+REG_RBSTART),rb_phys);
    outw((uint16_t)(io_base+REG_CAPR),0xFFF0u);
    outw((uint16_t)(io_base+REG_IMR),0u); /* Use bounded polling, no IRQ dependency. */
    outw((uint16_t)(io_base+REG_ISR),0xFFFFu);
    outl((uint16_t)(io_base+REG_TCR),0x03000700u);
    outl((uint16_t)(io_base+REG_RCR),0x0000008Fu); /* 8 KiB ring, wrap, accept broadcast/multicast/physical. */
    outb((uint16_t)(io_base+REG_CMD),CMD_RE|CMD_TE);
    rx_offset=0; tx_slot=0; rx_frames=tx_frames=rx_dropped=0; driver_up=1;
    log_write(LOG_INFO,"rtl8139","RTL8139 ready in polling mode (8 KiB RX ring)");
    return 0;
}
int rtl8139_probe(void) {
    rtl_candidate_t c={0,0,0,0};
    int rc=fluxmax_pci_scan(pci_read_bridge,find_rtl,&c);
    if(!c.found) { log_write(LOG_INFO,"rtl8139","No RTL8139 PCI network adapter found"); return -1; }
    return rc==1 ? rtl8139_init(c.bus,c.device,c.function) : -1;
}
void rtl8139_set_rx_handler(fluxmax_rtl8139_rx_fn handler,void *context) { rx_handler=handler; rx_context=context; }
void rtl8139_poll(void) {
    if(!driver_up) return;
    for(uint32_t budget=0;budget<RX_BUDGET;++budget) {
        if(inb((uint16_t)(io_base+REG_CMD))&CMD_BUFE) break;
        uint32_t off=rx_offset & (RX_RING_SIZE-1u);
        uint16_t status=(uint16_t)rx_ring[off] | ((uint16_t)rx_ring[off+1u]<<8);
        uint16_t length=(uint16_t)rx_ring[off+2u] | ((uint16_t)rx_ring[off+3u]<<8);
        if(length<18u || length>1518u || !(status&1u)) {
            ++rx_dropped;
            /* Reset receiver cursor to avoid looping forever on a corrupt header. */
            rx_offset=0; outb((uint16_t)(io_base+REG_CMD),CMD_RE|CMD_TE); outw((uint16_t)(io_base+REG_CAPR),0xFFF0u); break;
        }
        size_t frame_length=(size_t)length-4u; /* hardware includes four FCS bytes */
        for(size_t i=0;i<frame_length;++i) rx_frame[i]=rx_ring[(off+4u+(uint32_t)i)&(RX_RING_SIZE-1u)];
        if(rx_handler) rx_handler(rx_frame,frame_length,rx_context);
        ++rx_frames;
        rx_offset=(rx_offset+4u+(uint32_t)length+3u)&~3u;
        rx_offset &= RX_RING_SIZE-1u;
        outw((uint16_t)(io_base+REG_CAPR),(uint16_t)(rx_offset-16u));
    }
}
int rtl8139_send_frame(const uint8_t *frame,size_t length) {
    if(!driver_up || !frame || length<14u || length>1514u) return -1;
    uint32_t phys;
    uint8_t *buf=tx_buffers[tx_slot];
    memset(buf,0,sizeof(tx_buffers[0])); memcpy(buf,frame,length);
    size_t send_len=length<60u?60u:length;
    if(dma_region_contiguous(buf,send_len,&phys)!=0) return -1;
    outl((uint16_t)(io_base+REG_TSAD0+tx_slot*4u),phys);
    outl((uint16_t)(io_base+REG_TSD0+tx_slot*4u),(uint32_t)send_len);
    uint32_t spins=0, status=0;
    do { status=inl((uint16_t)(io_base+REG_TSD0+tx_slot*4u)); if(status&(TX_TOK|TX_TUN)) break; cpu_pause(); } while(++spins<POLL_LIMIT);
    tx_slot=(tx_slot+1u)%TX_SLOTS;
    if(spins>=POLL_LIMIT || !(status&TX_TOK)) return -1;
    ++tx_frames; return 0;
}
int rtl8139_is_up(void) { return driver_up; }
const uint8_t *rtl8139_mac_address(void) { return driver_up?mac:0; }
uint64_t rtl8139_rx_count(void) { return rx_frames; }
uint64_t rtl8139_tx_count(void) { return tx_frames; }
