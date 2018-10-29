#include "svs_access_control_manager.h"
#include "svs_log_msg.h"
#include "vms/vms.h"
#include "svs_acm_response.h"
#include "svs_acm_request.h"
#include "svs_ac_common.h"
#include "svs_daemon_thread.h"
#include "svs_control_stack.h"
#include "svs_server_stack.h"
#include "svs_access_control.h"
#include "svs_timer.h"
#include "svs_device_stack.h"



IAccessControlManager::IAccessControlManager()
{
    m_nTimerID = -1;
    SVS_TRACE();
}

IAccessControlManager::~IAccessControlManager()
{
    SVS_TRACE();
}

int32_t IAccessControlManager::initialize()
{
    SVS_TRACE();

    if(0 != m_queue.open(SVS_MSG_QUEUE_WATER, SVS_MSG_QUEUE_WATER))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Open message queue failed."));
        return SVS_ERROR_FAIL;
    }

    /* 启动定时通知定时器 */
    uint32_t nReportInterval = CAccessControlSvr::instance().get_Dev_Report_Interval();
    const ACE_Time_Value tvInterval(nReportInterval);
    int32_t nTimerID = CAC_Timer_Manager::instance()->alloc_timer(this, device_report_timer_callback,
                       TIMER_EVENT_ID_DEVICE_REPORT, tvInterval, tvInterval);
    if (nTimerID < 0)
    {
        SVS_LOG((SVS_LM_ERROR, "Create device report timer failed, interval is %d.", TIMER_INTERVAL_DEVICE_STACK));
        return SVS_ERROR_FAIL;
    }

    m_nTimerID = nTimerID;

    size_t *pArrayStackSize = NULL;
    try
    {
        pArrayStackSize = new size_t[SVS_ACM_THREAD_MAX];
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Create stack size for thread error."));
        return SVS_ERROR_FAIL;
    }

    for ( int32_t i = 0; i < SVS_ACM_THREAD_MAX; i++)
    {
        pArrayStackSize[i] = SVS_THREAD_STATCK_MAX;
    }

    m_bRunning = true;
    int32_t nResult = activate(THR_NEW_LWP,
                       SVS_ACM_THREAD_MAX,
                       0,
                       ACE_DEFAULT_THREAD_PRIORITY,
                       -1,
                       0,
                       0,
                       0,
                       pArrayStackSize,
                       0);

    delete []pArrayStackSize;
    pArrayStackSize = NULL;

    return nResult;
}

int32_t IAccessControlManager::release()
{
    SVS_TRACE();
    return SVS_ERROR_OK;
}

int32_t IAccessControlManager::start()
{
    SVS_TRACE();
    return SVS_ERROR_OK;
}

int32_t IAccessControlManager::stop()
{
    SVS_TRACE();
    return SVS_ERROR_OK;
}

int32_t IAccessControlManager::svc()
{
    SVS_TRACE();

    ACE_Message_Block *mb = NULL;
    ACE_Time_Value tvDelay(REPORT_THREAD_STATUS_INTERVAL);

    while (m_bRunning)
    {
        ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;
        if (SVS_ERROR_FAIL == m_queue.dequeue_head(mb, &tvTimeout))
        {
            continue;
        }
    }

    return SVS_ERROR_OK;
}

int32_t IAccessControlManager::notifyDeviceInfo(SVS_ACM::REQUEST_NOTIFY_DEV_INFO& stNodifyInfo)
{
    SVS_TRACE();

    const SVS_ACM::DEVICE_INFO& stDeviceInfo = stNodifyInfo.stDeviceInfo;
    if (SVS_DEV_STATUS_ONLINE == stDeviceInfo.eDeviceStatus)
    {   //上线
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
        m_mapDeviceInfo[stDeviceInfo.szDeviceID] = stDeviceInfo;
        SVS_ACM::DEVICE_INFO& rDeviceInfo = m_mapDeviceInfo[stDeviceInfo.szDeviceID];
        for (size_t i = 0; i < rDeviceInfo.vecLensInfo.size(); i++)
        {
            SVS_ACM::LENS_INFO& rLensInfo = rDeviceInfo.vecLensInfo[i];
            m_mapLensInfo[rLensInfo.szLensID] = &rLensInfo;
        }

        //仅开发测试用
        /*
        SVS_ACM::REQUEST_DEV_CTRL rRequest;
        rRequest.eCtrlType = SVS_ACM::DEV_CTRL_TYPE_LEFT_UP;
        rRequest.nCtrlParam1 = 1;
        strncpy(rRequest.szLensID, "34020000001320000001", sizeof(rRequest.szLensID) - 1);
        int32_t nResult = asyncRequest(rRequest, NULL, NULL);
        if (0 != nResult)
        {
            SVS_LOG((SVS_LM_ERROR, "asyncRequest failed."));
            return SVS_ERROR_FAIL;
        }
        */
        /*
        SVS_ACM::REQUEST_SEND_KEYFRAME2DEV rRequest;
        strncpy(rRequest.szLensID, "34020000001320000003", sizeof(rRequest.szLensID) - 1);
        int32_t nResult = asyncRequest(rRequest, NULL, NULL);
        if (0 != nResult)
        {
            SVS_LOG((SVS_LM_ERROR, "asyncRequest failed."));
            return SVS_ERROR_FAIL;
        }
        */
    }
    else
    {   //下线
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
        MAP_DEV_INFO::iterator iter = m_mapDeviceInfo.find(stDeviceInfo.szDeviceID);
        if (m_mapDeviceInfo.end() != iter)
        {
            SVS_ACM::DEVICE_INFO& rDeviceInfo = iter->second;
            for (size_t i = 0; i < rDeviceInfo.vecLensInfo.size(); i++)
            {
                SVS_ACM::LENS_INFO& rLensInfo = rDeviceInfo.vecLensInfo[i];
                m_mapLensInfo.erase(rLensInfo.szLensID);
            }
            m_mapDeviceInfo.erase(stDeviceInfo.szDeviceID);
        }
    }

    int32_t nResult = CAccessControlStack::instance().asyncRequest(stNodifyInfo);
    return nResult;
}

int32_t IAccessControlManager::getDeviceInfo(std::string& strDevID, SVS_ACM::DEVICE_INFO& stDeviceInfo)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
    if (m_mapDeviceInfo.end() == m_mapDeviceInfo.find(strDevID))
    {
        return SVS_ERROR_FAIL;
    }

    stDeviceInfo = m_mapDeviceInfo[strDevID];
    return SVS_ERROR_OK;
}

int32_t IAccessControlManager::getLensInfo(std::string& strLensID, SVS_ACM::LENS_INFO& stLensInfo)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
    if (m_mapLensInfo.end() == m_mapLensInfo.find(strLensID))
    {
        return SVS_ERROR_FAIL;
    }

    stLensInfo = *m_mapLensInfo[strLensID];
    return SVS_ERROR_OK;
}

void IAccessControlManager::device_report_timer_callback(void *pArg, int32_t eventId, int32_t timerId, int32_t nDummy)
{
    IAccessControlManager* pManager = (IAccessControlManager*)pArg;
    pManager->report_device_info();
}

void IAccessControlManager::report_device_info()
{
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex);

    if(0 == m_mapDeviceInfo.size())
    {
        return;
    }

    SVS_ACM::REQUEST_NOTIFY_ALL_DEV notify;

    MAP_DEV_INFO::iterator iter = m_mapDeviceInfo.begin();

    for(;iter != m_mapDeviceInfo.end();++iter)
    {
        SVS_ACM::DEVICE_INFO& stDeviceInfo = iter->second;
        notify.ListDeviceInfo.push_back(stDeviceInfo);
    }

    (void)CAccessControlStack::instance().asyncRequest(notify);
}

int32_t IAccessControlManager::notifyResponse(const SVS_ACM::RESPONSE& rResponse)
{
    SVS_TRACE();

    return SVS_ERROR_OK;
}

int32_t IAccessControlManager::handleToDevReq(SVS_ACM::REQUEST_TO_DEV& rToDev, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_TRACE();

    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
        MAP_LENS_INFO::iterator iter = m_mapLensInfo.find(rToDev.szLensID);
        if (m_mapLensInfo.end() == iter)
        {
            SVS_LOG((SVS_LM_ERROR, "Handle to device request failed, lens '%s' not found, request type is %d.",
                rToDev.szLensID, rToDev.getType()));
            return SVS_ERROR_FAIL;
        }
        SVS_ACM::LENS_INFO* pLensInfo = iter->second;
        strncpy(rToDev.szDeviceID, pLensInfo->szDeviceID, sizeof(rToDev.szDeviceID) - 1);
        rToDev.eDevType = pLensInfo->eLensType;
    }
    return IDeviceStackMgr::instance().asyncRequest(rToDev, pCallBack, pUserData);
}

int32_t IAccessControlManager::asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_TRACE();

    int32_t nResult = SVS_ERROR_FAIL;
    switch (rRequest.getType())
    {
    case SVS_ACM::REQ_TYPE_NOTIFY_DEV_INFO:
        {
            SVS_ACM::REQUEST_NOTIFY_DEV_INFO& rNotify = (SVS_ACM::REQUEST_NOTIFY_DEV_INFO&)rRequest;
            nResult = notifyDeviceInfo(rNotify);
            break;
        }
    case SVS_ACM::REQ_TYPE_SEND_INVITE2DEV:
    case SVS_ACM::REQ_TYPE_SEND_ACK2DEV:
    case SVS_ACM::REQ_TYPE_SEND_BYE2DEV:
    case SVS_ACM::REQ_TYPE_DEV_CTRL:
    case SVS_ACM::REQ_TYPE_SEND_KEYFRAME2DEV:
        {
            SVS_ACM::REQUEST_TO_DEV& rToDev = (SVS_ACM::REQUEST_TO_DEV&)rRequest;
            nResult = handleToDevReq(rToDev, pCallBack, pUserData);
            break;
        }
    case SVS_ACM::REQ_TYPE_NOTIFY_DEV_ALARM:
        {
            nResult = CAccessControlStack::instance().asyncRequest(rRequest, pCallBack, pUserData);
            break;
        }
    case SVS_ACM::REQ_TYPE_MAX:
    default:
        SVS_LOG((SVS_LM_ERROR, "Async request type %d is invalid.", rRequest.getType()));
        break;
    }

    return nResult;
}

void IAccessControlManager::show_deviceInfo(const std::string& strDevID,std::string& Info)
{
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex);
    std::string strTemp;

    if(0 < strDevID.length())
    {
        MAP_DEV_INFO::iterator iter = m_mapDeviceInfo.find(strDevID);
        if(iter == m_mapDeviceInfo.end())
        {
            Info = "not find the device info";
            return;
        }

        SVS_ACM::DEVICE_INFO& stDeviceInfo = m_mapDeviceInfo[strDevID];

        strTemp = (const char*)&stDeviceInfo.szDeviceID[0];
        Info += "ID:" + strTemp + "\r\n";
        strTemp = (const char*)&stDeviceInfo.szHost[0];
        Info += "address:" + strTemp + ":" ;
        strTemp = (const char*)&stDeviceInfo.szPort[0];
        Info += strTemp + "\r\n";
        Info += "lens list:\r\n";
        uint32_t ulSize = stDeviceInfo.vecLensInfo.size();
        for(uint32_t i = 0;i < ulSize;i++)
        {
            strTemp = (const char*)&stDeviceInfo.vecLensInfo[i].szLensID[0];
            Info += "\t["+ strTemp + "]\r\n";
        }

        return;
    }

    MAP_DEV_INFO::iterator iter = m_mapDeviceInfo.begin();
    Info += "device list:\r\n";

    for(;iter != m_mapDeviceInfo.end();++iter)
    {
       SVS_ACM::DEVICE_INFO& stDeviceInfo = iter->second;

       strTemp = (const char*)&stDeviceInfo.szDeviceID[0];
       Info += "ID:[" + strTemp + "] [";
       strTemp = (const char*)&stDeviceInfo.szHost[0];
       Info += strTemp + ":";
       strTemp = (const char*)&stDeviceInfo.szPort[0];
       Info += strTemp + "] [";
       strTemp = szDevType[stDeviceInfo.eDeviceType];
       Info += strTemp + "]\r\n";
    }

    return;
}
void IAccessControlManager::show_LensInfo(const std::string& strLensID,std::string& Info)
{

    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex);
    std::string strTemp;
    if(0 < strLensID.length())
    {
        if (m_mapLensInfo.end() == m_mapLensInfo.find(strLensID))
        {
            Info = "not find the lens info";
            return ;
        }

        SVS_ACM::LENS_INFO& stLensInfo = *m_mapLensInfo[strLensID];

        strTemp = (const char*)&stLensInfo.szLensID[0];
        Info += "ID:" + strTemp + "\r\n";
        strTemp = szDevType[stLensInfo.eLensType];
        Info += "Type:" + strTemp + "\r\n";
        strTemp =  szDevStatus[stLensInfo.eLensStatus];
        Info += "status:" +strTemp + "\r\n";
        strTemp =  (const char*)&stLensInfo.szDeviceID[0];
        Info += "DevID:" + strTemp + "\r\n";
        strTemp =  (const char*)&stLensInfo.szLensName[0];
        Info += "Name:" + strTemp + "\r\n";
        strTemp =  (const char*)&stLensInfo.szManufacturer[0];
        Info += "Manufacturer:" + strTemp + "\r\n";


        return;
    }

    MAP_LENS_INFO::iterator iter = m_mapLensInfo.begin();
    Info += "lens list:\r\n";

    for(;iter != m_mapLensInfo.end();++iter)
    {
       SVS_ACM::LENS_INFO& stLensInfo  = *iter->second;

       strTemp =  (const char*)&stLensInfo.szLensID[0];
       Info += "ID:[" + strTemp + "] ";
       strTemp =  szDevStatus[stLensInfo.eLensStatus];
       Info += "status:[" + strTemp + "]\r\n";
    }
    return;
}


