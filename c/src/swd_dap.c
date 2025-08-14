
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

static uint8_t get_paity_bit(uint32_t value);

static void swd_dap_idle_short(swd_dap_t *dap);
static void swd_dap_idle_long(swd_dap_t *dap);

static swd_err_t swd_dap_port_read_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data);
static swd_err_t swd_dap_port_read_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data);

static swd_err_t swd_dap_port_write_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t data);
static swd_err_t swd_dap_port_write_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t data);

static swd_err_t swd_dap_port_read_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t *data,
                                               uint32_t retry_count);
static swd_err_t swd_dap_port_write_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t data,
                                                uint32_t retry_count);

/*
 * Initialization utilities:
 */
void swd_dap_init(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);

    dap->is_stopped = false;
    dap->ap_error = false;
    dap->is_little_endian = true;

    dap->data_size = DEFAULT_CSW_VALUE;
    dap->addr_int_bits = DEFAULT_CSW_VALUE;

    dap->current_banksel = DEFAULT_SEL_VALUE;
    dap->current_ctrlsel = DEFAULT_SEL_VALUE;
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

#ifdef SWD_CONFIG_AUTO_JTAG_SWITCH
    swd_dap_set_jtag_to_swd(dap);
#endif // SWD_CONFIG_AUTO_JTAG_SWITCH

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
    if (swd_dap_port_read(dap, DP_CTRL_STAT, &data) != SWD_OK) {
        if (data & 0x50000000) {
            SWD_ERROR("Coult not verify CTRL/STAT for AP was written to");
            return SWD_ERR;
        }
    }

    return SWD_OK;
}

/*
 *
 *
 *
 */
void swd_dap_set_jtag_to_swd(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    swd_dap_reset_target(dap);
    swd_driver_write_bits(dap->driver, 0xE79E, 16);
    swd_dap_reset_target(dap);

    swd_dap_idle_short(dap);
}

void swd_dap_reset_target(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    swd_driver_write_bits(dap->driver, 0xFFFFFFFF, 32);
    swd_driver_write_bits(dap->driver, 0xFFFFFFFF, 32);
}

void swd_dap_stop(swd_dap_t *dap);
void swd_dap_reset(swd_dap_t *dap);

swd_err_t swd_dap_port_read(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(data != NULL);

    if (dap->is_stopped) {
        SWD_WARN("Attempting to read from a stopped DAP");
        return SWD_ERR;
    }

    SWD_DEBUG("Reading port %s", swd_dap_port_as_str(port));

    if (swd_dap_port_is_DP(port)) {
        return swd_dap_port_read_dp(dap, port, data);
    } else {
        return swd_dap_port_read_ap(dap, port, data);
    }
}

swd_err_t swd_dap_port_write(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    SWD_ASSERT(dap != NULL);

    if (dap->is_stopped) {
        SWD_WARN("Attempting to write to from a stopped DAP");
        return SWD_ERR;
    }

    SWD_DEBUG("Writing 0x%08lx to %s", data, swd_dap_port_as_str(port));

    if (swd_dap_port_is_DP(port)) {
        return swd_dap_port_write_dp(dap, port, data);
    } else {
        return swd_dap_port_write_ap(dap, port, data);
    }
}

static void swd_dap_idle_short(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    swd_driver_write_bits(dap->driver, 0x0, 2);
}

static void swd_dap_idle_long(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    swd_driver_write_bits(dap->driver, 0x0, 8);
}

static swd_err_t swd_dap_port_read_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data) {
    // Need to set CTRLSEL to 1 for DP_WCR
    if (port == DP_WCR) {
        uint32_t select_data;
        swd_dap_port_read(dap, DP_SELECT, &select_data);
        swd_dap_port_write(dap, DP_SELECT, select_data | 0x1);
    }

    uint8_t packet = swd_dap_port_as_packet(port, true);
    if (swd_dap_port_read_from_packet(dap, packet, data, RW_RETRY_COUNT) != SWD_OK) {
        return SWD_ERR;
    }

    // Unset the CTRLSEL bit if needed
    // A second read is needed if SELECT was the port which was written to
    if (port == DP_WCR) {
        uint32_t select_data;
        swd_dap_port_read(dap, DP_SELECT, &select_data);
        swd_dap_port_write(dap, DP_SELECT, select_data & 0x1);
    }

    return SWD_OK;
}

static swd_err_t swd_dap_port_read_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data) {
    return SWD_OK;
}

static swd_err_t swd_dap_port_write_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    // Need to set CTRLSEL to 1 for DP_WCR
    if (port == DP_WCR) {
        uint32_t select_data;
        swd_dap_port_read(dap, DP_SELECT, &select_data);
        swd_dap_port_write(dap, DP_SELECT, select_data | 0x1);
    }

    uint8_t packet = swd_dap_port_as_packet(port, false);
    if (swd_dap_port_write_from_packet(dap, packet, data, RW_RETRY_COUNT) != SWD_OK) {
        return SWD_ERR;
    }

    // Unset the CTRLSEL bit if needed
    // A second read is needed if SELECT was the port which was written to
    if (port == DP_WCR) {
        uint32_t select_data;
        swd_dap_port_read(dap, DP_SELECT, &select_data);
        swd_dap_port_write(dap, DP_SELECT, select_data & 0x1);
    }

    return SWD_OK;
}

static swd_err_t swd_dap_port_write_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    return SWD_OK;
}

static swd_err_t swd_dap_port_read_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t *data,
                                               uint32_t retry_count) {
    if (retry_count == 0) {
        SWD_WARN("Retry count for sending packet exceeded");
        return SWD_ERR;
    }

    // Perform read sequence
    swd_driver_write_bits(dap->driver, packet, 8);
    swd_driver_turnaround(dap->driver);
    uint8_t ack = swd_driver_read_bits(dap->driver, 3);

    // Check if device is Ok for a read
    // TODO: Move into switch block?
    uint32_t rd_data = 0;
    uint8_t rd_parity = 0;
    if (ack == ACK_OK) {
        rd_data = swd_driver_read_bits(dap->driver, 32);
        rd_parity = swd_driver_read_bits(dap->driver, 1);
        swd_driver_turnaround(dap->driver);
    }

    switch (ack) {
    case ACK_OK: {
        // Validate parity from rdata
        if (rd_parity != get_paity_bit(rd_parity)) {
            SWD_INFO("Data received was OK, but had invalid parity. Retrying");
            return swd_dap_port_read_from_packet(dap, packet, data, retry_count - 1);
        }
        *data = rd_data;
        return SWD_OK;
    }
    case ACK_WAIT:
        break;
    case ACK_FAULT:
        break;
    default:
        break;
    }
    return SWD_ERR;
}

static swd_err_t swd_dap_port_write_from_packet(swd_dap_t *dap, uint8_t packet, uint32_t data,
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

    // Continue to write data if the ack was OK
    // TODO: Move into switch block?
    if (ack == ACK_OK) {
        uint8_t parity = get_paity_bit(data);
        swd_driver_write_bits(dap->driver, data, 32);
        swd_driver_write_bits(dap->driver, parity, 1);
    }

    switch (ack) {
    case ACK_OK: {
        // Check if WDATAERR is set
        uint32_t ctrlstat;
        if (swd_dap_port_read(dap, DP_CTRL_STAT, &ctrlstat) != SWD_OK) {
            return SWD_ERR;
        }
        if (ctrlstat & 0x80) {
            return swd_dap_port_write_from_packet(dap, packet, data, retry_count - 1);
        }

        return SWD_OK;
    }
    case ACK_WAIT:
        break;
    case ACK_FAULT:
        break;
    default:
        break;
    }

    return SWD_ERR;
}

static uint8_t get_paity_bit(uint32_t value) {
    uint8_t parity = 0;
    while (value) {
        parity ^= (value & 1);
        value >>= 1;
    }
    return parity;
}