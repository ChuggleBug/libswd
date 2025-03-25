
#ifndef __SWD_HOST_H
#define __SWD_HOST_H

#include "driver.h"
#include "packet.h"

namespace swd {

enum class ACK { OK = 0b001, WAIT = 0b010, FAULT = 0b100, ERR = 0b111 };

class SWDHost {
  public:
    SWDHost(SWDDriver *d) : driver(d) {}
    ~SWDHost() {}

    void resetLine();

    uint32_t readPort(DP port);
    uint32_t readPort(AP port);

    void writePort(DP port, uint32_t data);
    void writePort(AP port, uint32_t data);

    // Refer to drivers declarations for these functions meanings
    void idleShort();
    void idleLong();

  private:
    SWDDriver *driver;

    // AP read and writes require the APBANKSEL
    // fields in the SELECT register
    void setAPBANKSEL(AP port);

    // Some DP reads (CTRL_STAT and WCR) require
    // the CTRLSEL bit set in the SELECT register
    void setCTRLSEL(uint8_t ctrlsel);

    // Generic flow for the protocol
    // Does not handle sending bits
    void sendPacket(uint8_t packet);
    ACK readACK();
    void writeData(uint32_t data);
    uint32_t readData();
};

} // namespace swd

#endif // __SWD_HOST_H