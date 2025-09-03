
#include <stdint.h>
#include <strings.h>

#include "swd_target_register.h"
#include "swd_log.h"

// DCRSR fields
#define REG_W          ((uint32_t)0x10000) // Enable write to register
#define REG_R          ((uint32_t)0x0)     // Mostly for readability

/* 
 * Register to string mapping
 */
static const struct reg_str_mapping {
    swd_target_register_t reg;
    const char *name;
} mappings[] = {
    { REG_R0,  "R0" },
    { REG_R1,  "R1" },
    { REG_R2,  "R2" },
    { REG_R3,  "R3" },
    { REG_R4,  "R4" },
    { REG_R5,  "R5" },
    { REG_R6,  "R6" },
    { REG_R7,  "R7" },
    { REG_R8,  "R8" },
    { REG_R9,  "R9" },
    { REG_R10, "R10" },
    { REG_R11, "R11" },
    { REG_R12, "R12" },
    { REG_SP,  "SP" },
    { REG_LR,  "LR" },
    { REG_DEBUG_RETURN_ADDRESS, "Debug Return Address (PC)" },
    { REG_XPSR, "XPSR" },
    { REG_MSP,  "MSP" },
    { REG_PSP,  "PSP" },
    { REG_CONTROL_FAULTMASK_BASEPRI_PRIMASK, "CONTROL/FAULTMASK/BASEPRI/PRIMASK (CFBP)" },
    { REG_FPSCR, "FPSCR" },

    { REG_S0,  "S0" },  { REG_S1,  "S1" },  { REG_S2,  "S2" },  { REG_S3,  "S3" },
    { REG_S4,  "S4" },  { REG_S5,  "S5" },  { REG_S6,  "S6" },  { REG_S7,  "S7" },
    { REG_S8,  "S8" },  { REG_S9,  "S9" },  { REG_S10, "S10" }, { REG_S11, "S11" },
    { REG_S12, "S12" }, { REG_S13, "S13" }, { REG_S14, "S14" }, { REG_S15, "S15" },
    { REG_S16, "S16" }, { REG_S17, "S17" }, { REG_S18, "S18" }, { REG_S19, "S19" },
    { REG_S20, "S20" }, { REG_S21, "S21" }, { REG_S22, "S22" }, { REG_S23, "S23" },
    { REG_S24, "S24" }, { REG_S25, "S25" }, { REG_S26, "S26" }, { REG_S27, "S27" },
    { REG_S28, "S28" }, { REG_S29, "S29" }, { REG_S30, "S30" }, { REG_S31, "S31" },
};


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
            SWD_LOGW("Unkown register");
            return DCRSR_REGSEL_ERR;
    }

    val |= (is_read ? REG_R : REG_W);

    return val;
}


const char* swd_target_register_as_str(swd_target_register_t reg) {
    for (uint32_t i = 0; i < sizeof(mappings) / sizeof(struct reg_str_mapping); i++) {
        if (mappings[i].reg == reg) {
            return mappings[i].name;
        }
    }
    return "UKNOWN";
}

bool swd_target_register_from_str(const char* str, swd_target_register_t *reg) {
    if (strcasecmp("PC", str) == 0) {
        *reg = REG_DEBUG_RETURN_ADDRESS;
        return true;
    }
    if (strcasecmp("CFBP", str) == 0) {
        *reg = REG_CONTROL_FAULTMASK_BASEPRI_PRIMASK;
        return true;
    }
    for (uint32_t i = 0; i < sizeof(mappings) / sizeof(struct reg_str_mapping); i++) {
        if (strcasecmp(mappings[i].name, str) == 0) {
            *reg = mappings[i].reg;
            return true;
        }
    }
    return false;
}
