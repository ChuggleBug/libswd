
#ifndef __SWD_ERR_H
#define __SWD_ERR_H

#include "swd_conf.h"

#ifdef SWD_DO_RUNTIME_ASSERT
#define SWD_ASSERT(cond) do { \
        if (!(cond)) { \
            while (1); \
        } \
    } while (0) \

#define SWD_ASSERT_OK(expr) SWD_ASSERT((expr == SWD_OK))

#else 

#define SWD_ASSERT(cond)
#define SWD_ASSERT_OK(expr)

#endif // defined(SWD_DO_RUNTIME_ASSERT)

typedef enum _swd_err_t {
    SWD_OK = 0,
    SWD_ERR,
} swd_err_t;

#endif // __SWD_ERR_H