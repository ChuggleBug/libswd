
#ifndef __SWD_LOG_H
#define __SWD_LOG_H

#include "swd_conf.h"

// Check if at least one of the log levels were defined if logging is enabled
#if defined(SWD_ENABLE_LOGGING) &&                                                                 \
    !(defined(SWD_LOG_LEVEL_VERBOSE) || defined(SWD_LOG_LEVEL_DEBUG) ||                            \
      defined(SWD_LOG_LEVEL_INFO) || defined(SWD_LOG_LEVEL_WARN) || defined(SWD_LOG_LEVEL_ERROR))
#warning "Logging was enabled, but no log level was set. Select one in 'swd_conf.h'"
#endif // SWD_ENABLE_LOGGING && ...

// Define higher level log levels
#ifdef SWD_LOG_LEVEL_VERBOSE
#define SWD_LOG_LEVEL_DEBUG
#define SWD_LOG_LEVEL_INFO
#define SWD_LOG_LEVEL_WARN
#define SWD_LOG_LEVEL_ERROR
#endif

#ifdef SWD_LOG_LEVEL_DEBUG
#define SWD_LOG_LEVEL_INFO
#define SWD_LOG_LEVEL_WARN
#define SWD_LOG_LEVEL_ERROR
#endif

#ifdef SWD_LOG_LEVEL_INFO
#define SWD_LOG_LEVEL_WARN
#define SWD_LOG_LEVEL_ERROR
#endif

#ifdef SWD_LOG_LEVEL_WARN
#define SWD_LOG_LEVEL_ERROR
#endif

#if (defined(SWD_ENABLE_LOGGING) && defined(SWD_USE_CUSTOM_PRINT))
/* User has provided a custom implementation somewhere*/

#ifdef SWD_LOG_LEVEL_VERBOSE
extern void _swd_log_verbse(char *fmt, ...);
#define SWD_LOGV(fmt, ...) _swd_log_verbse(fmt, ##__VA_ARGS__)
#endif // defined(VERBOSE)

#ifdef SWD_LOG_LEVEL_DEBUG
extern void _swd_log_debug(char *fmt, ...);
#define SWD_DEBUG(fmt, ...) _swd_log_debug(fmt, ##__VA_ARGS__)
#endif // defined(DEBUG)

#ifdef SWD_LOG_LEVEL_INFO
extern void _swd_log_info(char *fmt, ...);
#define SWD_LOGI(fmt, ...) _swd_log_info(fmt, ##__VA_ARGS__)
#endif // defined(INFO)

#ifdef SWD_LOG_LEVEL_WARN
extern void _swd_log_warn(char *fmt, ...);
#define SWD_LOGW(fmt, ...) _swd_log_warn(fmt, ##__VA_ARGS__)
#endif // defined(WARN)

#ifdef SWD_LOG_LEVEL_ERROR
extern void _swd_log_error(char *fmt, ...);
#define SWD_LOGE(fmt, ...) _swd_log_error(fmt, ##__VA_ARGS__)
#endif // defined(ERROR)

#elif defined(SWD_ENABLE_LOGGING)
/* A default printf is being used*/
#include <stdio.h>

#ifdef SWD_LOG_LEVEL_VERBOSE
#define SWD_LOGV(fmt, ...) printf("[VBOSE] %s:%d -> " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif // defined(VERBOSE)

#ifdef SWD_LOG_LEVEL_DEBUG
#define SWD_LOGD(fmt, ...) printf("[DEBUG] %s:%d -> " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif // defined(DEBUG)

#ifdef SWD_LOG_LEVEL_INFO
#define SWD_LOGI(fmt, ...) printf("[INFO ] %s:%d -> " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif // defined(DEBUG - INFO)

#ifdef SWD_LOG_LEVEL_WARN
#define SWD_LOGW(fmt, ...) printf("[WARN ] %s:%d -> " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif // defined(DEBUG - WARN)

#ifdef SWD_LOG_LEVEL_ERROR
#define SWD_LOGE(fmt, ...) printf("[ERROR] %s:%d -> " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif // defined(DEBUG - ERROR)

#endif // (defined(SWD_ENABLE_LOGGING) && defined(SWD_USE_CUSTOM_PRINT))

/* Any unused logging functions shouldn't be compiled */
#ifndef SWD_LOGV
#define SWD_LOGV(fmt, ...)
#endif

#ifndef SWD_LOGD
#define SWD_LOGD(fmt, ...)
#endif

#ifndef SWD_LOGI
#define SWD_LOGI(fmt, ...)
#endif

#ifndef SWD_LOGW
#define SWD_LOGW(fmt, ...)
#endif

#ifndef SWD_LOGE
#define SWD_LOGE(fmt, ...)
#endif

#endif // __SWD_LOG_H