#include "svs_device_stack.h"
#include "svs_device_stack_gb28181.h"
#include "svs_device_stack_ehome.h"
#include "svs_log_msg.h"
#include "svs_utility.h"
#include "svs_timer.h"

IDeviceStackMgr::IDeviceStackMgr()
    : m_pGB28181DeviceStack(NULL)
    , m_pEhomeDeviceStack(NULL)
    , m_nTimerID(-1)
{
    SVS_TRACE();
}

IDeviceStackMgr::~IDeviceStackMgr()
{
    SVS_TRACE();
}

int32_t IDeviceStackMgr::initialize()
{
    SVS_TRACE();

    CDeviceStackGB28181* pGB28181Stack = NULL;
    CDeviceStackEhome* pEhomeStack = NULL;


    pGB28181Stack = SVS_NEW(pGB28181Stack);
    if(NULL == pGB28181Stack)
    {
        SVS_LOG((SVS_LM_ERROR, "GB28181 Malloc memory failed."));
        return SVS_ERROR_FAIL;
    }
    m_pGB28181DeviceStack = pGB28181Stack;

    if (0 != m_pGB28181DeviceStack->initialize())
    {
        SVS_LOG((SVS_LM_ERROR, "Initialize GB28181 device stack failed."));
        return SVS_ERROR_FAIL;
    }

    pEhomeStack = SVS_NEW(pEhomeStack);
    if(NULL == pEhomeStack)
    {
        SVS_LOG((SVS_LM_ERROR, "Ehome Malloc memory failed."));
        return SVS_ERROR_FAIL;
    }
    m_pEhomeDeviceStack = pEhomeStack;

    if (0 != m_pEhomeDeviceStack->initialize())
    {
        SVS_LOG((SVS_LM_ERROR, "Initialize GB28181 device stack failed."));
        return SVS_ERROR_FAIL;
    }

    const ACE_Time_Value tvInterval(TIMER_INTERVAL_DEVICE_STACK);
    int32_t nTimerID = CAC_Timer_Manager::instance()->alloc_timer(this, device_stack_timer_callback,
                       TIMER_EVENT_ID_DEVICE_STACK, tvInterval, tvInterval);
    if (nTimerID < 0)
    {
        SVS_LOG((SVS_LM_ERROR, "Create device stack timer failed, interval is %d.", TIMER_INTERVAL_DEVICE_STACK));
        return SVS_ERROR_FAIL;
    }

    m_nTimerID = nTimerID;

    return SVS_ERROR_OK;
}

int32_t IDeviceStackMgr::release()
{
    SVS_TRACE();
    if(NULL != m_pGB28181DeviceStack)
    {
        m_pGB28181DeviceStack->release();
        SVS_DELETE(m_pGB28181DeviceStack);
        m_pGB28181DeviceStack = NULL;
    }



    if (NULL != m_pEhomeDeviceStack)
    {
        m_pEhomeDeviceStack->release();
        SVS_DELETE(m_pEhomeDeviceStack);
        m_pEhomeDeviceStack = NULL;
    }

    return SVS_ERROR_OK;
}

int32_t IDeviceStackMgr::start()
{
    SVS_TRACE();
    if(NULL != m_pGB28181DeviceStack)
    {
        m_pGB28181DeviceStack->start();
    }



    if (NULL != m_pEhomeDeviceStack)
    {
        m_pEhomeDeviceStack->start();
    }
    return SVS_ERROR_OK;
}

int32_t IDeviceStackMgr::stop()
{
    SVS_TRACE();
    if(NULL != m_pGB28181DeviceStack)
    {
        m_pGB28181DeviceStack->stop();
    }



    if (NULL != m_pEhomeDeviceStack)
    {
        m_pEhomeDeviceStack->stop();
    }
    return SVS_ERROR_OK;
}

void IDeviceStackMgr::device_stack_timer_callback(void *pArg, int32_t eventId, int32_t timerId, int32_t nDummy)
{
    IDeviceStackMgr* pMgr = (IDeviceStackMgr*)pArg;
    pMgr->handle_device_stack_timeout( eventId, timerId, nDummy);
}

void IDeviceStackMgr::handle_device_stack_timeout(int32_t eventId, int32_t timerId, int32_t nDummy)
{
    SVS_TRACE();
    if(NULL != m_pGB28181DeviceStack)
    {
        m_pGB28181DeviceStack->timeout();
    }

    if (NULL != m_pEhomeDeviceStack)
    {
        m_pEhomeDeviceStack->timeout();
    }
}


int32_t IDeviceStackMgr::asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_ACM::REQUEST_TO_DEV& rToDev = (SVS_ACM::REQUEST_TO_DEV&)rRequest;
    if(SVS_DEV_TYPE_GB28181 == rToDev.eDevType)
    {
        return m_pGB28181DeviceStack->asyncRequest(rRequest, pCallBack, pUserData);
    }
    else if(SVS_DEV_TYPE_EHOME == rToDev.eDevType)
    {
        return m_pEhomeDeviceStack->asyncRequest(rRequest, pCallBack, pUserData);
    }
}
