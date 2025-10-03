#ifndef VIRTIO_H
#define VIRTIO_H

#include <stdint.h>
#include <stdbool.h>

#define VIRTIO_RING_SIZE 256
#define VIRTIO_DESC_F_NEXT 1
#define VIRTIO_DESC_F_WRITE 2

typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;

typedef struct __attribute__((packed)) {
    __le64 addr;
    __le32 len;
    __le16 flags;
    __le16 next;
} vring_desc_t;

typedef struct __attribute__((packed)) {
    __le16 flags;
    __le16 idx;
    __le16 ring[VIRTIO_RING_SIZE];
} vring_avail_t;

typedef struct __attribute__((packed)) {
    __le32 id;
    __le32 len;
} vring_used_elem_t;

typedef struct __attribute__((packed)) {
    __le16 flags;
    __le16 idx;
    vring_used_elem_t ring[VIRTIO_RING_SIZE];
} vring_used_t;

typedef struct {
    vring_desc_t *desc;
    vring_avail_t *avail;
    vring_used_t *used;
    uint16_t num;
    uint16_t free_head;
    uint16_t last_used_idx;
} virtqueue_t;

virtqueue_t* virtio_queue_create(uint16_t queue_size);
void virtio_queue_destroy(virtqueue_t *vq);

bool virtio_descriptor_prepare_race(virtqueue_t *vq, uint64_t initial_addr,
                                     uint64_t target_addr, uint16_t *desc_idx);

bool virtio_descriptor_atomic_swap(vring_desc_t *desc, uint64_t new_addr);

typedef struct {
    uint64_t t_start;
    uint64_t t_swap;
    uint64_t t_inv_expected;
    bool swap_success;
} race_timing_t;

#endif
