#include "cache.h"

cache_result_t cache_flush_reload(volatile void *addr, uint64_t threshold) {
    cache_flush(addr);

    _mm_mfence();

    uint64_t reload_time = cache_probe_time(addr);

    if (reload_time < threshold) {
        return CACHE_HIT;
    } else {
        return CACHE_MISS;
    }
}
