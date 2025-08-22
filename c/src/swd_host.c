
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include "swd_dap.h"
#include "swd_dap_port.h"
#include "swd_err.h"
#include "swd_host.h"
#include "swd_log.h"
#include "swd_target_register.h"

#include "_swd_arch_addr_decl.h"

#define REGRDY_READ_RETRY_CNT (10)

#define FPB_ADDR_ERROR ((uint32_t)-1)

/*
 * All Host API function calls check if not null and not started
 */
#define SWD_HOST_CHECK_STARTED                                                                     \
    do {                                                                                           \
        SWD_ASSERT(host != NULL);                                                                  \
        if (host->is_stopped) {                                                                    \
            return SWD_HOST_NOT_STARTED;                                                           \
        }                                                                                          \
    } while (0);

/*
 * Error checking is done multiple times. Can only be used if the function returns a swd_err_t
 */
#define SWD_HOST_RETURN_IF_NON_OK(err)                                                             \
    do {                                                                                           \
        if ((err) != SWD_OK) {                                                                     \
            return err;                                                                            \
        }                                                                                          \
    } while (0)

enum FPB_VERSION {
    FPB_VERSION_1 = 0x0,
    FPB_VERSION_2 = 0x1,
};

/*
 * @brief Setup the DAP to a set of initial configurations
 */
swd_err_t _swd_host_setup_dap_configs(swd_host_t *host);

/*
 * @brief Setup any required architecture debug attributes
 */
swd_err_t _swd_host_enable_arch_configs(swd_host_t *host);

/*
 * @brief Read any detected architecture defined RO Debug attributes
 * @note Number of literal comparators is not implemented
 */
swd_err_t _swd_host_detect_arch_configs(swd_host_t *host);

swd_err_t _swd_host_dap_port_write_masked(swd_host_t *host, swd_dap_port_t port, uint32_t data,
                                          uint32_t mask);

uint32_t _fpb_cmp_encode_bkpt(uint32_t addr, uint8_t fp_version);
uint32_t _fpb_cmp_decode_bkpt(uint32_t cmp, uint8_t fp_version);

void swd_host_init(swd_host_t *host) {
    SWD_ASSERT(host != NULL);

    host->is_stopped = false;
}

void swd_host_set_dap(swd_host_t *host, swd_dap_t *dap) {
    SWD_ASSERT(host != NULL);
    SWD_ASSERT(dap != NULL);

    host->dap = dap;
}

// start host
swd_err_t swd_host_start(swd_host_t *host) {
    SWD_ASSERT(host != NULL);
    SWD_ASSERT(host->dap != NULL);

    SWD_INFO("Starting Host");

    host->is_stopped = false;
    swd_err_t err = swd_dap_start(host->dap);
    if (err != SWD_OK) {
        SWD_WARN("Host experienced an error starting the DAP: %s", swd_err_as_str(err));
        return SWD_HOST_START_ERR;
    }

    err = _swd_host_setup_dap_configs(host);
    if (err != SWD_OK) {
        SWD_WARN("Host experienced an error configuring the DAP: %s", swd_err_as_str(err));
        return SWD_HOST_START_ERR;
    }

    err = _swd_host_detect_arch_configs(host);
    if (err != SWD_OK) {
        SWD_WARN("Host experienced an error detecting required architecture configurations: %s",
                 swd_err_as_str(err));
        return SWD_HOST_START_ERR;
    }

    err = _swd_host_enable_arch_configs(host);
    if (err != SWD_OK) {
        SWD_WARN("Host experienced an error enabling required architecture configurations: %s",
                 swd_err_as_str(err));
        return SWD_HOST_START_ERR;
    }

    return SWD_OK;
}

// stop host
swd_err_t swd_host_stop(swd_host_t *host) {
    SWD_ASSERT(host != NULL);
    SWD_ASSERT(host->dap != NULL);

    host->is_stopped = true;
    swd_err_t err = swd_dap_start(host->dap);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

// haltTarget
swd_err_t swd_host_halt_target(swd_host_t *host) {
    SWD_HOST_CHECK_STARTED

    // Sent a halt signal via DHSCR
    swd_err_t err = swd_host_memory_write_word(host, DHCSR, DBG_KEY | C_HALT | C_DEBUGEN);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_step_target(swd_host_t *host) {
    SWD_HOST_CHECK_STARTED

    bool is_halted;
    swd_err_t err = swd_host_is_target_halted(host, &is_halted);
    SWD_HOST_RETURN_IF_NON_OK(err);

    if (!is_halted) {
        return SWD_TARGET_NOT_HALTED;
    }

    // Special logging logic to indicate that a breakpoint was stepped over
#ifdef SWD_ENABLE_LOGGING
    uint32_t pc;
    uint32_t step_pc;
    err = swd_host_register_read(host, REG_DEBUG_RETURN_ADDRESS, &pc);
    SWD_HOST_RETURN_IF_NON_OK(err);

    // Send a step signal via DHSCR
    err = swd_host_memory_write_word(host, DHCSR, DBG_KEY | C_STEP | C_DEBUGEN);
    SWD_HOST_RETURN_IF_NON_OK(err);

    err = swd_host_register_read(host, REG_DEBUG_RETURN_ADDRESS, &step_pc);
    SWD_HOST_RETURN_IF_NON_OK(err);

    if (pc != step_pc) {
        return SWD_OK;
    }

    SWD_INFO("Stepping over breakpoint");
    SWD_INFO("Note the core might be in a spin loop");

#endif // SWD_ENABLE_LOGGING

    // Temporarily Disable Breakpoints
    err = swd_host_memory_write_word(host, FP_CTRL, KEY | ~ENABLE);
    SWD_HOST_RETURN_IF_NON_OK(err);

    // Send a step signal via DHSCR
    err = swd_host_memory_write_word(host, DHCSR, DBG_KEY | C_STEP | C_DEBUGEN);
    SWD_HOST_RETURN_IF_NON_OK(err);

    // Reenable breakpoints
    err = swd_host_memory_write_word(host, FP_CTRL, KEY | ENABLE);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_continue_target(swd_host_t *host) {
    SWD_HOST_CHECK_STARTED

    // Dont sent any signals to DHCSR to continue
    swd_err_t err = swd_host_memory_write_word(host, DHCSR, DBG_KEY | C_DEBUGEN);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_reset_target(swd_host_t *host) {
    SWD_HOST_CHECK_STARTED

    swd_err_t err = swd_host_continue_target(host);
    SWD_HOST_RETURN_IF_NON_OK(err);

    // Signal to the external system to request a Local reset (core + peripherals reset)
    err = swd_host_memory_write_word(host, AIRCR, VECTKEY | SYSRESETREQ);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return err;
}

swd_err_t swd_host_halt_reset_target(swd_host_t *host) {
    SWD_HOST_CHECK_STARTED

    // Ensure halting debug is enabled
    swd_err_t err = swd_host_memory_write_word(host, DHCSR, DBG_KEY | C_DEBUGEN);
    SWD_HOST_RETURN_IF_NON_OK(err);

    // Get DEMCR to toggle bit between resets
    uint32_t demcr;
    err = swd_host_memory_read_word(host, DEMCR, &demcr);
    SWD_HOST_RETURN_IF_NON_OK(err);

    // Enable Reset Vector Catch
    err = swd_host_memory_write_word(host, DEMCR, demcr | VC_CORERESET);
    SWD_HOST_RETURN_IF_NON_OK(err);

    // Signal to the external system to request a Local reset (core + peripherals reset)
    err = swd_host_memory_write_word(host, AIRCR, VECTKEY | SYSRESETREQ);
    SWD_HOST_RETURN_IF_NON_OK(err);

    // Clear Reset Vector Catch bit
    err = swd_host_memory_write_word(host, DEMCR, demcr & ~VC_CORERESET);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_is_target_halted(swd_host_t *host, bool *is_halted) {
    SWD_HOST_CHECK_STARTED

    uint32_t dhcsr;
    swd_err_t err = swd_host_memory_read_word(host, DHCSR, &dhcsr);
    SWD_HOST_RETURN_IF_NON_OK(err);

    *is_halted = dhcsr & S_HALTED;
    return SWD_OK;
}

swd_err_t swd_host_memory_write_word(swd_host_t *host, uint32_t addr, uint32_t data) {
    SWD_HOST_CHECK_STARTED
    SWD_ASSERT(host->dap != NULL);

    if (addr & 0x3) {
        SWD_ERROR("Word writes need to be word aligned");
        return SWD_TARGET_INVALID_ADDR;
    }

    swd_err_t err = swd_dap_port_write(host->dap, AP_TAR, addr);
    SWD_HOST_RETURN_IF_NON_OK(err);

    err = swd_dap_port_write(host->dap, AP_DRW, data);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_memory_write_word_block(swd_host_t *host, uint32_t start_addr,
                                           uint32_t *data_buf, uint32_t bufsz) {
    SWD_HOST_CHECK_STARTED
    SWD_ASSERT(host->dap != NULL);

    if (start_addr & 0x3) {
        SWD_ERROR("Word writes need to be word aligned");
        return SWD_TARGET_INVALID_ADDR;
    }

    // Enable Auto increment TAR
    SWD_DEBUG("Enabling auto-increment TAR");
    swd_err_t err = _swd_host_dap_port_write_masked(host, AP_CSW, 0x10, 0x30);
    SWD_HOST_RETURN_IF_NON_OK(err);

    err = swd_dap_port_write(host->dap, AP_TAR, start_addr);
    SWD_HOST_RETURN_IF_NON_OK(err);

    for (uint32_t i = 0; i < bufsz; i++) {
        err = swd_dap_port_write(host->dap, AP_DRW, data_buf[i]);
        if (err != SWD_OK) {
            SWD_WARN("Write failed at data buffer index %" PRIu32, i);
            return err;
        }
    }

    // Disable Auto increment TAR
    SWD_DEBUG("Disabling auto-increment TAR");
    err = _swd_host_dap_port_write_masked(host, AP_CSW, 0x00, 0x30);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_memory_write_byte_block(swd_host_t *host, uint32_t start_addr, uint8_t *data_buf,
                                           uint32_t bufsz) {
    SWD_HOST_CHECK_STARTED
    SWD_ASSERT(host->dap != NULL);

    // Enable Auto increment TAR
    SWD_DEBUG("Enabling auto-increment TAR");
    swd_err_t err = _swd_host_dap_port_write_masked(host, AP_CSW, 0x10, 0x30);
    SWD_HOST_RETURN_IF_NON_OK(err);

    uint32_t end_addr = start_addr + bufsz;
    uint8_t byte_offset = start_addr & 0x3;

    if (byte_offset) {
        uint32_t word_buf;
        uint32_t aligned_start = start_addr & ~0x3;
        uint8_t shamt;
        SWD_DEBUG("Reading word 0x%08" PRIx32 " non word aligned byte transfer", aligned_start);
        // Write the bytes which are non word aligned in the front
        // Changes the buffer reference to a word aligned byte
        err = swd_host_memory_read_word(host, aligned_start, &word_buf);
        SWD_HOST_RETURN_IF_NON_OK(err);

        for (uint8_t i = byte_offset; i < 4; i++) {
            shamt = 8 * i;
            word_buf = (word_buf & ~(0xFF << shamt)) | (*(data_buf++) << shamt);
        }
        err = swd_host_memory_write_word(host, aligned_start, word_buf);
        // Adjust new start address to write word aligned memory block
        start_addr += (4 - byte_offset);
        bufsz -= (4 - byte_offset);
    }

    // Write as much data which is word aligned
    swd_dap_port_write(host->dap, AP_TAR, start_addr);
    for (uint32_t i = 0; i < bufsz / 4; i++) {
        err = swd_dap_port_write(host->dap, AP_DRW,
                                 data_buf[0] | (data_buf[1] << 8) | (data_buf[2] << 16) |
                                     (data_buf[3] << 24));
        SWD_HOST_RETURN_IF_NON_OK(err);
        // The starting address of the trailing bytes needs to be tracked
        // start_addr += 4;
        data_buf += 4;
    }

    byte_offset = end_addr & 0x3;
    if (byte_offset) {
        // There isnt enough bytes at the end to make a full word
        uint32_t word_buf;
        uint32_t aligned_end = end_addr & ~0x3;
        uint8_t shamt;
        SWD_DEBUG("Reading word 0x%08" PRIx32 " non word aligned byte transfer", aligned_end);
        err = swd_host_memory_read_word(host, aligned_end, &word_buf);
        SWD_HOST_RETURN_IF_NON_OK(err);
        for (uint8_t i = 0; i < byte_offset; i++) {
            shamt = 8 * i;
            word_buf = (word_buf & ~(0xFF << shamt)) | (*(data_buf++) << shamt);
        }
        err = swd_host_memory_write_word(host, aligned_end, word_buf);
        SWD_HOST_RETURN_IF_NON_OK(err);
    }

    // Disable Auto increment TAR
    SWD_DEBUG("Disabling auto-increment TAR");
    err = _swd_host_dap_port_write_masked(host, AP_CSW, 0x00, 0x30);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_memory_read_word(swd_host_t *host, uint32_t addr, uint32_t *data) {
    SWD_HOST_CHECK_STARTED
    SWD_ASSERT(host->dap != NULL);

    if (addr & 0x3) {
        SWD_ERROR("Word writes need to be word aligned");
        return SWD_TARGET_INVALID_ADDR;
    }

    swd_err_t err = swd_dap_port_write(host->dap, AP_TAR, addr);
    SWD_HOST_RETURN_IF_NON_OK(err);

    err = swd_dap_port_read(host->dap, AP_DRW, data);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_memory_read_word_block(swd_host_t *host, uint32_t start_addr, uint32_t *data_buf,
                                          uint32_t bufsz) {
    SWD_HOST_CHECK_STARTED
    SWD_ASSERT(host->dap != NULL);

    if (start_addr & 0x3) {
        SWD_ERROR("Word writes need to be word aligned");
        return SWD_TARGET_INVALID_ADDR;
    }

    // Enable Auto increment TAR
    SWD_DEBUG("Enabling auto-increment TAR");
    swd_err_t err = _swd_host_dap_port_write_masked(host, AP_CSW, 0x10, 0x30);
    SWD_HOST_RETURN_IF_NON_OK(err);

    err = swd_dap_port_write(host->dap, AP_TAR, start_addr);
    SWD_HOST_RETURN_IF_NON_OK(err);

    for (uint32_t i = 0; i < bufsz; i++) {
        err = swd_dap_port_read(host->dap, AP_DRW, data_buf + i);
        if (err != SWD_OK) {
            SWD_WARN("Read failed at data buffer index %" PRIu32, i);
            return err;
        }
    }

    // Disable Auto increment TAR
    SWD_DEBUG("Disabling auto-increment TAR");
    err = _swd_host_dap_port_write_masked(host, AP_CSW, 0x00, 0x30);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_memory_read_byte_block(swd_host_t *host, uint32_t start_addr, uint8_t *data_buf,
                                          uint32_t bufsz) {
    SWD_HOST_CHECK_STARTED
    SWD_ASSERT(host->dap != NULL);

    // Enable Auto increment TAR
    SWD_DEBUG("Enabling auto-increment TAR");
    swd_err_t err = _swd_host_dap_port_write_masked(host, AP_CSW, 0x10, 0x30);
    SWD_HOST_RETURN_IF_NON_OK(err);

    // TODO

    // Disable Auto increment TAR
    SWD_DEBUG("Disabling auto-increment TAR");
    err = _swd_host_dap_port_write_masked(host, AP_CSW, 0x00, 0x30);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_register_read(swd_host_t *host, swd_target_register_t reg, uint32_t *data) {
    SWD_HOST_CHECK_STARTED

    bool is_halted;
    swd_err_t err = swd_host_is_target_halted(host, &is_halted);
    SWD_HOST_RETURN_IF_NON_OK(err);

    if (!is_halted) {
        return SWD_TARGET_NOT_HALTED;
    }

    uint32_t regsel = swd_target_register_as_regsel(reg, true);
    err = swd_host_memory_write_word(host, DCRSR, regsel);

    uint32_t dhcsr;
    int32_t retry_count = REGRDY_READ_RETRY_CNT;
    do {
        err = swd_host_memory_read_word(host, DHCSR, &dhcsr);
        SWD_HOST_RETURN_IF_NON_OK(err);

        // Register is availible in DCRDR
        if (dhcsr & S_REGRDY) {
            err = swd_host_memory_read_word(host, DCRDR, data);
            SWD_HOST_RETURN_IF_NON_OK(err);
            return SWD_OK;
        }
    } while ((retry_count--) > 0);

    return SWD_ERR;
}

swd_err_t swd_host_register_write(swd_host_t *host, swd_target_register_t reg, uint32_t data) {
    SWD_HOST_CHECK_STARTED

    bool is_halted;
    swd_err_t err = swd_host_is_target_halted(host, &is_halted);
    SWD_HOST_RETURN_IF_NON_OK(err);

    if (!is_halted) {
        return SWD_TARGET_NOT_HALTED;
    }

    err = swd_host_memory_write_word(host, DCRDR, data);
    SWD_HOST_RETURN_IF_NON_OK(err);

    uint32_t dhcsr;
    uint32_t regsel = swd_target_register_as_regsel(reg, false);
    int32_t retry_count = REGRDY_READ_RETRY_CNT;
    do {
        err = swd_host_memory_read_word(host, DHCSR, &dhcsr);
        SWD_HOST_RETURN_IF_NON_OK(err);

        // Register is availible in DCRDR
        if (dhcsr & S_REGRDY) {
            err = swd_host_memory_write_word(host, DCRSR, regsel);
            SWD_HOST_RETURN_IF_NON_OK(err);
            return SWD_OK;
        }
    } while ((retry_count--) > 0);

    return SWD_ERR;
}

swd_err_t swd_host_add_breakpoint(swd_host_t *host, uint32_t addr) {
    SWD_HOST_CHECK_STARTED

    if (host->_fpb_version == FPB_VERSION_1 && addr >= SRAM_BASE_ADDR) {
        SWD_WARN("FPB V1 does not breakpoint signals beyond the ROM reigon");
        return SWD_TARGET_INVALID_ADDR;
    }

    // Scan all registers
    // Find the first empty breakpoint to write to
    // Find if the breakpoint exists
    // Cry if there are no more breakpoints
    swd_err_t err;
    uint32_t fp_cmp_w_addr;
    uint32_t fp_cmpn_addr;
    uint32_t fp_cmpn_data;
    uint32_t encoded_addr = _fpb_cmp_encode_bkpt(addr, host->_fpb_version);
    bool valid_addr_found = false;

    if (encoded_addr == FPB_ADDR_ERROR) {
        SWD_ERROR("Cannot encode 0x%08" PRIx32 " as a breakpoint address", addr);
        return SWD_TARGET_INVALID_ADDR;
    }
    for (uint32_t i = 0; i < host->code_cmp_cnt; i++) {
        fp_cmpn_addr = FP_CMPN + (4 * i);
        err = swd_host_memory_read_word(host, fp_cmpn_addr, &fp_cmpn_data);
        SWD_HOST_RETURN_IF_NON_OK(err);

        // Find the first valid spot (a comparator which is disabled)
        if (!valid_addr_found && ((fp_cmpn_data & 0x1) == 0)) {
            valid_addr_found = true;
            fp_cmp_w_addr = fp_cmpn_addr;
        }

        if (fp_cmpn_data == encoded_addr) {
            SWD_INFO("Requested breakpoint address 0x%08" PRIx32 " already exists", addr);
            return SWD_OK;
        }
    }

    if (!valid_addr_found) {
        return SWD_TARGET_NO_MORE_BKPT;
    }

    err = swd_host_memory_write_word(host, fp_cmp_w_addr, encoded_addr);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t swd_host_remove_breakpoint(swd_host_t *host, uint32_t addr) {
    SWD_HOST_CHECK_STARTED

    if (host->_fpb_version == FPB_VERSION_1 && addr >= SRAM_BASE_ADDR) {
        SWD_WARN("FPB V1 does not breakpoint signals beyond the ROM reigon");
        return SWD_TARGET_INVALID_ADDR;
    }

    swd_err_t err;
    uint32_t fp_cmp_addr;
    uint32_t fp_cmp_data;
    uint32_t encoded = _fpb_cmp_encode_bkpt(addr, host->_fpb_version);
    for (uint32_t i = 0; i < host->code_cmp_cnt; i++) {
        fp_cmp_addr = FP_CMPN + (4 * i);
        err = swd_host_memory_read_word(host, fp_cmp_addr, &fp_cmp_data);
        SWD_HOST_RETURN_IF_NON_OK(err);

        // Delete if match
        if (fp_cmp_data == encoded) {
            err = swd_host_memory_write_word(host, fp_cmp_addr, 0x0);
            SWD_HOST_RETURN_IF_NON_OK(err);
            return SWD_OK;
        }
    }

    return SWD_TARGET_INVALID_ADDR;
}

swd_err_t swd_host_clear_breakpoints(swd_host_t *host) {
    SWD_HOST_CHECK_STARTED

    swd_err_t err;
    uint32_t addr;
    for (uint32_t i = 0; i < host->code_cmp_cnt; i++) {
        addr = FP_CMPN + (4 * i);
        err = swd_host_memory_write_word(host, addr, 0x0);
        if (err != SWD_OK) {
            SWD_WARN("Could not clear breakpoint at address 0x%08" PRIx32, addr);
        }
    }

    return SWD_OK;
}

swd_err_t swd_host_get_breakpoint_count(swd_host_t *host, uint32_t *bkpt_cnt) {
    SWD_HOST_CHECK_STARTED

    *bkpt_cnt = host->code_cmp_cnt;

    return SWD_OK;
}

swd_err_t swd_host_get_breakpoints(swd_host_t *host, uint32_t *buf, uint32_t bufsz,
                                   uint32_t *rdcnt) {
    SWD_HOST_CHECK_STARTED

    if (bufsz < host->code_cmp_cnt) {
        SWD_WARN("Less space was provided than the maximum number of possible breakpoints. Given: "
                 "%" PRIu32 ", Possible: %" PRIu8,
                 bufsz, host->code_cmp_cnt);
    }
    bufsz = (bufsz < host->code_cmp_cnt) ? bufsz : host->code_cmp_cnt;

    *rdcnt = 0;
    swd_err_t err;
    uint32_t fp_cmp_addr;
    uint32_t fp_cmp_data;
    uint32_t decoded;
    for (uint32_t i = 0; i < host->code_cmp_cnt; i++) {
        fp_cmp_addr = FP_CMPN + (4 * i);
        err = swd_host_memory_read_word(host, fp_cmp_addr, &fp_cmp_data);
        SWD_HOST_RETURN_IF_NON_OK(err);

        decoded = _fpb_cmp_decode_bkpt(fp_cmp_data, host->_fpb_version);
        if (decoded == FPB_ADDR_ERROR) {
            SWD_WARN("Issue decoded comparator address at 0x%08" PRIx32, fp_cmp_addr);
        } else if (decoded != 0x0) {
            buf[(*rdcnt)++] = decoded;
        }
    }

    return SWD_OK;
}

swd_err_t _swd_host_setup_dap_configs(swd_host_t *host) {
    // Set transfers to word (CSW.Size = 0x2)
    SWD_INFO("Setting transfers to word");
    SWD_INFO("Setting address auto-increment to false");
    // uint32_t csw;
    // swd_err_t err = swd_dap_port_read(host->dap, AP_CSW, &csw);
    // SWD_HOST_RETURN_IF_NON_OK(err);
    // err = swd_dap_port_write(host->dap, AP_CSW, (csw & ~0x37) | 0x2); // Size = 0b010 (word),
    // AddrInc = 0b00 (no inc) SWD_HOST_RETURN_IF_NON_OK(err);

    // Size = 0b010 (word), AddrInc = 0b00 (no inc)
    swd_err_t err = _swd_host_dap_port_write_masked(host, AP_CSW, 0x02, 0x37);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

swd_err_t _swd_host_enable_arch_configs(swd_host_t *host) {
    // Enable the Flash Patch and Breakpoint unit
    SWD_INFO("Enabling FPB Unit");
    uint32_t fp_ctrl;
    swd_err_t err;
    swd_host_memory_write_word(host, FP_CTRL, KEY | ENABLE);
    err = swd_host_memory_read_word(host, FP_CTRL, &fp_ctrl);
    SWD_HOST_RETURN_IF_NON_OK(err);

    if (!(fp_ctrl & ENABLE)) {
        SWD_WARN("FPB unit failed to set ENABLE");
        return SWD_HOST_NOT_STARTED;
    }

    return SWD_OK;
}

swd_err_t _swd_host_detect_arch_configs(swd_host_t *host) {
    // Read FPB version
    uint32_t fp_ctrl;
    swd_err_t err = swd_host_memory_read_word(host, FP_CTRL, &fp_ctrl);
    SWD_HOST_RETURN_IF_NON_OK(err);

    host->_fpb_version = (uint8_t)(fp_ctrl >> 28);
    const char *v_str;
    switch (host->_fpb_version) {
    case FPB_VERSION_1:
        v_str = "v1";
        break;
    case FPB_VERSION_2:
        v_str = "v2";
        break;
    default:
        SWD_ERROR("Cannot detect FPB version");
        return SWD_HOST_START_ERR;
    }
    SWD_INFO("Detected FPB version: %s", v_str);

    // Get the number of code comparators (breakpoint addresses)
    host->code_cmp_cnt = (uint8_t)((fp_ctrl & 0x7000) >> 0x8) | ((fp_ctrl & 0xF0) >> 0x4);
    SWD_INFO("Detected number of code comparators (HW Breakpoints): %" PRIu8, host->code_cmp_cnt);
    SWD_INFO("Detected number of literal comparators (FP Remaps): %" PRIu8,
             (uint8_t)((fp_ctrl & 0xF00) >> 0x8));
    SWD_INFO("Note that literal comparators are not used");

    return SWD_OK;
}

swd_err_t _swd_host_dap_port_write_masked(swd_host_t *host, swd_dap_port_t port, uint32_t data,
                                          uint32_t mask) {
    uint32_t rd_data;
    swd_err_t err = swd_dap_port_read(host->dap, port, &rd_data);
    SWD_HOST_RETURN_IF_NON_OK(err);
    err = swd_dap_port_write(host->dap, port, (rd_data & ~mask) | (data & mask));
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}

uint32_t _fpb_cmp_encode_bkpt(uint32_t addr, uint8_t fp_version) {
    if (addr & 0x1) {
        SWD_WARN("Cannot encode 0x%08" PRIx32, addr);
        return FPB_ADDR_ERROR;
    }
    switch (fp_version) {
    case FPB_VERSION_1:
        return (addr & ~0xC0000003) | (addr & 0x2 ? 0x80000000 : 0x40000000) | ENABLE;
    case FPB_VERSION_2:
        return addr | ENABLE;
    default:
        SWD_WARN("Uknown FPB Version %" PRIu8, fp_version);
        return FPB_ADDR_ERROR;
    }
}

uint32_t _fpb_cmp_decode_bkpt(uint32_t cmp, uint8_t fp_version) {
    if (cmp == 0x0) {
        return 0x0;
    }

    switch (fp_version) {
    case FPB_VERSION_1: {
        uint8_t replace = cmp >> 30;
        switch (replace) {
        case 0b00:
            SWD_WARN("cmp value 0x%08" PRIx32 " is an FP Remap", cmp);
            return FPB_ADDR_ERROR;
        case 0b01:
            return cmp & ~0xC0000003;
        case 0b10:
            return (cmp & ~0xC0000003) | 0x2;
        case 0b11:
            SWD_WARN("comp value 0x%08" PRIx32
                     " maps to two addresses. This host does not manage this behavior",
                     cmp);
            return FPB_ADDR_ERROR;
            break;
        default:
            return FPB_ADDR_ERROR; // Unreachable
        }
        break;
    }
    case FPB_VERSION_2:
        if (!(cmp & 0x1)) {
            SWD_WARN("comp value 0x%08" PRIx32 " does not have its BE bit set", cmp);
            return FPB_ADDR_ERROR;
        }
        return cmp & ~0x1;
    default:
        SWD_WARN("Uknown FPB Version %" PRIu8, fp_version);
        return FPB_ADDR_ERROR;
    }
}
