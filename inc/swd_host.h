
#ifndef __SWD_HOST_H
#define __SWD_HOST_H

#include <stdbool.h>

#include "swd_dap.h"
#include "swd_err.h"
#include "swd_target_register.h"

#ifndef _Nullable
#define _Nullable
#endif // _Nullable

typedef struct _swd_host_t {
    /*
     * @brief DAP to communicate to the target with
     */
    swd_dap_t *dap;
    /*
     * @brief Flag denoting whether or not the host is started
     * @note This value does not represent wether or not the
     *          the target's core is halted
     */
    bool is_stopped;
    /*
     * Number of code comparators (HW breakpoints) supported
     */
    uint8_t code_cmp_cnt;
    /*
     * FPB unit version
     */
    uint8_t _fpb_version;
} swd_host_t;

/*
 * @brief Initialize the host structure for it to function
 * @param swd_host_t* reference of the host structure to initialize
 */
void swd_host_init(swd_host_t *host);

/*
 * @brief Assign a dap structure for the host to communicate through
 * @param swd_host_t* reference of the host structure to assign the dap to
 * @param swd_dap_t* reference of the dap to assign
 */
void swd_host_set_dap(swd_host_t *host, swd_dap_t *dap);

/*
 * @brief Start the bare minimum required of architecture components
 *          for the host to communicate properly. This function must be called
 *          to enable communication
 * @param swd_host_t* reference of the host structure to start
 * @note This does not make the target processor to enter a halt state
 */
swd_err_t swd_host_start(swd_host_t *host);

/*
 * @brief Stop the host and any underlying structures. Communication via the host
 *          and its underlying structures will also stop working
 * @param swd_host_t* reference of the host structure to start
 * @note This leaves the target processor as-is and simply "detatches"
 */
swd_err_t swd_host_stop(swd_host_t *host);

/*
 * @brief Send a signal to halt the processor
 * @param swd_host_t* reference of the host structure 
 */
swd_err_t swd_host_halt_target(swd_host_t *host);

/*
 * @brief Send a signal to perform a single step intruction. If the halt was caused by
 *          a hardware breakpoint, this will also step over the breakpoint
 * @param swd_host_t* reference of the host structure 
 * @note To step over breakpoints, the host will disable the FBP Unit (or Beakpoint Unit is ARMv6)
 */
swd_err_t swd_host_step_target(swd_host_t *host);

/*
 * @brief Send a signal to exit a halted state
 * @param swd_host_t* reference of the host structure 
 */
swd_err_t swd_host_continue_target(swd_host_t *host);

/*
 * @brief Send a signal to perform a software reset
 * @param swd_host_t* reference of the host structure 
 * @note Specifically, this resets the local core reset as well as
 *          resetting the processor's peripherals
 */
swd_err_t swd_host_reset_target(swd_host_t *host);

/*
 * @brief Send a signal to perform a software reset as well as a halt
 * @param swd_host_t* reference of the host structure 
 * @note This is the recomented call to use to perform these two operations
 *          beacuse calling `swd_host_halt_target` and `swd_host_reset_target`
 *          will have some delay associated. This function makes the
 *          processor halt on the very first instruction executed after
 *          a reset
 */
swd_err_t swd_host_halt_reset_target(swd_host_t *host);

/*
 * @brief Determine whether or not the target processor is halted
 * @param swd_host_t* reference of the host structure 
 * @param bool* reference of flag to set
 */
swd_err_t swd_host_is_target_halted(swd_host_t *host, bool *is_halted);

/*
 * @brief Write a single word of data at a specified address
 * @param swd_host_t* reference of the host structure 
 * @param uint32_t address of word to write
 * @param uint32_t word data
 * @return 
 */
swd_err_t swd_host_memory_write_word(swd_host_t *host, uint32_t addr, uint32_t data);

/*
 * @brief Write a block of words at specified starting address. If pressent, writes the
 *          number of successful WORD tranfers to `w_cnt`
 * @param swd_host_t* reference of the host structure 
 * @param uint32_t base address
 * @param uint32_t* reference to the data to write write
 * @param uint32_t array size of write buffer
 * @param uint32_t* number of successful word transactions done. Can be NULL
 */
swd_err_t swd_host_memory_write_word_block(swd_host_t *host, uint32_t start_addr, uint32_t *data_buf, 
                                           uint32_t bufsz, uint32_t* _Nullable w_cnt);

/*
 * @brief Write a block of bytes at specified starting address. If pressent, writes the
 *          number of successful BYTE tranfers to `w_cnt`
 * @param swd_host_t* reference of the host structure 
 * @param uint32_t base address
 * @param uint32_t* reference to the data to write write
 * @param uint32_t array size of write buffer
 * @param uint32_t* number of successful byte transactions done. Can be NULL
 * @note Because some DAPs might not support byte transfers, transfers are done one word at a time.
 *          This means that addresses `start_addr & ~3` to `start_addr + buf) & ~3` must exist.
 */
swd_err_t swd_host_memory_write_byte_block(swd_host_t *host, uint32_t start_addr, uint8_t *data_buf,
                                           uint32_t bufsz, uint32_t* _Nullable w_cnt);

/*
 * @brief Read a single word of data
 * @param swd_host_t* reference of the host structure 
 * @param uint32_t addresss to read from
 * @param uint32_t* buffer to write to
 */
swd_err_t swd_host_memory_read_word(swd_host_t *host, uint32_t addr, uint32_t *data);

/*
 * @brief Read a block of words at specified starting address. If pressent, writes the
 *          number of successful WORD tranfers to `rd_cnt`
 * @param swd_host_t* reference of the host structure 
 * @param uint32_t base address
 * @param uint32_t* reference to the buffer to place read data to
 * @param uint32_t array size of read buffer
 * @param uint32_t* number of successful word transactions done. Can be NULL
 */
swd_err_t swd_host_memory_read_word_block(swd_host_t *host, uint32_t start_addr, uint32_t *data_buf,
                                          uint32_t bufsz, uint32_t* _Nullable rd_cnt);

/*
 * @brief Read a block of bytes at specified starting address. If pressent, writes the
 *          number of successful BYTE tranfers to `rd_cnt`
 * @param swd_host_t* reference of the host structure 
 * @param uint32_t base address
 * @param uint32_t* reference to the buffer to place read data to
 * @param uint32_t array size of read buffer
 * @param uint32_t* number of successful byte transactions done. Can be NULL
 */
swd_err_t swd_host_memory_read_byte_block(swd_host_t *host, uint32_t start_addr, uint8_t *data_buf,
                                          uint32_t bufsz, uint32_t* _Nullable rd_cnt);

/*
 * @brief Read data present from one of the core's registers. The core must be halted 
 *          for the transaction to complete
 * @param swd_host_t* reference of the host structure 
 * @param swd_target_register_t Register to read from
 * @param uint32_t* buffer to write to 
 */
swd_err_t swd_host_register_read(swd_host_t *host, swd_target_register_t reg, uint32_t *data);

/*
 * @brief Write data to one of the core's registers. The core must be halted 
 *          for the transaction to complete
 * @param swd_host_t* reference of the host structure 
 * @param swd_target_register_t Register to write to
 * @param uint32_t write data
 */
swd_err_t swd_host_register_write(swd_host_t *host, swd_target_register_t reg, uint32_t data);

/*
 * @brief Add a hardware breakpoint at the specified address
 * @param swd_host_t* reference of the host structure 
 * @param uint32_t address of breakpoint
 */
swd_err_t swd_host_add_breakpoint(swd_host_t *host, uint32_t addr);

/*
 * @brief Delete a hardware breakpoint at the specified address
 * @param swd_host_t* reference of the host structure 
 * @param uint32_t address of breakpoint
 */
swd_err_t swd_host_remove_breakpoint(swd_host_t *host, uint32_t addr);

/*
 * @brief Zero out the entire hardware breakpoint address range
 * @param swd_host_t* reference of the host structure 
 */
swd_err_t swd_host_clear_breakpoints(swd_host_t *host);

/*
 * @brief Get the number of POSSIBLE breakpoints which can be added
 * @param swd_host_t* reference of the host structure 
 * @param uint32_t* buffer to write to
 */
swd_err_t swd_host_get_breakpoint_count(swd_host_t *host, uint32_t *bkpt_cnt);

/*
 * @brief Read out all enabled hardware breakpoints
 * @param swd_host_t* reference of the host structure 
 * @param uint32_t* reference of read buffer
 * @param uint32_t array size of read buffer
 * @param uint32_t* reference to store the number of enabled breakpoints
 */
swd_err_t swd_host_get_breakpoints(swd_host_t *host, uint32_t *buf, uint32_t bufsz,
                                   uint32_t *rdcnt);


#endif // __SWD_HOST_H