
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

  private:
    SWDDriver *driver;

    // Generic flow for the protocol
    // All class methods assume they are able to
    // being sending bits
    void sendPacket(uint8_t packet);

    // Does not handle the turnaround since different RW operations
    // require different period
    ACK readACK();

    void writeData(uint32_t data);
    uint32_t readData();
};

} // namespace swd

#endif // __SWD_HOST_H