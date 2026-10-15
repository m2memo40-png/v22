#ifndef FLUXMAX_LIB_MATH_TABLES_H
#define FLUXMAX_LIB_MATH_TABLES_H
#include <stdint.h>
typedef struct { int32_t sin_q30; int32_t cos_q30; } fluxmax_sincos_q30_t;
void fluxmax_sincos_turn(uint16_t phase, int32_t *sin_q30, int32_t *cos_q30);
uint16_t fluxmax_srgb_to_linear_u16(uint16_t encoded);
uint16_t fluxmax_atan_ratio_turn_q16(uint16_t ratio);
uint32_t fluxmax_exp2_fraction_q30(uint32_t fraction_q16);
#endif
