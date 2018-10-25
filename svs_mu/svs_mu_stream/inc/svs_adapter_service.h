/**
* @file    MduService.h
* @brief   Mdu服务的总入口，负责服务整体拉起
*
* Copyright (c) 2010 AllCam Tech.Co.,Ltd
*
* @date      2010-12-11
*/

#ifndef __MDUSERVICE_H__
#define __MDUSERVICE_H__
#ifdef __cplusplus
extern "C"
{
#endif
    const char *getServiceName();
    void startService(void);
    void stopService();

#ifdef UNITTEST
    int32_t utmain(int32_t argc, char* argv[]);
#endif


#ifdef __cplusplus
}
#endif

#include "svs_adapter_scc_connector.h"
#include "svs_adapter_service_task.h"

class CMduService
{
public:
    virtual ~CMduService();

    static CMduService *instance()
    {
        static CMduService mduService;
        return &mduService;
    }

    /// 打开MDU服务
    int32_t openService();

    /// 停止MDU服务
    int32_t closeService();

private:
    CMduService();

private:
    bool                m_bRunFlag;        /// 服务运行标志
};

#endif /* __MDUSERVICE_H__ */

