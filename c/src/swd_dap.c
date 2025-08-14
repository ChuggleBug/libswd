
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h> // TODO: remove me eventually

#include "driver/swd_driver.h"
#include "swd_dap.h"
#include "swd_err.h"
#include "swd_conf.h"

#define DEFAULT_SEL_VALUE (0xbeefcafe)
#define DEFAULT_CSW_VALUE (-1)

static void swd_dap_idle_short(swd_dap_t *dap);
static void swd_dap_idle_long(swd_dap_t *dap);

static swd_err_t swd_dap_port_read_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data);
static swd_err_t swd_dap_port_read_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data);

static swd_err_t swd_dap_port_write_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t data);
static swd_err_t swd_dap_port_write_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t data);


/*
 * Initialization utilities:
 */
void swd_dap_init(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);

    dap->host_stopped = false;
    dap->ap_powered_on = false;
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

void swd_dap_start(swd_dap_t *dap) {
    SWD_ASSERT(dap != NULL);
    SWD_ASSERT(dap->driver != NULL);

    swd_driver_start(dap->driver);

#ifdef SWD_CONFIG_AUTO_JTAG_SWITCH
    swd_dap_set_jtag_to_swd(dap);
#endif // SWD_CONFIG_AUTO_JTAG_SWITCH
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

    printf("Reading port %s..\n", swd_dap_port_as_str(port));

    if (swd_dap_port_is_DP(port))  {
        return swd_dap_port_read_dp(dap, port, data);
    } else {
        return swd_dap_port_read_ap(dap, port, data);
    }

}

swd_err_t swd_dap_port_write(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
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
    uint8_t packet = swd_dap_port_as_packet(port, true);

    printf("Writing 0x%02x\n", packet);

    // Write packet bits
    swd_driver_write_bits(dap->driver, (uint32_t) packet, 8);
    // Turnaround
    swd_driver_turnaround(dap->driver);
    // Read ACK
    uint8_t ack_bits = swd_driver_read_bits(dap->driver, 3);
    // Read 32 bits and then parity
    uint32_t data_bits = swd_driver_read_bits(dap->driver, 32);
    uint8_t parity_bits = swd_driver_read_bits(dap->driver, 1);

    swd_driver_turnaround(dap->driver);

    printf("ACK = 0x%x, Rdata = 0x%08lx, Pairty = 0x%x\n", ack_bits, data_bits, parity_bits);

    *data = data_bits;
    return SWD_OK;

}

static swd_err_t swd_dap_port_read_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data) {
    return SWD_OK;
}

static swd_err_t swd_dap_port_write_dp(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    return SWD_OK;
}

static swd_err_t swd_dap_port_write_ap(swd_dap_t *dap, swd_dap_port_t port, uint32_t data) {
    return SWD_OK;
}
