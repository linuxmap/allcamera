/**
* @file    SvsLoger.h
* @brief   Svs协议栈日志输出类
*
* Copyright (c) 2010 AllCam Tech.Co.,Ltd
*
* @date      2010-12-10
*/

#ifndef __SVSLOGER_H__
#define __SVSLOGER_H__

#include "svs_vms_stack.h"

class CSvsLoger: public ISvsLogger
{
public:
    CSvsLoger();
    virtual ~CSvsLoger();

    static CSvsLoger *instance()
    {
        static CSvsLoger svsLoger;
        return &svsLoger;
    }

    void writeLog(uint8_t ucType,
                  uint8_t ucLevel,
                  const char *pszLog,
                  uint32_t unLogLength,
                  char* pFile,
                  int32_t lLine);
};

#endif /* __SVSLOGER_H__ */
