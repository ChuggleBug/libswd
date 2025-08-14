
#ifndef __SWD_REGISTERS_H
#define __SWD_REGISTERS_H

#include <stdint.h>

namespace swd::target {

// Constants defined by ARMv7-M Architectural Manual
enum class REG : uint32_t {
    // Core registers R0-R12
    R0 = 0b0000000,
    R1 = 0b0000001,
    R2 = 0b0000010,
    R3 = 0b0000011,
    R4 = 0b0000100,
    R5 = 0b0000101,
    R6 = 0b0000110,
    R7 = 0b0000111,
    R8 = 0b0001000,
    R9 = 0b0001001,
    R10 = 0b0001010,
    R11 = 0b0001011,
    R12 = 0b0001100,

    // Special registers
    SP = 0b0001101, // Current SP
    LR = 0b0001110, // Link Register

    // The address of the instruction which will be execution
    // the instant the processor exits the debug state
    DebugReturnAddress = 0b0001111,

    xPSR = 0b0010000,

    MSP = 0b0010001, // Main Stack Pointer
    PSP = 0b0010010, // Process Stack Pointer

    CONTROL_FAULTMASK_BASEPRI_PRIMASK = 0b0010100,

    // Floating-point status/control
    FPSCR = 0b0100001,

    // Floating-point registers S0–S31
    S0 = 0b1000000,
    S1 = 0b1000001,
    S2 = 0b1000010,
    S3 = 0b1000011,
    S4 = 0b1000100,
    S5 = 0b1000101,
    S6 = 0b1000110,
    S7 = 0b1000111,
    S8 = 0b1001000,
    S9 = 0b1001001,
    S10 = 0b1001010,
    S11 = 0b1001011,
    S12 = 0b1001100,
    S13 = 0b1001101,
    S14 = 0b1001110,
    S15 = 0b1001111,
    S16 = 0b1010000,
    S17 = 0b1010001,
    S18 = 0b1010010,
    S19 = 0b1010011,
    S20 = 0b1010100,
    S21 = 0b1010101,
    S22 = 0b1010110,
    S23 = 0b1010111,
    S24 = 0b1011000,
    S25 = 0b1011001,
    S26 = 0b1011010,
    S27 = 0b1011011,
    S28 = 0b1011100,
    S29 = 0b1011101,
    S30 = 0b1011110,
    S31 = 0b1011111
};

} // namespace swd::target

#endif // __SWD_REGISTERS_H