
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

/*
 * @brief Checks wether or not a selected port is for DPACC
 * @note Any value passed in which not defined in swd_dap_port_t will return false
*/
bool swd_dap_port_is_DP(swd_dap_port_t port);

/*
 * @brief Checks wether or not a selected port is for APACC
 * @note Any value passed in which not defined in swd_dap_port_t will return false
*/
bool swd_dap_port_is_AP(swd_dap_port_t port);

/*
 * @brief Checks wether or not a selected port can be read from
 * @note Any value passed in which not defined in swd_dap_port_t will return false
*/
bool swd_dap_port_is_a_read_port(swd_dap_port_t port);

/*
 * @brief Checks wether or not a selected port can be written to
 * @note Any value passed in which not defined in swd_dap_port_t will return false
*/
bool swd_dap_port_is_a_write_port(swd_dap_port_t port);

/* 
 * Converts a DAP port into the required 8 bit packet to be sent over 
 * the SWD protocol
 * @param swd_dap_port The requested port
 * @param bool Wether or not the requested port is to be read from or written to
 * @note Any value passed in which not defined in swd_dap_port_t is undefined
*/
uint8_t swd_dap_port_as_packet(swd_dap_port_t port, bool is_read);

/*
 * Provides the required APBANKSEL bits to be used on SELECT to access the AP Port
 * @param swd_dap_port_t The AP port which is intended to be accessed
 * @note Any DP port or value not defined in swd_dap_port_t returns SELECT_APBANKSEL_ERR
 */
uint32_t swd_dap_port_as_apbanksel_bits(swd_dap_port_t port);

/* 
 * @brief Converts a port value into a human readable port abbreviation
*/ 
const char *swd_dap_port_as_str(swd_dap_port_t port);

/* 
 * @brief Converts a string into its swd_dap_port_t value
 * @param const char* String to check
 * @param swd_dap_port_t* reference to store port value
 * @return Whether or not the conversion was successful
 * @note in the case where this function fails and returns false, `port`
 *  will not be modified and its value might be unkown after the call.
 * @note This function is case insentivie
*/
bool swd_dap_port_from_str(const char* str, swd_dap_port_t *port);

#endif // __SWD_DAP_PORT_H