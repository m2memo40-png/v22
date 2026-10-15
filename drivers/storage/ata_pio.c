#include "drivers/storage/ata_pio.h"
#include "arch/x86_64/io.h"
#include "kernel/log.h"
#include "lib/mem.h"

#define ATA_REG_DATA       0u
#define ATA_REG_ERROR      1u
#define ATA_REG_FEATURES   1u
#define ATA_REG_SECCOUNT0  2u
#define ATA_REG_LBA0       3u
#define ATA_REG_LBA1       4u
#define ATA_REG_LBA2       5u
#define ATA_REG_HDDEVSEL   6u
#define ATA_REG_COMMAND    7u
#define ATA_REG_STATUS     7u
#define ATA_SR_ERR         0x01u
#define ATA_SR_DRQ         0x08u
#define ATA_SR_DF          0x20u
#define ATA_SR_BSY         0x80u
#define ATA_CMD_IDENTIFY   0xECu
#define ATA_CMD_READ_PIO   0x20u
#define ATA_CMD_WRITE_PIO  0x30u
#define ATA_CMD_CACHE_FLUSH 0xE7u
#define ATA_LBA28_LIMIT    0x10000000ULL
#define ATA_POLL_LIMIT     1000000u

static fluxmax_ata_device_t devices[FLUXMAX_ATA_MAX_DEVICES];
static uint32_t device_count;

static void ata_delay_400ns(const fluxmax_ata_device_t *d) {
    (void)inb(d->control_base);
    (void)inb(d->control_base);
    (void)inb(d->control_base);
    (void)inb(d->control_base);
}

static int wait_not_busy(const fluxmax_ata_device_t *d, uint8_t *last_status) {
    for (uint32_t i = 0; i < ATA_POLL_LIMIT; ++i) {
        uint8_t status = inb((uint16_t)(d->io_base + ATA_REG_STATUS));
        if (last_status) *last_status = status;
        if (status == 0x00u || status == 0xFFu) return -1;
        if (!(status & ATA_SR_BSY)) {
            if (status & (ATA_SR_ERR | ATA_SR_DF)) return -2;
            return 0;
        }
        cpu_pause();
    }
    return -3;
}

static int wait_data_request(const fluxmax_ata_device_t *d) {
    for (uint32_t i = 0; i < ATA_POLL_LIMIT; ++i) {
        uint8_t status = inb((uint16_t)(d->io_base + ATA_REG_STATUS));
        if (status == 0x00u || status == 0xFFu) return -1;
        if (status & (ATA_SR_ERR | ATA_SR_DF)) return -2;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0;
        cpu_pause();
    }
    return -3;
}

static int identify_device(fluxmax_ata_device_t *d) {
    uint16_t words[256];
    uint8_t status;
    outb(d->control_base, 0x02u); /* Disable device IRQs; use bounded polling. */
    outb((uint16_t)(d->io_base + ATA_REG_HDDEVSEL), (uint8_t)(0xA0u | (d->slave << 4)));
    ata_delay_400ns(d);
    outb((uint16_t)(d->io_base + ATA_REG_SECCOUNT0), 0);
    outb((uint16_t)(d->io_base + ATA_REG_LBA0), 0);
    outb((uint16_t)(d->io_base + ATA_REG_LBA1), 0);
    outb((uint16_t)(d->io_base + ATA_REG_LBA2), 0);
    outb((uint16_t)(d->io_base + ATA_REG_COMMAND), ATA_CMD_IDENTIFY);
    status = inb((uint16_t)(d->io_base + ATA_REG_STATUS));
    if (status == 0x00u || status == 0xFFu) return -1;
    if (wait_not_busy(d, &status) != 0) return -1;
    if (inb((uint16_t)(d->io_base + ATA_REG_LBA1)) != 0 ||
        inb((uint16_t)(d->io_base + ATA_REG_LBA2)) != 0) return -1; /* ATAPI/other device. */
    if (wait_data_request(d) != 0) return -1;
    for (uint32_t i = 0; i < 256u; ++i) words[i] = inw((uint16_t)(d->io_base + ATA_REG_DATA));
    uint64_t sectors = (uint64_t)words[60] | ((uint64_t)words[61] << 16);
    if (!sectors) return -1;
    if (sectors > ATA_LBA28_LIMIT) sectors = ATA_LBA28_LIMIT;
    d->sector_count = sectors;
    for (uint32_t i = 0; i < 20u; ++i) {
        d->model[i * 2u] = (char)(words[27u + i] >> 8);
        d->model[i * 2u + 1u] = (char)(words[27u + i] & 0xFFu);
    }
    d->model[40] = '\0';
    for (int i = 39; i >= 0 && d->model[i] == ' '; --i) d->model[i] = '\0';
    d->present = 1;
    return 0;
}

void fluxmax_ata_init(void) {
    static const uint16_t io_bases[2] = {0x1F0u, 0x170u};
    static const uint16_t ctrl_bases[2] = {0x3F6u, 0x376u};
    device_count = 0;
    memset(devices, 0, sizeof(devices));
    for (uint32_t channel = 0; channel < 2u; ++channel) {
        for (uint8_t slave = 0; slave < 2u; ++slave) {
            fluxmax_ata_device_t *d = &devices[device_count];
            d->io_base = io_bases[channel];
            d->control_base = ctrl_bases[channel];
            d->slave = slave;
            if (identify_device(d) == 0) {
                ++device_count;
                log_write(LOG_INFO, "ata", "Identified legacy ATA PIO device");
            } else {
                memset(d, 0, sizeof(*d));
            }
        }
    }
}

uint32_t fluxmax_ata_device_count(void) { return device_count; }

const fluxmax_ata_device_t *fluxmax_ata_get_device(uint32_t index) {
    if (index >= device_count || !devices[index].present) return 0;
    return &devices[index];
}

int fluxmax_ata_read_sector(uint32_t index, uint64_t lba, uint8_t sector[FLUXMAX_ATA_SECTOR_SIZE]) {
    const fluxmax_ata_device_t *d = fluxmax_ata_get_device(index);
    if (!d || !sector || lba >= d->sector_count || lba >= ATA_LBA28_LIMIT) return -1;
    outb(d->control_base, 0x02u);
    outb((uint16_t)(d->io_base + ATA_REG_HDDEVSEL),
         (uint8_t)(0xE0u | (d->slave << 4) | ((lba >> 24) & 0x0Fu)));
    ata_delay_400ns(d);
    outb((uint16_t)(d->io_base + ATA_REG_FEATURES), 0);
    outb((uint16_t)(d->io_base + ATA_REG_SECCOUNT0), 1);
    outb((uint16_t)(d->io_base + ATA_REG_LBA0), (uint8_t)lba);
    outb((uint16_t)(d->io_base + ATA_REG_LBA1), (uint8_t)(lba >> 8));
    outb((uint16_t)(d->io_base + ATA_REG_LBA2), (uint8_t)(lba >> 16));
    outb((uint16_t)(d->io_base + ATA_REG_COMMAND), ATA_CMD_READ_PIO);
    if (wait_data_request(d) != 0) return -1;
    for (uint32_t i = 0; i < 256u; ++i) {
        uint16_t word = inw((uint16_t)(d->io_base + ATA_REG_DATA));
        sector[i * 2u] = (uint8_t)word;
        sector[i * 2u + 1u] = (uint8_t)(word >> 8);
    }
    return wait_not_busy(d, 0);
}


int fluxmax_ata_write_sector(uint32_t index, uint64_t lba, const uint8_t sector[FLUXMAX_ATA_SECTOR_SIZE]) {
    const fluxmax_ata_device_t *d = fluxmax_ata_get_device(index);
    if (!d || !sector || lba >= d->sector_count || lba >= ATA_LBA28_LIMIT) return -1;
    outb(d->control_base, 0x02u); /* Disable device IRQs; bounded polling only. */
    if (wait_not_busy(d, 0) != 0) return -1;
    outb((uint16_t)(d->io_base + ATA_REG_HDDEVSEL),
         (uint8_t)(0xE0u | (d->slave << 4) | ((lba >> 24) & 0x0Fu)));
    ata_delay_400ns(d);
    outb((uint16_t)(d->io_base + ATA_REG_FEATURES), 0);
    outb((uint16_t)(d->io_base + ATA_REG_SECCOUNT0), 1);
    outb((uint16_t)(d->io_base + ATA_REG_LBA0), (uint8_t)lba);
    outb((uint16_t)(d->io_base + ATA_REG_LBA1), (uint8_t)(lba >> 8));
    outb((uint16_t)(d->io_base + ATA_REG_LBA2), (uint8_t)(lba >> 16));
    outb((uint16_t)(d->io_base + ATA_REG_COMMAND), ATA_CMD_WRITE_PIO);
    if (wait_data_request(d) != 0) return -1;
    for (uint32_t i = 0; i < 256u; ++i) {
        uint16_t word = (uint16_t)sector[i * 2u] | ((uint16_t)sector[i * 2u + 1u] << 8);
        outw((uint16_t)(d->io_base + ATA_REG_DATA), word);
    }
    return wait_not_busy(d, 0);
}

int fluxmax_ata_flush(uint32_t index) {
    const fluxmax_ata_device_t *d = fluxmax_ata_get_device(index);
    if (!d) return -1;
    if (wait_not_busy(d, 0) != 0) return -1;
    outb((uint16_t)(d->io_base + ATA_REG_COMMAND), ATA_CMD_CACHE_FLUSH);
    return wait_not_busy(d, 0);
}
