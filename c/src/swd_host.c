
#include <stdlib.h>

#include "swd_host.h"
#include "swd_err.h"
#include "swd_target_register.h"

#include "_swd_arch_addr_decl.h"

#define REGRDY_READ_RETRY_CNT (10)

/*
 * All Host API function calls check if not null and not started
 */
#define SWD_HOST_CHECK_STARTED do {     \
    SWD_ASSERT(host != NULL);           \
    if (host->is_stopped) {             \
        return SWD_HOST_NOT_STARTED;    \
    }} while (0);

/*
 * Error checking is done multiple times. Can only be used if the function returns a swd_err_t
 */
#define SWD_HOST_RETURN_IF_NON_OK(err) do { \
    if ( (err) != SWD_OK ) {                \
        return err;                         \
    } } while (0)


/*
 * @brief Setup the DAP to a set of initial configurations
 */
swd_err_t _swd_host_setup_dap_configs(swd_host_t *host);

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
        SWD_WARN("Host experienced an error on start: %s", swd_err_as_str(err));
        return SWD_HOST_START_ERR;
    }

    err = _swd_host_setup_dap_configs(host);
    if (err != SWD_OK) {
        SWD_WARN("Host experienced an error on start: %s", swd_err_as_str(err));
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

    // TODO: Handle Breakpoint logic
    // Send a step signal via DHSCR
    swd_err_t err = swd_host_memory_write_word(host, DHCSR, DBG_KEY | C_STEP | C_DEBUGEN);
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

    swd_err_t err = swd_dap_port_write(host->dap, AP_TAR, addr);
    SWD_HOST_RETURN_IF_NON_OK(err);

    err = swd_dap_port_write(host->dap, AP_DRW, data);
    SWD_HOST_RETURN_IF_NON_OK(err);
    
    return SWD_OK;
}
swd_err_t swd_host_memory_write_word_block(swd_host_t *host, uint32_t start_addr, uint32_t *data_buf, uint32_t bufsz) {
    SWD_HOST_CHECK_STARTED

    return SWD_OK;
}
swd_err_t swd_host_memory_write_byte_block(swd_host_t *host, uint32_t start_addr, uint8_t *data_buf, uint32_t bufsz) {
    SWD_HOST_CHECK_STARTED

    return SWD_OK;
}
swd_err_t swd_host_memory_read_word(swd_host_t *host, uint32_t addr, uint32_t* data) {
    SWD_HOST_CHECK_STARTED
    SWD_ASSERT(host->dap != NULL);

    swd_err_t err = swd_dap_port_write(host->dap, AP_TAR, addr);
    SWD_HOST_RETURN_IF_NON_OK(err);

    err = swd_dap_port_read(host->dap, AP_DRW, data);
    SWD_HOST_RETURN_IF_NON_OK(err);
    
    return SWD_OK;
}
swd_err_t swd_host_memory_read_word_block(swd_host_t *host, uint32_t start_addr, uint32_t *data_buf, uint32_t bufsz) {
    SWD_HOST_CHECK_STARTED

    return SWD_OK;
}
swd_err_t swd_host_memory_read_byte_block(swd_host_t *host, uint32_t start_addr, uint8_t *data_buf, uint32_t bufsz) {
    SWD_HOST_CHECK_STARTED

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

    return SWD_OK;
}
swd_err_t swd_host_remove_breakpoints(swd_host_t *host, uint32_t addr) {
    SWD_HOST_CHECK_STARTED

    return SWD_OK;
}
swd_err_t swd_host_get_breakpoint_count(swd_host_t *host, uint32_t *bkpt_cnt) {
    SWD_HOST_CHECK_STARTED

    return SWD_OK;
}
swd_err_t swd_host_get_breakpoints(swd_host_t *host, uint32_t *buf, uint32_t bufsz) {
    SWD_HOST_CHECK_STARTED

    return SWD_OK;
}

swd_err_t _swd_host_setup_dap_configs(swd_host_t *host) {
    // Set transfers to word (CSW.Size = 0x2)
    SWD_INFO("Setting transfers to word");
    uint32_t csw;
    swd_err_t err = swd_dap_port_read(host->dap, AP_CSW, &csw);
    SWD_HOST_RETURN_IF_NON_OK(err);
    err = swd_dap_port_write(host->dap, AP_CSW, (csw & ~0x7) | 0x2);
    SWD_HOST_RETURN_IF_NON_OK(err);

    return SWD_OK;
}