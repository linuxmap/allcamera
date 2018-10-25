#include "svs_ace_header.h"
#include "vms/vms.h"
#include "svs_private_msg.h"
#include "svs_stat_manager.h"
#include "svs_client_handler.h"
#include "svs_log_msg.h"
#include "svs_mb_buffer.h"
#include "svs_real_record_task.h"
#include "svs_real_record_server.h"

extern int32_t g_alarmStatus[ALARM_INDEX_NUM];

// 服务器类型列表
const char* g_strRemoteServerName[] = { "StoreSvr", "Console", "\0"};

CSVS_Client_Handler::CSVS_Client_Handler()
{
    (void)construct();
}

CSVS_Client_Handler::CSVS_Client_Handler( ACE_Reactor *pReactor,
                                          ACE_Message_Queue<ACE_SYNCH> *pMsgQueue,
                                          uint32_t nServerType)
    : ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_MT_SYNCH>(NULL, NULL, pReactor),
      thr_index_(0),
      server_type_(nServerType)
{
    (void)construct();

    p_signal_queue_ = pMsgQueue;
}

CSVS_Client_Handler::~CSVS_Client_Handler()
{
}

/*****************************************************************************
函 数 名  : construct
功能描述  : 初始化成员变量
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::construct()
{
    brunning_       = true;
    p_signal_queue_ = NULL;

    // 应用层心跳消息
    heartbeat_timeout_times_ = 0;
    new_loggin_time_ = 0;
    heartbeat_transaction_no_ = 0;

    recv_data_len_  = 0;

    send_msg_buffer_ = NULL;
    connect_fail_count_ = 0;
    conn_status_ = RT_CONN_STORE_INIT;

    record_status_ = RT_MONITOR_EVENT_STOP_RECORD;

    return 0;
}
/*****************************************************************************
函 数 名  : open
功能描述  : 初始化，启动发送线程，注册反应器
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::open(void *pArg)
{

    if (NULL == pArg)
    {
        ; //only for compile warnning
    }

    SVS_TRACE();
    SVS_LOG((SVS_LM_DEBUG,
        "Open CSVS_Client_Handler for %s. reactor[0x%08x], pArg[0x%08x].",
        g_strRemoteServerName[server_type_],
        reactor(),
        pArg ));

    // 应用层心跳
    ACE_Time_Value tv(HEARTBEAT_SEND_INTERVAL, 0);
    int32_t time_id_ = reactor()->schedule_timer(this, NULL, tv, tv);
    if (-1 == time_id_)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Set timer for heartbeat with %s failed.",
            g_strRemoteServerName[server_type_]));

        return -1;
    }

    // 消息队列水位
    (void)msg_queue()->close();
    if (0 != msg_queue()->open(SVS_SEND_MSG_QUEUE_WM, SVS_SEND_MSG_QUEUE_WM))
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Fail to open send msg queue for %s.",
            g_strRemoteServerName[server_type_]) );

        return -1;
    }

    if(SVS_RESULT_OK != set_conn_status(RT_CONN_STORE_DISCONNECTED))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to set connection status with %s,"
            "old status[%d], new status[%d].",
            g_strRemoteServerName[server_type_],
            conn_status_,
            RT_CONN_STORE_DISCONNECTED));

        return -1;
    }

    // 创建线程
    size_t stack_size[RT_HANDLE_CLIENT_MAX_THREAD];
    for(int32_t i = 0; i < RT_HANDLE_CLIENT_MAX_THREAD; i++ )
    {
       stack_size[i] =  SVS_THREAD_STATCK_MAX;
    }

    int32_t nRet = activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED,
                        RT_HANDLE_CLIENT_MAX_THREAD,
                        0,
                        ACE_DEFAULT_THREAD_PRIORITY,
                        -1,
                        0,
                        0,
                        0,
                        stack_size,
                        0 );
    if (0 != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Activate handle client thread for %s failed.",
            g_strRemoteServerName[server_type_]));
    }

    return nRet;
}

/*****************************************************************************
函 数 名  : svc
功能描述  : 发送消息线程函数，用于发送消息给StoreSvr
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::svc()
{
    g_p_msg_buffer->init();

    ACE_hthread_t currThreadID = (ACE_hthread_t)ACE_OS::thr_self();
    SVS_LOG((SVS_LM_WARNING, "Current thread ID[%u,%d]",currThreadID, currThreadID ));

    (void)signal(SIGPIPE, SIG_IGN);
    int32_t threadIndex = add_index();
    int32_t ret = SVS_RESULT_OK;
    switch(threadIndex)
    {
        case RT_HANDLE_CLIENT_SEND_MSG_THREAD:
        {
            ret = send_message_thread();
        }
        break;
        case RT_HANDLE_CLIENT_CHECK_CONNECT_THREAD:
        {
            ret = check_connection_status_thread();
        }
        break;
        default:
        {
            SVS_LOG((SVS_LM_CRITICAL, "The thread id is unknown ."));
        }
        break;
    }
    g_p_msg_buffer->clear();
    return ret;
}

/*****************************************************************************
函 数 名  : handle_input
功能描述  : 从StoreSvr接收数据
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::handle_input(ACE_HANDLE /*handle*/)
{
    SVS_TRACE();

    // 接收数据
    ACE_Time_Value tv(0, RECV_DATA_MAX_TIMEOUT );
    ssize_t recvResult = peer().recv(  &recv_data_buffer_[recv_data_len_],
                                        RECV_DATA_BUFF_MAX_LEN - recv_data_len_,
                                        &tv);

    if( 0 >= recvResult)
    {
        int32_t iErrorCode = ACE_OS::last_error();

        if ( (EAGAIN == iErrorCode)
            || (ETIME == iErrorCode)
            || (EWOULDBLOCK == iErrorCode)
            || (ETIMEDOUT == iErrorCode)
            || (EINTR == iErrorCode) )
        {
            return 0;
        }

        SVS_LOG( (SVS_LM_ERROR,
            "Fail to recv data from %s. return code[%d].",
            g_strRemoteServerName[server_type_],
            recvResult ) );
        (void)disconnection();
        return 0;
    }

    // 更改接收数据长度
    recv_data_len_ += (uint32_t)recvResult;

    // 解析数据
    int32_t parseResult = parse_data();
    if ( 0 != parseResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to parse data recieved from %s.",
            g_strRemoteServerName[server_type_]));

        (void)disconnection();
        return 0;
    }

    return 0;
}

/*****************************************************************************
 函 数 名  : handle_timeout
 功能描述  : 和存储控制服务器的心跳服务器超时回调函数,向调度发送心跳消息,
             如果连续三次未接收到存储控制服务器的心跳响应消息,重连存储控制服务器
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功发送心跳消息,返回0 发生错误,返回-1
 修改历史      :
 1. 日    期   : 2009年8月20日
    修改内容   : 创建
*****************************************************************************/
int32_t CSVS_Client_Handler::handle_timeout(const ACE_Time_Value & /*currentTime*/, const void * /*act = 0*/)
{
    SVS_TRACE();
    //非连接状态不检测
    if( RT_CONN_STORE_READY != conn_status_)
    {
        return 0;
    }

    if(0 < heartbeat_timeout_times_)
    {
        signal_msg_stat_.addHeartbeatTimeoutTimes(1);
        SVS_LOG((SVS_LM_INFO,
            "Heartbeat with %s timeout. times[%d]",
            g_strRemoteServerName[server_type_],
            heartbeat_timeout_times_));
    }

    heartbeat_timeout_times_++;
    if (heartbeat_timeout_times_ > HEARTBEAT_CHECK_INTERVAL/HEARTBEAT_SEND_INTERVAL)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Disconnected from %s. Havn't recv heartbeat [%d] times.",
            g_strRemoteServerName[server_type_],
            heartbeat_timeout_times_ - 1));

        heartbeat_timeout_times_ = 0;
        return disconnection();
    }

    //生成心跳消息
    uint16_t respMsgLen = sizeof(SVS_HEADER);

    ACE_Message_Block *respMsgMb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, respMsgLen );
    if ( NULL == respMsgMb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create heartbeat message for %s failed. Fail to allocate message block."
            "mb type[%d] size[%d]",
            g_strRemoteServerName[server_type_],
            CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
            respMsgLen ));

        return -1;
    }
    SVS_HEADER* pRespMsg = (SVS_HEADER*)(void*)respMsgMb->wr_ptr();
    respMsgMb->wr_ptr( respMsgLen );

    heartbeat_transaction_no_++;
    if( heartbeat_transaction_no_ > 0x7fff )
    {
        heartbeat_transaction_no_ = 1;
    }
    FillCommonHeader(   *pRespMsg,
                        SVS_MSG_TYPE_HEARTBEAT,
                        heartbeat_transaction_no_,
                        respMsgLen );

    SVS_LOG((SVS_LM_DEBUG,
        "Send heartbeat message to %s.",
        g_strRemoteServerName[server_type_]));

    // 把心跳消息加入队头
    ACE_Time_Value tv(ACE_OS::gettimeofday());
    tv.sec(tv.sec() + 10);
    if ( -1 == msg_queue()->enqueue_head(respMsgMb, &tv))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[SEND]Heartbeat message to %s enqueue failed.",
            g_strRemoteServerName[server_type_]));
    }

    return 0;
}

int32_t CSVS_Client_Handler::handle_close(ACE_HANDLE handle, ACE_Reactor_Mask)
{
    if ( ACE_INVALID_HANDLE == handle )
    {
        return 0;
    }

    SVS_LOG( (SVS_LM_DEBUG, "close client handler for %s.", g_strRemoteServerName[server_type_]) );

    return 0;
}

/*****************************************************************************
函 数 名  : build_msg
功能描述  : 根据消息头缓冲区，生成消息块
输入参数  : pMsgHeader  :   消息头指针
            mbType      :   需要生成的消息块类型
输出参数  : 无
返 回 值  : 成功,返回 生成的消息块指针 否则返回NULL
修改历史  :
*****************************************************************************/
ACE_Message_Block* CSVS_Client_Handler::build_msg(const SVS_HEADER* pMsgHeader,
                                                    uint8_t   mbType ) const
{
    SVS_TRACE();

    uint32_t msgBlockLen = pMsgHeader->PacketLength;

    // 申请消息缓冲区
    ACE_Message_Block *mb = alloc_mb( msgBlockLen, mbType);
    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Allocating message for %s block failed. msg type[%u].",
            g_strRemoteServerName[server_type_],
            mbType));

        return NULL;
    }

    (void)ACE_OS::memcpy( mb->wr_ptr(), (void*)pMsgHeader, msgBlockLen );

    mb->wr_ptr( msgBlockLen );

    return mb;
}

/*****************************************************************************
函 数 名  : parse_data
功能描述  : 对接收到的信令数据进行解析，分解出单个的消息
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::parse_data()
{
    SVS_TRACE();

    // 数据不够消息头长度，继续读取数据
    if ( recv_data_len_ < sizeof(SVS_HEADER) )
    {
        return 0;
    }

    // 分解出每个消息
    uint32_t procDataPos = 0;
    uint32_t nextDataPos = 0;
    SVS_HEADER *pMsgHeader = NULL;
    ACE_Time_Value tv;

    ACE_Message_Block *mb = NULL;
    ACE_Message_Block *mbBatch = NULL;
    ACE_Message_Block *mbCont = NULL;
    uint32_t msgCount = 0;
    int32_t iRet = 0;
    while ( procDataPos < recv_data_len_ )
    {
        iRet = check_data(procDataPos,nextDataPos,pMsgHeader);

        // 继续接收
        if(HANDLE_RECV_DATA_BREAK== iRet)
        {
            break;
        }
        else if(HANDLE_RECV_DATA_ERROR == iRet)// 接收到的数据有错误
        {
            return SVS_RESULT_FAILURE;
        }
        else if(HANDLE_RECV_DATA_CONTINUE == iRet)  // 继续处理其它信令消息
        {
            //心跳计数清0
            heartbeat_timeout_times_ = 0;
            procDataPos = nextDataPos;
            continue;
        }

        //心跳计数清0
        heartbeat_timeout_times_ = 0;
        ++msgCount;
        mb = NULL;
        SVS_LOG((SVS_LM_INFO,
            "[RECV]Receice message from from %s."
            " msg type[0x%04x], transNo[0x%04x], packet len[%d].",
            g_strRemoteServerName[server_type_],
            pMsgHeader->MsgType,
            pMsgHeader->TransactionNo,
            pMsgHeader->PacketLength ));

        switch (pMsgHeader->MsgType)
        {
        case SVS_MSG_TYPE_HEARTBEAT:    //心跳消息处理
            {
                //心跳消息无需入消息队列处理，直接返回
                procDataPos = nextDataPos;
                continue;
            }
            // break;

        case SVS_ALARM_KINESCOPE_CONTROL_REQ:   //告警事件消息处理
            {
                mb = build_msg( pMsgHeader, CSVS_Mb_Buffer_Manager::MB_TYPE_ALARM_EVENT );
            }
            break;
        default:    //其它消息
            {
                mb = build_msg( pMsgHeader, CSVS_Mb_Buffer_Manager::MB_TYPE_NORMAL_EVENT );
            }
            break;
        }

        if ( NULL != mb )
        {
            //将收到的消息组成一个消息串，批量压入消息处理队列中
            if ( NULL == mbBatch )
            {
                mbBatch = mb;
                mbCont = mb;
            }
            else
            {
                if ( NULL != mbCont )
                {
                    mbCont->cont( mb );
                }
                mbCont = mb;
            }
        }
        procDataPos = nextDataPos;
    }

    // 把解析好的消息，加入消息处理队列等待处理
    if(SVS_RESULT_OK != equeue_msg(mbBatch, tv))
    {
        return SVS_RESULT_FAILURE;
    }

    // 增加统计信息
    signal_msg_stat_.addSignalMessage(msgCount);

    // 更新缓存区游标位置
    return update_buffer_cursor(procDataPos,nextDataPos);
}

/*****************************************************************************
函 数 名  : send_message_thread
功能描述  : 发送消息线程。从发现消息队列中获取数据，发送到连接对应的server。对端
            可以是StoreSvr，也可以是Console.
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::send_message_thread()
{
    SVS_LOG( (SVS_LM_WARNING,
        "Send message thread for %s start to running..." ,
        g_strRemoteServerName[server_type_]));

    ACE_Message_Block *mb = NULL;
    uint32_t sendDataCount = 0;
    while ( brunning_ )
    {
        // ----可优化点 批量发送-----
        if (-1 == getq(mb) )
        {
            if(ESHUTDOWN == ACE_OS::last_error())
            {
                // 出错，重新打开队列
                SVS_LOG( (SVS_LM_CRITICAL,
                    "Fail to get message from message queue for %s.Reopen again.",
                    g_strRemoteServerName[server_type_]));
                (void)msg_queue()->close();
                if (0 != msg_queue()->open(SVS_SEND_MSG_QUEUE_WM, SVS_SEND_MSG_QUEUE_WM))
                {
                    SVS_LOG( (SVS_LM_ERROR,
                        "Fail to open send message queue for %s.",
                        g_strRemoteServerName[server_type_]) );
                }
            }
            continue;
        }

        PSVS_HEADER pHdr = (PSVS_HEADER)(void*)mb->rd_ptr();

        // 如果是非连接状态，则发送会不成功，不用发送了，直接释放消息内存
        if(RT_CONN_STORE_READY != conn_status_)
        {
            SVS_LOG( (SVS_LM_INFO,
                "Send message to %s failed."
                "Connection status is not READY."
                "current status[%d],msg type[0x%04x].",
                g_strRemoteServerName[server_type_],
                conn_status_,
                pHdr->MsgType) );

            // 释放消息缓冲区
            free_mb(mb );
            continue;
        }

        // 打印发送的消息基本信息
        ACE_INET_Addr remoteAddr;
        (void)peer().get_remote_addr( remoteAddr );

        SVS_LOG((SVS_LM_INFO,
            "[SEND]Send message to %s, "
            "%s addr[%s:%d], "
            "msg type[0x%04x], transNo[0x%04x], msg len[%d], mb.length[%d].",
            g_strRemoteServerName[server_type_],
            g_strRemoteServerName[server_type_],
            remoteAddr.get_host_addr(),
            remoteAddr.get_port_number (),
            pHdr->MsgType,
            pHdr->TransactionNo,
            pHdr->PacketLength,
            mb->length()));

        // 发送消息
        ACE_Time_Value tv(SVS_SEND_MSG_TIMEOUT, 0);
        int32_t sendResult = peer().send_n(mb->rd_ptr(), mb->length(), &tv);
        if ( ( -1 == sendResult ) && ( EAGAIN == ACE_OS::last_error() ) )
        {
            // 发送失败返回EAGAIN错误处理,重发一次
            sendResult = peer().send_n(mb->rd_ptr(), mb->length(), &tv);
        }

        // 发送消息个数统计
        ++sendDataCount;
        if (0 == sendDataCount%10000 )
        {
            SVS_LOG((SVS_LM_DEBUG,
                "Send message to %s count[%d].",
                g_strRemoteServerName[server_type_],
                sendDataCount));
        }

        // 发送消息失败
        if (mb->length() != (uint32_t)sendResult )
        {
            SVS_LOG( (SVS_LM_ERROR,
                "Fail to send message to %s. return code[%d].",
                g_strRemoteServerName[server_type_],
                sendResult ) );
            (void)this->disconnection();
        }

        // 释放消息缓冲区
        free_mb(mb );
    }

    SVS_LOG( (SVS_LM_WARNING,
        "Send message thread for %s exit.",
        g_strRemoteServerName[server_type_]));

    return 0;
}

/*****************************************************************************
函 数 名  : send_message_thread
功能描述  : 发送消息线程。从发现消息队列中获取数据，发送到连接对应的server。对端
            可以是StoreSvr，也可以是Console.
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::check_connection_status_thread()
{
    SVS_LOG( (SVS_LM_WARNING,
        "Check connection status with %s thread start to running...",
        g_strRemoteServerName[server_type_]));

    while ( brunning_ )
    {
        // 如果状态正常
        if((RT_CONN_STORE_INIT == conn_status_) || (RT_CONN_STORE_READY == conn_status_))
        {
            (void) usleep(10000);
            continue;
        }

        // 当连接状态为初始化或者断开时，重新连接
        if(RT_CONN_STORE_DISCONNECTED == conn_status_)
        {
            if ((SVS_RESULT_OK != reconnect()) || (SVS_RESULT_OK != register2server()))
            {
                (void)disconnection();

                // 连接服务器不成功，sleep一段时间后，再次重连
                (void) sleep(3);

                continue;
            }
        }

        // 如果是logging状态，判断是否是否等待超时
        if(RT_CONN_STORE_LOGGING == conn_status_)
        {
            time_t currTime = time(NULL);

            // 如果很久没有收到响应消息，则断开重连
            if(3 < (currTime - new_loggin_time_))
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Logging in status is over time.So disconnect from %s."
                    "current time [%u],last logging in time [%u].",
                    g_strRemoteServerName[server_type_],
                    currTime,
                    new_loggin_time_));
                (void)disconnection();
            }
        }

        (void) usleep(10000);
    }

    SVS_LOG( (SVS_LM_WARNING,
        "Check connection status with %s thread exit.",
        g_strRemoteServerName[server_type_]));

    return 0;
}

/*****************************************************************************
函 数 名  : check_data
功能描述  : 检查当前收到的数据的合法性，如果还没有收完则继续接收
输入参数  : 无
输出参数  : 无
返 回 值  : 已经接收到的数据解析失败 返回-1,成功,返回 0,还需要接续接收返回1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::check_data(const uint32_t procDataPos,
                                          uint32_t& nextDataPos,
                                          SVS_HEADER*&pMsgHeader)
{
    if ((recv_data_len_-procDataPos)  < sizeof(SVS_HEADER))
    {
        return HANDLE_RECV_DATA_BREAK;
    }

    pMsgHeader = (SVS_HEADER *)(void*)&recv_data_buffer_[procDataPos];
    nextDataPos = pMsgHeader->PacketLength + procDataPos;

    // 检查消息是否接收完全
    if ( nextDataPos > recv_data_len_ )
    {
        return HANDLE_RECV_DATA_BREAK;
    }

    if( (SVS_PROTOCOL_VERSION != pMsgHeader->ProtocolVersion )
        || (sizeof(SVS_HEADER) > pMsgHeader->PacketLength) )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "[RECV]Message header is invalid.Message from %s."
            "protocal version[%u], packet len[%u].",
            g_strRemoteServerName[server_type_],
            pMsgHeader->ProtocolVersion,
            pMsgHeader->PacketLength ) );
        return HANDLE_RECV_DATA_ERROR;
    }

    int32_t iRet = handle_register_response_msg(pMsgHeader);

    return iRet;
}

/*****************************************************************************
函 数 名  : equeue_msg
功能描述  : 把解析完整的消息块链表加入队列中，等待处理
输入参数  : ACE_Message_Block * mbBatch:消息块链表
            ACE_Time_Value tv:加入队列超时等待时长
输出参数  : 无
返 回 值  : 成功:0 失败:-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::equeue_msg(ACE_Message_Block * mbBatch, ACE_Time_Value tv)
{
    if ( NULL == mbBatch )
    {
        return SVS_RESULT_OK;
    }

    // 统一释放资源
    do
    {
        if ( NULL == p_signal_queue_ )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Fail to put message to queue for %s. "
                "The queue pointer p_signal_queue_ is NULL.",
                g_strRemoteServerName[server_type_]));

            // 队列异常，需要释放消息内存块
            break;
        }

        tv.set(ACE_OS::gettimeofday().sec() + 1, 0);
        if(-1 == p_signal_queue_->enqueue_tail(mbBatch, &tv))
        {
            SVS_LOG( (SVS_LM_ERROR,
                "[CSL]Fail to put message queue for %s. "
                "queue full flag[%d], state[%d], msg count[%u], msg len[%u], msg size[%u].",
                g_strRemoteServerName[server_type_],
                p_signal_queue_->is_full(),
                p_signal_queue_->state(),
                p_signal_queue_->message_count(),
                p_signal_queue_->message_length(),
                p_signal_queue_->message_bytes() ));
        }

        return SVS_RESULT_OK;

    }while(0);

    // 消息加入队列失败，释放资源，消息批量处理
    ACE_Message_Block *mbTemp = NULL;
    while (NULL != mbBatch)
    {
        mbTemp = mbBatch;
        mbBatch = mbBatch->cont();

        mbTemp->cont( NULL );

        free_mb(mbTemp);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : update_buffer_cursor
功能描述  : 把未解析完的数据移到缓存区的开始位置，等待下次继续接收完整再一起解析
输入参数  : uint32_t procDataPos:当前数据在缓存区中的指针偏移位置
            uint32_t nextDataPos:下个消息开始偏移
输出参数  : 无
返 回 值  : 成功:0 失败:-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::update_buffer_cursor(uint32_t procDataPos,
                                                    uint32_t nextDataPos)
{
    SVS_TRACE();

    // 第一个消息还未收全，直接返回
    if ( 0 == procDataPos )
    {
        return 0;
    }

    // 消息正好处理完，清接收数据长度后，直接返回
    if ( nextDataPos == recv_data_len_ )
    {
        recv_data_len_ = 0;
        return 0;
    }

    // 缓存区剩余空间
    int32_t leaveDataLen = (int32_t)(recv_data_len_ - procDataPos);

    if ( 0 > leaveDataLen )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Undealed data length is less than 0. "
            "recv data len[%d], "
            "deal data pos[%d], "
            "next data pos[%d].",
            recv_data_len_,
            procDataPos,
            nextDataPos ));
        return 0;
    }

    // 还有未处理完的部分消息，将其移动缓冲区头后，继续接收
    (void)ACE_OS::memmove(    &recv_data_buffer_[0],
                        &recv_data_buffer_[procDataPos],
                        (uint32_t)leaveDataLen );

    recv_data_len_ = (uint32_t)leaveDataLen;

    return 0;

}

/*****************************************************************************
函 数 名  : send_msg
功能描述  : 发送消息结果，对外接口。把要发送的消息加入发送消息列队，交由发送消息
            线程处理。如果和server的连接状态时非连接状态，则发送失败通知外层。
输入参数  : ACE_Message_Block * mb:要发送的消息块
            ACE_Time_Value * timeout:加入队列超时时长设置
输出参数  : 无
返 回 值  : 成功返回0，失败返回-1.
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::send_msg(ACE_Message_Block * mb, ACE_Time_Value * timeout)
{
    // 如果连接状态不正常，则返回失败
    if(conn_status_ != RT_CONN_STORE_READY)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[SEND] message enqueue failed.Connection status with %s is [%d]",
            g_strRemoteServerName[server_type_],
            conn_status_));

        return SVS_RESULT_FAILURE;
    }

    // 把消息加入消息发送队列
    if (-1 == this->putq(mb, timeout))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[SEND] message enqueue for %s failed.put message to queue failed.",
            g_strRemoteServerName[server_type_]));

        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : set_socket_option
功能描述  : 设置socket属性。
输入参数  : NA
输出参数  : NA
返 回 值  : NA
修改历史  :
*****************************************************************************/
void CSVS_Client_Handler::set_socket_option()const
{
    uint32_t nSize = 0;
    int32_t nLen = sizeof(nSize);

    // 设置SOCKET发送缓冲区
    nSize = SVS_SOCKET_BUFF_MAX;
    if (0 !=  peer().set_option(SOL_SOCKET, SO_SNDBUF, (void *)&nSize, sizeof(nSize)))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to set socket send buffer size for %s.",
            g_strRemoteServerName[server_type_]));
    }

    if (0 != peer().get_option(SOL_SOCKET, SO_SNDBUF, (void *)&nSize, &nLen))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to get socket send buffer size for %s.",
            g_strRemoteServerName[server_type_]));
    }
    else
    {
        ACE_OS::last_error(0);
        SVS_LOG((SVS_LM_DEBUG,
            "Success to set socket send buffer for %s. "
            "Sock handler[0x%08x], send buffer size[%d].",
            g_strRemoteServerName[server_type_],
            peer().get_handle(),
            nSize ));
    }

    // 接收缓冲区
    nSize = SVS_SOCKET_BUFF_MAX;
    if (0 !=  peer().set_option(SOL_SOCKET, SO_RCVBUF, (void *)&nSize, sizeof(nSize)))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to set socket recv buffer size for %s.",
            g_strRemoteServerName[server_type_]));
    }

    if (0 != peer().get_option(SOL_SOCKET, SO_RCVBUF, (void *)&nSize, &nLen))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to get socket recv buffer size. for %s",
            g_strRemoteServerName[server_type_]));
    }
    else
    {
        SVS_LOG((SVS_LM_DEBUG,
            "Success to set socket recv buffer size for %s. "
            "Sock handler[0x%08x], recv buffer size[%d].",
            g_strRemoteServerName[server_type_],
            peer().get_handle(),
            nSize ));
    }

    return;
}

/*****************************************************************************
函 数 名  : reconnect
功能描述  : 重连。向server发起连接。如果前面已经连接过了，把自己冲reactor中去注册掉
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::reconnect()
{
    // 如果状态出现问题，纠正处理
    if(RT_CONN_STORE_DISCONNECTED != conn_status_)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Reconnect with %s occur error. Set the connection status to disconnected."
            "old status[%d].",
            g_strRemoteServerName[server_type_],
            conn_status_));

        // 把自己从reactor主去注册
        (void)reactor()->remove_handler(this, ACE_Event_Handler::RWE_MASK
                                            | ACE_Event_Handler::DONT_CALL);
        ACE_OS::last_error(0);

        // 任何状态都可以到该状态，所以可以直接置状态
        conn_status_ = RT_CONN_STORE_DISCONNECTED;
    }

    // 初始化缓存区和socket
    recv_data_len_ = 0;

    CAC_RT_Record_Server * pServer = CAC_RT_Record_Server::instance();
    if(NULL == pServer)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Reconnect with %s failed. server pointor is NULL.",
            g_strRemoteServerName[server_type_]));

        return SVS_RESULT_FAILURE;
    }

    // 如果重连次数超过10次，发送email告警
    const int16_t nMaxFailCount = 10;
    connect_fail_count_++;
    if (nMaxFailCount < connect_fail_count_)
    {
        connect_fail_count_ = 0;
        char strTitle[512];
        char strContent[512];
        (void)ACE_OS::snprintf(   strTitle,
                            sizeof(strTitle),
                            "RtRecordSvr can't reconnect with %s, "
                            "RtRecordSvr[%s,%s:%d],StoreSvr[%s:%d]",
                            g_strRemoteServerName[server_type_],
                            pServer->str_id(),
                            pServer->lan_ip(),
                            pServer->lan_port(),
                            pServer->recordctrl_ip(),
                            pServer->recordctrl_port());

        (void)ACE_OS::snprintf(strContent,
                            sizeof(strContent),
                            "Reconnecting times is [%d]",
                            nMaxFailCount);

        (void)CallShellSendEMail(strTitle, strContent);
    }

    SVS_LOG((SVS_LM_WARNING, "Retry to connect with %s.", g_strRemoteServerName[server_type_]));

    //在开始连接之前将状态设置为"连接中"
    if(SVS_RESULT_OK != set_conn_status(RT_CONN_STORE_CONNECTING))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Set connection status with %s failed."
            "old status[%d], new status[%d]",
            g_strRemoteServerName[server_type_],
            conn_status_,
            RT_CONN_STORE_CONNECTING));

        return SVS_RESULT_FAILURE;
    }

    // 创建新连接
    ACE_SOCK_Connector connector;
    ACE_INET_Addr remoteAddr(pServer->recordctrl_port(),pServer->recordctrl_ip());
    ACE_INET_Addr localAddr(pServer->lan_port(),pServer->lan_ip());

    // 设备连接超时时间
    ACE_Time_Value tv(1, 0);
    if (-1 == connector.connect(this->peer(), remoteAddr, &tv, localAddr, 1))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to reconnect with %s, "
            "RtRecordSvr[%s:%d], StoreSvr[%s:%d].",
            g_strRemoteServerName[server_type_],
            localAddr.get_host_addr(),
            localAddr.get_port_number(),
            remoteAddr.get_host_addr(),
            remoteAddr.get_port_number()));

        //在统计信息中增加连接失败次数
        signal_msg_stat_.addConnStoreCtrlFailTimes(1);
        (void)this->peer().close();

        return SVS_RESULT_FAILURE;
    }

    // 取得对方地址并保存起来
    local_addr_ = localAddr;
    (void)peer().get_remote_addr(remote_addr_);

     char strRemoteIp[SVS_IP_LEN + 1] = {0};
    (void)remote_addr_.get_host_addr(strRemoteIp, sizeof(strRemoteIp));
    uint16_t usRemotePort = remote_addr_.get_port_number();

    char strLocalAddr[SVS_IP_LEN + 1] = {0};
    (void)local_addr_.get_host_addr(strLocalAddr, sizeof(strLocalAddr));
    uint16_t usLocalPort = local_addr_.get_port_number();

     SVS_LOG((SVS_LM_DEBUG,
        "LocalIPAddr[%s],LocalPort[%d],RemoteIPAddr[%s],RemotePort[%d].",
        strLocalAddr,usLocalPort,strRemoteIp,usRemotePort));

    // 设置socket属性
    set_socket_option();

    SVS_LOG((SVS_LM_WARNING,
        "Connect with StoreSvr succeeded, "
        "RtRecordSvr[%s:%d], StoreSvr[%s:%d].",
        localAddr.get_host_addr(),
        localAddr.get_port_number(),
        remoteAddr.get_host_addr(),
        remoteAddr.get_port_number()));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : register2server
功能描述  : 向server注册。首先向reactor注册自己，然后向Server发送注册消息
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::register2server()
{
    SVS_LOG((SVS_LM_WARNING, "Retry to register to %s.", g_strRemoteServerName[server_type_]));

    // 设置状态为 "注册中" 状态
    if(SVS_RESULT_OK != set_conn_status(RT_CONN_STORE_LOGGING))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Set connection status with %s failed."
            "old status[%d], new status[%d]",
            g_strRemoteServerName[server_type_],
            conn_status_,
            RT_CONN_STORE_LOGGING));

        return SVS_RESULT_FAILURE;
    }

    // 向反应器注册读事件
    if(-1 == reactor()->register_handler(this, ACE_Event_Handler::READ_MASK))
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Fail to register client handler for %s. this[0x%08x].",
            g_strRemoteServerName[server_type_],
            this ));

        return SVS_RESULT_FAILURE;
    }

    // 发送注册消息
    if (0 != send_register_msg())
    {
        SVS_LOG((SVS_LM_ERROR, "Register to %s failed.", g_strRemoteServerName[server_type_]));

        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : disconnection
功能描述  : 断开和Server的连接。如果连接状态已经是断开状态了，则断开完成。
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::disconnection()
{
    // 如果连接状态已经是断开状态了，则断开连接成功
    if(RT_CONN_STORE_DISCONNECTED == conn_status_)
    {
        SVS_LOG((SVS_LM_INFO,
            "Needn't to disconnected from %s, Current status is disconnected.",
            g_strRemoteServerName[server_type_]));

        return SVS_RESULT_OK;
    }

    // 增加统计信息
    signal_msg_stat_.addDisconnStoreCtrlTimes(1);
    signal_msg_stat_.updateDisconnStoreCtrlTime(time(NULL));


    SVS_LOG((SVS_LM_WARNING, "Begin to disconnect from %s.",g_strRemoteServerName[server_type_]));

    // 把自己从reactor主去注册
    (void)reactor()->remove_handler(this,ACE_Event_Handler::RWE_MASK | ACE_Event_Handler::DONT_CALL);

    // 创建停止录像消息
    (void)send_monitor_event(RT_MONITOR_EVENT_STOP_RECORD);


    // 断开连接
    (void)ACE_OS::shutdown(this->peer().get_handle(), SHUT_RDWR);
    (void)this->peer().close();
    this->peer().set_handle(ACE_INVALID_HANDLE);

    if(SVS_RESULT_OK != set_conn_status(RT_CONN_STORE_DISCONNECTED))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Set connection status with %s failed."
            "old status[%d], new status[%d].",
            g_strRemoteServerName[server_type_],
            conn_status_,
            RT_CONN_STORE_DISCONNECTED));
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : send_monitor_event
功能描述  : 发送monitor事件给事件处理线程。
输入参数  : int32_t monitorEventType:事件类型，可以是启动录像，也可以是停止录像。
            在和StoreSvr断开的时候需要停止所有录像，所以发送停止录像事件给事件
            监控线程
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::send_monitor_event(int32_t monitorEventType)
{
    // 判断当前的录像状态，如果当前的录像状态和monitor事件类型相同，则不用再发送事件
    if(record_status_ == monitorEventType)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Needn't to send record monitor event."
            "old record status[%d]=new record status.",
            record_status_));
        return SVS_RESULT_OK;
    }

    SVS_LOG((SVS_LM_INFO, "Begin to create monitor event.event type [%d].",monitorEventType));

    // 申请消息缓冲区
    ACE_Message_Block *mb =
        g_p_msg_buffer->alloc_mb(   CSVS_Mb_Buffer_Manager::MB_TYPE_NORMAL_EVENT,
                                    sizeof(RECONNECT_EVENT_MSG) );
    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create monitor event failed.Fail to allocate message block. "
            "msg type[%d], msg size[%d].monitor type[%d]",
            CSVS_Mb_Buffer_Manager::MB_TYPE_NORMAL_EVENT,
            sizeof(RECONNECT_EVENT_MSG) ,
            monitorEventType));
        return SVS_RESULT_FAILURE;
    }

    // 构造录像控制消息
    RECONNECT_EVENT_MSG *pMsg = (RECONNECT_EVENT_MSG *)(void*)mb->wr_ptr();
    pMsg->Hdr.Event = (uint16_t)monitorEventType;
    mb->wr_ptr( sizeof(RECONNECT_EVENT_MSG) );
    ACE_Time_Value tv(ACE_OS::gettimeofday());

    // 发送到时间队列
    int32_t sendRet = CAC_RT_Record_Server::instance()->enqueue_monitor_event(mb, &tv);

    if(SVS_RESULT_FAILURE != sendRet)
    {
        record_status_ = monitorEventType;
    }
    else
    {
        SVS_LOG((SVS_LM_ERROR,
            "Monitor event enqueque failed. monitor type[%d]",
            monitorEventType));
    }

    return sendRet;
}

/*****************************************************************************
函 数 名  : set_conn_status
功能描述  : 设置连接状态。有状态机机制。
输入参数  : int32_t newStatus:将要设置的连接状态
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::set_conn_status(int32_t newStatus)
{
    int32_t oldConnStatus = conn_status_;
    int32_t setResult = SVS_RESULT_FAILURE;
    switch(newStatus)
    {
        case RT_CONN_STORE_DISCONNECTED: // 断连状态
        {
            conn_status_ = newStatus;
            setResult = SVS_RESULT_OK;
        }
        break;
        case RT_CONN_STORE_CONNECTING: // 正在连接状态
        {
            if(RT_CONN_STORE_DISCONNECTED == conn_status_)
            {
                conn_status_ = newStatus;
                setResult = SVS_RESULT_OK;
            }
        }
        break;
        case RT_CONN_STORE_LOGGING: // logging状态
        {
            if(RT_CONN_STORE_CONNECTING == conn_status_)
            {
                conn_status_ = newStatus;
                setResult = SVS_RESULT_OK;
            }
        }
        break;
        case RT_CONN_STORE_READY: // 正常连接状态
        {
            if(RT_CONN_STORE_LOGGING == conn_status_)
            {
                conn_status_ = newStatus;
                setResult = SVS_RESULT_OK;
            }
        }
        break;
        default:
            break;
    }

    // 如果改变成功
    if(SVS_RESULT_OK == setResult)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Change connection status with %s successed, "
            "old status[%d], new status[%d]",
            g_strRemoteServerName[server_type_],
            oldConnStatus, conn_status_));
    }

    return setResult;
}



/*****************************************************************************
函 数 名  : send_register_msg
功能描述  : 发送注册消息给StoreSvr
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::send_register_msg()
{
    CAC_RT_Record_Server * pServer = CAC_RT_Record_Server::instance();

    if(NULL == pServer)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Reconnect with StoreSvr failed. %s pointor is NULL.",
            g_strRemoteServerName[server_type_]));

        return SVS_RESULT_FAILURE;
    }

    SVS_RECORDSVR_INTERNAL_REGISTER_REQ_MSG regMsg;
    (void)memset(&regMsg, 0, sizeof(regMsg));

    // 构造注册消息
    FillCommonHeader(   regMsg.Header,
                        SVS_MSG_TYPE_STORESVR_INTERNAL_REGISTER_REQ,
                        1,
                        sizeof(SVS_RECORDSVR_INTERNAL_REGISTER_REQ_MSG));
    pServer->get_register_info(regMsg);

    // 发送注册消息
    ACE_Time_Value tv(10, 0);
    if(sizeof(regMsg) != (uint32_t)this->peer().send_n(&regMsg, sizeof(regMsg), &tv))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[SEND]Failed to send registering message to %s.",
            g_strRemoteServerName[server_type_]));

        return SVS_RESULT_FAILURE;
    }

    // 记录login时间
    new_loggin_time_ = time(NULL);

    SVS_LOG((SVS_LM_WARNING,
        "[SEND]Send registering message to %s succeeded.",
        g_strRemoteServerName[server_type_]));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : handle_register_response_msg
功能描述  : 处理注册响应消息。如果连接状态不是"注册中"状态，则说明连接有问题，
            可能存在过期消息，需要断开连接。如果不是ready状态，接收到其他信令
            消息也需要断开。
输入参数  : const SVS_HEADER* pMsgHeader:消息头
输出参数  : NA
返 回 值  : 成功返回0，失败返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Client_Handler::handle_register_response_msg(const SVS_HEADER* pMsgHeader)
{
    if(SVS_MSG_TYPE_STORESVR_INTERNAL_REGISTER_RESP == pMsgHeader->MsgType)
    {
        // 如果状态转换失败,重新连接
        if(SVS_RESULT_OK != set_conn_status(RT_CONN_STORE_READY))
        {
            SVS_LOG((SVS_LM_ERROR,
                "[RECV]Receive a register response message from %s,"
                "but connection status is not logging in."
                "Real connection status [%d].",
                g_strRemoteServerName[server_type_],
                conn_status_));
            return HANDLE_RECV_DATA_ERROR;
        }

        // 如果注册失败，
        SVS_RECORDSVR_INTERNAL_REGISTER_RESP_MSG *pRegisterResp =
            (SVS_RECORDSVR_INTERNAL_REGISTER_RESP_MSG *)(void*)pMsgHeader;
        if (0 != pRegisterResp->RespCode)
        {
            SVS_LOG((SVS_LM_ERROR,
                "[RECV]The registering response message from %s is invalid."
                "The response code is invalid, response code[%d].",
                g_strRemoteServerName[server_type_],
                pRegisterResp->RespCode));

            return HANDLE_RECV_DATA_ERROR;
        }


        signal_msg_stat_.updateConnStoreCtrlTime(time(NULL));
        SVS_LOG((SVS_LM_INFO,
            "[RECV]Receive registering response message from %s successfully.",
            g_strRemoteServerName[server_type_]));

        // 接收到注册响应消息，开始启动录像
        (void)send_monitor_event(RT_MONITOR_EVENT_START_RECORD);

        if (ALARM_STATUS_FAULTING == g_alarmStatus[ALARM_CONNECT_STORESVR_INDEX])
        {
            g_alarmStatus[ALARM_CONNECT_STORESVR_INDEX] = ALARM_STATUS_RESUMED;
        }

        // 继续处理其它信令消息
        return HANDLE_RECV_DATA_CONTINUE;
    }
    else if(RT_CONN_STORE_READY != conn_status_) // 如果非连接状态收到消息，则断开连接
    {
        SVS_LOG((SVS_LM_ERROR,
            "[RECV]Receive a message from %s, "
            "but connection status is not RT_CONN_STORE_READY, "
            "Real connection status is %d.",
            g_strRemoteServerName[server_type_],
            conn_status_));

        return HANDLE_RECV_DATA_ERROR;
    }

    return HANDLE_RECV_DATA_SUCCESS;
}

ACE_Message_Block* CSVS_Client_Handler::alloc_mb(uint32_t mbMsgLen, uint8_t   mbType )const
{
    return g_p_msg_buffer->alloc_mb(mbType, mbMsgLen);
}

void CSVS_Client_Handler::free_mb(ACE_Message_Block * mb)const
{
    g_p_msg_buffer->free_mb(mb);
    return ;
}

