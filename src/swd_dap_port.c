
#include <strings.h>
#include <inttypes.h>
#include <stdint.h>

#include "swd_dap_port.h"
#include "swd_log.h"

// Packet bit constants
#define PACKET_BASE (0x81)
#define DP_PORT (0x0)
#define AP_PORT (0x2)

#define RW_READ (0x4)
#define RW_WRITE (0x0)

#define Ax0 (0x00)
#define Ax4 (0x08)
#define Ax8 (0x10)
#define AxC (0x18)

/*
 * Port to string mapping
 */
static const struct port_str_mapping {
    swd_dap_port_t port;
    const char* name;
} mappings[] = {
    { DP_ABORT, "ABORT" },
    { DP_IDCODE, "IDCODE" },
    { DP_CTRL_STAT, "DP CTSTAT" },
    { DP_WCR, "WCR" },
    { DP_RESEND, "RESEND" },
    { DP_SELECT, "SELECT" },
    { DP_RDBUFF, "RDBUFF" },
    { DP_ROUTESEL, "ROUTESEL" },

    { AP_CSW, "CSW" },
    { AP_TAR, "TAR" },
    { AP_DRW, "DRW" },
    { AP_DB0, "DB0" },
    { AP_DB1, "DB1" },
    { AP_DB2, "DB2" },
    { AP_DB3, "DB3" },
    { AP_CFG, "CFG" },
    { AP_BASE, "AP ASE" },
    { AP_IDR, "IDR" },
};

bool swd_dap_port_is_DP(swd_dap_port_t port) {
    switch (port) {
    case DP_ABORT:
    case DP_IDCODE:
    case DP_CTRL_STAT:
    case DP_WCR:
    case DP_RESEND:
    case DP_SELECT:
    case DP_RDBUFF:
    case DP_ROUTESEL:
        return true;
    case AP_CSW:
    case AP_TAR:
    case AP_DRW:
    case AP_DB0:
    case AP_DB1:
    case AP_DB2:
    case AP_DB3:
    case AP_CFG:
    case AP_BASE:
    case AP_IDR:
        return false;
    default:
        SWD_LOGW("Port value (%" PRIi32 ") is neither an AP or DP", (int32_t)port);
        return false;
    }
}

bool swd_dap_port_is_AP(swd_dap_port_t port) { return !swd_dap_port_is_DP(port); }

bool swd_dap_port_is_a_read_port(swd_dap_port_t port) {
    switch (port) {
    case DP_IDCODE:    // RO
    case DP_CTRL_STAT: // RW
    case DP_WCR:       // RW
    case DP_RESEND:    // RO
    case DP_RDBUFF:    // RO
    case AP_CSW:       // RW
    case AP_TAR:       // RW
    case AP_DRW:       // RW
    case AP_DB0:       // RW
    case AP_DB1:       // RW
    case AP_DB2:       // RW
    case AP_DB3:       // RW
    case AP_CFG:       // RO
    case AP_BASE:      // RO
    case AP_IDR:       // RO
        return true;
    case DP_ABORT:    // WO
    case DP_SELECT:   // WO
    case DP_ROUTESEL: // WO
        return false;
    default:
        SWD_LOGW("Port value (%" PRIi32 ") is neither a read or a write port", (int32_t)port);
        return false;
    }
}

bool swd_dap_port_is_a_write_port(swd_dap_port_t port) {
    switch (port) {
    case DP_ABORT:     // WO
    case DP_CTRL_STAT: // RW
    case DP_WCR:       // RW
    case DP_SELECT:    // WO
    case DP_ROUTESEL:  // WO
    case AP_CSW:       // RW
    case AP_TAR:       // RW
    case AP_DRW:       // RW
    case AP_DB0:       // RW
    case AP_DB1:       // RW
    case AP_DB2:       // RW
    case AP_DB3:       // RW
        return true;
    case DP_IDCODE: // RO
    case DP_RESEND: // RO
    case DP_RDBUFF: // RO
    case AP_CFG:    // RO
    case AP_BASE:   // RO
    case AP_IDR:    // RO
        return false;
    default:
        SWD_LOGW("Port value (%" PRIi32 ") is neither a read or a write port", (int32_t)port);
        return false;
    }
}

void set_packet_parity(uint8_t *packet) {}

uint8_t swd_dap_port_as_packet(swd_dap_port_t port, bool is_read) {
    uint8_t packet = PACKET_BASE;
    packet |= (swd_dap_port_is_DP(port) ? DP_PORT : AP_PORT);
    packet |= (is_read ? RW_READ : RW_WRITE);

    switch (port) {
    case DP_ABORT:
    case DP_IDCODE:
    case AP_CSW:
    case AP_DB0:
        packet |= Ax0;
        break;
    case DP_CTRL_STAT:
    case DP_WCR:
    case AP_TAR:
    case AP_DB1:
    case AP_CFG:
        packet |= Ax4;
        break;
    case DP_RESEND:
    case DP_SELECT:
    case AP_DB2:
    case AP_BASE:
        packet |= Ax8;
        break;
    case DP_RDBUFF:
    case DP_ROUTESEL:
    case AP_DRW:
    case AP_IDR:
    case AP_DB3:
        packet |= AxC;
        break;
    default:
        SWD_LOGW("Unknown dap port value (%" PRIi32 ")", ((int32_t)port));
        break;
    }

    // Calculate packet parity of packet[1:4] (APnDP, RnW, A[2:3])
    // 1 for odd number of 1's, 0 for even number of 1's
    uint8_t parity = 0;
    for (uint8_t i = 1; i <= 4; i++) {
        parity ^= (packet >> i);
    }
    packet = (packet & ~0x20) | ((parity & 1) << 5);

    return packet;
}

uint32_t swd_dap_port_as_apbanksel_bits(swd_dap_port_t port) {
    switch (port) {
    case AP_CSW:
    case AP_TAR:
    case AP_DRW:
        return 0x00;
    case AP_DB0:
    case AP_DB1:
    case AP_DB2:
    case AP_DB3:
        return 0x10;
    case AP_CFG:
    case AP_BASE:
    case AP_IDR:
        return 0xF0;
    default:
        SWD_LOGW("Invalid port passed to translate to apbanksel. Expected AP, got %s",
                 swd_dap_port_as_str(port));
        return SELECT_APBANKSEL_ERR;
    }
}

const char *swd_dap_port_as_str(swd_dap_port_t port) {
    for (uint32_t i = 0; i < sizeof(mappings) / sizeof(struct port_str_mapping); i++){
        if (mappings[i].port == port) {
            return mappings[i].name;
        }
    }
    return "UKNOWN";
}

bool swd_dap_port_from_str(const char* str, swd_dap_port_t *port) {
    for (uint32_t i = 0; i < sizeof(mappings) / sizeof(struct port_str_mapping); i++){
        if (strcasecmp(mappings[i].name, str) == 0) {
            *port = mappings[i].port;
            return true;
        }
    }
    return false;
}
