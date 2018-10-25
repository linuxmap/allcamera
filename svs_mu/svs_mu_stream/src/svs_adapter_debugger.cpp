
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
int32_t CMduDebugger::m_iClientNum = 0;

//==========================CMduDebugger BEGIN================================
CMduDebugger::CMduDebugger()
{
    constructor();
}
CMduDebugger::~CMduDebugger()
{
}
void CMduDebugger::constructor()
{
    (void)ACE_OS::memset( m_szInetAddr, 0, sizeof(m_szInetAddr));
    m_iTimerId = 0;
    m_iHeartbeatNum = 0;

    m_firstLevelMessage.insert( std::make_pair(SVS_CMD_SPACE, &CMduDebugger::sendSpaceCmd));
    m_firstLevelMessage.insert( std::make_pair(FIRST_LEVEL_CMD_HELP, &CMduDebugger::sendHelpInfo));
    m_firstLevelMessage.insert( std::make_pair(FIRST_LEVEL_CMD_SHOW, &CMduDebugger::handleShowCmd));
    m_firstLevelMessage.insert( std::make_pair(FIRST_LEVEL_CMD_QUIT, &CMduDebugger::exit));
    m_firstLevelMessage.insert( std::make_pair(FIRST_LEVEL_CMD_EXIT, &CMduDebugger::exit));

    m_showMessage.insert(std::make_pair(SVS_CMD_SPACE, &CMduDebugger::sendBasicInfo));
    m_showMessage.insert(std::make_pair(SECOND_LEVEL_CMD_DEBUG, &CMduDebugger::sendDebugInfo));
    m_showMessage.insert(std::make_pair(SECOND_LEVEL_CMD_SESSION, &CMduDebugger::handleShowSessionCmd));
    m_showMessage.insert(std::make_pair(SECOND_LEVEL_CMD_BUSINESS, &CMduDebugger::handleShowBusinessCmd));
    m_showMessage.insert(std::make_pair(SECOND_LEVEL_CMD_BUFFER, &CMduDebugger::sendBufferInfo));
    m_showMessage.insert(std::make_pair(SECOND_LEVEL_CMD_SDP, &CMduDebugger::handleShowSdpCmd));

    m_sessionMessage.insert(std::make_pair(SVS_CMD_SPACE, &CMduDebugger::sendAllSessionInfo));
    m_sessionMessage.insert(std::make_pair(THIRD_LEVEL_CMD_COUNT, &CMduDebugger::sendSessionCnt));

    m_businessMessage.insert(std::make_pair(SVS_CMD_SPACE, &CMduDebugger::sendAllBusinessInfo));
    m_businessMessage.insert(std::make_pair(THIRD_LEVEL_CMD_COUNT, &CMduDebugger::sendBusinessCnt));

    return;
}

int32_t CMduDebugger::open(void *)
{
    m_iClientNum++;
    if (m_iClientNum > MDU_MAX_DEBUG_NUM)
    {
        SVS_LOG((SVS_LM_ERROR,"[DEBUG]Debug client reach limit[%d].", MDU_MAX_DEBUG_NUM));
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

int32_t CMduDebugger::svc()
{
    return 0;
}
/*****************************************************************************
 ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½  : handle_close
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½Ø±ï¿½ï¿½ï¿½ï¿½ï¿½
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ACE_HANDLE
             ACE_Reactor_Mask
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ ï¿½ï¿½ Öµ  : ï¿½ï¿½ï¿½ï¿½0
 ï¿½Þ¸ï¿½ï¿½ï¿½Ê·      :
 1. ï¿½ï¿½    ï¿½ï¿½   : 2009ï¿½ï¿½4ï¿½ï¿½13ï¿½ï¿½
    ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½ï¿½   : ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
*****************************************************************************/
int32_t CMduDebugger::handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask /*mask*/)
{
    SVS_LOG((SVS_LM_INFO,
        "[DEBUG] Debug connection closed. client addr[%s], this[%p], handle[%d].",
        m_szInetAddr, this, get_handle()));

    m_iClientNum--;
    (void)reactor()->cancel_timer(this, 1);

    //ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ï¢
    (void)reactor()->remove_handler(this, ACE_Event_Handler::ALL_EVENTS_MASK | ACE_Event_Handler::DONT_CALL);

    (void)ACE_OS::shutdown(peer().get_handle(), SHUT_RDWR);
    (void)peer().close();

    delete this;

    return 0;
}
/*****************************************************************************
 ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½  : sendData
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½Í»ï¿½ï¿½Ë·ï¿½ï¿½Í»ï¿½ï¿½ï¿½ï¿½Ðµï¿½ï¿½ï¿½ï¿½ï¿½,Ò»ï¿½ï¿½ï¿½ï¿½ï¿½î´¦ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ê¾ï¿½ï¿½Ï¢
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ ï¿½ï¿½ Öµ  : ï¿½É¹ï¿½ï¿½ï¿½ï¿½ï¿½0,Ê§ï¿½Ü·ï¿½ï¿½ï¿½-1
 ï¿½Þ¸ï¿½ï¿½ï¿½Ê·      :
 1. ï¿½ï¿½    ï¿½ï¿½   : 2009ï¿½ï¿½4ï¿½ï¿½13ï¿½ï¿½
    ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½ï¿½   : ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
*****************************************************************************/
int32_t CMduDebugger::sendData()
{
    //ï¿½ï¿½Ö¹ï¿½Ú´ï¿½ï¿½ï¿½ï¿½Ô½ï¿½ï¿½
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
/*****************************************************************************
 ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½  : handle_input
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½ï¿½Õ¿Í»ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ACE_HANDLE
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ ï¿½ï¿½ Öµ  : ï¿½É¹ï¿½ï¿½ï¿½ï¿½ï¿½0,Ê§ï¿½Ü·ï¿½ï¿½ï¿½-1
 ï¿½Þ¸ï¿½ï¿½ï¿½Ê·      :
 1. ï¿½ï¿½    ï¿½ï¿½   : 2009ï¿½ï¿½4ï¿½ï¿½13ï¿½ï¿½
    ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½ï¿½   : ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
*****************************************************************************/
int32_t CMduDebugger::handle_input(ACE_HANDLE /*handle*/)
{
    m_iHeartbeatNum++;
    ACE_Time_Value tv(SVS_LOG_RECV_TIMEOUT);

    int32_t recvResult = peer().recv( m_szRecvBuf, MDU_DEBUG_CMD_LEN, &tv );

    if (0 >= recvResult)
    {
        SVS_LOG((SVS_LM_ERROR, "[DEBUG] Recv debug message fail. client[%s], this[%p].",
                   m_szInetAddr,
                   this));
        return -1;
    }

    // ï¿½ï¿½telnetï¿½ï¿½ï¿½ï¿½Ê±ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ctrlï¿½ï¿½Cï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ø¶ï¿½ï¿½ï¿½5ï¿½ï¿½ï¿½Ö½Ú£ï¿½0XFF 0XF4 0XFF 0XFD 0X06
    // Ö»Òªï¿½ï¿½ï¿½ï¿½ï¿½Ð¶ï¿½ï¿½Õµï¿½ï¿½ï¿½5ï¿½ï¿½ï¿½Ö½Ú£ï¿½ï¿½ï¿½ï¿½Ë³ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ë£ï¿½ï¿½Í¿ï¿½ï¿½ï¿½ï¿½ï¿½Ó¦Ctrl+Cï¿½ï¿½
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

    //cmd È¥ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
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


void CMduDebugger::parseCmd(std::string& strCmd, std::string& strSubCmd) const
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

/*****************************************************************************
 ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½  : handle_message
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½Ô¿Í»ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ð·Ö·ï¿½ï¿½ï¿½ï¿½ï¿½
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : const std::string &strCmd ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ ï¿½ï¿½ Öµ  : ï¿½É¹ï¿½ï¿½ï¿½ï¿½ï¿½0,Ê§ï¿½Ü·ï¿½ï¿½ï¿½-1
 ï¿½Þ¸ï¿½ï¿½ï¿½Ê·      :
 1. ï¿½ï¿½    ï¿½ï¿½   : 2009ï¿½ï¿½4ï¿½ï¿½13ï¿½ï¿½
    ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½ï¿½   : ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
*****************************************************************************/
int32_t CMduDebugger::handleFirstLevelCmd(std::string& strCmd)
{
    // ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ê½
    // ï¿½Ö¼ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
    // ï¿½ï¿½Ò»ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ö§ï¿½ï¿½ help,show,exit,quit


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

    // Ã»ï¿½Ð²ï¿½ï¿½Òµï¿½ï¿½ï¿½Ó¦ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
    iRet = sendBadCmd();
    return iRet;
}

// Ò»ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
/*****************************************************************************
 ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½  : sendHelpInfo
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½Ê¾ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ï¢
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ ï¿½ï¿½ Öµ  : ï¿½É¹ï¿½ï¿½ï¿½ï¿½ï¿½0,Ê§ï¿½Ü·ï¿½ï¿½ï¿½-1
 ï¿½Þ¸ï¿½ï¿½ï¿½Ê·      :
 1. ï¿½ï¿½    ï¿½ï¿½   : 2009ï¿½ï¿½4ï¿½ï¿½13ï¿½ï¿½
    ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½ï¿½   : ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
*****************************************************************************/
int32_t CMduDebugger::sendHelpInfo(std::string& strCmd)
{
    std::string strSubCmd;

    parseCmd(strCmd, strSubCmd);


    if (FIRST_LEVEL_CMD_HELP == strSubCmd)
    {
        (void)ACE_OS::snprintf( m_szSendBuf,
              sizeof(m_szSendBuf),
              "\n°ïÖúÐÅÏ¢: \n\t"
              "help                 ÏÔÊ¾°ïÖúÐÅÏ¢\n\t"
              "show                 ÏÔÊ¾·þÎñ»ù±¾ÅäÖÃÐÅÏ¢\n\t"
              "show debug           ÏÔÊ¾µ÷ÊÔÅäÖÃÐÅÏ¢\n\t"
              "show buffer          ÏÔÊ¾ÏµÍ³»º³åÇøÐÅÏ¢\n\t"
              "show session         ÏÔÊ¾Ã½Ìå»á»°ÐÅÏ¢\n\t"
              "show session [id]    ÏÔÊ¾Ã½Ìå»á»°ÏêÏ¸ÐÅÏ¢\n\t"
              "show session count   ÏÔÊ¾Ã½Ìå»á»°ÊýÁ¿\n\t"
              "show sdp [id]        ÏÔÊ¾Ã½Ìå»á»°SDPÐÅÏ¢\n\t"
              "show business        ÏÔÊ¾Ã½ÌåÒµÎñÐÅÏ¢\n\t"
              "show business [id]   ÏÔÊ¾Ã½ÌåÒµÎñÏêÏ¸ÐÅÏ¢\n\t"
              "show business count  ÏÔÊ¾Ã½ÌåÒµÎñÊýÁ¿\n\t"
              "exit                 ÍË³ö\n\t"
              "quit                 ÍË³ö\n\t"
              "ctrl+C               ÍË³ö\n\t"
              SVS_CMD_PROMPT);

        return sendData();
    }


    return sendBadCmd();

}

int32_t CMduDebugger::exit(std::string& strCmd)
{
    // ï¿½ï¿½ï¿½pclint
    strCmd = strCmd;

    // Ö»Òªï¿½ï¿½quit exitï¿½ï¿½Ö±ï¿½ï¿½ï¿½Ë³ï¿½
    return -1;
}//lint !e1762

int32_t CMduDebugger::handleShowCmd(std::string& strCmd)
{
    int32_t iRet = 0;
    // È¥ï¿½ï¿½ show
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


int32_t CMduDebugger::sendDebugInfo(std::string& strCmd)
{
    strCmd = strCmd;

    return sendData();
}

int32_t CMduDebugger::sendBasicInfo(std::string& strCmd)
{
    strCmd = strCmd;

    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));

    CMduSccConnector* pConnect = CMduServiceTask::instance()->getSccConnector();
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

    ACE_INET_Addr serverAddr(CMduConfig::instance()->getSccPort(), CMduConfig::instance()->getSccIp());
    snprintf(m_szSendBuf,
             MDU_DEBUG_INFO_LEN,
             "\n\tstream  id[%s]"
             "\n\tservice capacity[%d]"
             "\n\tcontrol addr[%s:%d]"
             "\n\tconnect status[%s]"
             SVS_CMD_PROMPT,
             CMduConfig::instance()->getServiceId(),
             CMduConfig::instance()->getServiceCapacity(),
             serverAddr.get_host_addr(),
             serverAddr.get_port_number(),
             szConnectStatus[pConnect->getStatus()]);

    return sendData();
}
/*****************************************************************************
 ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½  : sendSpaceCmd
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½Ê¾ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ï¢
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ ï¿½ï¿½ Öµ  : ï¿½É¹ï¿½ï¿½ï¿½ï¿½ï¿½0,Ê§ï¿½Ü·ï¿½ï¿½ï¿½-1
 ï¿½Þ¸ï¿½ï¿½ï¿½Ê·      :
 1. ï¿½ï¿½    ï¿½ï¿½   : 2009ï¿½ï¿½4ï¿½ï¿½13ï¿½ï¿½
    ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½ï¿½   : ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
*****************************************************************************/
int32_t CMduDebugger::sendBadCmd()
{
    (void)ACE_OS::snprintf( m_szSendBuf,
        sizeof(m_szSendBuf),
        "\nUnsuppored command, please enter [help] to show help info\n"
        SVS_CMD_PROMPT);
    return sendData();
}

/*****************************************************************************
 ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½  : sendBufferInfo
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½Ê¾ÏµÍ³Ô¤ï¿½È·ï¿½ï¿½ï¿½ï¿½ï¿½Ú´ï¿½ï¿½ï¿½ï¿½Ï¢
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ ï¿½ï¿½ Öµ  : ï¿½É¹ï¿½ï¿½ï¿½ï¿½ï¿½0,Ê§ï¿½Ü·ï¿½ï¿½ï¿½-1
 ï¿½Þ¸ï¿½ï¿½ï¿½Ê·      :
 1. ï¿½ï¿½    ï¿½ï¿½   : 2009ï¿½ï¿½4ï¿½ï¿½13ï¿½ï¿½
    ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½ï¿½   : ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
*****************************************************************************/
int32_t CMduDebugger::sendBufferInfo(std::string& strCmd)
{
    strCmd = strCmd;
    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));

    (void)ACE_OS::snprintf( m_szSendBuf,
                      MDU_DEBUG_INFO_LEN,
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
 ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½  : sendSpaceCmd
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½Ê¾ï¿½ï¿½Ê¾ï¿½ï¿½Ï¢
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½  : ï¿½ï¿½
 ï¿½ï¿½ ï¿½ï¿½ Öµ  : ï¿½É¹ï¿½ï¿½ï¿½ï¿½ï¿½0,Ê§ï¿½Ü·ï¿½ï¿½ï¿½-1
 ï¿½Þ¸ï¿½ï¿½ï¿½Ê·      :
 1. ï¿½ï¿½    ï¿½ï¿½   : 2009ï¿½ï¿½4ï¿½ï¿½13ï¿½ï¿½
    ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½ï¿½   : ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
*****************************************************************************/
int32_t CMduDebugger::sendSpaceCmd(std::string& strCmd)
{
    strCmd = strCmd;
    sendPrompt();
    return 0;
}

void CMduDebugger::sendNewLine()
{
    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf( m_szSendBuf, sizeof(m_szSendBuf), "\n");
    (void)sendData();

    return;
}

void CMduDebugger::sendPrompt()
{
    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf( m_szSendBuf, sizeof(m_szSendBuf), SVS_CMD_PROMPT );
    (void)sendData();

    return;
}

int32_t CMduDebugger::handleShowSessionCmd(std::string& strCmd)
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
                      MDU_DEBUG_INFO_LEN,
                      "\nPlease enter the session id, or enter [help] to show help info\n\t"
                      SVS_CMD_PROMPT);
        return sendData();
    }


    uint64_svs ullStreamId = strtoull(sessionCmd.c_str(), NULL, 0);

    sendSessionInfo(ullStreamId);

    return 0;
}


int32_t CMduDebugger::handleShowSdpCmd(std::string& strCmd)
{
    std::string::size_type idx = strlen(SECOND_LEVEL_CMD_SDP);
    strCmd = strCmd.substr(idx);

    std::string sessionCmd;
    parseCmd(strCmd, sessionCmd);

    idx = sessionCmd.find_first_not_of(" 0123456789");
    if (std::string::npos != idx)
    {
        (void)ACE_OS::snprintf(m_szSendBuf,
                      MDU_DEBUG_INFO_LEN,
                      "\nPlease enter the session id, or enter [help] to show help info\n\t"
                      SVS_CMD_PROMPT);
        return sendData();
    }


    uint64_svs ullStreamId = strtoull(sessionCmd.c_str(), NULL, 0);

    sendSessionsdp(ullStreamId);

    return 0;
}

int32_t CMduDebugger::handleShowBusinessCmd(std::string& strCmd)
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
                      MDU_DEBUG_INFO_LEN,
                      "\nPlease enter the business id, or enter [help] to show help info\n\t"
                      SVS_CMD_PROMPT);
        return sendData();
    }


    uint64_svs ullBusinessId = strtoull(businessCmd.c_str(), NULL, 0);

    sendBusinessInfo(ullBusinessId);

    return 0;
}



int32_t CMduDebugger::sendSessionCnt(std::string& strCmd)
{
    strCmd = strCmd;

    uint32_t inputNum = 0;
    uint32_t outputNum = 0;
    uint32_t bidirectionNum = 0;
    uint32_t totalNum = 0;

    CMduSessionFactory::instance()->getSessionCount(inputNum, outputNum, bidirectionNum, totalNum);

    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf(m_szSendBuf,
                           MDU_DEBUG_INFO_LEN,
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

void CMduDebugger::sendSessionInfo(uint64_svs ullStreamId)
{
    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));

    CMduSession* pSession = CMduSessionFactory::instance()->findSession(ullStreamId);

    if (NULL == pSession)
    {
        (void)ACE_OS::snprintf(m_szSendBuf,
                      MDU_DEBUG_INFO_LEN,
                      "\nSession id[%lld] is wrong, can't find corresponding session info."
                      SVS_CMD_PROMPT,
                      ullStreamId);
        (void)sendData();
        return;
    }

    sendNewLine();
    //dump the session info
    pSession->Dump(get_handle());
    CMduSessionFactory::instance()->releaseSession(pSession);

    sendPrompt();
    return;
}

void CMduDebugger::sendSessionsdp(uint64_svs ullStreamId)
{
    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));

    CMduBusiness* pBusiness = NULL;
    pBusiness = CMduBusinessManager::instance()->findBusiness(ullStreamId);
    if (NULL == pBusiness)
    {
        (void)ACE_OS::snprintf(m_szSendBuf,
                      MDU_DEBUG_INFO_LEN,
                      "\nSession id[%lld] is wrong, can't find corresponding session info."
                      SVS_CMD_PROMPT,
                      ullStreamId);
        (void)sendData();
        return;
    }

    sendNewLine();

    pBusiness->DumpSdp(ullStreamId, get_handle());

    CMduBusinessManager::instance()->releaseBusiness(pBusiness);

    sendPrompt();
    return;
}


int32_t CMduDebugger::sendAllSessionInfo(std::string &strCmd)
{
    strCmd = strCmd;

    CMduSessionList sessionList;
    CMduSessionFactory::instance()->getAllSession(sessionList);
    if (!sessionList.empty())
    {
        CMduSessionIter iter;
        CMduSession* pSession = NULL;
        for (iter = sessionList.begin(); iter != sessionList.end(); iter++)
        {
            sendNewLine();

            pSession = *iter;
            pSession->Dump(get_handle());
            CMduSessionFactory::instance()->releaseSession(pSession);
        }
    }

    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf(m_szSendBuf,
                           MDU_DEBUG_INFO_LEN,
                           "\n\nTotal session num[%d]"
                           SVS_CMD_PROMPT,
                           sessionList.size());
    return sendData();
}

int32_t CMduDebugger::sendBusinessCnt(std::string& strCmd)
{
    strCmd = strCmd;

    uint32_t businessNum = 0;

    businessNum = CMduBusinessManager::instance()->getBusinessCount();

    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf(m_szSendBuf,
                           MDU_DEBUG_INFO_LEN,
                           "\n\tBusiness total num[%u]"
                           SVS_CMD_PROMPT,
                           businessNum);

    return sendData();
}

void CMduDebugger::sendBusinessInfo(uint64_svs ullStreamId)
{
    CMduBusiness* pBusiness = CMduBusinessManager::instance()->findBusiness(ullStreamId);
    if (NULL == pBusiness)
    {
        (void)ACE_OS::snprintf(m_szSendBuf,
                      MDU_DEBUG_INFO_LEN,
                      "\nBusiness Session id[%lld] is wrong, can't find corresponding business info."
                      SVS_CMD_PROMPT,
                      ullStreamId);
        (void)sendData();
        return;
    }

    sendNewLine();
    pBusiness->Dump(get_handle());
    CMduBusinessManager::instance()->releaseBusiness(pBusiness);

    sendPrompt();
    return;
}

int32_t CMduDebugger::sendAllBusinessInfo(std::string &strCmd)
{
    BUSINESS_LIST      buslist;
    BUSINESS_LIST_ITER iter;
    CMduBusiness*      pBusiness = NULL;
    CMduBusinessManager::instance()->getAllBusiness(buslist);

    for (iter = buslist.begin(); iter != buslist.end(); ++iter)
    {
        pBusiness = *iter;
        if (NULL == pBusiness)
        {
            continue;
        }
        pBusiness->Dump(get_handle());
    }

    CMduBusinessManager::instance()->releaseBusiness(buslist);

    memset(m_szSendBuf, 0x0, sizeof(m_szSendBuf));
    (void)ACE_OS::snprintf(m_szSendBuf,
                           MDU_DEBUG_INFO_LEN,
                           "\n\nTotal business num[%d]"
                           SVS_CMD_PROMPT,
                           buslist.size());
    return sendData();
}

int32_t CMduDebugger::handle_timeout(const ACE_Time_Value&, const void*)
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
                            MDU_DEBUG_INFO_LEN,
                            "Sorry for Closing, more than 20 minutes no input\r\n");
    (void)sendData();

    return -1;
}

void CMduDebugger::sendVersion()
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


void CMduDebugger::sendLogFailMsg()
{
    (void)ACE_OS::snprintf( m_szSendBuf,
                      sizeof(m_szSendBuf),
                      "\nDebug client reach limit[%d] please retry later\n\n",
                      MDU_MAX_DEBUG_NUM);
    (void)sendData();
    return;
}

//==========================CMduDebugger END================================


int32_t CMduDebugger::sendIAC(uint8_t cmd,uint8_t option)
{
    memset(m_szSendBuf,0,sizeof(m_szSendBuf));

    uint8_t*p=(uint8_t*)m_szSendBuf;
    p[0]=IAC;
    p[1]=cmd;
    p[2]=option;

    SVS_LOG((SVS_LM_INFO,"Send IAC [0x%X,%d]\n",cmd,option));

    return sendData();
}
int32_t CMduDebugger::sendNegotiate()
{
    (void)sendIAC(DO,TType);
    (void)sendIAC(DO,Techo);
    return sendIAC(DO,TGA);

    //return sendIAC(WILL,TLine);
}


