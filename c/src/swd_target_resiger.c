
#include <stdint.h>

#include "swd_target_register.h"
#include "swd_log.h"

// DCRSR fields
#define REG_W          ((uint32_t)0x10000) // Enable write to register
#define REG_R          ((uint32_t)0x0)     // Mostly for readability

uint32_t swd_target_register_as_regsel(swd_target_register_t reg, bool is_read) {
    uint32_t val = 0;

    switch (reg) {
        case REG_R0:  val = 0b0000000; break;
        case REG_R1:  val = 0b0000001; break;
        case REG_R2:  val = 0b0000010; break;
        case REG_R3:  val = 0b0000011; break;
        case REG_R4:  val = 0b0000100; break;
        case REG_R5:  val = 0b0000101; break;
        case REG_R6:  val = 0b0000110; break;
        case REG_R7:  val = 0b0000111; break;
        case REG_R8:  val = 0b0001000; break;
        case REG_R9:  val = 0b0001001; break;
        case REG_R10: val = 0b0001010; break;
        case REG_R11: val = 0b0001011; break;
        case REG_R12: val = 0b0001100; break;
        case REG_SP:  val = 0b0001101; break;
        case REG_LR:  val = 0b0001110; break;
        case REG_DEBUG_RETURN_ADDRESS: val = 0b0001111; break;
        case REG_XPSR: val = 0b0010000; break;
        case REG_MSP:  val = 0b0010001; break;
        case REG_PSP:  val = 0b0010010; break;
        case REG_CONTROL_FAULTMASK_BASEPRI_PRIMASK: val = 0b0010100; break;
        case REG_FPSCR: val = 0b0100001; break;

        // Floating point registers follow a pattern: REG_S0 starts at 0b1000000
        case REG_S0:  val = 0b1000000; break;
        case REG_S1:  val = 0b1000001; break;
        case REG_S2:  val = 0b1000010; break;
        case REG_S3:  val = 0b1000011; break;
        case REG_S4:  val = 0b1000100; break;
        case REG_S5:  val = 0b1000101; break;
        case REG_S6:  val = 0b1000110; break;
        case REG_S7:  val = 0b1000111; break;
        case REG_S8:  val = 0b1001000; break;
        case REG_S9:  val = 0b1001001; break;
        case REG_S10: val = 0b1001010; break;
        case REG_S11: val = 0b1001011; break;
        case REG_S12: val = 0b1001100; break;
        case REG_S13: val = 0b1001101; break;
        case REG_S14: val = 0b1001110; break;
        case REG_S15: val = 0b1001111; break;
        case REG_S16: val = 0b1010000; break;
        case REG_S17: val = 0b1010001; break;
        case REG_S18: val = 0b1010010; break;
        case REG_S19: val = 0b1010011; break;
        case REG_S20: val = 0b1010100; break;
        case REG_S21: val = 0b1010101; break;
        case REG_S22: val = 0b1010110; break;
        case REG_S23: val = 0b1010111; break;
        case REG_S24: val = 0b1011000; break;
        case REG_S25: val = 0b1011001; break;
        case REG_S26: val = 0b1011010; break;
        case REG_S27: val = 0b1011011; break;
        case REG_S28: val = 0b1011100; break;
        case REG_S29: val = 0b1011101; break;
        case REG_S30: val = 0b1011110; break;
        case REG_S31: val = 0b1011111; break;
        default: 
            SWD_WARN("Unkown register. Using %s instead", swd_target_register_as_str(REG_DEBUG_RETURN_ADDRESS)); 
            val = 0b0001111; break;
    }

    val |= (is_read ? REG_R : REG_W);

    return val;
}


const char* swd_target_register_as_str(swd_target_register_t reg) {
    switch (reg) {
        case REG_R0: return "R0";
        case REG_R1: return "R1";
        case REG_R2: return "R2";
        case REG_R3: return "R3";
        case REG_R4: return "R4";
        case REG_R5: return "R5";
        case REG_R6: return "R6";
        case REG_R7: return "R7";
        case REG_R8: return "R8";
        case REG_R9: return "R9";
        case REG_R10: return "R10";
        case REG_R11: return "R11";
        case REG_R12: return "R12";
        case REG_SP: return "SP";
        case REG_LR: return "LR";
        case REG_DEBUG_RETURN_ADDRESS: return "Debug Return Address (PC)";
        case REG_XPSR: return "XPSR";
        case REG_MSP: return "MSP";
        case REG_PSP: return "PSP";
        case REG_CONTROL_FAULTMASK_BASEPRI_PRIMASK: return "CONTROL/FAULTMASK/BASEPRI/PRIMASK";
        case REG_FPSCR: return "FPSCR";

        case REG_S0: return "S0"; case REG_S1: return "S1"; case REG_S2: return "S2"; case REG_S3: return "S3";
        case REG_S4: return "S4"; case REG_S5: return "S5"; case REG_S6: return "S6"; case REG_S7: return "S7";
        case REG_S8: return "S8"; case REG_S9: return "S9"; case REG_S10: return "S10"; case REG_S11: return "S11";
        case REG_S12: return "S12"; case REG_S13: return "S13"; case REG_S14: return "S14"; case REG_S15: return "S15";
        case REG_S16: return "S16"; case REG_S17: return "S17"; case REG_S18: return "S18"; case REG_S19: return "S19";
        case REG_S20: return "S20"; case REG_S21: return "S21"; case REG_S22: return "S22"; case REG_S23: return "S23";
        case REG_S24: return "S24"; case REG_S25: return "S25"; case REG_S26: return "S26"; case REG_S27: return "S27";
        case REG_S28: return "S28"; case REG_S29: return "S29"; case REG_S30: return "S30"; case REG_S31: return "S31";
        default:
            return "UNKNOWN";
    }
}
