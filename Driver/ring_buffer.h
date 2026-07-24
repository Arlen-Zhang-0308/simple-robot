#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *storage;
    size_t capacity;
    volatile size_t head;
    volatile size_t tail;
} RingBuffer;

bool ring_buffer_init(RingBuffer *buffer, uint8_t *storage, size_t capacity);
bool ring_buffer_push(RingBuffer *buffer, uint8_t value);
bool ring_buffer_pop(RingBuffer *buffer, uint8_t *value);
size_t ring_buffer_size(const RingBuffer *buffer);
bool ring_buffer_is_empty(const RingBuffer *buffer);

#endif
