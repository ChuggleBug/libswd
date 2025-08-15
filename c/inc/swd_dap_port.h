
#ifndef __SWD_DAP_PORT_H
#define __SWD_DAP_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define SELECT_APBANKSEL_ERR ((uint32_t)(-1))

typedef enum _swd_dap_port_t {
    DP_ABORT,     // 0x0, CTRLSEL = X, WO
    DP_IDCODE,    // 0x0, CTRLSEL = X, RO
    DP_CTRL_STAT, // 0x4, CTRLSEL = 0, RW
    DP_WCR,       // 0x4, CTRLSEL = 1, RW
    DP_RESEND,    // 0x8, CTRLSEL = X, RO
    DP_SELECT,    // 0x8, CTRLSEL = X, WO
    DP_RDBUFF,    // 0xC, CTRLSEL = X, RO
    DP_ROUTESEL,  // 0xC, CTRLSEL = X, WO

    AP_CSW,  // 0x00, RW
    AP_TAR,  // 0x04, RW
    AP_DRW,  // 0x0C, RW
    AP_DB0,  // 0x10, RW
    AP_DB1,  // 0x14, RW
    AP_DB2,  // 0x18, RW
    AP_DB3,  // 0x1C, RW
    AP_CFG,  // 0xF4, RO
    AP_BASE, // 0xF8, RO
    AP_IDR   // 0xFC, RO
} swd_dap_port_t;

bool swd_dap_port_is_DP(swd_dap_port_t port);
bool swd_dap_port_is_AP(swd_dap_port_t port);

uint8_t swd_dap_port_as_packet(swd_dap_port_t port, bool is_read);
uint32_t swd_dap_port_as_apbanksel_bits(swd_dap_port_t port);

const char *swd_dap_port_as_str(swd_dap_port_t port);

#endif // __SWD_DAP_PORT_H