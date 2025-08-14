

#ifndef __SWD_CONF_H
#define __SWD_CONF_H

/* Enable runtime assertions using SWD_ASSERT */
#define SWD_DO_RUNTIME_ASSERT

/* Automatically switch from JTAG to SWD using the special configuration sequence */
#define SWD_CONFIG_AUTO_JTAG_SWITCH

/* Enable Built in logging of swd activity */
#define SWD_ENABLE_LOGGING

#ifdef SWD_ENABLE_LOGGING

/*
 * A custom printer should be used for the logger.
 * A printf-like function should be provided, as in it takes a
 * string format as well as vaardic arguments.
 * If this is not set stdio printf is used
 */
// #define SWD_USE_CUSTOM_PRINT

/* Log level used. At least one should be set */
#define SWD_LOG_LEVEL_DEBUG
// #define SWD_LOG_LEVEL_INFO
// #define SWD_LOG_LEVEL_WARN
// #define SWD_LOG_LEVEL_ERROR

#endif // SWD_ENABLE_LOGGING

#endif // __SWD_CONF_H