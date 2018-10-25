
#include "svs_adapter_svs_def.h"
#include "svs_print_version.h"
#include "svs_log_msg.h"
#include "svs_adapter_debugger.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_business_manager.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_service_task.h"

const uint8_t ctrlC[]     = {0XFF, 0XF4, 0XFF, 0XFD, 0X06};
const uint8_t formerCmd[] = {0X1B, 0X5B, 0X41};

const time_t g_ulStartTime = time(NULL);
int32_t CStreamDebugger::m_iClientNum = 0;

//==========================CStreamDebugger BEGIN================================
CStreamDebugger::CStreamDebugger()
{
    constructor();
}
CStreamDebugger::~CStreamDebugger()
{
}
void CStreamDebugger::constructor()
{
    (void)ACE_OS::memset( m_szInetAddr, 0, sizeof(m_szInetAddr));
    m_iTimerId = 0;
    m_iHeartbeatNum = 0;

    m_firstLevelMessage.insert( std::make_pair(SVS_CMD_SPACE, &CStreamDebugger::sendSpaceCmd));
    m_firstLevelMessage.insert( std::make_pair(FIRST_LEVEL_CMD_HELP, &CStreamDebugger::sendHelpInfo));
    m_firstLevelMessage.insert( std::make_pair(FIRST_LEVEL_CMD_SHOW, &CStreamDebugger::handleShowCmd));
    m_firstLevelMessage.insert( std::make_pair(FIRST_LEVEL_CMD_QUIT, &CStreamDebugger::exit));
    m_firstLevelMessage.insert( std::make_pair(FIRST_LEVEL_CMD_EXIT, &CStreamDebugger::exit));

    m_showMessage.insert(std::make_pair(SVS_CMD_SPACE, &CStreamDebugger::sendBasicInfo));
    m_showMessage.insert(std::make_pair(SECOND_LEVEL_CMD_DEBUG, &CStreamDebugger::sendDebugInfo));
    m_showMessage.insert(std::make_pair(SECOND_LEVEL_CMD_SESSION, &CStreamDebugger::handleShowSessionCmd));
    m_showMessage.insert(std::make_pair(SECOND_LEVEL_CMD_BUSINESS, &CStreamDebugger::handleShowBusinessCmd));
    m_showMessage.insert(std::make_pair(SECOND_LEVEL_CMD_BUFFER, &CStreamDebugger::sendBufferInfo));
    m_showMessage.insert(std::make_pair(SECOND_LEVEL_CMD_SDP, &CStreamDebugger::handleShowSdpCmd));

    m_sessionMessage.insert(std::make_pair(SVS_CMD_SPACE, &CStreamDebugger::sendAllSessionInfo));
    m_sessionMessage.insert(std::make_pair(THIRD_LEVEL_CMD_COUNT, &CStreamDebugger::sendSessionCnt));

    m_businessMessage.insert(std::make_pair(SVS_CMD_SPACE, &CStreamDebugger::sendAllBusinessInfo));
    m_businessMessage.insert(std::make_pair(THIRD_LEVEL_CMD_COUNT, &CStreamDebugger::sendBusinessCnt));

    return;
}

int32_t CStreamDebugger::open(void *)
{
    m_iClientNum++;
    if (m_iClientNum > STREAM_MAX_DEBUG_NUM)
    {
        SVS_LOG((SVS_LM_ERROR,"[DEBUG]Debug client reach limit[%d].", STREAM_MAX_DEBUG_NUM));
        sendLogFailMsg();
        return -1;
    }

    if(0 != reactor()->register_handler(this, ACE_Event_Handler::READ_MASK))
    {
        SVS_LOG((SVS_LM_ERROR,"[DEBUG] Register debug handler to reactor fail. [0x%08x].", get_handle()));
        return -1;
    }

    ACE_INET_Addr remoteaddr;
    (void)peer().get_remote_addr (remoteaddr);
    (void)ACE_OS::snprintf( m_szInetAddr,
                      sizeof(m_szInetAddr),
                      "%s:%d",
                      remoteaddr.get_host_addr(),
                      remoteaddr.get_port_number());

    SVS_LOG((SVS_LM_INFO,"[DEBUG] Recv a debug connection. form[%s], this[%p], handle[%d].",
               m_szInetAddr,
               this,
               get_handle()));

    (void)ACE_OS::memset(m_szLastRecvBuf, 0, sizeof(m_szLastRecvBuf));

    (void)sendNegotiate();


    ACE_Time_Value delay(DEBUG_HEARTBEAT_SEND_INTERVAL);
    m_iTimerId = reactor()->schedule_timer(this, 0, delay, delay);
    if (-1 == m_iTimerId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to allocate heartbeat timer. accept from [%s].",
            m_szInetAddr));
        return -1;
    }


    return 0;
}

int32_t CStreamDebugger::svc()
{
    return 0;
}
/*****************************************************************************
 �� �� ��  : handle_close
 ��������  : �ر�����
 �������  : ACE_HANDLE
             ACE_Reactor_Mask
 �������  : ��
 �� �� ֵ  : ����0
 �޸���ʷ      :
 1. ��    ��   : 2009��4��13��
    �޸�����   : ��������
*****************************************************************************/
int32_t CStreamDebugger::handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask /*mask*/)
{
    SVS_LOG((SVS_LM_INFO,
        "[DEBUG] Debug connection closed. client addr[%s], this[%p], handle[%d].",
        m_szInetAddr, this, get_handle()));

    m_iClientNum--;
    (void)reactor()->cancel_timer(this, 1);

    //���������Ϣ
    (void)reactor()->remove_handler(this, ACE_Event_Handler::ALL_EVENTS_MASK | ACE_Event_Handler::DONT_CALL);

    (void)ACE_OS::shutdown(peer().get_handle(), SHUT_RDWR);
    (void)peer().close();

    delete this;

    return 0;
}

int32_t CStreamDebugger::sendData()
{

    m_szSendBuf[ sizeof(m_szSendBuf) - 1 ] = '\0';

    ACE_Time_Value tv(SVS_LOG_SEND_TIMEOUT, 0);
    ssize_t len = (ssize_t)ACE_OS::strlen( m_szSendBuf );
    if (len != peer().send_n( m_szSendBuf, (uint32_t)len, &tv ))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEBUG] Send debug info to client fail. client[%s], debug info[%s],this[%p].",
            m_szInetAddr,
            m_szSendBuf,
            this));
        return -1;
    }

    return 0;
}

int32_t CStreamDebugger::handle_input(ACE_HANDLE /*handle*/)
{
    m_iHeartbeatNum++;
    ACE_Time_Value tv(SVS_LOG_RECV_TIMEOUT);

    int32_t recvResult = peer().recv( m_szRecvBuf, STREAM_DEBUG_CMD_LEN, &tv );

    if (0 >= recvResult)
    {
        SVS_LOG((SVS_LM_ERROR, "[DEBUG] Recv debug message fail. client[%s], this[%p].",
                   m_szInetAddr,
                   this));
        return -1;
    }

    int32_t i = 0;
    const int32_t ctrCLen = sizeof(ctrlC) / sizeof(ctrlC[0]);
    for (i = 0; i < ctrCLen; i++)
    {
        if (ctrlC[i] != static_cast<uint8_t>(m_szRecvBuf[i]))
        {
            break;
        }
    }
    if (ctrCLen == i)
    {
        return -1;
    }

    m_szRecvBuf[recvResult] = '\0';

    const int32_t formerCmdLen = sizeof(formerCmd) / sizeof(formerCmd[0]);
    for (i = 0; i < formerCmdLen; i++)
    {
        if (formerCmd[i] != static_cast<uint8_t>(m_szRecvBuf[i]))
        {
            break;
        }
    }
    if (formerCmdLen == i)
    {
        (void)ACE_OS::memcpy(m_szRecvBuf, m_szLastRecvBuf, sizeof(m_szRecvBuf));
    }

    SVS_LOG((SVS_LM_INFO,"Recv debug message.client[%s] message:%s",
        m_szInetAddr,
        m_szRecvBuf));
    //dump start
    uint8_t *pTmep=(uint8_t *)m_szRecvBuf;
    while(*pTmep != 0)
    {
        SVS_LOG((SVS_LM_INFO,"dump[0x%X,%d,%c]\n",*pTmep,*pTmep,*pTmep));
        pTmep++;
    }
    SVS_LOG((SVS_LM_INFO,"dump end\n"));
    //dump end
    string strCmd = m_szRecvBuf;

    string::size_type pos;
    while ((pos = strCmd.find( "\r\n" )) != string::npos)
    {
        (void)strCmd.replace( pos, 2, "" );
    }

    while ((pos = strCmd.find( "\n" )) != string::npos)
    {
        (void)strCmd.replace( pos, 1, "" );
    }
    (void)ACE_OS::memcpy(m_szLastRecvBuf, m_szRecvBuf, sizeof(m_szLastRecvBuf));

    return handleFirstLevelCmd(strCmd);

}


void CStreamDebugger::parseCmd(std::string& strCmd, std::string& strSubCmd) const
{

    std::string::size_type idx;
    idx = strCmd.find_first_not_of(" ");
    if (std::string::npos == idx)
    {
        strCmd = SVS_CMD_SPACE;
        strSubCmd = SVS_CMD_SPACE;
        return;
    }

    strSubCmd = strCmd.substr(idx);

    strCmd = strSubCmd;

    idx = strSubCmd.find_first_of(" ");
    if (std::string::npos != idx)
    {
        strSubCmd = strSubCmd.substr(0, idx);
    }
    else
    {
    }

    return;
}


int32_t CStreamDebugger::handleFirstLevelCmd(std::string& strCmd)
{

    std::string firstLevelCmd;
    parseCmd(strCmd,  firstLevelCmd);

    int32_t iRet = 0;
    MessageIter iter;
    iter = m_firstLevelMessage.find(firstLevelCmd);
    if (m_firstLevelMessage.end() != iter)
    {
        pMsgProcessor pCmdProcessor = iter->second;
        iRet = (this->*pCmdProcessor)(strCmd);
        return iRet;
    }

    iRet = sendBadCmd();
    return iRet;
}


int32_t CStreamDebugger::sendHelpInfo(std::string& strCmd)
{
    std::string strSubCmd;

    parseCmd(strCmd, strSubCmd);


    if (FIRST_LEVEL_CMD_HELP == strSubCmd)
    {
        (void)ACE_OS::snprintf( m_szSendBuf,
              sizeof(m_szSendBuf),
              "\nhelp information: \n\t"
              "help                 show help information\n\t"
              "show                 show service basic configure info\n\t"
              "show debug           show debug configuer\n\t"
              "show buffer          show system cache buffer info\n\t"
              "show session         show media session info\n\t"
              "show session [id]    show medis session detail info\n\t"
              "show session count   show all media session count\n\t"
              "show sdp [id]        show session sdp info\n\t"
              "show business        show media business info\n\t"
              "show business [id]   show media business detail info\n\t"
              "show business count  show all media business count\n\t"
              "exit                 quit\n\t"
              "quit                 quit\n\t"
              "ctrl+C               quit\n\t"
              SVS_CMD_PROMPT);

        return sendData();
    }


    return sendBadCmd();

}

int32_t CStreamDebugger::exit(std::string& strCmd)
{
    // ���pclint
    strCmd = strCmd;

    // ֻҪ��quit exit��ֱ���˳�
    return -1;
}//lint !e1762

int32_t CStreamDebugger::handleShowCmd(std::string& strCmd)
{
    int32_t iRet = 0;
    // ȥ�� show
    std::string::size_type idx = strlen(FIRST_LEVEL_CMD_SHOW);
    strCmd = strCmd.substr(idx);

    std::string showCmd;
    parseCmd(strCmd, showCmd);

    MessageIter iter;
    iter = m_showMessage.find(showCmd);
    if (m_showMessage.end() != iter)
    {
        pMsgProcessor pCmdProcessor = iter->second;
        iRet = (this->*pCmdProcessor)(strCmd);
        return iRet;
    }

    iRet = sendBadCmd();
    return iRet;
}


int32_t CStreamDebugger::sendDebugInfo(std::string& strCmd)
{
    strCmd = strCmd;

    return sendData();
}

int32_t CStreamDebugger::sendBasicInfo(std::string& strCmd)
{
    strCmd = strCmd;

    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));

    CStreamSccConnector* pConnect = CStreamServiceTask::instance()->getSccConnector();
    if (NULL == pConnect)
    {
        return 0;
    }

    const char* szConnectStatus[] =
    {
        "disconnect",
        "connecting",
        "connected",
        "registering",
        "registered"
    };

    if ((uint32_t)pConnect->getStatus() >= sizeof(szConnectStatus) / sizeof(szConnectStatus[0]))
    {
        return 0;
    }

    ACE_INET_Addr serverAddr(CStreamConfig::instance()->getSccPort(), CStreamConfig::instance()->getSccIp());
    snprintf(m_szSendBuf,
             STREAM_DEBUG_INFO_LEN,
             "\n\tstream  id[%s]"
             "\n\tservice capacity[%d]"
             "\n\tcontrol addr[%s:%d]"
             "\n\tconnect status[%s]"
             SVS_CMD_PROMPT,
             CStreamConfig::instance()->getServiceId(),
             CStreamConfig::instance()->getServiceCapacity(),
             serverAddr.get_host_addr(),
             serverAddr.get_port_number(),
             szConnectStatus[pConnect->getStatus()]);

    return sendData();
}
/*****************************************************************************
 �� �� ��  : sendSpaceCmd
 ��������  : ��ʾ������Ϣ
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ      :
 1. ��    ��   : 2009��4��13��
    �޸�����   : ��������
*****************************************************************************/
int32_t CStreamDebugger::sendBadCmd()
{
    (void)ACE_OS::snprintf( m_szSendBuf,
        sizeof(m_szSendBuf),
        "\nUnsuppored command, please enter [help] to show help info\n"
        SVS_CMD_PROMPT);
    return sendData();
}

/*****************************************************************************
 �� �� ��  : sendBufferInfo
 ��������  : ��ʾϵͳԤ�ȷ�����ڴ����Ϣ
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ      :
 1. ��    ��   : 2009��4��13��
    �޸�����   : ��������
*****************************************************************************/
int32_t CStreamDebugger::sendBufferInfo(std::string& strCmd)
{
    strCmd = strCmd;
    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));

    (void)ACE_OS::snprintf( m_szSendBuf,
                      STREAM_DEBUG_INFO_LEN,
                      "\n\tremain num[%d]"
                      "\n\ttotal  num[%d]"
                      "\n\tfailure num[%u]"
                      "\n\tblock size[%d]Bytes"
                      SVS_CMD_PROMPT,
                      CMediaBlockBuffer::instance().getRemainBlockNum(),
                      CMediaBlockBuffer::instance().getMaxBlockNum(),
                      CMediaBlockBuffer::instance().getFailureTimes(),
                      CMediaBlockBuffer::instance().getBlockSize());
    return sendData();
}
/*****************************************************************************
 �� �� ��  : sendSpaceCmd
 ��������  : ��ʾ��ʾ��Ϣ
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ      :
 1. ��    ��   : 2009��4��13��
    �޸�����   : ��������
*****************************************************************************/
int32_t CStreamDebugger::sendSpaceCmd(std::string& strCmd)
{
    strCmd = strCmd;
    sendPrompt();
    return 0;
}

void CStreamDebugger::sendNewLine()
{
    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf( m_szSendBuf, sizeof(m_szSendBuf), "\n");
    (void)sendData();

    return;
}

void CStreamDebugger::sendPrompt()
{
    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf( m_szSendBuf, sizeof(m_szSendBuf), SVS_CMD_PROMPT );
    (void)sendData();

    return;
}

int32_t CStreamDebugger::handleShowSessionCmd(std::string& strCmd)
{
    int32_t iRet = 0;
    std::string::size_type idx = strlen(SECOND_LEVEL_CMD_SESSION);
    strCmd = strCmd.substr(idx);

    std::string sessionCmd;
    parseCmd(strCmd, sessionCmd);

    MessageIter iter;
    iter = m_sessionMessage.find(sessionCmd);
    if (m_sessionMessage.end() != iter)
    {
        pMsgProcessor pCmdProcessor = iter->second;
        iRet = (this->*pCmdProcessor)(strCmd);
        return iRet;
    }

    idx = sessionCmd.find_first_not_of(" 0123456789");
    if (std::string::npos != idx)
    {
        (void)ACE_OS::snprintf(m_szSendBuf,
                      STREAM_DEBUG_INFO_LEN,
                      "\nPlease enter the session id, or enter [help] to show help info\n\t"
                      SVS_CMD_PROMPT);
        return sendData();
    }


    uint64_svs ullStreamId = strtoull(sessionCmd.c_str(), NULL, 0);

    sendSessionInfo(ullStreamId);

    return 0;
}


int32_t CStreamDebugger::handleShowSdpCmd(std::string& strCmd)
{
    std::string::size_type idx = strlen(SECOND_LEVEL_CMD_SDP);
    strCmd = strCmd.substr(idx);

    std::string sessionCmd;
    parseCmd(strCmd, sessionCmd);

    idx = sessionCmd.find_first_not_of(" 0123456789");
    if (std::string::npos != idx)
    {
        (void)ACE_OS::snprintf(m_szSendBuf,
                      STREAM_DEBUG_INFO_LEN,
                      "\nPlease enter the session id, or enter [help] to show help info\n\t"
                      SVS_CMD_PROMPT);
        return sendData();
    }


    uint64_svs ullStreamId = strtoull(sessionCmd.c_str(), NULL, 0);

    sendSessionsdp(ullStreamId);

    return 0;
}

int32_t CStreamDebugger::handleShowBusinessCmd(std::string& strCmd)
{
    int32_t iRet = 0;
    std::string::size_type idx = strlen(SECOND_LEVEL_CMD_BUSINESS);
    strCmd = strCmd.substr(idx);

    std::string businessCmd;
    parseCmd(strCmd, businessCmd);

    MessageIter iter;
    iter = m_businessMessage.find(businessCmd);
    if (m_businessMessage.end() != iter)
    {
        pMsgProcessor pCmdProcessor = iter->second;
        iRet = (this->*pCmdProcessor)(strCmd);
        return iRet;
    }

    idx = businessCmd.find_first_not_of(" 0123456789");
    if (std::string::npos != idx)
    {
        (void)ACE_OS::snprintf(m_szSendBuf,
                      STREAM_DEBUG_INFO_LEN,
                      "\nPlease enter the business id, or enter [help] to show help info\n\t"
                      SVS_CMD_PROMPT);
        return sendData();
    }


    uint64_svs ullBusinessId = strtoull(businessCmd.c_str(), NULL, 0);

    sendBusinessInfo(ullBusinessId);

    return 0;
}



int32_t CStreamDebugger::sendSessionCnt(std::string& strCmd)
{
    strCmd = strCmd;

    uint32_t inputNum = 0;
    uint32_t outputNum = 0;
    uint32_t bidirectionNum = 0;
    uint32_t totalNum = 0;

    CStreamSessionFactory::instance()->getSessionCount(inputNum, outputNum, bidirectionNum, totalNum);

    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf(m_szSendBuf,
                           STREAM_DEBUG_INFO_LEN,
                           "\n\tRecvOnly  session num[%u]"
                           "\n\tSendOnly  session num[%u]"
                           "\n\tSendRecv  session num[%u]"
                           "\n\tTotal     session num[%u]\n"
                           SVS_CMD_PROMPT,
                           inputNum,
                           outputNum,
                           bidirectionNum,
                           totalNum);

    return sendData();
}

void CStreamDebugger::sendSessionInfo(uint64_svs ullStreamId)
{
    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));

    CStreamSession* pSession = CStreamSessionFactory::instance()->findSession(ullStreamId);

    if (NULL == pSession)
    {
        (void)ACE_OS::snprintf(m_szSendBuf,
                      STREAM_DEBUG_INFO_LEN,
                      "\nSession id[%lld] is wrong, can't find corresponding session info."
                      SVS_CMD_PROMPT,
                      ullStreamId);
        (void)sendData();
        return;
    }

    sendNewLine();
    //dump the session info
    pSession->Dump(get_handle());
    CStreamSessionFactory::instance()->releaseSession(pSession);

    sendPrompt();
    return;
}

void CStreamDebugger::sendSessionsdp(uint64_svs ullStreamId)
{
    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));

    CStreamBusiness* pBusiness = NULL;
    pBusiness = CStreamBusinessManager::instance()->findBusiness(ullStreamId);
    if (NULL == pBusiness)
    {
        (void)ACE_OS::snprintf(m_szSendBuf,
                      STREAM_DEBUG_INFO_LEN,
                      "\nSession id[%lld] is wrong, can't find corresponding session info."
                      SVS_CMD_PROMPT,
                      ullStreamId);
        (void)sendData();
        return;
    }

    sendNewLine();

    pBusiness->DumpSdp(ullStreamId, get_handle());

    CStreamBusinessManager::instance()->releaseBusiness(pBusiness);

    sendPrompt();
    return;
}


int32_t CStreamDebugger::sendAllSessionInfo(std::string &strCmd)
{
    strCmd = strCmd;

    CStreamSessionList sessionList;
    CStreamSessionFactory::instance()->getAllSession(sessionList);
    if (!sessionList.empty())
    {
        CStreamSessionIter iter;
        CStreamSession* pSession = NULL;
        for (iter = sessionList.begin(); iter != sessionList.end(); iter++)
        {
            sendNewLine();

            pSession = *iter;
            pSession->Dump(get_handle());
            CStreamSessionFactory::instance()->releaseSession(pSession);
        }
    }

    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf(m_szSendBuf,
                           STREAM_DEBUG_INFO_LEN,
                           "\n\nTotal session num[%d]"
                           SVS_CMD_PROMPT,
                           sessionList.size());
    return sendData();
}

int32_t CStreamDebugger::sendBusinessCnt(std::string& strCmd)
{
    strCmd = strCmd;

    uint32_t businessNum = 0;

    businessNum = CStreamBusinessManager::instance()->getBusinessCount();

    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf(m_szSendBuf,
                           STREAM_DEBUG_INFO_LEN,
                           "\n\tBusiness total num[%u]"
                           SVS_CMD_PROMPT,
                           businessNum);

    return sendData();
}

void CStreamDebugger::sendBusinessInfo(uint64_svs ullStreamId)
{
    CStreamBusiness* pBusiness = CStreamBusinessManager::instance()->findBusiness(ullStreamId);
    if (NULL == pBusiness)
    {
        (void)ACE_OS::snprintf(m_szSendBuf,
                      STREAM_DEBUG_INFO_LEN,
                      "\nBusiness Session id[%lld] is wrong, can't find corresponding business info."
                      SVS_CMD_PROMPT,
                      ullStreamId);
        (void)sendData();
        return;
    }

    sendNewLine();
    pBusiness->Dump(get_handle());
    CStreamBusinessManager::instance()->releaseBusiness(pBusiness);

    sendPrompt();
    return;
}

int32_t CStreamDebugger::sendAllBusinessInfo(std::string &strCmd)
{
    BUSINESS_LIST      buslist;
    BUSINESS_LIST_ITER iter;
    CStreamBusiness*      pBusiness = NULL;
    CStreamBusinessManager::instance()->getAllBusiness(buslist);

    for (iter = buslist.begin(); iter != buslist.end(); ++iter)
    {
        pBusiness = *iter;
        if (NULL == pBusiness)
        {
            continue;
        }
        pBusiness->Dump(get_handle());
    }

    CStreamBusinessManager::instance()->releaseBusiness(buslist);

    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf(m_szSendBuf,
                           STREAM_DEBUG_INFO_LEN,
                           "\n\nTotal business num[%d]"
                           SVS_CMD_PROMPT,
                           buslist.size());
    return sendData();
}

int32_t CStreamDebugger::handle_timeout(const ACE_Time_Value&, const void*)
{
    if (m_iHeartbeatNum > 0)
    {
        SVS_LOG((SVS_LM_INFO,
            "Debug from[%s], wait heartbeat count is[%d].",
            m_szInetAddr,
            m_iHeartbeatNum));
        m_iHeartbeatNum = 0;
        return 0;
    }

    (void)ACE_OS::snprintf( m_szSendBuf,
                            STREAM_DEBUG_INFO_LEN,
                            "Sorry for Closing, more than 20 minutes no input\r\n");
    (void)sendData();

    return -1;
}

void CStreamDebugger::sendVersion()
{
    time_t now_time;
    (void)time(&now_time);

    char now_time_buffer[128] = {0};
    (void)ACE_OS::ctime_r(&now_time, now_time_buffer, sizeof now_time_buffer);

    char run_time_buffer[128] = {0};;
    (void)ACE_OS::ctime_r(&g_ulStartTime, run_time_buffer, sizeof run_time_buffer);


    (void)ACE_OS::snprintf(
        m_szSendBuf, sizeof m_szSendBuf,
        "\r\n\t==============================================================="
        "\r\n\r\n\t Welcome client from %s, current debug client[%d]"
        "\r\n\r\n\t version [%s] build time [%s, %s]"
        "\r\n\r\n\t\t ServerRun: %s"
        "\r\n\t\t ServerNow: %s"
        "\r\n\t==============================================================="
        "\r\n"SVS_CMD_PROMPT"Input Login Name:",
        m_szInetAddr, m_iClientNum,
        SVR_VERSION_INFO, __DATE__, __TIME__,
        run_time_buffer,
        now_time_buffer);

    (void)sendData();

    return;
}


void CStreamDebugger::sendLogFailMsg()
{
    (void)ACE_OS::snprintf( m_szSendBuf,
                      sizeof(m_szSendBuf),
                      "\nDebug client reach limit[%d] please retry later\n\n",
                      STREAM_MAX_DEBUG_NUM);
    (void)sendData();
    return;
}

//==========================CStreamDebugger END================================


int32_t CStreamDebugger::sendIAC(uint8_t cmd,uint8_t option)
{
    memset(m_szSendBuf,0,sizeof(m_szSendBuf));

    uint8_t*p=(uint8_t*)m_szSendBuf;
    p[0]=IAC;
    p[1]=cmd;
    p[2]=option;

    SVS_LOG((SVS_LM_INFO,"Send IAC [0x%X,%d]\n",cmd,option));

    return sendData();
}
int32_t CStreamDebugger::sendNegotiate()
{
    (void)sendIAC(DO,TType);
    (void)sendIAC(DO,Techo);
    return sendIAC(DO,TGA);

    //return sendIAC(WILL,TLine);
}


