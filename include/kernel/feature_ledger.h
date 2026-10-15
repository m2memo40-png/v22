#ifndef FLUXMAX_FEATURE_LEDGER_H
#define FLUXMAX_FEATURE_LEDGER_H
#include <stddef.h>
#include <stdint.h>
typedef enum { FLUXMAX_PHASE_FOUNDATION=1, FLUXMAX_PHASE_USERMODE, FLUXMAX_PHASE_FILESYSTEM,
    FLUXMAX_PHASE_USERLAND, FLUXMAX_PHASE_DRIVERS, FLUXMAX_PHASE_GUI,
    FLUXMAX_PHASE_NETWORK, FLUXMAX_PHASE_BROWSER, FLUXMAX_PHASE_POLISH } fluxmax_phase_t;
typedef enum { FLUXMAX_FEATURE_PLANNED=0, FLUXMAX_FEATURE_SCAFFOLD=1,
    FLUXMAX_FEATURE_PARTIAL=2, FLUXMAX_FEATURE_IMPLEMENTED=3 } fluxmax_feature_state_t;
typedef struct { uint32_t id; uint8_t phase; uint8_t state; const char *component; const char *contract; } fluxmax_feature_record_t;
extern const fluxmax_feature_record_t fluxmax_feature_ledger[];
extern const size_t fluxmax_feature_ledger_count;
#endif
