
#ifndef __SWD_HOST_H
#define __SWD_HOST_H

#include <stdbool.h>

#include "swd_dap.h"
#include "swd_err.h"
#include "swd_target_register.h"

typedef struct _swd_host_t {
    /* struct attrs*/
    swd_dap_t *dap;
    bool is_stopped;
} swd_host_t;

void swd_host_init(swd_host_t *host);
void swd_host_set_dap(swd_host_t *host, swd_dap_t *dap);

// start host
swd_err_t swd_host_start(swd_host_t *host);

// stop host
swd_err_t swd_host_stop(swd_host_t *host);

// haltTarget
swd_err_t swd_host_halt_target(swd_host_t *host); 

// stepTarget
swd_err_t swd_host_step_target(swd_host_t *host);

// continueTarget
swd_err_t swd_host_continue_target(swd_host_t *host);

// resetTarget
swd_err_t swd_host_reset_target(swd_host_t *host);

// haltingReset
swd_err_t swd_host_halt_reset_target(swd_host_t *host);

// checkIfTargetIsHalted
swd_err_t swd_host_is_target_halted(swd_host_t *host, bool *is_halted);

// memoryWriteWord
swd_err_t swd_host_memory_write_word(swd_host_t *host, uint32_t addr, uint32_t data);

// memoryWriteWordBlock
swd_err_t swd_host_memory_write_word_block(swd_host_t *host, uint32_t start_addr, uint32_t *data_buf, uint32_t bufsz);

// memoryWriteByteBlock
swd_err_t swd_host_memory_write_byte_block(swd_host_t *host, uint32_t start_addr, uint8_t *data_buf, uint32_t bufsz);

// memoryReadWord
swd_err_t swd_host_memory_read_word(swd_host_t *host, uint32_t addr, uint32_t* data);

// memoryReadWordBlock
swd_err_t swd_host_memory_read_word_block(swd_host_t *host, uint32_t start_addr, uint32_t *data_buf, uint32_t bufsz);

// memoryReadByteBlock
swd_err_t swd_host_memory_read_byte_block(swd_host_t *host, uint32_t start_addr, uint8_t *data_buf, uint32_t bufsz);

// memoryReadRegister
swd_err_t swd_host_register_read(swd_host_t *host, swd_target_register_t reg, uint32_t *data);

swd_err_t swd_host_register_write(swd_host_t *host, swd_target_register_t reg, uint32_t data);

// AddBreakPoint
swd_err_t swd_host_add_breakpoint(swd_host_t *host, uint32_t addr);

// RemoveBreakPoint
swd_err_t swd_host_remove_breakpoints(swd_host_t *host, uint32_t addr);

// GetBreakpoints
swd_err_t swd_host_get_breakpoint_count(swd_host_t *host, uint32_t *bkpt_cnt);

swd_err_t swd_host_get_breakpoints(swd_host_t *host, uint32_t *buf, uint32_t bufsz);

#endif // __SWD_HOST_H