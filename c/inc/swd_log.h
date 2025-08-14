
#ifndef __SWD_LOG_H
#define __SWD_LOG_H

#include "swd_conf.h"

// Check if at least one of the log levels were defined if logging is enabled
#if defined(SWD_ENABLE_LOGGING) && !(defined(SWD_LOG_LEVEL_DEBUG) || defined(SWD_LOG_LEVEL_INFO) || defined(SWD_LOG_LEVEL_WARN) || defined(SWD_LOG_LEVEL_ERROR))
#warning "Logging was enabled, but no log level was set. Select one in 'swd_conf.h'"
#endif // SWD_ENABLE_LOGGING && ...

#if (defined(SWD_ENABLE_LOGGING) && defined(SWD_USE_CUSTOM_PRINT))
/* User has provided a custom implementation somewhere*/


#if defined(SWD_LOG_LEVEL_DEBUG)
extern void _swd_log_debug(char* fmt, ...);
#define SWD_DEBUG(fmt, ...)     _swd_log_debug(fmt, ##__VA_ARGS__)
#endif // defined(DEBUG)

#if defined(SWD_LOG_LEVEL_INFO) || defined(SWD_LOG_LEVEL_DEBUG)
extern void _swd_log_info(char* fmt, ...);
#define SWD_INFO(fmt, ...)      _swd_log_info(fmt, ##__VA_ARGS__)
#endif // defined(DEBUG - INFO)

#if defined(SWD_LOG_LEVEL_WARN) || defined(SWD_LOG_LEVEL_INFO) || defined(SWD_LOG_LEVEL_DEBUG)
extern void _swd_log_warn(char* fmt, ...);
#define SWD_WARN(fmt, ...)      _swd_log_warn(fmt, ##__VA_ARGS__)
#endif // defined(DEBUG - WARN)

#if defined(SWD_LOG_LEVEL_ERROR) || defined(SWD_LOG_LEVEL_WARN) || defined(SWD_LOG_LEVEL_INFO) || defined(SWD_LOG_LEVEL_DEBUG)
extern void _swd_log_error(char* fmt, ...);
#define SWD_ERROR(fmt, ...)     _swd_log_error(fmt, ##__VA_ARGS__)
#endif // defined(DEBUG - ERROR)

#elif defined(SWD_ENABLE_LOGGING)
/* A default printf is being used*/
#include <stdio.h>

#if defined(SWD_LOG_LEVEL_DEBUG)
#define SWD_DEBUG(fmt, ...)     printf("[DEBUG] %s:%d -> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#endif // defined(DEBUG)

#if defined(SWD_LOG_LEVEL_INFO) || defined(SWD_LOG_LEVEL_DEBUG)
#define SWD_INFO(fmt, ...)      printf("[INFO ] %s:%d -> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#endif // defined(DEBUG - INFO)

#if defined(SWD_LOG_LEVEL_WARN) || defined(SWD_LOG_LEVEL_INFO) || defined(SWD_LOG_LEVEL_DEBUG)
#define SWD_WARN(fmt, ...)      printf("[WARN ] %s:%d -> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#endif // defined(DEBUG - WARN)

#if defined(SWD_LOG_LEVEL_ERROR) || defined(SWD_LOG_LEVEL_WARN) || defined(SWD_LOG_LEVEL_INFO) || defined(SWD_LOG_LEVEL_DEBUG)
#define SWD_ERROR(fmt, ...)     printf("[ERROR] %s:%d -> " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#endif // defined(DEBUG - ERROR)

#endif // (defined(SWD_ENABLE_LOGGING) && defined(SWD_USE_CUSTOM_PRINT))

/* Any unused logging functions shouldn't be compiled */
#ifndef SWD_DEBUG
#define SWD_DEBUG(fmt, ...)
#endif 

#ifndef SWD_INFO
#define SWD_INFO(fmt, ...)
#endif 

#ifndef SWD_WARN
#define SWD_WARN(fmt, ...)
#endif 

#ifndef SWD_ERROR
#define SWD_ERROR(fmt, ...)
#endif 

#endif // __SWD_LOG_H