
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
constexpr uint32_t FP_REMAP = 0xE0002004;
// While these two values are both considered FP_COMPn
// to allow the software to manage these values,
// the code and the literal address base spaces can be thought 
// of as separate spaces. In doing so, the host is able
// to remotely handle two "arrays" without the need to store it
// its own memory
constexpr uint32_t FP_CMP_CODE_BASE = 0xE0002008;
constexpr uint32_t FP_CMP_LIT_BASE = 0xE0002204;

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

// FP_CTRL fields
constexpr uint32_t KEY = 0x2;
// FP_COMPn fields
constexpr uint32_t ENABLE = 0x1;

// FP_REMAP fields
constexpr uint32_t RMPSPT = 0x10000000;

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
    enableFPB(true);
    readFBPConfigs();
}

void SWDHost::stop() { m_host_stopped = true; }

bool SWDHost::isStopped() { return m_host_stopped; }

bool SWDHost::haltTarget() { return memoryWrite(DHCSR, DBG_KEY | C_HALT | C_DEBUGEN); }

bool SWDHost::stepTarget() { 
    if (!isTargetHalted()) {
        Logger::warn("Cannot step a non-halted target");
        return false;
    }
    return enableFPB(false) && memoryWrite(DHCSR, DBG_KEY | C_STEP | C_DEBUGEN) && enableFPB(true);

    // // Measure the change in program counters after the step insturction
    // // If there is a change, then the program did step. If not
    // // then there is most likely a breakpoint
    // Optional<uint32_t> data = registerRead(REG::DebugReturnAddress);
    // uint32_t curr_pc, next_pc;
    // if (!data.hasValue()) { return false; }
    // curr_pc = data.getValue();
    // // If the program is not at a breakpoint, then the PC will change
    // // TODO: is there any special cases where this isnt the case?
    // if (!memoryWrite(DHCSR, DBG_KEY | C_STEP | C_DEBUGEN)) { return false; }
    // data = registerRead(REG::DebugReturnAddress);
    // if (!data.hasValue()) { return false; }
    // next_pc = data.getValue();
    
    // if (curr_pc != next_pc) {
    //     return true; // Target did step
    // }
    // // Disable the associated breakpoint and then step again
    // data = getBkptCmpAddr(curr_pc);
    // if (!data.hasValue()) { return false; }
    // uint32_t cmp_addr = data.getValue();




}

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
                data_buf[i] = data.getValue();
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


bool SWDHost::enableFPB(bool trigger) {
    Optional<uint32_t> data = memoryRead(FP_CTRL);
    if (data.hasValue() && memoryWrite(FP_CTRL, (data.getValue() & ~0x3) | KEY | (trigger ? ENABLE : 0x0))) {
        Logger::info("Set FPB to %s", trigger ? "on" : "off");
        return true;
    }
    Logger::warn("Could not change FP_CTRL");
    return false;
}

bool SWDHost::supportsFlashPatch() {
    return m_supports_fp;
}

// Private
void SWDHost::readFBPConfigs() {
    // Read Version
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

    // Read Comparator and values
    memoryRead(FP_CTRL).andThen([this](uint32_t data) {
        // NUM_CODE[6:4] is FP_CTRL[14:12]
        // NUM_CODE[3:0] is FP_CTRL[7:4] 
        m_num_code_cmp = ((data & 0x7000) >> 0x8) | ((data & 0xF0) >> 0x4);
        Logger::info("Allowable code comparators %d", m_num_code_cmp);
        // NUM_LIT is FP_CTRL[11:8] 
        m_num_lit_cmp = (data & 0xF00) >> 0x8;
        Logger::info("Allowable literal address comparators %d", m_num_lit_cmp);
    });

    m_supports_fp = memoryRead(FP_REMAP).map<bool>(
        [](uint32_t data) { return data & RMPSPT; },
        false
    );
}

uint32_t SWDHost::getCodeCompCount() {
    return m_num_code_cmp;
}

uint32_t SWDHost::getLiteralCompCount() {
    return m_num_lit_cmp;
}

uint32_t SWDHost::getBreakpointCount() {
    return m_num_bkpt;
}

// Code comparison operations
bool SWDHost::addBreakpoint(uint32_t addr) {
    // Get the first availible breakpoint
    int32_t bit_no = -1;
    for (uint32_t i = 0; i < m_num_code_cmp; i++) {
        if (((m_set_code_cmps[i/64] >> (i%64)) & 0x1) == 0x0) {
            bit_no = i;
            break;
        }
    }

    // bit number was not set 
    if (bit_no == -1) {
        Logger::warn("Not enough code comparators availible. Did not set 0x%08x", addr);
        return false;
    }
    
    // Try to write to FP_CODE_CMP{bit_no}
    // Needs to convert address to the registers required format
    uint32_t bkpt_addr = (addr & 0x1FFFFFFC);
    // set REPLACE
    switch (addr & 0x3)
    {
    case 0b00:
        bkpt_addr |= 0x40000000;
        m_num_bkpt++;
        break;
    case 0b10:
        bkpt_addr |= 0x80000000;
        m_num_bkpt++;
        break;
    default:
        Logger::warn("Unable to encode 0x%08x to breakpoint address", addr);
        return false;
    }
    if (memoryWrite(FP_CMP_CODE_BASE + 4*bit_no, bkpt_addr | ENABLE)) {
        // Set bit to 1
        *(m_set_code_cmps + bit_no/64) |= (0x1 << (bit_no%64));
        return true;
    }
    Logger::warn("Was not able to write breakpoint address");
    return false;
}

// Refer to armv7 architectural reference C1.11.5
static bool matches_cmp_encoded(uint32_t addr, uint32_t encoded) {
    switch (encoded & 0xC0000000) { // Check REPLACE bits
    case 0x0: // Is a remap address
        return false;
    case 0x40000000: 
        return addr == (encoded & 0x1FFFFFFC);
    case 0x80000000:
        return addr == ((encoded & 0x1FFFFFFC) | 0x2);
    case 0xC0000000:
        return addr == (encoded & 0x1FFFFFFC) ||
                addr == ((encoded & 0x1FFFFFFC) | 0x2);
    default:
        Logger::debug("matches_cmp_encoded: Unhandled case");
        return false;
    }
}

bool SWDHost::removeBreakpoint(uint32_t addr) {
    // Need to iterate over every single breakpoint and check
    // if the encoded address matches
    uint32_t cmps_raw[m_num_code_cmp];
    if (!memoryReadBlock(FP_CMP_CODE_BASE, cmps_raw, m_num_code_cmp)) {
        Logger::warn("Was not able to read breakpoints");
        return false;
    }

    for (uint32_t i = 0; i < m_num_code_cmp; i++) {
        if ((m_set_code_cmps[i/64] >> (i%64)) & 0x1) {
            if (matches_cmp_encoded(addr, cmps_raw[i])) {
                // Clear register
                if (!memoryWrite(FP_CMP_CODE_BASE + 4*i, 0x0)) {
                    Logger::warn("Was not able to clear breakpoint 0x%08x", addr);
                    return false;
                }
                switch (cmps_raw[i] & 0xC0000000) {
                case 0x0:
                    break;
                // When REPLACE is set to 0xC, theres actually two breakpoints
                // This debug implemntation does not use these types of comparators
                case 0xC0000000:
                    m_num_bkpt--;
                case 0x40000000:
                case 0x80000000:
                    m_num_bkpt--;
                }
                // Unset the associated bit
                *(m_set_code_cmps + i/64) &= ~(0x1 << (i%64));
                return true;
            }
        }
    }
    Logger::warn("Did not find breakpoint 0x%08x", addr);
    return false;
}

uint32_t SWDHost::getBreakpoints(uint32_t *bkpts) {
    uint32_t bkpt_cnt = 0;
    for (uint32_t i = 0; i < m_num_code_cmp; i++) {
        // Check if the i'th bit is set
        // Bit 64+ is located on the second array
        if ((m_set_code_cmps[i/64] >> (i%64)) & 0x1) {
            Optional<uint32_t> data = memoryRead(FP_CMP_CODE_BASE + 4*i);
            if (!data.hasValue()) {
                Logger::warn("Could not read from code comparator at 0x%08x. Stopping", FP_CMP_CODE_BASE + 4*i);
                return bkpt_cnt;
            }
            uint32_t encoded = data.getValue() & ~ENABLE;
            switch (encoded & 0xC0000000) { // Check REPLACE bits
            case 0x0: // Is a remap address
                Logger::debug("SWDHost::getBreakpoints: Read a literal comparator in code comparator reigon");
                break;
            case 0x40000000: 
                bkpts[bkpt_cnt++] = encoded & 0x1FFFFFFC;
                break;
            case 0x80000000:
                bkpts[bkpt_cnt++] = (encoded & 0x1FFFFFFC) | 0x2;
                break;
            case 0xC0000000:
                bkpts[bkpt_cnt++] = encoded & 0x1FFFFFFC;
                bkpts[bkpt_cnt++] = (encoded & 0x1FFFFFFC) | 0x2;
                break;
            default:
                Logger::debug("SWDHost::getBreakpoints: Unhandled case");
                break;
            }
        }
    }
    return bkpt_cnt;
}


// Literal Address Comparisons
bool SWDHost::setRemapAddress(uint32_t addr) {

}

bool SWDHost::addRemapComparator(uint32_t addr) {

}

bool SWDHost::removeRemapAddress(uint32_t addr) {

}

uint32_t SWDHost::getRemapComparators(uint32_t *remaps) {
    uint32_t remap_cnt = 0;
    for (uint32_t i = 0; i < m_num_lit_cmp; i++) {
        // Check if the i'th bit is set
        if ((m_set_lit_cmps >> i) & 0x1) {
            Optional<uint32_t> data = memoryRead(FP_CMP_LIT_BASE + 4*i);
            if (!data.hasValue()) {
                Logger::warn("Could not read from comparator at 0x%08x", FP_CMP_LIT_BASE + 4*i);
                return 0;
            }
            remaps[remap_cnt++] = data.getValue();
        }
    }
    return remap_cnt;
}

} // namespace swd
