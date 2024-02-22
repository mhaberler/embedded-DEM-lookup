#pragma once
#ifdef EMBEDDED
    #include <Arduino.h>
#endif

typedef enum {
    LOG_LEVEL_NONE,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE
} logLevel_t;

extern logLevel_t log_level;

static inline void set_loglevel(logLevel_t l) {
    log_level = l;
}

#define ___xstr(s) ___str(s)
#define ___str(s) #s

#ifndef LOG_PRINTF
    #ifdef EMBEDDED
        #define LOG_PRINTF Serial.printf
    #else
        #define LOG_PRINTF printf
    #endif
#endif

#define LOG_ERROR(format, ...) do { if (log_level >= LOG_LEVEL_ERROR) LOG_PRINTF(format ___str(\n) __VA_OPT__(,) __VA_ARGS__); } while (0);
#define LOG_WARN(format, ...) do { if (log_level >= LOG_LEVEL_WARN) LOG_PRINTF(format ___str(\n) __VA_OPT__(,) __VA_ARGS__); } while (0);
#define LOG_INFO(format, ...) do { if (log_level >= LOG_LEVEL_INFO)LOG_PRINTF(format ___str(\n) __VA_OPT__(,) __VA_ARGS__); } while (0);
#define LOG_DEBUG(format, ...) do { if (log_level >= LOG_LEVEL_DEBUG) LOG_PRINTF(format ___str(\n) __VA_OPT__(,) __VA_ARGS__); } while (0);
#define LOG_VERBOSE(format, ...) do { if (log_level >= LOG_LEVEL_VERBOSE) LOG_PRINTF(format ___str(\n) __VA_OPT__(,) __VA_ARGS__); } while (0);
