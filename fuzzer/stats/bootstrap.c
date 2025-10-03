#include "bootstrap.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

sample_population_t* population_create(uint32_t initial_capacity) {
    sample_population_t *pop = calloc(1, sizeof(sample_population_t));
    if (!pop) return NULL;

    pop->capacity = initial_capacity;
    pop->data = calloc(initial_capacity, sizeof(uint64_t));
    pop->count = 0;

    if (!pop->data) {
        free(pop);
        return NULL;
    }

    return pop;
}

void population_destroy(sample_population_t *pop) {
    if (pop) {
        free(pop->data);
        free(pop);
    }
}

bool population_add(sample_population_t *pop, uint64_t value) {
    if (pop->count >= pop->capacity) {
        uint32_t new_capacity = pop->capacity * 2;
        uint64_t *new_data = realloc(pop->data, new_capacity * sizeof(uint64_t));
        if (!new_data) return false;

        pop->data = new_data;
        pop->capacity = new_capacity;
    }

    pop->data[pop->count++] = value;
    return true;
}

static int compare_uint64(const void *a, const void *b) {
    uint64_t va = *(const uint64_t*)a;
    uint64_t vb = *(const uint64_t*)b;
    return (va > vb) - (va < vb);
}

uint64_t stats_median(uint64_t *data, uint32_t count) {
    if (count == 0) return 0;

    uint64_t *sorted = malloc(count * sizeof(uint64_t));
    memcpy(sorted, data, count * sizeof(uint64_t));
    qsort(sorted, count, sizeof(uint64_t), compare_uint64);

    uint64_t result;
    if (count % 2 == 0) {
        result = (sorted[count/2 - 1] + sorted[count/2]) / 2;
    } else {
        result = sorted[count/2];
    }

    free(sorted);
    return result;
}

double stats_mean(uint64_t *data, uint32_t count) {
    if (count == 0) return 0.0;

    uint64_t sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        sum += data[i];
    }

    return (double)sum / count;
}

double stats_stddev(uint64_t *data, uint32_t count, double mean) {
    if (count < 2) return 0.0;

    double var_sum = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        double diff = data[i] - mean;
        var_sum += diff * diff;
    }

    return sqrt(var_sum / (count - 1));
}

void population_clean_outliers(sample_population_t *pop) {
    if (pop->count < 4) return;

    uint64_t *sorted = malloc(pop->count * sizeof(uint64_t));
    memcpy(sorted, pop->data, pop->count * sizeof(uint64_t));
    qsort(sorted, pop->count, sizeof(uint64_t), compare_uint64);

    uint32_t q1_idx = pop->count / 4;
    uint32_t q3_idx = (3 * pop->count) / 4;

    uint64_t q1 = sorted[q1_idx];
    uint64_t q3 = sorted[q3_idx];
    uint64_t iqr = q3 - q1;

    uint64_t lower_bound = (q1 > iqr * 1.5) ? (q1 - iqr * 1.5) : 0;
    uint64_t upper_bound = q3 + iqr * 1.5;

    free(sorted);

    uint32_t new_count = 0;
    for (uint32_t i = 0; i < pop->count; i++) {
        if (pop->data[i] >= lower_bound && pop->data[i] <= upper_bound) {
            pop->data[new_count++] = pop->data[i];
        }
    }

    uint32_t removed = pop->count - new_count;
    pop->count = new_count;

    if (removed > 0) {
        printf("[*] Removed %u outliers from population\n", removed);
    }
}

static uint64_t* bootstrap_sample(uint64_t *data, uint32_t count) {
    uint64_t *sample = malloc(count * sizeof(uint64_t));

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = rand() % count;
        sample[i] = data[idx];
    }

    return sample;
}

bool bootstrap_test(sample_population_t *leak, sample_population_t *no_leak,
                    bootstrap_config_t *config, bootstrap_result_t *result) {

    if (leak->count < 10 || no_leak->count < 10) {
        fprintf(stderr, "[-] Insufficient samples for bootstrap test\n");
        return false;
    }

    printf("[*] Running bootstrap test (%u rounds)...\n", config->bootstrap_rounds);

    double *test_stats = calloc(config->bootstrap_rounds, sizeof(double));
    if (!test_stats) return false;

    uint64_t observed_median_leak = stats_median(leak->data, leak->count);
    uint64_t observed_median_no_leak = stats_median(no_leak->data, no_leak->count);
    double observed_diff = (double)observed_median_leak - (double)observed_median_no_leak;

    for (uint32_t i = 0; i < config->bootstrap_rounds; i++) {
        uint64_t *sample_leak = bootstrap_sample(leak->data, leak->count);
        uint64_t *sample_no_leak = bootstrap_sample(no_leak->data, no_leak->count);

        uint64_t median_leak = stats_median(sample_leak, leak->count);
        uint64_t median_no_leak = stats_median(sample_no_leak, no_leak->count);

        test_stats[i] = (double)median_leak - (double)median_no_leak;

        free(sample_leak);
        free(sample_no_leak);

        if (i % 1000 == 0 && i > 0) {
            printf("    Progress: %u/%u\r", i, config->bootstrap_rounds);
            fflush(stdout);
        }
    }
    printf("\n");

    qsort(test_stats, config->bootstrap_rounds, sizeof(double),
          (int(*)(const void*, const void*))compare_uint64);

    uint32_t lower_idx = (uint32_t)(config->alpha / 2.0 * config->bootstrap_rounds);
    uint32_t upper_idx = (uint32_t)((1.0 - config->alpha / 2.0) * config->bootstrap_rounds);

    result->median_diff = observed_diff;
    result->ci_lower = test_stats[lower_idx];
    result->ci_upper = test_stats[upper_idx];
    result->bootstrap_rounds = config->bootstrap_rounds;

    uint32_t count_extreme = 0;
    for (uint32_t i = 0; i < config->bootstrap_rounds; i++) {
        if (fabs(test_stats[i]) >= fabs(observed_diff)) {
            count_extreme++;
        }
    }
    result->p_value = (double)count_extreme / config->bootstrap_rounds;

    result->is_significant = (result->p_value < config->alpha);
    result->exceeds_threshold = (fabs(observed_diff) > config->negligible_threshold_cycles);

    printf("[+] Bootstrap Results:\n");
    printf("    Median difference: %.2f cycles\n", result->median_diff);
    printf("    95%% CI: [%.2f, %.2f]\n", result->ci_lower, result->ci_upper);
    printf("    p-value: %.6f\n", result->p_value);
    printf("    Significant: %s\n", result->is_significant ? "YES" : "NO");
    printf("    Exceeds threshold: %s\n", result->exceeds_threshold ? "YES" : "NO");

    free(test_stats);

    return (result->is_significant && result->exceeds_threshold);
}
