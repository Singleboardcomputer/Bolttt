#include "timing.h"
#include <stdio.h>
#include <stdlib.h>

#define CALIBRATION_ROUNDS 10000

void timing_calibrate(timing_calibration_t *cal) {
    uint64_t measurements[CALIBRATION_ROUNDS];
    uint64_t sum = 0;

    printf("[*] Calibrating timing overhead...\n");

    for (int i = 0; i < CALIBRATION_ROUNDS; i++) {
        measurements[i] = timing_measure();
        sum += measurements[i];
    }

    cal->overhead = sum / CALIBRATION_ROUNDS;

    printf("[+] Timing overhead: %lu cycles\n", cal->overhead);

    volatile uint64_t *cache_test = malloc(4096);
    uint64_t hit_measurements[1000];
    uint64_t miss_measurements[1000];

    for (int i = 0; i < 1000; i++) {
        uint64_t start = timing_start();
        volatile uint64_t dummy = *cache_test;
        (void)dummy;
        uint64_t end = timing_end();
        hit_measurements[i] = end - start - cal->overhead;
    }

    for (int i = 0; i < 1000; i++) {
        _mm_clflush((void*)cache_test);
        _mm_mfence();
        uint64_t start = timing_start();
        volatile uint64_t dummy = *cache_test;
        (void)dummy;
        uint64_t end = timing_end();
        miss_measurements[i] = end - start - cal->overhead;
    }

    uint64_t hit_sum = 0, miss_sum = 0;
    for (int i = 0; i < 1000; i++) {
        hit_sum += hit_measurements[i];
        miss_sum += miss_measurements[i];
    }

    cal->cache_hit_threshold = (hit_sum / 1000) + 20;
    cal->cache_miss_threshold = (miss_sum / 1000) - 20;

    printf("[+] Cache hit threshold: %lu cycles\n", cal->cache_hit_threshold);
    printf("[+] Cache miss threshold: %lu cycles\n", cal->cache_miss_threshold);

    free((void*)cache_test);
}

uint64_t timing_measure_corrected(timing_calibration_t *cal) {
    uint64_t raw = timing_measure();
    return (raw > cal->overhead) ? (raw - cal->overhead) : 0;
}
