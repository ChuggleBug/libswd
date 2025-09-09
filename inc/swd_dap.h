
#ifndef __SWD_DAP_H
#define __SWD_DAP_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/swd_driver.h"
#include "swd_dap_port.h"
#include "swd_err.h"

typedef struct _swd_dap_t {
    swd_driver_t *driver;
    bool is_stopped;
    bool _ap_error;
} swd_dap_t;

/*
 * @brief Initialize any required data required for
 *          the dap the function
 * @param swd_dap_t* reference of dap structure to initialize
 */
void swd_dap_init(swd_dap_t *dap);

/*
 * @brief Assign a driver for the dap to use for communication to a hardware port
 * @param swd_dap_t* reference of dap structure to assign the driver to
 * @param swd_driver_t* reference of the driver
 */
void swd_dap_set_driver(swd_dap_t *dap, swd_driver_t *driver);

/*
 * @brief Start the target's DAP by first initializing underlying hardware
 *          drivers and signaling a line reset
 * @param swd_dap_t* reference of dap structure to start
 * @note on some targets a JTAG to SWD sequence is required. This option can be set
 *          with SWD_CONFIG_AUTO_JTAG_SWITCH in "swd_conf.h"
 */
swd_err_t swd_dap_start(swd_dap_t *dap);

/*
 *  @brief Stop the target DAP and deinitialize and associated host hardware
 *  @param swd_dap_t* reference of dap structure to stop
 *  @note As the name implies, the dap read and writes are no longer able to be performed
 *          The dap needs to be started again at this point
 */
swd_err_t swd_dap_stop(swd_dap_t *dap);

/*
 * @brief Perform a single DP/AP port read
 * @param swd_dap_t* reference of dap structure to read form
 * @param swd_dap_port_t DP/AP port name
 * @param uint32_t* data buffer for read data to be written to on success
 * @return status of dap read
 */
swd_err_t swd_dap_port_read(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data);

/*
 * @brief Perform a single DP/AP port write
 * @param swd_dap_t* reference of dap structure to read form
 * @param swd_dap_port_t DP/AP port name
 * @param uint32_t data to be written to in the port
 * @return status of dap read
 */
swd_err_t swd_dap_port_write(swd_dap_t *dap, swd_dap_port_t port, uint32_t data);

#endif // __SWD_DAP_H