/******************************************************************************

                  版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文 件 名   : SVS_Console_Server.cpp
  版 本 号   : 初稿
  生成日期   : 2010年3月10日
  最近修改   :
  功能描述   : 与控制台的连接管理，消息接收和发送。
  函数列表   :
  修改历史   :
  1.日    期   : 2010年3月10日
    修改内容   : 创建文件

******************************************************************************/
#include "svs_timer.h"
#include "svs_rt_record_common.h"
#include "svs_stat_manager.h"
#include "svs_log_msg.h"
#include "svs_daemon_thread.h"
#include "svs_mb_buffer.h"
#include "svs_console_server.h"
#include "svs_real_record_server.h"
#include "svs_record_trace.h"

ACE_Message_Block* allocate_mb(uint32_t ulMbSize)
{
    ACE_Message_Block* mb = NULL;

    try
    {
        mb = new ACE_Message_Block( ulMbSize);
    }
    catch(...)
    {
        return NULL;
    }

    // 申请的内存判断
    if ( (NULL == mb->data_block())
        || (NULL == mb->base()))
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Allocate media buffer failed."
            "mb[0x%08x],mb->data_block[0x%08x],mb->base[0x%08x].",
            mb,
            mb->data_block(),
            mb->base()));

        SVS_DELETE ( mb );

        return NULL;
    }

    mb->reset();
    return mb;
}

//=================SVS_Console_Client_Handler BEGIN=============================
SVS_Console_Client_Handler::SVS_Console_Client_Handler(
  ACE_Reactor *pReactor,
  ACE_Message_Queue<ACE_SYNCH> *pMsgQueue)
  : CSVS_Client_Handler(pReactor,pMsgQueue, SVS_SERVER_TYPE_CONSOLE)
{
}

SVS_Console_Client_Handler::~SVS_Console_Client_Handler()
{

}

/*****************************************************************************
函 数 名  : reconnect
功能描述  : 重连Console。在连接上StoreSvr之后，才连接Console.
            如果前面已经连接过了，把自己冲reactor中去注册掉
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Console_Client_Handler::reconnect()
{
    // 判断和StoreSvr的连接状态
    if(!CAC_RT_Record_Server::instance()->is_store_svr_conn())
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Reconnect with Console failed."
            "Can't connect with Console as disconnecting from StoreSvr."));

        return SVS_RESULT_OK;
    }

    // 如果状态出现问题，纠正处理
    if(RT_CONN_STORE_DISCONNECTED != conn_status_)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Reconnect with Console occur error."
            "Set the connection status to disconnected."
            "old status[%d].",
            conn_status_));

        // 把自己从reactor主去注册
        (void)reactor()->remove_handler(
                    this,
                    ACE_Event_Handler::RWE_MASK | ACE_Event_Handler::DONT_CALL);
        ACE_OS::last_error(0);

        //任何状态都可以到该状态，所以可以直接置状态
        conn_status_ = RT_CONN_STORE_DISCONNECTED;
    }

    // 初始化缓存区和socket
    recv_data_len_ = 0;

    SVS_Console_Server * pServer = SVS_Console_Server::instance();
    if(NULL == pServer)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "[CSL]Reconnect with Console failed. Console pointor is NULL."));

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_WARNING, "[CSL]Retry to connect with Console."));

    //在开始连接之前将状态设置为"连接中"
    if(SVS_RESULT_OK != set_conn_status(RT_CONN_STORE_CONNECTING))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Set connection status with Console failed."
            "old status[%d], new status[%d]",
            conn_status_,
            RT_CONN_STORE_CONNECTING));

        return SVS_RESULT_FAILURE;
    }

    // 创建新连接
    ACE_SOCK_Connector connector;
    ACE_INET_Addr remoteAddr(pServer->consolePort(),pServer->strConsoleIp());
    ACE_INET_Addr localAddr(pServer->lanPort(),pServer->strLanIp());

    // 设备连接超时时长
    ACE_Time_Value tv(1, 0);
    if (-1 == connector.connect(this->peer(), remoteAddr, &tv, localAddr, 1))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to reconnect with Console, "
            "RtRecordSvr[%s:%d], Console[%s:%d].",
            localAddr.get_host_addr(),
            localAddr.get_port_number(),
            remoteAddr.get_host_addr(),
            remoteAddr.get_port_number()));

        (void)this->peer().close();

        return SVS_RESULT_FAILURE;
    }

    //设置缓存区
    set_socket_option();

    SVS_LOG((SVS_LM_WARNING,
        "[CSL]Connect with Console succeeded, "
        "RtRecordSvr[%s:%d], Console[%s:%d].",
        localAddr.get_host_addr(),
        localAddr.get_port_number(),
        remoteAddr.get_host_addr(),
        remoteAddr.get_port_number()));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : send_register_msg
功能描述  : 发送注册消息。
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Console_Client_Handler::send_register_msg()
{
    SVS_Console_Server * pServer = SVS_Console_Server::instance();

    if(NULL == pServer)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "[CSL]Reconnect with Console failed. server pointor is NULL."));

        return SVS_RESULT_FAILURE;
    }

    SVS_CONSOLE_REG_REQ_MSG regMsg;
    memset(&regMsg, 0, sizeof(regMsg));

    memcpy(&regMsg, pServer->regInfo(), sizeof(regMsg));
    // 构造注册消息
    FillCommonHeader(   regMsg.Header,
                        SVS_CONSOLE_REG_REQ,
                        1,
                        sizeof(SVS_CONSOLE_REG_REQ_MSG));

    ACE_Time_Value tv(10, 0);
    if(sizeof(regMsg) != (uint32_t)this->peer().send_n(&regMsg, sizeof(regMsg), &tv))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[SEND][CSL]Failed to send registering message to Console."));

        return SVS_RESULT_FAILURE;
    }

    new_loggin_time_ = time(NULL);
    char serverId[SESSIONID_LEN + 1] = {0,};
    memcpy(serverId, regMsg.SERVICE_ID, SESSIONID_LEN);

    char serverName[DEVICE_NAME_LEN + 1] = {0,};
    memcpy(serverName, regMsg.SERVICE_NAME, strlen((char*)regMsg.SERVICE_NAME));

    char nodeId[SVS_ZONE_ORG_LEN + 1] = {0,};
    memcpy(nodeId, regMsg.NODE_ID, sizeof(regMsg.NODE_ID));

    char nodeName[DEVICE_NAME_LEN + 1] = {0,};
    memcpy(nodeName, regMsg.NODE_NAME, strlen((char*)regMsg.NODE_NAME));

    SVS_LOG((SVS_LM_DEBUG,
        "[CSL]Information of register to Console:"
        "SERVICE_ID[%s],SERVICE_TYPE[%u],SERVICE_IP[%u],"
        "SERVICE_NAME[%s],NODE_ID[%s],NODE_NAME[%s].",
        serverId, regMsg.SERVICE_TYPE,regMsg.SERVICE_IP,
        serverName,nodeId,nodeName));


    SVS_LOG((SVS_LM_WARNING,
        "[SEND][CSL]Send registering message to Console succeeded."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : handle_register_response_msg
功能描述  : 处理来自控制台的注册相应消息。如果状态不是"注册中"时收到了注册相应消息
            说明有问题，断开连接交由重连线程重连。如果非ready状态收到其他信令消息，
            也需要断开连接，防止收到过期的消息。
输入参数  : const SVS_HEADER* pMsgHeader:消息头
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Console_Client_Handler::handle_register_response_msg(const SVS_HEADER* pMsgHeader)
{
    // 如果消息类型是注册响应消息
    if(SVS_CONSOLE_REG_RESP == pMsgHeader->MsgType)
    {
        // 如果状态转换失败,重新连接
        if(SVS_RESULT_OK != set_conn_status(RT_CONN_STORE_READY))
        {
            SVS_LOG((SVS_LM_ERROR,
                "[RECV][CSL]Receive a register response message from Console,"
                "but connection status is not logging in."
                "Real connection status [%d].",
                conn_status_));
            return HANDLE_RECV_DATA_ERROR;
        }

        // 如果注册失败，
        SVS_CONSOLE_REG_RESP_MSG *pRegisterResp =
            (SVS_CONSOLE_REG_RESP_MSG *)(void*)pMsgHeader;
        if (0 != pRegisterResp->ERROR_CODE)
        {
            SVS_LOG((SVS_LM_ERROR,
                "[RECV][CSL]The registering response message from Console is invalid."
                "The response code is invalid, response code[%d].",
                pRegisterResp->ERROR_CODE));

            return HANDLE_RECV_DATA_ERROR;
        }

        SVS_LOG((SVS_LM_INFO,
            "[RECV][CSL]Receive registering response message from Console successfully."));

        // 继续处理其它信令消息
        return HANDLE_RECV_DATA_CONTINUE;
    }
    else if(RT_CONN_STORE_READY != conn_status_) // 如果非连接状态收到消息，则断开连接
    {
        SVS_LOG((SVS_LM_ERROR,
            "[RECV][CSL]Receive a message from Console, "
            "but connection status is not RT_CONN_STORE_READY, "
            "Real connection status is %d.",
            conn_status_));

        return HANDLE_RECV_DATA_ERROR;
    }

    return HANDLE_RECV_DATA_SUCCESS;
}

/*****************************************************************************
函 数 名  : disconnection
功能描述  : 断开和Server的连接。如果连接状态已经是断开状态了，则断开完成。
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Console_Client_Handler::disconnection()
{
    // 如果状态已经是断开连接状态，则断开连接完成
    if(RT_CONN_STORE_DISCONNECTED == conn_status_)
    {
        SVS_LOG((SVS_LM_INFO,
            "[CSL]Needn't to disconnected from storeSvr, "
            "Current status is disconnected."));

        return SVS_RESULT_OK;
    }

    SVS_LOG((SVS_LM_WARNING, "[CSL]Begin to disconnect from Console."));
    // 把自己从reactor主去注册
    (void)reactor()->remove_handler(this,ACE_Event_Handler::RWE_MASK | ACE_Event_Handler::DONT_CALL);

    // 断开连接
    (void)ACE_OS::shutdown(this->peer().get_handle(), SHUT_RDWR);
    (void)this->peer().close();
    this->peer().set_handle(ACE_INVALID_HANDLE);

    // 设置连接状态为断开状态
    if(SVS_RESULT_OK != set_conn_status(RT_CONN_STORE_DISCONNECTED))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Set connection status with Console failed."
            "old status[%d], new status[%d].",
            conn_status_,
            RT_CONN_STORE_DISCONNECTED));
    }

    return SVS_RESULT_OK;
}

ACE_Message_Block* SVS_Console_Client_Handler::alloc_mb(
                                            uint32_t mbMsgLen,
                                            uint8_t /*mbType*/ )const
{
    return allocate_mb(mbMsgLen);
}

void SVS_Console_Client_Handler::free_mb(ACE_Message_Block *mb)const
{
    delete mb;
    mb = NULL;
    return;
}


//=================SVS_Console_Client_Handler END===============================

//=================SVS_Console_Server BEGIN=====================================
SVS_Console_Server::SVS_Console_Server():m_nThreadIndex(0)
{
    m_pClientHandler = NULL;
    m_pConnectEpollReactor = NULL;
    m_pConnectReactor = NULL;

    memset(m_strConsoleIp, 0 ,sizeof(m_strConsoleIp));
    memset(m_strLanIp, 0 ,sizeof(m_strLanIp));
    m_uConsolePort = 0;
    m_uLanPort = 0;
    memset(&m_szRegMsg, 0, sizeof(m_szRegMsg));
    m_bRunning = true;
}

SVS_Console_Server::~SVS_Console_Server()
{
    try
    {
        if(NULL != m_pClientHandler)
        {
            SVS_DELETE(m_pClientHandler);
        }

        if(NULL != m_pConnectReactor)
        {
            SVS_DELETE(m_pConnectReactor);
        }

        if(NULL != m_pConnectEpollReactor)
        {
            SVS_DELETE(m_pConnectEpollReactor);
        }
    }
    catch(...)
    {
    }
}

/*****************************************************************************
函 数 名  : open
功能描述  : 资源申请和线程启动。创建连接Console的handler对象。
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Console_Server::open(void *)
{
    // 如果连接控制台的开关未打开，则直接成功返回
    if(0 != m_ulConnectConsoleSwitch)
    {
        return SVS_RESULT_OK;
    }

    // 创建连接存储管理服务器epoll反应器
    try
    {
        m_pConnectEpollReactor = new ACE_Dev_Poll_Reactor(SVS_EPOLL_MAX_HANDLE_NUM);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "[CSL]Create the epoll reactor for connecting Console failed."));
        return SVS_RESULT_FAILURE;
    }

    try
    {
        m_pConnectReactor = new ACE_Reactor(m_pConnectEpollReactor);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "[CSL]Create the reactor for connecting Console failed."));

        return SVS_RESULT_FAILURE;
    }

    try
    {
        m_pClientHandler = new SVS_Console_Client_Handler(m_pConnectReactor, msg_queue());
    }
    catch( ...)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Create the handle for connection with Console failed."
            "Allocate memory failed."));

        return SVS_RESULT_FAILURE;
    }

    int32_t iRet = SVS_RESULT_FAILURE;
    // 打开消息处理队列
    (void)msg_queue()->close();
    iRet = msg_queue()->open( SVS_CONSOLE_MESSAGE_QUEUE_WM, SVS_CONSOLE_MESSAGE_QUEUE_WM );
    if ( SVS_RESULT_OK != iRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "[CSL]Open the queue of message from Console failed."));

        return SVS_RESULT_FAILURE;
    }

    // 打开消息处理队列
    (void)m_timerCallbackQueue.close();
    iRet = m_timerCallbackQueue.open( SVS_CONSOLE_TIMER_CALLBACK_QUEUE_WM,
                                      SVS_CONSOLE_TIMER_CALLBACK_QUEUE_WM );
    if ( SVS_RESULT_OK !=  iRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "[CSL]Open the timer callback queue for Console failed."));

        return SVS_RESULT_FAILURE;
    }

    // 连接服务
    iRet = m_pClientHandler->open(0);
    if (0 != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Register the handle for connecting with Console failed."));

        return SVS_RESULT_FAILURE;
    }


    //启动服务线程
    uint32_t runThreadNum = SVS_CONSOLE_MAX_THREAD;
    //修改线程栈大小-----------------
    size_t *stack_size = NULL;
    try
    {
        stack_size = new size_t[runThreadNum];
    }
    catch(...)
    {
        SVS_LOG( (SVS_LM_CRITICAL,
            "[CSL]Open Console service failed.Allocate memory failed." ) );

        SVS_DELETE(stack_size, SVS_DELETE_MULTI);

        return SVS_RESULT_FAILURE;
    }

    // 初始化线程栈最大值
    uint32_t i = 0;
    for ( i=0; i<runThreadNum; i++)
    {
        stack_size[i] = SVS_THREAD_STATCK_MAX;
    }

    //创建接收线程和发送线程
    int32_t activateResult = activate(
         THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED,
         (int32_t)runThreadNum,
         0,
         ACE_DEFAULT_THREAD_PRIORITY,
         -1,
         0,
         0,
         0,
         stack_size,
         0 );

    SVS_DELETE( stack_size, SVS_DELETE_MULTI );

    return activateResult;
}

int32_t SVS_Console_Server::svc(void)
{
    (void)signal(SIGPIPE, SIG_IGN);

    ACE_hthread_t currThreadID = (ACE_hthread_t)ACE_OS::thr_self();

    SVS_LOG((SVS_LM_WARNING,
        "[CSL]The current thread id is [%u, %d]",
        currThreadID,
        currThreadID ));

    int32_t threadIndex = addIndex();
    switch(threadIndex)
    {
        // 连接反应器线程
        case SVS_CONSOLE_CONNECT_THREAD:
        {
            return startConnect();
        }

        // 定时器任务处理线程
        case SVS_TIMER_CALLBACK_THREAD:
        {
            return timerCallbackThread();
        }

        // 消息处理线程
        case SVS_HANDLE_MESSAGE_THREAD:
        {
            return handleMessageThread();
        }

        default:
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Thread Index error.threadIndex[%u]",
                threadIndex));

            break;
        }
    }

    return SVS_RESULT_OK;
}

int32_t SVS_Console_Server::close(u_long)
{
    m_bRunning = false;

    // 关闭消息处理队列
    SVS_LOG((SVS_LM_WARNING,
        "[CSL]Close queue of handling message from Console."));
    (void)msg_queue()->close();

    // 关闭定时器队列
    SVS_LOG((SVS_LM_WARNING,
        "[CSL]Close queue of handling message from Console."));
    (void)m_timerCallbackQueue.close();

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Close the communicating with Console thread." ));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : initConfig
功能描述  : 读取连接控制台的相关配置
输入参数  : const char* const strConfigPath:配置文件路径
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Console_Server::initConfig(const char* const strConfigPath)
{
    if(NULL == strConfigPath)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Initialize Console configuration failed."
            "The parameter strConfigPath[0x%08x]",
            strConfigPath));

        return SVS_RESULT_FAILURE;
    }

    char strBuf[512];
    char *endPtr = NULL;
    CAC_Config sys_config((char *)strConfigPath);

    // 连接控制台的开关,默认不连接控制台
    m_ulConnectConsoleSwitch = 1;
    if (sys_config.get("SVS_Console", "ConnectConsoleSwitch", strBuf) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to read the item [SVS_Console][ConnectConsoleSwitch] from configuration file."
            "Use default value[%u]:off.configuration file name[%s].",
            m_ulConnectConsoleSwitch,
            strConfigPath));
//
//        return SVS_RESULT_FAILURE;
    }
    else
    {
        m_ulConnectConsoleSwitch = (uint16_t)ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }
    if(1 == m_ulConnectConsoleSwitch)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The switch for connecting with is off.m_ulConnectConsoleSwitch[%u]",
            m_ulConnectConsoleSwitch));

        return SVS_RESULT_OK;
    }
    else if(0 == m_ulConnectConsoleSwitch)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The switch for connecting with is on.m_ulConnectConsoleSwitch[%u]",
            m_ulConnectConsoleSwitch));
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,
            "The value of the switch for connecting with is invalid."
            "m_ulConnectConsoleSwitch[%u], use default value:off.",
            m_ulConnectConsoleSwitch));

        m_ulConnectConsoleSwitch = 1;
        return SVS_RESULT_OK;
    }

    // console的地址
    if (sys_config.get("SVS_Console", "ConsoleIp", strBuf) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to read the item [SVS_Console][ConsoleIp] from configuration file."
            "configuration file name[%s].",
            strConfigPath));
        return false;
    }
    snprintf(m_strConsoleIp,sizeof(m_strConsoleIp), "%s", strBuf);

    if (sys_config.get("SVS_Console", "ConsolePort", strBuf) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to read the item [SVS_Console][ConsolePort] from configuration file."
            "configuration file name[%s].",
            strConfigPath));
        return false;
    }
    m_uConsolePort = (uint16_t)ACE_OS::strtoul( strBuf, &endPtr, 10 );

    // 连接console的本地地址
    if (sys_config.get("SVS_Console", "LanIpForConsole", strBuf) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to read the item [SVS_Console][LanIpForConsole] from configuration file."
            "configuration file name[%s].",
            strConfigPath));
        return false;
    }
    snprintf(m_strLanIp,sizeof(m_strLanIp), "%s", strBuf);
//    m_szRegMsg.SERVICE_IP = inet_addr(m_strLanIp);
    m_szRegMsg.SERVICE_IP = inet_addr(CAC_RT_Record_Server::instance()->str_local_ip());

    if (sys_config.get("SVS_Console", "LanPortForConsole", strBuf) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to read the item [SVS_Console][LanPortForConsole] from configuration file."
            "configuration file name[%s].",
            strConfigPath));
        return false;
    }
    m_uLanPort = (uint16_t)ACE_OS::strtoul( strBuf, &endPtr, 10 );

    // 服务器ID
    if(sys_config.get("RECORD_SERVER", "ID", strBuf) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to read the item [RECORD_SERVER][ID] from configuration file."
            "configuration file name[%s].",
            strConfigPath));

        return false;
    }
    memcpy(m_szRegMsg.SERVICE_ID, strBuf, sizeof(m_szRegMsg.SERVICE_ID));

    // 服务器类型
    m_szRegMsg.SERVICE_TYPE = htonl(SVS_MRU_RECORD_MODULE_TYPE);

    // 服务器名称
    if(sys_config.get("RECORD_SERVER", "Name", strBuf) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to read the item [RECORD_SERVER][Name] from configuration file."
            "configuration file name[%s].",
            strConfigPath));

        return false;
    }
    (void)snprintf((char*)m_szRegMsg.SERVICE_NAME, sizeof(m_szRegMsg.SERVICE_NAME), "%s", strBuf);

    // POP ID
    if(sys_config.get("RECORD_SERVER", "NodeID", strBuf) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to read the item [RECORD_SERVER][NodeID] from configuration file."
            "configuration file name[%s].",
            strConfigPath));

        return false;
    }
    memcpy(m_szRegMsg.NODE_ID, strBuf, sizeof(m_szRegMsg.NODE_ID));

    // POP 名称
    if(sys_config.get("SVS_AAP", "PopName", strBuf) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to read the item [SVS_AAP][PopName] from configuration file."
            "configuration file name[%s].",
            strConfigPath));

        return false;
    }
    (void)snprintf((char*)m_szRegMsg.NODE_NAME, sizeof(m_szRegMsg.NODE_NAME), "%s", strBuf);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : start_connect
 功能描述  : 线程:录像管理服务器通讯服务处理线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Console_Server::startConnect()
{
    SVS_LOG((SVS_LM_WARNING,
        "[CSL]The thread for communicating with Console start running..."));

    if( NULL == m_pConnectReactor)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "[CSL]The reactor for communicating with Console is NULL."
            "The thread for communicating with Console exited."));

        return SVS_RESULT_FAILURE;
    }

    CThread_Stat_Reporter reporter("CommunicatingWithConsoleThread");
    (void)m_pConnectReactor->owner(ACE_OS::thr_self());

    int32_t ret = 0;
    while (m_bRunning)
    {
        ACE_Time_Value tv(1);
        ret = m_pConnectReactor->handle_events(tv);
        if ( 0 > ret )
        {
            SVS_LOG((SVS_LM_WARNING,
                "[CSL]The reactor for communicating with Console handle event failed."
                "m_pconnect_reactor[0x%08x].",
                m_pConnectReactor));

            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();
    }

    SVS_LOG((SVS_LM_WARNING,
        "[CSL]The thread for communicating with Console exited."));

    return ret;
}

/*****************************************************************************
 函 数 名  : handleMessageThread上
 功能描述  : 消息处理线程。从消息队列中取出消息处理。
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Console_Server::handleMessageThread()
{
    SVS_LOG((SVS_LM_WARNING,
        "[CSL]The thread for processing message from Console start running..."));

    ACE_Message_Block *mb = NULL;
    ACE_Message_Block *mbHandle = NULL;
    char threadName[MAX_THREAD_NAME] = {0};
    (void)snprintf(threadName,sizeof(threadName),"ProcessMessageFromConsoleThread");
    CThread_Stat_Reporter reporter(threadName);
    ACE_Time_Value tvDelay(1);

    while ( m_bRunning)
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;

        if ( -1 == getq(mb,&tv) )            //等待消息
        {
            // 如果不是超时
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "[CSL]Failed to get message from Console message queue."));
                (void)ACE_OS::sleep(1);
                mb = NULL;
            }
            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();

        //消息批量处理
        while ( m_bRunning && (NULL != mb) )
        {
            mbHandle = mb;
            mb = mb->cont();

            mbHandle->cont( NULL );

            //分发处理消息
            handleMessage(mbHandle);

            delete mbHandle;
            mbHandle = NULL;
        }
    }

    SVS_LOG((SVS_LM_WARNING,
        "[CSL]The thread of processing message from Console exited."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : timerCallbackThread
 功能描述  : 定时器回调任务处理线程。从定时器任务队列中取出定时器任务处理。
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Console_Server::timerCallbackThread()
{
    SVS_LOG((SVS_LM_WARNING,
        "[CSL]The thread for timer callback start running..."));

    ACE_Message_Block *mb = NULL;
    SVS_TIMER_CALL_BACK_MSG *pMsg = NULL;
    char threadName[MAX_THREAD_NAME] = {0};
    (void)snprintf(threadName,sizeof(threadName),"TimerCallbackForConsoleThread");
    CThread_Stat_Reporter reporter(threadName);
    ACE_Time_Value tvDelay(1);

    while ( m_bRunning)
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;

        if ( -1 == m_timerCallbackQueue.dequeue_head(mb,&tv) )            //等待消息
        {
            // 如果不是超时
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "[CSL]Failed to get timer callback task for Console from queue."));
                (void)ACE_OS::sleep(1);
                mb = NULL;
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();

        //回调处理
        pMsg = (SVS_TIMER_CALL_BACK_MSG *)(void*)mb->rd_ptr();
        switch ( pMsg->MsgType )
        {
            case SVS_TIMER_CALL_BACK_MSG::RECORD_TRACE_TYPE:
                {
                    SVS_Record_Trace_Manager*pManager =
                        (SVS_Record_Trace_Manager*)pMsg->ProcObject;
                    (void)pManager->onTimer( pMsg->EventId, pMsg->TimerId,pMsg->Dummy);
                }

                break;

            default:
                SVS_LOG((SVS_LM_ERROR,
                    "[CSL]Process timer callback task failed.Type[%d]",
                    pMsg->MsgType));

            break;
        }

        // 释放消息块内存
        delete mb;
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING,
        "[CSL]The thread of processing message from Console exited."));

    return SVS_RESULT_OK;
}

// 处理来自控制台的信令消息
int32_t SVS_Console_Server::handleMessage(const ACE_Message_Block *mb)
{
    SVS_Record_Trace_Manager *pRecordTraceManager = SVS_Record_Trace_Manager::instance();

    PSVS_HEADER pHdr = (PSVS_HEADER)(void*)mb->rd_ptr();

    SVS_LOG((SVS_LM_INFO,
        "[MSG][CSL]Received message from Console."
        "msgtype[0x%04x], transmission No[0x%04x], packet length[%u].",
        pHdr->MsgType,
        pHdr->TransactionNo,
        pHdr->PacketLength));

    switch(pHdr->MsgType)
    {
        // 录像质量跟踪策略同步请求消息
        case RECORD_QUALITY_POLICY_SYN_REQ:
            (void)pRecordTraceManager->syncRecordTraceStrategy(mb);
            break;

        // NRU定时上报录像质量信息响应消息
        case NRU_RECORD_QUALITY_RESP:
            {
                NRU_RECORD_QUALITY_RESP_MSG* pRespMsg =
                    (NRU_RECORD_QUALITY_RESP_MSG*)(void*)mb->rd_ptr();
                SVS_LOG((SVS_LM_INFO,
                    "[MSG][CSL]Received record quality response message."
                    "result code[%u]",
                    pRespMsg->ERROR_CODE));
            }
            break;

        // 控制台收到镜头录像质量信息完成通知消息后，返回响应
        case NRU_RECORD_QUALITY_DONE_RESP:
            {
                NRU_RECORD_QUALITY_DONE_RESP_MSG* pDoneRespMsg =
                    (NRU_RECORD_QUALITY_DONE_RESP_MSG*)(void*)mb->rd_ptr();
                SVS_LOG((SVS_LM_INFO,
                    "[MSG][CSL]Received record quality response message."
                    "result code[%u]",
                    pDoneRespMsg->ERROR_CODE));
            }
            break;
        default:
            SVS_LOG((SVS_LM_WARNING,
                "[MSG][CSL]Received invalid message form Console, "
                "Unknown msgtype[0x%04x].",
                pHdr->MsgType));
            break;
    }

    SVS_LOG((SVS_LM_INFO,
        "[MSG][CSL]Process message from Console finished, "
        "msgtype[0x%04x] transmission No[0x%04x] pachet length[%u].",
        pHdr->MsgType,
        pHdr->TransactionNo,
        pHdr->PacketLength));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : addTimerCallbackMsg
 功能描述  : 把定时器任务加入定时器任务处理队列。
 输入参数  : ACE_Message_Block* msg:定时器任务
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Console_Server::addTimerCallbackMsg( ACE_Message_Block* msg )
{
    //设置超时时间,防止队列满时死锁
    ACE_Time_Value tv(ACE_OS::gettimeofday());
    tv.set(tv.sec() + SVS_ENQUEUE_TIMEOUT, 0);

    int32_t result = m_timerCallbackQueue.enqueue_tail( msg, &tv );

    //入队列失败处理
    if ( SVS_RESULT_FAILURE == result )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "[CSL]Fail to put message to timer callback enqueue."
            "queue full flag[%d] state[%d] msg count[%u] len[%u] size[%u].",
            m_timerCallbackQueue.is_full(),
            m_timerCallbackQueue.state(),
            m_timerCallbackQueue.message_count(),
            m_timerCallbackQueue.message_length(),
            m_timerCallbackQueue.message_bytes() ));
        return SVS_RESULT_FAILURE;
    }

    return result;
}

/*****************************************************************************
 函 数 名  : sendMsg
 功能描述  : 对外的发现消息接口。把待发送的消息加入消息发送队列。
 输入参数  : ACE_Message_Block *mb:要发送的消息
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Console_Server::sendMsg(ACE_Message_Block *mb)
{
    if ( NULL == m_pClientHandler)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[SEND][CSL]Send message to Console failed, p_connector_ is null."));

        return SVS_RESULT_FAILURE;
    }

    ACE_Time_Value tvDelay(0, 100 * 1000);
    ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;

    if (-1 == m_pClientHandler->send_msg(mb, &tvTimeout))
    {
        SVS_LOG((SVS_LM_ERROR, "[SEND][CSL]message enqueue failed."));

        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}
//=================SVS_Console_Server END=====================================

