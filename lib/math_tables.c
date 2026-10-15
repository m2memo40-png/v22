#include "lib/math_tables.h"
#include <stddef.h>

static const fluxmax_sincos_q30_t sincos_table[65536] = {
#include "generated/sincos_q30_table.inc"
};
static const uint16_t srgb_to_linear_table[65536] = {
#include "generated/srgb_to_linear_u16_table.inc"
};
static const uint16_t atan_ratio_table[65536] = {
#include "generated/atan_ratio_turn_q16_table.inc"
};
static const uint32_t exp2_fraction_table[] = {
#include "generated/exp2_fraction_q30_table.inc"
};
#define FLUXMAX_EXP2_COUNT (sizeof(exp2_fraction_table) / sizeof(exp2_fraction_table[0]))

void fluxmax_sincos_turn(uint16_t phase, int32_t *sin_q30, int32_t *cos_q30) {
    if (sin_q30) *sin_q30 = sincos_table[phase].sin_q30;
    if (cos_q30) *cos_q30 = sincos_table[phase].cos_q30;
}

uint16_t fluxmax_srgb_to_linear_u16(uint16_t encoded) {
    return srgb_to_linear_table[encoded];
}

uint16_t fluxmax_atan_ratio_turn_q16(uint16_t ratio) {
    return atan_ratio_table[ratio];
}

uint32_t fluxmax_exp2_fraction_q30(uint32_t fraction_q16) {
    if (FLUXMAX_EXP2_COUNT == 0) return 0;
    if (fraction_q16 > 65535u) fraction_q16 = 65535u;
    size_t index = ((size_t)fraction_q16 * FLUXMAX_EXP2_COUNT) / 65536u;
    if (index >= FLUXMAX_EXP2_COUNT) index = FLUXMAX_EXP2_COUNT - 1u;
    return exp2_fraction_table[index];
}
