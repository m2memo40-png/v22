# Network API headers

`ipv4.h` declares address/checksum helpers; `stack.h` declares Ethernet/ARP/IPv4/ICMP/UDP and initial TCP packet processing; `dns.h` declares bounded DNS A-query building/parsing; `tcp.h` exposes the single-connection TCP client used by the plain-HTTP browser; `net.h` provides DHCP/DNS/ping shell services.

The TCP client is not a BSD-style socket API and does not implement comprehensive congestion control or robust recovery. TLS/HTTPS, IPv6, Wi-Fi, AHCI/NVMe/USB and broad multi-NIC support are not implemented. Host tests do not substitute for QEMU/physical runtime checks.
