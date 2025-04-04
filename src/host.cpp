#include "libswd/host.h"
#include "libswd/packet.h"
#include "libswd/optional.h"

// TODO: Remove Eventually
#include <Arduino.h>

namespace { // anonymous

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

uint8_t make_packet(swd::DP port, RW access) {
    Serial.println();
    uint8_t packet = PACKET_BASE | DP_Port | static_cast<uint8_t>(access);
    switch (port) {
    case swd::DP::ABORT:
    case swd::DP::IDCODE:
        Serial.println("make_packet: DP ABORT/IDCODE");
        packet |= Ax0;
        break;
    case swd::DP::CTRL_STAT:
    case swd::DP::WCR:
        Serial.println("make_packet: DP CTRL_STAT/WCR");
        packet |= Ax4;
        break;
    case swd::DP::RESEND:
    case swd::DP::SELECT:
        Serial.println("make_packet: DP RESET/SELECT");
        packet |= Ax8;
        break;
    case swd::DP::RDBUFF:
    case swd::DP::ROUTESEL:
        Serial.println("make_packet: DP RDBUFF/ROUTSEL");
        packet |= AxC;
        break;
    default:
        break;
    }
    set_packet_parity(&packet);
    return packet;
}

uint8_t make_packet(swd::AP port, RW access) {
    uint8_t packet = PACKET_BASE | AP_Port | static_cast<uint8_t>(access);
    switch (port) {
    case swd::AP::CSW:
    case swd::AP::DB0:
        Serial.println("make_packet: AP CSW/BD0");
        packet |= Ax0;
        break;
    case swd::AP::TAR:
    case swd::AP::DB1:
    case swd::AP::CFG:
        Serial.println("make_packet: AP TAR/BD1/CFG");
        packet |= Ax4;
        break;
    case swd::AP::DB2:
    case swd::AP::BASE:
        Serial.println("make_packet: AP BD2/BASE");
        packet |= Ax8;
        break;
    case swd::AP::DRW:
    case swd::AP::IDR:
    case swd::AP::DB3:
        Serial.println("make_packet: AP DRW/IDR/BD3");
        packet |= AxC;
        break;
    default:
        break;
    }
    set_packet_parity(&packet);
    return packet;
}

uint32_t getAPBANK(swd::AP port) {
    switch (port) {
    case swd::AP::CSW:
    case swd::AP::TAR:
    case swd::AP::DRW:
        return 0x00;
    case swd::AP::DB0:
    case swd::AP::DB1:
    case swd::AP::DB2:
    case swd::AP::DB3:
        return 0x10;
    case swd::AP::CFG:
    case swd::AP::BASE:
    case swd::AP::IDR:
        return 0xF0;
    default:
        return 0xF0;
    }
}

} // namespace

namespace swd {

void SWDHost::resetLine() {
    driver->init();
    if ( !readFromPacketUnsafe(make_packet(DP::IDCODE, RW::READ)).hasValue() ) {
        stop();
        return;
    }
    driver->idleShort();
    writePort(DP::ABORT, 0x1F); // Clear all errors on reset
}

void SWDHost::reset() {
    m_stop_host = false; 
    resetLine();
    m_ap_power_on = false;
    m_current_banksel = DEFAULT_SEL_VALUE;
    m_current_ctrlsel = DEFAULT_SEL_VALUE;
}

inline void SWDHost::stop() {
    Serial.println("Stopping Host");
    m_stop_host = true;
}

inline bool SWDHost::isStopped() {
    return m_stop_host;
}

void SWDHost::initAP() {
    Serial.println("Initializing Access Port");
    if (!(writePort(DP::CTRL_STAT, 0x50000000) && readPort(DP::CTRL_STAT).hasValue())) {
        Serial.println("Count not initalize AP port. Stopping host");
        stop();
        return;
    }
    m_ap_power_on = true;
}

// Private read and write methods
Optional<uint32_t> SWDHost::readFromPacketUnsafe(uint32_t packet, ACK *ack) {
    Serial.printf("SWDHost::readFromPacketUnsafe: packet = 0x%x\n\r", packet);
    sendPacket(packet);
    driver->turnaround();
    ACK rd_ack = readACK();
    Optional<uint32_t> data = Optional<uint32_t>::none();

    if (rd_ack == ACK::OK) {
        Serial.println("SWDHost::readFromPacketUnsafe: ACK = OK");
        data = Optional<uint32_t>::of(readData());
        driver->turnaround();
    }

    if (ack != nullptr) {
        *ack = rd_ack;
    }
    return data;
}

Optional<uint32_t> SWDHost::readFromPacket(uint32_t packet, uint32_t retry_count) {
    if (isStopped()) {
        Serial.println("SWDHost::writeFromPacket: Host is stopped. Will no longer respond");
        return Optional<uint32_t>::none();
    }

    Serial.printf("SWDHost::readFromPacket: Tries Left for this read: %lu\n\r", retry_count);
    if (retry_count == 0) {
        Serial.println("SWDHost::readFromPacket: Retry Count Exceeded. Exiting...");
        return Optional<uint32_t>::none();
    }

    ACK ack;
    Optional<uint32_t> data = readFromPacketUnsafe(packet, &ack);

    switch (ack) {
        case ACK::OK: {
            Serial.println("SWDHost::readFromPacket: ACK = Ok");
            return data;
        }
        case ACK::WAIT:
            Serial.println("SWDHost::readFromPacket: ACK = WAIT");
            driver->turnaround();
            return readFromPacket(packet, retry_count-1);
        case ACK::FAULT:
            driver->turnaround();
            Serial.println("SWDHost::readFromPacket: ACK = FAULT");
            handleFault();
            return readFromPacket(packet, retry_count-1);
    }
    Serial.println("SWDHost::readFromPacket: ACK = ERROR");
    handleError();
    return Optional<uint32_t>::none();
}


bool SWDHost::writeFromPacketUnsafe(uint32_t packet, uint32_t data, ACK *ack) {
    Serial.printf("SWDHost::writeFromPacketUnsafe: packet = 0x%x\n\r", packet);
    sendPacket(packet);
    driver->turnaround();
    ACK w_ack = readACK();
    driver->turnaround();

    // write method might return early, so save to reference now
    if (ack != nullptr) {
        *ack = w_ack;
    }

    if (w_ack == ACK::OK) {
        Serial.println("SWDHost::writeFromPacketUnsafe: ACK = OK");
        writeData(data);
        return true;
    }

    return false;
}


bool SWDHost::writeFromPacket(uint32_t packet, uint32_t data, uint32_t retry_count) {
    if (isStopped()) {
        Serial.println("SWDHost::writeFromPacket: Host is stopped. Will no longer respond");
        return false;
    }
    Serial.printf("SWDHost::writeFromPacket: Tries Left for this read: %lu\n\r", retry_count);
    if (retry_count == 0) {
        Serial.println("SWDHost::writeFromPacket: Retry Count Exceeded. Exiting...");
        return false;
    }

    ACK ack;
    writeFromPacketUnsafe(packet, data, &ack);

    switch (ack) {
        case ACK::OK: 
            Serial.println("SWDHost::writeFromPacket: ACK = OK");
            // Data was already written by the unsafe method
            return true;
        case ACK::WAIT:
            Serial.println("SWDHost::writeFromPacket: ACK = WAIT");
            return writeFromPacket(packet, data, retry_count-1);
        case ACK::FAULT:
            Serial.println("SWDHost::readFromPacket: ACK = FAULT");
            handleFault();
            return writeFromPacket(packet, data, retry_count-1);
    }
    Serial.println("SWDHost::writeFromPacket: ACK = ERROR");
    handleError();
    return false;
}

Optional<uint32_t> SWDHost::readPort(DP port) {
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

Optional<uint32_t> SWDHost::readPort(AP port) {
    if (!m_ap_power_on) {
        initAP();
    }
    setAPBANKSEL(port);
    Serial.printf("Current AP BANK: 0x%x\n\r", m_current_banksel);
    uint8_t packet = make_packet(port, RW::READ);
    Optional<uint32_t> data = readFromPacket(packet, 10);
    // If read was not successfull then return nothing
    if ( !data.hasValue() ) {
        return Optional<uint32_t>::none();
    }
    // Requires another read
    return readPort(DP::RDBUFF);
}

bool SWDHost::writePort(DP port, uint32_t data) {
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

bool SWDHost::writePort(AP port, uint32_t data) {
    if (!m_ap_power_on) {
        initAP();
    }
    setAPBANKSEL(port);
    uint8_t packet = make_packet(port, RW::WRITE);
    bool write_success = writeFromPacket(packet, data, 10);
    // Based on evidence, two short idle periods completes 
    // the AP transaction
    if ( write_success ) {
        idleShort();
        idleShort();
    }
    return write_success;
}

inline void SWDHost::idleShort() { driver->idleShort(); }

inline void SWDHost::idleLong() { driver->idleLong(); }

// private

void SWDHost::setAPBANKSEL(AP port) {
    uint32_t bank = getAPBANK(port);
    if (bank != m_current_banksel || m_current_banksel == DEFAULT_SEL_VALUE ) {
        m_current_banksel = bank;
        updateSELECT();
    }
}

void SWDHost::setCTRLSEL(uint8_t ctrlsel) {
    uint32_t sel_bit = (bool)ctrlsel;
    if (sel_bit != m_current_ctrlsel || m_current_ctrlsel == DEFAULT_SEL_VALUE) {
        m_current_ctrlsel = sel_bit;
        updateSELECT();
    }
}

void SWDHost::updateSELECT() {
    writePort(DP::SELECT, 
        (m_current_banksel == DEFAULT_SEL_VALUE ? 0x00 : m_current_banksel) |
        (m_current_ctrlsel == DEFAULT_SEL_VALUE ? 0x00 : m_current_ctrlsel)
    );
}

inline void SWDHost::sendPacket(uint8_t packet) { driver->writeBits(packet, 8); }

ACK SWDHost::readACK() {
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

// TODO: REMOVE ME AFTER TESTING
bool flip = false;

void SWDHost::writeData(uint32_t data) {
    uint32_t parity = calculate_data_parity(data);
    // TODO: REMOVE ME AFTER TESTING
    if (flip) {
        Serial.println("SWDHost::writeData: Testing With flipped parity");
        parity = ~parity;
    }
    driver->writeBits(data, 32);
    driver->writeBits(parity, 1);
    Serial.printf("SWDHost::writeData: Data Written: 0x%x, parity: 0x%x\n\r", data, parity & 0x1);
}

uint32_t SWDHost::readData() {
    uint32_t data = driver->readBits(32);
    uint32_t parity = driver->readBits(1);
    if (calculate_data_parity(data) != parity) {
    }
    Serial.printf("SWDHost::readData: Data Read: 0x%x, parity: 0x%x\n\r", data, parity);
    return data;
}

void SWDHost::handleFault() {
    // Cases that trigger a fault
    //  - Partiy error in the wdata
    uint32_t ctrl_stat = readPort(DP::CTRL_STAT).getValue();
    if (ctrl_stat & 0x80) { // WDATAERR
        Serial.println("Parity Error in the previous write data sent detected.");
        writePort(DP::ABORT, 0x8); // WDERRCLR
    } else {
        Serial.println("Detected Fault. Left unhandlded");
    }
}

void SWDHost::handleError() {
    // Cases that cause an error typically cuase a desync between the target and the host
    // a line reset is tried before stopping the host
    // For the most part, when an error ACK (a lack of one) is read, a single turnaround
    // should be expected to check and see if the target exists at all
    driver->turnaround();
    if ( readFromPacketUnsafe(make_packet(DP::IDCODE, RW::READ)).hasValue() ) {
        Serial.println("SWDHost::handleError: Target resynced. Packet dropped");
        return;
    }

    // Reset line and reread again
    resetLine();
    Serial.println("SWDHost::handleError: Target reset. Testing Read");

    if ( !readFromPacketUnsafe(make_packet(DP::IDCODE, RW::READ)).hasValue() ) {
        Serial.println("SWDHost::handleError: Cannot read from target, stopping host");
        stop();
    }

}

} // namespace swd
