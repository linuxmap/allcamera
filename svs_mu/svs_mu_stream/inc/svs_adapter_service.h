/**
* @file    MduService.h
* @brief   Mdu���������ڣ����������������
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

    /// ��MDU����
    int32_t openService();

    /// ֹͣMDU����
    int32_t closeService();

private:
    CMduService();

private:
    bool                m_bRunFlag;        /// �������б�־
};

#endif /* __MDUSERVICE_H__ */

