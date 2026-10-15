#include "drivers/e1000.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/config.h"
#include "kernel/bootinfo.h"
#include "kernel/log.h"
#include "mm/vmm.h"
#include "arch/x86_64/io.h"
#include "lib/mem.h"
#include <stddef.h>
#include <stdint.h>

/*
 * Minimal legacy Intel 8254x/E1000 driver. It intentionally uses bounded
 * polling rather than NIC interrupts, matching the current single-CPU kernel.
 * Only legacy 16-byte descriptors and standard BAR0 MMIO registers are used.
 */
#define E1000_VENDOR_ID 0x8086u
#define E1000_DEVICE_82540EM 0x100Eu
#define E1000_DEVICE_82545EM 0x100Fu
#define E1000_DEVICE_82543GC 0x1004u
#define E1000_DEVICE_82544EI 0x1008u
#define E1000_DEVICE_82544EI2 0x1009u

#define E1000_REG_CTRL   0x0000u
#define E1000_REG_STATUS 0x0008u
#define E1000_REG_EERD   0x0014u
#define E1000_REG_IMC    0x00D8u
#define E1000_REG_RCTL   0x0100u
#define E1000_REG_TCTL   0x0400u
#define E1000_REG_TIPG   0x0410u
#define E1000_REG_RDBAL  0x2800u
#define E1000_REG_RDBAH  0x2804u
#define E1000_REG_RDLEN  0x2808u
#define E1000_REG_RDH    0x2810u
#define E1000_REG_RDT    0x2818u
#define E1000_REG_TDBAL  0x3800u
#define E1000_REG_TDBAH  0x3804u
#define E1000_REG_TDLEN  0x3808u
#define E1000_REG_TDH    0x3810u
#define E1000_REG_TDT    0x3818u
#define E1000_REG_RAL0   0x5400u
#define E1000_REG_RAH0   0x5404u

#define E1000_CTRL_RST   (1u << 26)
#define E1000_CTRL_ASDE  (1u << 5)
#define E1000_CTRL_SLU   (1u << 6)
#define E1000_STATUS_LU  (1u << 1)
#define E1000_RCTL_EN    (1u << 1)
#define E1000_RCTL_BAM   (1u << 15)
#define E1000_RCTL_SECRC (1u << 26)
#define E1000_TCTL_EN    (1u << 1)
#define E1000_TCTL_PSP   (1u << 3)
#define E1000_TX_CMD_EOP  (1u << 0)
#define E1000_TX_CMD_IFCS (1u << 1)
#define E1000_TX_CMD_RS  (1u << 3)
#define E1000_DESC_DD    (1u << 0)
#define E1000_RX_DESC_EOP (1u << 1)
#define E1000_RAH_AV     (1u << 31)
#define E1000_EERD_START (1u << 0)
#define E1000_EERD_DONE  (1u << 4)
#define E1000_EERD_ADDR_SHIFT 8u
#define E1000_EERD_DATA_SHIFT 16u

#define E1000_RING_COUNT 16u
#define E1000_BUFFER_SIZE 2048u
#define E1000_RX_BUDGET 32u
#define E1000_POLL_LIMIT 1000000u
#define E1000_MMIO_MAP_BASE 0xFFFFC00000000000ULL
#define E1000_MMIO_MAP_SIZE 0x8000u
#define PAGE_SIZE 4096u

typedef struct __attribute__((packed, aligned(16))) {
    uint64_t buffer_address;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} e1000_rx_descriptor_t;

typedef struct __attribute__((packed, aligned(16))) {
    uint64_t buffer_address;
    uint16_t length;
    uint8_t checksum_offset;
    uint8_t command;
    uint8_t status;
    uint8_t checksum_start;
    uint16_t special;
} e1000_tx_descriptor_t;

_Static_assert(sizeof(e1000_rx_descriptor_t) == 16u, "E1000 RX descriptors must be 16 bytes");
_Static_assert(sizeof(e1000_tx_descriptor_t) == 16u, "E1000 TX descriptors must be 16 bytes");

static e1000_rx_descriptor_t rx_ring[E1000_RING_COUNT] __attribute__((aligned(4096)));
static e1000_tx_descriptor_t tx_ring[E1000_RING_COUNT] __attribute__((aligned(4096)));
static uint8_t rx_buffers[E1000_RING_COUNT][E1000_BUFFER_SIZE] __attribute__((aligned(4096)));
static uint8_t tx_buffers[E1000_RING_COUNT][E1000_BUFFER_SIZE] __attribute__((aligned(4096)));
static uint8_t mac[6];
static volatile uint32_t *registers;
static uint16_t rx_head, tx_tail;
static uint64_t rx_frames, tx_frames, rx_dropped;
static int driver_up;
static fluxmax_e1000_rx_fn rx_handler;
static void *rx_context;

typedef struct { int found; uint8_t bus, device, function; uint16_t device_id; } e1000_candidate_t;

static uint32_t pci_read_bridge(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, void *context) {
    return fluxmax_pci_read_config32(bus, device, function, offset, context);
}
static int supported_device(uint16_t id) {
    return id == E1000_DEVICE_82540EM || id == E1000_DEVICE_82545EM ||
           id == E1000_DEVICE_82543GC || id == E1000_DEVICE_82544EI ||
           id == E1000_DEVICE_82544EI2;
}
static int find_e1000(const fluxmax_pci_device_t *device, void *context) {
    e1000_candidate_t *candidate = (e1000_candidate_t *)context;
    if (device->vendor_id == E1000_VENDOR_ID && supported_device(device->device_id)) {
        candidate->found = 1;
        candidate->bus = device->bus;
        candidate->device = device->device;
        candidate->function = device->function;
        candidate->device_id = device->device_id;
        return 1;
    }
    return 0;
}
static uint32_t reg_read(uint32_t offset) {
    return registers[offset / sizeof(uint32_t)];
}
static void reg_write(uint32_t offset, uint32_t value) {
    registers[offset / sizeof(uint32_t)] = value;
    __asm__ volatile("mfence" ::: "memory");
}
static int dma_region(const void *pointer, size_t length, uint64_t *physical_out) {
    uint64_t first = 0, physical = 0;
    uintptr_t start, end, page;
    if (!pointer || !length || !physical_out) return -1;
    start = (uintptr_t)pointer;
    if (start > UINTPTR_MAX - (length - 1u)) return -1;
    end = start + length - 1u;
    if (vmm_translate((uint64_t)start, &first) != 0) return -1;
    page = (start & ~(uintptr_t)(PAGE_SIZE - 1u)) + PAGE_SIZE;
    while (page <= end) {
        if (vmm_translate((uint64_t)page, &physical) != 0 || physical != first + (uint64_t)(page - start)) return -1;
        if (page > UINTPTR_MAX - PAGE_SIZE) return -1;
        page += PAGE_SIZE;
    }
    if (vmm_translate((uint64_t)end, &physical) != 0 || physical != first + (uint64_t)(end - start)) return -1;
    *physical_out = first;
    return 0;
}
static int map_registers(uint64_t physical, volatile uint32_t **out) {
    uint64_t page_phys = physical & ~(uint64_t)(PAGE_SIZE - 1u);
    uint64_t page_offset = physical - page_phys;
    uint64_t span = page_offset + 0x6000u;
    uint64_t pages = (span + PAGE_SIZE - 1u) / PAGE_SIZE;
    uint8_t newly_mapped[E1000_MMIO_MAP_SIZE / PAGE_SIZE] = {0};
    if (!out || !physical || pages == 0 || pages > E1000_MMIO_MAP_SIZE / PAGE_SIZE) return -1;
    for (uint64_t i = 0; i < pages; ++i) {
        uint64_t va = E1000_MMIO_MAP_BASE + i * PAGE_SIZE;
        uint64_t pa = page_phys + i * PAGE_SIZE;
        uint64_t existing = 0;
        int tr = vmm_translate(va, &existing);
        if (tr == 0) {
            if ((existing & ~(uint64_t)(PAGE_SIZE - 1u)) != pa) {
                for (uint64_t j = 0; j < pages; ++j) if (newly_mapped[j]) (void)vmm_unmap(E1000_MMIO_MAP_BASE + j * PAGE_SIZE);
                return -1;
            }
        } else {
            if (vmm_map(va, pa, VMM_WRITABLE | VMM_WRITE_THROUGH | VMM_CACHE_DISABLE) != 0) {
                for (uint64_t j = 0; j < pages; ++j) if (newly_mapped[j]) (void)vmm_unmap(E1000_MMIO_MAP_BASE + j * PAGE_SIZE);
                return -1;
            }
            newly_mapped[i] = 1u;
        }
    }
    *out = (volatile uint32_t *)(uintptr_t)(E1000_MMIO_MAP_BASE + page_offset);
    return 0;
}
static int eeprom_read_word(uint8_t address, uint16_t *value) {
    if (!value) return -1;
    reg_write(E1000_REG_EERD, E1000_EERD_START | ((uint32_t)address << E1000_EERD_ADDR_SHIFT));
    for (uint32_t spin = 0; spin < 100000u; ++spin) {
        uint32_t result = reg_read(E1000_REG_EERD);
        if (result & E1000_EERD_DONE) {
            *value = (uint16_t)(result >> E1000_EERD_DATA_SHIFT);
            return 0;
        }
        cpu_pause();
    }
    return -1;
}
static int valid_mac(const uint8_t address[6]) {
    uint8_t all_zero = 1u, all_ff = 1u;
    for (unsigned i = 0; i < 6u; ++i) {
        if (address[i] != 0u) all_zero = 0u;
        if (address[i] != 0xFFu) all_ff = 0u;
    }
    return !all_zero && !all_ff && !(address[0] & 1u);
}
static int load_mac_address(void) {
    uint32_t low = reg_read(E1000_REG_RAL0), high = reg_read(E1000_REG_RAH0);
    if (high & E1000_RAH_AV) {
        mac[0] = (uint8_t)low;
        mac[1] = (uint8_t)(low >> 8);
        mac[2] = (uint8_t)(low >> 16);
        mac[3] = (uint8_t)(low >> 24);
        mac[4] = (uint8_t)high;
        mac[5] = (uint8_t)(high >> 8);
        if (valid_mac(mac)) return 0;
    }
    uint16_t word0, word1, word2;
    if (eeprom_read_word(0u, &word0) != 0 || eeprom_read_word(1u, &word1) != 0 ||
        eeprom_read_word(2u, &word2) != 0) return -1;
    mac[0] = (uint8_t)word0; mac[1] = (uint8_t)(word0 >> 8);
    mac[2] = (uint8_t)word1; mac[3] = (uint8_t)(word1 >> 8);
    mac[4] = (uint8_t)word2; mac[5] = (uint8_t)(word2 >> 8);
    if (!valid_mac(mac)) return -1;
    reg_write(E1000_REG_RAL0, (uint32_t)word0 | ((uint32_t)word1 << 16));
    reg_write(E1000_REG_RAH0, (uint32_t)word2 | E1000_RAH_AV);
    return 0;
}
static int e1000_init(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t bar_low = fluxmax_pci_read_config32(bus, device, function, 0x10u, 0);
    uint64_t bar_phys;
    uint32_t bar_type;
    uint32_t command;
    uint64_t rx_ring_phys, tx_ring_phys;
    if (bar_low == 0xFFFFFFFFu || (bar_low & 1u)) {
        log_write(LOG_WARN, "e1000", "BAR0 is not an MMIO BAR; driver declined this adapter");
        return -1;
    }
    bar_type = (bar_low >> 1) & 3u;
    if (bar_type == 2u) {
        uint32_t bar_high = fluxmax_pci_read_config32(bus, device, function, 0x14u, 0);
        bar_phys = ((uint64_t)bar_high << 32) | (uint64_t)(bar_low & 0xFFFFFFF0u);
    } else if (bar_type == 0u) {
        bar_phys = (uint64_t)(bar_low & 0xFFFFFFF0u);
    } else {
        log_write(LOG_WARN, "e1000", "Unsupported BAR0 memory type");
        return -1;
    }
    if (!bar_phys) return -1;
    command = fluxmax_pci_read_config32(bus, device, function, 0x04u, 0);
    if (command == 0xFFFFFFFFu ||
        fluxmax_pci_write_config32(bus, device, function, 0x04u,
            (command & 0xFFFF0000u) | ((command & 0xFFFFu) | 0x0006u), 0) != 0) return -1;
    if (map_registers(bar_phys, &registers) != 0) {
        registers = 0;
        log_write(LOG_WARN, "e1000", "Could not map the adapter MMIO registers");
        return -1;
    }
    if (reg_read(E1000_REG_STATUS) == 0xFFFFFFFFu) {
        registers = 0;
        log_write(LOG_WARN, "e1000", "MMIO register read is invalid; driver declined this adapter");
        return -1;
    }
    reg_write(E1000_REG_IMC, 0xFFFFFFFFu); /* Polling path; mask device interrupts. */
    reg_write(E1000_REG_RCTL, 0u);
    reg_write(E1000_REG_TCTL, 0u);
    reg_write(E1000_REG_CTRL, reg_read(E1000_REG_CTRL) | E1000_CTRL_RST);
    uint32_t spins = 0;
    while (reg_read(E1000_REG_CTRL) & E1000_CTRL_RST) {
        if (++spins >= E1000_POLL_LIMIT) {
            registers = 0;
            log_write(LOG_ERROR, "e1000", "Controller reset timed out");
            return -1;
        }
        cpu_pause();
    }
    reg_write(E1000_REG_CTRL, reg_read(E1000_REG_CTRL) | E1000_CTRL_ASDE | E1000_CTRL_SLU);
    if (load_mac_address() != 0) {
        log_write(LOG_ERROR, "e1000", "Could not read a valid unicast MAC address from RAR/EEPROM");
        registers = 0;
        return -1;
    }
    if (dma_region(rx_ring, sizeof(rx_ring), &rx_ring_phys) != 0 ||
        dma_region(tx_ring, sizeof(tx_ring), &tx_ring_phys) != 0 ||
        (rx_ring_phys & 0xFu) || (tx_ring_phys & 0xFu)) {
        log_write(LOG_ERROR, "e1000", "Descriptor rings are not physically contiguous/DMA-addressable");
        registers = 0;
        return -1;
    }
    memset(rx_ring, 0, sizeof(rx_ring));
    memset(tx_ring, 0, sizeof(tx_ring));
    memset(rx_buffers, 0, sizeof(rx_buffers));
    memset(tx_buffers, 0, sizeof(tx_buffers));
    for (uint32_t i = 0; i < E1000_RING_COUNT; ++i) {
        uint64_t rx_phys, tx_phys;
        if (dma_region(rx_buffers[i], E1000_BUFFER_SIZE, &rx_phys) != 0 ||
            dma_region(tx_buffers[i], E1000_BUFFER_SIZE, &tx_phys) != 0) {
            log_write(LOG_ERROR, "e1000", "A packet buffer is not physically contiguous/DMA-addressable");
            registers = 0;
            return -1;
        }
        rx_ring[i].buffer_address = rx_phys;
        tx_ring[i].buffer_address = tx_phys;
        tx_ring[i].status = E1000_DESC_DD;
    }
    reg_write(E1000_REG_RDBAL, (uint32_t)rx_ring_phys);
    reg_write(E1000_REG_RDBAH, (uint32_t)(rx_ring_phys >> 32));
    reg_write(E1000_REG_RDLEN, (uint32_t)sizeof(rx_ring));
    reg_write(E1000_REG_RDH, 0u);
    reg_write(E1000_REG_RDT, E1000_RING_COUNT - 1u);
    reg_write(E1000_REG_TDBAL, (uint32_t)tx_ring_phys);
    reg_write(E1000_REG_TDBAH, (uint32_t)(tx_ring_phys >> 32));
    reg_write(E1000_REG_TDLEN, (uint32_t)sizeof(tx_ring));
    reg_write(E1000_REG_TDH, 0u);
    reg_write(E1000_REG_TDT, 0u);
    reg_write(E1000_REG_TIPG, 0x0060200Au);
    reg_write(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | 0x00040100u);
    reg_write(E1000_REG_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC);
    rx_head = 0;
    tx_tail = 0;
    rx_frames = tx_frames = rx_dropped = 0;
    driver_up = 1;
    log_write(LOG_INFO, "e1000", (reg_read(E1000_REG_STATUS) & E1000_STATUS_LU)
              ? "Intel E1000 ready in polling mode; physical link reports UP"
              : "Intel E1000 ready in polling mode; link is awaiting carrier");
    return 0;
}
int e1000_probe(void) {
    e1000_candidate_t candidate = {0, 0, 0, 0, 0};
    if (driver_up) return 0;
    int rc = fluxmax_pci_scan(pci_read_bridge, find_e1000, &candidate);
    if (!candidate.found) {
        log_write(LOG_INFO, "e1000", "No supported Intel 8254x/E1000 PCI adapter found");
        return -1;
    }
    if (rc != 1 || e1000_init(candidate.bus, candidate.device, candidate.function) != 0) {
        log_write(LOG_WARN, "e1000", "A supported Intel PCI adapter was found but could not be initialized");
        return -1;
    }
    return 0;
}
void e1000_set_rx_handler(fluxmax_e1000_rx_fn handler, void *context) {
    rx_handler = handler;
    rx_context = context;
}
void e1000_poll(void) {
    if (!driver_up) return;
    for (uint32_t budget = 0; budget < E1000_RX_BUDGET; ++budget) {
        uint16_t index = rx_head;
        e1000_rx_descriptor_t *descriptor = &rx_ring[index];
        if (!(descriptor->status & E1000_DESC_DD)) break;
        uint16_t length = descriptor->length;
        uint8_t status = descriptor->status;
        if ((status & E1000_RX_DESC_EOP) && descriptor->errors == 0u &&
            length >= 14u && length <= 1514u) {
            if (rx_handler) rx_handler(rx_buffers[index], length, rx_context);
            ++rx_frames;
        } else {
            ++rx_dropped;
        }
        descriptor->length = 0;
        descriptor->checksum = 0;
        descriptor->errors = 0;
        descriptor->special = 0;
        descriptor->status = 0;
        __asm__ volatile("mfence" ::: "memory");
        reg_write(E1000_REG_RDT, index);
        rx_head = (uint16_t)((index + 1u) % E1000_RING_COUNT);
    }
}
int e1000_send_frame(const uint8_t *frame, size_t length) {
    if (!driver_up || !frame || length < 14u || length > 1514u) return -1;
    uint16_t index = tx_tail;
    e1000_tx_descriptor_t *descriptor = &tx_ring[index];
    uint32_t spins = 0;
    while (!(descriptor->status & E1000_DESC_DD) && ++spins < E1000_POLL_LIMIT) cpu_pause();
    if (!(descriptor->status & E1000_DESC_DD)) return -1;
    size_t send_length = length < 60u ? 60u : length;
    memset(tx_buffers[index], 0, send_length);
    memcpy(tx_buffers[index], frame, length);
    descriptor->length = (uint16_t)send_length;
    descriptor->checksum_offset = 0;
    descriptor->checksum_start = 0;
    descriptor->special = 0;
    descriptor->command = E1000_TX_CMD_EOP | E1000_TX_CMD_IFCS | E1000_TX_CMD_RS;
    descriptor->status = 0;
    __asm__ volatile("mfence" ::: "memory");
    reg_write(E1000_REG_TDT, (uint32_t)((index + 1u) % E1000_RING_COUNT));
    spins = 0;
    while (!(descriptor->status & E1000_DESC_DD) && ++spins < E1000_POLL_LIMIT) cpu_pause();
    tx_tail = (uint16_t)((index + 1u) % E1000_RING_COUNT);
    if (!(descriptor->status & E1000_DESC_DD)) return -1;
    ++tx_frames;
    return 0;
}
int e1000_is_up(void) { return driver_up; }
const uint8_t *e1000_mac_address(void) { return driver_up ? mac : 0; }
uint64_t e1000_rx_count(void) { return rx_frames; }
uint64_t e1000_tx_count(void) { return tx_frames; }
