
#ifndef __SWD_DRIVER_H
#define __SWD_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "../swd_err.h"

/*
 * @brief The underlying hardware interface which can be used
 *          to control a target device using the SWD protocol
 * @note Any function references defined in this structure should be
 *          called by non driver inteface code (so only functions defined in
 *          the driver's header file should call these functions)
 */
typedef struct _swd_driver_t {
    /*
     * @brief Function responsible for initializaing required hardware
     * @note Should configure SWCLK as an output pin with neither a pull-up or pull-down resistor.
     *          SWDIO's IO configurations are managed somewhere else in the driver
     */
    swd_err_t (*init)(void);

    /*
     * @brief Function responsible for deinitializaing required hardware
     * @note While not technically needed since any higher level constructs know when
     *          to stop using the driver, a function reference still needs to be provided.
     *          A minimal implementation can return an OK response
     */
    swd_err_t (*deinit)(void);

    /*
     * @brief Read the current state of the SWDIO pin
     * @return uint8_t A single bit denoting the current value
     * @note The state of the pin should be returned in the LSB of the return value
     */
    uint8_t (*SWDIO_read)(void);

    /*
     * @brief Set the current state of the SWDIO pin
     * @param uint8_t The value to set the wire to
     * @note The state of the wire should be set to the LSB of the argument
     */
    void (*SWDIO_write)(uint8_t);

    /*
     * @brief Configure the SWDIO pin as an input pin
     * @note Based on the SWD protocol description, SWDIO needs a
     *          pulldown resistor to properly read
     */
    void (*SWDIO_cfg_in)(void);

    /*
     * @brief Configure the SWDIO as an output pin
     * @note No pull-up/pull-down resistor is needed for output
     */
    void (*SWDIO_cfg_out)(void);

    /*
     * @brief set SWCLK to a HIGH value
     */
    void (*SWCLK_set)(void);

    /*
     * @brief set SWCLK to a LOW value
     */
    void (*SWCLK_clear)(void);

    /*
     * @brief Hold for a constant period of time
     * @note Based on the timing constriants provided by ARM.
     *          each high and low period for SWCLK needs
     *          to be between [10ns - 500us]
     * @note Trying to have a low idle period is important as
     *          a single dap port read and write operation
     *          takes multiple "hold" durations. High microsecond
     *          delays should be avoided
     */
    void (*hold)(void);

    /*
     * @brief internal component to prevent multiple [de]inits
     */
    bool _started;

} swd_driver_t;

/*
 * @brief Initialize any hardware required for the driver to function
 * @param swd_driver_t* reference of driver structure
 */
void swd_driver_start(swd_driver_t *driver);

/*
 * @brief Deinitialize any hardware which was required for the driver to function
 */
void swd_driver_stop(swd_driver_t *driver);

/*
 * @brief Read `cnt` bit at a time.
 * @param swd_driver_t* reference of driver structure
 * @param uint8_t number of bits to read. Up to 32 bits a time
 * @return uint32_t All data read where the `i`'th bit read is at bit position `i`
 */
uint32_t swd_driver_read_bits(swd_driver_t *driver, uint8_t cnt);

/*
 * @brief Write a set number bits on the wire, LSB first
 * @param swd_driver_t* reference of driver structure
 * @param uint32_t Bit sequence to write
 * @param uint8_t number of bits to read. Up to 32 bits a time
 */
void swd_driver_write_bits(swd_driver_t *driver, uint32_t data, uint8_t cnt);

/*
 * @brief Perform a single cycle turnaround
 * @note While this defintiion is not exact, a "turnaround" is a means
 *          for the host controll who's "turns" it is drive SWDIO.
 *          Typically this problem is resolved by Tx and Rx communication
 */
void swd_driver_turnaround(swd_driver_t *driver);

#endif // __SWD_DRIVER_H