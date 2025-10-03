#ifndef DB_H
#define DB_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "race.h"
#include "bootstrap.h"
#include "gadgets.h"

typedef struct {
    uint64_t campaign_id;
    char name[128];
    time_t start_time;
    time_t end_time;
    uint32_t total_attempts;
    uint32_t successful_leaks;
    double success_rate;
} campaign_t;

typedef struct {
    uint64_t experiment_id;
    uint64_t campaign_id;
    time_t timestamp;
    uint64_t gadget_addr;
    race_outcome_t outcome;
    bool leak_detected;
    uint64_t leak_latency;
    uint64_t window_estimate;
    double p_value;
    bool statistically_significant;
} experiment_t;

typedef struct {
    char filepath[512];
    FILE *fp;
} db_handle_t;

db_handle_t* db_open(const char *filepath);
void db_close(db_handle_t *db);

bool db_campaign_create(db_handle_t *db, campaign_t *campaign);
bool db_campaign_update(db_handle_t *db, campaign_t *campaign);
bool db_campaign_finalize(db_handle_t *db, uint64_t campaign_id);

bool db_experiment_log(db_handle_t *db, experiment_t *exp);

bool db_export_csv(db_handle_t *db, const char *output_path);

#endif
