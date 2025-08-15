
#include <stdint.h>
#include <stdio.h>

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
    default:
        return false;
    }
}

bool swd_dap_port_is_AP(swd_dap_port_t port) { return !swd_dap_port_is_DP(port); }

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
        printf("Unknown dap port value\n");
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
        SWD_WARN("Invalid port passed to translate to apbanksel. Expected AP, got %s",
                 swd_dap_port_as_str(port));
        return SELECT_APBANKSEL_ERR;
    }
}

const char *swd_dap_port_as_str(swd_dap_port_t port) {
    switch (port) {
    case DP_ABORT:
        return "DP ABORT";
    case DP_IDCODE:
        return "DP IDCODE";
    case DP_CTRL_STAT:
        return "DP CTRL/STAT";
    case DP_WCR:
        return "DP WCR";
    case DP_RESEND:
        return "DP RESEND";
    case DP_SELECT:
        return "DP SELECT";
    case DP_RDBUFF:
        return "DP RDBUFF";
    case DP_ROUTESEL:
        return "DP ROUTESEL";

    case AP_CSW:
        return "AP CSW";
    case AP_TAR:
        return "AP TAR";
    case AP_DRW:
        return "AP DRW";
    case AP_DB0:
        return "AP DB0";
    case AP_DB1:
        return "AP DB1";
    case AP_DB2:
        return "AP DB2";
    case AP_DB3:
        return "AP DB3";
    case AP_CFG:
        return "AP CFG";
    case AP_BASE:
        return "AP BASE";
    case AP_IDR:
        return "AP IDR";

    default:
        return "UNKNOWN";
    }
}
