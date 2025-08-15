
#include <stdbool.h>
#include <stdlib.h>

#include "driver/swd_driver.h"
#include "swd_err.h"
#include "swd_log.h"

void swd_driver_start(swd_driver_t *driver) {
    if (!driver->_started) {
        if (driver->init() != SWD_OK) {
            SWD_ERROR("Driver failed to initialize");
            return;
        }
        driver->_started = true;
    } else {
        SWD_WARN("Not starting a driver which was previously started");
    }
}

void swd_driver_stop(swd_driver_t *driver) {
    if (driver->_started) {
        if (driver->deinit() != SWD_OK) {
            SWD_ERROR("Driver failed to deinitialized");
        }
        driver->_started = false;
    } else {
        SWD_WARN("Not stopping a driver which is not stopped");
    }
}

uint32_t swd_driver_read_bits(swd_driver_t *driver, uint8_t cnt) {
    SWD_ASSERT(driver != NULL);
    SWD_ASSERT(cnt <= 32);

    driver->SWDIO_cfg_in();

    uint32_t data = 0;
    uint8_t i;
    for (i = 0; i < cnt; i++) {
        driver->SWCLK_set();
        driver->hold();
        driver->SWCLK_clear();
        driver->hold();
        data |= driver->SWDIO_read() << i;
    }
    return data;
}

void swd_driver_write_bits(swd_driver_t *driver, uint32_t data, uint8_t cnt) {
    SWD_ASSERT(driver != NULL);
    SWD_ASSERT(cnt <= 32);

    driver->SWDIO_cfg_out();

    uint8_t i;
    for (i = 0; i < cnt; i++) {
        driver->SWCLK_set();
        driver->hold();

        driver->SWDIO_write((data >> i) & 0x1);

        driver->SWCLK_clear();
        driver->hold();
    }
}

void swd_driver_turnaround(swd_driver_t *driver) {
    SWD_ASSERT(driver != NULL);

    driver->SWCLK_set();
    driver->hold();
    driver->SWCLK_clear();
    driver->hold();
}