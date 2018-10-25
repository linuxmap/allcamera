#ifndef __CSTREAMUTILITY_H__
#define __CSTREAMUTILITY_H__

#include <svs_ace_header.h>

class CStreamUtility
{
public:
    // 比较两个值的大小
    static bool less(uint16_t x, uint16_t y);
    static bool less(uint32_t x, uint32_t y);

};

#endif

