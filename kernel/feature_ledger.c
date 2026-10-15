#include "kernel/feature_ledger.h"
/* Status describes project source organization, not a claim of runtime validation. */
const fluxmax_feature_record_t fluxmax_feature_ledger[] = {
#include "generated/feature_ledger.inc"
};
const size_t fluxmax_feature_ledger_count = sizeof(fluxmax_feature_ledger) / sizeof(fluxmax_feature_ledger[0]);
