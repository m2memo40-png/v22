#include "fs/fat32_rw.h"
#include "drivers/storage/ata_pio.h"
#include "mm/heap.h"
#include "lib/mem.h"
#include <stddef.h>

#define MBR_PART_OFF 446u
#define MBR_ENTRY_LEN 16u
#define DIR_ENTRY_LEN 32u
#define ATTR_READ_ONLY 0x01u
#define ATTR_DIRECTORY 0x10u
#define ATTR_VOLUME_ID 0x08u
#define ATTR_ARCHIVE 0x20u
#define FAT_EOC_MIN 0x0FFFFFF8u
#define FAT_EOC 0x0FFFFFFFu
#define FAT_BAD 0x0FFFFFF7u
#define FAT_MASK 0x0FFFFFFFu
#define FAT_ENTRIES_PER_SECTOR 128u
#define FAT_MAX_FILE_SIZE (16u * 1024u * 1024u)
#define FAT_MAX_CLUSTER_LIST 32768u

typedef struct {
    uint64_t sector_lba;
    uint16_t offset;
    uint8_t raw[DIR_ENTRY_LEN];
    uint8_t found;
    uint8_t end_marker;
    uint32_t previous_cluster;
} dir_slot_t;
static uint16_t le16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static int read_rel(const fluxmax_fat32_ro_volume_t *v, uint64_t rel, uint8_t sec[512]) {
    const fluxmax_ata_device_t *d = v ? fluxmax_ata_get_device(v->device_index) : 0;
    if (!d || !sec || rel >= v->geometry.total_sectors || v->partition_lba >= d->sector_count ||
        rel > d->sector_count - v->partition_lba - 1u) return -1;
    return fluxmax_ata_read_sector(v->device_index, v->partition_lba + rel, sec);
}
static int write_rel(const fluxmax_fat32_ro_volume_t *v, uint64_t rel, const uint8_t sec[512]) {
    const fluxmax_ata_device_t *d = v ? fluxmax_ata_get_device(v->device_index) : 0;
    if (!d || !sec || rel >= v->geometry.total_sectors || v->partition_lba >= d->sector_count ||
        rel > d->sector_count - v->partition_lba - 1u) return -1;
    return fluxmax_ata_write_sector(v->device_index, v->partition_lba + rel, sec);
}
static int valid_volume(const fluxmax_fat32_ro_volume_t *v) {
    if (!v || v->geometry.bytes_per_sector != 512u || !v->geometry.sectors_per_cluster ||
        (v->geometry.sectors_per_cluster & (v->geometry.sectors_per_cluster - 1u)) ||
        !v->geometry.fat_count || !v->geometry.sectors_per_fat ||
        v->geometry.first_data_sector >= v->geometry.total_sectors) return 0;
    return fluxmax_fat32_cluster_valid(&v->geometry, v->geometry.root_cluster);
}
static int short_name_encode(const char *name, uint8_t out[11]) {
    size_t base=0, ext=0; const char *dot=0;
    if (!name || !*name) return -1;
    for (const char *p=name; *p; ++p) {
        unsigned char c=(unsigned char)*p;
        if (c=='.') { if (dot) return -1; dot=p; }
        else if (!(c>='A'&&c<='Z') && !(c>='a'&&c<='z') && !(c>='0'&&c<='9') &&
                 c!='_' && c!='-' && c!='$' && c!='~') return -1;
    }
    /* Measure components explicitly; this source has no libc dependency. */
    base=0; while (name[base] && name[base]!='.') ++base;
    if (base<1u || base>8u) return -1;
    ext=0; if (dot) { const char *q=dot+1; while (q[ext]) ++ext; if (ext<1u || ext>3u) return -1; }
    memset(out,' ',11);
    for (size_t i=0;i<base;++i) { char c=name[i]; out[i]=(uint8_t)((c>='a'&&c<='z') ? c-('a'-'A') : c); }
    for (size_t i=0;i<ext;++i) { char c=dot[1+i]; out[8+i]=(uint8_t)((c>='a'&&c<='z') ? c-('a'-'A') : c); }
    return 0;
}
static int short_name_matches(const uint8_t raw[32], const uint8_t name[11]) {
    if (raw[0]==0 || raw[0]==0xE5u || raw[11]==0x0Fu || (raw[11]&ATTR_VOLUME_ID)) return 0;
    for (size_t i=0;i<11;++i) {
        uint8_t c=raw[i]; if (i==0 && c==0x05u) c=0xE5u;
        if (c != name[i]) return 0;
    }
    return 1;
}
static uint64_t cluster_rel_lba(const fluxmax_fat32_ro_volume_t *v, uint32_t c) {
    return fluxmax_fat32_cluster_lba(&v->geometry,c);
}
static int fat_get(const fluxmax_fat32_ro_volume_t *v, uint32_t c, uint32_t *out) {
    uint8_t sec[512]; uint64_t byte=(uint64_t)c*4u;
    uint64_t ix=(uint64_t)v->geometry.reserved_sector_count+byte/512u;
    if (!out || byte/512u>=v->geometry.sectors_per_fat || read_rel(v,ix,sec)!=0) return -1;
    *out=le32(sec+(byte%512u))&FAT_MASK; return 0;
}
static int fat_set(const fluxmax_fat32_ro_volume_t *v, uint32_t c, uint32_t value) {
    uint8_t sec[512]; uint64_t byte=(uint64_t)c*4u, ix=byte/512u, off=byte%512u;
    if (ix>=v->geometry.sectors_per_fat) return -1;
    for (uint32_t copy=0;copy<v->geometry.fat_count;++copy) {
        uint64_t rel=(uint64_t)v->geometry.reserved_sector_count+(uint64_t)copy*v->geometry.sectors_per_fat+ix;
        if (read_rel(v,rel,sec)!=0) return -1;
        uint32_t old=le32(sec+off); put32(sec+off,(old&0xF0000000u)|(value&FAT_MASK));
        if (write_rel(v,rel,sec)!=0) return -1;
    }
    return 0;
}
static int cluster_zero(const fluxmax_fat32_ro_volume_t *v, uint32_t c) {
    uint8_t zero[512]={0}; uint64_t first=cluster_rel_lba(v,c);
    if (first==UINT64_MAX) return -1;
    for (uint32_t s=0;s<v->geometry.sectors_per_cluster;++s)
        if (write_rel(v,first+s,zero)!=0) return -1;
    return 0;
}
static int allocate_cluster(const fluxmax_fat32_ro_volume_t *v, uint32_t *out) {
    uint8_t fat[512];
    uint64_t data_sectors=(uint64_t)v->geometry.total_sectors-v->geometry.first_data_sector;
    uint64_t cluster_count=data_sectors/v->geometry.sectors_per_cluster;
    uint64_t max_cluster=cluster_count+1u;
    for (uint64_t fat_sec=0;fat_sec<v->geometry.sectors_per_fat;++fat_sec) {
        if (read_rel(v,(uint64_t)v->geometry.reserved_sector_count+fat_sec,fat)!=0) return -1;
        for (uint32_t i=0;i<FAT_ENTRIES_PER_SECTOR;++i) {
            uint64_t cl=fat_sec*FAT_ENTRIES_PER_SECTOR+i;
            if (cl<2u || cl>max_cluster || (le32(fat+i*4u)&FAT_MASK)!=0) continue;
            uint32_t c=(uint32_t)cl;
            if (!fluxmax_fat32_cluster_valid(&v->geometry,c)) continue;
            if (fat_set(v,c,FAT_EOC)!=0 || cluster_zero(v,c)!=0) {
                (void)fat_set(v,c,0); return -1;
            }
            *out=c; return 0;
        }
    }
    return -1;
}
static int free_chain(const fluxmax_fat32_ro_volume_t *v, uint32_t first) {
    uint64_t max = (uint64_t)v->geometry.total_sectors / v->geometry.sectors_per_cluster + 1u;
    uint32_t c=first;
    for (uint64_t i=0;i<max && c>=2u;++i) {
        uint32_t next;
        if (!fluxmax_fat32_cluster_valid(&v->geometry,c) || fat_get(v,c,&next)!=0) return -1;
        if (fat_set(v,c,0)!=0) return -1;
        if (next>=FAT_EOC_MIN) return 0;
        if (next==FAT_BAD || next<2u || !fluxmax_fat32_cluster_valid(&v->geometry,next)) return -1;
        c=next;
    }
    return -1;
}
static int find_root_slot(const fluxmax_fat32_ro_volume_t *v,const uint8_t alias[11],dir_slot_t *slot) {
    uint8_t sec[512]; uint32_t c=v->geometry.root_cluster, previous=0;
    uint64_t max=(uint64_t)v->geometry.total_sectors/v->geometry.sectors_per_cluster+1u;
    memset(slot,0,sizeof(*slot));
    for(uint64_t chain=0;chain<max;++chain) {
        uint64_t first=cluster_rel_lba(v,c); if(first==UINT64_MAX)return -1;
        for(uint32_t s=0;s<v->geometry.sectors_per_cluster;++s) {
            uint64_t rel=first+s; if(read_rel(v,rel,sec)!=0)return -1;
            for(uint16_t off=0;off<512u;off+=DIR_ENTRY_LEN) {
                uint8_t *raw=sec+off;
                if(raw[0]==0xE5u || raw[0]==0u) {
                    if(!slot->found) { slot->sector_lba=rel; slot->offset=off; memcpy(slot->raw,raw,32); slot->previous_cluster=previous; slot->end_marker=(raw[0]==0u); }
                    if(raw[0]==0u) return 0;
                    continue;
                }
                if(short_name_matches(raw,alias)) {
                    if(raw[11]&ATTR_DIRECTORY)return -2;
                    if(raw[11]&ATTR_READ_ONLY)return -3;
                    slot->sector_lba=rel; slot->offset=off; memcpy(slot->raw,raw,32); slot->found=1; slot->end_marker=0; slot->previous_cluster=previous;
                    return 0;
                }
            }
        }
        uint32_t next; if(fat_get(v,c,&next)!=0)return -1;
        if(next>=FAT_EOC_MIN) { slot->previous_cluster=c; return 0; }
        if(next==FAT_BAD || !fluxmax_fat32_cluster_valid(&v->geometry,next))return -1;
        previous=c; c=next;
    }
    return -1;
}
static int extend_root_for_slot(const fluxmax_fat32_ro_volume_t *v,dir_slot_t *slot) {
    uint32_t new_cluster;
    if(!slot->previous_cluster || allocate_cluster(v,&new_cluster)!=0)return -1;
    if(fat_set(v,slot->previous_cluster,new_cluster)!=0) { (void)fat_set(v,new_cluster,0); return -1; }
    slot->sector_lba=cluster_rel_lba(v,new_cluster); slot->offset=0; slot->previous_cluster=new_cluster; slot->end_marker=1; slot->found=0;
    return slot->sector_lba==UINT64_MAX?-1:0;
}
static int write_payload(const fluxmax_fat32_ro_volume_t *v,const uint32_t *clusters,size_t nclusters,const uint8_t *data,size_t size) {
    uint8_t sec[512]; size_t consumed=0;
    for(size_t i=0;i<nclusters;++i) {
        uint64_t first=cluster_rel_lba(v,clusters[i]); if(first==UINT64_MAX)return -1;
        for(uint32_t s=0;s<v->geometry.sectors_per_cluster;++s) {
            memset(sec,0,sizeof(sec));
            size_t take=size-consumed; if(take>sizeof(sec))take=sizeof(sec);
            if(take && data)memcpy(sec,data+consumed,take);
            if(write_rel(v,first+s,sec)!=0)return -1;
            consumed+=take;
        }
    }
    return consumed==size?0:-1;
}
int fluxmax_fat32_rw_write_root_file(const fluxmax_fat32_ro_volume_t *v,const char *short_name,const void *data,size_t size) {
    uint8_t alias[11],sec[512]; dir_slot_t slot; uint32_t *clusters=0; size_t nclusters=0; uint32_t old_first=0;
    if(!valid_volume(v)||(!data&&size)||size>FAT_MAX_FILE_SIZE||short_name_encode(short_name,alias)!=0)return -1;
    if(find_root_slot(v,alias,&slot)!=0)return -1;
    if(slot.found) old_first=((uint32_t)le16(slot.raw+20)<<16)|le16(slot.raw+26);
    uint64_t cluster_bytes=(uint64_t)v->geometry.sectors_per_cluster*512u;
    nclusters=size?(size_t)(((uint64_t)size+cluster_bytes-1u)/cluster_bytes):0;
    if(nclusters>FAT_MAX_CLUSTER_LIST)return -1;
    if(nclusters) {
        clusters=(uint32_t *)kmalloc(nclusters*sizeof(uint32_t));
        if(!clusters)return -1;
        memset(clusters,0,nclusters*sizeof(uint32_t));
        size_t allocated=0;
        for(;allocated<nclusters;++allocated) {
            if(allocate_cluster(v,&clusters[allocated])!=0) break;
            if(allocated && fat_set(v,clusters[allocated-1],clusters[allocated])!=0) { ++allocated; break; }
        }
        if(allocated!=nclusters) {
            for(size_t i=0;i<allocated;++i)(void)fat_set(v,clusters[i],0);
            kfree(clusters); return -1;
        }
        if(write_payload(v,clusters,nclusters,(const uint8_t *)data,size)!=0) {
            for(size_t i=0;i<nclusters;++i)(void)fat_set(v,clusters[i],0);
            kfree(clusters); return -1;
        }
    }
    /* If the directory is full, append one cluster to the root chain. */
    if(!slot.found && slot.sector_lba==0) {
        if(extend_root_for_slot(v,&slot)!=0) {
            if(clusters){for(size_t i=0;i<nclusters;++i)(void)fat_set(v,clusters[i],0);kfree(clusters);} return -1;
        }
    }
    if(read_rel(v,slot.sector_lba,sec)!=0) goto rollback_new;
    if(!slot.found && sec[slot.offset]!=0 && sec[slot.offset]!=0xE5u) goto rollback_new;
    uint8_t entry[32]; memset(entry,0,sizeof(entry));
    memcpy(entry,alias,11);
    entry[11]=slot.found?(uint8_t)(slot.raw[11]|ATTR_ARCHIVE):ATTR_ARCHIVE;
    uint32_t first_cluster=nclusters?clusters[0]:0;
    put16(entry+20,(uint16_t)(first_cluster>>16)); put16(entry+26,(uint16_t)first_cluster); put32(entry+28,(uint32_t)size);
    memcpy(sec+slot.offset,entry,sizeof(entry));
    if(write_rel(v,slot.sector_lba,sec)!=0)goto rollback_new;
    /* New metadata is visible now, so reclaim the previous data chain. */
    if(fluxmax_ata_flush(v->device_index)!=0) {
        /* Directory metadata and data are already written; report a flush fault. */
        if(clusters) kfree(clusters);
        return -1;
    }
    if(old_first>=2u && old_first!=first_cluster) (void)free_chain(v,old_first);
    if(clusters)kfree(clusters);
    return 0;
rollback_new:
    if(clusters) {
        for(size_t i=0;i<nclusters;++i) (void)fat_set(v,clusters[i],0);
        kfree(clusters);
    }
    return -1;
}
