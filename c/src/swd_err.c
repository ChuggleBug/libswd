

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

#ifdef SWD_DISABLE_UNDEFINED_PORT
    case SWD_DAP_UNDEFINED_PORT:
        return "SWD DAP Undefined Port";
#endif // SWD_DISABLE_UNDEFINED_PORT
    default:
        return "SWD Unknown Error Value";
    }
}