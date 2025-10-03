#ifndef CORESIDENCY_H
#define CORESIDENCY_H

#include <stdint.h>
#include <stdbool.h>
#include "timing.h"

typedef struct {
    int attacker_cpu;
    int victim_cpu;
    bool llc_shared;
    bool ht_siblings;
    uint64_t avg_hit_latency;
    uint64_t avg_miss_latency;
    double confidence;
} coresidency_result_t;

bool coresidency_verify_llc(int cpu1, int cpu2, timing_calibration_t *cal,
                             cooresidency_result_t *result);

bool coresidency_scan_and_verify(timing_calibration_t *cal,
                                  cooresidency_result_t *result);

#endif
