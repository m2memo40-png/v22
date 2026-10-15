#include "net/ipv4.h"

uint32_t fluxmax_ipv4_to_u32(fluxmax_ipv4_addr_t address) {
    return ((uint32_t)address.octet[0] << 24) |
           ((uint32_t)address.octet[1] << 16) |
           ((uint32_t)address.octet[2] << 8) |
           (uint32_t)address.octet[3];
}

fluxmax_ipv4_addr_t fluxmax_ipv4_from_u32(uint32_t value) {
    fluxmax_ipv4_addr_t result = {{
        (uint8_t)(value >> 24), (uint8_t)(value >> 16),
        (uint8_t)(value >> 8), (uint8_t)value
    }};
    return result;
}

uint16_t fluxmax_ipv4_checksum(const void *bytes, size_t length) {
    const uint8_t *p = (const uint8_t *)bytes;
    uint32_t sum = 0;
    while (length >= 2) {
        sum += ((uint16_t)p[0] << 8) | p[1];
        p += 2;
        length -= 2;
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    if (length) sum += (uint16_t)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)~sum;
}

int fluxmax_ipv4_parse(const char *text, fluxmax_ipv4_addr_t *out) {
    if (!text || !out) return -1;
    fluxmax_ipv4_addr_t result = {{0, 0, 0, 0}};
    unsigned component = 0;
    unsigned digits = 0;
    unsigned value = 0;
    for (const char *p = text; ; ++p) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            if (++digits > 3) return -1;
            value = value * 10u + (unsigned)(c - '0');
            if (value > 255u) return -1;
            continue;
        }
        if ((c == '.' || c == '\0') && digits && component < 4) {
            result.octet[component++] = (uint8_t)value;
            value = 0;
            digits = 0;
            if (c == '\0') break;
            continue;
        }
        return -1;
    }
    if (component != 4) return -1;
    *out = result;
    return 0;
}

int fluxmax_ipv4_prefix_match(fluxmax_ipv4_addr_t a, fluxmax_ipv4_addr_t b, uint8_t prefix) {
    if (prefix > 32) return 0;
    if (prefix == 0) return 1;
    uint32_t mask = prefix == 32 ? 0xFFFFFFFFu : (0xFFFFFFFFu << (32u - prefix));
    return (fluxmax_ipv4_to_u32(a) & mask) == (fluxmax_ipv4_to_u32(b) & mask);
}
