
#include "libswd/host.h"

namespace swd {

// Driver code

uint32_t SWDDriver::readBits(uint8_t cnt) {
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
    uint8_t i;
    for (i = 0; i < cnt; i++) {
        setSWCLK();
        hold();
        writeSWDIO( (data >> i) & 0x1);
        clearSWCLK();
        hold();
    }
}

void SWDDriver::turnaround() {
    setSWCLK();
    hold();
    clearSWCLK();
    hold();
}

// Host code

void SWDHost::init() {
    resetTarget();
    // Special bit sequence defined by
    driver->writeBits(0xE79E, 16);
    // In case that the target was in SWD Mode
    resetTarget();
    writeBits(0, 2); // small idle period to let the target stabilize
}

void SWDHost::resetTarget() {
    // Documentations typically specify to use at least 50 cycles 
    // of SWDIO set to high. 64 high bits are sent instead to ensure
    // that most devices are reset
    driver->cfgSWDIOOut();
    driver->writeBits(0xFFFFFFFF, 32); // Writes 32 High bits
    driver->writeBits(0xFFFFFFFF, 18); // This can also be 18 bits instead
}

void SWDHost::sendPacket(uint8_t packet) {
    driver->cfgSWDIOOut();
    driver->writeBits(packet, 8);
}

void SWDHost::turnaround(uint32_t trn) {
    uint32_t i;
    for (i = 0; i < trn; i++) {
        driver->turnaround();
    }
}

uint8_t SWDHost::readACK() {
    driver->cfgSWDIOIn();
    return driver->readBits(3);
}

uint32_t SWDHost::readBits(uint8_t cnt) {
    driver->cfgSWDIOIn();
    return driver->readBits(cnt);
}

void SWDHost::writeBits(uint32_t wdata, uint8_t cnt) {
    driver->cfgSWDIOOut();
    driver->writeBits(wdata, cnt);
}

void SWDHost::idleShort() {
    // Added this to remove ambiguity
    this->writeBits(0x0, 2);
}

void SWDHost::idleLong() {
    // Added this to remove ambiguity
    this->writeBits(0x0, 8);
}

} // namespace swd