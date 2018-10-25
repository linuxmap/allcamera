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

// �����������б�
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
�� �� ��  : construct
��������  : ��ʼ����Ա����
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::construct()
{
    brunning_       = true;
    p_signal_queue_ = NULL;

    // Ӧ�ò�������Ϣ
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
�� �� ��  : open
��������  : ��ʼ�������������̣߳�ע�ᷴӦ��
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
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

    // Ӧ�ò�����
    ACE_Time_Value tv(HEARTBEAT_SEND_INTERVAL, 0);
    int32_t time_id_ = reactor()->schedule_timer(this, NULL, tv, tv);
    if (-1 == time_id_)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Set timer for heartbeat with %s failed.",
            g_strRemoteServerName[server_type_]));

        return -1;
    }

    // ��Ϣ����ˮλ
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

    // �����߳�
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
�� �� ��  : svc
��������  : ������Ϣ�̺߳��������ڷ�����Ϣ��StoreSvr
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
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
�� �� ��  : handle_input
��������  : ��StoreSvr��������
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::handle_input(ACE_HANDLE /*handle*/)
{
    SVS_TRACE();

    // ��������
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

    // ���Ľ������ݳ���
    recv_data_len_ += (uint32_t)recvResult;

    // ��������
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
 �� �� ��  : handle_timeout
 ��������  : �ʹ洢���Ʒ�������������������ʱ�ص�����,����ȷ���������Ϣ,
             �����������δ���յ��洢���Ʒ�������������Ӧ��Ϣ,�����洢���Ʒ�����
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�����������Ϣ,����0 ��������,����-1
 �޸���ʷ      :
 1. ��    ��   : 2009��8��20��
    �޸�����   : ����
*****************************************************************************/
int32_t CSVS_Client_Handler::handle_timeout(const ACE_Time_Value & /*currentTime*/, const void * /*act = 0*/)
{
    SVS_TRACE();
    //������״̬�����
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

    //����������Ϣ
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

    // ��������Ϣ�����ͷ
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
�� �� ��  : build_msg
��������  : ������Ϣͷ��������������Ϣ��
�������  : pMsgHeader  :   ��Ϣͷָ��
            mbType      :   ��Ҫ���ɵ���Ϣ������
�������  : ��
�� �� ֵ  : �ɹ�,���� ���ɵ���Ϣ��ָ�� ���򷵻�NULL
�޸���ʷ  :
*****************************************************************************/
ACE_Message_Block* CSVS_Client_Handler::build_msg(const SVS_HEADER* pMsgHeader,
                                                    uint8_t   mbType ) const
{
    SVS_TRACE();

    uint32_t msgBlockLen = pMsgHeader->PacketLength;

    // ������Ϣ������
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
�� �� ��  : parse_data
��������  : �Խ��յ����������ݽ��н������ֽ����������Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::parse_data()
{
    SVS_TRACE();

    // ���ݲ�����Ϣͷ���ȣ�������ȡ����
    if ( recv_data_len_ < sizeof(SVS_HEADER) )
    {
        return 0;
    }

    // �ֽ��ÿ����Ϣ
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

        // ��������
        if(HANDLE_RECV_DATA_BREAK== iRet)
        {
            break;
        }
        else if(HANDLE_RECV_DATA_ERROR == iRet)// ���յ��������д���
        {
            return SVS_RESULT_FAILURE;
        }
        else if(HANDLE_RECV_DATA_CONTINUE == iRet)  // ������������������Ϣ
        {
            //����������0
            heartbeat_timeout_times_ = 0;
            procDataPos = nextDataPos;
            continue;
        }

        //����������0
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
        case SVS_MSG_TYPE_HEARTBEAT:    //������Ϣ����
            {
                //������Ϣ��������Ϣ���д���ֱ�ӷ���
                procDataPos = nextDataPos;
                continue;
            }
            // break;

        case SVS_ALARM_KINESCOPE_CONTROL_REQ:   //�澯�¼���Ϣ����
            {
                mb = build_msg( pMsgHeader, CSVS_Mb_Buffer_Manager::MB_TYPE_ALARM_EVENT );
            }
            break;
        default:    //������Ϣ
            {
                mb = build_msg( pMsgHeader, CSVS_Mb_Buffer_Manager::MB_TYPE_NORMAL_EVENT );
            }
            break;
        }

        if ( NULL != mb )
        {
            //���յ�����Ϣ���һ����Ϣ��������ѹ����Ϣ���������
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

    // �ѽ����õ���Ϣ��������Ϣ������еȴ�����
    if(SVS_RESULT_OK != equeue_msg(mbBatch, tv))
    {
        return SVS_RESULT_FAILURE;
    }

    // ����ͳ����Ϣ
    signal_msg_stat_.addSignalMessage(msgCount);

    // ���»������α�λ��
    return update_buffer_cursor(procDataPos,nextDataPos);
}

/*****************************************************************************
�� �� ��  : send_message_thread
��������  : ������Ϣ�̡߳��ӷ�����Ϣ�����л�ȡ���ݣ����͵����Ӷ�Ӧ��server���Զ�
            ������StoreSvr��Ҳ������Console.
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
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
        // ----���Ż��� ��������-----
        if (-1 == getq(mb) )
        {
            if(ESHUTDOWN == ACE_OS::last_error())
            {
                // �������´򿪶���
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

        // ����Ƿ�����״̬�����ͻ᲻�ɹ������÷����ˣ�ֱ���ͷ���Ϣ�ڴ�
        if(RT_CONN_STORE_READY != conn_status_)
        {
            SVS_LOG( (SVS_LM_INFO,
                "Send message to %s failed."
                "Connection status is not READY."
                "current status[%d],msg type[0x%04x].",
                g_strRemoteServerName[server_type_],
                conn_status_,
                pHdr->MsgType) );

            // �ͷ���Ϣ������
            free_mb(mb );
            continue;
        }

        // ��ӡ���͵���Ϣ������Ϣ
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

        // ������Ϣ
        ACE_Time_Value tv(SVS_SEND_MSG_TIMEOUT, 0);
        int32_t sendResult = peer().send_n(mb->rd_ptr(), mb->length(), &tv);
        if ( ( -1 == sendResult ) && ( EAGAIN == ACE_OS::last_error() ) )
        {
            // ����ʧ�ܷ���EAGAIN������,�ط�һ��
            sendResult = peer().send_n(mb->rd_ptr(), mb->length(), &tv);
        }

        // ������Ϣ����ͳ��
        ++sendDataCount;
        if (0 == sendDataCount%10000 )
        {
            SVS_LOG((SVS_LM_DEBUG,
                "Send message to %s count[%d].",
                g_strRemoteServerName[server_type_],
                sendDataCount));
        }

        // ������Ϣʧ��
        if (mb->length() != (uint32_t)sendResult )
        {
            SVS_LOG( (SVS_LM_ERROR,
                "Fail to send message to %s. return code[%d].",
                g_strRemoteServerName[server_type_],
                sendResult ) );
            (void)this->disconnection();
        }

        // �ͷ���Ϣ������
        free_mb(mb );
    }

    SVS_LOG( (SVS_LM_WARNING,
        "Send message thread for %s exit.",
        g_strRemoteServerName[server_type_]));

    return 0;
}

/*****************************************************************************
�� �� ��  : send_message_thread
��������  : ������Ϣ�̡߳��ӷ�����Ϣ�����л�ȡ���ݣ����͵����Ӷ�Ӧ��server���Զ�
            ������StoreSvr��Ҳ������Console.
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::check_connection_status_thread()
{
    SVS_LOG( (SVS_LM_WARNING,
        "Check connection status with %s thread start to running...",
        g_strRemoteServerName[server_type_]));

    while ( brunning_ )
    {
        // ���״̬����
        if((RT_CONN_STORE_INIT == conn_status_) || (RT_CONN_STORE_READY == conn_status_))
        {
            (void) usleep(10000);
            continue;
        }

        // ������״̬Ϊ��ʼ�����߶Ͽ�ʱ����������
        if(RT_CONN_STORE_DISCONNECTED == conn_status_)
        {
            if ((SVS_RESULT_OK != reconnect()) || (SVS_RESULT_OK != register2server()))
            {
                (void)disconnection();

                // ���ӷ��������ɹ���sleepһ��ʱ����ٴ�����
                (void) sleep(3);

                continue;
            }
        }

        // �����logging״̬���ж��Ƿ��Ƿ�ȴ���ʱ
        if(RT_CONN_STORE_LOGGING == conn_status_)
        {
            time_t currTime = time(NULL);

            // ����ܾ�û���յ���Ӧ��Ϣ����Ͽ�����
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
�� �� ��  : check_data
��������  : ��鵱ǰ�յ������ݵĺϷ��ԣ������û���������������
�������  : ��
�������  : ��
�� �� ֵ  : �Ѿ����յ������ݽ���ʧ�� ����-1,�ɹ�,���� 0,����Ҫ�������շ���1
�޸���ʷ  :
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

    // �����Ϣ�Ƿ������ȫ
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
�� �� ��  : equeue_msg
��������  : �ѽ�����������Ϣ�������������У��ȴ�����
�������  : ACE_Message_Block * mbBatch:��Ϣ������
            ACE_Time_Value tv:������г�ʱ�ȴ�ʱ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::equeue_msg(ACE_Message_Block * mbBatch, ACE_Time_Value tv)
{
    if ( NULL == mbBatch )
    {
        return SVS_RESULT_OK;
    }

    // ͳһ�ͷ���Դ
    do
    {
        if ( NULL == p_signal_queue_ )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Fail to put message to queue for %s. "
                "The queue pointer p_signal_queue_ is NULL.",
                g_strRemoteServerName[server_type_]));

            // �����쳣����Ҫ�ͷ���Ϣ�ڴ��
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

    // ��Ϣ�������ʧ�ܣ��ͷ���Դ����Ϣ��������
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
�� �� ��  : update_buffer_cursor
��������  : ��δ������������Ƶ��������Ŀ�ʼλ�ã��ȴ��´μ�������������һ�����
�������  : uint32_t procDataPos:��ǰ�����ڻ������е�ָ��ƫ��λ��
            uint32_t nextDataPos:�¸���Ϣ��ʼƫ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::update_buffer_cursor(uint32_t procDataPos,
                                                    uint32_t nextDataPos)
{
    SVS_TRACE();

    // ��һ����Ϣ��δ��ȫ��ֱ�ӷ���
    if ( 0 == procDataPos )
    {
        return 0;
    }

    // ��Ϣ���ô����꣬��������ݳ��Ⱥ�ֱ�ӷ���
    if ( nextDataPos == recv_data_len_ )
    {
        recv_data_len_ = 0;
        return 0;
    }

    // ������ʣ��ռ�
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

    // ����δ������Ĳ�����Ϣ�������ƶ�������ͷ�󣬼�������
    (void)ACE_OS::memmove(    &recv_data_buffer_[0],
                        &recv_data_buffer_[procDataPos],
                        (uint32_t)leaveDataLen );

    recv_data_len_ = (uint32_t)leaveDataLen;

    return 0;

}

/*****************************************************************************
�� �� ��  : send_msg
��������  : ������Ϣ���������ӿڡ���Ҫ���͵���Ϣ���뷢����Ϣ�жӣ����ɷ�����Ϣ
            �̴߳��������server������״̬ʱ������״̬������ʧ��֪ͨ��㡣
�������  : ACE_Message_Block * mb:Ҫ���͵���Ϣ��
            ACE_Time_Value * timeout:������г�ʱʱ������
�������  : ��
�� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1.
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::send_msg(ACE_Message_Block * mb, ACE_Time_Value * timeout)
{
    // �������״̬���������򷵻�ʧ��
    if(conn_status_ != RT_CONN_STORE_READY)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[SEND] message enqueue failed.Connection status with %s is [%d]",
            g_strRemoteServerName[server_type_],
            conn_status_));

        return SVS_RESULT_FAILURE;
    }

    // ����Ϣ������Ϣ���Ͷ���
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
�� �� ��  : set_socket_option
��������  : ����socket���ԡ�
�������  : NA
�������  : NA
�� �� ֵ  : NA
�޸���ʷ  :
*****************************************************************************/
void CSVS_Client_Handler::set_socket_option()const
{
    uint32_t nSize = 0;
    int32_t nLen = sizeof(nSize);

    // ����SOCKET���ͻ�����
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

    // ���ջ�����
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
�� �� ��  : reconnect
��������  : ��������server�������ӡ����ǰ���Ѿ����ӹ��ˣ����Լ���reactor��ȥע���
�������  : NA
�������  : NA
�� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::reconnect()
{
    // ���״̬�������⣬��������
    if(RT_CONN_STORE_DISCONNECTED != conn_status_)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Reconnect with %s occur error. Set the connection status to disconnected."
            "old status[%d].",
            g_strRemoteServerName[server_type_],
            conn_status_));

        // ���Լ���reactor��ȥע��
        (void)reactor()->remove_handler(this, ACE_Event_Handler::RWE_MASK
                                            | ACE_Event_Handler::DONT_CALL);
        ACE_OS::last_error(0);

        // �κ�״̬�����Ե���״̬�����Կ���ֱ����״̬
        conn_status_ = RT_CONN_STORE_DISCONNECTED;
    }

    // ��ʼ����������socket
    recv_data_len_ = 0;

    CAC_RT_Record_Server * pServer = CAC_RT_Record_Server::instance();
    if(NULL == pServer)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Reconnect with %s failed. server pointor is NULL.",
            g_strRemoteServerName[server_type_]));

        return SVS_RESULT_FAILURE;
    }

    // ���������������10�Σ�����email�澯
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

    //�ڿ�ʼ����֮ǰ��״̬����Ϊ"������"
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

    // ����������
    ACE_SOCK_Connector connector;
    ACE_INET_Addr remoteAddr(pServer->recordctrl_port(),pServer->recordctrl_ip());
    ACE_INET_Addr localAddr(pServer->lan_port(),pServer->lan_ip());

    // �豸���ӳ�ʱʱ��
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

        //��ͳ����Ϣ����������ʧ�ܴ���
        signal_msg_stat_.addConnStoreCtrlFailTimes(1);
        (void)this->peer().close();

        return SVS_RESULT_FAILURE;
    }

    // ȡ�öԷ���ַ����������
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

    // ����socket����
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
�� �� ��  : register2server
��������  : ��serverע�ᡣ������reactorע���Լ���Ȼ����Server����ע����Ϣ
�������  : NA
�������  : NA
�� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::register2server()
{
    SVS_LOG((SVS_LM_WARNING, "Retry to register to %s.", g_strRemoteServerName[server_type_]));

    // ����״̬Ϊ "ע����" ״̬
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

    // ��Ӧ��ע����¼�
    if(-1 == reactor()->register_handler(this, ACE_Event_Handler::READ_MASK))
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Fail to register client handler for %s. this[0x%08x].",
            g_strRemoteServerName[server_type_],
            this ));

        return SVS_RESULT_FAILURE;
    }

    // ����ע����Ϣ
    if (0 != send_register_msg())
    {
        SVS_LOG((SVS_LM_ERROR, "Register to %s failed.", g_strRemoteServerName[server_type_]));

        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : disconnection
��������  : �Ͽ���Server�����ӡ��������״̬�Ѿ��ǶϿ�״̬�ˣ���Ͽ���ɡ�
�������  : NA
�������  : NA
�� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::disconnection()
{
    // �������״̬�Ѿ��ǶϿ�״̬�ˣ���Ͽ����ӳɹ�
    if(RT_CONN_STORE_DISCONNECTED == conn_status_)
    {
        SVS_LOG((SVS_LM_INFO,
            "Needn't to disconnected from %s, Current status is disconnected.",
            g_strRemoteServerName[server_type_]));

        return SVS_RESULT_OK;
    }

    // ����ͳ����Ϣ
    signal_msg_stat_.addDisconnStoreCtrlTimes(1);
    signal_msg_stat_.updateDisconnStoreCtrlTime(time(NULL));


    SVS_LOG((SVS_LM_WARNING, "Begin to disconnect from %s.",g_strRemoteServerName[server_type_]));

    // ���Լ���reactor��ȥע��
    (void)reactor()->remove_handler(this,ACE_Event_Handler::RWE_MASK | ACE_Event_Handler::DONT_CALL);

    // ����ֹͣ¼����Ϣ
    (void)send_monitor_event(RT_MONITOR_EVENT_STOP_RECORD);


    // �Ͽ�����
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
�� �� ��  : send_monitor_event
��������  : ����monitor�¼����¼������̡߳�
�������  : int32_t monitorEventType:�¼����ͣ�����������¼��Ҳ������ֹͣ¼��
            �ں�StoreSvr�Ͽ���ʱ����Ҫֹͣ����¼�����Է���ֹͣ¼���¼����¼�
            ����߳�
�������  : NA
�� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::send_monitor_event(int32_t monitorEventType)
{
    // �жϵ�ǰ��¼��״̬�������ǰ��¼��״̬��monitor�¼�������ͬ�������ٷ����¼�
    if(record_status_ == monitorEventType)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Needn't to send record monitor event."
            "old record status[%d]=new record status.",
            record_status_));
        return SVS_RESULT_OK;
    }

    SVS_LOG((SVS_LM_INFO, "Begin to create monitor event.event type [%d].",monitorEventType));

    // ������Ϣ������
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

    // ����¼�������Ϣ
    RECONNECT_EVENT_MSG *pMsg = (RECONNECT_EVENT_MSG *)(void*)mb->wr_ptr();
    pMsg->Hdr.Event = (uint16_t)monitorEventType;
    mb->wr_ptr( sizeof(RECONNECT_EVENT_MSG) );
    ACE_Time_Value tv(ACE_OS::gettimeofday());

    // ���͵�ʱ�����
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
�� �� ��  : set_conn_status
��������  : ��������״̬����״̬�����ơ�
�������  : int32_t newStatus:��Ҫ���õ�����״̬
�������  : NA
�� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::set_conn_status(int32_t newStatus)
{
    int32_t oldConnStatus = conn_status_;
    int32_t setResult = SVS_RESULT_FAILURE;
    switch(newStatus)
    {
        case RT_CONN_STORE_DISCONNECTED: // ����״̬
        {
            conn_status_ = newStatus;
            setResult = SVS_RESULT_OK;
        }
        break;
        case RT_CONN_STORE_CONNECTING: // ��������״̬
        {
            if(RT_CONN_STORE_DISCONNECTED == conn_status_)
            {
                conn_status_ = newStatus;
                setResult = SVS_RESULT_OK;
            }
        }
        break;
        case RT_CONN_STORE_LOGGING: // logging״̬
        {
            if(RT_CONN_STORE_CONNECTING == conn_status_)
            {
                conn_status_ = newStatus;
                setResult = SVS_RESULT_OK;
            }
        }
        break;
        case RT_CONN_STORE_READY: // ��������״̬
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

    // ����ı�ɹ�
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
�� �� ��  : send_register_msg
��������  : ����ע����Ϣ��StoreSvr
�������  : NA
�������  : NA
�� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1
�޸���ʷ  :
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

    // ����ע����Ϣ
    FillCommonHeader(   regMsg.Header,
                        SVS_MSG_TYPE_STORESVR_INTERNAL_REGISTER_REQ,
                        1,
                        sizeof(SVS_RECORDSVR_INTERNAL_REGISTER_REQ_MSG));
    pServer->get_register_info(regMsg);

    // ����ע����Ϣ
    ACE_Time_Value tv(10, 0);
    if(sizeof(regMsg) != (uint32_t)this->peer().send_n(&regMsg, sizeof(regMsg), &tv))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[SEND]Failed to send registering message to %s.",
            g_strRemoteServerName[server_type_]));

        return SVS_RESULT_FAILURE;
    }

    // ��¼loginʱ��
    new_loggin_time_ = time(NULL);

    SVS_LOG((SVS_LM_WARNING,
        "[SEND]Send registering message to %s succeeded.",
        g_strRemoteServerName[server_type_]));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : handle_register_response_msg
��������  : ����ע����Ӧ��Ϣ���������״̬����"ע����"״̬����˵�����������⣬
            ���ܴ��ڹ�����Ϣ����Ҫ�Ͽ����ӡ��������ready״̬�����յ���������
            ��ϢҲ��Ҫ�Ͽ���
�������  : const SVS_HEADER* pMsgHeader:��Ϣͷ
�������  : NA
�� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Client_Handler::handle_register_response_msg(const SVS_HEADER* pMsgHeader)
{
    if(SVS_MSG_TYPE_STORESVR_INTERNAL_REGISTER_RESP == pMsgHeader->MsgType)
    {
        // ���״̬ת��ʧ��,��������
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

        // ���ע��ʧ�ܣ�
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

        // ���յ�ע����Ӧ��Ϣ����ʼ����¼��
        (void)send_monitor_event(RT_MONITOR_EVENT_START_RECORD);

        if (ALARM_STATUS_FAULTING == g_alarmStatus[ALARM_CONNECT_STORESVR_INDEX])
        {
            g_alarmStatus[ALARM_CONNECT_STORESVR_INDEX] = ALARM_STATUS_RESUMED;
        }

        // ������������������Ϣ
        return HANDLE_RECV_DATA_CONTINUE;
    }
    else if(RT_CONN_STORE_READY != conn_status_) // ���������״̬�յ���Ϣ����Ͽ�����
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

