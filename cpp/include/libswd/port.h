// port.h

#ifndef __SWD_PORT_H
#define __SWD_PORT_H

#include <stdint.h>

namespace swd::dap {

enum class DP {
    ABORT,     // 0x0, CTRLSEL = X, WO
    IDCODE,    // 0x0, CTRLSEL = X, RO
    CTRL_STAT, // 0x4, CTRLSEL = 0, RW
    WCR,       // 0x4, CTRLSEL = 1, RW
    RESEND,    // 0x8, CTRLSEL = X, RO
    SELECT,    // 0x8, CTRLSEL = X, WO
    RDBUFF,    // 0xC, CTRLSEL = X, RO
    ROUTESEL   // 0xC, CTRLSEL = X, WO
};

enum class AP {
    CSW,  // 0x00, RW
    TAR,  // 0x04, RW
    DRW,  // 0x0C, RW
    DB0,  // 0x10, RW
    DB1,  // 0x14, RW
    DB2,  // 0x18, RW
    DB3,  // 0x1C, RW
    CFG,  // 0xF4, RO
    BASE, // 0xF8, RO
    IDR   // 0xFC, RO
};

} // namespace swd::dap

#endif // __SWD_PORT_H