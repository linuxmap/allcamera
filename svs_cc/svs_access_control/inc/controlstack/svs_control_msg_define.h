#ifndef __ACCESS_CONTROL_MSG_DEFINE_H__
#define __ACCESS_CONTROL_MSG_DEFINE_H__
#include <svs_ace_header.h>
#include "vms/vms.h"
#include <map>
#include <list>


enum enAC_HTTP_THREAD
{
    AC_HTTP_THREAD_REACTOR   = 0,
    AC_HTTP_THREAD_CHECKMSG  = 1,
    AC_HTTP_THREAD_MAX
};

#endif

