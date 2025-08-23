
#ifndef __SWD_TARGET_REGISTER_H
#define __SWD_TARGET_REGISTER_H

#include <stdbool.h>

// TODO: arch specific registers
typedef enum _swd_target_register_t {
    // Core registers R0-R12
    REG_R0,
    REG_R1,
    REG_R2,
    REG_R3,
    REG_R4,
    REG_R5,
    REG_R6,
    REG_R7,
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,

    // Special registers
    REG_SP, // Current SP
    REG_LR, // Link Register

    // The address of the instruction which will be execution
    // the instant the processor exits the debug state
    REG_DEBUG_RETURN_ADDRESS,

    REG_XPSR,

    REG_MSP, // Main Stack Pointer
    REG_PSP, // Process Stack Pointer

    REG_CONTROL_FAULTMASK_BASEPRI_PRIMASK,

    // Floating-point status/control
    REG_FPSCR,

    // Floating-point registers S0–S31
    REG_S0,
    REG_S1,
    REG_S2,
    REG_S3,
    REG_S4,
    REG_S5,
    REG_S6,
    REG_S7,
    REG_S8,
    REG_S9,
    REG_S10,
    REG_S11,
    REG_S12,
    REG_S13,
    REG_S14,
    REG_S15,
    REG_S16,
    REG_S17,
    REG_S18,
    REG_S19,
    REG_S20,
    REG_S21,
    REG_S22,
    REG_S23,
    REG_S24,
    REG_S25,
    REG_S26,
    REG_S27,
    REG_S28,
    REG_S29,
    REG_S30,
    REG_S31,
} swd_target_register_t;

uint32_t swd_target_register_as_regsel(swd_target_register_t reg, bool is_read);
const char* swd_target_register_as_str(swd_target_register_t reg);

#endif // __SWD_TARGET_REGISTER_H