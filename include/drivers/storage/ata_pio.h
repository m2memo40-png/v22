#ifndef FLUXMAX_ATA_PIO_H
#define FLUXMAX_ATA_PIO_H
#include <stdint.h>
#define FLUXMAX_ATA_MAX_DEVICES 4u
#define FLUXMAX_ATA_SECTOR_SIZE 512u

typedef struct fluxmax_ata_device {
    uint16_t io_base;
    uint16_t control_base;
    uint8_t slave;
    uint8_t present;
    uint64_t sector_count; /* Identified LBA28-addressable sector count. */
    char model[41];
} fluxmax_ata_device_t;

/* Legacy ATA PIO only (primary/secondary IDE, master/slave); no AHCI/NVMe. */
void fluxmax_ata_init(void);
uint32_t fluxmax_ata_device_count(void);
const fluxmax_ata_device_t *fluxmax_ata_get_device(uint32_t index);
int fluxmax_ata_read_sector(uint32_t index, uint64_t lba, uint8_t sector[FLUXMAX_ATA_SECTOR_SIZE]);
/* Writes one LBA28 sector. Call flush after a grouped write operation. */
int fluxmax_ata_write_sector(uint32_t index, uint64_t lba, const uint8_t sector[FLUXMAX_ATA_SECTOR_SIZE]);
int fluxmax_ata_flush(uint32_t index);
#endif
