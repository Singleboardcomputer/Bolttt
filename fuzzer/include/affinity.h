#ifndef AFFINITY_H
#define AFFINITY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int physical_id;
    int core_id;
    int logical_cpu;
    bool is_ht_sibling;
} cpu_info_t;

typedef struct {
    int num_cpus;
    int num_cores;
    int num_sockets;
    cpu_info_t *cpus;
} topology_t;

topology_t* topology_detect(void);
void topology_free(topology_t *topo);
void topology_print(topology_t *topo);

bool affinity_pin_thread(int cpu_id);
int affinity_get_current_cpu(void);

bool affinity_find_ht_siblings(topology_t *topo, int cpu, int *sibling);
bool affinity_find_llc_sharers(topology_t *topo, int cpu, int *sharers, int max_sharers);

#endif
