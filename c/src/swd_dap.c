
#include <stdbool.h>
#include <stdlib.h>

#include "driver/swd_driver.h"
#include "swd_conf.h"
#include "swd_dap.h"
#include "swd_err.h"
#include "swd_log.h"

#define ACK_OK (0b001)
#define ACK_WAIT (0b010)
#define ACK_FAULT (0b100)

#define DEFAULT_SEL_VALUE (0xbeefcafe)
#define DEFAULT_CSW_VALUE (-1)

#define RW_RETRY_COUNT (10)

#define SELECT_CTRLSEL_MASK (0x01)
#define SELECT_APBANKSEL_MASK (0xF0)

#ifdef SWD_DISABLE_UNDEFINED_PORT
#define BLOCK_UNDEFINED_PORT(port)                                                                 \
    do {                                                                                           \
        if (port == AP_DB0 || port == AP_DB1 || port == AP_DB2 || port == AP_DB3 ||                \
            port == AP_BASE) {                                                                     \
            SWD_ERROR("**************************************************************************" \
                      "*****");                                                                    \
            SWD_ERROR("Under certain misconfigurations, the selected port (%s) can lead to ",      \
                      swd_dap_port_as_str(port));                                                  \
            SWD_ERROR("undefined behavior. To prevent this, this port has been disabled To ");     \
            SWD_ERROR(                                                                             \
                "allow usage of this port, unselect 'SWD_DISABLE_UNDEFINED_PORT' in swd_conf.h");  \
            SWD_ERROR("**************************************************************************" \
                      "*****");                                                                    \
            return SWD_ERR;                                                                        \
        }                                                                                          \
    } while (0)
#else
#define BLOCK_UNDEFINED_PORT(port)

#endif // SWD_DISABLE_UNDEFINED_PORT

static uint8_t _get_paity_bit(uint32_t value);

static swd_err_t _swd_dap_reset_line(swd_dap_t *dap);
static swd_err_t _swd_dap_post_line_reset_init_configs(swd_dap_t *dap);

static void _swd_dap_idle_short(swd_dap_t *dap);

static swd_err_t _swd_dap_port_read_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data);
static swd_err_t _swd_dap_port_read_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data);

static swd_err_t _swd_dap_port_write_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t data);
static swd_err_t _swd_dap_port_write_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t data);

static swd_err_t _swd_dap_port_set_banksel(swd_dap_t *dap, swd_dap_port_t port);

static swd_err_t _swd_dap_port_read_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t *data,
                                                uint32_t retry_count);
static swd_err_t _swd_dap_port_write_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t data,
                                                 uint32_t retry_count);

static void _swd_dap_handle_fault(swd_dap_t *dap);
static void _swd_dap_handle_error(swd_dap_t *dap);

/*
 * Initialization utilities:
 */
void swd_dap_init(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);

    dap->is_stopped = true;
    dap->ap_error = false;
    dap->is_little_endian = true;

    dap->data_size = DEFAULT_CSW_VALUE;
    dap->addr_int_bits = DEFAULT_CSW_VALUE;
}

void swd_dap_set_driver(swd_dap_t *dap, swd_driver_t *driver) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(driver != NULL);

    dap->driver = driver;
}

swd_err_t swd_dap_start(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    swd_driver_start(dap->driver);

    dap->is_stopped = false;
    _swd_dap_reset_line(dap);
    _swd_dap_post_line_reset_init_configs(dap);

    return SWD_OK;
}

void swd_dap_stop(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    dap->is_stopped = true;
    swd_driver_stop(dap->driver);
}

void swd_dap_reset(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    // Start the driver if needed
    if (dap->is_stopped) {
        swd_driver_start(dap->driver);
    }

    dap->is_stopped = false;
    _swd_dap_reset_line(dap);
    _swd_dap_post_line_reset_init_configs(dap);
}

void swd_dap_set_jtag_to_swd(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    swd_dap_reset_target(dap);
    swd_driver_write_bits(dap->driver, 0xE79E, 16);
    swd_dap_reset_target(dap);

    _swd_dap_idle_short(dap);
}

void swd_dap_reset_target(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    swd_driver_write_bits(dap->driver, 0xFFFFFFFF, 32);
    swd_driver_write_bits(dap->driver, 0xFFFFFFFF, 32);
}

swd_err_t swd_dap_port_read(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(data != NULL);

    if (dap->is_stopped) {
        SWD_WARN("Attempting to read from a stopped DAP");
        return SWD_ERR;
    }

    BLOCK_UNDEFINED_PORT(port);

    SWD_DEBUG("Reading port %s", swd_dap_port_as_str(port));

    if (swd_dap_port_is_DP(port)) {
        return _swd_dap_port_read_dp(dap, port, data);
    } else {
        return _swd_dap_port_read_ap(dap, port, data);
    }
}

swd_err_t swd_dap_port_write(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    SWD_ASSERT(dap != NULL);

    if (dap->is_stopped) {
        SWD_WARN("Attempting to write to from a stopped DAP");
        return SWD_ERR;
    }

    BLOCK_UNDEFINED_PORT(port);

    SWD_DEBUG("Writing 0x%08lx to %s", data, swd_dap_port_as_str(port));

    if (swd_dap_port_is_DP(port)) {
        return _swd_dap_port_write_dp(dap, port, data);
    } else {
        return _swd_dap_port_write_ap(dap, port, data);
    }
}

static void _swd_dap_idle_short(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    swd_driver_write_bits(dap->driver, 0x0, 2);
}

static swd_err_t _swd_dap_port_read_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data) {
    // Need to set CTRLSEL to 1 for DP_WCR
    if (port == DP_WCR) {
        uint32_t select_data;
        swd_dap_port_read(dap, DP_SELECT, &select_data);
        swd_dap_port_write(dap, DP_SELECT, select_data | 0x1);
    }

    uint8_t packet = swd_dap_port_as_packet(port, true);
    if (_swd_dap_port_read_from_packet(dap, packet, data, RW_RETRY_COUNT) != SWD_OK) {
        return SWD_ERR;
    }

    // Unset the CTRLSEL bit if needed
    // A second read is needed if SELECT was the port which was written to
    if (port == DP_WCR) {
        uint32_t select_data;
        swd_dap_port_read(dap, DP_SELECT, &select_data);
        swd_dap_port_write(dap, DP_SELECT, select_data & ~0x1);
    }

    return SWD_OK;
}

static swd_err_t _swd_dap_port_read_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data) {
    if (_swd_dap_port_set_banksel(dap, port) != SWD_OK) {
        SWD_ERROR("Could not update APBANKSEL");
        return SWD_ERR;
    }

    uint8_t packet = swd_dap_port_as_packet(port, true);

    // This packet will be ignored
    uint32_t buf;
    _swd_dap_port_read_from_packet(dap, packet, &buf, RW_RETRY_COUNT);

    // If there was an an error
    if ((swd_dap_port_read(dap, DP_RDBUFF, data) != SWD_OK) || dap->ap_error) {
        dap->ap_error = false;
        return SWD_ERR;
    }

    return SWD_OK;
}

static swd_err_t _swd_dap_port_write_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    // Need to set CTRLSEL to 1 for DP_WCR
    if (port == DP_WCR) {
        uint32_t select_data;
        swd_dap_port_read(dap, DP_SELECT, &select_data);
        swd_dap_port_write(dap, DP_SELECT, select_data | 0x1);
    }

    uint8_t packet = swd_dap_port_as_packet(port, false);
    if (_swd_dap_port_write_from_packet(dap, packet, data, RW_RETRY_COUNT) != SWD_OK) {
        return SWD_ERR;
    }

    // Unset the CTRLSEL bit if needed
    // A second read is needed if SELECT was the port which was written to
    if (port == DP_WCR) {
        uint32_t select_data;
        swd_dap_port_read(dap, DP_SELECT, &select_data);
        swd_dap_port_write(dap, DP_SELECT, select_data & ~0x1);
    }

    return SWD_OK;
}

static swd_err_t _swd_dap_port_write_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    _swd_dap_port_set_banksel(dap, port);

    uint8_t packet = swd_dap_port_as_packet(port, false);

    if ((_swd_dap_port_write_from_packet(dap, packet, data, RW_RETRY_COUNT) != SWD_OK) ||
        dap->ap_error) {
        return SWD_ERR;
    }

    // Delay for AP to process the write
    _swd_dap_idle_short(dap);
    _swd_dap_idle_short(dap);

    return SWD_OK;
}

static swd_err_t _swd_dap_port_read_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t *data,
                                                uint32_t retry_count) {
    if (retry_count == 0) {
        SWD_WARN("Retry count for sending packet exceeded");
        return SWD_ERR;
    }

    // Perform packet request and ACK read
    swd_driver_write_bits(dap->driver, packet, 8);
    swd_driver_turnaround(dap->driver);
    uint8_t ack = swd_driver_read_bits(dap->driver, 3);

    switch (ack) {
    case ACK_OK: {
        // Only continue to read if OK
        uint32_t rd_data = swd_driver_read_bits(dap->driver, 32);
        uint8_t rd_parity = swd_driver_read_bits(dap->driver, 1);
        swd_driver_turnaround(dap->driver);

        // Validate parity from rdata
        if (rd_parity != _get_paity_bit(rd_parity)) {
            SWD_INFO("Data received was OK, but had invalid parity. Retrying");
            return _swd_dap_port_read_from_packet(dap, packet, data, retry_count - 1);
        }

        *data = rd_data;
        return SWD_OK;
    }
    case ACK_WAIT:
        SWD_DEBUG("DAP sent back a WAIT. Retrying");
        swd_driver_turnaround(dap->driver);
        return _swd_dap_port_read_from_packet(dap, packet, data, retry_count - 1);
    case ACK_FAULT:
        SWD_DEBUG("DAP sent back a FAULT. Handling");
        swd_driver_turnaround(dap->driver);
        _swd_dap_handle_fault(dap);
        return _swd_dap_port_read_from_packet(dap, packet, data, retry_count - 1);
        break;
    default:
        SWD_DEBUG("DAP sent back a UNKOWN. Fallback to error");
        break;
    }
    _swd_dap_handle_error(dap);
    return SWD_ERR;
}

static swd_err_t _swd_dap_port_write_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t data,
                                                 uint32_t retry_count) {
    if (retry_count == 0) {
        SWD_WARN("Retry count for sending packet exceeded");
        return SWD_ERR;
    }

    // Perform a write sequence
    swd_driver_write_bits(dap->driver, packet, 8);
    swd_driver_turnaround(dap->driver);
    uint8_t ack = swd_driver_read_bits(dap->driver, 3);
    swd_driver_turnaround(dap->driver);

    switch (ack) {
    case ACK_OK: {
        // Perform write only if OK
        uint8_t parity = _get_paity_bit(data);
        swd_driver_write_bits(dap->driver, data, 32);
        swd_driver_write_bits(dap->driver, parity, 1);

        // Check if WDATAERR is set
        uint32_t ctrlstat;
        if (swd_dap_port_read(dap, DP_CTRL_STAT, &ctrlstat) != SWD_OK) {
            return SWD_ERR;
        }

        if (ctrlstat & 0x80) {
            SWD_WARN("WDATAERR detected. Resending");
            return _swd_dap_port_write_from_packet(dap, packet, data, retry_count - 1);
        }
        return SWD_OK;
    }
    case ACK_WAIT:
        SWD_DEBUG("DAP sent back a WAIT. Retrying");
        return _swd_dap_port_write_from_packet(dap, packet, data, retry_count - 1);
    case ACK_FAULT:
        SWD_DEBUG("DAP sent back a FAULT. Handling");
        _swd_dap_handle_fault(dap);
        return _swd_dap_port_write_from_packet(dap, packet, data, retry_count - 1);
    default:
        SWD_DEBUG("DAP sent back a UNKOWN. Fallback to error");
        break;
    }
    _swd_dap_handle_error(dap);
    return SWD_ERR;
}

static uint8_t _get_paity_bit(uint32_t value) {
    uint8_t parity = 0;
    while (value) {
        parity ^= (value & 1);
        value >>= 1;
    }
    return parity;
}

static void _swd_dap_handle_fault(swd_dap_t *dap) {
    // Cases that trigger a fault
    //  - Partiy error in the wdata
    //  - Error in AP transaction
    uint32_t ctrlstat;
    if (swd_dap_port_read(dap, DP_CTRL_STAT, &ctrlstat) != SWD_OK) {
        SWD_WARN("Could not read CTRL/STAT to handle FAULT");
        return;
    }

    if (ctrlstat & 0x80) { // WDATAERR
        SWD_INFO("Cause: Parity Error in the previous write data sent.");
        swd_dap_port_write(dap, DP_ABORT, 0x8); // WDERRCLR
    } else if (ctrlstat & 0x20) {               // STICKYERR
        SWD_INFO("Cause: Error in the previous in the AP transcation");
        swd_dap_port_write(dap, DP_ABORT, 0x4); // STKERRCLR
        dap->ap_error = true;
    } else {
        SWD_INFO("Cause: Unown Fault");
    }
}

static void _swd_dap_handle_error(swd_dap_t *dap) {
    // Cases that cause an error typically cuase a desync between the target and the host
    // a line reset is tried before stopping the host
    // For the most part, when an error ACK (a lack of one) is read, a single turnaround
    // should be expected to check and see if the target exists at all
    SWD_WARN("Resetting line due to an potentially out of sync DAP");
    _swd_dap_reset_line(dap);

    // This read sequence WILL NOT FAIL unless the DAP is not connected
    swd_driver_write_bits(dap->driver, swd_dap_port_as_packet(DP_IDCODE, true), 8);
    swd_driver_turnaround(dap->driver);
    uint8_t ack = swd_driver_read_bits(dap->driver, 3);
    swd_driver_turnaround(dap->driver);
    // The read data is not needed here
    swd_driver_read_bits(dap->driver, 32);
    swd_driver_read_bits(dap->driver, 1);
    swd_driver_turnaround(dap->driver);

    if (ack != ACK_OK) {
        SWD_ERROR("Cannot handle DAP error. Stopping");
        swd_dap_stop(dap);
        return;
    }

    SWD_WARN("Target resynced after error. Packet dropped");
    _swd_dap_post_line_reset_init_configs(dap);
    return;
}

static swd_err_t _swd_dap_reset_line(swd_dap_t *dap) {

#ifdef SWD_CONFIG_AUTO_JTAG_SWITCH
    swd_dap_set_jtag_to_swd(dap);
#else
    swd_dap_reset_target(dap);
#endif // SWD_CONFIG_AUTO_JTAG_SWITCH

    return SWD_OK;
}

static swd_err_t _swd_dap_post_line_reset_init_configs(swd_dap_t *dap) {
    // An IDCODE read is required after a reset
    uint32_t buf; // Not needed
    swd_dap_port_read(dap, DP_IDCODE, &buf);

    // Power on AP
    SWD_INFO("Initializing Access Port");
    if (swd_dap_port_write(dap, DP_CTRL_STAT, 0x50000000) != SWD_OK) {
        SWD_ERROR("Access Port failed to initialize");
        return SWD_ERR;
    }

    uint32_t data;
    _swd_dap_idle_short(dap);
    if (swd_dap_port_read(dap, DP_CTRL_STAT, &data) == SWD_OK) {
        if (!(data & 0xf0000000)) {
            SWD_ERROR("Could not verify CTRL/STAT for AP was written to");
            return SWD_ERR;
        } else {
            SWD_DEBUG("AP power on ACK received!");
        }
    }

    // Clear Abort Errors
    if (swd_dap_port_write(dap, DP_ABORT, 0x1F) != SWD_OK) {
        SWD_WARN("Could not reset active errors on reset");
    }

    return SWD_OK;
}

static swd_err_t _swd_dap_port_set_banksel(swd_dap_t *dap, swd_dap_port_t port) {
    uint32_t apbanksel = swd_dap_port_as_apbanksel_bits(port);
    if (apbanksel == SELECT_APBANKSEL_ERR) {
        return SWD_ERR;
    }

    // Read APBANKSEL. only write if a change is needed
    uint32_t select;
    if (swd_dap_port_read(dap, DP_SELECT, &select) != SWD_OK) {
        return SWD_ERR;
    }

    if (apbanksel != (select & SELECT_APBANKSEL_MASK)) {
        // Mask out APBANKSEL first, then only consdier CTRLSEL and APBANKSEL
        select = ((select & ~SELECT_APBANKSEL_MASK) | apbanksel) &
                 (SELECT_APBANKSEL_MASK | SELECT_CTRLSEL_MASK);
        return swd_dap_port_write(dap, DP_SELECT, select);
    }
    return SWD_OK;
}