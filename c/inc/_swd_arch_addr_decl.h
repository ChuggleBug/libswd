
#ifndef __SWD_ARCH_ADDR_DECL_H
#define __SWD_ARCH_ADDR_DECL_H

#include <stdint.h>

// System control register
#define AIRCR ((uint32_t)0xE000ED0C) // Application Interrupt and Reset Control Register
#define DFSR ((uint32_t)0xE000ED30)  // Debug Fault Status Register

// Debug Registers
#define DHCSR ((uint32_t)0xE000EDF0) // Debug Halting Control and Status Register
#define DCRSR ((uint32_t)0xE000EDF4) // Debug Core Register Selector Register
#define DCRDR ((uint32_t)0xE000EDF8) // Debug Core Register Data Register
#define DEMCR ((uint32_t)0xE000EDFC) // Debug Event and Monitor Control Register

// Flash Patch and Breakpoint unit
#define FP_CTRL ((uint32_t)0xE0002000)
#define FP_REMAP ((uint32_t)0xE0002004)
#define FP_CMPN ((uint32_t)0xE0002008)

// Constants for convenience
// DHCSR fields
#define DBG_KEY ((uint32_t)0xA05F0000) // Special Sequence required for DHCSR
#define C_DEBUGEN ((uint32_t)0x1)      // Bit required for DHCSR control bit
#define C_HALT ((uint32_t)0x2)         // Halt debugger
#define C_STEP ((uint32_t)0x4)         // Step debugger
#define C_MASKINTS ((uint32_t)0x8)     // Mask interrupts when debugging
#define S_HALTED ((uint32_t)0x20000)   // Halt status
#define S_REGRDY ((uint32_t)0x10000)   // DCRDR status checking

// DEMCR fields
#define VC_CORERESET ((uint32_t)0x1) // Catch a local reset

// AIRCR fields
#define VECTKEY ((uint32_t)0x05FA0000) // Special sequence required for AIRCR
#define SYSRESETREQ ((uint32_t)0x4)    // Local reset core + peripherals
#define VECTRESET ((uint32_t)0x1)      // Local reset core (and maybe peripherals)

// FP_CTRL fields
#define KEY ((uint32_t)0x2)
// FP_COMPn fields
#define ENABLE ((uint32_t)0x1)

// FP_REMAP fields
#define RMPSPT ((uint32_t)0x20000000)

// ARMv7-M Memory Regions
#define CODE_BASE_ADDR ((uint32_t)0x0)
#define CODE_END_ADDR ((uint32_t)0x1FFFFFFF)
#define SRAM_BASE_ADDR ((uint32_t)0x20000000)
#define SRAM_END_ADDR ((uint32_t)0x3FFFFFFF)

#endif // __SWD_ARCH_ADDR_DECL_H