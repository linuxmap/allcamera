/*
 * CServerManager.cpp
 *
 *  Created on: Sep 16, 2014
 *      Author: timehunter
 */
#include <sstream>
#include "svs_server_manager.h"
#include "svs_log_msg.h"
#include "svs_ac_common.h"
#include "svs_daemon_thread.h"
#include "svs_access_control.h"
#include "svs_business_manager.h"
#include "svs_utility.h"
#include "vms/vms.h"

#include "svs_server_stack.h"
#include "controlstack/svs_control_stack.h"

extern ACE_Recursive_Thread_Mutex g_tmp_send_msg_mutex;


void CSVSLoger::writeLog(uint8_t ucType, uint8_t ucLevel, const char *pszLog,
                  uint32_t unLogLength, char* pFile, int32_t lLine)
{
    SVS_LOG((SVS_LM_INFO,"%s",pszLog));
}


CServer::CServer()
{
    memset(m_szServerID,0,SERVER_ID_LEN+1);
    memset(m_szServerIP,0,SVS_IP_LEN+1);
    m_uiServerType = SERVER_TYPE_MAX;
    m_uiServerIndex = 0;
    m_uiRefCount    = 0;

    m_usRtspServerPort    = SCHEDULE_RTSP_PORT;
    m_usHLSServerPort     = SCHEDULE_HLS_PORT;
    m_usRtmpServerPort   = SCHEDULE_HLS_PORT;  // TODO: change default value.

    m_uiLastRunTime = 0;
    m_uiResetFlags = 0;
    m_uiServiceNetType = 0;
}
CServer:: ~CServer()
{
}

char* CServer::getServerID()
{
    return m_szServerID;
}
uint32_t CServer::getServerIndex()
{
    return m_uiServerIndex;
}
uint32_t CServer::getServerType()
{
    return m_uiServerType;
}

char* CServer::getServerName()
{
    return m_szServerName;
}

void CServer::setServerIP(const char* pszServerIP)
{
    strncpy(m_szServerIP, pszServerIP, SVS_IP_LEN);
}

char* CServer::getServerIP()
{
    return m_szServerIP;
}


std::string CServer::getMediaServerIP()
{
    return m_strMeidaSvrIP;
}

uint16_t CServer::getMediaRtspPort()
{
    return m_usRtspServerPort;
}

uint16_t CServer::getMediaHlsPort()
{
    return m_usHLSServerPort;
}

uint16_t CServer::getMediaRtmpPort()
{
    return m_usRtmpServerPort;
}

void CServer::setLastRunTime(uint32_t uiLastRunTime)
{
    m_uiLastRunTime = uiLastRunTime;
}
uint32_t CServer::getLastRunTime()
{
    return m_uiLastRunTime;
}

void CServer::setNetType(uint32_t uiNetType)
{
    m_uiServiceNetType = uiNetType;
}
uint32_t CServer::getNetType()
{
    return m_uiServiceNetType;
}

void CServer::setResetFlags(uint32_t uiResetFlags)
{
    m_uiResetFlags = uiResetFlags;
}
uint32_t CServer::getResetFlags()
{
    return m_uiResetFlags;
}

void  CServer::setServerID(const char* pszServerID)
{
    memset(m_szServerID,0,SERVER_ID_LEN+1);
    memcpy(m_szServerID,pszServerID,SERVER_ID_LEN);
}
void  CServer::setServerIndex(uint32_t ulIndex)
{
    m_uiServerIndex = ulIndex;
}

void  CServer::setServerName(const char* pszSvrName)
{
    memset(m_szServerName,0,SERVER_NAME_LEN+1);
    memcpy(m_szServerName,pszSvrName,SERVER_NAME_LEN);
}

void CServer::setMediaSvrIP(const char* pszServerIP)
{
    char    szMediaSvrIP[SVS_IP_LEN+1] = {0};
    memcpy(szMediaSvrIP,pszServerIP,SVS_IP_LEN);

    m_strMeidaSvrIP = szMediaSvrIP;
}

void CServer::SetMediaRtspPort(uint16_t usPort)
{
    m_usRtspServerPort = usPort;
}

void CServer::SetMediaHlsPort(uint16_t usPort)
{
    m_usHLSServerPort = usPort;
}

void CServer::SetMediaRtmpPort(uint16_t usPort)
{
    m_usRtmpServerPort = usPort;
}


int32_t  CServer::addReference()
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, guard, m_mutex, 0);
    m_uiRefCount++;
    return m_uiRefCount;

}
int32_t CServer::decReference()
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, guard, m_mutex, 0);
    if(0 < m_uiRefCount)
    {
        m_uiRefCount--;
    }
    return m_uiRefCount;
}
int32_t CServer::reference()
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, guard, m_mutex, 0);
    return m_uiRefCount;
}

// ---------------------------------------------------------

CStreamServer::CStreamServer()
{
    m_uiServerType = SERVER_TYPE_MU_STREAM;
    m_uibindSessionCount  = 0;
    m_uiPortUsage = SVS_USAGE_LOAD_RATE;
    m_uiTotalCapacity = 200;

    m_uiBusinessTotalNum = 0;

    // load info
    m_uiTransmitNumber = 0;
    m_uiTotalMemorySize = 0;
    m_uiUsedMemorySize = 0;
    m_uiUsedCpuLoad = 0;
    m_uiIowaitCpuLoad = 0;
    m_uiCacheTotalNum = 0;
    m_uiCacheUsedNum = 0;
    m_uiNetworkCardNum = 0;

    m_listNetworkCardInfo.clear();
}

CStreamServer::~CStreamServer()
{

}


uint32_t CStreamServer::findRTSessionbyDevID(const char* pszDevID)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, false);
    DevSession* pDevSession = NULL;
    RTDevMAP::iterator iter =  m_DevBindMap.find(pszDevID);
    if(iter == m_DevBindMap.end())
    {
        return 0;
    }
    pDevSession = iter->second;

    return pDevSession->m_uiSessionCount;
}

uint32_t CStreamServer::getAllSessionCount()
{
    return m_uibindSessionCount;
}

int32_t CStreamServer::allocStreamforRealtime(const char* pszDevID)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, -1);
    DevSession* pDevSession = NULL;
    RTDevMAP::iterator iter =  m_DevBindMap.find(pszDevID);
    if(iter == m_DevBindMap.end())
    {
        SVS_NEW(pDevSession);
        if(NULL == pDevSession)
        {
            return -1;
        }

        ACE_OS::memset(pDevSession,0,sizeof(DevSession));
        ACE_OS::strncpy(pDevSession->m_szDevID,
                        pszDevID,ACE_OS::strlen(pszDevID));
        m_DevBindMap.insert(RTDevMAP::value_type(pszDevID,pDevSession));
    }
    else
    {
        pDevSession = iter->second;
    }

    pDevSession->m_uiSessionCount++;
    SVS_LOG((
        SVS_LM_DEBUG,
        "Alloc mu for realtime. "
        "mu=%s, DevID=%s,SessionCnt=%d,totalCnt=%d.",
        m_szServerID, pszDevID,pDevSession->m_uiSessionCount,m_uibindSessionCount));
    return 0;
}

int32_t CStreamServer::freeStreamforRealtime(const char* pszDevID)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, -1);
    DevSession* pDevSession = NULL;
    RTDevMAP::iterator iter =  m_DevBindMap.find(pszDevID);
    if(iter == m_DevBindMap.end())
    {
        return -1;
    }

    pDevSession = iter->second;

    pDevSession->m_uiSessionCount--;

    SVS_LOG((
        SVS_LM_DEBUG,
        "Free mu for realtime. "
        "mu=%s, DevID=%s,SessionCnt=%d,totalCnt=%d.",
        m_szServerID, pszDevID,pDevSession->m_uiSessionCount,m_uibindSessionCount));
    if(0 == pDevSession->m_uiSessionCount)
    {
        m_DevBindMap.erase(iter);
        SVS_DELETE(pDevSession);
    }
    return 0;
}

int32_t CStreamServer::allocStreamforRecord(const char* pszDevID)
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, -1);

    SVS_LOG((
        SVS_LM_DEBUG,
        "Alloc mu for record. "
        "mu=%s, DevID=%s,totalCnt=%d.",
        m_szServerID, pszDevID,m_uibindSessionCount));
    return 0;
}

int32_t  CStreamServer::freeStreamforRecorde(const char* pszDevID)
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, -1);

    SVS_LOG((
        SVS_LM_DEBUG,
        "Free mu for record. "
        "mu=%s, DevID=%s,totalCnt=%d.",
        m_szServerID, pszDevID,m_uibindSessionCount));
    return 0;
}

void CStreamServer::updateLoadCount(uint32_t ulCount)
{
    m_uibindSessionCount = ulCount;
}

void CStreamServer::display_streamsvr_state()
{
    SVS_TRACE();

    SVS_LOG((
        SVS_LM_DEBUG,
        "Display mu server state. "
        "mu=%s, SessionCnt:%d.",
        m_szServerID, m_uibindSessionCount));
}

int32_t CStreamServer::handle_business_report_req(const char* pszMsg)
{
    SVS_TRACE();

    if(NULL == pszMsg)
    {
        return -1;
    }

    SVS_LOG((SVS_LM_INFO, "deal mu business report request, SvrID=%s.", m_szServerID));

    SVS_MSG_MU_BUSINESS_REPORT_REQ* t_pReq = (SVS_MSG_MU_BUSINESS_REPORT_REQ*)(void*)pszMsg;
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu business report request failed, "
                         "Invalid parameter. pReq is null, SvrID=%s.",
                         m_szServerID));
        return -1;
    }

    // int32_t                         iRespCode = -1;

    SVS_LOG((SVS_LM_INFO,
                     "receive mu business report request, SvrID=%s, SvrIP=%s, BusinessTotalNum=%d.",
                     m_szServerID, m_szServerIP, t_pReq->BusinessTotalNum));


    // report to the http server.
    SVS_ACM::REQUEST_HTTP_REPORT oHttpRequest;
    oHttpRequest.nRequestID = 0;
    strncpy(oHttpRequest.szServerID, m_szServerID, SERVER_ID_LEN);
    oHttpRequest.BusinessTotalNum = t_pReq->BusinessTotalNum;

    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    for (int i = 0; i < t_pReq->BusinessTotalNum; i ++)
    {
        ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->BusinessInfo[i].DeviceID, DEVICE_ID_LEN);
        SVS_LOG((SVS_LM_DEBUG,
                         "receive mu business report request, SvrID=%s, DeviceID=%s, ServiceType=%u, "
                         "szUrl=%s, FluxSize=%llu, "
                         "starttime=%u, endtime=%u.",
                         m_szServerID, t_szDevID, t_pReq->BusinessInfo[i].ServiceType,
                         t_pReq->BusinessInfo[i].szUrl, t_pReq->BusinessInfo[i].FluxSize,
                         t_pReq->BusinessInfo[i].OperateTimeSpan.StartTime, t_pReq->BusinessInfo[i].OperateTimeSpan.EndTime));

        oHttpRequest.BusinessInfoList.push_back(t_pReq->BusinessInfo[i]);
    }

    m_uiBusinessTotalNum = t_pReq->BusinessTotalNum;
    m_BusinessInfoList = oHttpRequest.BusinessInfoList;

    if ( 0 != CAccessControlStack::instance().asyncRequest(oHttpRequest, IServerStack::asyncResponse, NULL))
    {
        SVS_LOG((SVS_LM_WARNING,
                         "receive mu business report request, report to http server failed, "
                         "SvrID=%s, SvrIP=%s, BusinessTotalNum=%d.",
                         m_szServerID, m_szServerIP, t_pReq->BusinessTotalNum));
        return -1;

    }

    SVS_LOG((SVS_LM_INFO,
                     "receive mu business report request, report to http server success, "
                     "SvrID=%s, SvrIP=%s, BusinessTotalNum=%d.",
                     m_szServerID, m_szServerIP, t_pReq->BusinessTotalNum));

    return 0;
}


bool CStreamServer::is_load_light()
{
    SVS_LOG((SVS_LM_DEBUG,
                     "is load light judgement, SvrID=%s, SvrIP=%s, TransmitNumber=%u, "
                     "TotalMemorySize=%u, UsedMemorySize=%u, UsedCpuLoad=%u, IowaitCpuLoad=%u, "
                     "CacheTotalNum=%u, CacheUsedNum=%u, NetworkCardNum=%u.",
                     m_szServerID, m_szServerIP, m_uiTransmitNumber,
                     m_uiTotalMemorySize, m_uiUsedMemorySize, m_uiUsedCpuLoad, m_uiIowaitCpuLoad,
                     m_uiCacheTotalNum, m_uiCacheUsedNum, m_uiNetworkCardNum));

    if ((0 == m_uiTotalMemorySize)
        || (0 == m_uiCacheTotalNum))
    {
        return false;
    }

    if (LIGHT_LOAD_PERCENT < (m_uiUsedMemorySize*100/m_uiTotalMemorySize))
    {
        return false;
    }

    if (LIGHT_LOAD_PERCENT < (m_uiUsedCpuLoad))
    {
        return false;
    }

    if (LIGHT_LOAD_PERCENT < (m_uiIowaitCpuLoad))
    {
        return false;
    }

    if (LIGHT_LOAD_PERCENT < (m_uiCacheUsedNum*100/m_uiCacheTotalNum))
    {
        return false;
    }

    return true;
}

bool CStreamServer::is_load_heavy()
{
    SVS_LOG((SVS_LM_DEBUG,
                     "is load heavy judgement, SvrID=%s, SvrIP=%s, TransmitNumber=%u, "
                     "TotalMemorySize=%u, UsedMemorySize=%u, UsedCpuLoad=%u, IowaitCpuLoad=%u, "
                     "CacheTotalNum=%u, CacheUsedNum=%u, NetworkCardNum=%u.",
                     m_szServerID, m_szServerIP, m_uiTransmitNumber,
                     m_uiTotalMemorySize, m_uiUsedMemorySize, m_uiUsedCpuLoad, m_uiIowaitCpuLoad,
                     m_uiCacheTotalNum, m_uiCacheUsedNum, m_uiNetworkCardNum));

    if ((0 == m_uiTotalMemorySize)
        || (0 == m_uiCacheTotalNum))
    {
        return true;
    }

    if (HEAVY_LOAD_PERCENT < (m_uiUsedMemorySize*100/m_uiTotalMemorySize))
    {
        return true;
    }

    if (HEAVY_LOAD_PERCENT < (m_uiUsedCpuLoad))
    {
        return true;
    }

    if (HEAVY_LOAD_PERCENT < (m_uiIowaitCpuLoad))
    {
        return true;
    }

    if (HEAVY_LOAD_PERCENT < (m_uiCacheUsedNum*100/m_uiCacheTotalNum))
    {
        return true;
    }

    return false;
}


uint32_t CStreamServer::load_type()
{
    if (is_load_light())
    {
        return SVR_LOAD_TYPE_LIGHT;
    }
    else if (is_load_heavy())
    {
        return SVR_LOAD_TYPE_HEAVY;
    }
    else
    {
        return SVR_LOAD_TYPE_MODERATE;
    }
}

int32_t CStreamServer::get_health_value()
{
    SVS_LOG((SVS_LM_DEBUG,
                     "get health value, SvrID=%s, SvrIP=%s, TransmitNumber=%u, "
                     "TotalMemorySize=%u, UsedMemorySize=%u, UsedCpuLoad=%u, IowaitCpuLoad=%u, "
                     "CacheTotalNum=%u, CacheUsedNum=%u, NetworkCardNum=%u.",
                     m_szServerID, m_szServerIP, m_uiTransmitNumber,
                     m_uiTotalMemorySize, m_uiUsedMemorySize, m_uiUsedCpuLoad, m_uiIowaitCpuLoad,
                     m_uiCacheTotalNum, m_uiCacheUsedNum, m_uiNetworkCardNum));

    if ((0 == m_uiTotalMemorySize)
        || (0 == m_uiCacheTotalNum))
    {
        return 0;
    }

    uint32_t    uiLoad = 0;
    uint32_t    uiHealth = 0;

    uiLoad = ((double)m_uiUsedMemorySize*100/m_uiTotalMemorySize)*MEM_PERCENT
                    +m_uiUsedCpuLoad*CPU_PERCENT
                    +m_uiIowaitCpuLoad*IO_PERCENT
                    +((double)m_uiCacheUsedNum*100/m_uiCacheTotalNum)*CACHE_PERCENT;

    uiHealth = NO_LOAD_HEALTH - uiLoad;

    SVS_LOG((SVS_LM_DEBUG,
                     "get health value, SvrID=%s, HealthValue=%d.",
                     m_szServerID, uiHealth));
    return uiHealth;
}

int32_t CStreamServer::handle_load_info_report_req(const char* pszMsg)
{
    SVS_TRACE();

    if(NULL == pszMsg)
    {
        return -1;
    }

    SVS_LOG((SVS_LM_INFO, "deal mu load info report request, SvrID=%s.", m_szServerID));

    SVS_MSG_STREAM_LOAD_INFO_REQ* t_pReq = (SVS_MSG_STREAM_LOAD_INFO_REQ*)(void*)pszMsg;
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu load info report request failed, "
                         "Invalid parameter. pReq is null, SvrID=%s.",
                         m_szServerID));
        return -1;
    }

    // int32_t                         iRespCode = -1;

    SVS_LOG((SVS_LM_INFO,
                     "receive mu load info report request, SvrID=%s, SvrIP=%s, TransmitNumber=%u, "
                     "TotalMemorySize=%u, UsedMemorySize=%u, UsedCpuLoad=%u, IowaitCpuLoad=%u, "
                     "CacheTotalNum=%u, CacheUsedNum=%u, NetworkCardNum=%u.",
                     m_szServerID, m_szServerIP, t_pReq->TransmitNumber,
                     t_pReq->TotalMemorySize, t_pReq->UsedMemorySize, t_pReq->UsedCpuLoad, t_pReq->IowaitCpuLoad,
                     t_pReq->CacheTotalNum, t_pReq->CacheUsedNum, t_pReq->NetworkCardNum));

    // save load info for loadbalance
    m_uiTransmitNumber = t_pReq->TransmitNumber;
    m_uiTotalMemorySize = t_pReq->TotalMemorySize;
    m_uiUsedMemorySize = t_pReq->UsedMemorySize;
    m_uiUsedCpuLoad = t_pReq->UsedCpuLoad;
    m_uiIowaitCpuLoad = t_pReq->IowaitCpuLoad;
    m_uiCacheTotalNum = t_pReq->CacheTotalNum;
    m_uiCacheUsedNum = t_pReq->CacheUsedNum;
    m_uiNetworkCardNum = t_pReq->NetworkCardNum;

    m_listNetworkCardInfo.clear();
    for (int i = 0; i < t_pReq->NetworkCardNum; i++)
    {
        m_listNetworkCardInfo.push_back(t_pReq->NetworkCardInfo[i]);
    }

    return 0;
}

void CStreamServer::offline()
{
    SVS_TRACE();

    std::list<std::string>  BindDevList;
    {
        ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex);
        DevSession* pDevSession = NULL;
        RTDevMAP::iterator iter =  m_DevBindMap.begin();
        for(;iter != m_DevBindMap.end();++iter)
        {
            pDevSession = iter->second;
            if (NULL == pDevSession)
            {
                continue;
            }

            BindDevList.push_back(std::string(pDevSession->m_szDevID));
        }
    }

    CBusinessManager::instance().mu_server_offline(m_szServerID, BindDevList);

    return ;
}

void CStreamServer::outputSessionInfo(string &strDest)
{
    SVS_TRACE();

    char buf[PRINT_BUFFER_SIZE] = {0};
    strDest.clear();

    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex);
    DevSession* pDevSession = NULL;
    RTDevMAP::iterator iter =  m_DevBindMap.begin();
    for(;iter != m_DevBindMap.end();++iter)
    {
        pDevSession = iter->second;
        (void)ACE_OS::snprintf(
                buf, sizeof buf,
                "\r\nSession=%s,sessionCount=%d",
                pDevSession->m_szDevID, pDevSession->m_uiSessionCount);
        (void)strDest.insert(strDest.length(), buf);
    }

    return ;
}


// ----------------------------------------------------------
CRecordServer::CRecordServer()
{
    m_uiServerType = SERVER_TYPE_MU_RECORD;
    m_uibindSessionCount  = 0;
    m_uiPortUsage = SVS_USAGE_LOAD_RATE;
    m_uiTotalCapacity = 200;

}
CRecordServer::~CRecordServer()
{

}

uint32_t CRecordServer::findRTSessionbyContent(const char* pszContentID)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, false);
    ContentSession* pContentSession = NULL;
    RTCOUNTMAP::iterator iter =  m_ContentBindMap.find(pszContentID);
    if(iter == m_ContentBindMap.end())
    {
        return 0;
    }
    pContentSession = iter->second;

    return pContentSession->m_uiSessionCount;
}

uint32_t CRecordServer::getAllSessionCount()
{
    return m_uibindSessionCount;
}

int32_t CRecordServer::allocStreamforRealtime(const char* strContendId)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, -1);
    ContentSession* pContentSession = NULL;
    RTCOUNTMAP::iterator iter =  m_ContentBindMap.find(strContendId);
    if(iter == m_ContentBindMap.end())
    {
        SVS_NEW(pContentSession);
        if(NULL == pContentSession)
        {
            return -1;
        }

        ACE_OS::memset(pContentSession,0,sizeof(ContentSession));
        ACE_OS::strncpy(pContentSession->m_szContentID,
                        strContendId,ACE_OS::strlen(strContendId));
        m_ContentBindMap.insert(RTCOUNTMAP::value_type(strContendId,pContentSession));
    }
    else
    {
        pContentSession = iter->second;
    }


    pContentSession->m_uiSessionCount++;
    SVS_LOG((
        SVS_LM_DEBUG,
        "Alloc adapter for realtime. "
        "adapter=%s, Content=%s,SessionCnt=%d,totalCnt=%d.",
        m_szServerID, strContendId,pContentSession->m_uiSessionCount,m_uibindSessionCount));
    return 0;
}

int32_t CRecordServer::freeStreamforRealtime(const char* strContendId)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, -1);
    ContentSession* pContentSession = NULL;
    RTCOUNTMAP::iterator iter =  m_ContentBindMap.find(strContendId);
    if(iter == m_ContentBindMap.end())
    {
        return -1;
    }

    pContentSession = iter->second;

    pContentSession->m_uiSessionCount--;

    SVS_LOG((
        SVS_LM_DEBUG,
        "Free adapter for realtime. "
        "adapter=%s, Content=%s,SessionCnt=%d,totalCnt=%d.",
        m_szServerID, strContendId,pContentSession->m_uiSessionCount,m_uibindSessionCount));
    if(0 == pContentSession->m_uiSessionCount)
    {
        m_ContentBindMap.erase(iter);
        SVS_DELETE(pContentSession);
    }
    return 0;
}

int32_t CRecordServer::allocStreamforRecord(const char* strContendId)
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, -1);

    SVS_LOG((
        SVS_LM_DEBUG,
        "Alloc adapter for record. "
        "adapter=%s, Content=%s,totalCnt=%d.",
        m_szServerID, strContendId,m_uibindSessionCount));
    return 0;
}
int32_t  CRecordServer::freeStreamforRecorde(const char* strContendId)
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, -1);

    SVS_LOG((
        SVS_LM_DEBUG,
        "Free adapter for record. "
        "adapter=%s, Content=%s,totalCnt=%d.",
        m_szServerID, strContendId,m_uibindSessionCount));
    return 0;
}

void CRecordServer::updateLoadCount(uint32_t ulCount)
{
    m_uibindSessionCount = ulCount;
}


void CRecordServer::display_streamsvr_state()
{
    SVS_TRACE();

    SVS_LOG((
        SVS_LM_DEBUG,
        "Display adapter server state. "
        "adapter=%s, SessionCnt:%d.",
        m_szServerID, m_uibindSessionCount));
}

void CRecordServer::outputSessionInfo(string &strDest)
{
    SVS_TRACE();

    char buf[PRINT_BUFFER_SIZE] = {0};
    strDest.clear();

    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex);
    ContentSession* pContentSession = NULL;
    RTCOUNTMAP::iterator iter =  m_ContentBindMap.begin();
    for(;iter != m_ContentBindMap.end();++iter)
    {
        pContentSession = iter->second;
        (void)ACE_OS::snprintf(
                buf, sizeof buf,
                "\r\nSession=%s,sessionCount=%d",
                pContentSession->m_szContentID, pContentSession->m_uiSessionCount);
        (void)strDest.insert(strDest.length(), buf);
    }

    return ;
}


// ----------------------------------------------------------


CServerManager::CServerManager()
{

    m_ulThreadIndex           = 0;
    m_bRunning                = false;
    m_pServerLink             = NULL;
}

CServerManager::~CServerManager()
{
}



int32_t CServerManager::initialize()
{
    return 0;
}

int32_t CServerManager::start()
{
    return open();
}


int32_t CServerManager::stop()
{
    close();
    return 0;
}


int32_t CServerManager::release()
{
    return 0;
}

int32_t CServerManager::open()
{
    // get self ID
    m_strACSvrID = CAccessControlSvr::instance().getSelfID();

    int32_t nRet = openqueues();
    if(0 != nRet)
    {
        return SVS_ERROR_FAIL;
    }

    nRet = openlistener();
    if(0 != nRet)
    {
        return SVS_ERROR_FAIL;
    }

    size_t *stack_size = NULL;
    try
    {
        stack_size = new size_t[SERVER_THREAD_MAX];
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Create stack size for thread error."));

        delete []stack_size;
        stack_size = NULL;

        return SVS_ERROR_FAIL;
    }

    for ( int32_t i = 0; i < SERVER_THREAD_MAX; i++)
    {
        stack_size[i] = SVS_THREAD_STATCK_MAX;
    }
    m_bRunning = true;
    int32_t ret = activate(THR_NEW_LWP ,
                       SERVER_THREAD_MAX,
                       0,
                       ACE_DEFAULT_THREAD_PRIORITY,
                       SVS_ERROR_FAIL,
                       0,
                       0,
                       0,
                       stack_size,
                       0);

    delete []stack_size;
    stack_size = NULL;

    return ret;
}
void    CServerManager::close()
{
    m_bRunning = false;
    (void)wait();
    CLinkFactory::getInstance()->destroyLink(m_pServerLink);
    m_pServerLink = NULL;
}
int32_t CServerManager::svc()
{
    uint32_t ulIndex = getthreadindex();
    processMsgThread(ulIndex);
    return 0;
}
void CServerManager::freeMessageBlock(ACE_Message_Block* &mb)
{
    SVS_TRACE();

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR, "Invalid parameter. mb is NULL."));
        return;
    }

    (void)mb->release();
    mb = NULL;
}

ACE_Message_Block* CServerManager::allockMessageBlock(size_t length, ACE_Message_Block::ACE_Message_Type type)
{
    SVS_TRACE();

    ACE_Message_Block *mb = NULL;
    try
    {
        mb = new ACE_Message_Block(length, type);

        if (ACE_Message_Block::MB_STOP == type)
        {
            mb->rd_ptr(mb->base());
            mb->wr_ptr(mb->base());
        }
        else if (NULL == mb->data_block() || NULL == mb->base())
        {
            SVS_LOG((
                SVS_LM_ERROR,
                "Allocate AceMessageBlock error. "
                "length=%d, type=%d, data=0x%08x, base=0x%08x.",
                length, type, mb->data_block(), mb->base()));
            freeMessageBlock(mb);
        }
        else
        {
            mb->rd_ptr(mb->base());
            mb->wr_ptr(mb->base());
            (void)ACE_OS::memset(mb->base(), 0, length);
        }
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Allocate AceMessageBlock error. length=%d, type=%d",
            length, type));
        freeMessageBlock(mb);
    }

    if (NULL != mb)
    {
        mb->cont(NULL);
    }

    return mb;
}

CServer* CServerManager::findServer(const char* pszServerID)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex_, NULL);
    std::string strServerID = pszServerID;
    CServer* pServer = NULL;
    std::map<std::string,CServer*>::iterator iter = m_ServerIDMap.find(strServerID);
    if(iter != m_ServerIDMap.end())
    {
        pServer = iter->second;
        (void)pServer->addReference();
        return pServer;
    }
    return NULL;
}
CServer* CServerManager::findServer(const uint32_t ulServerIndex)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex_, NULL);
    CServer* pServer = NULL;
    std::map<uint32_t,CServer*>::iterator iter = m_ServerIndexMap.find(ulServerIndex);
    if(iter != m_ServerIndexMap.end())
    {
        pServer = iter->second;
        (void)pServer->addReference();
        return pServer;
    }
    return NULL;
}
void     CServerManager::ReleaseServer(CServer* pServer)
{
    SVS_LOG((SVS_LM_DEBUG,"CServerManager::ReleaseServer."));
    if(NULL == pServer)
    {
        return ;
    }
    RemoveServer(pServer);
    return;
}



int32_t CServerManager::sendMessage(const uint32_t unHandleIndex,char *pDatabuffer,uint32_t unLength)
{
    if(NULL == m_pServerLink)
    {
        return -1;
    }

    return m_pServerLink->sendMsg(unHandleIndex,pDatabuffer,unLength,0);
}

// put the message into the queue.
void CServerManager::sndMsgToSvrMgr(ACE_Message_Block *mb)
{
    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,"enqueueInnerMessage fail, pointer is null."));

        return;
    }

    SVS_MSG_HEADER *pHeader = (SVS_MSG_HEADER *)(void*)GetCommonHdr(mb->rd_ptr());

    SVS_LOG((SVS_LM_DEBUG, "process message, msgtype[0x%x] transno[0x%x] msglen:[%d].",
                            pHeader->MsgType, pHeader->TransactionNo,pHeader->PacketLength));

    uint32_t ulDealthread = reset_transactionno(pHeader->TransactionNo)%SERVER_THREAD_MAX;

    ACE_Time_Value tv = ACE_Time_Value(ACE_OS::gettimeofday()) + ACE_Time_Value(0, REPORT_THREAD_STATUS_INTERVAL);
    int32_t lRet = m_QueueArray[ulDealthread].enqueue_tail(mb, &tv);
    if (-1 == lRet)
    {
        SVS_LOG((SVS_LM_ERROR,"enqueueInnerMessage fail."));
        freeMessageBlock(mb);
        return ;
    }

    SVS_LOG((SVS_LM_DEBUG, "enqueueInnerMessage success."));

    return;
}

void CServerManager::handleMessage(const char* pszMsg,const uint32_t unMsgLength,
                       const uint32_t unHandleIndex,Result &stResult)
{
    if (NULL == pszMsg)
    {
        return;
    }

    if (sizeof(SVS_MSG_HEADER) > unMsgLength)
    {
        return;
    }

    SVS_MSG_HEADER *pHeader = (SVS_MSG_HEADER *)(void*)pszMsg;
    if(RESULT_TYPE_SEND == stResult.ucResultType)
    {
        SVS_LOG((SVS_LM_ERROR, "handle message fail, msgtype[0x%x] transno[0x%x].",
                    pHeader->MsgType, pHeader->TransactionNo));
        return;
    }

    SVS_LOG((SVS_LM_DEBUG, "handle recv message, msgtype[0x%x] transno[0x%x] msglen:[%d].",
                        pHeader->MsgType, pHeader->TransactionNo,pHeader->PacketLength));

    uint32_t ulDealthread = unHandleIndex%SERVER_THREAD_MAX;
    ACE_Message_Block *mb = NULL;
    mb = allockMessageBlock(unMsgLength+sizeof(SVS_RECV_MSG_HDR));
    if(NULL == mb)
    {
        return;
    }
    SVS_RECV_MSG_HDR* pHead = (SVS_RECV_MSG_HDR*)(void*)mb->rd_ptr();
    pHead->hdr.sendQueue    = unHandleIndex;

    mb->wr_ptr(sizeof(SVS_RECV_MSG_HDR));
    mb->copy(pszMsg,unMsgLength);

    ACE_Time_Value tv = ACE_Time_Value(ACE_OS::gettimeofday()) + ACE_Time_Value(0, REPORT_THREAD_STATUS_INTERVAL);
    int32_t lRet = m_QueueArray[ulDealthread].enqueue_tail(mb, &tv);
    if (-1 == lRet)
    {
        SVS_LOG((SVS_LM_ERROR,"enqueueInnerMessage fail."));
        freeMessageBlock(mb);
        return ;
    }

    SVS_LOG((SVS_LM_DEBUG, "handle recv message success."));

    return;
}

void CServerManager::handleConnEvent(const ConnEvent* pstEvent)
{
    if (NULL == pstEvent)
    {
        SVS_LOG((SVS_LM_WARNING,
                    "handleConnEvent, event pointer is null."));
        return;
    }

    SVS_LOG((SVS_LM_DEBUG,
                "handleConnEvent, event:%d(0:EVENT_TYPE_DISCONNECT|1:EVENT_TYPE_CONNECTED|2:EVENT_TYPE_TIMEOUT ), index:%d.",
                pstEvent->EventType, pstEvent->unHandleIndex));

    switch(pstEvent->EventType)
    {
        case EVENT_TYPE_DISCONNECT:
        {
            // TODO: release all the business on this server?
            CServer* pServer = findServer(pstEvent->unHandleIndex);
            if(NULL != pServer)
            {
                SVS_LOG((SVS_LM_DEBUG,
                            "handleConnEvent, find the index.."));
                pServer->offline();
                this->ReleaseServer(pServer);
                this->ReleaseServer(pServer);

            }
            else
            {
                SVS_LOG((SVS_LM_DEBUG,
                            "handleConnEvent, cann't find the index.."));
            }

            break;
        }

        case EVENT_TYPE_CONNECTED:
        {
            SVS_LOG((SVS_LM_DEBUG, "handleConnEvent, server connected.."));
            break;
        }

        case EVENT_TYPE_TIMEOUT:
        {
            // TODO: release all the business on this server?
            CServer* pServer = findServer(pstEvent->unHandleIndex);
            if(NULL != pServer)
            {
                SVS_LOG((SVS_LM_DEBUG,
                            "handleConnEvent,timeout , find the index.."));
                pServer->offline();
                this->ReleaseServer(pServer);
                this->ReleaseServer(pServer);

            }
            else
            {
                SVS_LOG((SVS_LM_DEBUG,
                            "handleConnEvent,timeout, cann't find the index.."));
            }
            break;
        }

        default:
            break;
    }

    return;
}




int32_t CServerManager::openqueues()
{
    uint32_t i = 0;
    bool bSuccess = true;

    for(i = 0;i < SERVER_THREAD_MAX;i++)
    {
        if(0 !=  m_QueueArray[i].open(SVS_MSG_QUEUE_WATER,SVS_MSG_QUEUE_WATER))
        {
            bSuccess = false;
            break;
        }
    }

    if(true == bSuccess)
    {
        return 0;
    }

    for(i = 0;i < SERVER_THREAD_MAX;i++)
    {
        (void)m_QueueArray[i].close();
    }
    return -1;
}
int32_t CServerManager::openlistener()
{
    ADDR  ListAddr;

    //TODO
    // ListAddr.usPort = 4430;//SVS_SCHEDULER::instance()->get_signal_mu_port();
    // ListAddr.unIp =   1000;//SVS_SCHEDULER::instance()->get_signal_ip();
    ListAddr.usPort = CAccessControlSvr::instance().getMuListenPort();
    ListAddr.unIp =   CAccessControlSvr::instance().getMuListenIP(0);

    int32_t nRet = CLinkFactory::getInstance()->init(DEFAULT_HEARTBEAT_INTERVAL, CSVSLoger::instance());
    if (SVS_STACK_OK != nRet)
    {
        return SVS_ERROR_FAIL;
    }

    nRet = CLinkFactory::getInstance()->run();
    if (SVS_STACK_OK != nRet)
    {
        return SVS_ERROR_FAIL;
    }

    nRet = CLinkFactory::getInstance()->createServerLink(&ListAddr,this,m_pServerLink);
    if(0 != nRet)
    {
        return SVS_ERROR_FAIL;
    }

    if(NULL == m_pServerLink)
    {
        return SVS_ERROR_FAIL;
    }
    nRet = m_pServerLink->startConn();

    if(0 != nRet)
    {
        CLinkFactory::getInstance()->destroyLink(m_pServerLink);
        m_pServerLink = NULL;
        return SVS_ERROR_FAIL;
    }
    return 0;
}
void CServerManager::processMsgThread(uint32_t ulThreadIndex)
{
    char threadName[MAX_THREAD_NAME] = {0};
    (void)snprintf( threadName,sizeof(threadName),"ServerMsgThread[%u]",ulThreadIndex);

    CThread_Stat_Reporter reporter(threadName);
    ACE_Time_Value tvDelay(REPORT_THREAD_STATUS_INTERVAL);
    ACE_Time_Value tv;

    ACE_Message_Block *mb = NULL;
    while(m_bRunning)
    {
        tv = ACE_OS::gettimeofday() + tvDelay;
        if (-1 == m_QueueArray[ulThreadIndex].dequeue_head(mb, &tv))
        {
            reporter.ReportStat();
            continue;
        }

        if (NULL != mb)
        {
            if (1 != processMsg(mb))
            {
                freeMessageBlock(mb);
            }
        }
        reporter.ReportStat();
    }
    return;
}
int32_t CServerManager::processMsg(ACE_Message_Block *mb)
{
    if(NULL == mb)
    {
        return 0;
    }
    int32_t iRet = 0;
    SVS_MSG_HEADER *pHeader = (SVS_MSG_HEADER *)(void*)GetCommonHdr(mb->rd_ptr());

    SVS_LOG((SVS_LM_DEBUG, "process message, msgtype[0x%x] transno[0x%x] msglen:[%d].",
                            pHeader->MsgType, pHeader->TransactionNo,pHeader->PacketLength));

    switch(pHeader->MsgType)
    {
    case SVS_MSG_TYPE_SERVER_REGIST_REQ:
        handleServerReg(byte2int(mb),GetCommonHdr(mb->rd_ptr()));
        break;
    case SVS_MSG_TYPE_STREAM_SESSION_SETUP_REQ:
        {
            iRet = CBusinessManager::instance().mu_session_setup_req(mb);
        }
        break;
    case SVS_MSG_TYPE_AC_INTER_MSG_REPORT_RESP:
        {
            CBusinessManager::instance().http_report_response(mb);
            break;
        }
    case SVS_MSG_TYPE_AC_INTER_MSG_INVITE_RESP:
        {
            CBusinessManager::instance().notify_dev_start_stream_response(mb);
            break;
        }
    case SVS_MSG_TYPE_AC_INTER_MSG_BYE_RESP:
        {
            CBusinessManager::instance().notify_dev_stop_stream_response(mb);
            break;
        }
    case SVS_MSG_TYPE_MU_SESSION_REPORT_REQ:
        {
            handleServerBusinessReport(byte2int(mb),GetCommonHdr(mb->rd_ptr()));
            break;
        }
    case SVS_MSG_TYPE_STREAM_LOAD_INFO_REQ:
        {
            handleServerLoadInfoReq(byte2int(mb),GetCommonHdr(mb->rd_ptr()));
            break;
        }
    case SVS_MSG_TYPE_STREAM_SESSION_TEARDOWN_REQ:
        {
            iRet = CBusinessManager::instance().mu_session_teardown_req(mb);
            break;
        }
    case SVS_MSG_TYPE_STREAM_SESSION_PLAY_REQ:
        {
            CBusinessManager::instance().mu_session_play_req(mb);
            break;
        }
    case SVS_MSG_TYPE_MEDIA_KEYFRAME_REQ:
        {
            CBusinessManager::instance().mu_media_keyframe_req(mb);
            break;
        }
    case SVS_MSG_TYPE_AC_INTER_MSG_KEYFRAME_RESP:
        {
            CBusinessManager::instance().mu_media_keyframe_resp(mb);
            break;
        }
    default:
        break;
    }

    return iRet;
}

void CServerManager::handleServerReg(uint32_t ulHandleIndex,const char* pszMsg)
{
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex_);

    if(NULL == pszMsg)
    {
        return;
    }

    CServer* pServer = NULL;

    SVS_MSG_SERVER_REGIST_REQ* pReqMsg = (SVS_MSG_SERVER_REGIST_REQ*)(void*)pszMsg;

    SVS_LOG((SVS_LM_DEBUG, "handle server register message, msgtype[0x%x] transno[0x%x] msglen:[%d].",
            pReqMsg->MsgHeader.MsgType, pReqMsg->MsgHeader.TransactionNo,pReqMsg->MsgHeader.PacketLength));

    int32_t nRet = AddServer(ulHandleIndex,pReqMsg,pServer);

    if(0 != nRet)
    {
        if(NULL != pServer)
        {
            (void)pServer->decReference();
            RemoveServer(pServer);
            pServer = NULL;
        }
    }

    SVS_MSG_SERVER_REGISTER_RESP stRespMsg;
    memset(&stRespMsg,0,sizeof(SVS_MSG_SERVER_REGISTER_RESP));
    stRespMsg.MsgHeader.ProtocolVersion = pReqMsg->MsgHeader.ProtocolVersion;
    stRespMsg.MsgHeader.TransactionNo   = transactionno_respond(pReqMsg->MsgHeader.TransactionNo);
    stRespMsg.MsgHeader.PacketLength    = sizeof(SVS_MSG_SERVER_REGISTER_RESP);
    stRespMsg.MsgHeader.MsgType         = SVS_MSG_TYPE_SERVER_REGISTER_RESP;
    stRespMsg.RespCode                  = nRet;
    stRespMsg.ReportMediaTaskFlag       = REPORT_MEDIA_TASK_UNREPORT;

    strncpy(stRespMsg.ServerID, m_strACSvrID.c_str(), SERVER_ID_LEN);

    nRet = sendMessage(ulHandleIndex,(char*)&stRespMsg,sizeof(SVS_MSG_SERVER_REGISTER_RESP));
    if(0 != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"send server register response message fail."));
        // need to clean the regist info.
        if(NULL != pServer)
        {
            (void)pServer->decReference();
            RemoveServer(pServer);
        }
    }

    return;
}

int32_t CServerManager::AddServer(uint32_t ulHandleIndex,SVS_MSG_SERVER_REGIST_REQ* pReqMsg,CServer*& pServer)
{
    SVS_LOG((SVS_LM_DEBUG,"CServerManager::AddServer."));
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex_, -1);

    if(NULL == pReqMsg)
    {
        SVS_LOG((SVS_LM_ERROR,"CServerManager::AddServer failed, message pointer is NULL."));
        return -1;
    }

    char szServerID[SERVER_ID_LEN+1] = {0};
    memcpy(szServerID,pReqMsg->BindServerID,SERVER_ID_LEN);
    std::map<uint32_t,CServer*>::iterator iter = m_ServerIndexMap.find(ulHandleIndex);
    if(iter != m_ServerIndexMap.end())
    {
        SVS_LOG((SVS_LM_ERROR,"CServerManager::AddServer failed, server index already exist, "
                    "index:%d, ServerID:%s.", ulHandleIndex, szServerID));
        return -1;
    }

    std::map<std::string,CServer*>::iterator iter1 = m_ServerIDMap.find(szServerID);
    if(iter1 != m_ServerIDMap.end())
    {
        SVS_LOG((SVS_LM_ERROR,"CServerManager::AddServer failed, server id already exist, "
                    "index:%d, ServerID:%s.", ulHandleIndex, szServerID));
        return -1;
    }

    SVS_LOG((SVS_LM_DEBUG,"CServerManager::AddServer, server index:%d, server id:%s, server type:%d.",
                ulHandleIndex, szServerID, pReqMsg->ServerType));

    pServer = NULL;
    char szIP[SVS_IP_LEN+1] = {0};

    if(SERVER_TYPE_MU_STREAM == pReqMsg->ServerType)
    {
        CStreamServer* pStreamSvr = new CStreamServer();
        pServer = pStreamSvr;

        memcpy(szIP,pReqMsg->serviceIP,SVS_IP_LEN);//TODO:only get first ip
        pStreamSvr->setMediaSvrIP(szIP);
        pStreamSvr->SetMediaRtspPort(pReqMsg->RtspservicePort);
        pStreamSvr->SetMediaHlsPort(pReqMsg->HlsservicePort);
        pStreamSvr->SetMediaRtmpPort(pReqMsg->RtmpservicePort);
    }
    else if (SERVER_TYPE_MU_RECORD == pReqMsg->ServerType)
    {
        CRecordServer* pRecordSvr = new CRecordServer();
        pServer = pRecordSvr;
        memcpy(szIP,pReqMsg->serviceIP,SVS_IP_LEN);//TODO:only get first ip
        pRecordSvr->setMediaSvrIP(szIP);
        pRecordSvr->SetMediaRtspPort(pReqMsg->RtspservicePort);
        pRecordSvr->SetMediaHlsPort(pReqMsg->HlsservicePort);
        pRecordSvr->SetMediaRtmpPort(pReqMsg->RtmpservicePort);
    }
    else
    {
        SVS_LOG((SVS_LM_DEBUG,"CServerManager::AddServer failed, wrong server type, "
                    "server index:%d, server id:%s, server type:%d.",
                    ulHandleIndex, szServerID, pReqMsg->ServerType));
        return -1;
    }

    pServer->setServerID(szServerID);
    pServer->setServerIndex(ulHandleIndex);
    pServer->setServerName(pReqMsg->Name);
    pServer->setServerIP(szIP);
    pServer->setNetType(pReqMsg->ServiceNetType);
    pServer->setLastRunTime(pReqMsg->LastRunTime);
    pServer->setResetFlags(pReqMsg->ResetFlags);

    m_ServerIndexMap.insert(std::map<uint32_t,CServer*>::value_type(ulHandleIndex,pServer));
    m_ServerIDMap.insert(std::map<std::string,CServer*>::value_type(szServerID,pServer));
    pServer->addReference();

    SVS_LOG((SVS_LM_DEBUG,"CServerManager::AddServer, add sever success."));
    return 0;
}
void  CServerManager::RemoveServer(CServer* pServer)
{
    SVS_LOG((SVS_LM_DEBUG,"CServerManager::RemoveServer."));
    if(NULL == pServer)
    {
        return ;
    }
    // uint32_t ulServerIndex = 0;
    {
        ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex_);
        if(0 < pServer->decReference())
        {
            return ;
        }

        // ulServerIndex = pServer->getServerIndex();
        std::map<std::string,CServer*>::iterator iter = m_ServerIDMap.find(pServer->getServerID());
        if(iter != m_ServerIDMap.end())
        {
            m_ServerIDMap.erase(iter);
        }

        std::map<uint32_t,CServer*>::iterator iter1 = m_ServerIndexMap.find(pServer->getServerIndex());
        if(iter1 != m_ServerIndexMap.end())
        {
            m_ServerIndexMap.erase(iter1);
        }

        delete pServer;
        pServer = NULL;
    }

    // strServerID;
    // TODO: what to do when mu offline.
    //CBusinessManager::instance().adapter_offline(ulServerIndex);

    return;
}

CServer* CServerManager::allocMuServer( PLAY_TYPE enParaPlayType,const char* pszDevID)
{
    CServer* pServer = NULL;
    if(enParaPlayType == PLAY_TYPE_LIVE)
    {
        pServer = allocStreamSvrforRealtime(pszDevID);
    }
    else if(enParaPlayType == PLAY_TYPE_PLAT_RECORD)
    {
        pServer = allocRecordSvrforRecord(pszDevID);
    }
    else
    {
        SVS_LOG((SVS_LM_ERROR, "the mu not support the service type:[%d] .", enParaPlayType));
        return NULL;
    }
    return pServer;
}

CServer* CServerManager::allocStreamSvrforRealtime(const char* pszDevID)
{
    CServer*    pServer = NULL;
    pServer = allocExistStreamSvrforRealtime(pszDevID);
    if(NULL != pServer)
    {
        return pServer;
    }

    pServer = allocBestStreamSvrforRealtime(pszDevID);

    return pServer;
}

CServer* CServerManager::allocExistStreamSvrforRealtime(const char* pszDevID)
{
    CServer*    pServer = NULL;
    CStreamServer* pStreamServer = NULL;
    uint32_t        uiCount        = 0;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex_, NULL);
    std::map<uint32_t,CServer*>::iterator iter = m_ServerIndexMap.begin();
    for(;iter != m_ServerIndexMap.end();++iter)
    {
        pServer = iter->second;

        if(SERVER_TYPE_MU_STREAM != pServer->getServerType())
        {
            continue;
        }
        pStreamServer = (CStreamServer*)(void*)pServer;
        uiCount = pStreamServer->findRTSessionbyDevID(pszDevID);

        if(0 == uiCount)
        {
            continue;
        }

        if(SVS_USAGE_LOAD_RATE <= uiCount)
        {
            continue;
        }

        return pServer;
    }

    return NULL;
}

CServer* CServerManager::allocBestStreamSvrforRealtime(const char* pszDevID)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex_, NULL);
    CServer*    pServer = NULL;
    CServer*    pBestServer = NULL;
    CStreamServer* pStreamServer = NULL;
    uint32_t        uiCount        = 0;
    uint32_t        uiMinCount     = 0xFFFFFFFF;
    std::map<uint32_t,CServer*>::iterator iter = m_ServerIndexMap.begin();
    for(;iter != m_ServerIndexMap.end();++iter)
    {
        pServer = iter->second;
        if(SERVER_TYPE_MU_STREAM != pServer->getServerType())
        {
            continue;
        }
        pStreamServer = (CStreamServer*)(void*)pServer;
        uiCount = pStreamServer->getAllSessionCount();
        if(uiMinCount > uiCount)
        {
            pBestServer = pServer;
            uiMinCount = uiCount;
        }
    }
    if(NULL != pBestServer)
    {
        (void)pBestServer->addReference();
    }
    return pBestServer;
}


/*
CServer* CServerManager::allocBestStreamSvrforRealtime(const char* pszDevID)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex_, NULL);
    CServer*    pServer = NULL;
    CServer*    pBestServer = NULL;
    CStreamServer* pStreamServer = NULL;

    uint32_t        uiCurrentSvrHealth = NO_LOAD_HEALTH;
    uint32_t        uiSvrHealthTmp = 0;

    uint32_t        uiCurrentSvrLoadType = SVR_LOAD_TYPE_MAX;
    uint32_t        uiSvrLoadTypeTmp = SVR_LOAD_TYPE_MAX;

    std::map<uint32_t,CServer*>::iterator iter = m_ServerIndexMap.begin();
    for(;iter != m_ServerIndexMap.end();++iter)
    {
        pServer = iter->second;
        if(SERVER_TYPE_MU_STREAM != pServer->getServerType())
        {
            continue;
        }
        pStreamServer = (CStreamServer*)pServer;
        uiSvrLoadTypeTmp = pStreamServer->load_type();
        if (uiCurrentSvrLoadType < uiSvrLoadTypeTmp)
        {
            // heavy than current server.
            continue;
        }
        else if (uiCurrentSvrLoadType > uiSvrLoadTypeTmp)
        {
            // light than current server.
            pBestServer = (CStreamServer*)(void*)pServer;
            uiCurrentSvrLoadType = uiSvrLoadTypeTmp;
            uiCurrentSvrHealth = pStreamServer->get_health_value();
            continue;
        }
        else
        {
            // same load type.
            uiSvrHealthTmp = pStreamServer->get_health_value();

            // bigger is more health.
            if (uiCurrentSvrHealth < uiSvrHealthTmp)
            {
                // more health
                pBestServer = (CStreamServer*)(void*)pServer;
                uiCurrentSvrHealth = uiSvrHealthTmp;
            }
        }
    }


    // cann't use heavy server.
    // if (SVR_LOAD_TYPE_HEAVY <= uiCurrentSvrLoadType)
    //  {
    //     return NULL;
    // }


    if(NULL != pBestServer)
    {
        (void)pBestServer->addReference();
    }

    return pBestServer;
}
*/

CServer* CServerManager::allocRecordSvrforRecord(const char* strContendId)
{
    return NULL;
}



void CServerManager::handleServerBusinessReport(uint32_t ulHandleIndex,const char* pszMsg)
{
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex_);

    if(NULL == pszMsg)
    {
        return;
    }

    int32_t     nRet = 0;

    SVS_MSG_MU_BUSINESS_REPORT_REQ* pReqMsg = (SVS_MSG_MU_BUSINESS_REPORT_REQ*)(void*)pszMsg;
    CServer* pCServer = NULL;

    SVS_LOG((SVS_LM_DEBUG, "handle server business report message, msgtype[0x%x] transno[0x%x] msglen:[%d].",
            pReqMsg->MsgHeader.MsgType, pReqMsg->MsgHeader.TransactionNo,pReqMsg->MsgHeader.PacketLength));

    pCServer = findServer(ulHandleIndex);

    if (NULL == pCServer)
    {
        SVS_LOG((SVS_LM_ERROR, "find server by server queue id:%d failed when dealing business report request.",
                  ulHandleIndex));
        return;
    }

    CStreamServer* pCStreamServer = NULL;
    pCStreamServer = (CStreamServer*)pCServer;

    // deal the request.
    // TODO: send the business report
    //pCStreamServer->handle_business_report_req(pszMsg);

    this->ReleaseServer(pCServer);

    SVS_MSG_COMMON_RESP stRespMsg;
    memset(&stRespMsg,0,sizeof(SVS_MSG_COMMON_RESP));
    stRespMsg.MsgHeader.ProtocolVersion = pReqMsg->MsgHeader.ProtocolVersion;
    stRespMsg.MsgHeader.TransactionNo   = transactionno_respond(pReqMsg->MsgHeader.TransactionNo);
    stRespMsg.MsgHeader.PacketLength    = sizeof(SVS_MSG_COMMON_RESP);
    stRespMsg.MsgHeader.MsgType         = SVS_MSG_TYPE_MU_SESSION_REPORT_RESP;
    stRespMsg.RespCode                  = nRet;
    stRespMsg.RequestType               = SVS_MSG_TYPE_MU_SESSION_REPORT_REQ;

    nRet = sendMessage(ulHandleIndex,(char*)&stRespMsg,sizeof(SVS_MSG_COMMON_RESP));
    if(0 != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"send server business report response message fail."));
    }

    return;
}


void CServerManager::handleServerLoadInfoReq(uint32_t ulHandleIndex,const char* pszMsg)
{
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex_);

    if(NULL == pszMsg)
    {
        return;
    }

    int32_t     nRet = 0;

    SVS_MSG_STREAM_LOAD_INFO_REQ* pReqMsg = (SVS_MSG_STREAM_LOAD_INFO_REQ*)(void*)pszMsg;
    CServer* pCServer = NULL;

    SVS_LOG((SVS_LM_DEBUG, "handle server load info report message, msgtype[0x%x] transno[0x%x] msglen:[%d].",
            pReqMsg->MsgHeader.MsgType, pReqMsg->MsgHeader.TransactionNo,pReqMsg->MsgHeader.PacketLength));

    pCServer = findServer(ulHandleIndex);

    if (NULL == pCServer)
    {
        SVS_LOG((SVS_LM_ERROR, "find server by server queue id:%d failed when dealing load info report request.",
                  ulHandleIndex));
        return;
    }

    CStreamServer* pCStreamServer = NULL;
    pCStreamServer = (CStreamServer*)pCServer;

    // deal the request.
    pCStreamServer->handle_load_info_report_req(pszMsg);

    this->ReleaseServer(pCServer);

    SVS_MSG_COMMON_RESP stRespMsg;
    memset(&stRespMsg,0,sizeof(SVS_MSG_COMMON_RESP));
    stRespMsg.MsgHeader.ProtocolVersion = pReqMsg->MsgHeader.ProtocolVersion;
    stRespMsg.MsgHeader.TransactionNo   = transactionno_respond(pReqMsg->MsgHeader.TransactionNo);
    stRespMsg.MsgHeader.PacketLength    = sizeof(SVS_MSG_COMMON_RESP);
    stRespMsg.MsgHeader.MsgType         = SVS_MSG_TYPE_STREAM_LOAD_INFO_RESP;
    stRespMsg.RespCode                  = nRet;
    stRespMsg.RequestType       = SVS_MSG_TYPE_STREAM_LOAD_INFO_REQ;

    nRet = sendMessage(ulHandleIndex,(char*)&stRespMsg,sizeof(SVS_MSG_COMMON_RESP));
    if(0 != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"send server load info report response message fail."));
    }

    return;
}

void CServerManager::show_server_info(const std::string& strSvrID,std::string& Info)
{
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex_);
    std::map<std::string,CServer*>::iterator iter;
    CServer* pServer = NULL;
    std::string strTmp;
    stringstream strValue;
    if(0 < strSvrID.length())
    {
        iter = m_ServerIDMap.find(strSvrID);
        if(iter == m_ServerIDMap.end())
        {
            Info = "not find the server info";
            return;
        }
        pServer = iter->second;
        strTmp = pServer->getServerID();
        Info = "ID:" + strTmp + "\r\n";
        strTmp = pServer->getServerIP();
        Info += "Address:" + strTmp + "\r\n";
        strTmp = pServer->getServerName();
        Info += "Name:" + strTmp + "\r\n";

        strValue.str("");
        strValue << pServer->getMediaRtspPort();
        strTmp =strValue.str();
        Info += "rtsp port:" + strTmp + "\r\n";

        strValue.str("");
        strValue << pServer->getMediaRtmpPort();
        strTmp =strValue.str();
        Info += "rtsp port:" + strTmp + "\r\n";

        strValue.str("");
        strValue << pServer->getMediaHlsPort();
        strTmp =strValue.str();

        Info += "hls port:" + strTmp + "\r\n";

        return;
    }

    iter = m_ServerIDMap.begin();
    for(;iter != m_ServerIDMap.end();++iter)
    {
        pServer = iter->second;
        strTmp = pServer->getServerID();
        Info += "ID:[" + strTmp + "] ";
        strTmp = pServer->getServerIP();
        Info += "Address:[" + strTmp + "]\r\n";
    }

    return;
}




