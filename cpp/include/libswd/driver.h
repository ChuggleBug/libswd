// host.h

#ifndef __SWD_DRIVER_H
#define __SWD_DRIVER_H

#include <stdint.h>

namespace swd {

/**
 * Template for the set of functions which a device needs to implement
 * to properly use SWDHost. Any setup should be implemented inside of
 * the derived class'ss constructor. Direction configuration is handled
 * by SWDHost
 */
class SWDDriver {
  public:
    // By default a driver does not have any setup
    SWDDriver() {};
    virtual ~SWDDriver() {};

    /**
     * Data Read and Write operations. Support for 32 bits.
     * Writes from least significant bit first
     */
    uint32_t readBits(uint8_t cnt);
    void writeBits(uint32_t data, uint8_t cnt);

    /**
     * Runs the JTAG to SWD sequence. In the case where SWD was
     * configured by default for the target. It resets the
     * target after the special bit sequence was sent
     */
    void init();

    /**
     * Resets a JTAG or SWD device to their default state
     */
    void resetTarget();

    /**
     * Perform a set amount of turnarounds
     */
    void turnaround(uint32_t trn = 1);

    /**
     * In some cases, the host needs to wait for a couple of cycles for
     * the target DAP to configure itself. The target typically indicates
     * this with a WAIT ack response. The host then needs to idle for
     * a couple of cycles before it can resend any message
     */
    void idleShort();

    /**
     * In synchronous SWD, when the host wants to stop cycling the clock,
     * it must send at least 8 clock rising edges. After this point, it
     * can leave the driver idle
     */
    void idleLong();

    /**
     * Configuring SWDIO as either an input or output pin
     */
    virtual void cfgSWDIOOut() = 0;
    virtual void cfgSWDIOIn() = 0;

  protected:
    // Hardware specific implementation
    virtual uint8_t readSWDIO() = 0;
    virtual void writeSWDIO(uint8_t b) = 0;
    virtual void setSWCLK() = 0;
    virtual void clearSWCLK() = 0;
    virtual void hold() = 0;
};

} // namespace swd

#endif // __SWD_DRIVER_H