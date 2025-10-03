#include "gadgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gadget_list_t* gadget_list_create(void) {
    gadget_list_t *list = calloc(1, sizeof(gadget_list_t));
    if (!list) return NULL;

    list->capacity = 256;
    list->gadgets = calloc(list->capacity, sizeof(gadget_t));
    list->count = 0;

    if (!list->gadgets) {
        free(list);
        return NULL;
    }

    return list;
}

void gadget_list_destroy(gadget_list_t *list) {
    if (list) {
        free(list->gadgets);
        free(list);
    }
}

bool gadget_list_add(gadget_list_t *list, gadget_t *gadget) {
    if (list->count >= list->capacity) {
        uint32_t new_capacity = list->capacity * 2;
        gadget_t *new_gadgets = realloc(list->gadgets,
                                         new_capacity * sizeof(gadget_t));
        if (!new_gadgets) return false;

        list->gadgets = new_gadgets;
        list->capacity = new_capacity;
    }

    memcpy(&list->gadgets[list->count++], gadget, sizeof(gadget_t));
    return true;
}

bool gadget_is_lvi_susceptible(const uint8_t *code, uint32_t length) {
    if (length < 2) return false;

    for (uint32_t i = 0; i < length - 1; i++) {
        if ((code[i] & 0xF8) == 0x48) {
            if ((code[i+1] & 0xC7) == 0x87) {
                return true;
            }

            if ((code[i+1] & 0xC7) == 0x03 ||
                (code[i+1] & 0xC7) == 0x0B ||
                (code[i+1] & 0xC7) == 0x13 ||
                (code[i+1] & 0xC7) == 0x1B) {
                return true;
            }
        }

        if (code[i] == 0x0F) {
            if (code[i+1] == 0xB6 || code[i+1] == 0xB7 ||
                code[i+1] == 0xBE || code[i+1] == 0xBF) {
                return true;
            }
        }

        if ((code[i] & 0xF0) == 0x40 && i < length - 2) {
            if (code[i+1] == 0x8B || code[i+1] == 0x8A) {
                return true;
            }
        }
    }

    return false;
}

bool gadget_scan_memory_region(uint64_t start_addr, size_t size,
                                gadget_list_t *results) {

    printf("[*] Scanning memory region 0x%lx - 0x%lx\n",
           start_addr, start_addr + size);

    const uint8_t *mem = (const uint8_t *)start_addr;
    uint32_t gadget_count = 0;

    for (size_t offset = 0; offset < size - MAX_GADGET_LENGTH; offset++) {
        if (gadget_is_lvi_susceptible(&mem[offset], MAX_GADGET_LENGTH)) {
            gadget_t gadget = {0};
            gadget.address = start_addr + offset;
            gadget.length = 16;
            gadget.type = GADGET_LOAD_FAULTING;
            gadget.exploitability_score = 0.5;

            memcpy(gadget.instructions, &mem[offset],
                   gadget.length > MAX_GADGET_LENGTH ? MAX_GADGET_LENGTH : gadget.length);

            snprintf(gadget.disassembly, sizeof(gadget.disassembly),
                     "mov rax, [rbx+rcx*8] @ 0x%lx", gadget.address);

            if (gadget_list_add(results, &gadget)) {
                gadget_count++;
            }

            offset += 8;
        }
    }

    printf("[+] Found %u potential LVI gadgets\n", gadget_count);
    return gadget_count > 0;
}

bool gadget_scan_binary(const char *binary_path, gadget_list_t *results) {
    FILE *f = fopen(binary_path, "rb");
    if (!f) {
        fprintf(stderr, "[-] Failed to open binary: %s\n", binary_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buffer = malloc(size);
    if (!buffer) {
        fclose(f);
        return false;
    }

    fread(buffer, 1, size, f);
    fclose(f);

    printf("[*] Scanning binary: %s (%zu bytes)\n", binary_path, size);

    bool result = gadget_scan_memory_region((uint64_t)buffer, size, results);

    free(buffer);
    return result;
}

void gadget_print(gadget_t *gadget) {
    const char *type_str[] = {
        "LOAD_FAULTING",
        "LOAD_ASSIST",
        "SPECULATIVE_STORE",
        "BRANCH_MISTRAIN",
        "UNKNOWN"
    };

    printf("[Gadget @ 0x%016lx] Type: %s, Score: %.2f\n",
           gadget->address, type_str[gadget->type], gadget->exploitability_score);
    printf("  Bytes: ");
    for (uint32_t i = 0; i < gadget->length && i < 16; i++) {
        printf("%02x ", gadget->instructions[i]);
    }
    printf("\n");
    printf("  %s\n", gadget->disassembly);
}

void gadget_list_print(gadget_list_t *list) {
    printf("\n[*] Gadget List (%u total):\n", list->count);
    printf("====================================\n");

    for (uint32_t i = 0; i < list->count && i < 10; i++) {
        gadget_print(&list->gadgets[i]);
        printf("\n");
    }

    if (list->count > 10) {
        printf("... and %u more gadgets\n", list->count - 10);
    }
}
