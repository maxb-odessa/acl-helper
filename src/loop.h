
/** \file */

#ifndef __ACLH_LOOP_H__
#define __ACLH_LOOP_H__

#include "misc.h"

//! max squid input line size (urls may be very long...)
#define SQUID_BUF_SIZE    65535

//! max squid input line tokens
#define SQUID_MAX_TOKENS  64 

extern int loop_run(void);

#endif //__ACLH_LOOP_H__


