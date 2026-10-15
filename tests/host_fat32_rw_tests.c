#include "fs/fat32_rw.h"
#include "fs/fat32_ro.h"
#include "drivers/storage/ata_pio.h"
#include "mm/heap.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISK_SECTORS 70000u
static uint8_t *disk_bytes;
static unsigned passed, failed;
static const fluxmax_ata_device_t fake_device = { .present=1, .sector_count=DISK_SECTORS };
void *kmalloc(size_t size) { return size ? malloc(size) : NULL; }
void kfree(void *p) { free(p); }
uint32_t fluxmax_ata_device_count(void) { return 1; }
const fluxmax_ata_device_t *fluxmax_ata_get_device(uint32_t i) { return i==0 ? &fake_device : NULL; }
int fluxmax_ata_read_sector(uint32_t i,uint64_t lba,uint8_t sector[512]) {
    if(i!=0||!sector||lba>=DISK_SECTORS) return -1;
    memcpy(sector,disk_bytes+lba*512u,512);
    return 0;
}
int fluxmax_ata_write_sector(uint32_t i,uint64_t lba,const uint8_t sector[512]) {
    if(i!=0||!sector||lba>=DISK_SECTORS) return -1;
    memcpy(disk_bytes+lba*512u,sector,512);
    return 0;
}
int fluxmax_ata_flush(uint32_t i) { return i==0 ? 0 : -1; }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static void put32(uint8_t *p,uint32_t v) { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24); }
static uint32_t fat_get(uint32_t cluster) { return get32(disk_bytes+(32u*512u)+(uint64_t)cluster*4u)&0x0FFFFFFFu; }
static void check(const char *name,int ok) { printf("[%s] %s\n",ok?"PASS":"FAIL",name); if(ok)++passed;else ++failed; }
typedef struct { uint8_t *buffer; size_t capacity, used; int overflow; } capture_t;
static int capture(const uint8_t *data,size_t length,void *opaque) {
    capture_t *c=(capture_t *)opaque; if(length>c->capacity-c->used){c->overflow=1;return -1;}
    memcpy(c->buffer+c->used,data,length); c->used+=length; return 0;
}
static int list_count(const char *name,uint8_t attr,uint32_t size,void *ctx) {
    (void)name;(void)attr;(void)size; ++*(unsigned *)ctx; return 0;
}
int main(void) {
    disk_bytes=(uint8_t *)calloc(DISK_SECTORS,512u);
    if(!disk_bytes){fprintf(stderr,"test disk allocation failed\n");return 2;}
    /* FAT32 test volume: reserved=32 sectors, two 600-sector FATs, root cluster=2. */
    fluxmax_fat32_ro_volume_t v={0};
    v.device_index=0; v.partition_lba=0;
    v.geometry.bytes_per_sector=512; v.geometry.sectors_per_cluster=1;
    v.geometry.reserved_sector_count=32; v.geometry.fat_count=2;
    v.geometry.sectors_per_fat=600; v.geometry.total_sectors=DISK_SECTORS;
    v.geometry.root_cluster=2; v.geometry.first_data_sector=1232;
    const uint64_t fat_copy_lba[2]={32u,632u};
    for(unsigned f=0;f<2;++f){uint8_t *fat=disk_bytes+fat_copy_lba[f]*512u;put32(fat+0,0x0FFFFFF8u);put32(fat+4,0x0FFFFFFFu);put32(fat+8,0x0FFFFFFFu);}
    uint8_t readback[4096]; capture_t cap={readback,sizeof(readback),0,0};
    check("write and read a new persistent root file",fluxmax_fat32_rw_write_root_file(&v,"HELLO.TXT","hello",5)==0&&
          fluxmax_fat32_ro_stream_root_file(&v,"hello.txt",capture,&cap)==0&&cap.used==5&&!memcmp(readback,"hello",5));
    uint8_t big[1000];for(size_t i=0;i<sizeof(big);++i)big[i]=(uint8_t)(i*13u+7u);
    cap.used=0;cap.overflow=0;
    int overwrite_ok=fluxmax_fat32_rw_write_root_file(&v,"HELLO.TXT",big,sizeof(big))==0&&
          fluxmax_fat32_ro_stream_root_file(&v,"HELLO.TXT",capture,&cap)==0&&cap.used==sizeof(big)&&!cap.overflow&&!memcmp(readback,big,sizeof(big));
    check("replace file using a multi-cluster chain",overwrite_ok);
    check("old data cluster reclaimed after replace",fat_get(3)==0);
    unsigned listed=0;
    check("root directory exposes persisted directory entries",fluxmax_fat32_ro_list_root(&v,list_count,&listed)==0&&listed==1);
    check("empty persistent file is supported",fluxmax_fat32_rw_write_root_file(&v,"EMPTY.DAT",NULL,0)==0&&
          fluxmax_fat32_ro_stream_root_file(&v,"EMPTY.DAT",capture,&cap)==0&&cap.used==sizeof(big));
    check("reject invalid long filename and paths",fluxmax_fat32_rw_write_root_file(&v,"bad-name-too-long.txt","x",1)!=0&&
          fluxmax_fat32_rw_write_root_file(&v,"../BAD.TXT","x",1)!=0);
    check("reject data larger than configured writer limit",fluxmax_fat32_rw_write_root_file(&v,"BIG.BIN",big,16u*1024u*1024u+1u)!=0);
    printf("FAT32 writer tests: %u passed, %u failed.\n",passed,failed);
    free(disk_bytes);
    return failed?1:0;
}
