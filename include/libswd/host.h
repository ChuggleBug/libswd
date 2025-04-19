
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

    explicit SWDHost(SWDDriver *driver) : m_dap(dap::DAP(driver)) {}
    ~SWDHost() {}

    // Host Operations
    void reset();
    void stop();
    bool isStopped();

    // Target operations
    bool haltTarget();
    bool stepTarget(); 
    bool resetTarget();
    bool haltAfterResetTarget();
    bool continueTarget();
    bool isTargetHalted();

    // Target DAP operations
    bool isDAPStopped();

    // High Level Debug Operations
    bool memoryWrite(uint32_t address, uint32_t data);
    bool memoryWriteBlock(uint32_t base, uint32_t *data_buf, uint32_t data_size);
    bool memoryWriteBlock(uint32_t base, uint8_t *data_buf, uint32_t data_size);

    Optional<uint32_t> memoryRead(uint32_t address);
    bool memoryReadBlock(uint32_t base, uint32_t *data_buf, uint32_t data_size);

    Optional<uint32_t> registerRead(target::REG reg);
    bool registerWrite(target::REG reg, uint32_t data);

    bool addBreakpoint(uint32_t addr);
    void getBreakpoints(Optional<uint32_t> *bkpts);

  private:
    const uint32_t DEFAULT_RETRY_COUNT = 10;

    bool m_host_stopped = false;

    uint32_t fpb_cmp_cnt = 0;
    dap::DAP m_dap;
    Optional<uint32_t> m_bkpts[6];
    target::FBP_VER m_fbv_version;

    void setFBPVersion();

    bool FBPEnable();
    // void FPBsetC
};

} // namespace swd

#endif // __SWD_HOST_h