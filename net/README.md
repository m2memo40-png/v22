# Fluxmax network stack (development source)

The active driver targets PCI RTL8139 in polling/DMA mode. The stack implements Ethernet II, ARP, IPv4 validation/checksums (fragments rejected), ICMP echo and checked UDP, with an initial DHCP client and DNS A resolver. `net/tcp.c` adds a single outbound TCP connection for the plain HTTP text browser: basic handshake, sequential send, a fixed receive ring and bounded retries. It is not a full socket API and does not implement comprehensive congestion control/recovery.

Shell commands: `net`, `netconfig dhcp`, `netconfig IPv4 MASK GATEWAY`, `dns SERVER_IPV4`, `resolve hostname`, `ping IPv4`, `browser http://host/path`, and `web`.

The web client is plain HTTP only, uses a bounded 128 KiB response buffer and supports basic Content-Length/chunked response framing. It renders HTML as text. No TLS/HTTPS or certificate validation exists, so do not enter credentials or sensitive data. Modern browser features such as JavaScript, CSS layout, images, cookies, redirect following and comprehensive HTTP support are absent.

Host tests cover packet construction/parsing, TCP state transitions with a simulated peer, and HTTP handling with a mocked transport. They do not prove physical DMA or live network behavior. GitHub Actions now has a QEMU integration check using an RTL8139 NIC and local HTTP fixture; inspect a successful run before claiming the running path works.

Not implemented: AHCI/NVMe/USB/Wi-Fi, IPv6, firewall/NAT/multi-NIC routing, general sockets, congestion control, TLS, modern browser engine. `net_is_online()` means only that a supported NIC initialized and IPv4 is configured; it does not certify reachability.
