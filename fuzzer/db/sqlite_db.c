#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

db_handle_t* db_open(const char *filepath) {
    db_handle_t *db = calloc(1, sizeof(db_handle_t));
    if (!db) return NULL;

    strncpy(db->filepath, filepath, sizeof(db->filepath) - 1);

    db->fp = fopen(filepath, "a+");
    if (!db->fp) {
        free(db);
        return NULL;
    }

    fseek(db->fp, 0, SEEK_END);
    if (ftell(db->fp) == 0) {
        fprintf(db->fp, "# LVI-DMA Fuzzer Experiment Log\n");
        fprintf(db->fp, "# Generated: %ld\n", (long)time(NULL));
        fprintf(db->fp, "timestamp,campaign_id,experiment_id,gadget_addr,outcome,leak_detected,leak_latency,window_estimate,p_value,significant\n");
        fflush(db->fp);
    }

    printf("[+] Opened experiment database: %s\n", filepath);

    return db;
}

void db_close(db_handle_t *db) {
    if (db) {
        if (db->fp) {
            fclose(db->fp);
        }
        free(db);
    }
}

bool db_campaign_create(db_handle_t *db, campaign_t *campaign) {
    campaign->campaign_id = (uint64_t)time(NULL);
    campaign->start_time = time(NULL);
    campaign->total_attempts = 0;
    campaign->successful_leaks = 0;
    campaign->success_rate = 0.0;

    printf("[+] Created campaign %lu: %s\n", campaign->campaign_id, campaign->name);

    return true;
}

bool db_campaign_update(db_handle_t *db, campaign_t *campaign) {
    if (campaign->total_attempts > 0) {
        campaign->success_rate = (double)campaign->successful_leaks / campaign->total_attempts;
    }

    return true;
}

bool db_campaign_finalize(db_handle_t *db, uint64_t campaign_id) {
    printf("[*] Finalizing campaign %lu\n", campaign_id);
    return true;
}

bool db_experiment_log(db_handle_t *db, experiment_t *exp) {
    if (!db || !db->fp) return false;

    const char *outcome_str[] = {
        "SUCCESS", "TOO_EARLY", "TOO_LATE", "FAILED", "UNKNOWN"
    };

    fprintf(db->fp, "%ld,%lu,%lu,0x%lx,%s,%d,%lu,%lu,%.6f,%d\n",
            (long)exp->timestamp,
            exp->campaign_id,
            exp->experiment_id,
            exp->gadget_addr,
            outcome_str[exp->outcome],
            exp->leak_detected,
            exp->leak_latency,
            exp->window_estimate,
            exp->p_value,
            exp->statistically_significant);

    fflush(db->fp);

    return true;
}

bool db_export_csv(db_handle_t *db, const char *output_path) {
    printf("[*] Exporting database to: %s\n", output_path);

    FILE *in = fopen(db->filepath, "r");
    FILE *out = fopen(output_path, "w");

    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        return false;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), in)) {
        fputs(buffer, out);
    }

    fclose(in);
    fclose(out);

    printf("[+] Database exported successfully\n");

    return true;
}
