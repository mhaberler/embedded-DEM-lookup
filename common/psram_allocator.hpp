// https://github.com/dk307/AirQualitySensor-IDF/blob/b13d478c166b29d81220994e345e78ad1de85a9c/main/util/psram_allocator.h#L10
#pragma once

#ifdef ESP32
    #include <esp_heap_caps.h>
#endif

#include <memory>

namespace esp32 {
namespace psram {
template <typename T> class allocator {
  public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T *pointer;
    typedef const T *const_pointer;
    typedef T &reference;
    typedef const T &const_reference;
    typedef T value_type;

    allocator() = default;
    ~allocator() = default;

    template <class U> struct rebind {
        typedef allocator<U> other;
    };

    pointer address(reference x) const {
        return &x;
    }
    const_pointer address(const_reference x) const {
        return &x;
    }
    size_type max_size() const throw() {
        return size_t(-1) / sizeof(value_type);
    }

    pointer allocate(size_type n, const void *hint = 0) {

#ifdef ESP32
        return static_cast<pointer>(heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
        return static_cast<pointer>(malloc(n * sizeof(T));
#endif
    }

    void deallocate(pointer p, size_type n) {
#ifdef ESP32
        heap_caps_free(p);
#else
        free(p);
#endif
    }

    template <class U, class... Args> void construct(U *p, Args &&...args) {
        ::new ((void *)p) U(std::forward<Args>(args)...);
    }

    void destroy(pointer p) {
        p->~T();
    }
};

struct deleter {
    void operator()(void *p) const {
#ifdef ESP32
        heap_caps_free(p);
#else
        free(p);
#endif
    }
};

template <class T, class... Args> std::unique_ptr<T, deleter> make_unique(Args &&...args) {
#ifdef ESP32
    auto p = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM);
#else
    auto p = malloc(sizeof(T));
#endif
    return std::unique_ptr<T, deleter>(::new (p) T(std::forward<Args>(args)...), deleter());
}

struct json_allocator {
    void *allocate(size_t size) {
#ifdef ESP32
        return heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
        return malloc(n * sizeof(T));
#endif
    }

    void deallocate(void *pointer) {
#ifdef ESP32
        heap_caps_free(pointer);
#else
        free(pointer);
#endif
    }

    void *reallocate(void *ptr, size_t new_size) {
#ifdef ESP32
        return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
        return realloc(ptr, new_size);
#endif
    }
};

using string = std::basic_string<char, std::char_traits<char>, esp32::psram::allocator<char>>;
} // namespace psram
} // namespace esp32