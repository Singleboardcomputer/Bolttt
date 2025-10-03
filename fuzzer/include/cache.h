#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <x86intrin.h>

#define CACHE_LINE_SIZE 64

static inline void cache_flush(volatile void *addr) {
    _mm_clflush(addr);
    _mm_mfence();
}

static inline void cache_flush_range(volatile void *start, size_t size) {
    volatile uint8_t *ptr = (volatile uint8_t *)start;
    for (size_t i = 0; i < size; i += CACHE_LINE_SIZE) {
        _mm_clflush((void*)(ptr + i));
    }
    _mm_mfence();
}

static inline uint64_t cache_probe_time(volatile void *addr) {
    uint64_t start, end;
    uint32_t aux;

    _mm_lfence();
    start = __rdtsc();
    _mm_lfence();

    volatile uint64_t dummy = *(volatile uint64_t *)addr;
    (void)dummy;

    _mm_lfence();
    end = __rdtscp(&aux);
    _mm_lfence();

    return end - start;
}

typedef enum {
    CACHE_HIT,
    CACHE_MISS,
    CACHE_UNKNOWN
} cache_result_t;

cache_result_t cache_flush_reload(volatile void *addr, uint64_t threshold);

#endif
