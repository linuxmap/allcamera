/******************************************************************************

                  版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文 件 名   : StreamUtility.cpp
  版 本 号   : 初稿
  生成日期   : 2011年7月5日
  最近修改   :
  功能描述   : 工具函数
  函数列表   :
  修改历史   :
  1.日    期   : 2011年7月5日
    修改内容   : 创建文件

******************************************************************************/
#include "svs_adapter_utility.h"

bool CStreamUtility::less(uint16_t x, uint16_t y)
{
    if (x < y)
    {
        if (y - x > 0x8000)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        if (x - y > 0x8000)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}

bool CStreamUtility::less(uint32_t x, uint32_t y)
{
    if (x < y)
    {
        if (y - x > 0x80000000)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        if (x - y > 0x80000000)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}





