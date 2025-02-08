// libswd.h

#ifndef __SWD_H
#define __SWD_H

#include <stdint.h>

namespace swd {

/**
* SWD ACK responses are driven LSB
* first
*/
enum ACK {
    OK = 0b001,
    WAIT = 0b010,
    FAULT = 0b100, // typo in ADI 5.3.5???
    ERR = 0b111 
};

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
    * Configuring SWDIO as either an input or output pin
    */
    virtual void cfgSWDIOOut() = 0;
    virtual void cfgSWDIOIn() = 0;

    /**
    * Reading and writing bits. 
    */
    uint32_t readBits(uint8_t cnt);
    void writeBits(uint32_t data, uint8_t cnt);

    /**
    * Single turnaround period when no device drives the clock
    */
    void turnaround();

   protected:
    // Hardware specific implementation
    virtual uint8_t readSWDIO() = 0;
    virtual void writeSWDIO(uint8_t b) = 0;
    virtual void setSWCLK() = 0;
    virtual void clearSWCLK() = 0;
    virtual void hold() = 0;
};

class SWDHost {
   private:
    SWDDriver *driver;

   public:
    // Constructors and destructors
    explicit SWDHost(SWDDriver *d)
    : driver(d) {}
    ~SWDHost() { delete driver; }

    
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
    * Send a request packet to the target device 
    * @param uint8_t packet     The issued SW-DP packet request
    */
    void sendPacket(uint8_t packet);
    
    /**
    * Perform a set amount of turnarounds
    */
    void turnaround(uint32_t trn = 1);

    /**
    * Read an ACK response sent from the target device.
    * bits 2:0 are used to hold the data
    * @return uint8_t   ACK returned from target
    */
    uint8_t readACK();

    /**
    * Data Read and Write operations. Support for 32 bits. 
    * Writes from least significant bit first
    */
    uint32_t readBits(uint8_t cnt);
    void writeBits(uint32_t data, uint8_t cnt);
};


} // namespace swd

#endif // __SWD_H