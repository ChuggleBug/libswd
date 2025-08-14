
#ifndef __SWD_DRIVER_H
#define __SWD_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#include "../swd_err.h"

typedef struct _swd_driver_t {
    /*
     * Setup subroutines:
     *
     * Should be responsible for initializaing and deinitializing
     * required hardware
     *
     * @note the initalization function should be responsible
     *       for configuring SWCLK as neither a pullup or pulldown resistor.
     *       SWDIO's IO configurations are managed somewhere else in the driver
     *
     */
    swd_err_t (*init)(void);
    swd_err_t (*deinit)(void);

    /*
     * SWDIO subroutines:
     *  uint8_t SWDIO_read(void):
     *      @brief read the current state of the SWDIO pin
     *      @return uint8_t: A single bit denoting the current value
     *
     * void SWDIO_write(uint8_t):
     *      @brief set the current sate of SWDIO
     *      @note since thie wire can only be set to high or low, only
     *              the lowest bit should be considered
     *
     * void SWDIO_cfg_in(void):
     *      @brief configure the SWDIO pin as an input pin
     *      @note based on the SWD protocol description, SWDIO needs a
     *              pulldown resistor to properly read
     *
     * void SWD_cfg_out(void):
     *      @brief configure the SWDIO as an output pin
     *      @note no pullup/pulldown resistor is needed for output
     *
     */
    uint8_t (*SWDIO_read)(void);
    void (*SWDIO_write)(uint8_t);
    void (*SWDIO_cfg_in)(void);
    void (*SWDIO_cfg_out)(void);

    /*
     * SWCLK Operations:
     *  void SWCLK_set(void):
     *      @brief set SWCLK to a HIGH value
     *
     * void SWCLK_clear(void):
     *      @brief set SWCLK to a LOW value
     */
    void (*SWCLK_set)(void);
    void (*SWCLK_clear)(void);

    /*
     * SWD Idle period subroutines:
     *  void hold(void):
     *      @brief Hold for a constant period of time
     *      @note based on the timing constriants provided by ARM.
     *              each high and low period for SWCLK needs
     *              to be betwen [10ns - 500us]
     *
     */
    void (*hold)(void);

    /*
     * Software Managed Attributes:
    */
   bool started;

} swd_driver_t;

void swd_driver_start(swd_driver_t *driver);
void swd_driver_stop(swd_driver_t *driver);

/*
 * Data Read and Write operations. Support for 32 bits.
 * Writes from least significant bit first
 */
uint32_t swd_driver_read_bits(swd_driver_t *driver, uint8_t cnt);
void swd_driver_write_bits(swd_driver_t *driver, uint32_t data, uint8_t cnt);

/*
 * Single cycle turnaround
*/
void swd_driver_turnaround(swd_driver_t *driver);

#endif // __SWD_DRIVER_H