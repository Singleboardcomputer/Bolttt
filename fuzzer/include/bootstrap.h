#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_SAMPLES 100000
#define DEFAULT_BOOTSTRAP_ROUNDS 10000
#define DEFAULT_ALPHA 0.05
#define DEFAULT_NEGLIGIBLE_THRESHOLD 50

typedef struct {
    uint64_t *data;
    uint32_t count;
    uint32_t capacity;
} sample_population_t;

sample_population_t* population_create(uint32_t initial_capacity);
void population_destroy(sample_population_t *pop);
bool population_add(sample_population_t *pop, uint64_t value);
void population_clean_outliers(sample_population_t *pop);

typedef struct {
    double median_diff;
    double ci_lower;
    double ci_upper;
    double p_value;
    bool is_significant;
    bool exceeds_threshold;
    uint32_t bootstrap_rounds;
} bootstrap_result_t;

typedef struct {
    uint32_t bootstrap_rounds;
    double alpha;
    uint64_t negligible_threshold_cycles;
} bootstrap_config_t;

bool bootstrap_test(sample_population_t *leak, sample_population_t *no_leak,
                    bootstrap_config_t *config, bootstrap_result_t *result);

uint64_t stats_median(uint64_t *data, uint32_t count);
double stats_mean(uint64_t *data, uint32_t count);
double stats_stddev(uint64_t *data, uint32_t count, double mean);

#endif
