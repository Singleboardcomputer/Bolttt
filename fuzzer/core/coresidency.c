#define _GNU_SOURCE
#include "coresidency.h"
#include "cache.h"
#include "affinity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define TEST_ITERATIONS 10000
#define SHARED_MEMORY_SIZE 4096

typedef struct {
    volatile uint64_t *shared_mem;
    int cpu_id;
    bool run;
    timing_calibration_t *cal;
} thread_data_t;

static void* victim_thread(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;

    if (!affinity_pin_thread(data->cpu_id)) {
        return NULL;
    }

    while (data->run) {
        volatile uint64_t dummy = *data->shared_mem;
        (void)dummy;

        for (volatile int i = 0; i < 100; i++);
    }

    return NULL;
}

bool coresidency_verify_llc(int cpu1, int cpu2, timing_calibration_t *cal,
                             cooresidency_result_t *result) {

    volatile uint64_t *shared_mem = aligned_alloc(4096, SHARED_MEMORY_SIZE);
    if (!shared_mem) {
        return false;
    }

    memset((void*)shared_mem, 0xAA, SHARED_MEMORY_SIZE);

    thread_data_t victim_data = {
        .shared_mem = shared_mem,
        .cpu_id = cpu2,
        .run = true,
        .cal = cal
    };

    pthread_t victim_tid;
    if (pthread_create(&victim_tid, NULL, victim_thread, &victim_data) != 0) {
        free((void*)shared_mem);
        return false;
    }

    usleep(10000);

    if (!affinity_pin_thread(cpu1)) {
        victim_data.run = false;
        pthread_join(victim_tid, NULL);
        free((void*)shared_mem);
        return false;
    }

    uint64_t hit_measurements[TEST_ITERATIONS];
    int hit_count = 0;

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        cache_flush((void*)shared_mem);
        _mm_mfence();

        usleep(10);

        uint64_t reload_time = cache_probe_time(shared_mem);

        if (reload_time < cal->cache_miss_threshold) {
            hit_measurements[hit_count++] = reload_time;
        }
    }

    victim_data.run = false;
    pthread_join(victim_tid, NULL);

    double hit_rate = (double)hit_count / TEST_ITERATIONS;

    if (hit_count > 0) {
        uint64_t sum = 0;
        for (int i = 0; i < hit_count; i++) {
            sum += hit_measurements[i];
        }
        result->avg_hit_latency = sum / hit_count;
    }

    result->attacker_cpu = cpu1;
    result->victim_cpu = cpu2;
    result->confidence = hit_rate;
    result->llc_shared = (hit_rate > 0.1);

    topology_t *topo = topology_detect();
    result->ht_siblings = (topo->cpus[cpu1].physical_id == topo->cpus[cpu2].physical_id &&
                           topo->cpus[cpu1].core_id == topo->cpus[cpu2].core_id);
    topology_free(topo);

    free((void*)shared_mem);

    printf("[%s] Co-residency test: CPU %d <-> CPU %d\n",
           result->llc_shared ? "+" : "-", cpu1, cpu2);
    printf("      Hit rate: %.2f%%, Avg latency: %lu cycles\n",
           hit_rate * 100, result->avg_hit_latency);

    return result->llc_shared;
}

bool coresidency_scan_and_verify(timing_calibration_t *cal,
                                  cooresidency_result_t *result) {
    topology_t *topo = topology_detect();

    printf("[*] Scanning for co-resident CPUs...\n");

    for (int i = 0; i < topo->num_cpus; i++) {
        for (int j = i + 1; j < topo->num_cpus; j++) {
            if (topo->cpus[i].physical_id == topo->cpus[j].physical_id) {
                if (cooresidency_verify_llc(i, j, cal, result)) {
                    topology_free(topo);
                    printf("[+] Found co-resident pair: CPU %d <-> CPU %d\n", i, j);
                    return true;
                }
            }
        }
    }

    topology_free(topo);
    printf("[-] No co-resident CPUs found\n");
    return false;
}
