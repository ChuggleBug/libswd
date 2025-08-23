
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "driver/swd_driver.h"
#include "swd_conf.h"
#include "swd_dap.h"
#include "swd_err.h"
#include "swd_log.h"

/*
 * ACK bit sequences, where LSB is the
 * first bit which appears on the wire
 */
#define ACK_OK (0b001)
#define ACK_WAIT (0b010)
#define ACK_FAULT (0b100)

#define RW_RETRY_COUNT (10)

#define SELECT_CTRLSEL_MASK (0x01)
#define SELECT_APBANKSEL_MASK (0xF0)

#ifdef SWD_DISABLE_UNDEFINED_PORT
#define BLOCK_UNDEFINED_PORT(port)                                                                 \
    do {                                                                                           \
        if (port == AP_DB0 || port == AP_DB1 || port == AP_DB2 || port == AP_DB3 ||                \
            port == AP_BASE) {                                                                     \
            SWD_LOGE("**************************************************************************"  \
                     "*****");                                                                     \
            SWD_LOGE("Under certain misconfigurations, the selected port (%s) can lead to ",       \
                     swd_dap_port_as_str(port));                                                   \
            SWD_LOGE("undefined behavior. To prevent this, this port has been disabled To ");      \
            SWD_LOGE(                                                                              \
                "allow usage of this port, unselect 'SWD_DISABLE_UNDEFINED_PORT' in swd_conf.h");  \
            SWD_LOGE("**************************************************************************"  \
                     "*****");                                                                     \
            return SWD_DAP_UNDEFINED_PORT;                                                         \
        }                                                                                          \
    } while (0)
#else
#define BLOCK_UNDEFINED_PORT(port)

#endif // SWD_DISABLE_UNDEFINED_PORT

/*
 * @brief Generate bit for even parity
 */
static uint8_t _get_pairty_bit(uint32_t value);

/*
 * @brief Perform a line reset to resync communication between a host and a target
 * @note on some targets a JTAG to SWD sequence is required. This option can be set
 *          with SWD_CONFIG_AUTO_JTAG_SWITCH in "swd_conf.h"
 * @return Status of resest operation
 */
static swd_err_t _swd_dap_reset_line(swd_dap_t *dap);

/*
 * @brief Ensure a proper DAP initialization has been made by
 *          (1) Checking if IDCODE is readable
 *          (2) Ensure that the Access Port is powered on
 * @return Status of DAP initialization
 */
static swd_err_t _swd_dap_setup(swd_dap_t *dap);

/*
 * @brief in some cases, SWCLK needs to be cycled in order for the DAP to
 *          "process" operations such as an AP write
 */
static void _swd_dap_idle_short(swd_dap_t *dap);

/*
 * @brief To able to write to some AP registers, APBANKSEL needs to set. Manages SELECT
 * register to allow bank selection
 */
static swd_err_t _swd_dap_port_set_banksel(swd_dap_t *dap, swd_dap_port_t port);

/*
 * @brief Wrapper functions for DP/AP read/write operations. In order for AP read
 *          and writes to process in the same function call, a different set of
 *          operations need to be done
 */

static swd_err_t _swd_dap_port_read_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data);
static swd_err_t _swd_dap_port_write_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t data);

static swd_err_t _swd_dap_port_read_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data);
static swd_err_t _swd_dap_port_write_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t data);

/*
 * @brief Low level read and write operations to communicate with the dap. In order to try
 *          and complete a read/write in the same function call, a retry count is used
 *           to handle backoff.
 * @note In some cases where the dap is unesponsive or experiecnes an error, the entire dap
 *          instance can be stopped and will require a restart
 */

static swd_err_t _swd_dap_port_read_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t *data,
                                                uint32_t retry_count);
static swd_err_t _swd_dap_port_write_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t data,
                                                 uint32_t retry_count);

/*
 * @brief To try and maintain a working connection to the dap, specific handlers for
 *          FAULT and ERROR responses
 */

static void _swd_dap_handle_fault(swd_dap_t *dap);
static void _swd_dap_handle_error(swd_dap_t *dap);

/*
 * Initialization utilities:
 */
void swd_dap_init(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);

    dap->is_stopped = true;
    dap->_ap_error = false;
}

void swd_dap_set_driver(swd_dap_t *dap, swd_driver_t *driver) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(driver != NULL);

    dap->driver = driver;
}

swd_err_t swd_dap_start(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    SWD_LOGI("Starting DAP");

    swd_driver_start(dap->driver);

    dap->is_stopped = false;

    if (_swd_dap_reset_line(dap) != SWD_OK) {
        SWD_LOGE("Cannot drive DAP. Stopping");
        swd_driver_stop(dap->driver);
        dap->is_stopped = true;
        return SWD_DAP_START_ERR;
    }

    if (_swd_dap_setup(dap) != SWD_OK) {
        SWD_LOGE("Cannot establish DAP connection. Stopping");
        swd_driver_stop(dap->driver);
        dap->is_stopped = true;
        return SWD_DAP_START_ERR;
    }

    return SWD_OK;
}

swd_err_t swd_dap_stop(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    dap->is_stopped = true;

    swd_driver_stop(dap->driver);

    return SWD_OK;
}

swd_err_t swd_dap_port_read(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(data != NULL);

    if (dap->is_stopped) {
        SWD_LOGW("Attempting to read from a stopped DAP");
        return SWD_DAP_NOT_STARTED;
    }

    if (!swd_dap_port_is_a_read_port(port)) {
        SWD_LOGW("Requested port (%s) is not allowed to be read from", swd_dap_port_as_str(port));
        return SWD_DAP_INVALID_PORT_OP;
    }

    BLOCK_UNDEFINED_PORT(port);

    // SWD_LOGV("Reading port %s", swd_dap_port_as_str(port));

    if (swd_dap_port_is_DP(port)) {
        return _swd_dap_port_read_dp(dap, port, data);
    } else {
        return _swd_dap_port_read_ap(dap, port, data);
    }
}

swd_err_t swd_dap_port_write(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    SWD_ASSERT(dap != NULL);

    if (dap->is_stopped) {
        SWD_LOGW("Attempting to read from a stopped DAP");
        return SWD_DAP_NOT_STARTED;
    }

    if (!swd_dap_port_is_a_write_port(port)) {
        SWD_LOGW("Requested port (%s) is not allowed to be written to", swd_dap_port_as_str(port));
        return SWD_DAP_INVALID_PORT_OP;
    }

    BLOCK_UNDEFINED_PORT(port);

    // SWD_LOGV("Writing 0x%08" PRIx32 " to %s", data, swd_dap_port_as_str(port));

    if (swd_dap_port_is_DP(port)) {
        return _swd_dap_port_write_dp(dap, port, data);
    } else {
        return _swd_dap_port_write_ap(dap, port, data);
    }
}

/*                                    */
/* PRIVATE FUCNTION DEFINITIONS BEGIN */
/*                                    */

static uint8_t _get_pairty_bit(uint32_t value) {
    uint8_t parity = 0;
    while (value) {
        parity ^= (value & 1);
        value >>= 1;
    }
    return parity;
}

static swd_err_t _swd_dap_reset_line(swd_dap_t *dap) {
    // General SWD line reset
    swd_driver_write_bits(dap->driver, 0xFFFFFFFF, 32);
    swd_driver_write_bits(dap->driver, 0xFFFFFFFF, 32);

#ifdef SWD_CONFIG_AUTO_JTAG_SWITCH
    swd_driver_write_bits(dap->driver, 0xE79E, 16); // Special JTAG to SWD key

    // Yet another reset
    swd_driver_write_bits(dap->driver, 0xFFFFFFFF, 32);
    swd_driver_write_bits(dap->driver, 0xFFFFFFFF, 32);

    // Some cycle time to ensure process completed
    _swd_dap_idle_short(dap);
#endif // SWD_CONFIG_AUTO_JTAG_SWITCH

    return SWD_OK;
}

static swd_err_t _swd_dap_setup(swd_dap_t *dap) {
    // An IDCODE read is required after a reset
    // Higher level operations cant be done since target dap might not even exist
    uint8_t packet = swd_dap_port_as_packet(DP_IDCODE, true);
    swd_driver_write_bits(dap->driver, packet, 8);
    swd_driver_turnaround(dap->driver);
    uint8_t ack = swd_driver_read_bits(dap->driver, 3);
    uint32_t idcode = swd_driver_read_bits(dap->driver, 32);
    uint32_t parity = swd_driver_read_bits(dap->driver, 1);
    swd_driver_turnaround(dap->driver);

    // IDCODE read MUST return an OK
    if (ack != ACK_OK) {
        SWD_LOGE("Cannot read IDCODE, no connection to target can be established");
        return SWD_DAP_START_ERR;
    }
    // Check if the IDCODE data is valid
    SWD_LOGI("IDCODE = 0x%08" PRIx32, idcode);
    if (parity != _get_pairty_bit(idcode)) {
        SWD_LOGE("IDCODE read, but parity sent is invalid");
        return SWD_DAP_START_ERR;
    }

    // Power on AP
    SWD_LOGD("Initializing Access Port");
    if (swd_dap_port_write(dap, DP_CTRL_STAT, 0x50000000) !=
        SWD_OK) { // CDBGPWRUPREQ | CSYSPWRUPREQ
        SWD_LOGE("Access Port failed to initialize");
        return SWD_DAP_START_ERR;
    }

    uint32_t data;
    _swd_dap_idle_short(dap);
    if (swd_dap_port_read(dap, DP_CTRL_STAT, &data) == SWD_OK) {
        if (!(data & 0xf0000000)) { // CSYSPWRUPACK |  CDBGPWRUPACK | CDBGPWRUPREQ | CSYSPWRUPREQ
            SWD_LOGE("Could not verify AP was powered on");
            return SWD_DAP_START_ERR;
        } else {
            SWD_LOGV("AP power on ACK received!");
        }
    }

    // Clear Abort Errors
    if (swd_dap_port_write(dap, DP_ABORT, 0x1F) != SWD_OK) {
        SWD_LOGW("Could not reset active errors on reset");
    }

    return SWD_OK;
}

static void _swd_dap_idle_short(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    swd_driver_write_bits(dap->driver, 0x0, 2);
}

static swd_err_t _swd_dap_port_set_banksel(swd_dap_t *dap, swd_dap_port_t port) {
    uint32_t apbanksel = swd_dap_port_as_apbanksel_bits(port);
    if (apbanksel == SELECT_APBANKSEL_ERR) {
        return SWD_ERR;
    }

    uint32_t select = apbanksel & SELECT_APBANKSEL_MASK;
    return swd_dap_port_write(dap, DP_SELECT, select);
}

static swd_err_t _swd_dap_port_read_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data) {
    // Need to set CTRLSEL to 1 for DP_WCR
    if (port == DP_WCR) {
        swd_dap_port_write(dap, DP_SELECT, 0x1);
    }

    uint8_t packet = swd_dap_port_as_packet(port, true);
    swd_err_t err;
    if ((err = _swd_dap_port_read_from_packet(dap, packet, data, RW_RETRY_COUNT)) != SWD_OK) {
        return err;
    }

    // Unset the CTRLSEL bit if needed
    // A second read is needed if SELECT was the port which was written to
    if (port == DP_WCR) {
        swd_dap_port_write(dap, DP_SELECT, 0x0);
    }

    return SWD_OK;
}

static swd_err_t _swd_dap_port_write_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    // Need to set CTRLSEL to 1 for DP_WCR
    if (port == DP_WCR) {
        swd_dap_port_write(dap, DP_SELECT, 0x1);
    }

    uint8_t packet = swd_dap_port_as_packet(port, false);
    swd_err_t err;
    if ((err = _swd_dap_port_write_from_packet(dap, packet, data, RW_RETRY_COUNT)) != SWD_OK) {
        return err;
    }

    // Unset the CTRLSEL bit if needed
    // A second read is needed if SELECT was the port which was written to
    if (port == DP_WCR) {
        swd_dap_port_write(dap, DP_SELECT, 0x0);
    }

    return SWD_OK;
}

static swd_err_t _swd_dap_port_read_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data) {
    swd_err_t err;
    if ((err = _swd_dap_port_set_banksel(dap, port)) != SWD_OK) {
        SWD_LOGE("Could not update APBANKSEL");
        return SWD_ERR;
    }

    uint8_t packet = swd_dap_port_as_packet(port, true);

    // This packet will be ignored
    uint32_t buf;
    _swd_dap_port_read_from_packet(dap, packet, &buf, RW_RETRY_COUNT);

    // If there was an an error
    swd_dap_port_read(dap, DP_RDBUFF, data);
    // Read to DP might have been ok, but an AP error might have been detected
    if (dap->_ap_error) {
        dap->_ap_error = false;
        return SWD_ERR;
    }

    return SWD_OK;
}

static swd_err_t _swd_dap_port_write_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    _swd_dap_port_set_banksel(dap, port);

    uint8_t packet = swd_dap_port_as_packet(port, false);

    _swd_dap_port_write_from_packet(dap, packet, data, RW_RETRY_COUNT);
    if (dap->_ap_error) {
        dap->_ap_error = false;
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
        SWD_LOGV("Retry count for sending packet exceeded");
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
        if (rd_parity != _get_pairty_bit(rd_parity)) {
            SWD_LOGV("Data received was OK, but had invalid parity. Retrying");
            return _swd_dap_port_read_from_packet(dap, packet, data, retry_count - 1);
        }

        *data = rd_data;
        return SWD_OK;
    }
    case ACK_WAIT:
        SWD_LOGV("DAP sent back a WAIT. Retrying");
        swd_driver_turnaround(dap->driver);
        return _swd_dap_port_read_from_packet(dap, packet, data, retry_count - 1);
    case ACK_FAULT:
        SWD_LOGD("DAP sent back a FAULT. Handling");
        swd_driver_turnaround(dap->driver);
        _swd_dap_handle_fault(dap);
        return _swd_dap_port_read_from_packet(dap, packet, data, retry_count - 1);
        break;
    default:
        SWD_LOGD("DAP sent back a UNKOWN. Fallback to error");
        break;
    }
    _swd_dap_handle_error(dap);
    return SWD_ERR;
}

static swd_err_t _swd_dap_port_write_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t data,
                                                 uint32_t retry_count) {
    if (retry_count == 0) {
        SWD_LOGW("Retry count for sending packet exceeded");
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
        uint8_t parity = _get_pairty_bit(data);
        swd_driver_write_bits(dap->driver, data, 32);
        swd_driver_write_bits(dap->driver, parity, 1);

        // Check if WDATAERR is set
        uint32_t ctrlstat;
        if (swd_dap_port_read(dap, DP_CTRL_STAT, &ctrlstat) != SWD_OK) {
            return SWD_ERR;
        }

        if (ctrlstat & 0x80) {
            SWD_LOGV("WDATAERR detected. Resending");
            return _swd_dap_port_write_from_packet(dap, packet, data, retry_count - 1);
        }
        return SWD_OK;
    }
    case ACK_WAIT:
        SWD_LOGV("DAP sent back a WAIT. Retrying");
        return _swd_dap_port_write_from_packet(dap, packet, data, retry_count - 1);
    case ACK_FAULT:
        SWD_LOGD("DAP sent back a FAULT. Handling");
        _swd_dap_handle_fault(dap);
        return _swd_dap_port_write_from_packet(dap, packet, data, retry_count - 1);
    default:
        SWD_LOGD("DAP sent back a UNKOWN. Fallback to error");
        break;
    }
    _swd_dap_handle_error(dap);
    return SWD_ERR;
}

static void _swd_dap_handle_fault(swd_dap_t *dap) {
    // Cases that trigger a fault
    //  - Partiy error in the wdata
    //  - Error in AP transaction
    uint32_t ctrlstat;
    if (swd_dap_port_read(dap, DP_CTRL_STAT, &ctrlstat) != SWD_OK) {
        SWD_LOGV("Could not read CTRL/STAT to handle FAULT");
        return;
    }

    if (ctrlstat & 0x80) { // WDATAERR
        SWD_LOGD("Cause: Parity Error in the previous write data sent.");
        swd_dap_port_write(dap, DP_ABORT, 0x8); // WDERRCLR
    } else if (ctrlstat & 0x20) {               // STICKYERR
        SWD_LOGD("Cause: Error in the previous in the AP transcation");
        swd_dap_port_write(dap, DP_ABORT, 0x4); // STKERRCLR
        dap->_ap_error = true;
    } else {
        SWD_LOGD("Cause: Unown Fault");
    }
}

static void _swd_dap_handle_error(swd_dap_t *dap) {
    // Cases that cause an error typically cuase a desync between the target and the host
    // a line reset is tried before stopping the host
    // For the most part, when an error ACK (a lack of one) is read, a single turnaround
    // should be expected to check and see if the target exists at all
    SWD_LOGW("Resetting line due to an potentially out of sync DAP");
    _swd_dap_reset_line(dap);

    if (_swd_dap_setup(dap) != ACK_OK) {
        SWD_LOGE("Could not connect to DAP. Is it powered on?");
        swd_dap_stop(dap);
    } else {
        SWD_LOGW("Target resynced after error. Packet dropped");
    }
    return;
}