#pragma once

#include <Arduino.h>
#include <stdint.h>

#ifndef BUFFER_SIZE
    #define BUFFER_SIZE 1024
#endif

static inline size_t next_power_of_2(size_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
};

// a buffer which grows as needed

typedef struct {
    void *buffer;
    size_t size;
    uint32_t caps;
} buffer_t;

static inline void init_buffer(buffer_t &b, size_t size = BUFFER_SIZE, uint32_t caps = MALLOC_CAP_SPIRAM) {
    size_t n = next_power_of_2(size);
    b.buffer =  n ? heap_caps_malloc(size, caps) : NULL;
    b.size = n;
    b.caps = caps;
}

static inline void *get_buffer(buffer_t &b, size_t size) {
    if (size <= b.size) {
        return b.buffer;
    }
    size_t n = next_power_of_2(size);
    b.buffer = heap_caps_realloc(b.buffer, n, b.caps);
    b.size = n;
}

static inline size_t buffer_size(const buffer_t &b) {
    return b.size;
}

static inline void free_buffer(buffer_t &b) {
    if (b.size) {
        heap_caps_free(b.buffer);
    }
    b.size = 0;
}
