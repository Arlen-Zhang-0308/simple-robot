#include "ring_buffer.h"

bool ring_buffer_init(RingBuffer *buffer, uint8_t *storage, size_t capacity)
{
    if ((buffer == NULL) || (storage == NULL) || (capacity < 2U)) {
        return false;
    }

    buffer->storage = storage;
    buffer->capacity = capacity;
    buffer->head = 0U;
    buffer->tail = 0U;
    return true;
}

bool ring_buffer_push(RingBuffer *buffer, uint8_t value)
{
    size_t next;

    if ((buffer == NULL) || (buffer->storage == NULL)) {
        return false;
    }

    next = (buffer->head + 1U) % buffer->capacity;
    if (next == buffer->tail) {
        return false;
    }

    buffer->storage[buffer->head] = value;
    buffer->head = next;
    return true;
}

bool ring_buffer_pop(RingBuffer *buffer, uint8_t *value)
{
    if ((buffer == NULL) || (value == NULL) || (buffer->storage == NULL) ||
        (buffer->tail == buffer->head)) {
        return false;
    }

    *value = buffer->storage[buffer->tail];
    buffer->tail = (buffer->tail + 1U) % buffer->capacity;
    return true;
}

size_t ring_buffer_size(const RingBuffer *buffer)
{
    if ((buffer == NULL) || (buffer->storage == NULL)) {
        return 0U;
    }

    return (buffer->head + buffer->capacity - buffer->tail) % buffer->capacity;
}

bool ring_buffer_is_empty(const RingBuffer *buffer)
{
    return ring_buffer_size(buffer) == 0U;
}
