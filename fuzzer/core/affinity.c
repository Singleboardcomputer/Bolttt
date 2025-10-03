#define _GNU_SOURCE
#include "affinity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>

topology_t* topology_detect(void) {
    topology_t *topo = calloc(1, sizeof(topology_t));

    topo->num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    topo->cpus = calloc(topo->num_cpus, sizeof(cpu_info_t));

    for (int i = 0; i < topo->num_cpus; i++) {
        char path[256];
        FILE *f;

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", i);
        f = fopen(path, "r");
        if (f) {
            fscanf(f, "%d", &topo->cpus[i].physical_id);
            fclose(f);
        }

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/topology/core_id", i);
        f = fopen(path, "r");
        if (f) {
            fscanf(f, "%d", &topo->cpus[i].core_id);
            fclose(f);
        }

        topo->cpus[i].logical_cpu = i;
    }

    for (int i = 0; i < topo->num_cpus; i++) {
        for (int j = i + 1; j < topo->num_cpus; j++) {
            if (topo->cpus[i].physical_id == topo->cpus[j].physical_id &&
                topo->cpus[i].core_id == topo->cpus[j].core_id) {
                topo->cpus[i].is_ht_sibling = true;
                topo->cpus[j].is_ht_sibling = true;
            }
        }
    }

    int max_core = 0;
    for (int i = 0; i < topo->num_cpus; i++) {
        if (topo->cpus[i].core_id > max_core) {
            max_core = topo->cpus[i].core_id;
        }
    }
    topo->num_cores = max_core + 1;

    int max_socket = 0;
    for (int i = 0; i < topo->num_cpus; i++) {
        if (topo->cpus[i].physical_id > max_socket) {
            max_socket = topo->cpus[i].physical_id;
        }
    }
    topo->num_sockets = max_socket + 1;

    return topo;
}

void topology_free(topology_t *topo) {
    if (topo) {
        free(topo->cpus);
        free(topo);
    }
}

void topology_print(topology_t *topo) {
    printf("[*] CPU Topology:\n");
    printf("    Total CPUs: %d\n", topo->num_cpus);
    printf("    Physical Cores: %d\n", topo->num_cores);
    printf("    Sockets: %d\n", topo->num_sockets);
    printf("\n");
    printf("    CPU  Socket  Core  HT\n");
    printf("    ---  ------  ----  --\n");
    for (int i = 0; i < topo->num_cpus; i++) {
        printf("    %3d  %6d  %4d  %s\n",
               topo->cpus[i].logical_cpu,
               topo->cpus[i].physical_id,
               topo->cpus[i].core_id,
               topo->cpus[i].is_ht_sibling ? "Y" : "N");
    }
}

bool affinity_pin_thread(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("sched_setaffinity");
        return false;
    }

    return true;
}

int affinity_get_current_cpu(void) {
    return sched_getcpu();
}

bool affinity_find_ht_siblings(topology_t *topo, int cpu, int *sibling) {
    if (cpu >= topo->num_cpus) return false;

    int target_socket = topo->cpus[cpu].physical_id;
    int target_core = topo->cpus[cpu].core_id;

    for (int i = 0; i < topo->num_cpus; i++) {
        if (i != cpu &&
            topo->cpus[i].physical_id == target_socket &&
            topo->cpus[i].core_id == target_core) {
            *sibling = i;
            return true;
        }
    }

    return false;
}

bool affinity_find_llc_sharers(topology_t *topo, int cpu, int *sharers, int max_sharers) {
    if (cpu >= topo->num_cpus) return false;

    int target_socket = topo->cpus[cpu].physical_id;
    int count = 0;

    for (int i = 0; i < topo->num_cpus && count < max_sharers; i++) {
        if (i != cpu && topo->cpus[i].physical_id == target_socket) {
            sharers[count++] = i;
        }
    }

    return count > 0;
}
