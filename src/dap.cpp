#include "libswd/dap.h"
#include "libswd/logger.h"
#include "libswd/optional.h"
#include "libswd/port.h"

namespace { // anonymous

using namespace swd::dap;

constexpr uint8_t PACKET_BASE = 0x81;
constexpr uint8_t DP_Port = 0x0;
constexpr uint8_t AP_Port = 0x2;
// Addresses shifted for better or'ing
constexpr uint8_t Ax0 = 0x00;
constexpr uint8_t Ax4 = 0x08;
constexpr uint8_t Ax8 = 0x10;
constexpr uint8_t AxC = 0x18;

enum class RW : uint8_t { READ = 0x4, WRITE = 0x0 };

uint32_t calculate_data_parity(uint32_t data) {
    uint8_t parity = 0;
    while (data) {
        parity ^= (data & 1); // XOR each bit
        data >>= 1;
    }
    return parity; // Returns 1 if odd number of 1s, otherwise 0
}

void set_packet_parity(uint8_t *packet) {
    uint8_t parity = 0;

    for (uint8_t i = 1; i <= 4; i++) {
        parity ^= (*packet >> i);
    }

    *packet = (*packet & ~0x20) | ((parity & 1) << 5);
}

uint8_t make_packet(DP port, RW access) {

    uint8_t packet = PACKET_BASE | DP_Port | static_cast<uint8_t>(access);

    // Mostly for debugging
    if (Logger::isSet()) {
        switch (port) {
        case DP::ABORT:
            Logger::debug("make_packet: DP ABORT");
            break;
        case DP::IDCODE:
            Logger::debug("make_packet: DP IDCODE");
            break;
        case DP::CTRL_STAT:
            Logger::debug("make_packet: DP CTRL/STAT");
            break;
        case DP::WCR:
            Logger::debug("make_packet: DP WCR");
            break;
        case DP::RESEND:
            Logger::debug("make_packet: DP RESEND");
            break;
        case DP::SELECT:
            Logger::debug("make_packet: DP SELECT");
            break;
        case DP::RDBUFF:
            Logger::debug("make_packet: DP RDBUFF");
            break;
        case DP::ROUTESEL:
            Logger::debug("make_packet: DP ROUTESEL");
            break;
        default:
            Logger::warn("Unknown debug port");
            break;
        }
        switch (access) {
        case RW::READ:
            Logger::debug("make_packet: READ");
            break;
        case RW::WRITE:
            Logger::debug("make_packet: WRITE");
            break;
        }
    }

    switch (port) {
    case DP::ABORT:
    case DP::IDCODE:
        packet |= Ax0;
        break;
    case DP::CTRL_STAT:
    case DP::WCR:
        packet |= Ax4;
        break;
    case DP::RESEND:
    case DP::SELECT:
        packet |= Ax8;
        break;
    case DP::RDBUFF:
    case DP::ROUTESEL:
        packet |= AxC;
        break;
    default:
        break;
    }
    set_packet_parity(&packet);
    return packet;
}

uint8_t make_packet(AP port, RW access) {
    uint8_t packet = PACKET_BASE | AP_Port | static_cast<uint8_t>(access);

    if (Logger::isSet()) {
        switch (port) {
        case AP::CSW:
            Logger::debug("make_packet: AP CSW");
            break;
        case AP::DB0:
            Logger::debug("make_packet: AP DB0");
            break;
        case AP::TAR:
            Logger::debug("make_packet: AP TAR");
            break;
        case AP::DB1:
            Logger::debug("make_packet: AP DB1");
            break;
        case AP::CFG:
            Logger::debug("make_packet: AP CFG");
            break;
        case AP::DB2:
            Logger::debug("make_packet: AP DB2");
            break;
        case AP::BASE:
            Logger::debug("make_packet: AP BASE");
            break;
        case AP::DRW:
            Logger::debug("make_packet: AP DRW");
            break;
        case AP::IDR:
            Logger::debug("make_packet: AP IDR");
            break;
        case AP::DB3:
            Logger::debug("make_packet: AP BD3");
            break;
        default:
            Logger::warn("Unknown access port");
            break;
        }
        switch (access) {
        case RW::READ:
            Logger::debug("make_packet: READ");
            break;
        case RW::WRITE:
            Logger::debug("make_packet: WRITE");
            break;
        }
    }

    switch (port) {
    case AP::CSW:
    case AP::DB0:
        packet |= Ax0;
        break;
    case AP::TAR:
    case AP::DB1:
    case AP::CFG:
        packet |= Ax4;
        break;
    case AP::DB2:
    case AP::BASE:
        packet |= Ax8;
        break;
    case AP::DRW:
    case AP::IDR:
    case AP::DB3:
        packet |= AxC;
        break;
    default:
        break;
    }
    set_packet_parity(&packet);
    return packet;
}

uint32_t getAPBANK(AP port) {
    switch (port) {
    case AP::CSW:
    case AP::TAR:
    case AP::DRW:
        return 0x00;
    case AP::DB0:
    case AP::DB1:
    case AP::DB2:
    case AP::DB3:
        return 0x10;
    case AP::CFG:
    case AP::BASE:
    case AP::IDR:
        return 0xF0;
    default:
        return 0xF0;
    }
}

} // namespace

namespace swd::dap {

enum class ACK { OK = 0b001, WAIT = 0b010, FAULT = 0b100, ERR = 0b111 };

void DAP::resetLine() {
    driver->init();
    if (!readFromPacketUnsafe(make_packet(DP::IDCODE, RW::READ)).hasValue()) {
        stop();
        return;
    }
    driver->idleShort();
    writePort(DP::ABORT, 0x1F); // Clear all errors on reset
}

void DAP::reset() {
    m_stop_host = false;
    resetLine();
    m_ap_power_on = false;
    m_current_banksel = DEFAULT_SEL_VALUE;
    m_current_ctrlsel = DEFAULT_SEL_VALUE;
    // Check endianess
    // CFG is readonly and cannot be changed
    m_is_little_endian = readPort(AP::CFG).map<bool>([](uint32_t cfg) { return (cfg & 0x1) == 0; },
                                                     true); // Is most likely true
    setConfigs();
}

void DAP::stop() {
    Logger::error("Host cannot connect to target. Stopping...");
    m_stop_host = true;
}

bool DAP::isStopped() { return m_stop_host; }

bool DAP::isTargetLittleEndian() { return m_is_little_endian; }

void DAP::setConfigs() {
    setDataLengthByte();
    setAutoIncrementTAR(false);
}

void DAP::initAP() {
    Logger::info("Initializing Access Port");
    if (!(writePort(DP::CTRL_STAT, 0x50000000) && readPort(DP::CTRL_STAT).hasValue())) {
        Logger::error("Count not initalize AP port. Stopping host");
        stop();
        return;
    }
    Logger::info("Access Port Initialized");
    m_ap_power_on = true;
}

bool DAP::APPoweredOn() { return m_ap_power_on; }

// Private read and write methods
Optional<uint32_t> DAP::readFromPacketUnsafe(uint32_t packet, ACK *ack) {
    sendPacket(packet);
    driver->turnaround();
    ACK rd_ack = readACK();
    Optional<uint32_t> data = Optional<uint32_t>::none();

    if (rd_ack == ACK::OK) {
        data = readData();
        driver->turnaround();
    }

    if (ack != nullptr) {
        *ack = rd_ack;
    }
    return data;
}

Optional<uint32_t> DAP::readFromPacket(uint32_t packet, uint32_t retry_count) {
    if (isStopped()) {
        Logger::warn("Host is stopped. Will no longer read");
        return Optional<uint32_t>::none();
    }

    if (retry_count == 0) {
        Logger::info("Read retry count exceeded. Aborting read");
        return Optional<uint32_t>::none();
    }

    Logger::debug("DAP::readFromPacket: packet = 0x%x", packet);
    ACK ack;
    Optional<uint32_t> data = readFromPacketUnsafe(packet, &ack);

    switch (ack) {
    case ACK::OK: {
        Logger::debug("DAP::readFromPacket: ACK = Ok");
        if (!data.hasValue()) {
            Logger::info("Resending packet request");
            return readFromPacket(packet, retry_count - 1);
        }
        Logger::debug("DAP::readFromPacket: data = 0x%x", data);
        return data;
    }
    case ACK::WAIT:
        Logger::debug("DAP::readFromPacket: ACK = WAIT");
        driver->turnaround();
        return readFromPacket(packet, retry_count - 1);
    case ACK::FAULT:
        driver->turnaround();
        Logger::debug("DAP::readFromPacket: ACK = FAULT");
        handleFault();
        return readFromPacket(packet, retry_count - 1);
    }
    Logger::debug("DAP::readFromPacket: ACK = ERROR");
    handleError();
    return Optional<uint32_t>::none();
}

bool DAP::writeFromPacketUnsafe(uint32_t packet, uint32_t data, ACK *ack) {
    sendPacket(packet);
    driver->turnaround();
    ACK w_ack = readACK();
    driver->turnaround();

    // write method might return early, so save to reference now
    if (ack != nullptr) {
        *ack = w_ack;
    }

    if (w_ack == ACK::OK) {
        writeData(data);
        return true;
    }

    return false;
}

bool DAP::writeFromPacket(uint32_t packet, uint32_t data, uint32_t retry_count) {
    if (isStopped()) {
        Logger::warn("Host is stopped. Will no longer write");
        return false;
    }
    if (retry_count == 0) {
        Logger::info("Retry Count Exceeded. Exiting...");
        return false;
    }

    Logger::debug("DAP::writeFromPacket: packet = 0x%x, data = 0x%08x", packet, data);
    ACK ack;
    writeFromPacketUnsafe(packet, data, &ack);

    switch (ack) {
    case ACK::OK:
        Logger::debug("DAP::writeFromPacket: ACK = OK");
        // Data was already written by the unsafe method
        if (writeDataErrSet()) {
            Logger::info("Parity error in write data sent. Resending data");
            return writeFromPacket(packet, data, retry_count - 1);
        }
        return true;
    case ACK::WAIT:
        Logger::debug("DAP::writeFromPacket: ACK = WAIT");
        return writeFromPacket(packet, data, retry_count - 1);
    case ACK::FAULT:
        Logger::debug("DAP::readFromPacket: ACK = FAULT");
        handleFault();
        return writeFromPacket(packet, data, retry_count - 1);
    }
    Logger::debug("DAP::writeFromPacket: ACK = ERROR");
    handleError();
    return false;
}

Optional<uint32_t> DAP::readPort(DP port) {
    // CTRLSEL needs to be set
    if (port == DP::WCR) {
        setCTRLSEL(1);
    }
    uint8_t packet = make_packet(port, RW::READ);
    Optional<uint32_t> data = readFromPacket(packet, 10);
    // Ensure that CTRL/STAT register is always availible
    if (port == DP::WCR) {
        setCTRLSEL(0);
    }
    return data;
}

Optional<uint32_t> DAP::readPort(AP port) {
    if (!APPoweredOn()) {
        initAP();
    }
    setAPBANKSEL(port);
    uint8_t packet = make_packet(port, RW::READ);
    // Read data can be dropped since it won't be
    // present on initial read
    readFromPacket(packet, 10);
    // Requires another read
    Optional<uint32_t> data = readPort(DP::RDBUFF);
    // AP experienced and error so RDBUFF is unreliable
    if (apErr()) {
        return Optional<uint32_t>::none();
    }
    return data;
}

bool DAP::writePort(DP port, uint32_t data) {
    // CTRLSEL needs to be set
    if (port == DP::WCR) {
        setCTRLSEL(1);
    }
    uint8_t packet = make_packet(port, RW::WRITE);
    bool write_success = writeFromPacket(packet, data, 10);
    // Ensure that CTRL/STAT register is always availible
    if (port == DP::WCR) {
        setCTRLSEL(0);
    }
    return write_success;
}

bool DAP::writePort(AP port, uint32_t data) {
    if (!APPoweredOn()) {
        initAP();
    }
    setAPBANKSEL(port);
    uint8_t packet = make_packet(port, RW::WRITE);
    bool write_success = writeFromPacket(packet, data, 10);
    // AP experienced an error
    if (apErr()) {
        return false;
    }
    // Based on trials, two short idle periods
    // completes the AP transaction
    if (write_success) {
        Logger::debug("DAP::writePort: Idle delay for AP write");
        idleShort();
        idleShort();
    }
    return write_success;
}

void DAP::idleShort() { driver->idleShort(); }

void DAP::idleLong() { driver->idleLong(); }

// private

void DAP::setAPBANKSEL(AP port) {
    uint32_t bank = getAPBANK(port);
    if (bank != m_current_banksel || m_current_banksel == DEFAULT_SEL_VALUE) {
        m_current_banksel = bank;
        updateSELECT();
    }
}

void DAP::setCTRLSEL(uint8_t ctrlsel) {
    uint32_t sel_bit = (bool)ctrlsel;
    if (sel_bit != m_current_ctrlsel || m_current_ctrlsel == DEFAULT_SEL_VALUE) {
        m_current_ctrlsel = sel_bit;
        updateSELECT();
    }
}

void DAP::updateSELECT() {
    writePort(DP::SELECT, (m_current_banksel == DEFAULT_SEL_VALUE ? 0x00 : m_current_banksel) |
                              (m_current_ctrlsel == DEFAULT_SEL_VALUE ? 0x00 : m_current_ctrlsel));
}

// Mostly for convenience
// Write to some bits in CSW
static bool writeCSWBits(DAP &dap, uint32_t bits, uint32_t mask) {
    Optional<uint32_t> csw = Optional<uint32_t>::none();
    if ((csw = dap.readPort(AP::CSW)).hasValue() &&
        dap.writePort(AP::CSW, (csw.getValue() & (~mask)) | bits)) {
        return true;
    }
    return false;
}

bool DAP::setDataLengthWord() {
    if (m_data_size == 4) {
        return true;
    }
    if (writeCSWBits(*this, 0b010, 0x7)) {
        Logger::info("Set memory transfers to word");
        m_data_size = 4;
        return true;
    }
    Logger::warn("Faced an issue when setting memory transfers to word");
    return false;
}

bool DAP::setDataLengthByte() {
    if (m_data_size == 1) {
        return true;
    }
    if (writeCSWBits(*this, 0b000, 0x7)) {
        Logger::info("Set memory transfers to byte");
        m_data_size = 1;
        return true;
    }
    Logger::warn("Faced an issue when setting memory transfers to byte");
    return false;
}

bool DAP::setAutoIncrementTAR(bool increment) {
    uint32_t bits = increment ? 0x10 : 0x00;
    if (bits == m_addr_int_bits) {
        return true;
    }
    if (writeCSWBits(*this, bits, 0x30)) {
        Logger::info("Updated AddrInc to 0x%02x", bits);
        m_addr_int_bits = bits;
        return true;
    }
    Logger::warn("Was not able to update AddrInc");
    return false;
}

inline void DAP::sendPacket(uint8_t packet) { driver->writeBits(packet, 8); }

ACK DAP::readACK() {
    uint32_t ack = driver->readBits(3);

    switch (ack) {
    case 0b001:
        return ACK::OK;
    case 0b010:
        return ACK::WAIT;
    case 0b100:
        return ACK::FAULT;
    default:
        return ACK::ERR;
    }
}

// Testing purposes
bool flip = false;

void DAP::writeData(uint32_t data) {
    uint32_t parity = calculate_data_parity(data);
    if (flip) {
        Logger::debug("DAP::writeData: Flipping Parity");
        parity = ~parity;
    }
    driver->writeBits(data, 32);
    driver->writeBits(parity, 1);
}

Optional<uint32_t> DAP::readData() {
    uint32_t data = driver->readBits(32);
    uint32_t parity = driver->readBits(1);
    if (calculate_data_parity(data) != parity) {
        Logger::info("Parity error detected in read data received.");
        return Optional<uint32_t>::none();
    }
    return Optional<uint32_t>::of(data);
}

inline bool DAP::writeDataErrSet() {
    uint32_t data = readPort(DP::CTRL_STAT).getValue();
    return (bool)(data & 0x80);
}

bool DAP::apErr() {
    if (m_ap_err) {
        Logger::info("AP error detected");
        m_ap_err = false;
        return true;
    }
    return false;
}

void DAP::handleFault() {
    // Cases that trigger a fault
    //  - Partiy error in the wdata
    //  - Error in AP transaction
    uint32_t ctrl_stat = readPort(DP::CTRL_STAT).getValue(); // Cant issue fault
    Logger::debug("DAP::handleFault: CTRL/STAT = 0x%08x", ctrl_stat);
    if (ctrl_stat & 0x80) { // WDATAERR
        Logger::warn("Parity Error in the previous write data sent detected.");
        writePort(DP::ABORT, 0x8); // WDERRCLR
    } else if (ctrl_stat & 0x20) { // STICKYERR
        Logger::info("Error in the previous in the AP transcation");
        writePort(DP::ABORT, 0x4); // STKERRCLR
        m_ap_err = true;
    } else {
        Logger::warn("Detected Fault. Left unhandlded");
    }
}

void DAP::handleError() {
    // Cases that cause an error typically cuase a desync between the target and the host
    // a line reset is tried before stopping the host
    // For the most part, when an error ACK (a lack of one) is read, a single turnaround
    // should be expected to check and see if the target exists at all
    driver->turnaround();
    if (readFromPacketUnsafe(make_packet(DP::IDCODE, RW::READ)).hasValue()) {
        Logger::info("Target resynced after error. Packet dropped");
        return;
    }

    // Reset line and reread again
    resetLine();
    Logger::warn("Resetting line due to an potentially out of sync DAP");

    if (!readFromPacketUnsafe(make_packet(DP::IDCODE, RW::READ)).hasValue()) {
        Logger::error("Was not able to read from target");
        // Might have been stopped by the reset line method
        if (!isStopped()) {
            stop();
        }
    }
}

} // namespace swd::dap
