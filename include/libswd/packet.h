// packet.h

#ifndef __SWD_PACKET_H
#define __SWD_PACKET_H

#include <stdint.h>

namespace swd {

enum class DP {
    ABORT,     // 0x0, CTRLSEL = X, RO
    IDCODE,    // 0x0, CTRLSEL = X, WO
    CTRL_STAT, // 0x4, CTRLSEL = 0, RW
    WCR,       // 0x4, CTRLSEL = 1, RW
    RESEND,    // 0x8, CTRLSEL = X, RO
    SELECT,    // 0x8, CTRLSEL = X, WO
    RDBUFF,    // 0xC, CTRLSEL = X, RO
    ROUTESEL   // 0xC, CTRLSEL = X, WO
};

enum class AP {
    CSW,  // 0x00
    TAR,  // 0x04
    DRW,  // 0x0C
    DB0,  // 0x10
    DB1,  // 0x14
    DB2,  // 0x18
    DB3,  // 0x1C
    CFG,  // 0xF4
    BASE, // 0xF8
    IDR   // 0xFC
};

} // namespace swd

#endif // __SWD_PACKET_H