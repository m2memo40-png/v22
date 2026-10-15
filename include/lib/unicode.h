#ifndef FLUXMAX_LIB_UNICODE_H
#define FLUXMAX_LIB_UNICODE_H
#include <stddef.h>
#include <stdint.h>

typedef uint32_t fluxmax_codepoint_t;
enum fluxmax_unicode_category {
    FLUXMAX_UC_CN, FLUXMAX_UC_LU, FLUXMAX_UC_LL, FLUXMAX_UC_LT,
    FLUXMAX_UC_LM, FLUXMAX_UC_LO, FLUXMAX_UC_MN, FLUXMAX_UC_MC,
    FLUXMAX_UC_ME, FLUXMAX_UC_ND, FLUXMAX_UC_NL, FLUXMAX_UC_NO,
    FLUXMAX_UC_PC, FLUXMAX_UC_PD, FLUXMAX_UC_PS, FLUXMAX_UC_PE,
    FLUXMAX_UC_PI, FLUXMAX_UC_PF, FLUXMAX_UC_PO, FLUXMAX_UC_SM,
    FLUXMAX_UC_SC, FLUXMAX_UC_SK, FLUXMAX_UC_SO, FLUXMAX_UC_ZS,
    FLUXMAX_UC_ZL, FLUXMAX_UC_ZP, FLUXMAX_UC_CC, FLUXMAX_UC_CF,
    FLUXMAX_UC_CS, FLUXMAX_UC_CO
};
enum fluxmax_unicode_flags {
    FLUXMAX_UF_ALPHA = 1u << 0,
    FLUXMAX_UF_DECIMAL = 1u << 1,
    FLUXMAX_UF_SPACE = 1u << 2,
    FLUXMAX_UF_PRINTABLE = 1u << 3,
    FLUXMAX_UF_UPPER = 1u << 4,
    FLUXMAX_UF_LOWER = 1u << 5
};
typedef struct {
    uint32_t codepoint;
    uint32_t lower;
    uint32_t upper;
    uint8_t category;
    uint8_t combining_class;
    uint16_t flags;
} fluxmax_unicode_record_t;
const fluxmax_unicode_record_t *fluxmax_unicode_lookup(fluxmax_codepoint_t cp);
fluxmax_codepoint_t fluxmax_unicode_lower(fluxmax_codepoint_t cp);
fluxmax_codepoint_t fluxmax_unicode_upper(fluxmax_codepoint_t cp);
int fluxmax_unicode_is_alpha(fluxmax_codepoint_t cp);
int fluxmax_unicode_is_decimal(fluxmax_codepoint_t cp);
int fluxmax_unicode_is_space(fluxmax_codepoint_t cp);
int fluxmax_unicode_is_printable(fluxmax_codepoint_t cp);
#endif
