
#ifndef __SWD_DAP_H
#define __SWD_DAP_H

#include "driver.h"
#include "optional.h"
#include "port.h"

namespace swd::dap {

enum class ACK;

class DAP {
  public:
    explicit DAP(SWDDriver *d) : driver(d) {}
    ~DAP() {}

    // Stops host from being able to send packets
    void stop();

    // Resets the host to its initial state
    void reset();

    // Status checking
    bool isStopped();            // if dap is stopped
    bool isTargetLittleEndian(); // if target is little endian

    // Reads to DP WCR toggles the CTRLSEL bit
    Optional<uint32_t> readPort(DP port);
    Optional<uint32_t> readPort(AP port);

    bool writePort(DP port, uint32_t data);
    bool writePort(AP port, uint32_t data);

    // Refer to drivers declarations for these functions meanings
    void idleShort();
    void idleLong();

    // Configurations for the DAP
    // Presets are handled by setConfigs
    // but can be changed later
    bool setDataLengthWord();
    bool setDataLengthByte();
    bool setAutoIncrementTAR(bool increment);

  private:
    SWDDriver *driver;

    // Host/Target flags
    bool m_stop_host = false;   // Target has some unrecoverable error and host needs to be stopped
    bool m_ap_power_on = false; // Target's AP port is powered on
    bool m_ap_err = false;      // Triggered in the event that the AP had an error or aborted
    bool m_is_little_endian = true; // Is little endain in most cases, but check either way
    uint8_t m_data_size = -1;       // Width of data transfers specified by CSW.Size
    uint8_t m_addr_int_bits = -1;   // Bits assigned to CSW.AddrInc

    // BANKSEL and CTRLSEL bits
    // Used to prevent repetitive reads and writes to the target
    const uint32_t DEFAULT_SEL_VALUE = 0xbeefcafe;
    uint32_t m_current_banksel = DEFAULT_SEL_VALUE; // Force set on first time
    uint32_t m_current_ctrlsel = DEFAULT_SEL_VALUE; // Force set on first time

    // Powers on the AP module using CTRL_STAT
    // Additionally waits for the target to
    // provide its power up ACKs
    void initAP();
    bool APPoweredOn();

    // Check if the AP has an error
    // clears the error flag if set
    bool apErr();

    // Sets up any required configurations for the device
    void setConfigs();

    // Trigger a line reset
    void resetLine();

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
    void handleError();

    // Generic variants for reading and writing to a target
    Optional<uint32_t> readFromPacket(uint32_t packet, uint32_t retry_count = 10);
    bool writeFromPacket(uint32_t packet, uint32_t data, uint32_t retry_count = 10);

    // Unsafe Generic variants of reading and writing methods
    // Procceeds with the OK flow of control, and returns error values
    // on non OK cases
    Optional<uint32_t> readFromPacketUnsafe(uint32_t packet, ACK *ack = nullptr);
    bool writeFromPacketUnsafe(uint32_t packet, uint32_t data, ACK *ack = nullptr);

    // Checks if the previous data write was successful by
    // checking the CTRL/STAT WDATAERR field
    bool writeDataErrSet();

    // Generic flow for the protocol
    // Does not handle sending bits
    void sendPacket(uint8_t packet);
    ACK readACK();
    void writeData(uint32_t data);
    Optional<uint32_t> readData();
};

} // namespace swd::dap

#endif // __SWD_DAP_H