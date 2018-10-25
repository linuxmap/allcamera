#include "vms/vms.h"
#include "svs_stat_manager.h"
#include "svs_real_record_server.h"
#include "svs_rt_record_common.h"
#include "svs_media_processor.h"
#include "svs_recv_stream.h"
#include "svs_io_scheduler.h"
#include "svs_bakup_front_record_lens.h"

extern CFluxStat g_FluxStat;
//==============================CAC_Recv_Stream BEGIN=========================
CAC_Recv_Stream::CAC_Recv_Stream()
{
    reset();

    // ������ָ�뵥������
    p_media_buffer_             = NULL;
}

CAC_Recv_Stream::CAC_Recv_Stream(ACE_Reactor *pRecvReactor,
                                              CSVS_Recv_RT_Media_Buffer *pMediaBuffer,
                                              uint32_t recvThreadNo)
    : ACE_Event_Handler(pRecvReactor)
{
    SVS_TRACE();
    reset();

    // ������ָ�뵥������
    p_media_buffer_ = pMediaBuffer;
    m_ulRecvThreadNo = recvThreadNo;
}


CAC_Recv_Stream::~CAC_Recv_Stream()
{
    try
    {
        SVS_LOG((SVS_LM_DEBUG, "Close recv stream. port[0x%08x].", this->endpoint_.get_handle()));
        (void)endpoint_.close();

        p_media_buffer_ = NULL;
    }
    catch(...)
    {
        p_media_buffer_ = NULL;
    }

}

void CAC_Recv_Stream::reset()
{
    // ������ָ����Ҫ��������
    (void)ACE_OS::memset( device_id_, 0, sizeof(device_id_));

    rt_stream_state_            = RT_STREAM_STATE_CLOSE;
    m_StopRecvFlag              = false;
    remain_recv_buff_size_      = 0;

    recv_type_                  = LENS_TYPE_BASE;
    recv_rt_media_buff_         = NULL;
    cur_frame_remain_size_      = 0;
    alloc_buffer_failure_times_ = 0;
    start_recv_time_            = 0;
    last_recv_packet_time_      = 0;

    last_check_index_time_      = 0;
    last_check_record_time_     = 0;
    recv_time_remove_           = 0;

    record_key_frame_flag_      = false;
    record_key_frame_rate_      = 0;
    recv_frame_num_then_lost_   = 0;

    (void)ACE_OS::memset((char*)&m_MediaResp, 0x0, sizeof(m_MediaResp));
    m_MediaRespLen              = 0;

    (void)ACE_OS::memset((char*)&m_packHeader, 0x0, sizeof(m_packHeader));
    m_recv_first_fram_time      = 0;
    m_first_frame_timetick      = 0;
    m_last_frame_timetick       = 0;
    m_last_Adjust_timetick = 0;

    cur_recv_frame_id_          = 0;
    last_recv_frame_id_         = 0;
    lost_frame_num_             = 0;
    cur_frame_start_recv_time_  = 0;
    m_break_frame_num           = 0;
    m_ulLastError               = ROUTE_STOP_REASON_OK;

    return;
}

/*****************************************************************************
�� �� ��  : init
��������  : ��ʼ����Ԥ�ȷ��仺����
�������  : CSVS_Record_Lens *pDevice:��ͷ����ָ��
            const ACE_INET_Addr &addr:��ͷ��ַ
�������  : ��
�� �� ֵ  : �ɹ�:0  ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Recv_Stream::init(const char *szDevID)
{
    if (NULL == szDevID)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to init camera as the pointer pDevice is NULL."));
        return SVS_RESULT_FAILURE;
    }

    // ����Ƿ���Գ�ʼ��init״̬
    if (RT_STREAM_STATE_CLOSE != rt_stream_state_)
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Can not initialize recv stream. camera[%s], current status[0x%02x].",
            device_id_,
            rt_stream_state_ ) );
        return SVS_RESULT_FAILURE;
    }

    // ����Ҫ����p_media_buffer_
    reset();

    SetLocalizeDeviceId((uint8_t *)device_id_,
        sizeof(device_id_),
        szDevID);

    rt_stream_state_ = RT_STREAM_STATE_INIT;

    ingress_stat_.init(device_id_);

    SVS_LOG((SVS_LM_INFO, "Init Receive stream scoket success.cameraId[%s].", szDevID));

    return SVS_RESULT_OK;
}

int32_t CAC_Recv_Stream::start_recv(uint32_t ulLocalIndex,
                                    int32_t nType,
                                    uint32_t ulRouteID,
                                    const ACE_INET_Addr &local_addr,
                                    const ACE_INET_Addr &remote_addr)
{
    if ( RT_STREAM_STATE_INIT != rt_stream_state_ )
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] start recv stream fail, "
            "state[%d] invalid.",
            device_id_, rt_stream_state_));

        return SVS_RESULT_FAILURE;
    }

    m_StopRecvFlag = false;
    local_index_   = ulLocalIndex;
    recv_type_     = nType;
    router_id_     = ulRouteID;
    local_addr_    = local_addr;
    remote_addr_   = remote_addr;

    // ���ӵ�VTDU
    if (ACE_INVALID_HANDLE != endpoint_.get_handle())
    {
        (void)endpoint_.close();
    }

    //1 TODO ��ΪTCP���ӿ��ܻ��������Ҫ�޸�
    ACE_SOCK_Connector  myConnector;
    ACE_Time_Value      tv(1);
    int32_t nRet = myConnector.connect(endpoint_, remote_addr_, &tv, local_addr_, 1);
    if (-1 == nRet)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to open socket, camera[%s], port[%d].",
            device_id_,
            remote_addr_.get_port_number()));
        rt_stream_state_ = RT_STREAM_STATE_CLOSE;
        m_ulLastError = ROUTE_STOP_REASON_MRU_CONNECT_ERROR;
        return SVS_RESULT_FAILURE;
    }

    ACE_HANDLE          sock_handle = endpoint_.get_handle();

    uint32_t nSize = SVS_SOCKET_RECV_BUFF_MAX;

    //���ý��ջ�������С
    if (0 != endpoint_.set_option(SOL_SOCKET, SO_RCVBUF, (void *)&nSize, sizeof(nSize)))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to set socket recv buffer, "
            "camera[%s] errcode[%d].",
            device_id_ ,
            ACE_OS::last_error()));
        rt_stream_state_ = RT_STREAM_STATE_CLOSE;
        return SVS_RESULT_FAILURE;
    }

    //�첽����
    if (ACE_OS::fcntl(sock_handle, F_SETFL, ACE_OS::fcntl(sock_handle, F_GETFL) | O_NONBLOCK))
    {
        SVS_LOG((SVS_LM_ERROR,
            "set socket option O_NONBLOCK failed, "
            "camera[%s] handle[%d] errcode[%d].",
            device_id_,
            sock_handle,
            ACE_OS::last_error()));

        rt_stream_state_ = RT_STREAM_STATE_CLOSE;
        return SVS_RESULT_FAILURE;
    }

    //״̬��Ϊ�ȴ�����ý�������Ӧ״̬
    rt_stream_state_  = RT_STREAM_STATE_RECV_VTDU_RESP;
    start_recv_time_  = ACE_OS::time(NULL);

    last_print_time_ = ACE_OS::gettimeofday();

    //����շ�Ӧ��ע�������߳�
    nRet = reactor()->register_handler(this, ACE_Event_Handler::READ_MASK);
    if (-1 == nRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to register socket handle, "
            "camera[%s], handle[0x%08x].",
            device_id_,
            sock_handle));

        rt_stream_state_ = RT_STREAM_STATE_CLOSE;
         return SVS_RESULT_FAILURE;
    }


    //��VTDU����ý����������
    SVS_MEDIA_CONNECT_REQ_MSG req;
    req.Header.MsgType = htonl(SVS_MSG_TYPE_CLIENT_MEDIA_CONNECT_REQ);
    req.Header.PacketLength = htonl(sizeof(req));
    req.Header.ProtocolVersion = htons(SVS_PROTOCOL_VERSION);
    req.Header.TransactionNo = htons(CAC_RT_Record_Server::instance()->transaction_no());

    req.StreamID    = htonl(router_id_);
    req.CONNECTIP   = CAC_RT_Record_Server::instance()->get_local_ip(0);
    req.CONNECTPORT = 0;


    if (LENS_TYPE_RECORDBAK == nType)
    {
        SetNLSDeviceId((uint8_t *)req.SessionID,
            sizeof(req.SessionID),
            CAC_RT_Record_Server::instance()->str_id());

        //(void)ACE_OS::memcpy(req.SessionID,
        //                     CAC_RT_Record_Server::instance()->str_id(),
        //                     SESSIONID_LEN);
    }
    else
    {
        (void)ACE_OS::memset(req.SessionID,
                         0x0,//CAC_RT_Record_Server::instance()->str_id(),
                         SESSIONID_LEN);
    }

    ACE_Time_Value tvTimeout(0, (100 * 1000));    //100ms
    size_t     send_len = 0;
    (void)endpoint_.send_n(&req, sizeof(req), &tvTimeout, &send_len);
    if (send_len != sizeof(req))
    {
        SVS_LOG((SVS_LM_WARNING, "camera[%s] send media connect req fail, errno[%d].",
                  device_id_, ACE_OS::last_error()));

        return SVS_RESULT_FAILURE;
    }

    ACE_OS::last_error(0);
    SVS_LOG((SVS_LM_INFO,
              "camera[%s] start recv stream success, socket handle[0x%08x], local port[%d].",
              device_id_,
              sock_handle,
              local_addr_.get_port_number()));

    return SVS_RESULT_OK;
}

int32_t CAC_Recv_Stream::stop_recv()
{
    if (RT_STREAM_STATE_CLOSE == rt_stream_state_)
    {
        // �����Ѿ�ֹͣ��ֱ��֪ͨMediaProcessorֹͣ���
        (void)SVS_Media_Processor::instance().recv_close_notify(local_index_);

        SVS_LOG((SVS_LM_WARNING, "camera[%s] stop recv stream, status already RT_STREAM_STATE_CLOSE.",
                   device_id_));
        return SVS_RESULT_OK;
    }

    // �����˳���־
    m_StopRecvFlag = true;

    // �ر�����
    // ��notify����100ms��ʱ����ֹ�߳�����
    ACE_Time_Value timeout(0, 100 * 1000);
    //int32_t nRetVal = reactor()->notify((ACE_Event_Handler*)this, ACE_Event_Handler::READ_MASK);
    int32_t nRetVal = reactor()->notify((ACE_Event_Handler*)this, ACE_Event_Handler::READ_MASK, &timeout);
    if (-1 == nRetVal)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Notify reactor failed.cameraId[%s],route[%u] vtdu addr[%s:%d].",
            device_id_,
            router_id_,
            remote_addr_.get_host_addr(),
            remote_addr_.get_port_number()));
    }
    SVS_LOG((SVS_LM_INFO, "camera[%s] stop recv stream, route[%u] vtdu addr[%s:%d].",
               device_id_,
               router_id_,
               remote_addr_.get_host_addr(),
               remote_addr_.get_port_number()));
    return SVS_RESULT_OK;
}

void CAC_Recv_Stream::record_key_frame(bool bSwitch)
{
    if (bSwitch)
    {
        // ��֡������10%������ൽ50%
        if (DEFAULT_FRAME_LOST_NUM > record_key_frame_rate_)
        {
            record_key_frame_rate_++;
        }
        SVS_LOG((SVS_LM_WARNING, "camera[%s] start record_lost_non_key_frame mode,"
                   "current lost num[%d].",
                   device_id_, Frame_Lost_Num[record_key_frame_rate_ - 1]));
    }
    else
    {
        // �رճ�֡
        record_key_frame_rate_    = 0;
        recv_frame_num_then_lost_ = 0;
        SVS_LOG((SVS_LM_WARNING, "camera[%s] stop record_lost_non_key_frame mode.",
                   device_id_));
    }

    record_key_frame_flag_ = bSwitch;
    return;
}

int32_t CAC_Recv_Stream::check_status()
{
    time_t curTime = ACE_OS::time(NULL);
    if ( ( (RT_STREAM_STATE_CLOSE == rt_stream_state_)
          || (RT_STREAM_STATE_INIT == rt_stream_state_) )
         && (VTDU_SEND_DATA_MIN_TIMEOUT < curTime - start_recv_time_) )
    {
        // ��ʱ��(5s)δ��ʼ����������VTDU��ʱ
        SVS_LOG((SVS_LM_WARNING, "camera[%s] not start, current status[%d].",
                  device_id_, rt_stream_state_));
        m_ulLastError |= ROUTE_STOP_REASON_MRU_CONNECT_ERROR;
        return SVS_ERR_CONNOVERTIME;
    }

    if ((RT_STREAM_STATE_RECV_VTDU_RESP == rt_stream_state_)
         && (VTDU_SEND_DATA_MIN_TIMEOUT < curTime - start_recv_time_))
    {
        // �ȴ�VTDU��Ӧ��ʱ
        SVS_LOG((SVS_LM_WARNING, "camera[%s] recv vtdu resp time out.",
                  device_id_));
        m_ulLastError |= ROUTE_STOP_REASON_MRU_CONNECT_ERROR;
        return SVS_ERR_CONNOVERTIME;
    }
    else if(RT_STREAM_STATE_RECV_VTDU_RESP == rt_stream_state_)// ���ڵȴ���Ӧ����δ��ʱ
    {
        // �ȴ�VTDU��Ӧ����δ��ʱ
        SVS_LOG((SVS_LM_INFO, "camera[%s] wait VTDU response.",
                  device_id_));
        return SVS_ERR_WAIT_RESP;
    }

    if ((RT_STREAM_STATE_RECV_STREAM == rt_stream_state_)
        && (VTDU_SEND_DATA_MIN_TIMEOUT < curTime - last_recv_packet_time_))
    {
        // ����5sδ���յ����ݣ���������ʱ
        SVS_LOG((SVS_LM_WARNING, "camera[%s] recv media time out.",
                  device_id_));
        m_ulLastError |= ROUTE_STOP_REASON_MRU_RECVTIMEOUT;
        return SVS_ERR_RECV_OVERTIME;
    }

    return SVS_RESULT_OK;
}

// ����enqueue_tmpfs��ʽ���
int32_t CAC_Recv_Stream::enqueue_tmpfs()
{
    if(NULL == p_media_buffer_)
    {
        SVS_LOG((SVS_LM_ERROR, "Enqueue temp fs failed.p_media_buffer_ is NULL"));

        return SVS_RESULT_FAILURE;
    }
    // ������ý�建������������벻�ɹ������tmpfs���뻺�����������ݷŵ�tmpfs�У��������
    ACE_Message_Block *tmpMb = p_media_buffer_->alloc_buffer( (NULL == recv_rt_media_buff_) );
    if ( NULL == tmpMb )
    {
        if ( NULL == recv_rt_media_buff_ )
        {
            return SVS_ERR_NOMEM;
        }

        // ��tmpfs���뻺����
        ACE_Message_Block *tmpfsMb = p_media_buffer_->alloc_tmpfs_buffer();
        // ����ʧ�ܣ���ֱ�ӷ��ش���
        if ( NULL == tmpfsMb )
        {
            return SVS_ERR_NOMEM;
        }

        // ��������������룬�������һ֡�ĳ���
        char *pBuffBase = (char*)recv_rt_media_buff_->base();
        SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)recv_rt_media_buff_->base();

        const uint32_t stripeWidth = CAC_RT_Record_Server::instance()->write_media_data_stripe_width();
        uint32_t posValue = pRecvMediaHeader->CurFrameStartPos - pRecvMediaHeader->MediaDataStartPos;
        SVS_NEW_ALL_HEADER *pAllHeader = (SVS_NEW_ALL_HEADER*)(void*)(pBuffBase + pRecvMediaHeader->LastFrameStartPos);
        if ( stripeWidth != posValue )
        {
            posValue = stripeWidth - posValue;
            pAllHeader->ExtendFrameHeader.nAdjustFrameSize = pAllHeader->ExtendFrameHeader.nFrameSize + posValue;
        }

        // ����ǰ�������ݷ���tmpfs�ļ���
        SVS_Recv_Media_Header *pTmpfsRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)tmpfsMb->base();
        int32_t writeResult = write(pTmpfsRecvMediaHeader->TmpfsFd,(void*)recv_rt_media_buff_->rd_ptr(), stripeWidth );

        pTmpfsRecvMediaHeader->HandlerIndex         = pRecvMediaHeader->HandlerIndex;           //д¼����
        pTmpfsRecvMediaHeader->FirstFrameRecvTime   = pRecvMediaHeader->FirstFrameRecvTime;     //��һ֡����ʱ��
        pTmpfsRecvMediaHeader->LastFrameRecvTime    = pRecvMediaHeader->LastFrameRecvTime;      //���һ֡����ʱ��
        pTmpfsRecvMediaHeader->LastPacketRecvTime   = pRecvMediaHeader->LastPacketRecvTime;     //���һ������ʱ��
        pTmpfsRecvMediaHeader->MediaDataStartPos    = pRecvMediaHeader->MediaDataStartPos;      //ý�����ݿ�ʼƫ��λ��
        pTmpfsRecvMediaHeader->RecvedFrameCount     = pRecvMediaHeader->RecvedFrameCount;       //�ѽ���֡����
        pTmpfsRecvMediaHeader->LastFrameStartPos    = pRecvMediaHeader->LastFrameStartPos;      //���һ����ȫ֡�ڻ�������ƫ��λ��
        pTmpfsRecvMediaHeader->IndexList            = pRecvMediaHeader->IndexList;              // �û����������б�

        pTmpfsRecvMediaHeader->CurFrameStartPos     = pRecvMediaHeader->CurFrameStartPos;       //��ǰ֡��ʼ�ڻ�������ƫ��λ��
        pTmpfsRecvMediaHeader->CurRecvFrameID       = pRecvMediaHeader->CurRecvFrameID;         //��ǰ����֡ID
        pTmpfsRecvMediaHeader->CurRecvedDataLen     = pRecvMediaHeader->CurRecvedDataLen;       //��ǰ֡�������ݳ���
        pTmpfsRecvMediaHeader->CurRecvedPacketCount = pRecvMediaHeader->CurRecvedPacketCount;   //��ǰ֡�ѽ��հ�����
        pTmpfsRecvMediaHeader->TmpfsDatalen         = stripeWidth;
        pTmpfsRecvMediaHeader->IsTmpfsMemory        = true;

        if ( (uint32_t)writeResult != stripeWidth )
        {
            SVS_LOG( ( SVS_LM_ERROR,
                "Camera move data to tmpfs failed. camera[%s].",
                device_id_ ) );
            return SVS_RESULT_FAILURE;
        }

        // ����ý�崦�������д���
        int32_t nRet = SVS_Media_Processor::instance().write_media(local_index_, tmpfsMb);
        if (SVS_RESULT_OK != nRet)
        {
            SVS_LOG((SVS_LM_ERROR, "Camera[%s] fail to write video frame. ", device_id_ ));
            (void)recv_rt_media_buff_->release();
            return SVS_RESULT_FAILURE;
        }
    }
    else
    {
        // ��ǰ����������ý�崦����
        enqueue_dio();
        recv_rt_media_buff_ = tmpMb;
    }

    (void)init_media_buffer();

    return SVS_RESULT_OK;
}
int32_t CAC_Recv_Stream::handle_input(ACE_HANDLE handle)
{
    // ����Ѿ��˳������˳�
    if(m_StopRecvFlag)
    {
        SVS_LOG((SVS_LM_INFO,
            "camera[%s] handle input. m_StopRecvFlag is true."
            "sockethandle[%d], handler[0x%08x].",
            device_id_,
            this->get_handle(),
            this));

        return SVS_RESULT_FAILURE;
    }

    if (ACE_INVALID_HANDLE == handle)
    {
        SVS_LOG((SVS_LM_ERROR,
                   "camera[%s] handle input failed, invaild ace handle."
                   "sockethandle[%d], handler[0x%08x].",
                   device_id_,
                   this->get_handle(),
                   this));

        return SVS_RESULT_FAILURE;
    }

    // ����ǵȴ�VTDU����Ӧ����Ҫ����յ����ǲ�����Ӧ��Ϣ������
    if (RT_STREAM_STATE_RECV_VTDU_RESP == rt_stream_state_)
    {
        return recv_media_response();
    }

    // ����Ҫ�յ����ݳ���
    if ( 0 == cur_frame_remain_size_ )
    {
        if (SVS_RESULT_OK != get_cur_frame_size())
        {
            // ���մ�����ͷ������,����ʧ�ܲ��ر�����
            m_ulLastError |= ROUTE_STOP_REASON_MRU_STREAM_ERROR;
            return SVS_RESULT_FAILURE;
        }

        if (0 == cur_frame_remain_size_)
        {
            // ��ͷδ�����꣬�ȴ��´ν���
            return SVS_RESULT_OK;
        }
    }

    // ��ǰ�������Ƿ��㹻���뵱ǰ֡,������������Ƚ���ǰ�������е�����д��
    if ((0 == remain_recv_buff_size_)
         || (cur_frame_remain_size_ > (remain_recv_buff_size_ - SVS_RECORD_FILE_ALIGN_LEN))
         || (NULL == recv_rt_media_buff_))
    {
        int32_t enqueueResult =  enqueue_tmpfs();

        // ������ڴ����ʧ�ܣ���ֱ�ӷ��سɹ����ȴ���һ��
        if ( SVS_ERR_NOMEM == enqueueResult )
        {
            alloc_buffer_failure_times_++;
            if(0 == (alloc_buffer_failure_times_ % 5000) )
            {
                SVS_LOG((SVS_LM_WARNING,
                    "camera[%s] handle input failed, no media buffer."
                    "sockethandle[%d], handler[0x%08x] error code[%d], "
                    "alloc buffer failure times[%u].",
                    device_id_,
                    this->get_handle(),
                    this,
                    enqueueResult,
                    alloc_buffer_failure_times_));
            }
            m_ulLastError |= ROUTE_STOP_REASON_MRU_EXCEPTION;

           return SVS_RESULT_OK;
        }// �������󣬷���ʧ��
        else if( enqueueResult != SVS_RESULT_OK )
        {
            SVS_LOG((SVS_LM_ERROR,
                       "camera[%s] handle input failed, enqueue_tmpfs failed."
                       "sockethandle[%d], handler[0x%08x] error code[%d].",
                       device_id_,
                       this->get_handle(),
                       this,
                       enqueueResult));
            return SVS_RESULT_FAILURE;
        }
    }


    return recv_media_data();
}

int32_t CAC_Recv_Stream::recv_media_data()
{
    if(NULL == recv_rt_media_buff_)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Receive media data failed.recv_rt_media_buff_[0x%08x].",
            recv_rt_media_buff_));

        return SVS_RESULT_FAILURE;
    }

    // ��������
    //1 TODO �������Ͽ�ʱ�Ƿ���Ҫ����
    char *pRecvBuffer = recv_rt_media_buff_->wr_ptr();
    int32_t recvResult = (int32_t)endpoint_.recv(pRecvBuffer, cur_frame_remain_size_);
    if ( 0 >= recvResult  )
    {
        int32_t iErrorCode = ACE_OS::last_error();
        if (!(EAGAIN == iErrorCode
            || ETIME == iErrorCode
            || EWOULDBLOCK == iErrorCode
            || ETIMEDOUT == iErrorCode
            || EINTR == iErrorCode))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Camera fail to recv media stream. camera[%s], errno[%d], recvlen[%u] bufferlen[%u].",
                device_id_, iErrorCode, cur_frame_remain_size_,
                remain_recv_buff_size_ - SVS_RECORD_FILE_ALIGN_LEN));
            return SVS_RESULT_FAILURE;
        }

        return SVS_RESULT_OK;
    }

    // ������Ϣ����
    last_recv_packet_time_ =  ACE_OS::time( NULL );
    ingress_stat_.updateLastRecvPacketTime(last_recv_packet_time_);
    char *pBuffBase = (char*)recv_rt_media_buff_->base();
    SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)pBuffBase;
    pRecvMediaHeader->CurRecvedDataLen   += (uint32_t)recvResult;
//    pRecvMediaHeader->LastPacketRecvTime = last_recv_packet_time_ - recv_time_remove_;

    // ʣ�໺������С����
    remain_recv_buff_size_ -= (uint32_t)recvResult;
    cur_frame_remain_size_ -= (uint32_t)recvResult;
    recv_rt_media_buff_->wr_ptr((uint32_t)recvResult);
    ingress_stat_.addRecvByteNum((uint32_t)recvResult);
    g_FluxStat.AddFlux(m_ulRecvThreadNo, (uint32_t)recvResult );
    if (0 == cur_frame_remain_size_)
    {
        // ��ǰ֡������ɣ���ϰ�
        int32_t nRet = combin_packet();
        if (0 != nRet)
        {
            SVS_LOG((SVS_LM_WARNING,
              "Camera[%s] combin packet fail. ",
              device_id_));
            m_ulLastError |= ROUTE_STOP_REASON_MRU_STREAM_ERROR;
            return SVS_RESULT_FAILURE;
        }

        // ����Ƿ���Ҫ�����������Ƿ���Ҫǿ��ˢ��ǰһ�����������
        check_record_time_slice();
    }

    print_recv_info();
    recv_byte_num_ += (uint32_t)recvResult;
    return SVS_RESULT_OK;

}

/*****************************************************************************
�� �� ��  : handle_close
��������  : ����ر��߳�
�������  : ACE_HANDLE handle,
            ACE_Reactor_Mask
�������  : ��
�� �� ֵ  : �ɹ�:0  ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Recv_Stream::handle_close(ACE_HANDLE handle, ACE_Reactor_Mask)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO,"camera[%s] close recv stream, handle[0x%x].",
               device_id_, handle));

    // �ر�����
    int32_t nRetVal = reactor()->remove_handler(this,
                                             ACE_Event_Handler::RWE_MASK
                                             | ACE_Event_Handler::DONT_CALL);
    if (-1 == nRetVal)
    {
        SVS_LOG((SVS_LM_WARNING,
                   "Remove handler failed while callback handleclose, camera[%s] ret=%d,"
                   "handle=[0x%x],this=0x%x, errorCode = %d.",
                   device_id_,
                   nRetVal,
                   this->get_handle(),
                   this,
                   ACE_OS::last_error()));
    }

    // ��ʣ�����ݷ���ý�崦����
    enqueue_dio( true );

    // �ر�����
    (void)endpoint_.close();
    endpoint_.set_handle(ACE_INVALID_HANDLE);

    // ֪ͨ����̹߳ر����
    if (m_StopRecvFlag)
    {
        // ��ֹ֤ͣʱֻ�ᷢ��һ�ιر�֪ͨ
        m_StopRecvFlag = false;

        SVS_LOG((SVS_LM_INFO,
            "camera[%s] close recv success, notify media processor.",
            device_id_));

        (void)SVS_Media_Processor::instance().recv_close_notify(local_index_);
    }

    // ��Ҫ��ת����close״̬
    rt_stream_state_ = RT_STREAM_STATE_CLOSE;

    return SVS_RESULT_OK;
}


/*** ������˽�к���ʵ�� ***/

/*****************************************************************************
�� �� ��  : init_media_buffer
��������  : ��ʼ��ý����ջ�����
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0  ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Recv_Stream::init_media_buffer()
{
    // SVS_TRACE();
    if( NULL == recv_rt_media_buff_ )
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to init recv media buffer. "
            "the pointer recv_rt_media_buff_ is NULL."));
        return SVS_RESULT_FAILURE;
    }

    // ʣ��������ݴ�С��ʼ��Ϊ���õ�������С + 4KB
    remain_recv_buff_size_ = CAC_RT_Record_Server::instance()->recv_media_buffer_size();

    recv_rt_media_buff_->reset();
    uint32_t basePos     = (uint32_t)(uint64_t)(char*)recv_rt_media_buff_->base();
    uint32_t alignPos    = basePos + sizeof( SVS_Recv_Media_Header );
    uint32_t extHeadSize = sizeof( SVS_EXTEND_FRAME_HEADER );

    // ��4K����,����4K���봦��
    if( 0 != (alignPos % SVS_RECORD_FILE_ALIGN_LEN) )
    {
        alignPos = (alignPos + SVS_RECORD_FILE_ALIGN_LEN) & 0xfffff000;
    }

    // ������������Ϣ��ʼ��
    SVS_Recv_Media_Header *pRecvMediaHeader =
        (SVS_Recv_Media_Header*)(void*)recv_rt_media_buff_->base();
    (void)ACE_OS::memset( (void*)pRecvMediaHeader, 0, sizeof(SVS_Recv_Media_Header) );

    pRecvMediaHeader->HandlerIndex = local_index_;            // ��ý�崦����ע��ʱ���ص�����
    pRecvMediaHeader->MediaDataStartPos = alignPos - basePos;
    pRecvMediaHeader->LastFrameStartPos = alignPos - basePos;
    pRecvMediaHeader->CurFrameStartPos = alignPos - basePos;
//    pRecvMediaHeader->FirstFrameRecvTime = ACE_OS::time( NULL ) - recv_time_remove_;

    recv_rt_media_buff_->rd_ptr( (uint32_t)(alignPos - basePos) );
    recv_rt_media_buff_->wr_ptr( (uint32_t)(alignPos - basePos) );

    // ��������������
    remain_recv_buff_size_ -= ( alignPos - basePos );

    // ���ӵ�һ��֡����չ֡ͷ
    char *pAllHeader = (char*)(recv_rt_media_buff_->rd_ptr() + extHeadSize);
    (void)ACE_OS::memset(pAllHeader, 0x0, extHeadSize);

    // дָ��ʼ��ָ����һ֡�Ŀ�ʼλ�ã���������չ֡ͷ
    recv_rt_media_buff_->wr_ptr( extHeadSize );

    // ��Ӧ�ģ���������СΪӦ�ü�Сһ����չ֡����
    remain_recv_buff_size_ -= extHeadSize;
    pRecvMediaHeader->IsTmpfsMemory = false;
    pRecvMediaHeader->TmpfsFd = -1;

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : get_cur_frame_size
��������  : ����������һ����ͷ�����������֡��������ͷ���ĳ���
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0  ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Recv_Stream::get_cur_frame_size()
{
    ACE_Time_Value tv(0, 100 * 1000);

    // ���ð�ͷ���ռ�¼
    (void)ACE_OS::memset((char*)&m_packHeader, 0x0, sizeof(m_packHeader));

    char *pRecvBuff    = (char*)&m_packHeader;
    size_t header_size = sizeof(AVS_PACK_HEAD);

    int32_t nRet = (int32_t)endpoint_.recv(pRecvBuff, header_size, MSG_PEEK, &tv);
    if ( 0 >= nRet )
    {
        int32_t iErrorCode = ACE_OS::last_error();
        if (!(EAGAIN == iErrorCode
            || ETIME == iErrorCode
            || EWOULDBLOCK == iErrorCode
            || ETIMEDOUT == iErrorCode
            || EINTR == iErrorCode))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Camera fail to recv media header. "
                "camera[%s], errno[%d], recvlen[%d:%u].",
                device_id_,
                iErrorCode,
                nRet,
                header_size));

            return SVS_RESULT_FAILURE;
        }

        SVS_LOG((SVS_LM_INFO,
            "camera[%s] not recv all packet header,iErrorCode[%d], nRet [%d].",
            device_id_,
            iErrorCode,
            nRet));

        return SVS_RESULT_OK;
     }

    // ������ܵ������ֽ�
    if ((int32_t)header_size != nRet)
    {

        SVS_LOG((SVS_LM_INFO,
            "camera[%s] not recv all packet header, recvlen[%d:%u].",
            device_id_,
            nRet,
            header_size));

        return SVS_RESULT_OK;
    }
    if (0xFF == m_packHeader.m_btIsKeyFrame)
    {
        int32_t iRet = 0;
        iRet = CAC_Bakup_Record_Dev_Manager::instance()->proc_eos_packet(router_id_);
        return iRet;
    }

    if (!check_pack_head(&m_packHeader))
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] recv packet header fail, packet header invalid,"
            "FrameID[%u] FrameSize[%u] KeyFrame[%d] PackID[%d] PackCount[%d] PackSize[%d],"
            "Route[%d] VTDU addr[%s:%d] last frame id[%u].",
            device_id_,
            m_packHeader.m_dwFrameID,
            m_packHeader.m_dwFrameLen,
            m_packHeader.m_btIsKeyFrame,
            m_packHeader.m_btPackID,
            m_packHeader.m_btPackCount,
            m_packHeader.m_wPackDataLen,
            router_id_,
            remote_addr_.get_host_addr(),
            remote_addr_.get_port_number(),
            cur_recv_frame_id_));

        ingress_stat_.addLostPacketNum(1);
        return SVS_RESULT_FAILURE;
    }

    // ���ݰ����������֡�ܳ���(����ͷ)
    uint32_t modRet = (m_packHeader.m_dwFrameLen % SEGMENT_MAX_SIZE);
    if(0 == modRet)
    {
        modRet = SEGMENT_MAX_SIZE;
    }

    // ���ݰ����������֡�ܳ���(����ͷ)
    cur_frame_remain_size_ = (m_packHeader.m_btPackCount - 1) * SVS_STREAM_DATAGRAM_MAX
                            + modRet + header_size;

    ingress_stat_.updateFrameSize(cur_frame_remain_size_);

    // ��ǰ֡ID
    cur_recv_frame_id_          = m_packHeader.m_dwFrameID;
    cur_frame_start_recv_time_  = ACE_OS::time(NULL);

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : recv_media_response
��������  : ����VTDU���͵�ý��ע����Ӧ��Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0  ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Recv_Stream::recv_media_response()
{
    SVS_TRACE();

    // ���Ƚ�����Ӧ��Ϣ
    char *pBuffer = (char*)&m_MediaResp + m_MediaRespLen;
    size_t rcvLen = sizeof(SVS_ERROR_MSG) - m_MediaRespLen;
    ACE_Time_Value tv(0, 100 * 1000);

    int32_t nRet = (int32_t)endpoint_.recv_n(pBuffer, rcvLen, &tv);
    if ( (0 >= nRet) || (nRet != (int32_t)rcvLen))
    {
        int32_t iErrorCode = ACE_OS::last_error();
        if (!(EAGAIN == iErrorCode
            || ETIME == iErrorCode
            || EWOULDBLOCK == iErrorCode
            || ETIMEDOUT == iErrorCode
            || EINTR == iErrorCode))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Camera fail to recv media response."
                " camera[%s], errno[%d], recvlen[%u:%u].",
                device_id_,
                iErrorCode,
                nRet,
                rcvLen));
            return SVS_RESULT_FAILURE;
        }

        if (0 < rcvLen)
        {
            m_MediaRespLen += (size_t)nRet;
        }

        SVS_LOG((SVS_LM_WARNING,
            "Receive part response message from VTDU."
            "camera[%s] ,real recv length[%d], need recv length[%u].",
            device_id_, nRet, rcvLen));

        return SVS_RESULT_OK;
    }

    if (0 != m_MediaResp.ErrorCode)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Receive error response message from VTDU."
            "camera[%s],error code[%d],VTDU[%s:%d].",
            device_id_,
            m_MediaResp.ErrorCode,
            remote_addr_.get_host_addr(), remote_addr_.get_port_number()));

        return SVS_RESULT_FAILURE;
    }

    ACE_INET_Addr localAddr;
    (void)endpoint_.get_local_addr( localAddr ) ;

    // ��״̬ת��Ϊ��ʼ����ý����״̬
    rt_stream_state_ = RT_STREAM_STATE_RECV_STREAM;
    last_recv_packet_time_ = time(NULL);
    ingress_stat_.updateLastRecvPacketTime(last_recv_packet_time_);

    // �����ǰ��¼�񱸷�,������
    (void)CAC_Bakup_Record_Dev_Manager::instance()->play_media_request(router_id_);

    SVS_LOG((SVS_LM_INFO,
        "[RECV]Success to receive media response from VTDU. "
        "camera[%s], address[%s:%d], VTDU[%s:%d].",
        device_id_,
        localAddr.get_host_addr(), localAddr.get_port_number(),
        remote_addr_.get_host_addr(), remote_addr_.get_port_number()));

    return SVS_RESULT_OK;
}

// ����ͷ�Ƿ���ȷ
bool CAC_Recv_Stream::check_pack_head(const AVS_PACK_HEAD *pHeader)const
{
    // �������Ϸ�
    if (SVS_STREAM_DATAGRAM_MAX < pHeader->m_wPackDataLen)
    {
        return false;
    }

    // ���֡��Ϊ0,����-1
    if ( 0 == pHeader->m_dwFrameLen )
    {
        return false;
    }

    // ������Ӧ��С�ڵ���֡����
    if (pHeader->m_wPackDataLen > pHeader->m_dwFrameLen)
    {
        return false;
    }

    // ��ID����Ϊ0
    if (0 != pHeader->m_btPackID)
    {
        return false;
    }

    // �ؼ�֡��ʶֻ��1��0
    if ((0 != pHeader->m_btIsKeyFrame) && (1 != pHeader->m_btIsKeyFrame))
    {
        return false;
    }

    // ��IDӦ��С�ڰ���
    if (pHeader->m_btPackID >= pHeader->m_btPackCount)
    {
        return false;
    }

    // ǰn-1���������ݴ�СӦ�õ���SEGMENT_MAX_SIZE.
    if (pHeader->m_btPackID < (pHeader->m_btPackCount - 1)
        && ( SEGMENT_MAX_SIZE != pHeader->m_wPackDataLen ))
    {
        return false;
    }

    return true;
}

/*****************************************************************************
�� �� ��  : combin_packet
��������  : ���յ����������
�������  :
�������  : ��
�� �� ֵ  : �ɹ�:0  ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Recv_Stream::combin_packet()
{
    // SVS_TRACE();
    if (NULL == recv_rt_media_buff_)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The member pointer recv_rt_media_buff_ is NULL."));
        return -1;
    }

    char *pBuffBase = (char*)recv_rt_media_buff_->base();
    SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)pBuffBase;
    SVS_NEW_ALL_HEADER *pAllHeader =
        (SVS_NEW_ALL_HEADER*)(void*)(pBuffBase + pRecvMediaHeader->CurFrameStartPos);
    size_t extHeaderSize = sizeof( SVS_EXTEND_FRAME_HEADER );

    // ���г�֡�ж�,���ܶ����ؼ�֡����Ƶ֡
    if ((record_key_frame_flag_)
         && (1 != pAllHeader->PacketHeader.m_btIsKeyFrame)
         && (0 != pAllHeader->FrameHeaer.nVideoSize))
    {
        recv_frame_num_then_lost_++;
        //�����֡�ʱ�ʶ���󣬸���Ϊ����֡��.for pclint
        if(record_key_frame_rate_ > DEFAULT_FRAME_LOST_NUM)
        {
            record_key_frame_rate_ = DEFAULT_FRAME_LOST_NUM;
        }

        if (recv_frame_num_then_lost_ >= Frame_Lost_Num[record_key_frame_rate_ - 1])
        {
            // ��ǰ֡��Ҫ����
            SVS_LOG((SVS_LM_WARNING,
                "camera[%s] in record_lost_non_key_frame mode, lost frame[%u] "
                "size[%u] packet num[%u].",
                device_id_, pAllHeader->PacketHeader.m_dwFrameID,
                pAllHeader->PacketHeader.m_dwFrameLen, pAllHeader->PacketHeader.m_btPackCount));

            // �����趨���һ֡��λ�úͻ���������
            recv_rt_media_buff_->wr_ptr( ((char*)pAllHeader + extHeaderSize));
            remain_recv_buff_size_ += pRecvMediaHeader->CurRecvedDataLen;

            // ��λ��ǰ֡����Ϣ
            pRecvMediaHeader->CurRecvedPacketCount = 0;
            pRecvMediaHeader->CurRecvedDataLen = 0;
            pRecvMediaHeader->CurRecvFrameID = 0;
            return SVS_RESULT_OK;
        }
    }

    int32_t nRet = SVS_RESULT_OK;
    nRet = check_timetick();
    if(1 != nRet)
    {
        // ��λ��ǰ֡����Ϣ
        pRecvMediaHeader->CurRecvedPacketCount = 0;
        pRecvMediaHeader->CurRecvedDataLen = 0;
        pRecvMediaHeader->CurRecvFrameID = 0;

        return nRet;
    }

    // �����չ֡ͷ������
    pAllHeader->ExtendFrameHeader.nFrameSize = pRecvMediaHeader->CurRecvedDataLen;
    pAllHeader->ExtendFrameHeader.nFrameSize += extHeaderSize;
    pAllHeader->ExtendFrameHeader.nAdjustFrameSize = pAllHeader->ExtendFrameHeader.nFrameSize;
    pAllHeader->ExtendFrameHeader.nPacketCount = pAllHeader->PacketHeader.m_btPackCount;
    (void)ACE_OS::memset( (void*)pAllHeader->ExtendFrameHeader.ReserveData,
                         0,
                         sizeof( pAllHeader->ExtendFrameHeader.ReserveData ) );

    // ����֡������1
    pRecvMediaHeader->RecvedFrameCount++;

    // �ѽ�����һ��֡,����֡�ϲ�
    pRecvMediaHeader->LastFrameRecvTime = pRecvMediaHeader->LastPacketRecvTime;
    pRecvMediaHeader->LastFrameStartPos = pRecvMediaHeader->CurFrameStartPos;
    pRecvMediaHeader->CurFrameStartPos = pAllHeader->ExtendFrameHeader.nFrameSize
                                      + pRecvMediaHeader->CurFrameStartPos;

    // ��λ��ǰ֡����Ϣ
    pRecvMediaHeader->CurRecvedPacketCount = 0;
    pRecvMediaHeader->CurRecvedDataLen = 0;
    pRecvMediaHeader->CurRecvFrameID = 0;

    // ����������㹻��������Ƴ�һ����չ֡ͷ�Ĵ�С��׼�������µ�֡
    if ((SVS_RECORD_FILE_ALIGN_LEN < remain_recv_buff_size_)
        && (SVS_RECORD_FILE_ALIGN_LEN < remain_recv_buff_size_ - extHeaderSize))
    {
        recv_rt_media_buff_->wr_ptr( extHeaderSize );

        // ��Ӧ�ģ���������СΪӦ�ü�Сһ����չ֡����
        remain_recv_buff_size_ -= extHeaderSize;
    }


    ingress_stat_.addRecvFramNum(1, pAllHeader->FrameHeaer.bKeyFrame);
    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : enqueue_dio
��������  : �����յ�ý����������ѹ��д�ļ�����
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0  ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
void CAC_Recv_Stream::enqueue_dio( bool bLastBufferFlag )
{
    // SVS_TRACE();
    if ( NULL == recv_rt_media_buff_ )
    {
        return ;
    }

    if ( bLastBufferFlag )
    {
        // for lint ������ACE�е��࣬������ģ��ܱ�֤�����������ע�͸澯��
        ((SVS_Media_Block*)recv_rt_media_buff_)->SetLastMemoryBufferFlag( true );//lint !e1774
    }

    char *pBuffBase = (char*)recv_rt_media_buff_->base();
    SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)pBuffBase;

    // û�н��յ�һ������֡
    if( 0 == pRecvMediaHeader->RecvedFrameCount )
    {
        (void)recv_rt_media_buff_->release();
        recv_rt_media_buff_    = NULL;
        remain_recv_buff_size_ = 0;
        SVS_LOG((SVS_LM_WARNING, "Recv buffer has no frame."));
        return ;
    }

    const uint32_t stripeWidth = CAC_RT_Record_Server::instance()->write_media_data_stripe_width();
    uint32_t posValue = pRecvMediaHeader->CurFrameStartPos - pRecvMediaHeader->MediaDataStartPos;
    SVS_NEW_ALL_HEADER *pAllHeader = (SVS_NEW_ALL_HEADER*)(void*)(pBuffBase + pRecvMediaHeader->LastFrameStartPos);
    if ( stripeWidth != posValue )
    {
        posValue = stripeWidth - posValue;
        pAllHeader->ExtendFrameHeader.nAdjustFrameSize = pAllHeader->ExtendFrameHeader.nFrameSize + posValue;
    }

    // ��дָ����д���,��ֹ���ַ�4k���������
    char *wrPtr = (char*)((char*)(pAllHeader) + pAllHeader->ExtendFrameHeader.nAdjustFrameSize );
    recv_rt_media_buff_->wr_ptr( wrPtr );

    // ����ý�崦�������д���
    int32_t nRet = SVS_Media_Processor::instance().write_media(local_index_, recv_rt_media_buff_);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
                  "Camera[%s] fail to write video frame. ",
                  device_id_ ));
        (void)recv_rt_media_buff_->release();
        recv_rt_media_buff_    = NULL;
        remain_recv_buff_size_ = 0;
        return ;
    }

    remain_recv_buff_size_ = 0;
    recv_rt_media_buff_    = NULL;
    return ;
}

int32_t CAC_Recv_Stream::check_timetick()
{
    if(NULL == recv_rt_media_buff_)
    {
        return -1;
    }

    char *pBuffBase = (char*)recv_rt_media_buff_->base();
    SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)pBuffBase;
    SVS_NEW_ALL_HEADER *pAllHeader =
        (SVS_NEW_ALL_HEADER*)(void*)(pBuffBase + pRecvMediaHeader->CurFrameStartPos);
    bool newIndexSegFalg = false;

    uint32_t currTimeTick = pAllHeader->FrameHeaer.nTimeTick;

    //ֻ����Ƶ֡������
    if( 0 == pAllHeader->FrameHeaer.nVideoSize)
    {
        last_recv_frame_id_ = cur_recv_frame_id_;
        if(0 == m_last_Adjust_timetick)
        {
            m_last_Adjust_timetick  = currTimeTick;
        }
        //��Ƶ֡��ʱ�������һ����Ƶ֡һ��
        pAllHeader->FrameHeaer.nTimeTick = m_last_Adjust_timetick;
        return 1;
    }

    // ���ʱ�����С
    if (m_last_frame_timetick > currTimeTick)
    {
            SVS_LOG((SVS_LM_WARNING,
                "Conbin packet failed.frame timetick or frame id is invalid."
                "camera[%s], last FrameId[%u],current FrameId[%u],last frame timetick[%u],"
                " current frame timetick[%u].",
                device_id_, last_recv_frame_id_,cur_recv_frame_id_,
                m_last_frame_timetick, currTimeTick));
            newIndexSegFalg = true;

    }
    else if((0 != m_last_frame_timetick) && ((currTimeTick - m_last_frame_timetick) > SVS_FRAME_TIMETICK_INTERVAL_MAX))// �������ͻȻ��ĺܴ�
    {
        SVS_LOG((SVS_LM_WARNING,
            "Conbin packet failed.frame timetick or frame id is invalid."
            "camera[%s], last FrameId[%u],current FrameId[%u],last frame timetick[%u],"
            " current frame timetick[%u].",
            device_id_, last_recv_frame_id_,cur_recv_frame_id_,
            m_last_frame_timetick, currTimeTick));
            newIndexSegFalg = true;
    }
    else if(0 == m_last_Adjust_timetick)
    {
        m_last_Adjust_timetick  = currTimeTick;
        newIndexSegFalg = false;
    }

    if(true == newIndexSegFalg)
    {
        m_last_Adjust_timetick = m_last_Adjust_timetick + SVS_FRAME_DEFAULT_TIMETICK;
    }
    else
    {
        m_last_Adjust_timetick = m_last_Adjust_timetick + (currTimeTick - m_last_frame_timetick);
    }

    pAllHeader->FrameHeaer.nTimeTick = m_last_Adjust_timetick;

    count_frame_info(newIndexSegFalg);

    m_last_frame_timetick = currTimeTick;

    // ����ǵ�һ�ν������ݣ���¼��һ֡��ʱ����Ϣ
    if(0 == m_recv_first_fram_time)
    {
        m_recv_first_fram_time = time(NULL) - recv_time_remove_;
        m_first_frame_timetick = m_last_frame_timetick;
    }
    // �����ǰ֡�ǻ��������յĵ�һ֡�����¼ý�建����յ�һ֡ʱ��
    time_t currFrameTime = time(NULL) - recv_time_remove_;
    if(0 == pRecvMediaHeader->FirstFrameRecvTime)
    {
        pRecvMediaHeader->FirstFrameRecvTime = currFrameTime;
    }

    pRecvMediaHeader->LastPacketRecvTime = currFrameTime;

    last_recv_frame_id_ = cur_recv_frame_id_;

    return 1;
}

/*****************************************************************************
 �� �� ��  : check_record_time_slice
 ��������  : ���ʱ��Ƭ�Ƿ񵽴������Ƿ���Ҫ����������¼���ļ�:
             1��ÿ30S����һ����������Ϊ��Ҫ������һ�����������ݣ�
                �����Ȱ�������¼��������д������ʱ��FileEgress���¡�
             2��ÿСʱ�л�һ��¼���ļ�����Ҫ����һ��Сʱ��ý������ѹ����̣�
                �����д����һ�����������ݡ�
 �������  : VOID
 �������  : VOID
 �� �� ֵ  : VOID
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2009��9��2��
    �޸�����   : �����ɺ���

*****************************************************************************/
void CAC_Recv_Stream::check_record_time_slice()
{
    if (NULL == recv_rt_media_buff_)
    {
        return ;
    }

//    time_t nowTime = ACE_OS::time( NULL ) - recv_time_remove_;
    time_t currFrameTime =0;
    if(RT_RECORD_TIMETICK_LOCAL == CAC_RT_Record_Server::instance()->GetRecordTimetickFlag())
    {
        currFrameTime = ACE_OS::time( NULL ) - recv_time_remove_;
    }
    else
    {
        currFrameTime = m_recv_first_fram_time + ((time_t)(m_last_frame_timetick - m_first_frame_timetick) / 1000);
    }
    time_t nowTime = currFrameTime;
    int32_t nRet = SVS_RESULT_OK;
    // ����Ƿ���Ҫ��������
    if( SVS_RECORDFILE_SLICE_TIME <= ( nowTime - last_check_index_time_) )
    {
        nRet = updateCluster(nowTime);
        if(1 != nRet)
        {
            return;
        }
    }

    // ����Ƿ���Ҫ����¼���ļ�
    if (BIG_RECORDFILE_TIME_SLICE <= ( nowTime - last_check_record_time_))
    {
        nRet = updateRecordFile(nowTime);
        if(1 != nRet)
        {
            return;
        }
    }
}


void CAC_Recv_Stream::print_recv_info()
{
    ACE_Time_Value nowTime = ACE_OS::gettimeofday();

    // ��δ��ͳ��ʱ��,����
    if( PRINT_RECV_PACKET_STATUS_INTERVAL_TIME > nowTime.sec() - last_print_time_.sec())
    {
        return ;
    }

    ACE_Date_Time tmOld( last_print_time_ );
    ACE_Date_Time tmNow( nowTime );

    double rate = (nowTime.sec() - last_print_time_.sec()) + 1.000001;

    rate = ((double)recv_byte_num_ * RTRECORD_UNIT_BYTE / rate) / RTRECORD_UNIT_K;

    double lostFrameRate =0;

    if(0 != total_frame_num_)
    {
       lostFrameRate = ((double)lost_frame_num_ / (double)total_frame_num_)*100;
    }
    SVS_LOG( (SVS_LM_INFO,
        "Camera recv pack status. Camera[%s] from[%04d-%02d-%02d %02d:%02d:%02d]"
        " to [%04d-%02d-%02d %02d:%02d:%02d] "
        "recv byte[%d], bit rate[%8.2fKbit/s], "
        "total frame num[%u], lost frame num[%u], lost frame rate[%8.5f%%].",
        device_id_,
        tmOld.year(), tmOld.month(), tmOld.day(), tmOld.hour(), tmOld.minute(), tmOld.second(),
        tmNow.year(), tmNow.month(), tmNow.day(), tmNow.hour(), tmNow.minute(), tmNow.second(),
        recv_byte_num_,rate ,
        total_frame_num_, lost_frame_num_, lostFrameRate) );

    last_print_time_ = nowTime;
    recv_byte_num_ = 0;
    lost_frame_num_ = 0;
    total_frame_num_ = 0;
    return;
}

void CAC_Recv_Stream::recv_rate_info(time_t & statStartTime,
                                time_t & statEndTime,
                                uint32_t & recvByteNum,
                                uint32_t & lostFrameNum,
                                uint32_t & totalFrameNum)
{
    statStartTime = last_print_time_.sec();
    statEndTime = time(NULL);

    recvByteNum = recv_byte_num_;

    if(lost_frame_num_ > total_frame_num_)
    {
       lost_frame_num_ = 0;
       total_frame_num_ = 0;
    }

    lostFrameNum = lost_frame_num_;
    totalFrameNum = total_frame_num_;

    return;
}

void CAC_Recv_Stream::count_frame_info(bool addIndexFlag)
{
    // �������Ҫ�����µ�һ���������ż��㶪֡��
    if(addIndexFlag)
    {
        return;
    }

    // �ж��Ƿ�֡
    if((cur_recv_frame_id_ != (last_recv_frame_id_ + 1)) && (0 != last_recv_frame_id_))
    {
        // ��֡�������µ�֡ID��ǰ���յ���֡ID�Ĳ�ֵ��1
        uint32_t frameIdNum = (cur_recv_frame_id_ - last_recv_frame_id_) - 1;
        ingress_stat_.addLostFramNum(frameIdNum);
        lost_frame_num_ += frameIdNum;
        total_frame_num_ += frameIdNum;
    }
    else
    {
        total_frame_num_++;
    }

    return;
}

/*****************************************************************************
 �� �� ��  : updateCluster
 ��������  : ʱ�䵽�˸��´ص�ʱ��(��ǰ��30sһ��¼���ļ�����30s�̶ȣ��л���)���
             ¼�µĴ���ý�建���е�λ��
 �������  : time_t nowTime:��ǰʱ��
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Recv_Stream::updateCluster(time_t nowTime)
{
    // ����ǵ�һ�θ�������
    if (0 == last_check_index_time_)
    {
        last_check_index_time_ = nowTime - nowTime % SVS_RECORDFILE_SLICE_TIME;
        return SVS_RESULT_OK;
    }

    // ��ǰ���Ѹ��¹�������¼����������
    if( last_check_index_time_ == nowTime )
    {
        return SVS_RESULT_OK;
    }

    char *pBuffBase = (char*)recv_rt_media_buff_->base();
    SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)pBuffBase;
    if (NULL == pRecvMediaHeader->IndexList)
    {
        // IndexList�ڴ����룬Ϊ���쳣����������Ҫ�ͷŴ���ķ����ԣ���SVS_Recv_Media_Header��releas���ͷ�
        pRecvMediaHeader->IndexList = SVS_NEW(pRecvMediaHeader->IndexList);
        if (NULL == pRecvMediaHeader->IndexList)
        {
            SVS_LOG((SVS_LM_WARNING,
                "camera[%s] check update index time fail, alloc index list fail.",
                device_id_));
            return SVS_RESULT_OK;
        }
    }

    SVS_Media_Index curIndex;
    curIndex.LastIndexFrameID = cur_recv_frame_id_;
    curIndex.LastIndexRecvTime = pRecvMediaHeader->LastPacketRecvTime;
    pRecvMediaHeader->IndexList->push_back(curIndex);

    last_check_index_time_ = nowTime;

    return 1;
}

/*****************************************************************************
 �� �� ��  : updateRecordFile
 ��������  : ʱ�䵽���л�¼����ļ���ʱ��(��ǰ��1��Сʱһ��¼���ļ��������л��ļ�)
             ��ѵ�ǰ��ý�建���ǿ��ѹջ
 �������  : time_t nowTime:��ǰʱ��
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Recv_Stream::updateRecordFile(time_t nowTime)
{
    // ����ǵ�һ���½�¼���ļ�
    if (0 == last_check_record_time_)
    {
        last_check_record_time_ = nowTime - nowTime % BIG_RECORDFILE_TIME_SLICE;

        return SVS_RESULT_OK;
    }

    // ��ǰ���Ѹ��¹�¼���ļ�����������
    if( last_check_record_time_ == nowTime )
    {
        return SVS_RESULT_OK;
    }

    // ǿ�ư���һСʱ��¼���ļ�д�����
    int32_t enqueueResult =  enqueue_tmpfs();

    // ������ڴ����ʧ�ܣ���ֱ�ӷ��سɹ����ȴ���һ��
    if ( SVS_ERR_NOMEM == enqueueResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Camera[%s] fail to enqueue data, no media buffer. ",
            device_id_ ));

        return SVS_RESULT_OK;
    }// �������󣬷���ʧ��
    else if( enqueueResult != SVS_RESULT_OK )
    {
        SVS_LOG((SVS_LM_ERROR,
            "camera[%s] fail to enqueue data, enqueue_tmpfs failed. error code[%d].",
            device_id_,
            enqueueResult));
        return SVS_RESULT_OK;
    }

    last_check_record_time_ = nowTime;

    return SVS_RESULT_OK;
}

/*-End of Class Placeholder: CAC_Recv_Stream -*/
//===============================CAC_Recv_Stream END==========================


