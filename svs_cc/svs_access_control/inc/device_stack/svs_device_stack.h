#ifndef _SVS_DEVICE_STACK_H_
#define _SVS_DEVICE_STACK_H_

#include "vms/vms.h"
#include "svs_acm_async.h"

class IDeviceStack : public SVS_ACM::IAsync
{
public:
    IDeviceStack(){};
    virtual ~IDeviceStack(){};

    virtual int32_t initialize() = 0;
    virtual int32_t release() = 0;

    virtual int32_t start() = 0;
    virtual int32_t stop() = 0;

    virtual int32_t timeout() = 0;
};

class IDeviceStackMgr : public SVS_ACM::IAsync
{
public:
    static IDeviceStackMgr& instance()
    {
        static IDeviceStackMgr obj;
        return obj;
    }

    int32_t initialize();
    int32_t release();

    int32_t start();
    int32_t stop();

    virtual int32_t asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack = NULL, void* pUserData = NULL);

    void handle_device_stack_timeout(int32_t eventId, int32_t timerId, int32_t nDummy);

private:
    IDeviceStackMgr();
    virtual ~IDeviceStackMgr();
private:
    static void device_stack_timer_callback(void *pArg, int32_t eventId, int32_t timerId, int32_t nDummy);

private:
    int32_t       m_nTimerID;
    IDeviceStack* m_pGB28181DeviceStack;
    IDeviceStack* m_pEhomeDeviceStack;
};

#endif
