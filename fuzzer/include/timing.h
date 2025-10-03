#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>
#include <x86intrin.h>

typedef struct {
    uint64_t overhead;
    uint64_t cache_hit_threshold;
    uint64_t cache_miss_threshold;
} timing_calibration_t;

static inline uint64_t timing_start(void) {
    uint64_t cycles;
    _mm_lfence();
    cycles = __rdtsc();
    _mm_lfence();
    return cycles;
}

static inline uint64_t timing_end(void) {
    uint64_t cycles;
    uint32_t aux;
    _mm_lfence();
    cycles = __rdtscp(&aux);
    _mm_lfence();
    return cycles;
}

static inline uint64_t timing_measure(void) {
    uint64_t start, end;
    start = timing_start();
    end = timing_end();
    return end - start;
}

void timing_calibrate(timing_calibration_t *cal);
uint64_t timing_measure_corrected(timing_calibration_t *cal);

#endif
