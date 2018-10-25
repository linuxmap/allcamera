/**
* @file    StreamService.h
* @brief   Stream���������ڣ����������������
*
* Copyright (c) 2010 AllCam Tech.Co.,Ltd
*
* @date      2010-12-11
*/

#ifndef __STREAMSERVICE_H__
#define __STREAMSERVICE_H__
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

class CStreamService
{
public:
    virtual ~CStreamService();

    static CStreamService *instance()
    {
        static CStreamService streamService;
        return &streamService;
    }

    /// ��STREAM����
    int32_t openService();

    /// ֹͣSTREAM����
    int32_t closeService();

private:
    CStreamService();

private:
    bool                m_bRunFlag;        /// �������б�־
};

#endif /* __STREAMSERVICE_H__ */

