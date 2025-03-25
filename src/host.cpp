#include "libswd/host.h"
#include "libswd/packet.h"

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
    readPort(DP::IDCODE);
    driver->idleShort();
}

uint32_t SWDHost::readPort(DP port) {
    if (port == DP::CTRL_STAT) {
        setCTRLSEL(0);
    } else if (port == DP::WCR) {
        setCTRLSEL(1);
    }
    uint8_t packet = make_packet(port, RW::READ);
    Serial.printf("SWDHost::readPort: Debug Port: 0x%x\n", packet);
    sendPacket(packet);
    driver->turnaround();

    ACK ack = readACK();
    Serial.printf("SWDHost::readPort: Debug Port ACK: 0x%x\n", ack);
    uint32_t data = readData();
    driver->turnaround();
    return data;
}

uint32_t SWDHost::readPort(AP port) {
    setAPBANKSEL(port);
    uint8_t packet = make_packet(port, RW::READ);
    Serial.printf("SWDHost::readPort: Access Port: 0x%x\n", packet);
    sendPacket(packet);
    driver->turnaround();

    ACK ack = readACK();
    Serial.printf("SWDHost::readPort: Access Port ACK: 0x%x\n", ack);
    uint32_t data = readData();
    driver->turnaround();

    return data;
}

void SWDHost::writePort(DP port, uint32_t data) {
    if (port == DP::CTRL_STAT) {
        setCTRLSEL(0);
    } else if (port == DP::WCR) {
        setCTRLSEL(1);
    }
    uint8_t packet = make_packet(port, RW::WRITE);
    Serial.printf("SWDHost::writePort: Debug Port: 0x%x\n", packet);
    sendPacket(packet);
    driver->turnaround();

    ACK ack = readACK();
    Serial.printf("SWDHost::writePort: Debug Port ACK: 0x%x\n", ack);
    // Write needs a turnaround period from host to target
    driver->turnaround();

    writeData(data);
}

void SWDHost::writePort(AP port, uint32_t data) {
    setAPBANKSEL(port);
    uint8_t packet = make_packet(port, RW::WRITE);
    Serial.printf("SWDHost::writePort: Access Port: 0x%x\n", packet);
    sendPacket(packet);
    driver->turnaround();

    ACK ack = readACK();
    Serial.printf("SWDHost::writePort: Access Port ACK: 0x%x\n", ack);
    // Write needs a turnaround period from host to target
    driver->turnaround();

    writeData(data);
}

void SWDHost::idleShort() { driver->idleShort(); }

void SWDHost::idleLong() { driver->idleLong(); }

// private

void SWDHost::setAPBANKSEL(AP port) { writePort(DP::SELECT, getAPBANK(port)); }

void SWDHost::setCTRLSEL(uint8_t ctrlsel) { writePort(DP::SELECT, (bool)ctrlsel); }

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

void SWDHost::writeData(uint32_t data) {
    driver->writeBits(data, 32);
    driver->writeBits(calculate_data_parity(data), 1);
    Serial.printf("SWDHost::writeData: Data Written: 0x%x, parity: 0x%x\n\n", data,
                  calculate_data_parity(data));
}

uint32_t SWDHost::readData() {
    uint32_t data = driver->readBits(32);
    uint32_t parity = driver->readBits(1);
    if (calculate_data_parity(data) != parity) {
    }
    Serial.printf("SWDHost::readData: Data Read: 0x%x, parity: 0x%x\n\n", data, parity);
    return data;
}

} // namespace swd