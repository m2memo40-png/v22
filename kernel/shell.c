#include "kernel/shell.h"
#include "user/elf.h"
#include "browser/browser.h"
#include "mm/heap.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "kernel/bootinfo.h"
#include "kernel/feature_ledger.h"
#include "kernel/selftest.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/pit.h"
#include "arch/x86_64/pic.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "fs/vfs.h"
#include "drivers/keyboard.h"
#include "drivers/pci/pci.h"
#include "drivers/pci/config.h"
#include "drivers/storage/ata_pio.h"
#include "fs/fat32_ro.h"
#include "fs/fat32_rw.h"
#include "gui/console.h"
#include "gui/desktop.h"
#include "sched/scheduler.h"
#include "sched/process.h"
#include "net/ipv4.h"
#include "net/net.h"
#include <stddef.h>
#include <stdint.h>

#define LINE_CAP 256u
static int eq(const char *a,const char *b) { if(!a||!b)return 0; while(*a&&*b&&*a==*b){++a;++b;} return *a==0&&*b==0; }
static size_t str_len_local(const char *s) { size_t n=0; if(s) while(s[n]) ++n; return n; }
static void print_u64(uint64_t n) { char b[24]; size_t i=0; if(!n){serial_write("0");return;} while(n && i<sizeof(b)){b[i++]=(char)('0'+n%10u);n/=10u;} while(i) serial_write_char(b[--i]); }
static char *skip_spaces(char *p) { while(*p==' '||*p=='\t') ++p; return p; }
static char *take_word(char **cursor) { char *p=skip_spaces(*cursor); if(!*p){*cursor=p;return p;} char *start=p; while(*p && *p!=' ' && *p!='\t') ++p; if(*p) *p++='\0'; *cursor=p; return start; }
static int parse_u32_arg(const char *s, uint32_t *out) {
    uint32_t value = 0;
    if (!s || !*s || !out) return -1;
    while (*s) {
        if (*s < '0' || *s > '9') return -1;
        uint32_t digit = (uint32_t)(*s++ - '0');
        if (value > (UINT32_MAX - digit) / 10u) return -1;
        value = value * 10u + digit;
    }
    *out = value;
    return 0;
}
static int fat_list_visit(const char *name, uint8_t attributes, uint32_t size, void *context) {
    (void)context;
    serial_write((attributes & 0x10u) ? "[DIR]  " : "[FILE] ");
    serial_write(name);
    if (!(attributes & 0x10u)) { serial_write("  "); print_u64(size); serial_write(" bytes"); }
    serial_write("\n");
    return 0;
}
static int fat_stream_sink(const uint8_t *data, size_t length, void *context) {
    (void)context;
    for (size_t i = 0; i < length; ++i) {
        if (data[i] == '\n') serial_write("\n");
        else serial_write_char((char)data[i]);
    }
    return 0;
}

typedef struct fatget_context { vfs_node_t *node; uint64_t offset; int failed; } fatget_context_t;
static int fatget_sink(const uint8_t *data, size_t length, void *opaque) {
    fatget_context_t *ctx=(fatget_context_t *)opaque;
    if (!ctx || !ctx->node || ctx->failed) return -1;
    int64_t written=vfs_write(ctx->node,ctx->offset,data,length);
    if (written!=(int64_t)length) { ctx->failed=1; return -1; }
    ctx->offset+=(uint64_t)length;
    return 0;
}

static void browser_output_char(char ch, void *context) { (void)context; serial_write_char(ch); }

static void browser_preview_file(const char *path) {
    if (!path || !*path) { serial_write("usage: browser /path/to/page.html OR browser http://host/path (plain HTTP only; HTTPS is unsupported)\n"); return; }
    if (path[0] != '/') { serial_write("browser: use an absolute VFS path; import disk files with fatget first\n"); return; }
    vfs_node_t *node=vfs_lookup(path);
    if (!node || node->type!=VFS_NODE_FILE) { serial_write("browser: local file not found\n"); return; }
    if (node->size>512u*1024u) { serial_write("browser: local preview is limited to 512 KiB per document\n"); return; }
    char *html=(char *)kmalloc((size_t)node->size+1u);
    if (!html) { serial_write("browser: not enough memory\n"); return; }
    size_t total=0;
    while(total<(size_t)node->size) {
        int64_t n=vfs_read(node,total,html+total,(size_t)node->size-total);
        if(n<=0) break;
        total+=(size_t)n;
    }
    if(total!=(size_t)node->size) { kfree(html); serial_write("browser: file read failed\n"); return; }
    html[total]=0;
    serial_write("Fluxmax HTML preview — local file only\n--------------------------------------\n");
    if(fluxmax_browser_render_html_text(html,total,browser_output_char,0)!=0) serial_write("\nbrowser: malformed document or renderer error\n");
    serial_write("\n--------------------------------------\nHTTPS/TLS, JavaScript, CSS layout and external images are not implemented.\n");
    kfree(html);
}

static void show_help(void) {
    serial_write("Commands: help, about, clear, mem, uptime, tasks, features, selftest, desktop, ls [path], stat PATH, cat PATH, touch PATH, mkdir PATH, write PATH TEXT, append PATH TEXT, rm PATH, disks, fatls [DEVICE], fatcat DEVICE NAME.EXT, fatget DEVICE NAME.EXT /path, fatput DEVICE NAME.EXT /path, pci, net, internet, netconfig dhcp | IP MASK GATEWAY, dns SERVER_IP, resolve HOSTNAME, ping IPv4, browser /path.html | browser http://host/path, web, run PATH, reboot, halt\n");
}
static int list_node(const vfs_node_t *n,void *ctx) {
    (void)ctx; serial_write(n->type==VFS_NODE_DIRECTORY?"[DIR]  ":"[FILE] ");
    serial_write(n->name); if(n->type==VFS_NODE_FILE){serial_write("  ");print_u64(n->size);serial_write(" bytes");} serial_write("\n"); return 0;
}
static int pci_visit(const fluxmax_pci_device_t *d,void *ctx) {
    uint32_t *seen=(uint32_t *)ctx;
    if(*seen>=64u) return 1;
    serial_write("PCI ");print_u64(d->bus);serial_write(":");print_u64(d->device);serial_write(".");print_u64(d->function);
    serial_write(" vendor=");serial_write_hex(d->vendor_id);serial_write(" device=");serial_write_hex(d->device_id);
    serial_write(" class=");serial_write_hex(d->class_code);serial_write("/");serial_write_hex(d->subclass);serial_write("\n"); ++*seen; return 0;
}
/* Real PCI configuration mechanism #1 reader is in drivers/pci/config.c. */
static uint32_t pci_reader_bridge(uint8_t b,uint8_t d,uint8_t f,uint8_t o,void *ctx) { return fluxmax_pci_read_config32(b,d,f,o,ctx); }

static void run_user_program(const char *path) {
    if (!path || !*path) { serial_write("usage: run /absolute/path\n"); return; }
    vfs_node_t *node = vfs_lookup(path);
    if (!node || node->type != VFS_NODE_FILE || node->size < 64u || node->size > 1024u * 1024u) {
        serial_write("run: expected an ELF file between 64 bytes and 1 MiB\n"); return;
    }
    uint8_t *file = (uint8_t *)kmalloc((size_t)node->size);
    if (!file) { serial_write("run: out of kernel heap memory\n"); return; }
    size_t total = 0;
    while (total < node->size) {
        int64_t got = vfs_read(node, total, file + total, (size_t)(node->size - total));
        if (got <= 0) break;
        total += (size_t)got;
    }
    if (total != node->size) {
        kfree(file); serial_write("run: could not read complete executable\n"); return;
    }
    fluxmax_user_image_t image;
    int load_result = fluxmax_elf_load(file, total, &image);
    kfree(file);
    if (load_result != 0) {
        serial_write("run: invalid/unsupported ELF64 executable or insufficient memory (static ET_EXEC only)\n");
        return;
    }
    process_t *p = process_spawn_user(path, image.space, image.entry, image.stack_pointer, 16u);
    if (!p) {
        fluxmax_user_image_destroy(&image);
        serial_write("run: could not schedule user process\n"); return;
    }
    serial_write("Starting Ring-3 process PID "); print_u64(p->pid); serial_write(" from "); serial_write(path); serial_write("\n");
    /* This kernel-thread blocks on HLT while the preemptive scheduler runs the
     * user task. It resumes after SYS_EXIT or a handled user-mode exception. */
    while (p->state != PROC_ZOMBIE) cpu_hlt();
    serial_write("Process PID "); print_u64(p->pid); serial_write(" exited with status ");
    print_u64((uint64_t)(int64_t)p->exit_status); serial_write("\n");
    fluxmax_user_image_destroy(&image);
    if (process_reap(p) != 0) serial_write("run: warning: process reap failed\n");
}

static void execute(char *line) {
    char *cursor=line, *cmd=take_word(&cursor);
    if(!*cmd) return;
    if(eq(cmd,"help")){show_help();return;}
    if(eq(cmd,"about")||eq(cmd,"uname")) { serial_write("Fluxmax / x86_64 / Limine boot protocol / independent freestanding kernel\n"); serial_write("Kernel shell and RAM filesystem are experimental; not all planned subsystems are complete.\n"); return; }
    if(eq(cmd,"clear")){fluxmax_display_console_clear();serial_write("Fluxmax console cleared.\n");return;}
    if(eq(cmd,"mem")){serial_write("Physical free pages: ");print_u64(pmm_free_pages());serial_write(" (4 KiB each)\nKernel heap used: ");print_u64(heap_bytes_used());serial_write(" / ");print_u64(heap_capacity());serial_write(" bytes\n");return;}
    if(eq(cmd,"uptime")){serial_write("Timer ticks at 1000 Hz: ");print_u64(pit_ticks());serial_write("\n");return;}
    if(eq(cmd,"tasks")){serial_write("Scheduler tasks: ");print_u64(fluxmax_sched_count());serial_write("; process records: ");print_u64(process_count());serial_write("\n");return;}
    if(eq(cmd,"features")){
        uint64_t counts[4]={0,0,0,0};
        for(size_t i=0;i<fluxmax_feature_ledger_count;++i){uint8_t s=fluxmax_feature_ledger[i].state;if(s<4u)++counts[s];}
        serial_write("Feature ledger (source status; not runtime proof):\n");
        serial_write("  planned: ");print_u64(counts[0]);serial_write("\n  scaffold: ");print_u64(counts[1]);
        serial_write("\n  partial: ");print_u64(counts[2]);serial_write("\n  implemented: ");print_u64(counts[3]);serial_write("\n  total: ");print_u64(fluxmax_feature_ledger_count);serial_write("\n");return;
    }
    if(eq(cmd,"selftest")){uint32_t failures=fluxmax_selftest_run();if(failures)serial_write("Self-test detected failures; inspect each [SELFTEST FAIL] line above.\n");else serial_write("Self-test checks passed.\n");return;}
    if(eq(cmd,"stat")){
        char *path=take_word(&cursor);vfs_node_t*n=*path?vfs_lookup(path):0;
        if(!n){serial_write("stat: path not found\n");return;}
        serial_write("Name: ");serial_write(n->name);serial_write("\nType: ");
        if(n->type==VFS_NODE_DIRECTORY){uint64_t children=0;for(vfs_node_t*c=n->first_child;c;c=c->next_sibling)++children;serial_write("directory\nChildren: ");print_u64(children);serial_write("\n");}
        else{serial_write("file\nSize: ");print_u64(n->size);serial_write(" bytes\nCapacity: ");print_u64(n->capacity);serial_write(" bytes\n");}
        return;
    }
    if(eq(cmd,"ls")){char *path=take_word(&cursor);if(!*path)path="/";vfs_node_t *n=vfs_lookup(path);if(!n)serial_write("ls: path not found\n");else if(n->type!=VFS_NODE_DIRECTORY)serial_write("ls: not a directory\n");else if(vfs_list(n,list_node,0)!=0)serial_write("ls: listing failed\n");return;}
    if(eq(cmd,"cat")){char *path=take_word(&cursor);vfs_node_t*n=vfs_lookup(path);if(!*path||!n){serial_write("cat: file not found\n");return;}if(n->type!=VFS_NODE_FILE){serial_write("cat: not a file\n");return;}char b[128];uint64_t off=0;for(;;){int64_t r=vfs_read(n,off,b,sizeof(b));if(r<=0)break;for(int64_t i=0;i<r;++i)serial_write_char(b[i]);off+=(uint64_t)r;}serial_write("\n");return;}
    if(eq(cmd,"touch")){char *path=take_word(&cursor);if(!*path||!vfs_create(path,VFS_NODE_FILE))serial_write("touch: failed (use a new absolute path with an existing parent)\n");else serial_write("created\n");return;}
    if(eq(cmd,"mkdir")){char *path=take_word(&cursor);if(!*path||vfs_mkdir(path)!=0)serial_write("mkdir: failed (use a new absolute path with an existing parent)\n");else serial_write("directory created\n");return;}
    if(eq(cmd,"write")){char *path=take_word(&cursor);char *body=skip_spaces(cursor);vfs_node_t*n=vfs_lookup(path);if(!*path||!*body||!n||n->type!=VFS_NODE_FILE)serial_write("write: use write /path text (create the file with touch first)\n");else {int64_t r=vfs_write(n,0,body,str_len_local(body));if(r<0)serial_write("write: failed\n");else serial_write("written\n");}return;}
    if(eq(cmd,"append")){char *path=take_word(&cursor);char *body=skip_spaces(cursor);vfs_node_t*n=vfs_lookup(path);if(!*path||!*body||!n||n->type!=VFS_NODE_FILE)serial_write("append: use append /path text (create the file with touch first)\n");else {int64_t r=vfs_write(n,n->size,body,str_len_local(body));if(r<0)serial_write("append: failed\n");else serial_write("appended\n");}return;}
    if(eq(cmd,"rm")){char *path=take_word(&cursor);if(vfs_unlink(path)!=0)serial_write("rm: failed (directory must be empty)\n");else serial_write("removed\n");return;}
    if(eq(cmd,"disks")) {
        uint32_t count=fluxmax_ata_device_count();
        serial_write("Legacy ATA PIO devices: "); print_u64(count); serial_write("\n");
        for(uint32_t i=0;i<count;++i) {
            const fluxmax_ata_device_t *d=fluxmax_ata_get_device(i);
            serial_write("disk "); print_u64(i); serial_write(": ");
            serial_write(d && d->model[0] ? d->model : "ATA device");
            serial_write("; sectors="); print_u64(d ? d->sector_count : 0); serial_write("\n");
        }
        if(!count) serial_write("No legacy IDE device found. AHCI/NVMe/USB storage is not supported yet.\n");
        return;
    }
    if(eq(cmd,"fatls")) {
        char *arg=take_word(&cursor); uint32_t device=0; fluxmax_fat32_ro_volume_t volume;
        if(*arg && parse_u32_arg(arg,&device)!=0) { serial_write("usage: fatls [DEVICE]\n"); return; }
        if(fluxmax_fat32_ro_mount(&volume,device)!=0) { serial_write("fatls: no readable FAT32 volume found on that ATA device\n"); return; }
        serial_write("FAT32 root directory (read-only, 8.3 names):\n");
        if(fluxmax_fat32_ro_list_root(&volume,fat_list_visit,0)!=0) serial_write("fatls: directory read failed or FAT chain is damaged\n");
        return;
    }
    if(eq(cmd,"fatcat")) {
        char *arg=take_word(&cursor); char *name=take_word(&cursor); uint32_t device=0; fluxmax_fat32_ro_volume_t volume;
        if(parse_u32_arg(arg,&device)!=0 || !*name) { serial_write("usage: fatcat DEVICE NAME.EXT (root directory, 8.3 filename)\n"); return; }
        if(fluxmax_fat32_ro_mount(&volume,device)!=0) { serial_write("fatcat: no readable FAT32 volume found\n"); return; }
        if(fluxmax_fat32_ro_stream_root_file(&volume,name,fat_stream_sink,0)!=0) serial_write("\nfatcat: file not found or read failed\n");
        else serial_write("\n");
        return;
    }
    if(eq(cmd,"fatget")) {
        char *arg=take_word(&cursor); char *name=take_word(&cursor); char *path=take_word(&cursor); uint32_t device=0; fluxmax_fat32_ro_volume_t volume;
        if(parse_u32_arg(arg,&device)!=0 || !*name || !*path || path[0]!='/') { serial_write("usage: fatget DEVICE NAME.EXT /vfs/path\n"); return; }
        if(fluxmax_fat32_ro_mount(&volume,device)!=0) { serial_write("fatget: no readable FAT32 volume found\n"); return; }
        if(vfs_lookup(path)) { serial_write("fatget: destination already exists\n"); return; }
        vfs_node_t *dest=vfs_create(path,VFS_NODE_FILE);
        if(!dest) { serial_write("fatget: could not create destination (parent must exist)\n"); return; }
        fatget_context_t ctx={dest,0,0};
        if(fluxmax_fat32_ro_stream_root_file(&volume,name,fatget_sink,&ctx)!=0 || ctx.failed) {
            (void)vfs_unlink(path); serial_write("fatget: read/import failed; partial file removed\n"); return;
        }
        serial_write("Imported "); serial_write(name); serial_write(" to "); serial_write(path); serial_write(" ( "); print_u64(ctx.offset); serial_write(" bytes )\n"); return;
    }
    if(eq(cmd,"fatput")) {
        char *arg=take_word(&cursor); char *name=take_word(&cursor); char *path=take_word(&cursor); uint32_t device=0; fluxmax_fat32_ro_volume_t volume;
        if(parse_u32_arg(arg,&device)!=0 || !*name || !*path) { serial_write("usage: fatput DEVICE NAME.EXT /vfs/path\n"); return; }
        if(fluxmax_fat32_ro_mount(&volume,device)!=0) { serial_write("fatput: no readable FAT32 volume found\n"); return; }
        vfs_node_t *src=vfs_lookup(path);
        if(!src || src->type!=VFS_NODE_FILE) { serial_write("fatput: source must be a RAM-VFS file\n"); return; }
        serial_write("WARNING: writing persistent data to disk "); print_u64(device); serial_write(" FAT32 root as "); serial_write(name); serial_write("\n");
        if(fluxmax_fat32_rw_write_root_file(&volume,name,src->data,(size_t)src->size)!=0) { serial_write("fatput: write failed; inspect the disk before retrying\n"); return; }
        serial_write("fatput: file written and cache flush completed\n"); return;
    }
    if(eq(cmd,"pci")){uint32_t seen=0;int r=fluxmax_pci_scan(pci_reader_bridge,pci_visit,&seen);if(r!=0)serial_write("PCI scan stopped early.\n");if(!seen)serial_write("No PCI functions reported (or configuration mechanism unavailable).\n");return;}
    if(eq(cmd,"net")){
        fluxmax_net_status_t st; fluxmax_net_get_status(&st);
        serial_write("Network adapter: "); serial_write(st.nic_present?st.driver_name:"not present / not supported"); serial_write("\n");
        if(st.nic_present) {
            serial_write("MAC: "); for(unsigned i=0;i<6u;++i){serial_write_hex(st.mac[i]);if(i!=5u)serial_write(":");} serial_write("\n");
            serial_write("IPv4: "); if(st.ipv4_configured){for(unsigned i=0;i<4u;++i){print_u64(st.address.octet[i]);if(i!=3u)serial_write(".");} serial_write("\n");}
            else serial_write("DHCP discovery pending or static configuration missing\n");
            if(st.ipv4_configured) {
                serial_write("Netmask: "); for(unsigned i=0;i<4u;++i){print_u64(st.netmask.octet[i]);if(i!=3u)serial_write(".");} serial_write("\nGateway: ");
                for(unsigned i=0;i<4u;++i){print_u64(st.gateway.octet[i]);if(i!=3u)serial_write(".");} serial_write("\nDNS: ");
                for(unsigned i=0;i<4u;++i){print_u64(st.dns_server.octet[i]);if(i!=3u)serial_write(".");} serial_write("\n");
            }
            serial_write("NIC RX/TX: "); print_u64(st.nic_rx); serial_write("/"); print_u64(st.nic_tx);
            serial_write("; parsed RX/TX: "); print_u64(st.stack_rx); serial_write("/"); print_u64(st.stack_tx);
            serial_write("; dropped: "); print_u64(st.dropped); serial_write("\n");
        }
        serial_write("Protocols: Intel E1000/Realtek RTL8139 polling, Ethernet, ARP, IPv4, ICMP, UDP, DHCP, DNS A lookup, initial TCP client for plain HTTP.\n");
        serial_write("Limitations: no TLS/HTTPS, Wi-Fi, IPv6, general sockets or modern full browser. IPv4 configuration alone does not prove public Internet access.\n");
        return;
    }
    if(eq(cmd,"netconfig")) {
        char *ip=take_word(&cursor), *mask=take_word(&cursor), *gateway=take_word(&cursor);
        if(eq(ip,"dhcp")) {
            if(fluxmax_net_start_dhcp()==0) serial_write("DHCP discovery restarted; use net to inspect the lease.\n");
            else serial_write("DHCP unavailable: no supported network adapter initialized.\n");
            return;
        }
        if(!*ip || !*mask || !*gateway || fluxmax_net_configure(ip,mask,gateway)!=0) {
            serial_write("usage: netconfig IPv4 NETMASK GATEWAY  (requires an initialized E1000/RTL8139 NIC; QEMU user network example: 10.0.2.15 255.255.255.0 10.0.2.2)\n");
        } else serial_write("Static IPv4 configured. Test reachability with ping; values must match your network.\n");
        return;
    }
    if(eq(cmd,"dns")) {
        char *server=take_word(&cursor);
        if(!*server || fluxmax_net_set_dns(server)!=0) serial_write("usage: dns SERVER_IPV4 (use net to see the DHCP-provided DNS server)\n");
        else { serial_write("DNS server set to "); serial_write(server); serial_write("\n"); }
        return;
    }
    if(eq(cmd,"resolve")) {
        char *host=take_word(&cursor); fluxmax_ipv4_addr_t address;
        if(!*host) { serial_write("usage: resolve hostname.example\n"); return; }
        serial_write("Resolving "); serial_write(host); serial_write("...\n");
        int rc=fluxmax_net_resolve_ipv4(host,5000u,&address);
        if(rc==0) { serial_write("DNS A record: "); for(unsigned i=0;i<4u;++i){print_u64(address.octet[i]);if(i!=3u)serial_write(".");} serial_write("\n"); }
        else serial_write("DNS lookup failed or no A record was returned. Check net, gateway, and DNS server.\n");
        return;
    }
    if(eq(cmd,"internet")) {
        fluxmax_net_status_t status;
        fluxmax_net_get_status(&status);
        if(!status.nic_present) { serial_write("INTERNET TEST: FAIL - no supported network adapter.\n"); return; }
        if(!status.ipv4_configured) { serial_write("INTERNET TEST: WAIT/FAIL - no IPv4 lease. Run 'netconfig dhcp' and then 'net'.\n"); return; }
        serial_write("Adapter and IPv4 configuration are present; testing gateway and DNS...\n");
        if(fluxmax_net_ping_address(status.gateway,3000u)==0) serial_write("Gateway: PASS (ICMP reply received).\n");
        else serial_write("Gateway: NO ICMP REPLY (firewall may block ping; test DNS too).\n");
        fluxmax_ipv4_addr_t address;
        if(fluxmax_net_resolve_ipv4("example.com",5000u,&address)==0) {
            serial_write("DNS/public network path: PASS - example.com resolved to ");
            for(unsigned i=0;i<4u;++i){print_u64(address.octet[i]);if(i!=3u)serial_write(".");}
            serial_write("\n");
            serial_write("This does not test HTTPS. The built-in browser supports plain HTTP only.\n");
        } else {
            serial_write("DNS/public network path: FAIL - no DNS A response; inspect gateway/DNS and NIC RX/TX using 'net'.\n");
        }
        return;
    }
    if(eq(cmd,"ping")) {
        char *arg=take_word(&cursor); fluxmax_ipv4_addr_t address;
        if(!*arg || fluxmax_ipv4_parse(arg,&address)!=0) { serial_write("usage: ping IPv4_ADDRESS\n"); return; }
        serial_write("PING "); serial_write(arg); serial_write(": waiting up to 3 seconds...\n");
        int rc=fluxmax_net_ping_address(address,3000u);
        if(rc==0) { serial_write("ICMP echo reply received.\n"); }
        else serial_write("No ICMP echo reply. Check NIC, netconfig, gateway, and firewall.\n");
        return;
    }
    if(eq(cmd,"browser")){
        char *arg=take_word(&cursor);
        if(arg[0]=='/') browser_preview_file(arg);
        else {
            int rc=browser_open_url(arg);
            if(rc==-1) serial_write("browser: request failed, network unavailable, malformed URL, HTTP server error, or unsupported scheme (HTTPS/TLS is not implemented).\n");
        }
        return;
    }
    if(eq(cmd,"desktop") || eq(cmd,"web")) { desktop_run(); return; }
    if(eq(cmd,"run")){char *path=take_word(&cursor);run_user_program(path);return;}
    if(eq(cmd,"reboot")){serial_write("Rebooting...\n");for(unsigned i=0;i<100000u&&(inb(0x64)&2u);++i){}outb(0x64,0xFE);for(;;)cpu_hlt();}
    if(eq(cmd,"halt")){serial_write("CPU halted. Reset the virtual machine to continue.\n");cpu_cli();for(;;)cpu_hlt();}
    serial_write("Unknown command: ");serial_write(cmd);serial_write(" (type help)\n");
}

void fluxmax_shell_task(void *argument) {
    (void)argument;
    char line[LINE_CAP]; size_t length=0;
    /* The boot desktop owns PS/2 input and the framebuffer until the user presses q. */
    while(fluxmax_desktop_is_active()) cpu_hlt();
    serial_write("\nFluxmax interactive kernel shell\nType help for shell commands. Input works from PS/2 keyboard or COM1 serial.\n");
    for(;;) {
        while(fluxmax_desktop_is_active()) cpu_hlt();
        serial_write("fluxmax> "); length=0;
        for(;;) {
            if(fluxmax_desktop_is_active()) { cpu_hlt(); continue; }
            char c; int got=ps2_keyboard_read_char(&c);
            if(!got) got=serial_try_read_char(&c);
            if(!got) { cpu_hlt(); continue; }
            if(c=='\r') c='\n';
            if(c=='\n') { serial_write("\n"); line[length]='\0'; break; }
            if(c=='\b' || c==0x7F) { if(length){--length;serial_write_char('\b');} continue; }
            if((unsigned char)c>=32u && (unsigned char)c<127u && length+1u<sizeof(line)) { line[length++]=c;serial_write_char(c); }
        }
        execute(line);
    }
}
