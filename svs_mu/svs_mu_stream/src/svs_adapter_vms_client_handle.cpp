#include "svs_log_msg.h"
#include "svs_adapter_vms_client_handle.h"
#include <vms/vms.h>
#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_vms_loger.h"
#include "svs_adapter_svs_def.h"
#include "svs_adapter_time.h"
#include "svs_daemon_thread.h"
#include "svs_adapter_vms_auth.h"

CSvsClientHandle::CSvsClientHandle()
{
    m_bExitFlag        = false;
    m_bFirstConnect    = true;

    m_enStatus         = HANDLE_STATUS_DISCONNECT;
    m_ulStatusTime     = 0;
    m_bHandleFailMsg   = true;

    m_pClientLink      = NULL;
    m_unLinkIndex      = 0;

    m_unServerType     = 0;
    m_unResetFlags     = RESET_FLAGS_RESET;
    m_unServiceNetType = SERVICE_NET_TYPE_FIX;

    memset(m_szServerID, 0x0, SERVER_ID_LEN + 1);
    memset(m_szName, 0x0, SERVER_NAME_LEN + 1);

    memset(m_szZoneID, 0x0, SERVER_ID_LEN + 1);

    m_LocalPortList.clear();

    memset(m_szLocalIp, 0x0, SERVER_ID_LEN);
    memset(m_szServerIp, 0x0, SERVER_ID_LEN);
    m_ulServerLastRunTime = 0;
    m_bReportTaskFlag = false;

    m_nRegisterPasswdLen = 0;
    memset(m_szRegisterPasswd, 0x00, sizeof(m_szRegisterPasswd));
}


CSvsClientHandle::~CSvsClientHandle()
{
    try
    {
        close();
    }
    catch(...)
    {
    }

    m_pClientLink      = NULL;
}


int32_t CSvsClientHandle::init(const char *szServiceID,
                           uint32_t unServerType,
                           const char *szServerName,
                           uint32_t unServiceNetType,
                           uint32_t unHearbeatInterval,
                           const char *szRegisterPasswd)
{
    SVS_LOG((SVS_LM_INFO,"init svs client handle: ServiceID[%s] ServerType[%d] ServerName[%s]"
                    " ServiceNetType[%d].",
                    szServiceID, unServerType, szServerName,
                    unServiceNetType));

    if ((NULL == szServiceID) || (NULL == szServerName))
    {
        SVS_LOG((SVS_LM_WARNING,"init svs client handle fail, invalid param"));

        return RET_ERR_PARAM;
    }

    if ((SERVER_TYPE_MU_STREAM != unServerType)
            && (SERVER_TYPE_MU_RECORD != unServerType)
            && (SERVER_TYPE_MU_STREAM != unServerType))
    {
        SVS_LOG((SVS_LM_WARNING,"init svs client handle fail, invalid param: ServerType[%d].",
                        unServerType));
        return RET_ERR_PARAM;
    }

    if ((SERVICE_NET_TYPE_FIX != unServiceNetType)
            && (SERVICE_NET_TYPE_MOTION != unServiceNetType)
            && (SERVICE_NET_TYPE_BOTH != unServiceNetType))
    {
        SVS_LOG((SVS_LM_WARNING,"init svs client handle fail, invalid param: ServerNetType[%d].",
                        unServiceNetType));
        return RET_ERR_PARAM;
    }

    m_unServerType     = unServerType;
    m_unResetFlags     = RESET_FLAGS_RESET;
    m_unServiceNetType = unServiceNetType;

    memcpy(m_szServerID, szServiceID, SERVER_ID_LEN);
    memcpy(m_szName, szServerName, SERVER_NAME_LEN);

    m_nRegisterPasswdLen = strlen(szRegisterPasswd);
    m_nRegisterPasswdLen=(m_nRegisterPasswdLen > AUTH_PASSWD_LEN)?AUTH_PASSWD_LEN : m_nRegisterPasswdLen;
    memcpy(m_szRegisterPasswd, szRegisterPasswd, AUTH_PASSWD_LEN);
    m_szRegisterPasswd[m_nRegisterPasswdLen] = '\0';

    int32_t nRet = CLinkFactory::getInstance()->init(unHearbeatInterval, CSvsLoger::instance());
    if (SVS_STACK_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"init svs stack fail."));
        return RET_FAIL;
    }

    nRet = CLinkFactory::getInstance()->run();
    if (SVS_STACK_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"run svs stack fail"));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"init svs client handle success."));
    return RET_OK;
}

void CSvsClientHandle::setLastRunTime(uint32_t ulRunTime)
{
    m_ulServerLastRunTime = ulRunTime;
}


void CSvsClientHandle::setServerAddr(uint32_t unServerIp, uint16_t usServerPort)
{
    m_ServerAddr.set(usServerPort, unServerIp);
    m_ServerAddr.get_host_addr(m_szServerIp, SERVER_ID_LEN);
    SVS_LOG((SVS_LM_INFO,"set svs client handle's remote addr[%s:%d].",
                    m_szServerIp,
                    m_ServerAddr.get_port_number()));
}

void CSvsClientHandle::getServerAddr(uint32_t &unServerIp,
                                     uint16_t &usServerPort) const
{
    unServerIp   = m_ServerAddr.get_ip_address();
    usServerPort = m_ServerAddr.get_port_number();

    return;
}

void CSvsClientHandle::setLocalAddr(uint32_t unLocalIp, const LOCAL_PORT_LIST &localPortList)
{
    m_LocalAddr.set((uint16_t)0, unLocalIp);
    m_LocalPortList  = localPortList;

    m_LocalAddr.get_host_addr(m_szLocalIp, SERVER_ID_LEN);
    return;
}


void CSvsClientHandle::getLocalAddr(uint32_t &unLocalIp,
                                        uint16_t &usLocalPort) const
{
    unLocalIp   = m_LocalAddr.get_ip_address();
    usLocalPort = m_LocalAddr.get_port_number();

    return;
}


const char* CSvsClientHandle::getServerID() const
{
    return m_szServerID;
}

const char* CSvsClientHandle::getZoneID() const
{
    return m_szZoneID;
}


int32_t CSvsClientHandle::open(void * /*argv*/)
{
    if ((0 == m_ServerAddr.get_ip_address())
         || (0 == m_ServerAddr.get_port_number()))
    {
        return RET_ERR_PARAM;
    }

    m_bExitFlag    = false;

    m_enStatus       = HANDLE_STATUS_DISCONNECT;
    m_ulStatusTime   = SVS_GetSecondTime();

    size_t stackSize = DEFAULT_THREAD_STACK_SIZE;
    int32_t nRet = activate(THR_NEW_LWP  | THR_JOINABLE,
                        1,
                        0,
                        ACE_DEFAULT_THREAD_PRIORITY,
                        -1,
                        0,
                        0,
                        0,
                        &stackSize,
                        0);
    if (-1 == nRet)
    {
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"open svs client handle success."));
    return RET_OK;
}


int32_t CSvsClientHandle::svc()
{
    CThread_Stat_Reporter report("SvsClientHandleThread");
    while (!m_bExitFlag)
    {
        switch(m_enStatus)
        {
        case HANDLE_STATUS_DISCONNECT:
            startConnect();
            break;

        case HANDLE_STATUS_CONNECTING:
            checkConnect();
            break;

        case HANDLE_STATUS_CONNECTED:
            registServer();
            break;

        case HANDLE_STATUS_REGISTING:
            checkRegiste();
            break;

        default:
            break;
        }

        usleep(STATUS_CHECK_INTERVAL);
        report.ReportStat();
    }

    SVS_LOG((SVS_LM_INFO,"exit client handle thread."));
    return RET_OK;
}


void CSvsClientHandle::close()
{
    m_bExitFlag = true;
    (void)wait();

    if (NULL == m_pClientLink)
    {
        return;
    }

    int32_t nRet = m_pClientLink->stopConn();
    if (SVS_STACK_OK != nRet)
    {
        return;
    }

    nRet = CLinkFactory::getInstance()->destroyLink(m_pClientLink);
    if (SVS_STACK_OK != nRet)
    {
        return;
    }

    m_enStatus    = HANDLE_STATUS_DISCONNECT;

    SVS_LOG((SVS_LM_INFO,"close client handle success."));
    return;
}


HANDLE_STATUS CSvsClientHandle::getStatus() const
{
    return m_enStatus;
}


bool CSvsClientHandle::reportTaskOK()const
{
    return m_bReportTaskFlag;
}


void CSvsClientHandle::handleMessage(const char* pszMsg,
                                     const uint32_t unMsgLength,
                                     const uint32_t unHandleIndex,
                                     Result &stResult)
{
    if ((NULL == pszMsg) || (0 == unMsgLength))
    {
        SVS_LOG((SVS_LM_WARNING,"client handle[%d] handle failed message finished",
                    m_unLinkIndex));

        m_bHandleFailMsg = true;
        return;
    }

    SVS_MSG_HEADER *pHeader = (SVS_MSG_HEADER*)(void*)pszMsg;
    if (unMsgLength != pHeader->PacketLength)
    {
        SVS_LOG((SVS_LM_WARNING,"client handle[%d] recved invalid length[%d] message, "
                        "MsgType[0x%x] MsgLength[%d] transno[0x%x]",
                        m_unLinkIndex,
                        unMsgLength,
                        pHeader->MsgType,
                        pHeader->PacketLength,
                        pHeader->TransactionNo));
        return;
    }

    if ((HANDLE_STATUS_REGISTED != m_enStatus)
        && (SVS_MSG_TYPE_SERVER_REGISTER_RESP != pHeader->MsgType))
    {
        SVS_LOG((SVS_LM_WARNING,"client handle[%d] recved message when UNREGISTED, MsgType[0x%x] "
                        "MsgLength[%d] transno[0x%x]",
                    m_unLinkIndex,
                    pHeader->MsgType,
                    pHeader->PacketLength,
                    pHeader->TransactionNo));

        return;
    }

    if (SVS_MSG_TYPE_SERVER_REGISTER_RESP == pHeader->MsgType)
    {
        handleRegistResp(pszMsg, unMsgLength);
        return;
    }

    if(RESULT_TYPE_TIMEOUT == stResult.ucResultType)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Client handle[%d] wait response message timeout,"
            " MsgType[0x%x] MsgLength[%d], transno[0x%x]",
            m_unLinkIndex, pHeader->MsgType,
            pHeader->PacketLength, pHeader->TransactionNo));

        return;
    }

    ACE_Message_Block *pMsg = NULL;
    int32_t nRet = createMsgBlock(unMsgLength, pMsg);
    if (RET_OK != nRet)
    {
        return;
    }
    pMsg->copy(pszMsg, unMsgLength);

    if (RESULT_TYPE_RECV == stResult.ucResultType)
    {
        handleRecvMessage(pMsg);
        return;
    }

    if (RESULT_TYPE_SEND == stResult.ucResultType)
    {
        handleFailedMessage(pMsg);
        return;
    }

    pMsg->release();
    return;
}


void CSvsClientHandle::handleConnEvent(const ConnEvent* pstEvent)
{
    if (NULL == pstEvent)
    {
        return;
    }

    switch(pstEvent->EventType)
    {
    case EVENT_TYPE_DISCONNECT:
        m_enStatus       = HANDLE_STATUS_DISCONNECT;
        m_ulStatusTime   = SVS_GetSecondTime();
        m_bHandleFailMsg = false;

        SVS_LOG((SVS_LM_WARNING,"connect[%d] recved disconnect event[%d].",
                    pstEvent->unHandleIndex, pstEvent->EventType));
        break;

    case EVENT_TYPE_CONNECTED:
        m_unLinkIndex  = pstEvent->unHandleIndex;
        m_enStatus     = HANDLE_STATUS_CONNECTED;
        m_ulStatusTime = SVS_GetSecondTime();

        SVS_LOG((SVS_LM_INFO,"connect[%d] recved connect success event[%d].",
                    pstEvent->unHandleIndex, pstEvent->EventType));
        break;

    case EVENT_TYPE_TIMEOUT:
        m_enStatus       = HANDLE_STATUS_DISCONNECT;
        m_ulStatusTime   = SVS_GetSecondTime();
        m_bHandleFailMsg = false;

        SVS_LOG((SVS_LM_INFO,"connect[%d] recved connect timeout event[%d].",
                    pstEvent->unHandleIndex, pstEvent->EventType));

        break;

    default:
        break;
    }

    connectNotify(pstEvent);

    return;
}


void CSvsClientHandle::connectNotify(const ConnEvent* pstEvent)
{
    return;
}


void CSvsClientHandle::registerNotify()
{
    return;
}


int32_t CSvsClientHandle::sendMessage(char *pDatabuffer,
                                     uint32_t unLength,
                                     uint32_t unTimeout)
{
    if ((NULL == pDatabuffer) || (0 == unLength))
    {
        return RET_ERR_PARAM;
    }

    if (HANDLE_STATUS_REGISTED != m_enStatus)
    {
        return RET_ERR_UNREGISTED;
    }

    if (NULL == m_pClientLink)
    {
        return RET_FAIL;
    }

    int32_t nRet = m_pClientLink->sendMsg(m_unLinkIndex, pDatabuffer, unLength, unTimeout);
    if (SVS_STACK_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"client handle[%d] send message fail.", m_unLinkIndex));
        return RET_FAIL;
    }

    return RET_OK;
}


int32_t CSvsClientHandle::restartHandle()
{
    m_enStatus       = HANDLE_STATUS_DISCONNECT;
    m_bHandleFailMsg = true;

    return RET_OK;
}



void CSvsClientHandle::startConnect()
{
    if (HANDLE_STATUS_DISCONNECT != m_enStatus)
    {
        return;
    }


    if (m_bFirstConnect)
    {
        m_bFirstConnect    = false;
    }
    else
    {
        if ((MAX_RETRY_INTERVAL > SVS_GetSecondTime() - m_ulStatusTime) || (!m_bHandleFailMsg))
        {
            return;
        }
    }

    if (NULL != m_pClientLink)
    {
        (void)m_pClientLink->stopConn();
        (void)CLinkFactory::getInstance()->destroyLink(m_pClientLink);
    }


    ADDR localAddr;
    if (!m_LocalPortList.empty())
    {
        localAddr.unIp = m_LocalAddr.get_ip_address();
        if (0 == m_LocalAddr.get_port_number())
        {
            localAddr.usPort = m_LocalPortList.front();
        }
        else
        {
            LOCAL_PORT_LIST_ITER iter = find(m_LocalPortList.begin(),
                                             m_LocalPortList.end(),
                                             m_LocalAddr.get_port_number());
            iter++;
            if (m_LocalPortList.end() == iter)
            {
                localAddr.usPort = m_LocalPortList.front();
            }
            else
            {
                localAddr.usPort = *iter;
            }

        }
    }
    else
    {
        localAddr.unIp   = m_LocalAddr.get_ip_address();
        localAddr.usPort = 0;
    }


    m_LocalAddr.set(localAddr.usPort, localAddr.unIp);

    SVS_LOG((SVS_LM_INFO,"start connect to server[%s:%d], localAddr[%s:%d].",
                        m_szServerIp,
                        m_ServerAddr.get_port_number(),
                        m_szLocalIp,
                        m_LocalAddr.get_port_number()));
    ADDR serverAddr;
    serverAddr.unIp   = m_ServerAddr.get_ip_address();
    serverAddr.usPort = m_ServerAddr.get_port_number();
    int32_t nRet = CLinkFactory::getInstance()->createClientLink(&serverAddr, &localAddr,
               this, m_pClientLink);
    if (SVS_STACK_OK != nRet)
    {
        m_enStatus     = HANDLE_STATUS_DISCONNECT;
        m_ulStatusTime = SVS_GetSecondTime();

        SVS_LOG((SVS_LM_WARNING,"create client handle connection fail, localAddr[%s:%d] "
                "server[%s:%d].",
                m_szLocalIp,
                m_LocalAddr.get_port_number(),
                m_szServerIp,
                m_ServerAddr.get_port_number()));
        return;
    }

    m_enStatus     = HANDLE_STATUS_CONNECTING;
    m_ulStatusTime = SVS_GetSecondTime();

    nRet = m_pClientLink->startConn();
    if (SVS_STACK_OK != nRet)
    {
        m_enStatus     = HANDLE_STATUS_DISCONNECT;
        m_ulStatusTime = SVS_GetSecondTime();

        SVS_LOG((SVS_LM_WARNING,"start client handle connection fail, localAddr[%s:%d] "
                "server[%s:%d].",
                m_szLocalIp,
                m_LocalAddr.get_port_number(),
                m_szServerIp,
                m_ServerAddr.get_port_number()));
        return;
    }

    SVS_LOG((SVS_LM_INFO,"client handle[%d] start connect remote success, localAddr[%s:%d] "
            "server[%s:%d].",
            m_unLinkIndex,
            m_szLocalIp,
            m_LocalAddr.get_port_number(),
            m_szServerIp,
            m_ServerAddr.get_port_number()));
    return;
}


void CSvsClientHandle::checkConnect()
{
    if ((HANDLE_STATUS_CONNECTING != m_enStatus)
        || (MAX_RETRY_INTERVAL > SVS_GetSecondTime() - m_ulStatusTime))
    {
        return;
    }

    if (NULL != m_pClientLink)
    {
        m_pClientLink->stopConn();
        CLinkFactory::getInstance()->destroyLink(m_pClientLink);
    }

    m_enStatus     = HANDLE_STATUS_DISCONNECT;

    SVS_LOG((SVS_LM_WARNING,"check client handle[%d] connecting time out in[%d]s, localAddr[%s:%d] "
            "server[%s:%d].",
            m_unLinkIndex,
            MAX_RETRY_INTERVAL,
            m_szLocalIp,
            m_LocalAddr.get_port_number(),
            m_szServerIp,
            m_ServerAddr.get_port_number()));
    return;
}


void CSvsClientHandle::registServer()
{
    if (HANDLE_STATUS_CONNECTED != m_enStatus)
    {
        return;
    }

    if (NULL == m_pClientLink)
    {
        return;
    }

    SVS_LOG((SVS_LM_INFO,"start regist to server[%s:%d], localAddr[%s:%d].",
            m_szServerIp,
            m_ServerAddr.get_port_number(),
            m_szLocalIp,
            m_LocalAddr.get_port_number()));

    SVS_MSG_SERVER_REGIST_REQ regReq;
    memset(&regReq, 0x0, sizeof(SVS_MSG_SERVER_REGIST_REQ));
    regReq.MsgHeader.MsgType         = SVS_MSG_TYPE_SERVER_REGIST_REQ;
    regReq.MsgHeader.ProtocolVersion = SVS_MSG_PROTOCOL_VERSION;
    regReq.MsgHeader.TransactionNo   = 1;
    regReq.MsgHeader.PacketLength    = sizeof(SVS_MSG_SERVER_REGIST_REQ);

    regReq.ResetFlags                = m_unResetFlags;
    regReq.ServerType                = m_unServerType;
    regReq.ServiceNetType            = m_unServiceNetType;
    memcpy(regReq.Name, m_szName, SERVER_NAME_LEN);
    regReq.LastRunTime = m_ulServerLastRunTime;
    fillRegisterMd5Sum(&regReq,  m_szRegisterPasswd, m_nRegisterPasswdLen);

    int32_t nRet = m_pClientLink->sendMsg(m_unLinkIndex, (char*)&regReq,
                                      regReq.MsgHeader.PacketLength, 0);
    if (SVS_STACK_OK != nRet)
    {
        m_enStatus     = HANDLE_STATUS_DISCONNECT;
        m_ulStatusTime = SVS_GetSecondTime();

        SVS_LOG((SVS_LM_WARNING,"client handle[%d]  send regist msg fail, localAddr[%s:%d] "
                        "server[%s:%d].",
                        m_unLinkIndex,
                        m_szLocalIp,
                        m_LocalAddr.get_port_number(),
                        m_szServerIp,
                        m_ServerAddr.get_port_number()));
        return;
    }

    m_enStatus     = HANDLE_STATUS_REGISTING;
    m_ulStatusTime = SVS_GetSecondTime();
    SVS_LOG((SVS_LM_INFO,"client handle[%d] send regist message success, localAddr[%s:%d], "
                    "server[%s:%d].",
                    m_unLinkIndex,
                    m_szLocalIp,
                    m_LocalAddr.get_port_number(),
                    m_szServerIp,
                    m_ServerAddr.get_port_number()));
    return;
}


void CSvsClientHandle::checkRegiste()
{
    if ((HANDLE_STATUS_REGISTING != m_enStatus)
        || (MAX_RETRY_INTERVAL > SVS_GetSecondTime() - m_ulStatusTime))
    {
        return;
    }

    m_enStatus     = HANDLE_STATUS_DISCONNECT;

    SVS_LOG((SVS_LM_WARNING,"check client handle[%d] registing time out in[%d]s, localAddr[%s:%d] "
                    "server[%s:%d].",
                    m_unLinkIndex,
                    MAX_RETRY_INTERVAL,
                    m_szLocalIp,
                    m_LocalAddr.get_port_number(),
                    m_szServerIp,
                    m_ServerAddr.get_port_number()));
    return;
}


void CSvsClientHandle::handleRegistResp(const char *pszMsg, uint32_t unMsgLen)
{
    if ((NULL == pszMsg) || (sizeof(SVS_MSG_SERVER_REGISTER_RESP) != unMsgLen))
    {
        return;
    }

    if (HANDLE_STATUS_REGISTED == m_enStatus)
    {
        SVS_LOG((SVS_LM_INFO,"client handle[%d] recved repeate regist message. "
                        " localAddr[%s:%d] server[%s:%d]",
                        m_unLinkIndex,
                        m_szLocalIp,
                        m_LocalAddr.get_port_number(),
                        m_szServerIp,
                        m_ServerAddr.get_port_number()));
        return;
    }

    SVS_MSG_SERVER_REGISTER_RESP *pResp = (SVS_MSG_SERVER_REGISTER_RESP*)(void*)pszMsg;
    if (SVS_MSG_OK != pResp->RespCode)
    {
        m_enStatus     = HANDLE_STATUS_DISCONNECT;
        m_ulStatusTime = SVS_GetSecondTime();

        SVS_LOG((SVS_LM_ERROR,"client handle[%d] regist to server [%s:%d] fail, "
                        "respcode[0x%x] localAddr[%s:%d] ",
                        m_unLinkIndex,
                        m_szServerIp,
                        m_ServerAddr.get_port_number(),
                        pResp->RespCode,
                        m_szLocalIp,
                        m_LocalAddr.get_port_number()));

        return;
    }

    memcpy(m_szZoneID,pResp->ServerID,SERVER_ID_LEN);

    m_enStatus     = HANDLE_STATUS_REGISTED;
    m_ulStatusTime = SVS_GetSecondTime();

    m_unResetFlags = RESET_FLAGS_RUN;

    registerNotify();

    SVS_LOG((SVS_LM_INFO,"client handle[%d] regist to server [%s:%d] success, "
                    "localAddr[%s:%d] report flag[%d]",
                    m_unLinkIndex,
                    m_szServerIp,
                    m_ServerAddr.get_port_number(),
                    m_szLocalIp,
                    m_LocalAddr.get_port_number(),
                    pResp->ReportMediaTaskFlag));

    if (REPORT_MEDIA_TASK_REPORT != pResp->ReportMediaTaskFlag)
    {
        m_bReportTaskFlag = true;
        return;
    }

    handleTaskReport();
    m_bReportTaskFlag = true;
    return;
}

int32_t CSvsClientHandle::createMsgBlock(uint32_t unMsgLen,
                                     ACE_Message_Block *&pMsg) const
{
    try
    {
        pMsg = new ACE_Message_Block( unMsgLen );
    }
    catch (...)
    {
        return RET_ERR_SYS_NEW;
    }

    if( (NULL == pMsg->data_block() )
            || (NULL == pMsg->base()))
    {
        delete pMsg;
        pMsg = NULL;
        return RET_ERR_SYS_NEW;
    }

    return RET_OK;
}

