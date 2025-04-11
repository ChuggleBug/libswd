/**
 * libswd.h
 * Implementation for a minimum viable debugger for ARM ADI v5.0+ devices
 */

#ifndef __LIBSWD_H
#define __LIBSWD_H

// Basic Driver Library
// Host needs to define how to set pin signals
#include "driver.h"

// Utility Port Declarations
// Defines debug and access port options
#include "packet.h"

// SWJ-DP Library
// Allows communication to the target
// requires a driver to send signals
#include "dap.h"

// Logging utility
// Provides simple serial output which 
// can be used anywhere by the library
#include "logger.h"

#endif // __LIBSWD_H