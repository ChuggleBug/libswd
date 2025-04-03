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

bool SWDHost::resetLine() {
    driver->init();
    if ( readPort(DP::IDCODE).hasValue() ) {
        driver->idleShort();
        writePort(DP::ABORT, 0x1F); // Clear all errors on reset
        return true;
    }
    return false;
}

void SWDHost::initAP() {
    writePort(DP::CTRL_STAT, 0x50000000);
    Optional<uint32_t> powerup_ack = Optional<uint32_t>::none();
    uint32_t retry_count = 10;
    do {
        powerup_ack = readPort(DP::CTRL_STAT);
        if (!powerup_ack.hasValue()) {
            retry_count--;
        }
        if (retry_count == 0) {
            Serial.println("Count not initalize AP port. Stopping host");
            stopHost();
            return;
        }
    } while (!powerup_ack.hasValue() && powerup_ack.getValue() != 0xF0000000 );
    m_ap_power_on = true;
}


void SWDHost::stopHost() {
    m_stopHost = true;
}


// Private read and write methods
Optional<uint32_t> SWDHost::readFromPacket(uint32_t packet, uint32_t retry_count) {
    if (m_stopHost) {
        Serial.println("SWDHost::writeFromPacket: Host is stopped. Will no longer respond");
        return Optional<uint32_t>::none();
    }

    Serial.printf("SWDHost::readFromPacket: Tries Left for this read: %lu\n\r", retry_count);
    if (retry_count == 0) {
        Serial.println("SWDHost::readFromPacket: Retry Count Exceeded. Exiting...");
        return Optional<uint32_t>::none();
    }

    sendPacket(packet);
    driver->turnaround();
    ACK ack = readACK();
    Serial.printf("SWDHost::readFromPacket: ACK = 0x%x\n\r", ack);
    switch (ack) {
        case ACK::OK: {
            Serial.println("SWDHost::readFromPacket: ACK = OK");
            uint32_t data = readData();
            driver->turnaround();
            return Optional<uint32_t>::of(data);
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
    handleError(retry_count-1);
    return Optional<uint32_t>::none();
}

bool SWDHost::writeFromPacket(uint32_t packet, uint32_t data, uint32_t retry_count) {
    if (m_stopHost) {
        Serial.println("SWDHost::writeFromPacket: Host is stopped. Will no longer respond");
        return false;
    }
    Serial.printf("SWDHost::writeFromPacket: Tries Left for this read: %lu\n\r", retry_count);
    if (retry_count == 0) {
        Serial.println("SWDHost::writeFromPacket: Retry Count Exceeded. Exiting...");
        return false;
    }

    sendPacket(packet);
    driver->turnaround();

    ACK ack = readACK();
    driver->turnaround();
    switch (ack) {
        case ACK::OK: 
            Serial.println("SWDHost::writeFromPacket: ACK = OK");
            writeData(data);
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
    handleError(retry_count-1);
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
    if (data.hasValue()) { Serial.println("Data was read"); } 
    else { Serial.println("Data was not read"); }
    
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
    if ( write_success ) {
        // TODO: check if all write to AP need this
        idleShort();
        return true;
    }
    return false;
}

void SWDHost::idleShort() { driver->idleShort(); }

void SWDHost::idleLong() { driver->idleLong(); }

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

void SWDHost::sendPacket(uint8_t packet) { driver->writeBits(packet, 8); }

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

void SWDHost::handleError(uint32_t retry_count) {
    // Cases that cause an error typically cuase a desync between the target and the host
    // a line reset is tried before stopping the host
    if ( retry_count == 0 ) {
        Serial.println("SWDHost::handleError: Could not resync target and host. Trying a line reset");
        bool reset_success = resetLine();
        if ( !reset_success ) {
            Serial.println("SWDHost::handleError: Line reset was not successful. Stopping host");
            stopHost();
            return;
        }
    }
    // Try to continue to read a ID code
    readFromPacket(make_packet(DP::IDCODE, RW::READ), retry_count);
}

} // namespace swd
