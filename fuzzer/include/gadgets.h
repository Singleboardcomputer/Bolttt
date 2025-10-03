#ifndef GADGETS_H
#define GADGETS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_GADGET_LENGTH 32
#define MAX_GADGETS 1024

typedef enum {
    GADGET_LOAD_FAULTING,
    GADGET_LOAD_ASSIST,
    GADGET_SPECULATIVE_STORE,
    GADGET_BRANCH_MISTRAIN,
    GADGET_UNKNOWN
} gadget_type_t;

typedef struct {
    uint64_t address;
    uint32_t length;
    gadget_type_t type;
    uint8_t instructions[MAX_GADGET_LENGTH];
    char disassembly[256];
    double exploitability_score;
} gadget_t;

typedef struct {
    gadget_t *gadgets;
    uint32_t count;
    uint32_t capacity;
} gadget_list_t;

gadget_list_t* gadget_list_create(void);
void gadget_list_destroy(gadget_list_t *list);
bool gadget_list_add(gadget_list_t *list, gadget_t *gadget);

bool gadget_scan_memory_region(uint64_t start_addr, size_t size,
                                gadget_list_t *results);

bool gadget_scan_binary(const char *binary_path, gadget_list_t *results);

bool gadget_is_lvi_susceptible(const uint8_t *code, uint32_t length);

void gadget_print(gadget_t *gadget);
void gadget_list_print(gadget_list_t *list);

#endif
