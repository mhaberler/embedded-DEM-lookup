#pragma once

#ifdef ESP32
    #include <Arduino.h>
#else
    #define MALLOC_CAP_SPIRAM 0
    #define MALLOC_CAP_8BIT 0
#endif
#include <stdint.h>
#include "logging.hpp"

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
    size_t capacity;
    uint32_t caps;
} buffer_t;

static inline void print_buffer(const char *tag, const buffer_t &b) {
    LOG_DEBUG("%s buffer %p size %zu capacity %zu caps %u", tag, b.buffer, b.size, b.capacity, b.caps);
}
static inline void print_buffer(const buffer_t &b) {
    print_buffer("",b);
}

static inline void init_buffer(buffer_t &b, size_t size = BUFFER_SIZE, uint32_t caps = MALLOC_CAP_SPIRAM) {
    size_t n = next_power_of_2(size);
#ifdef ESP32
    b.buffer =  n ? heap_caps_malloc(size, caps) : NULL;
    b.caps = caps;
#else
    b.buffer =  n ? malloc(size) : NULL;
    b.caps = 0;
#endif
    b.size = 0;
    b.capacity = n;

}

static inline void *get_buffer(buffer_t &b, size_t size = 0) {
    if (size <= b.capacity) {
        return b.buffer;
    }
    size_t n = next_power_of_2(size);
    LOG_DEBUG("get_buffer need %zu realloc %zu -> %zu  %p", size, b.capacity, n);
#ifdef ESP32
    b.buffer = heap_caps_realloc(b.buffer, n, b.caps);
#else
    b.buffer = realloc(b.buffer, n);
#endif
    b.capacity = n;
    return b.buffer;
}

static inline size_t buffer_size(const buffer_t &b) {
    return b.size;
}

// static inline size_t buffer_size(const buffer_t &b) {
//     return b.size;
// }


static inline bool set_buffer_size(buffer_t &b, size_t size) {
    if (size > b.capacity) {
        b.size = b.capacity;
        return false;
    }
    b.size = size;
    return true;
}

static inline size_t buffer_capacity(const buffer_t &b) {
    return b.capacity;
}

static inline void free_buffer(buffer_t &b) {
    if (b.size) {
#ifdef ESP32
        heap_caps_free(b.buffer);
#else
        free(b.buffer);
#endif

    }
    b.size = 0;
}
