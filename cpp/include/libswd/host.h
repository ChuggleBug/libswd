
#ifndef __SWD_HOST_H
#define __SWD_HOST_H


#include "dap.h"
#include "target_registers.h"
#include "optional.h"

namespace swd {

namespace target {
    enum class FBP_VER : uint32_t;
}

class SWDHost {
  public:

    explicit SWDHost(SWDDriver *driver);
    ~SWDHost();

    // Host Operations
    /**
     * @brief Resets the host by running the DAP's initalization
     *  sequence in addition to resetting and halting the target
     *  device
     */
    void reset();
    /**
     * @brief Stops the host. Read and write operations should
     *  not return any value if stopped
     */
    void stop();
    /**
     * @brief Checks if host it stoppped
     */
    bool isStopped();

    // Target Process Operations
    // All of these listed functions return whether or not
    //  the operations was successful
    /**
     * @brief Halts the processor, by entering the debug sate. 
     *  Writes to DHCSR.C_HALT
     */
    bool haltTarget();
    /**
     * @brief Performs a single step instruction by writing 
     *  to DHSR.C_HALT
     */
    bool stepTarget();
    /**
     * @brief Performs a sysyem reset. This includes the the System
     *  Control space (with the exception of debug addresses) and
     *  peripheral space addresses. Writes to AIRCR.SYSRESETREQ
     */
    bool resetTarget();
    /**
     * @brief In addition to requesting a system reset, configures the
     *  target to catch and halt when a reset interrupt occurrs. 
     *  Writes to DEMCR.VC_CORERESET (and enables debug in DHCSR).
     *  Additionally, the host disables reset interrupt catching after halting.
     */
    bool haltAfterResetTarget();
    /**
     * @brief Disables halting and stepping by clearing associated 
     *  halt and step bits in DHCSR
     */
    bool continueTarget();

    // Target status operations
    /**
     * @brief Checks if the target is halted. Read DHCSR.S_HALTED
     */
    bool isTargetHalted();


    // Target DAP operations
    /**
     * @brief Checks if the DAP is stopped. In most cases if the
     *  If DAP is stopped, most target operations will fail.
     *  A reset should be done if this is the case
     */
    bool isDAPStopped();


    // High Level Debug Operations
    // In most cases, a true or an Optional with a value is 
    //  returned if the operation was successful
    /**
     * @brief writes 32 bits of data to a specified address
     *  Non-word aligned memory writes leads to unintended behavior
     *  Memory writes to disallowed spaces will return false positives
     * 
     * @param address   32 bit address to write to
     * @param data      32 bit data which will be written
     */
    bool memoryWrite(uint32_t address, uint32_t data);
    /**
     * @brief Sequentially writes a list of data at a specified base
     *  address. Sequential data will be word aligned. Fails if
     *  any single write fails
     * 
     * @param base      32 bit address of the inital data
     * @param data_buf  List of 32 bit data values
     * @param data_size Size of list
     */
    bool memoryWriteBlock(uint32_t base, uint32_t *data_buf, uint32_t data_size);
    /**
     * @brief Sequentially writes a list of data at a specified base
     *  address. Sequential data will be byte aligned. Endianness is handled
     *  by byte laning data when writing to DRW. Fails is the CSW cannot be
     *  configured to specify byte transfers or if any single write fails
     * 
     * @param base      32 bit address of the inital data
     * @param data_buf  List of 32 bit data values
     * @param data_size Size of list
     */
    bool memoryWriteBlock(uint32_t base, uint8_t *data_buf, uint32_t data_size);

    /**
     * @brief Reads the 32 bit value in the specified memory address
     * 
     * @param address   32 bit address to read from
     */
    Optional<uint32_t> memoryRead(uint32_t address);
    /**
     * @brief Read a set of ammount of words begining at a base value.
     *  Fails if any single read fails. Compared to single memory
     *  address reads, optionals are not used in this situation.
     * 
     * @param base      32 bit address of initial data
     * @param data_buf  List of 32 bit values to store read data to
     * @param data_size Size of list
     */
    bool memoryReadBlock(uint32_t base, uint32_t *data_buf, uint32_t data_size);
    /**
     * @brief Read a set of ammount of words begining at a base value.
     *  Fails if byte transfers can not be configured  or if any 
     *  single read fails. Endianness is handled by byte laning data 
     *  when reading from the DRW. Compared to single memory address reads, 
     *  optionals are not used in this situation.
     * 
     * @param base      32 bit address of initial data
     * @param data_buf  List of 32 bit values to store read data to
     * @param data_size Size of list
     */
    bool memoryReadBlock(uint32_t base, uint8_t *data_buf, uint32_t data_size);

    /**
     * @brief 
     * 
     * @param reg 
     * @return Optional<uint32_t> 
     */
    Optional<uint32_t> registerRead(target::REG reg);
    bool registerWrite(target::REG reg, uint32_t data);


    bool enableFPB(bool trigger);
    bool supportsFlashPatch();

    // Returns 0 on error
    uint32_t getCodeCompCount();
    uint32_t getLiteralCompCount();
    // Technically a code comparator can compare two addresses
    // So this needs to be determined at runtime
    uint32_t getBreakpointCount();

    // TODO: Implement support for FPBv2
    // Code comparison operations
    bool addBreakpoint(uint32_t addr);
    bool removeBreakpoint(uint32_t addr);
    // Returns the address of all breakpoints, both enabled and disabled
    uint32_t getBreakpoints(uint32_t *bkpts);
    bool containsBreakpoint(uint32_t addr);
    bool enableBreakpoint(uint32_t addr, bool trigger);

    // Literal Address Comparisons
    bool setRemapAddress(uint32_t addr);
    bool resetRemapAddress();
    Optional<uint32_t> getRemapAddress();

    bool addRemapComparator(uint32_t addr);
    bool removeRemapComparator(uint32_t addr);
    uint32_t getRemapComparators(uint32_t *remaps);
    bool containsRemapComparator(uint32_t addr);
    bool enableRemalComparator(uint32_t addr, bool trigger);

  private:
    static const uint32_t DEFAULT_RETRY_COUNT = 10;

    // Max allowable instruction and 
    // literal address comparators
    static constexpr uint32_t MAX_CODE_CMP = 127;
    static constexpr uint32_t MAX_LIT_CMP = 15;

    bool m_host_stopped = false;

    dap::DAP m_dap;

    // FBV associated values
    target::FBP_VER m_fbv_version;
    bool m_supports_fp = false;

    // Number of implemented instruction and 
    // literal address comparators
    uint32_t m_num_code_cmp = 0;
    uint32_t m_num_lit_cmp = 0;

    // Number of breakpoints set
    // becuase a code comparator can map to two addresses
    // this value can be different than the number 
    // of set comparators
    uint32_t m_num_bkpt = 0;

    uint32_t m_code_cmp[MAX_CODE_CMP];
    uint32_t m_lit_cmp[MAX_LIT_CMP];


    void readFBPConfigs();
    void resetFPComparators();

    int32_t getBreakpointIndex(uint32_t addr);
    int32_t getFPComparatorIndex(uint32_t addr);
};

} // namespace swd

#endif // __SWD_HOST_H