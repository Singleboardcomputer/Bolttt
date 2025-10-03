#ifndef RACE_H
#define RACE_H

#include <stdint.h>
#include <stdbool.h>
#include "virtio.h"
#include "timing.h"

typedef struct {
    uint64_t iotlb_inv_mean;
    uint64_t iotlb_inv_min;
    uint64_t iotlb_inv_max;
    uint64_t iotlb_inv_stddev;
    uint32_t sample_count;
} iotlb_profile_t;

typedef enum {
    RACE_SUCCESS,
    RACE_TOO_EARLY,
    RACE_TOO_LATE,
    RACE_FAILED,
    RACE_UNKNOWN
} race_outcome_t;

typedef struct {
    race_outcome_t outcome;
    uint64_t t_trigger;
    uint64_t t_swap;
    uint64_t t_load;
    uint64_t t_probe;
    uint64_t window_estimate;
    bool leak_detected;
    uint64_t leak_latency;
} race_attempt_t;

bool race_estimate_iotlb_window(iotlb_profile_t *profile, uint32_t samples);

race_outcome_t race_execute_lvi_attempt(virtqueue_t *vq, uint16_t desc_idx,
                                         uint64_t target_addr, uint64_t probe_addr,
                                         iotlb_profile_t *profile,
                                         timing_calibration_t *cal,
                                         race_attempt_t *result);

#endif
