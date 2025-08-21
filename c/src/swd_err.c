

#include "swd_err.h"

const char *swd_err_as_str(swd_err_t err) {
    switch (err) {
    case SWD_OK:
        return "SWD OK";
    case SWD_ERR:
        return "SWD General Error";
    case SWD_DAP_NOT_STARTED:
        return "SWD DAP Not Started";
    case SWD_DAP_INVALID_PORT_OP:
        return "SWD DAP Invalid Port Operation";
    case SWD_DAP_START_ERR:
        return "SWD DAP Start Error";
    case SWD_HOST_NOT_STARTED:
        return "SWD Host Not Started";
    case SWD_TARGET_NOT_HALTED:
        return "SWD Target Not Halted";
    case SWD_HOST_START_ERR:
        return "SWD Host Start Error";
    case SWD_TARGET_INVALID_ADDR:
        return "SWD Target Invalid Address";
    case SWD_TARGET_NO_MORE_BKPT:
        return "SWD Target No More Breakpoints";

#ifdef SWD_DISABLE_UNDEFINED_PORT
    case SWD_DAP_UNDEFINED_PORT:
        return "SWD DAP Undefined Port";
#endif // SWD_DISABLE_UNDEFINED_PORT
    default:
        return "SWD Unknown Error Value";
    }
}