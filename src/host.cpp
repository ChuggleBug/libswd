
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
constexpr uint32_t FP_CMP_CODE_BASE = 0xE0002008;   // FP_CMP + 0
constexpr uint32_t FP_CMP_LIT_BASE = 0xE0002204;    // FP_CMP + 127 bytes

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
constexpr uint32_t RMPSPT = 0x20000000;

// ARMv7-M Memory Reigons
constexpr uint32_t CODE_BASE_ADDR = 0x0;
constexpr uint32_t CODE_END_ADDR = 0x1FFFFFFF;
constexpr uint32_t SRAM_BASE_ADDR = 0x20000000;
constexpr uint32_t SRAM_END_ADDR = 0x3FFFFFFF;


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

SWDHost::SWDHost(SWDDriver *driver) 
: m_dap(dap::DAP(driver)) {
    // Initalize Bufers
    for (uint32_t i = 0; i < MAX_CODE_CMP; i++) {
        m_code_cmp[i] = 0x0;
    }
    for (uint32_t i = 0; i < MAX_LIT_CMP; i++) {
       m_lit_cmp[i] = 0x0;
    }
}

SWDHost::~SWDHost() {}

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
    resetFPComparators();
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
    
    Optional<uint32_t> data = registerRead(REG::DebugReturnAddress);
    if (!data.hasValue()) { return false; }
    uint32_t curr_pc = data.getValue();
    memoryWrite(DHCSR, DBG_KEY | C_STEP | C_DEBUGEN);
    // Check if there was step
    data = registerRead(REG::DebugReturnAddress);
    if (!data.hasValue()) { return false; }
    uint32_t next_pc = data.getValue();
    // If there was a change in pc then the program did step
    if (curr_pc != next_pc) { return true; }
    Logger::debug("SWDHost::stepTarget: PC did not change. Checking if there is a breakpoint");
    // If there wasnt, then there might have been a hardware breakpoint
    // temporarily disable the breakpoint and then 
    if (containsBreakpoint(curr_pc) && enableBreakpoint(curr_pc, false)) {
        Logger::debug("SWDHost::stepTarget: Stepping over breakpoint");
        return memoryWrite(DHCSR, DBG_KEY | C_STEP | C_DEBUGEN) && enableBreakpoint(curr_pc, true);
    }
    // In the event where the target is stuck in spin (such as a branch instruction\
    // jumping to itself), then the inital step was successful
    // TODO: test this
    return true;
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
    Logger::info("Flash Patch remap is%s supported", m_supports_fp ? "" : " not");
}

// Private
void SWDHost::resetFPComparators() {
    Logger::info("Restting all FP comparators to 0");
    for (uint32_t i = 0; i < MAX_CODE_CMP; i++) {
        bool write_success = false;
        uint32_t retry_cnt = DEFAULT_RETRY_COUNT;
        while (!write_success) {
            // Code comparators and base comparators are next to eachother
            write_success = memoryWrite(FP_CMP_CODE_BASE, 0x0);
            if (!write_success && --retry_cnt == 0) {
                Logger::warn("Could not clear FP comparator %d", i);
                continue;
            }
        }
        m_code_cmp[i] = 0x0;
    }
    for (uint32_t i = 0; i < MAX_LIT_CMP; i++) {
        bool write_success = false;
        uint32_t retry_cnt = DEFAULT_RETRY_COUNT;
        while (!write_success) {
            // Code comparators and base comparators are next to eachother
            write_success = memoryWrite(FP_CMP_LIT_BASE, 0x0);
            if (!write_success && --retry_cnt == 0) {
                Logger::warn("Could not clear FP comparator %d", i);
                continue;
            }
        }
        m_lit_cmp[i] = 0x0;
    }
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
    if (addr > CODE_END_ADDR) {
        Logger::warn("FPBv1 does not support addresses greater than 0x%08x", CODE_END_ADDR);
        return false;
    }
    // Get the first availible breakpoint
    int32_t cmp_no = -1;
    for (uint32_t i = 0; i < m_num_code_cmp; i++) {
        if ((m_code_cmp[i] & 0x1) == 0x0) {
            cmp_no = i;
            break;
        }
    }

    // bit number was not set 
    if (cmp_no == -1) {
        Logger::warn("Not enough code comparators availible. Did not set 0x%08x", addr);
        return false;
    }
    
    // Try to write to FP_CODE_CMP{cmp_no}
    // Needs to convert address to the registers required format
    uint32_t encoded_addr = (addr & 0x1FFFFFFC);
    // set REPLACE
    switch (addr & 0x3)
    {
    case 0b00:
        encoded_addr |= 0x40000000;
        m_num_bkpt++;
        break;
    case 0b10:
        encoded_addr |= 0x80000000;
        m_num_bkpt++;
        break;
    default:
        Logger::warn("Unable to encode 0x%08x to breakpoint address", addr);
        return false;
    }
    encoded_addr |= ENABLE;
    if (memoryWrite(FP_CMP_CODE_BASE + 4*cmp_no, encoded_addr)) {
        // Make the value in target and the host match
        m_code_cmp[cmp_no] = encoded_addr;
        return true;
    }
    Logger::warn("Was not able to write breakpoint address");
    return false;
}

// Refer to armv7 architectural reference C1.11.5
static bool matches_bkpt_cmp_encoded(uint32_t addr, uint32_t encoded) {
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
    for (uint32_t i = 0; i < m_num_code_cmp; i++) {
        uint32_t encoded_addr = m_code_cmp[i];
        if (encoded_addr & ENABLE) {
            if (matches_bkpt_cmp_encoded(addr, encoded_addr)) {
                // Clear register
                if (!memoryWrite(FP_CMP_CODE_BASE + 4*i, 0x0)) {
                    Logger::warn("Was not able to clear breakpoint 0x%08x", addr);
                    return false;
                }
                switch (encoded_addr & 0xC0000000) {
                // When REPLACE is set to 0xC, theres actually two breakpoints
                // This debug implemntation does not use these types of comparators
                case 0xC0000000:
                    m_num_bkpt--;
                case 0x40000000:
                case 0x80000000:
                    m_num_bkpt--;
                }
                // Clear the associated index on the host as well
                m_code_cmp[i] = 0x0;
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
        uint32_t encoded = m_code_cmp[i];
        if (encoded & ENABLE) {
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

bool SWDHost::containsBreakpoint(uint32_t addr) {
    return getBreakpointIndex(addr) != -1;
}

bool SWDHost::enableBreakpoint(uint32_t addr, bool trigger) {
    int32_t cmp_index = getBreakpointIndex(addr);
    if (cmp_index == -1) {
        Logger::warn("Breakpoint at 0x%08x does not exist", addr);
        return false;
    }

    Optional<uint32_t> data = memoryRead(FP_CMP_CODE_BASE + 4*cmp_index);
    if (!data.hasValue()) {
        Logger::warn("Could not retrieve breakpoint information");
        return false;
    }
    uint32_t cmp_data = (data.getValue() & ~ENABLE) | (trigger ? ENABLE : 0x0);
    if (memoryWrite(FP_CMP_CODE_BASE + 4*cmp_index, cmp_data)) {
        m_code_cmp[cmp_index] = cmp_data;
        return true;
    }
    Logger::warn("Could not write updated breakpoint information");
    return false;
}

int32_t SWDHost::getBreakpointIndex(uint32_t addr) {
    int32_t index = -1;
    for (uint32_t i = 0; i < m_num_code_cmp; i++) {
        if (matches_bkpt_cmp_encoded(addr, m_code_cmp[i])) {
            index = i;
            break;
        }
    }
    return index;
}

static inline uint32_t getFPRemapAlignmentMask(uint32_t num_cmps) {
    // Minimum of 8 word alignment alignment
    // This bit mask is 32 - 1 = 0x1F
    uint32_t byte_alignment = num_cmps * 4;
    if (byte_alignment <= 32) {
        return 0x1F;
    }
    // Get the next highest power of 2
    // This is supposed to be added by one, but the mask is going to be used
    byte_alignment--;
    byte_alignment |= byte_alignment >> 1;   
    byte_alignment |= byte_alignment >> 2;   
    byte_alignment |= byte_alignment >> 4;
    byte_alignment |= byte_alignment >> 8;
    byte_alignment |= byte_alignment >> 16;
    return byte_alignment;
}


bool SWDHost::setRemapAddress(uint32_t addr) {
    if (!m_supports_fp) {
        Logger::warn("Flash Patch is not supported");
        return false;
    }
    if (addr < SRAM_BASE_ADDR || addr > SRAM_END_ADDR) {
        Logger::warn("The Flash Patch unit only supports addresses in range (0x%08x - 0x%08x). Did not set 0x%08x", SRAM_BASE_ADDR, SRAM_END_ADDR, addr);
        return false;
    }
    uint32_t remap = addr - SRAM_BASE_ADDR;
    uint32_t mask = getFPRemapAlignmentMask(m_num_code_cmp + m_num_lit_cmp);
    if (remap & mask) {
        Logger::warn("Remap address 0x%08x does not meet alignment requirements of %d bytes", addr, mask+1);
        return false;
    }

    return memoryWrite(FP_REMAP, remap & ~(0xE0000000 | mask));
}

bool SWDHost::resetRemapAddress() {
    if (!m_supports_fp) {
        Logger::warn("Flash Patch is not supported");
        return false;
    }
    return memoryWrite(FP_REMAP, 0x0);
}

Optional<uint32_t> SWDHost::getRemapAddress() {
    if (!m_supports_fp) {
        Logger::warn("Flash Patch is not supported");
        return Optional<uint32_t>::none();
    }
    return memoryRead(FP_CTRL).map<Optional<uint32_t>>(
        [](uint32_t data) {
            return Optional<uint32_t>::of((data & ~0xE000001F) + SRAM_BASE_ADDR);
        }, Optional<uint32_t>::none());
}


bool SWDHost::addRemapComparator(uint32_t addr) {
    if (!m_supports_fp) {
        Logger::warn("Flash Patch is not supported");
        return false;
    }
    if (addr > CODE_END_ADDR) {
        Logger::warn("FPBv1 does not support addresses greater than 0x%08x", CODE_END_ADDR);
        return false;
    }
    if (addr & 0x3) {
        Logger::warn("FPBv1 does not support non word aligned FP remap comparator addresses. Did not set 0x%08x", addr);
        return false;
    }
    
    // Get the first availible comparator
    int32_t cmp_no = -1;
    for (uint32_t i = 0; i < m_num_lit_cmp; i++) {
        if ((m_lit_cmp[i] & 0x1) == 0x0) {
            cmp_no = i;
            break;
        }
    }

    // bit number was not set 
    if (cmp_no == -1) {
        Logger::warn("Not enough literal comparators availible. Did not set 0x%08x", addr);
        return false;
    }
    
    // Try to write to FP_LIT_CMP{cmp_no}
    // Needs to convert address to the registers required format
    uint32_t encoded_addr = (addr & 0x1FFFFFFC);
    encoded_addr |= ENABLE;
    if (memoryWrite(FP_CMP_LIT_BASE + 4*cmp_no, encoded_addr)) {
        // Make the value in target and the host match
        m_lit_cmp[cmp_no] = encoded_addr;
        return true;
    }
    Logger::warn("Was not able to write FP comparator address");
    return false;
    
}

static bool matches_fp_cmp_encoded(uint32_t addr, uint32_t encoded) {
    return addr == ((encoded * 0x1FFFFFFC) + SRAM_BASE_ADDR);
}

bool SWDHost::removeRemapComparator(uint32_t addr) {
    if (!m_supports_fp) {
        Logger::warn("Flash Patch is not supported");
        return false;
    }
    // Need to iterate over every single comparator and check
    // if the encoded address matches
    for (uint32_t i = 0; i < m_num_lit_cmp; i++) {
        uint32_t encoded_addr = m_lit_cmp[i];
        if (encoded_addr & ENABLE) {
            if (matches_fp_cmp_encoded(addr, encoded_addr)) {
                // Clear register
                if (!memoryWrite(FP_CMP_CODE_BASE + 4*i, 0x0)) {
                    Logger::warn("Was not able to clear breakpoint 0x%08x", addr);
                    return false;
                }
                // Unlike breakpoints, one literal comparator can map 
                // to only one address
                m_num_lit_cmp--;
                m_code_cmp[i] = 0x0;
                return true;
            }
        }
    }
    Logger::warn("Did not find breakpoint 0x%08x", addr);
    return false;
}

uint32_t SWDHost::getRemapComparators(uint32_t *remaps) {
    if (!m_supports_fp) {
        Logger::warn("Flash Patch is not supported");
        return 0;
    }
    uint32_t cmp_cnt = 0;
    for (uint32_t i = 0; i < m_num_lit_cmp; i++) {
        uint32_t encoded = m_lit_cmp[i];
        if (encoded & ENABLE) {
            switch (encoded & 0xC0000000) { // Check REPLACE bits
            case 0x0:
                remaps[cmp_cnt++] = (encoded & 0x1FFFFFFC) + SRAM_BASE_ADDR;
                break;
            case 0x40000000: 
            case 0x80000000:
            case 0xC0000000:
                Logger::debug("SWDHost::getRemapComparators: Read a breakpoint comparator in the literal comparator reigon");
                break;
            default:
                Logger::debug("SWDHost::getRemapComparators: Unhandled case");
                break;
            }
        }
    }
    return cmp_cnt;
}

bool SWDHost::containsRemapComparator(uint32_t addr) {
    if (!m_supports_fp) {
        Logger::warn("Flash Patch is not supported");
        return false;
    }
    return getFPComparatorIndex(addr) != -1;
}

bool SWDHost::enableRemalComparator(uint32_t addr, bool trigger) {
    if (!m_supports_fp) {
        Logger::warn("Flash Patch is not supported");
        return false;
    }

    int32_t cmp_index = getBreakpointIndex(addr);
    if (cmp_index == -1) {
        Logger::warn("FP Comparator at 0x%08x does not exist", addr);
        return false;
    }

    Optional<uint32_t> data = memoryRead(FP_CMP_LIT_BASE+ 4*cmp_index);
    if (!data.hasValue()) {
        Logger::warn("Could not retrieve FP comparator information");
        return false;
    }
    uint32_t cmp_data = (data.getValue() & ~ENABLE) | (trigger ? ENABLE : 0x0);
    if (memoryWrite(FP_CMP_LIT_BASE + 4*cmp_index, cmp_data)) {
        m_lit_cmp[cmp_index] = cmp_data;
        return true;
    }
    Logger::warn("Could not write updated FP comparator information");
    return false;
}

int32_t SWDHost::getFPComparatorIndex(uint32_t addr) {
    int32_t index = -1;
    for (uint32_t i = 0; i < m_num_lit_cmp; i++) {
        if (matches_fp_cmp_encoded(addr, m_lit_cmp[i])) {
            index = i;
            break;
        }
    }
    return index;
}




} // namespace swd
