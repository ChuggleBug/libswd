
#ifndef __SWD_HOST_H
#define __SWD_HOST_H

#include "driver.h"
#include "optional.h"
#include "packet.h"

namespace swd {

enum class ACK { OK = 0b001, WAIT = 0b010, FAULT = 0b100, ERR = 0b111 };

class SWDHost {
  public:
    SWDHost(SWDDriver *d) : driver(d) {}
    ~SWDHost() {}

    // Trigger a line reset for the swd protocol
    // Additionally triggers the JTAG to SWD sequence
    // Returns whether or not a reset was successful
    bool resetLine();

    // Powers on the AP module using CTRL_STAT
    void initAP();

    // Stops host from being able to send packets
    void stopHost();

    // Reads to DP WCR toggles the CTRLSEL bit
    Optional<uint32_t> readPort(DP port);
    Optional<uint32_t> readPort(AP port);

    bool writePort(DP port, uint32_t data);
    bool writePort(AP port, uint32_t data);

    // Refer to drivers declarations for these functions meanings
    void idleShort();
    void idleLong();

    // TODO: Move to private
    Optional<uint32_t> readFromPacket(uint32_t packet, uint32_t retry_count);
    bool writeFromPacket(uint32_t packet, uint32_t data, uint32_t retry_count);

  private:
    SWDDriver *driver;

    // Host/Target flags
    bool m_stopHost = false;    // Target has some unrecoverable error and host needs to be stopped
    bool m_ap_power_on = false; // Target's AP port is powered on

    // BANKSEL and CTRLSEL bits
    // Used to prevent repetitive reads and writes to the target
    const uint32_t DEFAULT_SEL_VALUE = 0xbeefcafe;
    uint32_t m_current_banksel = DEFAULT_SEL_VALUE; // Force set on first time
    uint32_t m_current_ctrlsel = DEFAULT_SEL_VALUE; // Force set on first time

    // Generic SELECT write
    void updateSELECT();

    // AP read and writes require the APBANKSEL
    // fields in the SELECT register
    void setAPBANKSEL(AP port);

    // Some DP reads (CTRL_STAT and WCR) require
    // the CTRLSEL bit set in the SELECT register
    void setCTRLSEL(uint8_t ctrlsel);

    // Fault and Error status handling
    void handleFault();
    void handleError(uint32_t retry_count);

    // Generic flow for the protocol
    // Does not handle sending bits
    void sendPacket(uint8_t packet);
    ACK readACK();
    void writeData(uint32_t data);
    uint32_t readData();
};

} // namespace swd

#endif // __SWD_HOST_H