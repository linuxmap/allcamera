#include "svs_device_stack_gb28181.h"
#include "svs_log_msg.h"
#include "svs_logging_strategy.h"
#include "svs_ac_common.h"
#include "ace/OS_Log_Msg_Attributes.h"
#include "svs_gb28181_manscdp.h"
#include "svs_access_control_manager.h"
#include "svs_acm_request.h"
#include "svs_access_control.h"

static ACE_OS_Log_Msg_Attributes g_objACELogMsgAttr;


CDeviceStackGB28181::CDeviceStackGB28181()
    : m_pEXosipCtx(NULL)
    , m_bRunning(false)
    , m_nThreadIndex(0)
    , m_tSendQueryCatalogReqTime(0)
{
    SVS_TRACE();
}

CDeviceStackGB28181::~CDeviceStackGB28181()
{
    SVS_TRACE();
}

static void osip_trace_log_callback(char *fi, int li, osip_trace_level_t level, char *chfr, va_list ap)
{
    if (!ACE_Log_Msg::exists())
    {
        ACE_Log_Msg::inherit_hook(NULL, g_objACELogMsgAttr);
        SVS_LOG((SVS_LM_INFO, "ACE_Log_Msg::inherit_hook in osip_trace_log_callback."));
    }

    //SVS_TRACE();

    char szLogTmp[1024] = { 0 };
    (void)::vsnprintf(szLogTmp, sizeof(szLogTmp), chfr, ap);

    int32_t nLogLM = SVS_LM_CRITICAL;

    switch (level)
    {
    case OSIP_INFO1:
    case OSIP_INFO2:
    case OSIP_INFO3:
    case OSIP_INFO4:
        nLogLM = SVS_LM_DEBUG;
        break;
    case OSIP_WARNING:
        nLogLM = SVS_LM_WARNING;
        break;
    case OSIP_ERROR:
    case OSIP_BUG:
        nLogLM = SVS_LM_ERROR;
        break;
    case OSIP_FATAL:
        nLogLM = SVS_LM_CRITICAL;
        break;
    default:
        break;
    }
    SVS_LOG((nLogLM, "osip:[%s:%d]:%s", fi, li,szLogTmp));
}

static void initExosipLog()
{
    SVS_TRACE();

    ACE_Log_Msg::init_hook(g_objACELogMsgAttr);

    int32_t nLogLM = SVS_Log_Msg::get_log_priority();
    osip_trace_level_t oSipLogLevel = OSIP_WARNING;

    switch (nLogLM)
    {
    case SVS_LM_TRACE:
    case SVS_LM_DEBUG:
        oSipLogLevel = OSIP_INFO4;
        break;
    case SVS_LM_INFO:
        oSipLogLevel = OSIP_INFO3;
        break;
    case SVS_LM_WARNING:
        oSipLogLevel = OSIP_WARNING;
        break;
    case SVS_LM_ERROR:
        oSipLogLevel = OSIP_ERROR;
        break;
    case SVS_LM_CRITICAL:
        oSipLogLevel = OSIP_FATAL;
        break;
    default:
        break;
    }

    osip_trace_initialize_func(osip_trace_level_t(oSipLogLevel + 1), &osip_trace_log_callback);
    SVS_LOG((SVS_LM_INFO, "osip_is_trace_level_activate %d is %d.", oSipLogLevel, osip_is_trace_level_activate(oSipLogLevel)));
}

int32_t CDeviceStackGB28181::initExosip()
{
    SVS_TRACE();

    initExosipLog();

    m_pEXosipCtx = eXosip_malloc();

    int32_t nResult = eXosip_init(m_pEXosipCtx);
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "eXosip_init failed, result is %d.", nResult));
        return SVS_ERROR_FAIL;
    }

    int32_t nPort = CAccessControlSvr::instance().get_device_gb28181_stack_listen_port();
    const char* pszIP = CAccessControlSvr::instance().getSvrIp();
    nResult = eXosip_listen_addr(m_pEXosipCtx, IPPROTO_UDP, pszIP, nPort, AF_INET, 0);
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "eXosip_listen_addr UDP port %u failed, result is %d.", nPort, nResult));
        return SVS_ERROR_FAIL;
    }

    const char* pszDomain = CAccessControlSvr::instance().getSelfID();
    char szPort[SVS_PORT_LEN + 1];
    snprintf(szPort, sizeof(szPort), "%d", nPort);
    m_strFrom = "sip:";
    m_strFrom = m_strFrom + pszDomain + "@" + pszIP + ":" + szPort;

    eXosip_masquerade_contact(m_pEXosipCtx, pszIP, nPort);

    eXosip_set_user_agent(m_pEXosipCtx, ALLCAM_AGENT_NAME);
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::activeThread()
{
    SVS_TRACE();

    size_t *pArrayStackSize = NULL;
    try
    {
        pArrayStackSize = new size_t[SVS_GB28181_RECV_THREAD_MAX];
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Create stack size for thread error."));
        return SVS_ERROR_FAIL;
    }

    for ( int32_t i = 0; i < SVS_GB28181_RECV_THREAD_MAX; i++)
    {
        pArrayStackSize[i] = SVS_THREAD_STATCK_MAX;
    }

    m_bRunning = true;
    int32_t nResult = activate(THR_NEW_LWP,
                       SVS_GB28181_RECV_THREAD_MAX,
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

int32_t CDeviceStackGB28181::initialize()
{
    SVS_TRACE();

    int32_t nResult = initExosip();
    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Initialize exosip failed."));
        return SVS_ERROR_FAIL;
    }

    nResult = activeThread();
    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Active device stack thread failed."));
        return SVS_ERROR_FAIL;
    }

    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::release()
{
    SVS_TRACE();
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::start()
{
    SVS_TRACE();
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::stop()
{
    SVS_TRACE();
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::timeout()
{
    if (0 == m_tSendQueryCatalogReqTime)
    {
        (void)time(&m_tSendQueryCatalogReqTime);
        return SVS_ERROR_OK;
    }

    time_t nCurrentTime;
    (void)time(&nCurrentTime);
    if (nCurrentTime - m_tSendQueryCatalogReqTime >= CAccessControlSvr::instance().get_query_catalog_interval())
    {
        sendQueryCatalogReq();
        (void)time(&m_tSendQueryCatalogReqTime);
    }
    checkExpires();
    return SVS_ERROR_OK;
}


int32_t CDeviceStackGB28181::response(eXosip_event_t& rEvent, int32_t nErrorCode)
{
    SVS_TRACE();

    int32_t nResult = OSIP_SUCCESS;
    //osip_message_t *pAnswer = NULL;
    /*若对pAnswer无特殊处理，eXosip_message_send_answer中会自动调用eXosip_message_build_answer
    nResult = eXosip_message_build_answer(m_pEXosipCtx, rEvent.tid, nRespCode, &pAnswer);
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Build %s response failed, error code is %d.",
            rEvent.request->sip_method, nResult));
        return SVS_ERROR_FAIL;
    }
    */

    int32_t nRespCode = SIP_BAD_REQUEST;
    if(SVS_ERROR_OK == nErrorCode)
    {
        nRespCode = SIP_OK;
    }
    else if(SVS_ERROR_USER_NO_EXIST == nErrorCode)
    {
        nRespCode = SIP_NOT_FOUND;
    }


    CExOsipLockGuard objExOsipLockGuard(m_pEXosipCtx);
    nResult = eXosip_message_send_answer(m_pEXosipCtx, rEvent.tid, nRespCode, NULL);
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Send %s response failed, error code is %d.",
            rEvent.request->sip_method, nResult));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_INFO, "Send %s response success, response code is %d.",
            rEvent.request->sip_method, nRespCode));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::handleRegisterReq(eXosip_event_t& rEvent)
{
    SVS_TRACE();

    int32_t nResult = SVS_ERROR_OK;
    std::string strTo;
    std::string strBody;
    bool bNewRegister = false;

    do
    {
        //expires字段
        osip_header_t* pExpires = NULL;
        osip_message_get_expires(rEvent.request, 0, &pExpires);
        if (NULL == pExpires)
        {
            SVS_LOG((SVS_LM_ERROR, "Parse %s expires failed.", rEvent.request->sip_method));
            break;
        }

        if (pExpires->hvalue[0] != '0') //注册
        {
            int32_t nExpires = atoi(pExpires->hvalue);
            if (nExpires < 0)
            {
                SVS_LOG((SVS_LM_ERROR, "Parse %s expires failed, value is %d.", rEvent.request->sip_method, nExpires));
                break;
            }
            nResult = handleDeviceRegister(rEvent, bNewRegister, strTo, strBody, nExpires);
        }
        else    //注销
        {
            nResult = handleDeviceUnRegister(rEvent);
        }
    }while(0);


    nResult = response(rEvent, nResult);
    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Response %s failed.", rEvent.request->sip_method));
        return SVS_ERROR_FAIL;
    }

    if (bNewRegister)
    {
        //发送Query Catalog请求
        int32_t nTransactionID = 0;
        nResult = sendMessageReq(strTo, strBody, nTransactionID);
        if (0 != nResult)
        {
            SVS_LOG((SVS_LM_ERROR, "Send message request failed."));
            return SVS_ERROR_FAIL;
        }

        //仅开发过程中测试用
        /*
        strBody = "\
v=0\r\n\
o=34020000002000000001 0 0 IN IP4 114.55.109.31\r\n\
s=Play\r\n\
c=IN IP4 114.55.109.31\r\n\
t=0 0\r\n\
m=video 5494 RTP/AVP 96 98 97\r\n\
a=recvonly\r\n\
a=rtpmap:96 PS/90000\r\n\
a=rtpmap:98 H264/90000\r\n\
a=rtpmap:97 MPEG4/90000\r\n\
y=0999999999";
        std::string strSubject = "34020000001320000001:0,34020000002000000001:0";
        int32_t nCallID = 0;
        sendInviteReq(strTo, strBody, strSubject, nCallID);

        INVITE_INFO stInviteInfo;
        strncpy(stInviteInfo.szLensID, "34020000001320000001", sizeof(stInviteInfo.szLensID) - 1);
        stInviteInfo.pCallBack = NULL;
        stInviteInfo.pUserData = NULL;
        stInviteInfo.nCallID = nCallID;

        {
            ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapInviteInfo, SVS_ERROR_FAIL);
            m_mapCallID2InviteInfo[nCallID] = stInviteInfo;
            m_mapLensID2InviteInfo[stInviteInfo.szLensID] = &m_mapCallID2InviteInfo[nCallID];
        }
        */
    }

    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::handleDeviceRegister(eXosip_event_t& rEvent,
    bool& bIsNewRegister, std::string& strTo, std::string& strBody, int32_t nExpires)
{
    SVS_TRACE();

    //contact字段
    osip_contact_t* pContact = NULL;
    osip_message_get_contact(rEvent.request, 0, &pContact);
    if (NULL == pContact)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse %s contact failed.", rEvent.request->sip_method));
        return SVS_ERROR_FAIL;
    }

    const char* pszToHost = pContact->url->host;
    const char* pszToPort = pContact->url->port;

    osip_via_t *pVia = (osip_via_t *)osip_list_get (&rEvent.request->vias, 0);
    if (NULL == pVia)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse %s via failed.", rEvent.request->sip_method));
        return SVS_ERROR_FAIL;
    }


    USER_INFO stUserInfo;
    stUserInfo.nExpires = nExpires;
    (void)time(&stUserInfo.tLastRegisterTime);
    strncpy(stUserInfo.szHost, pContact->url->host, sizeof(stUserInfo.szHost) - 1);
    strncpy(stUserInfo.szPort, pContact->url->port, sizeof(stUserInfo.szPort) - 1);
    strncpy(stUserInfo.szUserName, pContact->url->username, sizeof(stUserInfo.szUserName) - 1);

    osip_generic_param_t *pReceived = NULL;
    osip_generic_param_t *pRport = NULL;
    osip_via_param_get_byname (pVia, "received", &pReceived);
    if (NULL != pReceived)
    {
        osip_via_param_get_byname (pVia, "rport", &pRport);
        if (NULL == pRport)
        {
            SVS_LOG((SVS_LM_ERROR, "Parse %s rport failed.", rEvent.request->sip_method));
            return SVS_ERROR_FAIL;
        }

        pszToHost = pReceived->gvalue;
        pszToPort = pRport->gvalue;
        strncpy(stUserInfo.szHost, pszToHost, sizeof(stUserInfo.szHost) - 1);
        strncpy(stUserInfo.szPort, pszToPort, sizeof(stUserInfo.szPort) - 1);
    }


    stUserInfo.strTo = stUserInfo.strTo + stUserInfo.szUserName + "@" + stUserInfo.szHost + ":" + stUserInfo.szPort;
    strTo = stUserInfo.strTo;

    //设备会定期重复注册，暂不考虑两个不同设备具有相同用户名的问题
    {
        bIsNewRegister = false;
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
        if (m_mapUser.end() == m_mapUser.find(stUserInfo.szUserName))
        {
            bIsNewRegister = true;
        }
        (void)time(&stUserInfo.tLastKeepAliveTime);
        m_mapUser[stUserInfo.szUserName] = stUserInfo;
    }

    if (bIsNewRegister)
    {
        CManscdp objManscdp;
        int32_t nResult = objManscdp.createQueryCatalog(stUserInfo.szUserName);
        if (0 != nResult)
        {
            SVS_LOG((SVS_LM_ERROR, "Create query catalog xml failed."));
            return SVS_ERROR_FAIL;
        }
        strBody = objManscdp.getXML();
    }

    SVS_LOG((SVS_LM_INFO, "Register %s user \"%s\", host is %s, port is %s.",
            bIsNewRegister?"new":"old", stUserInfo.szUserName,
            stUserInfo.szHost, stUserInfo.szPort));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::handleDeviceUnRegister(eXosip_event_t& rEvent)
{
    SVS_TRACE();

    //contact字段
    osip_contact_t* pContact = NULL;
    osip_message_get_contact(rEvent.request, 0, &pContact);
    if (NULL == pContact)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse %s contact failed.", rEvent.request->sip_method));
        return SVS_ERROR_FAIL;
    }

    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
        m_mapUser.erase(pContact->url->username);
    }

    int32_t nResult = notifyDeviceOffline(pContact->url->username);
    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Notfiy device offline failed."));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_INFO, "Unregister user \"%s\".", pContact->url->username));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::notifyDeviceOnline(SVS_ACM::REQUEST_NOTIFY_DEV_INFO& rRequest)
{
    SVS_TRACE();

    int32_t nResult = SVS_ERROR_OK;

    do
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
        MAP_USER::iterator iter = m_mapUser.find(rRequest.stDeviceInfo.szDeviceID);
        if (m_mapUser.end() == iter)
        {
            nResult = SVS_ERROR_USER_NO_EXIST;
            break;
        }

        const USER_INFO& rInfo = iter->second;
        strncpy(rRequest.stDeviceInfo.szHost, rInfo.szHost, sizeof(rRequest.stDeviceInfo.szHost) - 1);
        strncpy(rRequest.stDeviceInfo.szPort, rInfo.szPort, sizeof(rRequest.stDeviceInfo.szPort) - 1);
    }while(0);

    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Notfiy device '%s' online failed, user not found.", rRequest.stDeviceInfo.szDeviceID));
        return nResult;
    }

    nResult = IAccessControlManager::instance().asyncRequest(rRequest);
    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Notfiy device '%s' online failed.", rRequest.stDeviceInfo.szDeviceID));
        return nResult;
    }

    SVS_LOG((SVS_LM_INFO, "Notfiy device '%s' online success.", rRequest.stDeviceInfo.szDeviceID));
    return SVS_ERROR_OK;
}
int32_t CDeviceStackGB28181::notifyDeviceKeepAlive(const char* pszDeviceID)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
    MAP_USER::iterator iter = m_mapUser.find(pszDeviceID);
    if (m_mapUser.end() == iter)
    {
        return SVS_ERROR_USER_NO_EXIST;
    }

    USER_INFO& rUserInfo = iter->second;
    (void)time(&rUserInfo.tLastKeepAliveTime);
    return SVS_ERROR_OK;
}


int32_t CDeviceStackGB28181::notifyDeviceOffline(const char* pszDeviceID)
{
    SVS_TRACE();

    SVS_ACM::REQUEST_NOTIFY_DEV_INFO stRequest;
    SVS_ACM::DEVICE_INFO& stDeviceInfo = stRequest.stDeviceInfo;
    stDeviceInfo.eDeviceType = SVS_DEV_TYPE_GB28181;
    stDeviceInfo.eDeviceStatus = SVS_DEV_STATUS_OFFLINE;
    strncpy(stDeviceInfo.szDeviceID, pszDeviceID, sizeof(stDeviceInfo.szDeviceID) - 1);

    int32_t nResult = IAccessControlManager::instance().asyncRequest(stRequest);
    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Notfiy device '%s' offline failed.", pszDeviceID));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_INFO, "Notfiy device '%s' offline success.", pszDeviceID));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::handleMessageReq(eXosip_event_t& rEvent)
{
    SVS_TRACE();

    int32_t nResult = SVS_ERROR_OK;

    do
    {
        osip_from_t *pFrom = osip_message_get_from(rEvent.request);
        if (NULL == pFrom)
        {
            SVS_LOG((SVS_LM_ERROR, "Parse %s from failed.", rEvent.request->sip_method));
            nResult = SVS_ERROR_FAIL;
            break;
        }

        {
            ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
            if (m_mapUser.find(pFrom->url->username) == m_mapUser.end())
            {
                SVS_LOG((SVS_LM_INFO, "User \"%s\" hasn't register.", pFrom->url->username));
                nResult = SVS_ERROR_USER_NO_EXIST;
                break;
            }
        }

        osip_body_t *pBody = NULL;
        osip_message_get_body(rEvent.request, 0, &pBody);
        if (NULL == pBody)
        {
            SVS_LOG((SVS_LM_ERROR, "Parse %s body failed.", rEvent.request->sip_method));
            nResult = SVS_ERROR_FAIL;
            break;
        }

        CManscdp objManscdp;
        objManscdp.setDeviceStackGB28181(*this);
        nResult = objManscdp.parse(pBody->body);
        if (SVS_ERROR_OK != nResult)
        {
            SVS_LOG((SVS_LM_ERROR, "Parse %s body failed, content is %s.", rEvent.request->sip_method, pBody->body));
            break;
        }

        SVS_LOG((SVS_LM_INFO, "Parse %s body OK, content is %s.", rEvent.request->sip_method, pBody->body));
    }while(0);


    nResult = response(rEvent, nResult);
    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Response %s failed.", rEvent.request->sip_method));
        return SVS_ERROR_FAIL;
    }

    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::svc()
{
    SVS_TRACE();

    int32_t nThreadIndex = m_nThreadIndex++;
    SVS_LOG((SVS_LM_INFO, "Device stack gb28181 thread index is %d.", nThreadIndex));

    if (nThreadIndex < SVS_GB28181_RECV_THREAD_MAX)
    {
        receiveThread();
    }

    return SVS_ERROR_OK;
}

void CDeviceStackGB28181::receiveThread()
{
    SVS_TRACE();

    eXosip_event_t *pEvent = NULL;
    int32_t nResult = SVS_ERROR_OK;

    while (m_bRunning)
    {
        pEvent = eXosip_event_wait(m_pEXosipCtx, 0, 100);
        if (NULL == pEvent)
        {
            continue;
        }

        nResult = checkUserName(pEvent);
        if (SVS_ERROR_OK != nResult)
        {
            continue;
        }

        dealSipEvent(pEvent);
    }
}

int32_t CDeviceStackGB28181::checkUserName(eXosip_event_t *pEvent)
{
    int32_t nResult = SVS_ERROR_OK;
    //int32_t nRespCode = SIP_OK;

    do
    {
        if (NULL != pEvent->request && MSG_IS_REQUEST(pEvent->request))
        {
            osip_uri_t *pURI = osip_message_get_uri(pEvent->request);
            if (NULL == pURI || NULL == pURI->username)
            {
                SVS_LOG((SVS_LM_ERROR, "Parse %s uri failed.", pEvent->request->sip_method));
                nResult = SVS_ERROR_FAIL;
                //nRespCode = SIP_BAD_REQUEST;
                break;
            }

            const char* pszDomain = CAccessControlSvr::instance().getSelfID();
            const char* pszIP = CAccessControlSvr::instance().getSvrIp();
            if (   0 != strncmp(pURI->username, pszDomain, SVS_DEVICEID_LEN)
                && 0 == strncmp(pURI->host, pszIP, SVS_IP_LEN))
            {
                SVS_LOG((SVS_LM_WARNING, "The user name '%s' of request uri is not belong this server. Server user name is %s.",
                    pURI->username, pszDomain));
                nResult = SVS_ERROR_FAIL;
                //nRespCode = SIP_NOT_FOUND;
                break;
            }
        }
    }while(0);

    if (SVS_ERROR_OK != nResult)
    {
        //(void)response(*pEvent, nRespCode);

        {
            CExOsipLockGuard objExOsipLockGuard(m_pEXosipCtx);
            eXosip_event_free(pEvent);
        }
        return nResult;
    }

    return SVS_ERROR_OK;
}

void CDeviceStackGB28181::dealSipEvent(eXosip_event_t *pEvent)
{
    int32_t nResult = SVS_ERROR_OK;

    switch (pEvent->type)
    {
    case EXOSIP_MESSAGE_NEW:
        {
            SVS_LOG((SVS_LM_INFO, "Receivce message %s.", pEvent->request->sip_method));

            if (MSG_IS_REGISTER(pEvent->request))
            {
                nResult = handleRegisterReq(*pEvent);
            }
            else if (MSG_IS_MESSAGE(pEvent->request)
                || MSG_IS_NOTIFY(pEvent->request))
            {
                nResult = handleMessageReq(*pEvent);
            }
            break;
        }
    case EXOSIP_CALL_ANSWERED:
        {
            SVS_LOG((SVS_LM_INFO, "EXOSIP_CALL_ANSWERED: %s, call id is %d.", pEvent->textinfo, pEvent->cid));
            nResult = handleCallAnswered(*pEvent);
            break;
        }
    case EXOSIP_CALL_MESSAGE_ANSWERED:
        {
            SVS_LOG((SVS_LM_INFO, "EXOSIP_CALL_MESSAGE_ANSWERED: %s, call id is %d.", pEvent->textinfo, pEvent->cid));
            nResult = handleCallMessageAnswered(*pEvent);
            break;
        }
    case EXOSIP_MESSAGE_ANSWERED:       //200 OK
        {
            SVS_LOG((SVS_LM_INFO, "Receivce response %d %s.", pEvent->response->status_code, pEvent->response->reason_phrase));
            nResult = handleMessageAnswered(*pEvent);
            break;
        }
    case EXOSIP_CALL_NOANSWER:          //timeout
    case EXOSIP_CALL_REQUESTFAILURE:    //4XX
    case EXOSIP_CALL_SERVERFAILURE:     //5XX
    case EXOSIP_CALL_GLOBALFAILURE:     //6XX
        {
            SVS_LOG((SVS_LM_INFO, "Receivce response %d %s.", pEvent->response->status_code, pEvent->response->reason_phrase));
            nResult = handleCallAnsweredError(*pEvent);
            break;
        }
    case EXOSIP_CALL_PROCEEDING:        //100 Trying
        {
            SVS_LOG((SVS_LM_INFO, "Receivce response %d %s.", pEvent->response->status_code, pEvent->response->reason_phrase));
            break;
        }
    case EXOSIP_CALL_RELEASED:
        {
            SVS_LOG((SVS_LM_INFO, "EXOSIP_CALL_RELEASED: %s.", pEvent->textinfo));
            break;
        }
    case EXOSIP_MESSAGE_REQUESTFAILURE:    //4XX
    case EXOSIP_MESSAGE_SERVERFAILURE:     //5XX
    case EXOSIP_MESSAGE_GLOBALFAILURE:     //6XX
        {
            if((NULL != pEvent->response)&&(NULL != pEvent->response->reason_phrase)) {
                SVS_LOG((SVS_LM_INFO, "Receivce response %d %s.", pEvent->response->status_code, pEvent->response->reason_phrase));
            }
            nResult = handleMessageAnsweredError(*pEvent);
            break;
        }
    default:
        SVS_LOG((SVS_LM_INFO, "eXosip_event_wait OK. type is %d, text info is %s.", pEvent->type, pEvent->textinfo));
        break;
    }

    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Handle %s failed.", pEvent->request->sip_method));
    }

    {
        CExOsipLockGuard objExOsipLockGuard(m_pEXosipCtx);
        eXosip_event_free(pEvent);
    }

    return ;
}

int32_t CDeviceStackGB28181::handleMessageAnswered(eXosip_event_t& rEvent)
{
    SVS_TRACE();

    do
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapTID2Info, SVS_ERROR_FAIL);
        MAP_TID_2_INFO::iterator iter = m_mapTID2Info.find(rEvent.tid);
        if (m_mapTID2Info.end() == iter)
        {
            break;
        }

        TRANSACTION_INFO& stTransactionInfo = iter->second;
        if (NULL == stTransactionInfo.pCallBack)
        {
            SVS_LOG((SVS_LM_INFO, "Response message callback is NULL, tid is %d.", rEvent.tid));
            break;
        }

        SVS_ACM::RESPONSE stResponse;
        stResponse.nRequestID = stTransactionInfo.nRequestID;
        stTransactionInfo.pCallBack(stResponse, stTransactionInfo.pUserData);
        SVS_LOG((SVS_LM_INFO, "Response message callback, request id is %d, tid is %d.",
            stTransactionInfo.nRequestID, rEvent.tid));
    }while(0);

    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::handleMessageAnsweredError(eXosip_event_t& rEvent)
{
    SVS_TRACE();

    do
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapTID2Info, SVS_ERROR_FAIL);
        MAP_TID_2_INFO::iterator iter = m_mapTID2Info.find(rEvent.tid);
        if (m_mapTID2Info.end() == iter)
        {
            break;
        }

        TRANSACTION_INFO& stTransactionInfo = iter->second;
        if (NULL == stTransactionInfo.pCallBack)
        {
            SVS_LOG((SVS_LM_INFO, "Response message callback is NULL, tid is %d.", rEvent.tid));
            break;
        }

        SVS_ACM::RESPONSE stResponse;
        stResponse.nRequestID = stTransactionInfo.nRequestID;
        stResponse.nResponseCode = SVS_ERROR_FAIL;
        stTransactionInfo.pCallBack(stResponse, stTransactionInfo.pUserData);
        SVS_LOG((SVS_LM_INFO, "Response message callback, request id is %d, tid is %d, result is error.",
            stTransactionInfo.nRequestID, rEvent.tid));
    }while(0);

    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::handleCallAnswered(eXosip_event_t& rEvent)
{
    SVS_TRACE();

    osip_body_t *pBody = NULL;
    osip_message_get_body(rEvent.response, 0, &pBody);
    if (NULL == pBody)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse call answered body failed, call id is %d.", rEvent.cid));
        (void)response(rEvent, SVS_ERROR_FAIL);
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_INFO, "Parse call answered body success, call id is %d, sdp is %s.", rEvent.cid, pBody->body));

    do
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapInviteInfo, SVS_ERROR_FAIL);
        MAP_CALL_ID_2_INVITE_INFO::iterator iter = m_mapCallID2InviteInfo.find(rEvent.cid);
        if (m_mapCallID2InviteInfo.end() == iter)
        {
            break;
        }

        INVITE_INFO& stInviteInfo = iter->second;
        stInviteInfo.nDialogID  = rEvent.did;
        if (NULL == stInviteInfo.pCallBack)
        {
            SVS_LOG((SVS_LM_INFO, "Response invite callback is NULL, lens is %s.", stInviteInfo.szLensID));
            break;
        }

        SVS_ACM::RESPONSE_SEND_INVITE2DEV stResponse;
        stResponse.nRequestID = stInviteInfo.nRequestID;
        stResponse.SdpLen = strlen(pBody->body);
        if (stResponse.SdpLen >= sizeof(stResponse.szSdp))
        {
            SVS_LOG((SVS_LM_ERROR, "SDP buffer length is too small. Request id is %d, call id is %d, "
                "buffer length is %d, sdp length is %d.", stInviteInfo.nRequestID, rEvent.cid,
                sizeof(stResponse.szSdp) - 1, stResponse.SdpLen));
            (void)response(rEvent, SIP_BAD_REQUEST);
            return SVS_ERROR_FAIL;
        }
        memcpy(stResponse.szSdp, pBody->body, stResponse.SdpLen);
        strncpy(stResponse.szLensID, stInviteInfo.szLensID, sizeof(stResponse.szLensID) - 1);
        stInviteInfo.pCallBack(stResponse, stInviteInfo.pUserData);
        SVS_LOG((SVS_LM_INFO, "Response invite callback, request id is %d, lens is %s.",
            stInviteInfo.nRequestID, stInviteInfo.szLensID));
    }while(0);

    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::handleCallAnsweredError(eXosip_event_t& rEvent)
{
    SVS_TRACE();

    do
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapInviteInfo, SVS_ERROR_FAIL);
        MAP_CALL_ID_2_INVITE_INFO::iterator iter = m_mapCallID2InviteInfo.find(rEvent.cid);
        if (m_mapCallID2InviteInfo.end() == iter)
        {
            break;
        }

        INVITE_INFO& stInviteInfo = iter->second;
        stInviteInfo.nDialogID  = rEvent.did;
        if (NULL == stInviteInfo.pCallBack)
        {
            SVS_LOG((SVS_LM_INFO, "Response invite or bye callback is NULL, lens is %s.", stInviteInfo.szLensID));
            break;
        }

        SVS_ACM::RESPONSE stResponse;
        stResponse.nRequestID       = stInviteInfo.nRequestID;
        stResponse.nResponseCode    = SVS_ERROR_FAIL;
        stInviteInfo.pCallBack(stResponse, stInviteInfo.pUserData);
        SVS_LOG((SVS_LM_INFO, "Response invite or bye callback, request id is %d, lens is %s, result is error.",
            stInviteInfo.nRequestID, stInviteInfo.szLensID));
    }while(0);

    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::handleCallMessageAnswered(eXosip_event_t& rEvent)
{
    SVS_TRACE();

    do
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapInviteInfo, SVS_ERROR_FAIL);
        MAP_CALL_ID_2_INVITE_INFO::iterator iter = m_mapCallID2InviteInfo.find(rEvent.cid);
        if (m_mapCallID2InviteInfo.end() == iter)
        {
            break;
        }

        INVITE_INFO& stInviteInfo = iter->second;
        stInviteInfo.nDialogID  = rEvent.did;
        if (NULL == stInviteInfo.pCallBack)
        {
            SVS_LOG((SVS_LM_INFO, "Response bye callback is NULL, lens is %s.", stInviteInfo.szLensID));
            break;
        }

        SVS_ACM::RESPONSE_SEND_BYE2DEV stResponse;
        stResponse.nRequestID = stInviteInfo.nRequestID;
        strncpy(stResponse.szLensID, stInviteInfo.szLensID, sizeof(stResponse.szLensID) - 1);
        stInviteInfo.pCallBack(stResponse, stInviteInfo.pUserData);
        SVS_LOG((SVS_LM_INFO, "Response bye callback, request id is %d, lens is %s.",
            stInviteInfo.nRequestID, stInviteInfo.szLensID));
    }while(0);

    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::sendMessageReq(std::string& strTo, std::string& strBody, int32_t& nTransactionID)
{
    const char* pszSipMethod = "MESSAGE";
    osip_message_t *pRequest = NULL;

    CExOsipLockGuard objExOsipLockGuard(m_pEXosipCtx);

    int32_t nResult = eXosip_message_build_request(m_pEXosipCtx, &pRequest, pszSipMethod, strTo.c_str(), m_strFrom.c_str(), NULL);
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "eXosip_message_build_request %s failed, error code is %d.", pszSipMethod, nResult));
        return SVS_ERROR_FAIL;
    }

    nResult = osip_message_set_body(pRequest, strBody.c_str(), strBody.length());
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "osip_message_set_body %s failed, error code is %d.", pszSipMethod, nResult));
        osip_message_free(pRequest);
        return SVS_ERROR_FAIL;
    }

    nResult = osip_message_set_content_type (pRequest, "Application/MANSCDP+xml");
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "osip_message_set_body %s failed, error code is %d.", pszSipMethod, nResult));
        osip_message_free(pRequest);
        return SVS_ERROR_FAIL;
    }

    nResult = eXosip_message_send_request(m_pEXosipCtx, pRequest);
    if (OSIP_SUCCESS >= nResult)    //大于0表示成功
    {
        SVS_LOG((SVS_LM_ERROR, "eXosip_message_send_request %s failed, error code is %d.", pszSipMethod, nResult));
        return SVS_ERROR_FAIL;
    }

    nTransactionID = nResult;

    SVS_LOG((SVS_LM_INFO, "eXosip_message_send_request %s success, tid is %d.", pszSipMethod, nResult));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::sendSubscribeReq(std::string& strTo, std::string& strBody)
{
    SVS_TRACE();

    const char* pszSipMethod = "SUBSCRIBE";
    osip_message_t *pRequest = NULL;

    CExOsipLockGuard objExOsipLockGuard(m_pEXosipCtx);

    int32_t nResult = eXosip_message_build_request(m_pEXosipCtx, &pRequest, pszSipMethod, strTo.c_str(), m_strFrom.c_str(), NULL);
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "eXosip_message_build_request %s failed, error code is %d.", pszSipMethod, nResult));
        return SVS_ERROR_FAIL;
    }

    nResult = osip_message_set_body(pRequest, strBody.c_str(), strBody.length());
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "osip_message_set_body %s failed, error code is %d.", pszSipMethod, nResult));
        osip_message_free(pRequest);
        return SVS_ERROR_FAIL;
    }

    nResult = osip_message_set_content_type (pRequest, "Application/MANSCDP+xml");
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "osip_message_set_body %s failed, error code is %d.", pszSipMethod, nResult));
        osip_message_free(pRequest);
        return SVS_ERROR_FAIL;
    }

    nResult = osip_message_set_expires (pRequest, "3600");
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "osip_message_set_expires %s failed, error code is %d.", pszSipMethod, nResult));
        osip_message_free(pRequest);
        return SVS_ERROR_FAIL;
    }

    nResult = osip_message_set_header (pRequest, "Event", "presence");
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "osip_message_set_header %s event presence failed, error code is %d.", pszSipMethod, nResult));
        osip_message_free(pRequest);
        return SVS_ERROR_FAIL;
    }

    nResult = eXosip_message_send_request(m_pEXosipCtx, pRequest);
    if (OSIP_SUCCESS >= nResult)    //大于0表示成功
    {
        SVS_LOG((SVS_LM_ERROR, "eXosip_message_send_request %s failed, error code is %d.", pszSipMethod, nResult));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_INFO, "eXosip_message_send_request %s success, tid is %d.", pszSipMethod, nResult));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::sendInviteReq(std::string& strTo, std::string& strBody, std::string& strSubject, int32_t& nCallID)
{
    SVS_TRACE();

    CExOsipLockGuard objExOsipLockGuard(m_pEXosipCtx);

    osip_message_t *pRequest = NULL;
    int32_t nResult = eXosip_call_build_initial_invite(m_pEXosipCtx, &pRequest, strTo.c_str(), m_strFrom.c_str(), NULL, strSubject.c_str());
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "eXosip_call_build_initial_invite failed, error code is %d.", nResult));
        return SVS_ERROR_FAIL;
    }

    nResult = osip_message_set_body(pRequest, strBody.c_str(), strBody.length());
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "osip_message_set_body INVITE failed, error code is %d.", nResult));
        osip_message_free(pRequest);
        return SVS_ERROR_FAIL;
    }
    nResult = osip_message_set_content_type (pRequest, "Application/SDP");
    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "osip_message_set_body INVITE failed, error code is %d.", nResult));
        osip_message_free(pRequest);
        return SVS_ERROR_FAIL;
    }

    nResult = eXosip_call_send_initial_invite(m_pEXosipCtx, pRequest);
    if (OSIP_SUCCESS >= nResult)    //大于0表示成功
    {
        SVS_LOG((SVS_LM_ERROR, "eXosip_call_send_initial_invite failed, error code is %d.", nResult));
        return SVS_ERROR_FAIL;
    }

    nCallID = nResult;

    SVS_LOG((SVS_LM_INFO, "eXosip_call_send_initial_invite success, call id is %d.", nCallID));
    return SVS_ERROR_OK;
}

void CDeviceStackGB28181::sendQueryCatalogReq()
{
    SVS_TRACE();

    std::string strBody;

    //如有必要，可以先提取设备信息，然后在锁外构造并发送消息，以减少锁的范围
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex);
    MAP_USER::iterator iter = m_mapUser.begin();
    for (; m_mapUser.end() != iter; ++iter)
    {
        USER_INFO& stUserInfo = iter->second;
        CManscdp objManscdp;
        int32_t nResult = objManscdp.createQueryCatalog(stUserInfo.szUserName);
        if (0 != nResult)
        {
            SVS_LOG((SVS_LM_ERROR, "Create query catalog xml for '%s' failed.", stUserInfo.szUserName));
            continue;
        }
        strBody = objManscdp.getXML();

        int32_t nTransactionID = 0;
        nResult = sendMessageReq(stUserInfo.strTo, strBody, nTransactionID);
        if (0 != nResult)
        {
            SVS_LOG((SVS_LM_ERROR, "Send query catalog request for '%s' failed.", stUserInfo.szUserName));
            continue;
        }
    }
}

void CDeviceStackGB28181::checkExpires()
{
    time_t nCurrentTime;
    (void)time(&nCurrentTime);

    std::vector<const char*> vecUser;
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex);
    MAP_USER::iterator iter = m_mapUser.begin();
    for (; m_mapUser.end() != iter; ++iter)
    {
        USER_INFO& stUserInfo = iter->second;
        if (nCurrentTime - stUserInfo.tLastRegisterTime > stUserInfo.nExpires)
        {
            SVS_LOG((SVS_LM_WARNING, "The user '%s' expires.", stUserInfo.szUserName));
            notifyDeviceOffline(stUserInfo.szUserName);
            vecUser.push_back(stUserInfo.szUserName);
            continue;
        }

        if (nCurrentTime - stUserInfo.tLastKeepAliveTime > CAccessControlSvr::instance().get_device_keep_alive_time_out())
        {
            SVS_LOG((SVS_LM_WARNING, "The user '%s' keep alive time out.", stUserInfo.szUserName));
            notifyDeviceOffline(stUserInfo.szUserName);
            vecUser.push_back(stUserInfo.szUserName);
            continue;
        }
    }

    std::vector<const char*>::iterator iterUser = vecUser.begin();
    for (; vecUser.end() != iterUser; ++iterUser)
    {
        const char* pszUserName = *iterUser;
        SVS_LOG((SVS_LM_WARNING, "The user '%s' erase.", pszUserName));
        m_mapUser.erase(pszUserName);
    }
}

int32_t CDeviceStackGB28181::convertDeviceID2SipToHeader(const char* pszDeviceID, std::string& strTo)
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
    MAP_USER::iterator iter = m_mapUser.find(pszDeviceID);
    if (m_mapUser.end() == iter)
    {
        SVS_LOG((SVS_LM_ERROR, "Convert device id to SIP to header failed, user '%s' not found.", pszDeviceID));
        return SVS_ERROR_FAIL;
    }
    USER_INFO& rUser = iter->second;
    strTo = rUser.strTo;
    return SVS_ERROR_OK;
}
int32_t CDeviceStackGB28181::convertLensID2SipToHeader(const char* pszDeviceID,const char* pszLensID, std::string& strTo)
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
    MAP_USER::iterator iter = m_mapUser.find(pszDeviceID);
    if (m_mapUser.end() == iter)
    {
        SVS_LOG((SVS_LM_ERROR, "Convert device id to SIP to header failed, user '%s' not found.", pszDeviceID));
        return SVS_ERROR_FAIL;
    }
    USER_INFO& rUser = iter->second;
    std::string strLens = pszLensID;
    std::string strHost = &rUser.szHost[0];
    std::string strPort = &rUser.szPort[0];
    strTo = "sip:" + strLens + "@" + strHost + ":" + strPort;
    return SVS_ERROR_OK;
}


int32_t CDeviceStackGB28181::sendInviteReq(SVS_ACM::REQUEST_SEND_INVITE2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "Send invite request, lens is %s, mu server id is %s.", rRequest.szLensID, rRequest.szMuSvrID));

    std::string strTo;
    //int32_t nResult = convertDeviceID2SipToHeader(rRequest.szDeviceID, strTo);
    int32_t nResult = convertLensID2SipToHeader(rRequest.szDeviceID,rRequest.szLensID, strTo);
    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Convert device id to SIP to header failed, device id is '%s'.", rRequest.szDeviceID));
        return nResult;
    }

    std::string strBody((const char*)rRequest.szSdp, (size_t)rRequest.SdpLen);
    std::string strSubject(rRequest.szLensID);
    //std::string strSubject(rRequest.szDeviceID);
    strSubject = strSubject + ":0," + rRequest.szMuSvrID + ":0";

    int32_t nCallID = 0;
    nResult = sendInviteReq(strTo, strBody, strSubject, nCallID);

    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Send invite request failed."));
        return SVS_ERROR_FAIL;
    }

    INVITE_INFO stInviteInfo;
    strncpy(stInviteInfo.szLensID, rRequest.szLensID, sizeof(stInviteInfo.szLensID) - 1);
    stInviteInfo.nRequestID = rRequest.nRequestID;
    stInviteInfo.pCallBack  = pCallBack;
    stInviteInfo.pUserData  = pUserData;
    stInviteInfo.nCallID    = nCallID;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapInviteInfo, SVS_ERROR_FAIL);
        m_mapCallID2InviteInfo[nCallID] = stInviteInfo;
        m_mapLensID2InviteInfo[stInviteInfo.szLensID] = &m_mapCallID2InviteInfo[nCallID];
    }

    SVS_LOG((SVS_LM_INFO, "Send invite request success, request id is %d, lens is %s, call id is %d.",
        stInviteInfo.nRequestID, stInviteInfo.szLensID, stInviteInfo.nCallID));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::notifyAck(SVS_ACM::REQUEST_SEND_ACK2DEV& rAck)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapInviteInfo, SVS_ERROR_FAIL);
    MAP_LENS_ID_2_INVITE_INFO::iterator iter = m_mapLensID2InviteInfo.find(rAck.szLensID);
    if (m_mapLensID2InviteInfo.end() == iter)
    {
        SVS_LOG((SVS_LM_ERROR, "Send ack failed, lens '%s' not found.", rAck.szLensID));
        return SVS_ERROR_FAIL;
    }

    INVITE_INFO& stInviteInfo   = *iter->second;
    stInviteInfo.nRequestID     = rAck.nRequestID;
    int32_t nResult = OSIP_SUCCESS;
    {
        CExOsipLockGuard objExOsipLockGuard(m_pEXosipCtx);
        nResult = eXosip_call_send_ack(m_pEXosipCtx, stInviteInfo.nDialogID, NULL);
    }

    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Send ack failed, lens is '%s', error code is %d.",
            stInviteInfo.szLensID, nResult));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_INFO, "Send ack success, request id is %d, lens is %s, call id is %d, dialog id is %d.",
        stInviteInfo.nRequestID, stInviteInfo.szLensID, stInviteInfo.nCallID, stInviteInfo.nDialogID));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::sendByeReq(SVS_ACM::REQUEST_SEND_BYE2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapInviteInfo, SVS_ERROR_FAIL);
    MAP_LENS_ID_2_INVITE_INFO::iterator iter = m_mapLensID2InviteInfo.find(rRequest.szLensID);
    if (m_mapLensID2InviteInfo.end() == iter)
    {
        SVS_LOG((SVS_LM_ERROR, "Send bye request failed, lens '%s' not found.", rRequest.szLensID));
        return SVS_ERROR_FAIL;
    }

    INVITE_INFO& stInviteInfo = *iter->second;
    //先更新异步回调信息
    stInviteInfo.nRequestID = rRequest.nRequestID;
    stInviteInfo.pCallBack  = pCallBack;
    stInviteInfo.pUserData  = pUserData;

    //再发送BYE异步请求
    int32_t nResult = OSIP_SUCCESS;
    {
        CExOsipLockGuard objExOsipLockGuard(m_pEXosipCtx);
        nResult = eXosip_call_terminate(m_pEXosipCtx, stInviteInfo.nCallID, stInviteInfo.nDialogID);
    }

    if (OSIP_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Send bye request failed, lens is '%s', error code is %d.",
            stInviteInfo.szLensID, nResult));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_INFO, "Send bye request success, request id is %d, lens is %s, call id is %d, dialog id is %d.",
        stInviteInfo.nRequestID, stInviteInfo.szLensID, stInviteInfo.nCallID, stInviteInfo.nDialogID));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::sendKeyFrameReq(SVS_ACM::REQUEST_SEND_KEYFRAME2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "Send key frame request, lens is %s.", rRequest.szLensID));

    std::string strTo;
    //int32_t nResult = convertDeviceID2SipToHeader(rRequest.szDeviceID, strTo);
    int32_t nResult = convertLensID2SipToHeader(rRequest.szDeviceID, rRequest.szLensID, strTo);
    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Convert lens id to SIP to header failed, lens id is '%s'.", rRequest.szLensID));
        return nResult;
    }

    CManscdp objManscdp;
    nResult = objManscdp.createKeyFrame();
    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Create key frame xml for '%s' failed.", rRequest.szLensID));
        return nResult;
    }

    std::string strBody = objManscdp.getXML();
    int32_t nTransactionID = 0;
    nResult = sendMessageReq(strTo, strBody, nTransactionID);
    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Send key frame request for '%s' failed.", rRequest.szLensID));
        return nResult;
    }

    TRANSACTION_INFO stTransactionInfo;
    stTransactionInfo.nRequestID        = rRequest.nRequestID;
    stTransactionInfo.pCallBack         = pCallBack;
    stTransactionInfo.pUserData         = pUserData;
    stTransactionInfo.nTransactionID    = nTransactionID;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapTID2Info, SVS_ERROR_FAIL);
        m_mapTID2Info[nTransactionID] = stTransactionInfo;
    }

    SVS_LOG((SVS_LM_INFO, "Send key frame request success, request id is %d, lens is %s, tid is %d.",
        stTransactionInfo.nRequestID, rRequest.szLensID, nTransactionID));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::sendDevCtrlReq(SVS_ACM::REQUEST_DEV_CTRL& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "Send device control request, lens is %s.", rRequest.szLensID));

    std::string strTo;
    int32_t nResult = convertDeviceID2SipToHeader(rRequest.szDeviceID, strTo);
    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Convert device id to SIP to header failed, device id is '%s'.", rRequest.szDeviceID));
        return nResult;
    }

    CManscdpDevCtrl objManscdp(rRequest);
    nResult = objManscdp.createDeviceControl();
    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Create device control xml for '%s' failed.", rRequest.szLensID));
        return nResult;
    }

    std::string strBody = objManscdp.getXML();
    int32_t nTransactionID = 0;
    nResult = sendMessageReq(strTo, strBody, nTransactionID);
    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Send device control request for '%s' failed.", rRequest.szLensID));
        return nResult;
    }

    TRANSACTION_INFO stTransactionInfo;
    stTransactionInfo.nRequestID        = rRequest.nRequestID;
    stTransactionInfo.pCallBack         = pCallBack;
    stTransactionInfo.pUserData         = pUserData;
    stTransactionInfo.nTransactionID    = nTransactionID;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexMapTID2Info, SVS_ERROR_FAIL);
        m_mapTID2Info[nTransactionID] = stTransactionInfo;
    }

    SVS_LOG((SVS_LM_INFO, "Send device control request success, request id is %d, lens is %s, tid is %d.",
        stTransactionInfo.nRequestID, rRequest.szLensID, nTransactionID));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackGB28181::asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_TRACE();

    int32_t nResult = SVS_ERROR_OK;

    switch (rRequest.getType())
    {
    case SVS_ACM::REQ_TYPE_SEND_INVITE2DEV:
        {
            nResult = sendInviteReq((SVS_ACM::REQUEST_SEND_INVITE2DEV&)rRequest, pCallBack, pUserData);
            break;
        }
    case SVS_ACM::REQ_TYPE_SEND_BYE2DEV:
        {
            nResult = sendByeReq((SVS_ACM::REQUEST_SEND_BYE2DEV&)rRequest, pCallBack, pUserData);
            break;
        }
    case SVS_ACM::REQ_TYPE_SEND_ACK2DEV:
        {
            nResult = notifyAck((SVS_ACM::REQUEST_SEND_ACK2DEV&)rRequest);
            break;
        }
    case SVS_ACM::REQ_TYPE_SEND_KEYFRAME2DEV:
        {
            nResult = sendKeyFrameReq((SVS_ACM::REQUEST_SEND_KEYFRAME2DEV&)rRequest, pCallBack, pUserData);
            break;
        }
    case SVS_ACM::REQ_TYPE_DEV_CTRL:
        {
            nResult = sendDevCtrlReq((SVS_ACM::REQUEST_DEV_CTRL&)rRequest, pCallBack, pUserData);
            break;
        }
    default:
        SVS_LOG((SVS_LM_ERROR, "Async request type %d is invalid.", rRequest.getType()));
        return SVS_ERROR_FAIL;
        //break;
    }
    return nResult;
}
