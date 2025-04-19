
#include "libswd/host.h"
#include "libswd/logger.h"
#include "libswd/target_registers.h"

namespace { // anonymous

// System control register
constexpr uint32_t AIRCR = 0xE000ED0C; // Application Interrupt and Reset Control Register
constexpr uint32_t DFSR = 0xE000ED30;  // Debug Fault Status Register

// Debug Registers
constexpr uint32_t DHCSR = 0xE000EDF0; // Debug Halting Control and Status Register
constexpr uint32_t DCRSR = 0xE000EDF4; // Debug Core Register Selector Register
constexpr uint32_t DCRDR = 0xE000EDF8; // Debug Core Register Data Register
constexpr uint32_t DEMCR = 0xE000EDFC; // Debug Event and Monitor Control Register

// Flash Patch and Breakpoint unit
constexpr uint32_t FP_CTRL = 0xE0002000;

// Constants for convenience
// DHCSR fields
constexpr uint32_t DBG_KEY = 0xA05F0000; // Special Sequence required for DHCSR
constexpr uint32_t C_DEBUGEN = 0x1;      // Bit required for DHCSR control bit
constexpr uint32_t C_HALT = 0x2;         // Halt debugger
constexpr uint32_t C_STEP = 0x4;         // Step debugger
constexpr uint32_t C_MASKINTS = 0x8;     // Mask interrupts when debugging
constexpr uint32_t S_HALTED = 0x20000;   // Halt status
constexpr uint32_t S_REGRDY = 0x10000;   // DCRDR status checking

// DEMCR fields
constexpr uint32_t VC_CORERESET = 0x1; // Catch a local reset

// AIRCR fields
constexpr uint32_t VECTKEY = 0x05FA0000; // Special sequence required for AIRCR
// Trigger a local reset
// requires DBG_EN to be set in DHCSR
constexpr uint32_t SYSRESETREQ = 0x4; // Local reset core + peripherals
constexpr uint32_t VECTRESET = 0x1;   // Local reset core (and maybe peripherals)

// DCRSR fields
constexpr uint32_t REG_W = 0x10000; // Enable write to register
constexpr uint32_t REG_R = 0x0;     // Mostly for readability

} // namespace

namespace swd::target {

enum class FBP_VER : uint32_t {
    V1 = 0x00000000,
    V2 = 0x10000000,
};

}

namespace swd {

using namespace target;
using namespace dap;

void SWDHost::reset() {
    // Reset DAP
    m_dap.reset();
    m_host_stopped = false;
    // Reset and halt host the moment it is reset
    if (haltAfterResetTarget()) {
        Logger::info("Target Halted on reset");
    }
    // Aquire FPB version
    setFBPVersion();
}

void SWDHost::stop() { m_host_stopped = true; }

bool SWDHost::isStopped() { return m_host_stopped; }

bool SWDHost::haltTarget() { return memoryWrite(DHCSR, DBG_KEY | C_HALT | C_DEBUGEN); }

bool SWDHost::stepTarget() { return memoryWrite(DHCSR, DBG_KEY | C_STEP | C_DEBUGEN); }

bool SWDHost::resetTarget() {
    return continueTarget() && memoryWrite(AIRCR, VECTKEY | SYSRESETREQ);
}

bool SWDHost::haltAfterResetTarget() {
    bool w_status = memoryWrite(DHCSR, DBG_KEY | C_DEBUGEN) && memoryWrite(DEMCR, VC_CORERESET) &&
                    memoryWrite(AIRCR, VECTKEY | SYSRESETREQ);
    // Ok if this function fails
    memoryRead(DEMCR).andThen([this](uint32_t data) { memoryWrite(DEMCR, data & ~VC_CORERESET); });
    return w_status;
}

bool SWDHost::continueTarget() { return memoryWrite(DHCSR, DBG_KEY | C_DEBUGEN); }

bool SWDHost::isTargetHalted() {
    return memoryRead(DHCSR).map<bool>([](uint32_t data) { return data & S_HALTED; }, false);
}

bool SWDHost::isDAPStopped() { return m_dap.isStopped(); }

bool SWDHost::memoryWrite(uint32_t address, uint32_t data) {
    return m_dap.setDataLengthWord() && m_dap.writePort(AP::TAR, address) &&
           m_dap.writePort(AP::DRW, data);
}

bool SWDHost::memoryWriteBlock(uint32_t base, uint32_t *data_buf, uint32_t data_size) {
    if (!m_dap.setDataLengthWord() || !m_dap.setAutoIncrementTAR(true)) {
        Logger::warn("Was not able to configure target for word block transfers");
        return false;
    }
    m_dap.writePort(AP::TAR, base);
    for (uint32_t i = 0; i < data_size; i++) {
        uint32_t retry_cnt = DEFAULT_RETRY_COUNT;
        while (!m_dap.writePort(AP::DRW, data_buf[i])) {
            retry_cnt--;
            if (retry_cnt == 0) {
                Logger::warn("Was not able to write block to memory");
                return false;
            }
        }
    }
    return true;
}

bool SWDHost::memoryWriteBlock(uint32_t base, uint8_t *data_buf, uint32_t data_size) {
    if (!m_dap.setDataLengthByte() || !m_dap.setAutoIncrementTAR(true)) {
        Logger::warn("Was not able to configure target for byte block transfers");
        return false;
    }

    bool little = m_dap.isTargetLittleEndian();

    m_dap.writePort(AP::TAR, base);
    for (uint32_t i = 0; i < data_size; i++, base++) {
        uint32_t retry_cnt = DEFAULT_RETRY_COUNT;
        // Needs to shift by a certain ammount while considering endianness (ADIv5 Table 8-3)
        uint32_t byte_lane = data_buf[i]
                             << (little ? 8 * (base & 0x3)          // LSB stays at LSB
                                        : 24 - (8 * (base & 0x3))); // LSB moves over to MSB
        while (!m_dap.writePort(AP::DRW, byte_lane)) {
            retry_cnt--;
            if (retry_cnt == 0) {
                Logger::warn("Was not able to write block to memory");
                return false;
            }
        }
    }
    return true;
}

Optional<uint32_t> SWDHost::memoryRead(uint32_t address) {
    if (!m_dap.setDataLengthWord() || !m_dap.writePort(AP::TAR, address)) {
        return Optional<uint32_t>::none();
    }
    return m_dap.readPort(AP::DRW);
}

bool SWDHost::memoryReadBlock(uint32_t base, uint32_t *data_buf, uint32_t data_size) {
    m_dap.setAutoIncrementTAR(true);
    m_dap.writePort(AP::TAR, base);
    for (uint32_t i = 0; i < data_size; i++) {
        uint32_t retry_cnt = DEFAULT_RETRY_COUNT;
        bool read_success = false;
        while (!read_success) {
            Optional<uint32_t> data = m_dap.readPort(AP::DRW);
            if (data.hasValue()) {
                data_buf[data_size++] = data.getValue();
                read_success = true;
                continue;
            }
            if (--retry_cnt == 0) {
                Logger::warn("Was not able to read block of memory");
                return false;
            }
        }
    }
    return true;
}

// Convenience Wrapper for register reading and writing
// does not need to exist inside of class declaration
static void blockUntilREGRDY(SWDHost &host) {
    while (
        host.memoryRead(DHCSR).map<bool>([](uint32_t data) { return !(data & S_REGRDY); }, true)) {
        Logger::info("DCRDR might not be ready, waiting");
        // Unless the dap is stopped
        if (host.isDAPStopped()) {
            Logger::info("DAP is stopped so REGRDY will return nothing");
            return;
        }
    }
}

Optional<uint32_t> SWDHost::registerRead(target::REG reg) {
    if (!isTargetHalted()) {
        Logger::warn("Target is not halted so registers can't be read");
        return Optional<uint32_t>::none();
    }
    // REG enum already holds bit information
    if (memoryWrite(DCRSR, REG_R | (uint32_t)reg)) {
        blockUntilREGRDY(*this);
        return memoryRead(DCRDR);
    }
    return Optional<uint32_t>::none();
}

bool SWDHost::registerWrite(target::REG reg, uint32_t data) {
    if (!isTargetHalted()) {
        Logger::warn("Target is not halted so registers can't be written to");
        return false;
    }
    // Strang program flow, but refer to ARMv7 reference "Use of DCRSR and DCRDR"
    if (memoryWrite(DCRDR, data)) {
        blockUntilREGRDY(*this);
        // REG enum already holds bit information
        return memoryWrite(DCRSR, REG_W | (uint32_t)reg);
    }
    return false;
}

// Private

void SWDHost::setFBPVersion() {
    // Default case
    m_fbv_version = FBP_VER::V1;
    memoryRead(FP_CTRL).andThen([this](uint32_t data) {
        switch (data >> 28) {
        case 0b0000:
            Logger::info("Detected FPB V1");
            m_fbv_version = FBP_VER::V1;
            break;
        case 0b0001:
            Logger::info("Detected FPB V2");
            m_fbv_version = FBP_VER::V2;
            break;
        default:
            Logger::info("Detected unknown FBP version");
            break;
        }
    });
}

} // namespace swd
