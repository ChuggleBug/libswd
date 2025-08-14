
#ifndef __SWD_DAP_H
#define __SWD_DAP_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/swd_driver.h"
#include "swd_err.h"
#include  "swd_dap_port.h"


typedef struct _swd_dap_t {
    swd_driver_t *driver;
    uint8_t data_size;
    uint8_t addr_int_bits;
    bool is_stopped;
    bool ap_error;
    bool is_little_endian;
    bool current_banksel;
    bool current_ctrlsel;
} swd_dap_t;

/*
 * Initialization utilities:
 */
void swd_dap_init(swd_dap_t *dap);
void swd_dap_set_driver(swd_dap_t *dap, swd_driver_t *driver);
swd_err_t swd_dap_start(swd_dap_t *dap);

/*
 *
 *
 *
 */
void swd_dap_set_jtag_to_swd(swd_dap_t *dap);
void swd_dap_reset_target(swd_dap_t *dap);

void swd_dap_stop(swd_dap_t *dap);
void swd_dap_reset(swd_dap_t *dap);

swd_err_t swd_dap_port_read(swd_dap_t *dap, swd_dap_port_t port, uint32_t *data);
swd_err_t swd_dap_port_write(swd_dap_t *dap, swd_dap_port_t port, uint32_t data);

#endif // __SWD_DAP_H