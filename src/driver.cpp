
#include "libswd/driver.h"

namespace swd {

// Driver code

uint32_t SWDDriver::readBits(uint8_t cnt) {
    cfgSWDIOIn();
    uint32_t data = 0;
    uint8_t i;
    for (i = 0; i < cnt; i++) {
        setSWCLK();
        hold();
        clearSWCLK();
        hold();
        data |= readSWDIO() << i;
    }
    return data;
}

void SWDDriver::writeBits(uint32_t data, uint8_t cnt) {
    cfgSWDIOOut();
    uint8_t i;
    for (i = 0; i < cnt; i++) {
        setSWCLK();
        hold();
        writeSWDIO((data >> i) & 0x1);
        clearSWCLK();
        hold();
    }
}

void SWDDriver::init() {
    resetTarget();
    // Special bit sequence defined by ADI
    writeBits(0xE79E, 16);
    // In case that the target was in SWD Mode
    resetTarget();
    idleShort(); // small idle period to let the target stabilize
}

void SWDDriver::resetTarget() {
    // Documentations typically specify to use at least 50 cycles
    // of SWDIO set to high.
    cfgSWDIOOut();
    writeBits(0xFFFFFFFF, 32); // Writes 32 High bits
    writeBits(0xFFFFFFFF, 18); // 32 + 18 = 50
}

void SWDDriver::turnaround(uint32_t trn) {
    uint32_t i;
    for (i = 0; i < trn; i++) {
        setSWCLK();
        hold();
        clearSWCLK();
        hold();
    }
}

void SWDDriver::idleShort() {
    // Added this to remove ambiguity
    writeBits(0x0, 2);
}

void SWDDriver::idleLong() {
    // Added this to remove ambiguity
    writeBits(0x0, 8);
}

} // namespace swd