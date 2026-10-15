#include "lib/unicode.h"

/* Generated from the host Unicode database for the Basic Multilingual Plane.
 * Code points outside this table are handled conservatively by callers. */
static const fluxmax_unicode_record_t unicode_bmp[] = {
#include "generated/unicode_bmp_table.inc"
};

const fluxmax_unicode_record_t *fluxmax_unicode_lookup(fluxmax_codepoint_t cp) {
    if (cp > 0xFFFFu) return 0;
    return &unicode_bmp[cp];
}

fluxmax_codepoint_t fluxmax_unicode_lower(fluxmax_codepoint_t cp) {
    const fluxmax_unicode_record_t *r = fluxmax_unicode_lookup(cp);
    return r ? r->lower : cp;
}

fluxmax_codepoint_t fluxmax_unicode_upper(fluxmax_codepoint_t cp) {
    const fluxmax_unicode_record_t *r = fluxmax_unicode_lookup(cp);
    return r ? r->upper : cp;
}

int fluxmax_unicode_is_alpha(fluxmax_codepoint_t cp) {
    const fluxmax_unicode_record_t *r = fluxmax_unicode_lookup(cp);
    return r && (r->flags & FLUXMAX_UF_ALPHA) != 0;
}

int fluxmax_unicode_is_decimal(fluxmax_codepoint_t cp) {
    const fluxmax_unicode_record_t *r = fluxmax_unicode_lookup(cp);
    return r && (r->flags & FLUXMAX_UF_DECIMAL) != 0;
}

int fluxmax_unicode_is_space(fluxmax_codepoint_t cp) {
    const fluxmax_unicode_record_t *r = fluxmax_unicode_lookup(cp);
    return r && (r->flags & FLUXMAX_UF_SPACE) != 0;
}

int fluxmax_unicode_is_printable(fluxmax_codepoint_t cp) {
    const fluxmax_unicode_record_t *r = fluxmax_unicode_lookup(cp);
    return r && (r->flags & FLUXMAX_UF_PRINTABLE) != 0;
}
