#include "vms/vms.h"
#include "svs_auto_reference_ptr.h"
#include "svs_config.h"
#include "svs_timer.h"
#include "svs_stat_manager.h"
#include "svs_record_index.h"
#include "svs_real_record_task.h"
#include "svs_record_index.h"
#include "svs_real_record_lens.h"
#include "svs_real_record_server.h"
#include "svs_record_inform.h"
#include <list>
#include "svs_log_msg.h"
#include "svs_rt_record_common.h"
#include "svs_media_processor.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"

using namespace SVS_SS_UTILITIES;
#include <string>
using namespace std;

// �¼�����
const char *g_strEventName[] =
{
    "ManualRecord",
    "Alarm",
    "VideoLose/Resume",
    "ImageMove",
    "CameraOnline/Offline",
    "ImageShaded/Unshaded",
    "DiskAlarm",
    "SDCARDFail",
    "SDCARDBadBlock",
    "IntrusionDetect",
    "TripLineDetection",
    "StaticDetection",
    "RemoveDetection",
    "PresenceDetection",
    "PathDetection",
    "SpeedDetection",
    "\0"
};
// ¼������
const char *g_strRecordType[] =
{
    "FORBID RECORD",
    "ALWAYS RECORD",
    "EVENT RECORD",
    "MANUAL RECORD",
    "\0"
};

typedef CAC_Auto_Reference_Ptr <CAC_RT_Record_Device_Manager,
                                CAC_Realtime_Record_Device *> SVS_RT_RECORD_DEVICE_AUTO_PTR;

//=========================CAC_Realtime_Record_Device BEGIN===================
#ifdef UNITTEST
CAC_Realtime_Record_Device::CAC_Realtime_Record_Device()
{
}
#endif

CAC_Realtime_Record_Device::CAC_Realtime_Record_Device(
    const char *strLensId,
    uint32_t encoderType,
    uint32_t nIndex )
    : CSVS_Record_Lens(strLensId, nIndex)
{
    record_flag_            = RECORD_TYPE_PROBID;
    record_start_time_      = 0;

    //��ʼ���¼�����VECTOR
    EVENTMAP emap;
    for (uint32_t i = 0; i < RECORD_EVENT_TYPE_MAX; i++)
    {
        event_vector_.push_back(emap);
    }
    total_event_        = 0;
    event_begin_time_   = 0;
    event_over_time_    = 0;

    referenc_count_     = 1;
    state_              = RECORD_STATE_INIT;
    encoder_type_       = (uint8_t)encoderType;

    stream_id_          = 0;
    status_start_time_  = time(NULL);

    // ==========Զ�̵�����ز���=========
    request_route_times_        = 0;      // ����·�ɴ���
    connect_vtdu_times_         = 0;      // ����VTDU����
    connect_vtdu_timeout_times_ = 0;      // ����VTDU��ʱ����
    recv_media_data_timeout_times_ = 0;


    // ===========�¼�¼����ز���======
    // �¼�ǰ¼������
    event_record_before_seconds_ = 0;
    // �¼���¼������
    event_record_after_seconds_ = 0;
    // �¼���ʼʱ��
    event_record_start_time_ = 0;
    // �¼�����ʱ��
    event_record_end_time_ = 0;

    m_ulIOHandlerIndex = (uint32_t)IO_INVALID_HANDLE;
    // �¼�¼�����ʱ����ʱ��
    event_record_over_timer_id_ = -1;
    // �¼�¼�������ʧ��ʱ��
    event_record_lost_timer_id_ = -1;
    device_stat_.init(str_frontdev_id_);
    request_route_overtime_len_ = REQUEST_ROUTER_INTERVAL_TIME;
}

CAC_Realtime_Record_Device::~CAC_Realtime_Record_Device()
{
    try
    {
        // �����ǰ��¼���¼���ʧ��ʱ��
        (void)kill_event_record_lost_timer();
    }
    catch(...)
    {
    }
}

/*****************************************************************************
 �� �� ��  : init
 ��������  : ��ʼ����ͷ��Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true,ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_Realtime_Record_Device::init()
{
    // �����µ�¼���¼���ʧ����ʱ��,�������:A/Cͬʱ����B�澯
    ACE_Time_Value tv(SVS_EVENT_RECORD_LOST_INTERVAL, 0);
    (void)set_event_record_lost_timer(tv);

    //�Ѿ���ʼ���������ٴγ�ʼ��
    if ( (RECORD_STATE_INIT < state_)
        && (FRONT_DEVICE_STATE_STOP != state_))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stop to initialize camera recording, "
            "The Camera has been initialized.CameraId[%s], "
            "event start time[%d], "
            "event end time[%d], "
            "prerecord time length[%d], event record time length[%d].",
            str_frontdev_id_,
            event_begin_time_,
            event_over_time_,
            event_record_before_seconds_,
            event_record_after_seconds_));

        return true;
    }

    SVS_LOG((SVS_LM_INFO,
        "Initialize camera recording succeeded, "
        "cameraId[%s], event start time[%d], event end time[%d], "
        "prerecord time length[%d], event record time length[%d]s.",
        str_frontdev_id_,
        event_begin_time_,
        event_over_time_,
        event_record_before_seconds_,
        event_record_after_seconds_));

    (void)set_status(RECORD_STATE_REQUEST_ROUTER);

    return true;
}

/*****************************************************************************
 �� �� ��  : reset
 ��������  : ��λ��ͷ��Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::reset()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO,"Begin to reset camera.CameraId[%s].",str_frontdev_id_));

    // �����ǰ��¼���¼���ʧ��ʱ��,�������:A/Cͬʱ����B�澯
    (void)kill_event_record_lost_timer();

    SVS_LOG((SVS_LM_DEBUG, "Begin to clear alarm event list.CameraId[%s].", str_frontdev_id_));
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    for (size_t i = 0; i < event_vector_.size() && i < RECORD_EVENT_TYPE_MAX; i++)
    {
        event_vector_[i].clear();
    }
    event_vector_.clear();

    SVS_LOG((SVS_LM_INFO,"Finish to reset camera.CameraId[%s].",str_frontdev_id_));
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : update_record_parameter
 ��������  : ����¼����� �¼�ǰ¼��ʱ��/�¼���¼��ʱ��
 �������  : uint32_t nBeforeSeconds:Ԥ¼ʱ��
             uint32_t nAfterSeconds:��¼ʱ��
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::update_record_parameter(
    uint32_t nBeforeSeconds,
    uint32_t nAfterSeconds )
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    event_record_before_seconds_ = (time_t)nBeforeSeconds;
    event_record_after_seconds_ = (time_t)nAfterSeconds;

    SVS_LOG((SVS_LM_INFO,
        "Update record parmater finished, "
        "CameraId[%s],"
        "prerecord time length[%u], "
        "postrecord time length[%u].",
        str_frontdev_id_,
        event_record_before_seconds_,
        event_record_after_seconds_));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : send_stop_recv_msg
 ��������  : ����:����ֹͣ��Ϣ�������߳���д¼���߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::send_stop_recv_msg()
{
    SVS_TRACE();

    // ��ý�崦�������Ƴ�ע��ľ��
    if ((uint32_t)IO_INVALID_HANDLE != m_ulIOHandlerIndex)
    {
        int32_t nRet = SVS_Media_Processor::instance().stop_record(m_ulIOHandlerIndex);
        if (SVS_RESULT_OK != nRet)
        {
            SVS_LOG((SVS_LM_WARNING, "device[%s] stop record fail, io handle[%u].",
                        str_frontdev_id_, m_ulIOHandlerIndex));
            return -1;
        }

        m_ulIOHandlerIndex = (uint32_t)IO_INVALID_HANDLE;
    }

    SVS_LOG((SVS_LM_DEBUG, "device[%s] stop record success.",
                str_frontdev_id_));
    return 0;
}

/*****************************************************************************
 �� �� ��  : off_line
 ��������  : �豸���ߴ���
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::off_line()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_DEBUG,"Camera off line.CameraId[%s].",str_frontdev_id_));
    // �����ص� �¼���ʧ
    (void)clear_event_record_info();

    int32_t stopResult = stop_device();
    return stopResult;
}

/*****************************************************************************
 �� �� ��  : stop_device
 ��������  : ֹͣ�豸¼��
 �������  : bool bSendOverMsg: �Ƿ�����ȷ��ͽ���·����Ϣ, Ĭ��Ϊ����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::stop_device( bool bSendOverMsg )
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    SVS_LOG((SVS_LM_INFO, "Start to stop recording.CameraId[%s], state[0x%04x].",
        str_frontdev_id_, state_));

    // �豸�豸Ϊ����״̬
    (void)set_status(RECORD_STATE_SLEEP);

    // ����Ѿ�û���¼�¼���ˣ����豸�豸Ϊֹͣ״̬
    if ((0 >= total_event_) && (0 == event_over_time_))
    {
        (void)set_status(FRONT_DEVICE_STATE_STOP);
    }

    // ��Ϊ���ߣ�û��ý�������ݱ����͹�������Ҫ�����˳���Ϣ����ý������
    // ���̺߳��ļ�����߳��˳���
    (void)send_stop_recv_msg();

    // �����Ҫ����ֹͣ¼����Ϣ
    if( bSendOverMsg )
    {
        (void)shutdown_router();
    }
    else
    {
        // �ֶ�¼��״̬֪ͨ
        if (RECORDING_ON_MANUAL == record_flag_)
        {
            (void)send_status_notify_msg(STORESVR_MANAUL_RECORD_STATE_STOP,
                                        (uint8_t)(record_flag_ + 1));
        }
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : request_router
 ��������  : �������ϵͳ����·�ɣ�ͨ��StoreSvr���͸�·��������Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::request_router()
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to request route for realtime recording, cameraId[%s].",
        str_frontdev_id_));
    (void)set_status(RECORD_STATE_REQUEST_ROUTER);
    request_route_overtime_len_ = get_rand_request_router_time();

    // �洢��ϵͳ·��������Ϣ
    uint16_t respMsgLen = sizeof(SVS_STORESERVER_ROUTER_REQ_MSG);

    ACE_Message_Block *respMsgMb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, respMsgLen );

    if ( NULL == respMsgMb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to create requesting route message for the camera, "
            "Allcate message block failed, "
            "cameraId[%s], mb type[%d], size[%d].",
            str_frontdev_id_,
            CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
            respMsgLen ));

        return SVS_RESULT_FAILURE;
    }

    SVS_STORESERVER_ROUTER_REQ_MSG* pRespMsg =
        (SVS_STORESERVER_ROUTER_REQ_MSG*)(void*)respMsgMb->wr_ptr();
    respMsgMb->wr_ptr( respMsgLen );

    FillCommonHeader(   &pRespMsg->Header,
                        SVS_MSG_TYPE_STORESERVER_ROUTER_REQ,
                        CAC_RT_Record_Server::instance()->transaction_no(),
                        respMsgLen );

    SetNLSDeviceId((uint8_t *)&pRespMsg->FrontDevId,
        sizeof(pRespMsg->FrontDevId),
        str_frontdev_id_);

    pRespMsg->StreamType = STREAM_TYPE_TIMINGRECORD;

    SetNLSDeviceId((uint8_t *)pRespMsg->ServerId,
        sizeof(pRespMsg->ServerId),
        CAC_RT_Record_Server::instance()->str_id());

    pRespMsg->ByVtdu = 1;
    (void)ACE_OS::memset(pRespMsg->SessionId, 0, SESSIONID_LEN);

    request_route_times_++;

    //����·����Ϣ
    int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( respMsgMb );

    if ( 0 != sendResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Add requesting route message to queue failed, "
            "CameraId[%s], msg type[0x%04x], return code[%d].",
            str_frontdev_id_,
            pRespMsg->Header.MsgType,
            sendResult ));

        g_p_msg_buffer->free_mb( respMsgMb );

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Add requesting route message to queue succeeded, CameraId[%s].",
        str_frontdev_id_));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : shutdown_router
 ��������  : ���Ƚ�������ͨ��StoreSvr���͸�·�ɽ�����Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::shutdown_router(bool bGetlastError/*= false*/)
{
    SVS_TRACE();
    if ( 0 == stream_id_ )
    {
        SVS_LOG( (SVS_LM_WARNING,
            "Shutdown route for camera failed.Route id is 0.CameraId[%s].",
            str_frontdev_id_ ) );
        return SVS_RESULT_FAILURE;
    }

    // ��Ϊ��ʱ¼���ʶ�Ѿ����ı䣬���ж�¼�������Ƿ�Ϊ�ֶ�¼��ͷ���ֹͣ�ֶ�¼����Ϣ
    (void)send_status_notify_msg(  STORESVR_MANAUL_RECORD_STATE_STOP,
                                    (uint8_t)(record_flag_ + 1));

    // ���ɽ���·����Ϣ
    uint16_t respMsgLen = sizeof(SVS_STORESERVER_SCHEDULE_OVER_REQ_MSG);

    if(true == bGetlastError)
    {
        respMsgLen += SVS_TLV_LEN_BIT_STRING_4+TLV_TYPE_LEN+TLV_LEN_LEN;
    }

    ACE_Message_Block *respMsgMb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, respMsgLen );
    if ( NULL == respMsgMb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Shutdown route for camera failed.Failed to allocate message block."
            "cameraId[%s],mb type[%d] size[%d]",
            str_frontdev_id_,
            CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
            respMsgLen ));

        return SVS_RESULT_FAILURE;
    }

    SVS_STORESERVER_SCHEDULE_OVER_REQ_MSG* pRespMsg =
        (SVS_STORESERVER_SCHEDULE_OVER_REQ_MSG*)(void*)respMsgMb->wr_ptr();
    respMsgMb->wr_ptr( respMsgLen );

    (void)ACE_OS::memset(pRespMsg, 0, respMsgLen);

    FillCommonHeader(   &pRespMsg->Header,
                        SVS_MSG_TYPE_STORESERVER_SCHEDULE_OVER_REQ,
                        CAC_RT_Record_Server::instance()->transaction_no(),
                        respMsgLen );

    pRespMsg->StreamId   = stream_id_;
    pRespMsg->StreamType = STREAM_TYPE_TIMINGRECORD;

    if(true == bGetlastError)
    {
        uint32_t ulLastError = SVS_Media_Processor::instance().GetHandleLastError(m_ulIOHandlerIndex);

        char* pTLVData = ((char*)(void*)respMsgMb->wr_ptr()) + sizeof(SVS_STORESERVER_SCHEDULE_OVER_REQ_MSG);

        *((uint16_t*)(void*)pTLVData) = htons(SVS_TLV_TYPE_ROUTE_REASON);
        pTLVData += sizeof(uint16_t);
        *((uint16_t*)(void*)pTLVData) = htons(SVS_TLV_LEN_BIT_STRING_4);
        pTLVData += sizeof(uint16_t);
        *((uint32_t*)(void*)pTLVData) = htonl(ulLastError);
    }

    //������Ϣ
    int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( respMsgMb );
    if ( 0 != sendResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Add stopping route message to queue failed."
            "cameraId[%s] ,msg type[0x%04x] return code[%d].",
            str_frontdev_id_,
            pRespMsg->Header.MsgType,
            sendResult ));
        g_p_msg_buffer->free_mb( respMsgMb );
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Add stopping route message to queue succeeded. "
        "cameraId[%s],route id[%u].",
        str_frontdev_id_,
        stream_id_));

    //�����ID
    stream_id_ = 0;

    return SVS_RESULT_OK;

}

/*****************************************************************************
 �� �� ��  : notify_vtdu
 ��������  : ��VTDU����ע����Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::notify_vtdu()
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    //��media processor����io handler
    int32_t nRet = SVS_Media_Processor::instance().alloc_handle(str_frontdev_id_,
                                                            LENS_TYPE_REALTIME,
                                                            0,
                                                            m_ulIOHandlerIndex);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
                   "Connecte with VTDU failed. alloc io handler failed."
                   "cameraId[%s].",
                   str_frontdev_id_));

        (void)set_status(RECORD_STATE_REQUEST_ROUTER);

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "After notify vtdu, the device realtime record's info, "
        "record_flag[%d], event_begin_time[%d].",
        record_flag_, event_begin_time_));

    // ���Ϊ���¼�¼������¼�¼��Ŀ�ʼʱ�䲻Ϊ0����Ҫ֪ͨIO��ʱ��ʼ��¼����
    // ���Ϊ�¼�¼�����¼�¼��ʼʱ��δ���ã������¼�����ʱ�ſ�ʼ��¼����
    if ((RECORD_TYPE_EVENT != record_flag_)
        || (0 < event_begin_time_))
    {
        record_start_time_ = event_begin_time_ > 0 ? event_begin_time_ : ACE_OS::time(NULL);
        nRet = SVS_Media_Processor::instance().add_record_index(m_ulIOHandlerIndex,
                                                               record_start_time_,
                                                               0);

        if (SVS_RESULT_OK != nRet)
        {
            SVS_LOG((SVS_LM_WARNING,
                   "cameraId[%s] set index start time failed.",
                   str_frontdev_id_));

            return SVS_RESULT_FAILURE;
        }
    }

    // ��ʼ¼��
    ACE_INET_Addr addr((uint16_t)0);
    nRet = SVS_Media_Processor::instance().start_record(m_ulIOHandlerIndex,
                                                        stream_id_,
                                                        addr,
                                                        vtdu_addr_);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
                   "Connecte with VTDU failed. start recv failed."
                   "cameraId[%s].",
                   str_frontdev_id_));
        return SVS_RESULT_FAILURE;
    }

    // ������ʱ����ʱ������״̬
    (void)set_status(RECORD_STATE_CONNECT_VTDU);
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : handle_routerinfo
 ��������  : �������������·����Ϣ
 �������  : const void *pbyMsg:·����Ӧ��Ϣ����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::handle_routerinfo(const void *pbyMsg)
{
    SVS_TRACE();

    if (NULL == pbyMsg)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Process route failed, The parameter pbyMsg is NULL, cameraId[%s].",
            str_frontdev_id_));

        return SVS_RESULT_FAILURE;
    }

    const SVS_STORESERVER_ROUTER_INFO_REQ_MSG *pRouterInfo =
        (SVS_STORESERVER_ROUTER_INFO_REQ_MSG *)pbyMsg;

    //�Ѿ����·��,ֱ�ӷ���
    if ((RECORD_STATE_REQUEST_ROUTER != state_)
        && (RECORD_STATE_REQUEST_ROUTER < state_))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Process route for camera failed, The camera already has route"
            "cameraId[%s],routeId[%d].",
            str_frontdev_id_,
            stream_id_));

        //�Ѿ����·�ɣ���ID������Ϊ 0
        if (0 == stream_id_)
        {
            SVS_LOG((SVS_LM_ERROR,
                "The camera already has route, but route id is 0, cameraId[%s].",
                str_frontdev_id_));
        }

        return SVS_RESULT_FAILURE;
    }

    // �������·��ʧ��
    if (0 != pRouterInfo->RespCode)
    {
        (void)set_status(RECORD_STATE_REQUEST_ROUTER);
        return SVS_RESULT_FAILURE;
    }

    (void)set_status(RECORD_STATE_CONNECT_VTDU);

    stream_id_ = pRouterInfo->StreamId;

    (void)vtdu_addr_.set(pRouterInfo->VTDUPORT, pRouterInfo->VTDUIP, 0);

    SVS_LOG((SVS_LM_DEBUG,
        "Parse route message finished, cameraId[%s], route id[%u], VTDU address[%s:%u].",
        str_frontdev_id_,
        pRouterInfo->StreamId,
        vtdu_addr_.get_host_addr(),
        vtdu_addr_.get_port_number()));

    //��VTDUע��
    if ( SVS_RESULT_OK != notify_vtdu() )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Process route failed.Occur error while CameraId connecte with VTDU.cameraId[%s],VTDU address[%s:%u].",
            str_frontdev_id_,
            vtdu_addr_.get_host_addr(),
            vtdu_addr_.get_port_number()));

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Process route succeeded."
        "cameraId[%s], route id[%u] ,VTDU address[%s:%u].",
        str_frontdev_id_,
        pRouterInfo->StreamId,
        vtdu_addr_.get_host_addr(),
        vtdu_addr_.get_port_number()));

    return SVS_RESULT_OK;
}

int32_t CAC_Realtime_Record_Device::set_status(uint8_t newStatus)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    SVS_LOG((SVS_LM_DEBUG,
        "The camera begin to set status, "
        "cameraId[%s], old status start time[%d], state[0x%04x=>0x%04x].",
        str_frontdev_id_,
        status_start_time_,
        state_,
        newStatus ));

    state_ = newStatus;
    status_start_time_ = time(NULL);
    // ���������·��״̬������������·��ʱ��
    if(RECORD_STATE_REQUEST_ROUTER == newStatus)
    {
        request_route_overtime_len_= REQUEST_ROUTER_DELAY_TIME;
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : on_timer
 ��������  : ��ʱ���ص�����
 �������  : int32_t nDummy:��ʱ���¼�����
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::on_timer(int32_t, int32_t, int32_t nDummy)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    // �¼�¼���������
    if ( TIMER_TYPE_EVENT_RECORD_OVER == nDummy )
    {
        SVS_LOG((SVS_LM_DEBUG, "Camera[%s] on timer, dummy[%d] stat[%d].",
           str_frontdev_id_, nDummy, state_));
        proc_event_record_over_time();
        return SVS_RESULT_OK;
    }
    else if ( TIMER_TYPE_EVENT_RECORD_LOST == nDummy ) // �¼���ʧ��ʱ��
    {
        SVS_LOG((SVS_LM_DEBUG, "Camera[%s] on timer, dummy[%d] stat[%d].",
           str_frontdev_id_, nDummy, state_));
         return proc_event_record_lost();
    }

    // ��ͷ״̬��ʱ����ʱ����
    switch (state_)
    {
        case RECORD_STATE_RECEIVING_FILE:   //���ڽ���ý����
            {
                // �����������ʱ
                recv_stream_on_timer();
            }
            break;

        case RECORD_STATE_SLEEP:          // �ȴ�״̬
            break;

        case RECORD_STATE_DISK_FULL:          // �ȴ�״̬
            {
                // ����Ƿ��п��ô���
                time_t nowTime = ACE_OS::time(NULL);
                if ((SVS_RETRY_SLEEP_LENS_TIME <= nowTime - status_start_time_)
                    && (SVS_RESULT_OK == SVS_Disk_Manager::instance().checkUseableRaid()))
                {
                    (void)set_status(RECORD_STATE_REQUEST_ROUTER);
                }
            }
            break;

        case RECORD_STATE_REQUEST_ROUTER:   //��������·��
            {
                request_router_on_timer();  //�ٴ�����
            }
            break;

        case RECORD_STATE_CONNECT_VTDU:     //��VTDU����ý��ע����Ϣ
            {
               conn_vtdu_on_timer();
            }
            break;

        case RECORD_STATE_WAITING_OVER:    // ����ǵȴ��ײ�رս���״̬
            {
                waiting_over_on_timer();
            }
            break;
        default:
            SVS_LOG((SVS_LM_WARNING,
                "The camera process the callback of the timer failed."
                "The status is invalid.cameraId[%s],status[%d].",
                str_frontdev_id_,
                state_ ));
            break;
    }

    // ����¼��״̬
    device_stat_.updateRecordInfo(record_flag_, state_, stream_id_);
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : recv_stream_on_timer
 ��������  : ��ʱ���ص���¼��״̬�ǽ�����״̬���жϽ�����д¼���Ƿ�ʱ
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CAC_Realtime_Record_Device::recv_stream_on_timer()
{
    SVS_TRACE();
    // �������д���Ƿ�����
    //��������ʱ��д¼���ļ���ʱ,��������·��
    int32_t nRet = SVS_Media_Processor::instance().get_handle_status(m_ulIOHandlerIndex);
    if (SVS_RESULT_OK == nRet)
    {
        // ״̬����
        return;
    }

    ++recv_media_data_timeout_times_;

    // ¼��״̬�쳣����������·��
    SVS_LOG((SVS_LM_ERROR,
        "camera[%s] check record status invalid, try request route again.",
        str_frontdev_id_));
    (void)shutdown_router(true);
    (void)send_stop_recv_msg();

    if (SVS_ERR_NORAID == nRet)
    {
        // �޴��̿��ã�״̬תΪwaiting
        (void)set_status(RECORD_STATE_DISK_FULL);
    }
    else
    {
        (void)set_status(RECORD_STATE_REQUEST_ROUTER);
    }

    return;
}

void CAC_Realtime_Record_Device::conn_vtdu_on_timer()
{
    // ������Ϊ-1����û�г�ʱ�����ò�ѯ״̬
    time_t currTime = time(NULL);
    if(((uint32_t)IO_INVALID_HANDLE == m_ulIOHandlerIndex)
    && (VTDU_SEND_DATA_MIN_TIMEOUT > (currTime - status_start_time_)))
    {
        return;
    }

    // �ж�����VTDU�Ƿ�ʱ
    // ����Ѿ����ӳɹ��ˣ����޸�¼��״̬
    int32_t result = SVS_Media_Processor::instance().get_handle_status(m_ulIOHandlerIndex);
    if (SVS_RESULT_OK == result)
    {
        (void)set_status(RECORD_STATE_RECEIVING_FILE);

        // ������ֶ�¼�������ֶ�¼��֪ͨ��Ϣ
        if (RECORDING_ON_MANUAL == record_flag_)
        {
            (void)send_status_notify_msg(STORESVR_MANAUL_RECORD_STATE_RUNNING,
                                  (uint8_t)( record_flag_ + 1));
        }

        device_stat_.setRecordStartTime(time(NULL));
        return ;
    }
    else if(SVS_ERR_WAIT_RESP == result ) // ������ڵȴ�VTDU��Ӧ������û�г�ʱ
    {
        return ;
    }

    // ����VTDU��ʱ����������·��
    // ������ն���Ϊ�ջ��߽��ն���״̬�쳣������Ҫ��������·��
    SVS_LOG((SVS_LM_ERROR, "Connect with VTDU timeout.cameraId[%s].",
        str_frontdev_id_));

    (void)shutdown_router(true);
    (void)send_stop_recv_msg();

    connect_vtdu_timeout_times_++;

    (void)set_status(RECORD_STATE_REQUEST_ROUTER);
}

void CAC_Realtime_Record_Device::request_router_on_timer()
{
    // �жϸľ�ͷ�ĵײ�handle�Ƿ��Ѿ��ͷţ������û���ͷţ���ȴ�
    uint32_t handleIndex = (uint32_t)IO_INVALID_HANDLE;
    int32_t nRet = SVS_Media_Processor::instance().get_handle_close_status(str_frontdev_id_,
                                                                    LENS_TYPE_REALTIME,
                                                                    0, handleIndex);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera's Hanlde is closing, so wait it. cameraId[%s],handle index[%u].",
            str_frontdev_id_,
            handleIndex));

        (void)set_status(RECORD_STATE_WAITING_OVER);
        return ;
    }

    time_t currTime = time(NULL);
    // ���ʱ���Ѿ���ʱ������������·��,���û�г�ʱ���������
    if ((currTime - status_start_time_) < request_route_overtime_len_)
    {
        return ;
    }

    SVS_LOG((SVS_LM_WARNING,
        "Request router timeout,Request router again.cameraId[%s]",
        str_frontdev_id_));

    // ����·��ǰ���ȳ��Թر�·��
    if(0 < stream_id_)
    {
        (void)shutdown_router();
    }

    // ����·��
    if (SVS_RESULT_OK != request_router())
    {
        SVS_LOG((SVS_LM_ERROR, "Request router failed."));
        return ;
    }

    return ;
}



void CAC_Realtime_Record_Device::waiting_over_on_timer()
{
    // �����ѯ̫Ƶ������������5s�Ժ��ٲ�ѯһ��
    time_t currTime = time(NULL);
    // ���ʱ���Ѿ���ʱ������������·��,���û�г�ʱ���������
    if ((currTime - status_start_time_) < REQUEST_ROUTER_DELAY_TIME)
    {
        return ;
    }

    // �жϸľ�ͷ�ĵײ�handle�Ƿ��Ѿ��ͷţ������û���ͷţ���ȴ�
    uint32_t handleIndex = (uint32_t)IO_INVALID_HANDLE;
    int32_t nRet = SVS_Media_Processor::instance().get_handle_close_status(str_frontdev_id_,
                                                                    LENS_TYPE_REALTIME,
                                                                    0, handleIndex);
    // ���handle�Ѿ��ͷţ�����������·��
    if(SVS_RESULT_OK == nRet)
    {
        // ����·��ǰ���ȳ��Թر�·��
        if(0 < stream_id_)
        {
            (void)shutdown_router();
        }

        // ����·��
        (void)request_router();
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera's Hanlde is closing, so wait it. cameraId[%s], handle index[%u].",
            str_frontdev_id_,
            handleIndex));

        status_start_time_ = currTime;
    }

    return ;
}

/*****************************************************************************
 �� �� ��  : proc_event_record_over_time
 ��������  : �¼�¼�񳬳���������ʱ��
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CAC_Realtime_Record_Device::proc_event_record_over_time()
{
    SVS_TRACE();
    device_stat_.updateEventInfo(event_begin_time_, event_over_time_, (uint32_t)total_event_);

    //�����¼�����ֹͣ¼��
    if ( 0 < total_event_ )
    {
        return;
    }

    //��������¼�¼��ֹͣ����¼������
    if ( RECORD_TYPE_EVENT == record_flag_ )
    {
        //����¼���ļ������б����ʱ��
        if (0 != event_begin_time_)
        {
            time_t stopTime = ACE_OS::time(NULL);
            (void)SVS_Media_Processor::instance().add_record_index(m_ulIOHandlerIndex ,
                                                                      record_start_time_,
                                                                      stopTime);
            SVS_LOG((SVS_LM_INFO, "camera[%s] stop update database index time[%d:%d].",
                       str_frontdev_id_, event_begin_time_, stopTime));

            event_begin_time_ = 0;
            event_over_time_ = 0;
            record_start_time_ = 0;
        }

        //����ҪԤ¼��ֹͣ¼��
        if ( 0 == event_record_before_seconds_)
        {
            (void)stop_device();
        }
    }
    // ����ǽ�ֹ¼����ԣ���ֹͣ¼��
    else if (RECORD_TYPE_PROBID == record_flag_)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stop recording.Alarm event over timer ,total alarm event number is 0 and "
            "record type is [RECORD_TYPE_PROBID], so stop record.cameraId[%s].",
            str_frontdev_id_));
        (void)clear_event_record_info();
        (void)stop_device();
    }
    event_begin_time_ = 0;
    event_over_time_ = 0;

    return ;
}

/*****************************************************************************
 �� �� ��  : proc_event_record_lost
 ��������  : ¼���¼���ʧ��ʱ��
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::proc_event_record_lost()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    int32_t totalEvent = total_event_;
    // �������и澯�¼�,����Чʱ����һ������
    vector <EVENTMAP >::size_type eventVecSize = event_vector_.size();
    EVENTMAP::iterator pIter;
    for ( size_t i = 0; i < eventVecSize; i++ )
    {
        for (pIter = event_vector_[i].begin( ) ; pIter != event_vector_[i].end( ) ; pIter++)
        {
            pIter->second -= SVS_EVENT_RECORD_LOST_INTERVAL;
            if (pIter->second <= 0)
            {
                event_vector_[i].erase(pIter);
                total_event_--;
                device_stat_.updateEventInfo(event_begin_time_, event_over_time_, (uint32_t)total_event_);
            }
        }
    }

    // ��������¼�¼����������¼��
    for (size_t j = 0; j < eventVecSize; j++ )
    {
        if (0 != event_vector_[j].size())
        {
            return SVS_RESULT_OK;
        }
    }
    // ������һ���¼�����
    if (0 < totalEvent)
    {
        SVS_LOG((SVS_LM_DEBUG,
            "Alarm event lost timer take effect."
            "cameraId[%s],alarm event total number[%d].",
            str_frontdev_id_,
            total_event_));
        //��������¼�¼�������һ���¼�¼���ڣ���ֹͣ����¼������
        if (RECORD_TYPE_EVENT == record_flag_)
        {
            //����¼���ļ������б����ʱ��
            if (0 != event_begin_time_)
            {
                time_t stopTime = ACE_OS::time(NULL);
                (void)SVS_Media_Processor::instance().add_record_index(m_ulIOHandlerIndex,
                                                                      record_start_time_,
                                                                      stopTime);

                SVS_LOG((SVS_LM_INFO,
                    "camera[%s] stop update database index.event begin time[%d],index time[%d:%d].",
                    str_frontdev_id_, event_begin_time_, record_start_time_, stopTime));
                record_start_time_ = 0;
                event_begin_time_ = 0;
            }

            //����ҪԤ¼��ֹͣ¼��
            if ( 0 == event_record_before_seconds_)
            {

                SVS_LOG((SVS_LM_DEBUG,
                    "Stop recording.Alarm event lost,alarm event total number is 0 "
                    "and need not prerecord,so stop record.cameraId[%s].",
                    str_frontdev_id_));
                (void)clear_event_record_info();
                (void)stop_device();
            }
        }
        // ����ǽ�ֹ¼����ԣ���ֹͣ¼��
        else if (RECORD_TYPE_PROBID == record_flag_)
        {
            SVS_LOG((SVS_LM_ERROR,
                "Stop recording.Alarm event lost ,total alarm event number is 0"
                " and record type is [RECORD_TYPE_PROBID], so stop record. cameraId[%s].",
                str_frontdev_id_));
            (void)clear_event_record_info();
            (void)stop_device();
        }
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : probid_recording
 ��������  : ������ֹ¼��
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::probid_recording()
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_WARNING,
        "The camera begin to forbid recording, cameraId[%s].",
        str_frontdev_id_));

    (void)set_status(RECORD_STATE_SLEEP);

    (void)stop_device();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    record_flag_        = RECORD_TYPE_PROBID;

    return SVS_RESULT_OK;
}


/*****************************************************************************
 �� �� ��  : clear_event_record_info
 ��������  : ��������¼�¼����Ϣ
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::clear_event_record_info()
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    SVS_LOG((SVS_LM_DEBUG,
        "The camera begin to clear alarm event, "
        "cameraId[%s], alarm event total number [%d].",
        str_frontdev_id_,
        total_event_));

    event_begin_time_ = 0;
    event_over_time_ = 0;
    for (size_t i = 0; i < event_vector_.size(); i++)
    {
        event_vector_[i].clear();
    }

    total_event_ = 0;
    device_stat_.updateEventInfo(event_begin_time_, event_over_time_, (uint32_t)total_event_);

    SVS_LOG((SVS_LM_INFO,
        "the camera clear alarm event succeeded, cameraId[%s].",
        str_frontdev_id_));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : start_record
 ��������  : �����豸��ǰ��¼��״̬��������ֹͣ¼��
 �������  : recordType :   ����¼���¼������
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::start_record(uint8_t recordType)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
        "The camera start recording, cameraId[%s], "
        "new record type flag[%d].",
        str_frontdev_id_,
        recordType));

    //¼�����Ͳ���ȷ��ֱ�ӷ���
    if (RECORD_TYPE_MAX <= recordType)
    {
        SVS_LOG((SVS_LM_ERROR,
            "The camera start recording failed, "
            "The record type is invalid, "
            "cameraId[%s], recordtype[%d].",
            str_frontdev_id_,
            recordType ));

        return SVS_RESULT_FAILURE;
    }

    bool bStartRecord = false;

    //¼������ת��
    if(SVS_RESULT_OK != record_conversion(recordType, bStartRecord))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Conversion record type failed, "
            "camera[%s], recordType[%d].",
            str_frontdev_id_,
            recordType));

        return SVS_RESULT_FAILURE;
    }

    //����µ�¼�������ǽ�ֹ¼��
    if(RECORD_TYPE_PROBID == record_flag_)
    {
        return SVS_RESULT_OK;
    }

    //������ֶ�¼�񣬷��Ϳ�ʼ�ֶ�¼��֪ͨ
    if ((RECORD_TYPE_MANUAL == record_flag_)&&(0 != router_id()))
    {
        (void)send_status_notify_msg(STORESVR_MANAUL_RECORD_STATE_RUNNING,
                                    (uint8_t)(recordType + 1));
    }

    // �����¼�¼�������ʱ��
    time_t eventRecordTime = event_over_time_ - time(NULL);
    if((RECORD_TYPE_EVENT == record_flag_) && (0 < eventRecordTime))
    {
        (void)set_event_record_over_timer(ACE_Time_Value(eventRecordTime));
    }

    //�粻�Ǵӽ�ֹ¼������¼��ֱ�ӷ���
    if ( !bStartRecord )
    {
        return 0;
    }

    //����·��������Ϣ
    (void)init();

    int32_t reqResult = request_router();

    return reqResult;
}

/*****************************************************************************
 �� �� ��  : control_record
 ��������  : ���ƾ�ͷ�������ָ��¼��������ͽ���¼��
 �������  : recordFlag :   ����¼��Ĳ�������
             endTime    :   ����ʱ��
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::control_record( uint8_t recordFlag,
                                                   time_t /*endTime*/,
                                                   time_t eventStratEndTime )
{
    SVS_TRACE();

    //���豸û���κ�¼�������Чʱ,��¼��״̬���
    if (0 >= eventStratEndTime)
    {
        SVS_LOG((SVS_LM_DEBUG,
            "The camera does't have effective event strategy, "
            "Clear all alarm event, "
            "cameraId[%s], "
            "alarm event total number[%d], "
            "event strategy end time[%d].",
            str_frontdev_id_,
            total_event_,
            eventStratEndTime));

        (void)clear_event_record_info();
    }
    uint8_t oldRecordFlag = record_flag_;
    int32_t oldEventNum = total_event_;
    int32_t result = SVS_RESULT_FAILURE;
    switch ( recordFlag )
    {
        case FROBID_RECORDING:      //��ֹ¼��
            result = probid_recording();
            break;

        case RECORDING_ON_EVENT:    //�¼�¼��
            result = start_record(RECORD_TYPE_EVENT);
            break;

        case ALWAYS_RECORDING:      //����¼��
            result = start_record(RECORD_TYPE_ALWAYS);
            break;

        case RECORDING_ON_MANUAL:   //�ֶ�¼��
            result = start_record(RECORD_TYPE_MANUAL);
            break;
        default :
            //û��¼�����ʱ�����Ϊ��ֹ¼��
            result = probid_recording();
            break;
    };

    updateRecordTaskStartTime(oldRecordFlag,record_flag_, oldEventNum);


    return result;
}

/*****************************************************************************
 �� �� ��  : event_happen
 ��������  : ¼��澯�¼���������
 �������  : const char *strAlarmDeviceId:�澯�����豸
             uint32_t eventType    :   �¼�����
             time_t endTime :�¼�Ԥ�ƽ���ʱ��
             bool bIsRecord:��ʶ���¼�����ʱ���Ƿ�������ʼ¼��
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::event_happen(   const char *strAlarmDeviceId,
                                                uint32_t eventType,
                                                time_t endTime,
                                                bool bIsRecord)
{
    SVS_TRACE();
    //���¼������Ƿ���ȷ,����ȷֱ�ӷ���
    if ( RECORD_EVENT_TYPE_MAX <= eventType )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Process alarm event failed, Alarm event type is invalid, "
            "cameraId[%s], alarm event type[%u].",
            str_frontdev_id_,
            eventType));

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Begin to process alarm event, "
        "cameraId[%s], alarm event type[ %s], alarm camera id[%s].",
        str_frontdev_id_,
        g_strEventName[eventType],
        strAlarmDeviceId));

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    // ������豸�Ѳ����˸����¼����¼�¼��,��ֱ�ӷ���
    if ( 0 < event_vector_[eventType][strAlarmDeviceId] )
    {
        SVS_LOG((SVS_LM_WARNING,
            "Occur error while process alarm event, "
            "This type of alarm event already exist and the time int32_t is [%d], "
            "CameraId[%s] ,event type[%u] ,alarm camera id[%s].",
            event_vector_[eventType][strAlarmDeviceId],
            str_frontdev_id_,
            eventType,
            strAlarmDeviceId));

        //�����¼�¼�������Чʱ��
        event_vector_[eventType][strAlarmDeviceId] =
            (int32_t)CAC_RT_Record_Server::instance()->event_record_lost_timeout();

        SVS_LOG((SVS_LM_INFO,
            "Update recording time length of this alarm event type to [%d]s, cameraId[%s].",
            event_vector_[eventType][strAlarmDeviceId],
            str_frontdev_id_));

        return SVS_RESULT_FAILURE;
    }

    //�����¼����͵ĸø澯�豸�ñ�־
    event_vector_[eventType][strAlarmDeviceId] =
        (int32_t)CAC_RT_Record_Server::instance()->event_record_lost_timeout();

    SVS_LOG((SVS_LM_INFO,
        "Recording time length of this alarm event type is [%d]s, cameraId[%s].",
        event_vector_[eventType][strAlarmDeviceId],
        str_frontdev_id_));

    //�¼�������1
    total_event_++;

    // �����û�п�ʼ¼�񣬲�����Ҫ¼����ʼ�¼�¼��
    if (RECORD_TYPE_PROBID == record_flag_ && bIsRecord)
    {
        SVS_LOG((SVS_LM_INFO,
            "Start alarm event recording, cameraId[%s].",
            str_frontdev_id_));

        //�����¼���ʼʱ��
        event_begin_time_ = time(NULL);

        //ȡ����ǰ���õ�ֹͣ�¼�¼��ʱ��
        (void)kill_event_record_over_timer();

        (void)control_record( RECORDING_ON_EVENT, endTime ,1);
        total_event_ = 1;

        device_stat_.setRecordTaskStartTime(time(NULL));
    }
    else
    {
        //�Ѿ���ʼ¼�񣬵��¼���δ��ʼʱ����Ҫ֪ͨEgress��ʼ��¼����
        if (0 == event_begin_time_)
        {
            event_begin_time_ = time(NULL) - event_record_before_seconds_;

            //ȡ����ǰ���õ�ֹͣ�¼�¼��ʱ��
            (void)kill_event_record_over_timer();

            // ����Ѿ��ڼ�¼������������֪ͨEgress��ʼ��¼����
            if(0 == record_start_time_)
            {
                // ֪ͨEgress��ʼ��¼����
                record_start_time_ = event_begin_time_;
                int32_t nRet = SVS_Media_Processor::instance().add_record_index(m_ulIOHandlerIndex,
                                                                           record_start_time_,
                                                                           0);
                if (SVS_RESULT_OK != nRet)
                {
                    SVS_LOG((SVS_LM_WARNING,
                        "Start alarm event recording, set index start time fail, "
                        "cameraId[%s], alarm event type [%s], io handle[%u].",
                        str_frontdev_id_,
                        g_strEventName[eventType],
                        m_ulIOHandlerIndex));
                }
                else
                {
                    SVS_LOG((SVS_LM_INFO,
                        "Start alarm event recording, cameraId[%s],alarm event type [%s].",
                        str_frontdev_id_,
                        g_strEventName[eventType]));
                }
            }

        }
    }

    event_over_time_ = 0;
    device_stat_.updateEventInfo(event_begin_time_, event_over_time_, (uint32_t)total_event_);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : event_over
 ��������  : �澯�����¼�����
 �������  : const char *strAlarmDeviceId:�澯�����豸
             uint32_t eventType:�澯�¼�����
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::event_over( const char *strAlarmDeviceId,
                                            uint32_t eventType)
{
    SVS_TRACE();
    //���¼������Ƿ���ȷ
    if (  RECORD_EVENT_TYPE_MAX <= eventType )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Stop alarm event recording failed."
            "Alarm event type is invalid."
            "cameraId[%s], alarm event type[%u],alarm camera id[%s].",
            str_frontdev_id_,
            eventType,
            strAlarmDeviceId));
        return SVS_RESULT_FAILURE;
    }
    SVS_LOG((SVS_LM_INFO,
        "Begin to stop alarm event recording."
        "cameraId[%s],alarm event type[%s],alarm camera id[%s].",
        str_frontdev_id_,
        g_strEventName[eventType],
        strAlarmDeviceId));

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    EVENTMAP::iterator it = event_vector_[eventType].find(strAlarmDeviceId);

    //����ָ���澯�豸��¼���¼��б���û�з���
    if (event_vector_[eventType].end() == it)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stop alarm event recording failed."
            "Alarm camera does't hava alarm event."
            "cameraId[%s], alarm camera id[%s], alarm event type[%d].",
            str_frontdev_id_,
            strAlarmDeviceId,
            eventType));
        return SVS_RESULT_FAILURE;
    }

    // �����Ӧ�ĸ澯�¼������ڣ�����
    if ((*it).second == 0)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stop alarm event recording failed."
            "Alarm camera's recording time length of this alarm event is less than 0."
            "cameraId[%s],alarm camera id[%s],alarm event type[%d],Time length[%d].",
            str_frontdev_id_,
            strAlarmDeviceId,
            eventType,
            (*it).second));
        return SVS_RESULT_FAILURE;
    }

    event_vector_[eventType].erase(it);
    // �����¼��¼��״̬�ϱ���׼����
    device_stat_.updateEventInfo(event_begin_time_, event_over_time_, (uint32_t)total_event_);

    total_event_--;

    // �������͵��¼���Ϊ0�ǲſ���ֹͣ¼��
    vector <EVENTMAP >::size_type eventVecSize = event_vector_.size();
    for ( size_t i = 0; i < eventVecSize; i++ )
    {
        if (event_vector_[i].size() > 0)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Stop alarm event recording failed."
                "This type alarm event's number is not 0."
                "cameraId[%s],alarm event type[%d],alarm event number[%u].",
                str_frontdev_id_,
                i,
                event_vector_[i].size()));

            return SVS_RESULT_FAILURE;
        }
    }

    //��ȡ�¼�����ʱ��
    time_t nowTime = time( NULL );
    event_over_time_  = nowTime + event_record_after_seconds_;

    char strStartTime[SVS_STR_TIME_MAX_LEN];
    (void)time2string(strStartTime, sizeof(strStartTime), event_begin_time_);

    char strEndTime[SVS_STR_TIME_MAX_LEN];
    (void)time2string(strEndTime, sizeof(strEndTime), nowTime);

    SVS_LOG((SVS_LM_INFO,
        "Process alarm event over succeeded."
        "cameraId[%s],alarm event happen time[%s:%d] , alarm event over time[%s:%d].",
        str_frontdev_id_,
        strStartTime,
        event_begin_time_,
        strEndTime,
        nowTime ));

    //�����¼�¼���������ʱ��
    (void)set_event_record_over_timer( ACE_Time_Value(event_record_after_seconds_) );

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : debug_get_device_status
 ��������  : ��ȡ��¼���豸��״̬����INT��ת����string�ͣ�telent������
 �������  : NA
 �������  : string &strDeviceStatus:¼���豸״̬string��ʾ
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CAC_Realtime_Record_Device::debug_get_device_status(string &strDeviceStatus)const
{
    SVS_TRACE();
    switch (state_)
    {
        case RECORD_STATE_INIT:
            strDeviceStatus = "Initializing";
            break;

        case RECORD_STATE_SLEEP:
            strDeviceStatus = "Sleeping";
            break;

        case RECORD_STATE_DISK_FULL:
            strDeviceStatus = "DiskFull,Waiting";
            break;

        case RECORD_STATE_REQUEST_ROUTER:
            strDeviceStatus = "RequestingRoute";
            break;

        case RECORD_STATE_CONNECT_VTDU:
            strDeviceStatus = "ConnectingVTDU";
            break;

        case RECORD_STATE_RECEIVING_FILE:
            strDeviceStatus = "ReceivingMediaStream";
            break;

        case RECORD_STATE_WAITING_OVER:
            strDeviceStatus = "WaitingOver";
            break;

        case FRONT_DEVICE_STATE_STOP:
            strDeviceStatus = "Stopping";
            break;

        default:
            strDeviceStatus = "UnknowStatus";
            break;
    }

    return;
}

/*****************************************************************************
 �� �� ��  : debug_get_record_flag
 ��������  : ��ȡ��¼���豸��¼�����ͣ���INT��ת����string�ͣ�telent������
 �������  : NA
 �������  : string &strRecordFlag:¼������״̬string��ʾ
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CAC_Realtime_Record_Device::debug_get_record_flag(string &strRecordFlag)const
{
    SVS_TRACE();
    switch (record_flag_)
    {
        case RECORD_TYPE_PROBID:
            strRecordFlag = "FORBID RECORD";
            break;

        case RECORD_TYPE_ALWAYS:
            strRecordFlag = "ALWAYS RECORD";
            break;

        case RECORD_TYPE_EVENT:
            strRecordFlag = "EVENT RECORD";
            break;
        case RECORD_TYPE_MANUAL:
            strRecordFlag = "MANUAL RECORD";
            break;

        default:
            strRecordFlag = "UNKOWN RECORD TYPE";
            break;
    }

    return;
}

//1 TODOʵ�־�ͷ��ϸ������Ϣ
/*****************************************************************************
 �� �� ��  : debug_get_device_info
 ��������  : ��ȡ¼���豸����ϸ��Ϣ
 �������  : uint32_t szDeviceInfo:����������
 �������  : char *strDeviceInfo:telnet����������
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::debug_get_device_info(
    char *strDeviceInfo,
    uint32_t ulBuffLen )
{
    SVS_TRACE();
    string strRecordFlag;
    string strDeviceStatus;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, obj, mutex_, false);
    //¼������
    debug_get_record_flag(strRecordFlag);

    //�豸״̬
    debug_get_device_status(strDeviceStatus);

    char strTime[SVS_STR_TIME_MAX_LEN];

    //�¼���ʼʱ��
    (void)time2string(strTime, sizeof(strTime), event_begin_time_);
    string strEventBeginTime = strTime;

    //�¼�����ʱ��
    (void)time2string(strTime, sizeof(strTime), event_over_time_);
    string strEventOverTime = strTime;

    (void)ACE_OS::snprintf( strDeviceInfo, ulBuffLen,
                      "\tCameraId               [%s]\n\t"
                      "EncoderType              [%d]\n\t"
                      "RecordFlag               [%s:0x%04x]\n\t"
                      "PrerecordTimeLen         [%d]\n\t"
                      "PostrecordTimeLen        [%d]\n\t"
                      "EventHappenTime          [%s:%d]\n\t"
                      "EventOverTime            [%s:%d]\n\t"
                      "RouteId                  [%u]\n\t"
                      "EventTotalNum            [%d]\n\t"
                      "ReferencCount            [%d]\n\t"
                      "Status                   [%s:0x%04x]\n\t"
                      "InternalId               [%d]\n\t"
                      "RequestRouteTimes        [%d]\n\t"
                      "ConnectVTDUTimes         [%d]\n\t"
                      "ConnectVTDUTimeoutTimes  [%d]\n\t"
                      "RecvStreamTimeoutTimes   [%d]\n\t"
                      "IOProcesserHandleIndex   [%u]\n\t",
                      str_frontdev_id_,
                      encoder_type_,
                      strRecordFlag.c_str(), record_flag_,
                      event_record_before_seconds_, //�¼�ǰ����
                      event_record_after_seconds_,
                      strEventBeginTime.c_str(), event_begin_time_,
                      strEventOverTime.c_str(),  event_over_time_,
                      stream_id_,
                      total_event_, //�¼�����
                      referenc_count_,
                      strDeviceStatus.c_str(), state_,
                      internal_id_,
                      request_route_times_,
                      connect_vtdu_times_,
                      connect_vtdu_timeout_times_,
                      recv_media_data_timeout_times_,
                      m_ulIOHandlerIndex);

    if((uint32_t)IO_INVALID_HANDLE != m_ulIOHandlerIndex)
    {
        uint32_t writeSize = strlen(strDeviceInfo);
        uint32_t leftBuffSize = ulBuffLen - writeSize;
        SVS_Media_Processor::instance().debug_get_handle_info(m_ulIOHandlerIndex,
                                            &strDeviceInfo[writeSize],
                                            leftBuffSize);
    }

    return 0;
}

/*****************************************************************************
 �� �� ��  : stop_record
 ��������  : ֹͣ¼��, ���豸״̬Ϊ����·��״̬(������·��֪ͨ��Ϣ)
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::stop_record()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO,
        "Camera begin to release route.cameraId[%s],reoute id[%u],current state[%d].",
        str_frontdev_id_,
        stream_id_,
        state_ ));

    //״̬��Ϊ������������VTDU״̬,ֱ�ӷ���
    if( ( RECORD_STATE_RECEIVING_FILE != state_ )
        && ( RECORD_STATE_CONNECT_VTDU != state_ ) )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Camera release route failed.Its status is invalid."
            "cameraId[%s],its status[%d],route id[%u].",
            str_frontdev_id_,
            state_,
            stream_id_));
        return SVS_RESULT_FAILURE;
    }

    // �����ֶ�¼��״̬֪ͨ��Ϣ
    if (RECORDING_ON_MANUAL == record_flag_)
    {
        (void)send_status_notify_msg(STORESVR_MANAUL_RECORD_STATE_STOP,
                                    (uint8_t)(record_flag_ + 1));
    }

    //�����豸��״̬Ϊ����·��״̬
    (void)set_status(RECORD_STATE_REQUEST_ROUTER);

    //�����˳���Ϣ����ý���������̺߳��ļ�����߳��˳���
    (void)send_stop_recv_msg();

    return 0;
}


/*****************************************************************************
 �� �� ��  : send_status_notify_msg
 ��������  : �����ֶ�¼��״̬֪ͨ��Ϣ��¼����������
 �������  : const int32_t recordStatus:¼��״̬
             uint8_t recordType:¼�����ͣ����������ӿ�
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::send_status_notify_msg(
                                                   const int32_t recordStatus,
                                                   uint8_t recordType)
{
    SVS_TRACE();
    if ((STORESVR_MANAUL_RECORD_STATE_RUNNING == recordStatus)
        && (0 == router_id()))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Send message to StoreSvr failed."
            "cameraId[%s],messagetpe[SVS_MSG_TYPE_STORESVR_RECORD_STATUS_NOTIFY_REQ],"
            "Manual Record,RouteId[0].",
            str_frontdev_id_));

        return SVS_RESULT_OK;
    }

    // ¼��״̬֪ͨ��Ϣ,ÿ��ֻ��һ��¼���豸
    uint16_t respMsgLen = sizeof(SVS_STORESVR_RECORD_STATUS_NOTIFY_REQ_MSG);

    ACE_Message_Block *respMsgMb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, respMsgLen );
    if ( NULL == respMsgMb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Send message to StoreSvr failed."
            "Failed to allocate message block."
            "cameraId[%s],messagetpe[SVS_MSG_TYPE_STORESVR_RECORD_STATUS_NOTIFY_REQ],"
            "mb type[%d] size[%d].",
            str_frontdev_id_,
            CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
            respMsgLen ));

        return -1;
    }

    SVS_STORESVR_RECORD_STATUS_NOTIFY_REQ_MSG* pRespMsg =
        (SVS_STORESVR_RECORD_STATUS_NOTIFY_REQ_MSG*)(void*)respMsgMb->wr_ptr();
    respMsgMb->wr_ptr( respMsgLen );

    FillCommonHeader(   &pRespMsg->Header,
                        SVS_MSG_TYPE_STORESVR_RECORD_STATUS_NOTIFY_REQ,
                        CAC_RT_Record_Server::instance()->transaction_no(),
                        respMsgLen);

    pRespMsg->RecordDeviceStatusCount = 1;

    //(void)ACE_OS::memcpy( pRespMsg->RecordDeviceStatusList[0].DeviceID,
    //                      str_frontdev_id_, DEVICEID_LEN);
    SetNLSDeviceId((uint8_t *)pRespMsg->RecordDeviceStatusList[0].DeviceID,
            sizeof(pRespMsg->RecordDeviceStatusList[0].DeviceID),
            str_frontdev_id_);

    pRespMsg->RecordDeviceStatusList[0].RecordID = (uint32_t)router_id();

    pRespMsg->RecordDeviceStatusList[0].RecordStatus = (uint8_t)recordStatus;
    pRespMsg->RecordDeviceStatusList[0].RecordType = (uint8_t)recordType;

    //������Ϣ
    int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( respMsgMb );
    if (SVS_RESULT_OK != sendResult)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Send message to StoreSvr failed."
            "cameraId[%s],messagetype[SVS_MSG_TYPE_STORESVR_RECORD_STATUS_NOTIFY_REQ] ,"
            "RouteId[%u],RecordStatus[%u],RecordType[%u].",
            str_frontdev_id_,
            pRespMsg->RecordDeviceStatusList[0].RecordID,
            pRespMsg->RecordDeviceStatusList[0].RecordStatus,
            pRespMsg->RecordDeviceStatusList[0].RecordType));

        g_p_msg_buffer->free_mb( respMsgMb );

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Send message to StoreSvr succeeded."
        "cameraId[%s],messagetype[SVS_MSG_TYPE_STORESVR_RECORD_STATUS_NOTIFY_REQ],"
        "RouteId[%u],RecordStatus[%u],RecordType[%u].",
        str_frontdev_id_,
        pRespMsg->RecordDeviceStatusList[0].RecordID,
        pRespMsg->RecordDeviceStatusList[0].RecordStatus,
        pRespMsg->RecordDeviceStatusList[0].RecordType));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : kill_event_record_over_timer
��������  : ȡ���¼���������ʱ��
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/

int32_t CAC_Realtime_Record_Device::kill_event_record_over_timer()
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    if ( 0 <= event_record_over_timer_id_ )
    {
        (void)CAC_Timer_Manager::instance()->free_timer(event_record_over_timer_id_);
        event_record_over_timer_id_= -1;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : set_event_over_timer
��������  : �����¼���������ʱ��
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::set_event_record_over_timer(const ACE_Time_Value &delayTime )
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_DEBUG,
        "Allocate timer for alarm event overing."
        "cameraId[%s], delay time[%d].",
        str_frontdev_id_,
        delayTime.sec() ));
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    // �ͷžɵĶ�ʱ��
    if ( 0 <= event_record_over_timer_id_ )
    {
        (void)CAC_Timer_Manager::instance()->free_timer(event_record_over_timer_id_);
        event_record_over_timer_id_= -1;
    }

    // �����ʱΪ0����ֱ�Ӵ�����ʱ������
    if(0 == delayTime.sec())
    {
        CAC_RT_Record_Device_Manager::timer_callback(CAC_RT_Record_Device_Manager::instance(),
                                                                 (int32_t)internal_id_,
                                                                 -1, // ����עtimerid������Ϊ-1
                                                                 TIMER_TYPE_EVENT_RECORD_OVER);
        SVS_LOG((SVS_LM_INFO,
            "The alarm event over delay time is 0."
            "Create timer task for alarm event overing succeeded."
            "cameraId[%s].",
            str_frontdev_id_));
    }
    else
    {
        // һ���Զ�ʱ������Ϊ0
        event_record_over_timer_id_ = CAC_Timer_Manager::instance()->alloc_timer(
                        CAC_RT_Record_Device_Manager::instance(),
                        CAC_RT_Record_Device_Manager::timer_callback,
                        (int32_t)internal_id_,
                        delayTime,
                        ACE_Time_Value::zero,
                        TIMER_TYPE_EVENT_RECORD_OVER);

        // ������붨ʱ��ʧ��
        if ( 0 > event_record_over_timer_id_ )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Allocate timer for alarm event overing failed. cameraId[%s].",
                str_frontdev_id_));

            return SVS_RESULT_FAILURE;
        }

        SVS_LOG((SVS_LM_INFO,
            "Allocate timer for alarm event overing succeeded."
            "cameraId[%s],timer ID[%u].",
            str_frontdev_id_,
            event_record_over_timer_id_ ));
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : kill_event_record_lost_timer
��������  : ȡ��¼���¼���ʧ����ʱ��
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/

int32_t CAC_Realtime_Record_Device::kill_event_record_lost_timer()
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    if ( 0 <= event_record_lost_timer_id_ )
    {
        (void)CAC_Timer_Manager::instance()->free_timer(event_record_lost_timer_id_);
        event_record_lost_timer_id_= -1;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : set_event_record_lost_timer
��������  : ����¼���¼���ʧ����ʱ��
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::set_event_record_lost_timer(
                                        const ACE_Time_Value & /*delayTime*/ )
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
        "Allocate timer for alarm event lost, "
        "cameraId[%s], TIMER_TYPE_EVENT_RECORD_LOST nDummy[%d].",
        str_frontdev_id_,
        TIMER_TYPE_EVENT_RECORD_LOST ));
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

        // �ͷžɵĶ�ʱ��
        if ( 0 <= event_record_lost_timer_id_ )
        {
            (void)CAC_Timer_Manager::instance()->free_timer(event_record_lost_timer_id_);

            event_record_lost_timer_id_= -1;
        }

        ACE_Time_Value tv(SVS_EVENT_RECORD_LOST_INTERVAL, 0);
        event_record_lost_timer_id_ = CAC_Timer_Manager::instance()->alloc_timer(
                        CAC_RT_Record_Device_Manager::instance(),
                        CAC_RT_Record_Device_Manager::timer_callback,
                        (int32_t)internal_id_,
                        tv,
                        tv,
                        TIMER_TYPE_EVENT_RECORD_LOST);

        if ( 0 > event_record_lost_timer_id_ )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Allocate timer for alarm event lost failed, cameraId[%s].",
                str_frontdev_id_));

            return SVS_RESULT_FAILURE;
        }
    }

    SVS_LOG((SVS_LM_INFO,
        "Allocate timer for alarm event lost succeeded, "
        "cameraId[%s], TIMER_TYPE_EVENT_RECORD_LOST nDummy[%d].",
        str_frontdev_id_,
        TIMER_TYPE_EVENT_RECORD_LOST ));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : refresh_event_record_outtime
��������  : �¼���ʧ�����¼��ʱ�����ö�̬����֪ͨ�ӿ�
�������  : int32_t shiftValue:���ԭ����ֵ��ƫ�������������ɸ�
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::refresh_event_record_outtime(int32_t shiftValue)
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    // �������и澯�¼�,����Чʱ��������ƫ����
    vector <EVENTMAP >::size_type eventVecSize = event_vector_.size();
    EVENTMAP::iterator pIter;
    for ( size_t i = 0; i < eventVecSize; i++ )
    {
        for (pIter = event_vector_[i].begin( ) ; pIter != event_vector_[i].end( ) ; pIter++)
        {
            pIter->second += shiftValue;
            if (pIter->second <= 0)
            {
                event_vector_[i].erase(pIter);
            }
        }
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : record_conversion
��������  : ¼������ת��
�������  : uint8_t newRecordFlag:�µ�¼�����ͱ�ʶ
�������  : bool &bStartRecord:�Ƿ���Ҫ��������¼�񣬽�ֹ¼��ת��������¼��ʱ��Ҫ
            ����¼��
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::record_conversion(uint8_t newRecordFlag,
                                                           bool &bStartRecord)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    SVS_LOG((SVS_LM_INFO,
        "Begin to transform record type, cameraId[%s], "
        "Conversion information: record type[%s=>%s], Current status[0x%04x].",
        str_frontdev_id_,
        g_strRecordType[record_flag_],
        g_strRecordType[newRecordFlag],
        state_ ));

   SVS_LOG((SVS_LM_DEBUG,
        "CameraId[%s]'s prerecordTimeLen[%d], "
        "total event number [%d], event over time[%d].",
        str_frontdev_id_,
        event_record_before_seconds_,
        total_event_ ,
        event_over_time_));

    switch( record_flag_ )
    {
        case RECORD_TYPE_PROBID:    //��ֹ¼��
        {
            //�ɽ�ֹ¼��״̬���¼�¼��״̬
            if ((RECORD_TYPE_EVENT == newRecordFlag) && (!is_event_record_effective()))
            {
                return SVS_RESULT_OK;
            }

            //֮ǰ���ڽ�ֹ¼��״̬, ����¼��
            bStartRecord        = true;

            if (RECORD_STATE_SLEEP == state_)
            {
                (void)set_status(RECORD_STATE_REQUEST_ROUTER);
            }
        }
        break;

        case RECORD_TYPE_ALWAYS:    //����¼��
        {
            //������¼��״̬���¼�¼��״̬
            if ((RECORD_TYPE_EVENT == newRecordFlag) && (!is_event_record_effective()))
            {
                return probid_recording();
            }

            always_record_over(newRecordFlag);
        }
        break;

        case RECORD_TYPE_EVENT:     //�¼�¼��
        {
            //���¼�¼��״̬���¼�¼��״̬
            if ((RECORD_TYPE_EVENT == newRecordFlag) && (!is_event_record_effective()))
            {
                return probid_recording();
            }

            // �����Ԥ¼������¼����ֶ�¼��״̬
            prerecord_over(newRecordFlag);
        }
        break;

        case RECORD_TYPE_MANUAL:    //�ֶ�¼��
        {
            return manual_record_over(newRecordFlag);
        }

        default:
        {
            SVS_LOG((SVS_LM_ERROR,
                "Transform record type failed, Record type is invalid, "
                "cameraId[%s], new record flag[%d].",
                str_frontdev_id_,
                newRecordFlag ));

            return SVS_RESULT_FAILURE;
        }
    }

    record_flag_ = newRecordFlag;

    SVS_LOG((SVS_LM_INFO,
        "The camera start recording, cameraId[%s],"
        "Request route flag[%d], record type flag[%d:%s].",
        str_frontdev_id_,
        bStartRecord ? 1 : 0,
        record_flag_,
        g_strRecordType[record_flag_] ));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : prerecord_over
��������  : �����Ԥ¼���ֶ�¼�������¼����֪ͨegress����������¼
�������  : const uint8_t newRecordFlag:�µ�¼�����ͱ�ʶ
�������  : NA
�� �� ֵ  : NA
�޸���ʷ  :
*****************************************************************************/
void CAC_Realtime_Record_Device::prerecord_over(uint8_t newRecordFlag)
{
    // �����Ԥ¼������¼����ֶ�¼��״̬,���һ�û�п�ʼ��¼����
    if ((0 != event_record_before_seconds_) && (0 == record_start_time_))
    {
        if((RECORD_TYPE_ALWAYS == newRecordFlag)
        || (RECORD_TYPE_MANUAL == newRecordFlag))
        {
            // ֪ͨEgress��ʼ��¼����
            record_start_time_ = time(NULL);
            int32_t nRet = SVS_Media_Processor::instance().add_record_index(m_ulIOHandlerIndex,
                                                                       record_start_time_,
                                                                       0);
            if (SVS_RESULT_OK != nRet)
            {
                SVS_LOG((SVS_LM_WARNING,
                    "Start recording, set index start time fail, "
                    "cameraId[%s], new record type[%s], io handle[%u].",
                    str_frontdev_id_,
                    g_strRecordType[newRecordFlag],
                    m_ulIOHandlerIndex));

                 return;
            }

            SVS_LOG((SVS_LM_INFO,
                "Start recording.cameraId[%s],new record type [%s].",
                str_frontdev_id_,
                g_strRecordType[newRecordFlag]));
        }
    }

    return;
}
/*****************************************************************************
�� �� ��  : manual_record_over
��������  : �ֶ�¼�����ʱ�Ĵ���
�������  : const uint8_t newRecordFlag:�µ�¼�����ͱ�ʶ
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::manual_record_over(const uint8_t newRecordFlag)
{
    SVS_TRACE();
    switch ( newRecordFlag )
    {
        // ���������[�ֶ�¼��]ת��[���ֶ�¼��]�������ֶ�¼��ֹͣ��Ϣ
        case RECORD_TYPE_PROBID:
        case RECORD_TYPE_ALWAYS:
            // ���������[�ֶ�¼��]ת��[���ֶ�¼��]�������ֶ�¼��ֹͣ��Ϣ
            (void)send_status_notify_msg(
                        STORESVR_MANAUL_RECORD_STATE_STOP,
                        (uint8_t)(newRecordFlag + 1));
            break;
        case RECORD_TYPE_EVENT:
            //���ֶ�¼��״̬���¼�¼��״̬�����Ԥ¼ʱ��Ϊ0��ֹͣ¼��
            if (!is_event_record_effective())
            {
                return probid_recording();
            }

            // ���������[�ֶ�¼��]ת��[���ֶ�¼��]�������ֶ�¼��ֹͣ��Ϣ
            (void)send_status_notify_msg(
                        STORESVR_MANAUL_RECORD_STATE_STOP,
                        (uint8_t)(newRecordFlag + 1));

            always_record_over(newRecordFlag);

            break;

        default:
            break;
    }
    record_flag_ = newRecordFlag;

    SVS_LOG((SVS_LM_INFO,
        "The carera start recording.cameraId[%s],"
        "Request route flag [%d] ,record type flag[%d:%s].",
        str_frontdev_id_,
        0,
        record_flag_,
        g_strRecordType[record_flag_] ));
   return SVS_RESULT_OK;
}

// ���������¼����ֶ�¼��Ԥ¼����֪ͨ����ֹͣ
void CAC_Realtime_Record_Device::always_record_over(uint8_t newRecordFlag)
{
    if((RECORD_TYPE_EVENT == newRecordFlag) && (0 != event_record_before_seconds_) && (0 == total_event_))
    {
        time_t stopTime = ACE_OS::time(NULL);
        int32_t nRet = SVS_Media_Processor::instance().add_record_index(m_ulIOHandlerIndex ,
                                                                  record_start_time_ ,
                                                                  stopTime);
        if (SVS_RESULT_OK != nRet)
        {
            SVS_LOG((SVS_LM_WARNING,
                "camera[%s] stop update database index failed.time[%d:%d].",
                str_frontdev_id_, record_start_time_, stopTime));

            return;
        }

        SVS_LOG((SVS_LM_INFO,
            "camera[%s] stop update database index successful. time[%d:%d].",
            str_frontdev_id_, record_start_time_, stopTime));
        record_start_time_ = 0;
    }

    return;
}

int32_t CAC_Realtime_Record_Device::overwrite_record_notify(time_t newIndexStartTime)
{
    int32_t nRet = SVS_RESULT_FAILURE;
    if ((uint32_t)IO_INVALID_HANDLE != m_ulIOHandlerIndex)
    {
        nRet = SVS_Media_Processor::instance().overwriteRecordNotify(m_ulIOHandlerIndex,newIndexStartTime);
    }

    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING, "camera[%s] overwrite record notify fail, "
                  "index start time[%d], handle[%u].",
                  str_frontdev_id_,
                  newIndexStartTime, m_ulIOHandlerIndex));
    }

    return nRet;
}

void CAC_Realtime_Record_Device::updateRecordTaskStartTime(
                                    uint8_t oldRecordFlag,
                                    uint8_t newRecordFlag,
                                    int32_t oldEventNum)
{
    time_t ulRecordTaskStartTime = time(NULL);

    // �����������û�б仯�����ø���
    if(oldRecordFlag == newRecordFlag)
    {
        return ;
    }

    // ����ո��ǽ�ֹ¼�����,����Ҫ����¼������ͣ������task
    if(FROBID_RECORDING == oldRecordFlag)
    {
        if((ALWAYS_RECORDING == newRecordFlag) || (RECORDING_ON_MANUAL == newRecordFlag))
        {
            device_stat_.setRecordTaskStartTime(ulRecordTaskStartTime);
        }
        // ���ת�����¼�¼�����¼���������0�������task
        else if((ALWAYS_RECORDING == newRecordFlag) && 0 < total_event_)
        {
            device_stat_.setRecordTaskStartTime(ulRecordTaskStartTime);
        }
    }
    // ����ո����¼�¼����ԣ���û��¼������������Ҫ����¼�������task
    else if((RECORDING_ON_EVENT == oldRecordFlag) && (0 >= oldEventNum))
    {
        if((ALWAYS_RECORDING == newRecordFlag) || (RECORDING_ON_MANUAL == newRecordFlag))
        {
            device_stat_.setRecordTaskStartTime(ulRecordTaskStartTime);
        }
    }

    return;
}
//========================CAC_Realtime_Record_Device END===========================

//======================CAC_RT_Record_Device_Manager BEGIN=========================

CAC_RT_Record_Device_Manager::CAC_RT_Record_Device_Manager()
{
    max_lens_ = 0;
}

CAC_RT_Record_Device_Manager::~CAC_RT_Record_Device_Manager()
{
}

/*****************************************************************************
�� �� ��  : init
��������  : ��ʼ��¼�����һ���Է��䣬�������ʱ�ͷ�
�������  : uint32_t nMaxLens:ϵͳ֧�ֵ����ͷ��
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Device_Manager::init(uint32_t nMaxLens)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to initialize the cameras manager, "
        "Maximal support camera number is [%u], "
        "the class name[CAC_RT_Record_Device_Manager].",
        nMaxLens));

    max_lens_ = nMaxLens;
    char *pch = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, false);

    uint32_t lensIndex = 0;
    for ( lensIndex = 0; lensIndex < nMaxLens; lensIndex++)
    {
        try
        {
            pch = new char[sizeof(CAC_Realtime_Record_Device)];
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_ERROR,
                "Initialize the cameras manager failed, "
                "Create the objects for camera failed, "
                "the class name[Realtime_Record_Device]."));

            break;
        }

        *((uint32_t *)pch) = lensIndex;   //��������
        (void)free_list_.insert_tail(pch);

        //ΪʲôҪ���Ѿ�ʹ�õ�����push�ն���
        using_vector_.push_back(NULL);
    }

    //���뻺��ʧ�ܣ������ǰ����Ļ�����
    if ( lensIndex != nMaxLens )
    {
        ACE_DLList_Iterator<char> iter( free_list_ );
        while( !iter.done () )
        {
            char *freeBuff = iter.next ();
            (void)iter.remove ();
            SVS_DELETE( freeBuff, SVS_DELETE_MULTI );
        }

        using_vector_.clear();
        //whileѭ�����Ѿ�����SVS_DELETE�ͷ��ˣ�����ֱ������lint�澯
        return false;    //lint !e429
    }

    SVS_LOG((SVS_LM_INFO,
        "Initialize the camera manager succeeded, "
        "Maximal support camera number is [%u].",
        nMaxLens));

    //��ʼ���ɹ�������Ŀռ��������Ӧ���˳�ʱ���ͷţ�����ֱ������lint�澯
    return true;        //lint !e429
}

/*****************************************************************************
�� �� ��  : add_device
��������  : ���¼��ͷ����.
�������  : const char * strLensId:��ͷID
            uint8_t encoderType:��ͷ��������
�������  : CAC_Realtime_Record_Device *&pDevice:��ͷ����ָ���ַ
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::add_device(
    const char * strLensId,
    uint8_t encoderType,
    CAC_Realtime_Record_Device *&pDevice )
{
    SVS_TRACE();
    pDevice = NULL;
    CAC_Realtime_Record_Device *pTmpDevice = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, SVS_RESULT_FAILURE);

    (void)device_map_.find(strLensId, pTmpDevice);

    if (NULL == pTmpDevice)
    {
        if (free_list_.is_empty())
        {
            SVS_LOG((SVS_LM_ERROR,
                "Create a record object for camera failed."
                "Free list is empty.cameraId[%s].There are [%u] cameras recording now.",
                strLensId ,
                device_map_.current_size()));

            return SVS_RESPCODE_MEM_ERROR;
        }

        char *pch = free_list_.delete_head();

        uint32_t nIndex = *((uint32_t *)(void*)pch);
        // SVS_DELETE catch,�����ظ�catch�����Թر�
        try
        {
            pTmpDevice = new (pch) CAC_Realtime_Record_Device(strLensId, encoderType, nIndex);
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_ERROR,
                "Create a record object for camera failed."
                "Alloacate memory failed.cameraId[%s]."
                "There are [%u] cameras recording now.",
                strLensId ,
                device_map_.current_size()));

            *((uint32_t *)(void*)pch) = nIndex;
            (void)free_list_.insert_tail(pch);

            return SVS_RESPCODE_OTHER_ERROR;
        }

        int32_t bindResult = device_map_.bind(pTmpDevice->get_frontdev_id(), pTmpDevice);

        if ( 0 != bindResult )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Bind a record object for camera to map failed."
                "cameraId[%s],Return code[%d]."
                "There are [%u] cameras recording now.",
                strLensId,
                bindResult,
                device_map_.current_size()));
            pTmpDevice->~CAC_Realtime_Record_Device();
            // ����pclint�澯����placement new���ڴ���Ҫ�Żض��У�û���ڴ�й©
            pTmpDevice = NULL;//lint !e423

            *((uint32_t *)(void*)pch) = nIndex;
            (void)free_list_.insert_tail(pch);

            return SVS_RESPCODE_OTHER_ERROR;
        }

        // �Ѷ���bind��readonly device map�У����ʧ�ܣ���Ӱ��ҵ��
        (void)bind2readonlyDeviceMap(pTmpDevice);

        // û�е�һ�ΰ󶨾�ͷ���󣬹����־��ACE���⣬�ָ�error code
        ACE_OS::last_error(0);

        using_vector_[nIndex] = pTmpDevice;

        SVS_LOG((SVS_LM_INFO,
            "Create a record object for camera succeeded, "
            "cameraId[%s], "
            "There are [%u] cameras recording now.",
            strLensId,
            device_map_.current_size()));
    }
    else
    {
        (void)pTmpDevice->increase_reference();
    }

    pDevice = pTmpDevice;

    return 0;
}

/*****************************************************************************
�� �� ��  : get_device
��������  : ����ָ�����豸ID�ִ������豸����
�������  : const char *strFrontDevId : Ҫ���ҵ��豸ID
�������  : CAC_Realtime_Record_Device *&pDevice:��ͷ����ָ���ַ
�� �� ֵ  : �ɹ����ز��ҵ����豸����ָ��, ʧ�ܷ���NULL
�޸���ʷ  :
*****************************************************************************/
CAC_Realtime_Record_Device *
CAC_RT_Record_Device_Manager::get_device(const char *strFrontDevId)
{
    SVS_TRACE();

    CAC_Realtime_Record_Device *pDevice = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);

    int32_t findResult = device_map_.find(strFrontDevId, pDevice);

    if ( 0 != findResult )
    {
        return NULL;
    }

    //���ü�����1
    int32_t increaseResult = pDevice->increase_reference();
    if( SVS_RESULT_FAILURE == increaseResult )
    {
        pDevice = NULL;
        return NULL;
    }

    return pDevice;

}

/*****************************************************************************
�� �� ��  : get_device
��������  : ����ָ�����豸ID�ִ������豸����
�������  : uint32_t nId:�����ڲ�ID
�������  : CAC_Realtime_Record_Device *&pDevice:��ͷ����ָ���ַ
�� �� ֵ  : �ɹ����ز��ҵ����豸����ָ��, ʧ�ܷ���NULL
�޸���ʷ  :
*****************************************************************************/
CAC_Realtime_Record_Device *CAC_RT_Record_Device_Manager::get_device(uint32_t nId)
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);
    CAC_Realtime_Record_Device *pDevice = using_vector_[nId];
    if (NULL == pDevice)
    {
        return NULL;
    }

    //���ü�����1
    int32_t increaseResult = pDevice->increase_reference();
    //������1ʧ��,����NULL
    if( SVS_RESULT_FAILURE == increaseResult )
    {
        pDevice = NULL;
        return NULL;
    }

    return pDevice;
}

/*****************************************************************************
�� �� ��  : control_record
��������  : ����¼��
�������  : const char * const strLensId:��ͷID
            uint8_t encoderType:��ͷ��������
            uint8_t recordFlag:��ͷ¼������
            time_t endTime:���Խ���ʱ��
            uint32_t eventBeforeSeconds:Ԥ¼ʱ��
            uint32_t eventAfterSeconds:��¼ʱ��
            time_t eventStratEndTime:¼�����ʱ��
�������  : NA
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::control_record(
    const char * const strLensId,
    uint8_t encoderType,
    uint8_t recordFlag,
    time_t endTime,
    uint32_t eventBeforeSeconds,
    uint32_t eventAfterSeconds,
    time_t eventStratEndTime)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO,
        "Start recording, "
        "CameraId[%s], encoder type[%u], record type[%u], "
        "prerecord time length[%u], postrecord time length[%u].",
        strLensId,
        encoderType,
        recordFlag,
        eventBeforeSeconds,
        eventAfterSeconds ));

    //����¼���豸����
    CAC_Realtime_Record_Device *pLens = get_device(strLensId);

    if (NULL == pLens)
    {
        //����ǽ�ֹ¼��ֱ�ӷ���
        if (FROBID_RECORDING == recordFlag)
        {
            SVS_LOG((SVS_LM_INFO,
                "Start recording finished, "
                "The camera's record type is forbid record, cameraId[%s].",
                strLensId ));

            return 0;
        }

        //������¼�¼����Ԥ¼��ʱ��Ϊ0��ֱ�ӷ���
        if ( (RECORDING_ON_EVENT == recordFlag)
            && ( 0 == eventBeforeSeconds ))
        {
            SVS_LOG((SVS_LM_INFO,
                "Start recording finished, "
                "the record type is event record, "
                "and prerecord time is 0, cameraId[%s].",
                strLensId ));

            return 0;
        }

        //����¼���豸
        int32_t addResult = add_device(strLensId, encoderType, pLens);

        if ( 0 != addResult )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Start recording failed, "
                "Create a object for camera failed, "
                "cameraId[%s], encode type[%u].",
                strLensId,
                encoderType));

            return SVS_RESULT_FAILURE;
        }

    }

    //����¼�����
    (void)pLens->update_record_parameter(eventBeforeSeconds, eventAfterSeconds);

    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pLens);

    int32_t recordResult = pLens->control_record(recordFlag, endTime, eventStratEndTime);

    return recordResult;
}

/*****************************************************************************
�� �� ��  : event_happen
��������  : �¼�¼����
�������  : const char * const strLensId:��ͷID
            const char * strAlarmDeviceId:�澯��ͷID
            uint32_t eventType:�¼�����
            uint8_t encoderType:����������
            time_t endTime:���Խ����¼�
            uint32_t eventBeforeSeconds:Ԥ¼ʱ��
            uint32_t eventAfterSeconds:��¼ʱ��
            bool bIsRecord:�¼�����ʱ���Ƿ�������Ҫ¼��
�������  : NA
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::event_happen(
    const char * const strLensId,
    const char * strAlarmDeviceId,
    uint32_t eventType,
    uint8_t encoderType,
    time_t endTime,
    uint32_t eventBeforeSeconds,
    uint32_t eventAfterSeconds,
    bool bIsRecord)
{
    SVS_TRACE();

    //���ҽ����¼�¼���豸����
    CAC_Realtime_Record_Device *pLens = get_device(strLensId);

    if (NULL == pLens )
    {
        //����¼���豸
        int32_t addResult = add_device(strLensId, encoderType, pLens);

        if ( SVS_RESULT_OK != addResult )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Add a object for camera failed, cameraId[%s], encode type[%u].",
                strLensId,
                encoderType ));

            return SVS_RESULT_FAILURE;
        }

        (void)pLens->update_record_parameter(eventBeforeSeconds, eventAfterSeconds );

    }

    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pLens);
    int32_t eventProcResult = pLens->event_happen(strAlarmDeviceId,
                                                eventType,
                                                endTime,
                                                bIsRecord);

    return eventProcResult;
}

/*****************************************************************************
�� �� ��  : event_over
��������  : �¼���������
�������  : const char * const strLensId:��ͷID
            const char * strAlarmDeviceId:�澯��ͷID
            uint32_t eventType:�¼�����
�������  : NA
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::event_over(
    const char * const strLensId,
    const char * strAlarmDeviceId,
    uint32_t eventType )
{
    SVS_TRACE();
    //���ҽ����¼�¼���豸����
    CAC_Realtime_Record_Device *pLens = get_device(strLensId);
    if (NULL == pLens)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Process alarm event overing failed.Camera is unfound."
            "cameraId[%s].",
            strLensId));
        return SVS_RESULT_FAILURE;
    }

    // �ҵ���ͷ��֪ͨ��ͷ�澯�����¼�
    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pLens);
    int32_t eventProcResult = pLens->event_over(strAlarmDeviceId, eventType);

    return eventProcResult;
}

/*****************************************************************************
�� �� ��  : handle_routerinfo
��������  : ·����Ϣ����
�������  : const ACE_Message_Block *mb:·����Ϣ����
�������  : NA
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::handle_routerinfo(const ACE_Message_Block *mb)
{
    PSVS_STORESERVER_ROUTER_INFO_REQ_MSG pReq =
        (PSVS_STORESERVER_ROUTER_INFO_REQ_MSG)(void*)mb->rd_ptr();

    char strDevId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0};

    SetLocalizeDeviceId((uint8_t *)strDevId,
        sizeof(strDevId),
        pReq->DeviceId.ID,
        NLS_DEVICE_ID_MAX_LEN);

    if (0 == pReq->StreamId)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Handle route info failed, routeId is invalid, "
            "cameraId[%s], routeId[%u], route type[%u], responseCode[%u].",
            strDevId, pReq->StreamId, pReq->StreamType,
            pReq->RespCode));

        return SVS_RESULT_FAILURE;
    }

    ACE_INET_Addr addr;
    (void)addr.set( pReq->VTDUPORT, pReq->VTDUIP, 0 );

    SVS_LOG((SVS_LM_INFO,
        "Begin to handle route, "
        "CameraId[%s], routeId[%u], route type[%u],"
        "protocol[%u], VTDU[%s:%u], resp code[%d].",
        strDevId,
        pReq->StreamId,
        pReq->StreamType,
        pReq->ProtocolType,
        addr.get_host_addr(),
        addr.get_port_number(),
        pReq->RespCode));

    //������Ϣ������
    uint16_t msgLen = sizeof(SVS_ROUTER_INFO_ACK_MSG);

    ACE_Message_Block *respMsgMb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, msgLen );

    if ( NULL == respMsgMb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Send response message of route to StoreSvr failed, "
            "Allocate message block failed, type[%d] size[%d]",
            CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, msgLen));

        return SVS_RESULT_FAILURE;
    }

    SVS_ROUTER_INFO_ACK_MSG* pRespMsg =
        (SVS_ROUTER_INFO_ACK_MSG*)(void*)respMsgMb->wr_ptr();

    respMsgMb->wr_ptr( msgLen );

    FillCommonHeader(   &pRespMsg->Header,
                        SVS_MSG_TYPE_STORESERVER_ROUTER_INFO_RESP,
                        transactionno_respond(pReq->Header.TransactionNo),
                        msgLen);

    pRespMsg->StreamId   = pReq->StreamId;
    pRespMsg->StreamType = pReq->StreamType;
    pRespMsg->RespCode = 0;
    (void)ACE_OS::memset(pRespMsg->Dummy, 0, sizeof(pRespMsg->Dummy));

    CAC_Realtime_Record_Device *pFrontDev = get_device(strDevId); //�����豸

    if (NULL == pFrontDev)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Process route failed, Camera is unfound, cameraId[%s].",
            strDevId));

        pRespMsg->RespCode = SVS_RESPCODE_NO_FRONTDEV;

        int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( respMsgMb );

        if ( 0 != sendResult )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Message enqueue failed, msgtype[0x%04x], returncode[%d].",
                pRespMsg->Header.MsgType,
                sendResult ));
            g_p_msg_buffer->free_mb( respMsgMb );

            return SVS_RESULT_FAILURE;
        }

        SVS_LOG((SVS_LM_INFO,
            "Send response message of route to StoreSvr succeeded, "
            "transmission No[0x%04x] responseCode[%d] routeId[%u].",
            pRespMsg->Header.TransactionNo,
            pRespMsg->RespCode,
            pRespMsg->StreamId));

        return sendResult;
    }

    {
        SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pFrontDev);

        if (0 != pReq->RespCode) //����·��ʧ��
        {
            SVS_LOG((SVS_LM_ERROR,
                "Request route failed, "
                "CameraId[%s],route id[%d], resp code[%d].",
                pFrontDev->get_frontdev_id(),
                pReq->StreamId,
                pReq->RespCode));

            g_p_msg_buffer->free_mb( respMsgMb );
            (void)pFrontDev->handle_routerinfo(pReq);
            return SVS_RESULT_FAILURE;
        }

        //·�ɷ���ɹ�������·����Ϣ
        int32_t handleRouterinfoResult = pFrontDev->handle_routerinfo(pReq);

        if ( SVS_RESULT_OK != handleRouterinfoResult )
        {
            pRespMsg->RespCode = SVS_RESPCODE_OTHER_ERROR;
        }
    }

    int32_t sendResult =  CAC_RT_Record_Server::instance()->send_msg( respMsgMb );
    if ( 0 != sendResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Send response message of route to StoreSvr failed, "
            "msgtype[0x%04x], returncode[%d].",
            pRespMsg->Header.MsgType,
            sendResult ));
        g_p_msg_buffer->free_mb( respMsgMb );

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Send response message of route to StoreSvr succeeded, "
        "transmission No[0x%04x] responseCode[%d] routeId[%u].",
        pRespMsg->Header.TransactionNo,
        pRespMsg->RespCode,
        pRespMsg->StreamId));

    return sendResult;
}

/*****************************************************************************
�� �� ��  : shutdown_router
��������  : ֹͣ���е��豸·��
�������  : NA
�������  : NA
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
void CAC_RT_Record_Device_Manager::shutdown_router()
{
    SVS_DEVICE_MAP_ENTRY *pEntry = NULL;
    CAC_Realtime_Record_Device *pDev = NULL;

    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, map_mutex_);

    SVS_DEVICE_MAP_ITER iter(device_map_);

    SVS_LOG( ( SVS_LM_WARNING,
        "Begin to shutdown all routes.The number of current recording cameras is [%d].",
        device_map_.current_size() ) );

    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pDev = pEntry->int_id_;
       (void) pDev->shutdown_router();
    }

    SVS_LOG( ( SVS_LM_WARNING, "Shutdown all routes finished." ) );
}

/*****************************************************************************
�� �� ��  : stop_device_record
��������  : ָֹͣ���豸��¼��
�������  : const char * const strDevId : Ҫֹͣ¼����豸�ɣ�
            bool bSendOverMsg �����ͽ���·����Ϣ��־��Ĭ��Ϊtrue(����)
�������  : NA
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::stop_device_record(
    const char * const strDevId,
    bool bSendOverMsg )
{
    SVS_LOG((SVS_LM_INFO, "Begin to stop camera recording.CameraId[%s].",
        strDevId));

    CAC_Realtime_Record_Device *pDevice = get_device(strDevId);
    if (NULL == pDevice)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stop camera recording failed.Camera is unfound.cameraId[%s].",
            strDevId));
        return SVS_RESPCODE_OTHER_ERROR;
    }

    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pDevice);
    (void)pDevice->clear_event_record_info();
    int32_t stopDeviceResult = pDevice->stop_device( bSendOverMsg );

    return stopDeviceResult;
}

/*****************************************************************************
�� �� ��  : stop_device_record
��������  : �豸���ߴ���
�������  : const char * const strDevId :�豸ID
�������  : NA
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::device_off_line(const char * const strDevId)
{
    SVS_TRACE();
    // �Ҿ�ͷ�Ƿ���¼��
    CAC_Realtime_Record_Device *pDevice = get_device(strDevId);

    // ���û���ҵ���ͷ
    if (NULL == pDevice)
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Process camera offline failed.CameraId is unfound.cameraId[%s].",
            strDevId ) );
        return SVS_RESPCODE_OTHER_ERROR;
    }

    // �ҵ���ͷ��֪ͨ��ͷ����
    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pDevice);
    int32_t offLineResult = pDevice->off_line();

    // ��ͷ���߳ɹ����map����ɾ��
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
    int32_t unbindResult = device_map_.unbind(strDevId);
    if (0 != unbindResult )
    {
        SVS_LOG((SVS_LM_WARNING,
            "device offline: Delete the object of camera failed."
            "The object is unfound.cameraId[%s].",
            strDevId));
        return SVS_RESULT_FAILURE;
    }
    return offLineResult;
}

/*****************************************************************************
�� �� ��  : on_timer
��������  : ��ʱ���ص�����
�������  : int32_t eventId:�¼�ID
            int32_t timerId:��ʱ��ID
            int32_t nDummy:��ʱ���¼�����
�������  : NA
�� �� ֵ  : NA
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::on_timer(int32_t eventId, int32_t timerId, int32_t nDummy )
{
    SVS_TRACE();

    // �����¼��״̬��ⶨʱ������������е�����¼��ľ�ͷ�������״̬
    if (CAC_Realtime_Record_Device::TIMER_TYPE_RECORD_STATUS == nDummy)
    {
        CAC_Realtime_Record_Device *pDevice = NULL;
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pReadonlyObj, readonly_device_map_mutex_, -1);

        SVS_DEVICE_MAP_ITER iter(readonly_device_map_);
        SVS_DEVICE_MAP_ENTRY *pEntry = NULL;

        for (; 0 != iter.next(pEntry); (void)iter.advance())
        {
            pDevice = pEntry->int_id_;
            (void)pDevice->on_timer(eventId, timerId, nDummy);
        }

        return SVS_RESULT_OK;
    }

    // �Ҿ����¼���豸��֪ͨ�䶨ʱ���¼�
    CAC_Realtime_Record_Device *pDevice = get_device((uint32_t)eventId);

    if (NULL != pDevice)
    {
        SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pDevice );
        (void)pDevice->on_timer(eventId, timerId, nDummy);
        return SVS_RESULT_OK;
    }

    SVS_LOG((SVS_LM_WARNING,
        "Process timer callback failed, can't find the camera, timerId[%d],eventId[%d].",
        eventId,
        timerId));

    return SVS_RESULT_FAILURE;
}

/*****************************************************************************
�� �� ��  : decrease_reference
��������  : ���ü����Զ���
�������  : CAC_Realtime_Record_Device *pDevice:¼���豸�����ָ��
�������  : NA
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::decrease_reference(
        CAC_Realtime_Record_Device *pDevice)
{
    SVS_TRACE();
    if( NULL == pDevice )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Process reference decreasing for recording object failed."
            "The parameter is NULL.") );
        return SVS_RESULT_FAILURE;
    }

    size_t nTotal = 0;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, SVS_RESULT_FAILURE);
        if (1 != pDevice->decrease_reference())
        {
            pDevice = NULL;
            return SVS_RESULT_OK;
        }
        CAC_Realtime_Record_Device *pOldDevice = NULL;
        int32_t findResult = device_map_.find(pDevice->get_frontdev_id(), pOldDevice);
        if ( ( 0 == findResult ) && (pDevice == pOldDevice))
        {
            (void)device_map_.unbind(pDevice->get_frontdev_id());
            SVS_LOG((SVS_LM_INFO,
                      "remove CameraId[%s] from device map.",
                      pDevice->get_frontdev_id()));
        }

        using_vector_[pDevice->internal_id()] = NULL;
        nTotal = device_map_.current_size();

        {
            // unbind from readonly map�У����ʧ�ܲ�Ӱ��ҵ��
            ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pReadonlyObj, readonly_device_map_mutex_, SVS_RESULT_FAILURE);
            (void)readonly_device_map_.unbind(pDevice->get_frontdev_id());
            ACE_OS::last_error(0);
        }
    }

    SVS_LOG((SVS_LM_INFO,
        "Begin to stop the camera recording.CameraId[%s]release source."
        "Still has [%d] cameras recording now.",
        pDevice->get_frontdev_id(),
        nTotal ));

    char strLensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {'\0'};
    //(void)ACE_OS::memcpy(strLensId, pDevice->get_frontdev_id(), DEVICEID_LEN);
    SetLocalizeDeviceId((uint8_t *)strLensId,
            sizeof(strLensId),
            pDevice->get_frontdev_id());

    uint32_t id = pDevice->internal_id();
    (void)pDevice->reset();
    pDevice->~CAC_Realtime_Record_Device();
    *((uint32_t *)(void*)pDevice) = id;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, SVS_RESULT_FAILURE);
    (void)free_list_.insert_tail((char *)pDevice);

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : debug_get_device_info
��������  : telnet��ȡ�豸��ϸ��Ϣ
�������  : const char *strFrontDevId:��ͷID
            uint32_t szDeviceInfo :����������
�������  : char *strDeviceInfo:telnet���������
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::debug_get_device_info(
    const char *strFrontDevId,
    char *strDeviceInfo,
    uint32_t szDeviceInfo )
{
    SVS_TRACE();
    if ((NULL == strDeviceInfo)
        || (NULL == strFrontDevId))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Remote debugging get camera's detailed recording information failed."
            "The parameter is invalid.DeviceInfo[0x%08x] strFrontDevId[0x%08x]",
            strDeviceInfo,
            strFrontDevId));
        return SVS_RESULT_FAILURE;
    }

    CAC_Realtime_Record_Device *pDevice = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);

    if (0 != device_map_.find(strFrontDevId, pDevice))
    {
        (void)ACE_OS::snprintf( strDeviceInfo,
                                szDeviceInfo,
                                "Camera is unfound.CameraId[%s]",
                                strFrontDevId  );
        return 1;
    }

    int32_t getDeviceInfoResult = pDevice->debug_get_device_info(
                                    strDeviceInfo,
                                    szDeviceInfo );

    return getDeviceInfoResult;
}

/*****************************************************************************
�� �� ��  : debug_get_all_device_info
��������  : ȡ�����豸�Ļ�����Ϣ
�������  : uint32_t szDeviceInfo :����������
�������  : char *strDeviceInfo:telnet���������
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::debug_get_all_device_info(
    char *strDeviceInfo,
    uint32_t szDeviceInfo )
{
    SVS_TRACE();
    CAC_Realtime_Record_Device *pDevice = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);

    SVS_DEVICE_MAP_ITER iter(device_map_);
    SVS_DEVICE_MAP_ENTRY *pEntry = NULL;
    size_t writeLen = 0;
    int32_t recordingDeviceNum = 0;

    int32_t probidDeviceNum = 0;    //��ֹ¼���豸����
    int32_t alwaysDeviceNum = 0;    //����¼���豸����
    int32_t eventDeviceNum = 0;     //�¼�¼���豸����
    int32_t manualDeviceNum = 0;    // �ֶ�¼���豸����

    SVS_STATUS_DEV_NUM_T devNumInfo;
    memset(&devNumInfo, 0, sizeof(devNumInfo));

    (void)ACE_OS::snprintf(   strDeviceInfo,
                        szDeviceInfo,
                        "\t       CameraId         RouteId  RecordType  RecordStatus\n" );

    int32_t freeBuffLen = 0;
    for (; 0 != iter.next(pEntry); (void)iter.advance() )
    {
        pDevice = pEntry->int_id_;
        writeLen = ACE_OS::strlen( strDeviceInfo );

        //�����������꣬�˳�ѭ��
        freeBuffLen = (int32_t)(szDeviceInfo-writeLen);
        if( 0 >= freeBuffLen )
        {
            break;
        }

        (void)ACE_OS::snprintf(   &strDeviceInfo[writeLen],
                            szDeviceInfo-writeLen,
                            "\t%s    %8d  %5d   %2d\n",
                            pDevice->get_frontdev_id(),
                            pDevice->router_id(),
                            pDevice->record_flag(),
                            pDevice->state() );

        //��ȡ¼���־���豸����
        switch( pDevice->record_flag() )
        {
            case CAC_Realtime_Record_Device::RECORD_TYPE_PROBID:
                ++probidDeviceNum;
                break;
            case CAC_Realtime_Record_Device::RECORD_TYPE_ALWAYS:
                ++alwaysDeviceNum;
                break;
            case CAC_Realtime_Record_Device::RECORD_TYPE_EVENT:
                ++eventDeviceNum;
                break;
            case CAC_Realtime_Record_Device::RECORD_TYPE_MANUAL:
                ++manualDeviceNum;
                break;
            default :
                SVS_LOG( (SVS_LM_ERROR, "Record falg is unknow.Flag:%d",
                    pDevice->record_flag() ) );
                break;
        }

        //��ȡ�豸״̬�豸����
        count_dev_num_by_status(pDevice,devNumInfo);
        recordingDeviceNum++;
    }
    writeLen = ACE_OS::strlen( strDeviceInfo );
    if( 0 < szDeviceInfo-writeLen )
    {
        (void)ACE_OS::snprintf(   &strDeviceInfo[writeLen],
                            szDeviceInfo-writeLen,
                            "\n\t  CameraTotalNum                [%5d]"
                            "\n"
                            "\n\t  ForbidRecordCameraNum         [%5d]"
                            "\n\t  AlwaysRecordCameraNum         [%5d]"
                            "\n\t  EventRecordCameraNum          [%5d]"
                            "\n\t  ManualRecordCameraNum         [%5d]"
                            "\n"
                            "\n\t  InitializingCameraNum         [%5d]"
                            "\n\t  SleepingCameraNum             [%5d]"
                            "\n\t  DiskFull,WaitingCameraNum     [%5d]"
                            "\n\t  RequestingRouteCameraNum      [%5d]"
                            "\n\t  ConnectingVTDUCameraNum       [%5d]"
                            "\n\t  ReceivingMediaStreamCameraNum [%5d]"
                            "\n\t  WaitingOverCameraNum          [%5d]"
                            "\n\t  StoppingCameraNum             [%5d]"
                            "\n\n",
                            recordingDeviceNum,
                            probidDeviceNum,
                            alwaysDeviceNum,
                            eventDeviceNum,
                            manualDeviceNum,
                            devNumInfo.initStatusDeviceNum,
                            devNumInfo.sleepStatusDeviceNum,
                            devNumInfo.diskFullWaitingDeviceNum,
                            devNumInfo.requestRouterStatusDeviceNum,
                            devNumInfo.connectVtduStatusDeviceNum,
                            devNumInfo.recvFileStatusDeviceNum,
                            devNumInfo.waittingOverStatusDeviceNum,
                            devNumInfo.stopStatusDeviceNum);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : debug_get_all_device_detail_info
��������  : ȡ�����豸����ϸ��Ϣ
�������  : uint32_t szDeviceInfo :����������
�������  : char *strDeviceInfo:telnet���������
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::debug_get_all_device_detail_info(
    char *strDeviceInfo,
    uint32_t szDeviceInfo )
{
    SVS_TRACE();
    CAC_Realtime_Record_Device *pDevice = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);

    SVS_DEVICE_MAP_ITER iter(device_map_);
    SVS_DEVICE_MAP_ENTRY *pEntry = NULL;
    size_t writeLen = 0;
    strDeviceInfo[0] = '\0';

    for (; 0 != iter.next(pEntry); (void)iter.advance() )
    {
        pDevice = pEntry->int_id_;
        writeLen = ACE_OS::strlen( strDeviceInfo );

        //�����������꣬�˳�ѭ��
        if( 16 >= szDeviceInfo-writeLen )
        {
            break;
        }

        strDeviceInfo[writeLen++] = '\n';
        strDeviceInfo[writeLen] = '\0';

        (void)pDevice->debug_get_device_info( &strDeviceInfo[writeLen], szDeviceInfo-writeLen );
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : count_dev_num_by_status
��������  :
�������  : CAC_Realtime_Record_Device* pDevice:������Ҫ�������豸����ָ��
�������  : SVS_STATUS_DEV_NUM_T & devStateNum:�ۼƽ��
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
void CAC_RT_Record_Device_Manager::count_dev_num_by_status(
                                   const CAC_Realtime_Record_Device* pDevice,
                                   SVS_STATUS_DEV_NUM_T & devStateNum)const
{
    SVS_TRACE();
    switch( pDevice->state() )
    {
        case CAC_Realtime_Record_Device::RECORD_STATE_INIT:
            ++devStateNum.initStatusDeviceNum;
            break;
        case CAC_Realtime_Record_Device::RECORD_STATE_SLEEP:
            ++devStateNum.sleepStatusDeviceNum;
            break;
        case CAC_Realtime_Record_Device::RECORD_STATE_DISK_FULL:
            ++devStateNum.diskFullWaitingDeviceNum;
             break;
        case CAC_Realtime_Record_Device::RECORD_STATE_REQUEST_ROUTER:
            ++devStateNum.requestRouterStatusDeviceNum;
            break;
        case CAC_Realtime_Record_Device::RECORD_STATE_CONNECT_VTDU:
            ++devStateNum.connectVtduStatusDeviceNum;
            break;
        case CAC_Realtime_Record_Device::RECORD_STATE_RECEIVING_FILE:
            ++devStateNum.recvFileStatusDeviceNum;
            break;
        case CAC_Realtime_Record_Device::RECORD_STATE_WAITING_OVER:
            ++devStateNum.waittingOverStatusDeviceNum;
            break;
        case SVS_FRONT_DEVICE_STATE_STOP:
            ++devStateNum.stopStatusDeviceNum;
            break;
        default:
            SVS_LOG( (SVS_LM_ERROR,
                "Status is unknown .status[%d]",  pDevice->state() ) );
            break;
    }

    return;
}

/*****************************************************************************
�� �� ��  : get_device_by_routerId
��������  : ����ָ����·��ID�����豸����
�������  : uint32_t RouterId : Ҫ���ҵ�·��ID
�������  : NA
�� �� ֵ  : �ɹ����ز��ҵ����豸����,ʧ�ܷ���NULL
�޸���ʷ  :
*****************************************************************************/
CAC_Realtime_Record_Device *
CAC_RT_Record_Device_Manager::get_device_by_routerId( uint32_t RouterId )
{
    SVS_TRACE();
    CAC_Realtime_Record_Device *pDevice = NULL;
    CAC_Realtime_Record_Device *pDeviceTmp = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);

    // ����·��ID�����豸
    SVS_DEVICE_MAP_ITER iter(device_map_);
    SVS_DEVICE_MAP_ENTRY *pEntry = NULL;
    for (; 0 != iter.next(pEntry); (void)iter.advance() )
    {
        pDeviceTmp = pEntry->int_id_;
        if ( RouterId == pDeviceTmp->router_id() )
        {
            pDevice = pDeviceTmp;
            break;
        }
    }

    //û���ҵ�,����NULL
    if( NULL == pDevice )
    {
        return NULL;
    }

    //���ҳɹ�,���ü�����1��,���ظö���ָ��
    int32_t increaseResult = pDevice->increase_reference();
    if( SVS_RESULT_FAILURE == increaseResult )
    {
        pDevice = NULL;
        return NULL;
    }

    return pDevice;

}

/*****************************************************************************
�� �� ��  : shutdown_router
��������  : �ر�ָ��·�ɵ�¼��·��
�������  : const uint32_t RouterID : Ҫ�رյ�·��ID
�������  : NA
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::shutdown_router( const uint32_t RouterID )
{
    SVS_TRACE();
    //����ָ��·�ɵ��豸
    CAC_Realtime_Record_Device *pDevice = get_device_by_routerId( RouterID );
    if( NULL == pDevice )
    {
        SVS_LOG( ( SVS_LM_WARNING,
            "Shutdown route failed.The route id is unfound.routeId[%u].",
            RouterID ) );
        return SVS_RESULT_FAILURE;
    }

    //ֹͣ¼��
    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pDevice);
    int32_t stopRecordResult = pDevice->stop_record();

    return stopRecordResult;
}

/*****************************************************************************
�� �� ��  : timer_callback
��������  : ��ʱ���ص�����
�������  : const uint32_t RouterID : Ҫ�رյ�·��ID
�������  : NA
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
void CAC_RT_Record_Device_Manager::timer_callback( void *pArg,
                            int32_t eventId,
                            int32_t timerId,
                            int32_t nDummy )
{
    uint32_t msgLen = sizeof(SVS_TIMER_CALL_BACK_MSG);

    ACE_Message_Block *mb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK, msgLen );

    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to allocate mb. type[%d], size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK,
            msgLen ));

        return ;
    }

    SVS_TIMER_CALL_BACK_MSG* pMsg = (SVS_TIMER_CALL_BACK_MSG*)(void*)mb->wr_ptr();
    if(CAC_Realtime_Record_Device::TIMER_TYPE_RECORD_STATUS == nDummy)
    {
        pMsg->MsgType = SVS_TIMER_CALL_BACK_MSG::REALTIME_RECORD_STATUS_TYPE;
    }
    else
    {
        pMsg->MsgType = SVS_TIMER_CALL_BACK_MSG::RT_RECORD_DEVICE_MANAGER_TYPE;
    }

    pMsg->ProcObject = pArg;
    pMsg->EventId = eventId;
    pMsg->TimerId = timerId;
    pMsg->Dummy = nDummy;
    mb->wr_ptr( msgLen );

    (void)CAC_RT_Record_Server::instance()->add_timer_callback_msg( mb );
}

/*****************************************************************************
�� �� ��  : build_update_record_index_sql
��������  : ���ݴ����¼��������Ϣ����SQL���
�������  : mb :        ¼��������Ϣ
            sqlBuff:    SQL��仺����
            sqlBuffSize: SQL��仺������С
�������  : sqlBuff:    SQL��仺����
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::build_update_record_index_sql(
    const ACE_Message_Block* mb,
    char *sqlBuff,
    uint32_t sqlBuffSize) const
{
    SVS_TRACE();
    // ����У��
    if (( NULL == mb ) || ( NULL == sqlBuff ))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Build update record index sql failed.The parameter is invalid."));
        return SVS_RESULT_FAILURE;
    }

    const Update_Record_Index *pIndex = (Update_Record_Index *)(void*)mb->rd_ptr();

    switch ( pIndex->Type )
    {
        // ����Ǹ�������
    case RECORD_INDEX_TYPE_UPDATE:
        {
            char strEndTime[TIME_STRING_LEN];
            char strStartTime[TIME_STRING_LEN];
            (void)SVS_SS_UTILITIES::time2str(strEndTime, sizeof(strEndTime), pIndex->EndTime);
            (void)SVS_SS_UTILITIES::time2str(strStartTime, sizeof(strStartTime), pIndex->StartTime);

            time_t startTime = pIndex->StartTime;
            time_t endTime = pIndex->EndTime;
            int32_t fileCount = 0;

            if ( 0 != pIndex->StartTime % BIG_RECORDFILE_TIME_SLICE )
            {
                startTime += BIG_RECORDFILE_TIME_SLICE;
                startTime -= startTime % BIG_RECORDFILE_TIME_SLICE;
                fileCount += BIG_RECORDFILE_TIME_SLICE;

            }

            if ( 0 != pIndex->EndTime % BIG_RECORDFILE_TIME_SLICE )
            {
                endTime = pIndex->EndTime - pIndex->EndTime%BIG_RECORDFILE_TIME_SLICE;
                fileCount += BIG_RECORDFILE_TIME_SLICE;
            }

            fileCount += endTime - startTime;
            fileCount /= BIG_RECORDFILE_TIME_SLICE;

            (void)ACE_OS::snprintf(  sqlBuff,
                sqlBuffSize,
                "UPDATE tbl_recordfile_outline_info "
                "   SET end_time = TO_DATE('%s', 'YYYYMMDDHH24MISS'), "
                "       file_count = %d "
                " WHERE device_id = '%s' "
                "   AND start_time = TO_DATE('%s', 'YYYYMMDDHH24MISS')",
                strEndTime,
                fileCount,
                pIndex->LensId,
                strStartTime);
            break;
        }

        // ����ǲ�������
    case RECORD_INDEX_TYPE_INSERT:
        {
            char strEndTime[TIME_STRING_LEN];
            char strStartTime[TIME_STRING_LEN];
            (void)SVS_SS_UTILITIES::time2str(strEndTime, sizeof(strEndTime), pIndex->EndTime);
            (void)SVS_SS_UTILITIES::time2str(strStartTime, sizeof(strStartTime), pIndex->StartTime);
            (void)ACE_OS::snprintf(   sqlBuff,
                sqlBuffSize,
                "INSERT INTO tbl_recordfile_outline_info"
                "     VALUES (%d, '%s', TO_DATE('%s', 'YYYYMMDDHH24MISS'),"
                "             TO_DATE('%s', 'YYYYMMDDHH24MISS'), "
                "             1, '%s', '%s', %d)",
                pIndex->StartTime,
                pIndex->LensId,
                strStartTime,
                strEndTime,
                pIndex->RecordFilePath,
                CAC_RT_Record_Server::instance()->str_id(),
                CAC_RT_Record_Server::instance()->storage_type());
            break;
        }

        // ����Ǹ�������
    case RECORD_INDEX_TYPE_OVERWRITE:
        {
            char strTimeId[TIME_STRING_LEN];
            char strStartTime[TIME_STRING_LEN];
            (void)SVS_SS_UTILITIES::time2str(strTimeId, sizeof(strTimeId), pIndex->ChangeBeforeStartTime);
            (void)SVS_SS_UTILITIES::time2str(strStartTime, sizeof(strStartTime), pIndex->StartTime);

            (void)ACE_OS::snprintf(   sqlBuff,
                sqlBuffSize,
                "UPDATE tbl_recordfile_outline_info "
                "   SET start_time = TO_DATE('%s', 'YYYYMMDDHH24MISS') "
                " WHERE device_id = '%s' "
                "   AND start_time = TO_DATE('%s', 'YYYYMMDDHH24MISS')",
                strStartTime,
                pIndex->LensId,
                strTimeId);
            break;
        }

    default:
        {
            SVS_LOG((SVS_LM_WARNING,
                "Not support record index type."
                "cameraId[%s],record index type[%d].",
                pIndex->LensId,
                pIndex->Type ));

            return SVS_RESULT_FAILURE;
        }
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : handle_record_index
��������  : ���ݴ����¼��������Ϣ�б�������ݿ�
�������  : recordIndexList :   ¼��������Ϣ�б�
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
//int32_t CAC_RT_Record_Device_Manager::handle_record_index(
//    list<ACE_Message_Block*> &recordIndexList ) const
//{
//    SVS_TRACE();
//
//    list<ACE_Message_Block*>::iterator itRecordIndex = recordIndexList.begin();
//
//    for( itRecordIndex = recordIndexList.begin();
//             itRecordIndex != recordIndexList.end(); ++itRecordIndex)
//    {
//        Update_Record_Index *pIndex = (Update_Record_Index *)(void*)(*itRecordIndex)->rd_ptr();
//
//        if (NULL == pIndex)
//        {
//            SVS_LOG((SVS_LM_WARNING, "Update record index info is NULL."));
//
//            continue;
//        }
//
//        DTRecordIndex stRecordIndex;
//        memset(&stRecordIndex, 0x0, sizeof(stRecordIndex));
//
//        switch ( pIndex->Type )
//        {
//            //����Ǹ�������
//            case RECORD_INDEX_TYPE_UPDATE:
//            {
//                char szEndTimeTmp[TIME_STRING_LEN];
//                char szStartTimeTmp[TIME_STRING_LEN];
//                (void)SVS_SS_UTILITIES::time2str(szEndTimeTmp, sizeof(szEndTimeTmp), pIndex->EndTime);
//                (void)SVS_SS_UTILITIES::time2str(szStartTimeTmp, sizeof(szStartTimeTmp), pIndex->StartTime);
//
//                time_t startTime = pIndex->StartTime;
//                time_t endTime = pIndex->EndTime;
//
//                int32_t fileCount = 0;
//
//                if ( 0 != pIndex->StartTime % BIG_RECORDFILE_TIME_SLICE )
//                {
//                    startTime += BIG_RECORDFILE_TIME_SLICE;
//                    startTime -= startTime % BIG_RECORDFILE_TIME_SLICE;
//                    fileCount += BIG_RECORDFILE_TIME_SLICE;
//                }
//
//                if ( 0 != pIndex->EndTime % BIG_RECORDFILE_TIME_SLICE )
//                {
//                    endTime = pIndex->EndTime - pIndex->EndTime%BIG_RECORDFILE_TIME_SLICE;
//                    fileCount += BIG_RECORDFILE_TIME_SLICE;
//                }
//
//                fileCount += endTime - startTime;
//                fileCount /= BIG_RECORDFILE_TIME_SLICE;
//
//                stRecordIndex.uiFileCount = (uint32_t)fileCount;
//
//                strncpy(stRecordIndex.szEndTime, szEndTimeTmp, DBAPI_TIMESTMP_LEN);
//                strncpy(stRecordIndex.szStartTime, szStartTimeTmp, DBAPI_TIMESTMP_LEN);
//                SetLocalizeDeviceId((uint8_t *)stRecordIndex.szDevId,
//                    sizeof(stRecordIndex.szDevId),
//                    pIndex->LensId);
//            }
//            break;
//
//            //����ǲ�������
//            case RECORD_INDEX_TYPE_INSERT:
//            {
//                char szEndTimeTmp[TIME_STRING_LEN];
//                char szStartTimeTmp[TIME_STRING_LEN];
//                (void)SVS_SS_UTILITIES::time2str(szEndTimeTmp, sizeof(szEndTimeTmp), pIndex->EndTime);
//                (void)SVS_SS_UTILITIES::time2str(szStartTimeTmp, sizeof(szStartTimeTmp), pIndex->StartTime);
//
//                stRecordIndex.uiFileCount = 1;
//                strncpy(stRecordIndex.szEndTime, szEndTimeTmp, DBAPI_TIMESTMP_LEN);
//                strncpy(stRecordIndex.szStartTime, szStartTimeTmp, DBAPI_TIMESTMP_LEN);
//                SetLocalizeDeviceId((uint8_t *)stRecordIndex.szDevId,
//                    sizeof(stRecordIndex.szDevId),
//                    pIndex->LensId);
//                strncpy(stRecordIndex.szFilePath, pIndex->RecordFilePath, DBAPI_FILE_PATH_LEN);
//                SetLocalizeDeviceId((uint8_t *)stRecordIndex.szRtServerId,
//                    sizeof(stRecordIndex.szRtServerId),
//                    CAC_RT_Record_Server::instance()->str_id());
//            }
//            break;
//
//            //����Ǹ�������
//            case RECORD_INDEX_TYPE_OVERWRITE:
//            {
//                char szTimeId[TIME_STRING_LEN];
//                char szStartTimeTmp[TIME_STRING_LEN];
//                (void)SVS_SS_UTILITIES::time2str(szTimeId, sizeof(szTimeId), pIndex->ChangeBeforeStartTime);
//                (void)SVS_SS_UTILITIES::time2str(szStartTimeTmp, sizeof(szStartTimeTmp), pIndex->StartTime);
//
//                strncpy(stRecordIndex.szEndTime, szTimeId, DBAPI_TIMESTMP_LEN);
//                strncpy(stRecordIndex.szStartTime, szStartTimeTmp, DBAPI_TIMESTMP_LEN);
//                SetLocalizeDeviceId((uint8_t *)stRecordIndex.szDevId,
//                    sizeof(stRecordIndex.szDevId),
//                    pIndex->LensId);
//            }
//            break;
//
//            default:
//            {
//                SVS_LOG((SVS_LM_WARNING,
//                    "Not support record index type, cameraId[%s], record index type[%d].",
//                    pIndex->LensId,
//                    pIndex->Type));
//
//                return SVS_RESULT_FAILURE;
//            }
//        }
//
//        int32_t iRet = DBAPI::instance()->WriteRecordIndex(pIndex->Type, stRecordIndex);
//
//        if (DBAPI_RETCODE_SUCCESS != iRet)
//        {
//            SVS_LOG((SVS_LM_ERROR,
//                "Write record index to database failed, camera[%s], writeType[%d], timeRange[%s-%s].",
//                stRecordIndex.szDevId,
//                pIndex->Type,
//                stRecordIndex.szStartTime,
//                stRecordIndex.szEndTime));
//        }
//    }
//
//    return SVS_RESULT_OK;
//}

/*****************************************************************************
�� �� ��  : refresh_event_record_outtime
��������  : �¼���ʧ���¼��ʱ����̬ˢ��
�������  : int32_t shiftValue:ƫ��ʱ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::refresh_event_record_outtime(
                                                             int32_t shiftValue)
{
    SVS_TRACE();
    CAC_Realtime_Record_Device *pDev = NULL;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);
    SVS_DEVICE_MAP_ITER iter(device_map_);
    SVS_DEVICE_MAP_ENTRY *pEntry = NULL;
    //��������¼�����
    for (; 0 != iter.next(pEntry); (void)iter.advance() )
    {
        pDev = pEntry->int_id_;
        (void)pDev->refresh_event_record_outtime(shiftValue);
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : overwrite_record_notify
��������  : ������������д��ʱ��ѡ��ɾ��ϵͳ�������¼�������ɾ��¼��ľ�ͷ
            ����¼����֪ͨ�þ�ͷ
�������  : char* strLensId: ��ͷid,
            time_t oldIndexStartTime: ɾ��ǰdb�������Ŀ�ʼʱ�䣬
            time_t newIndexStartTime: ɾ����db����������ʼʱ��
�������  :
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::overwrite_record_notify(
                                                char* strLensId,
                                                time_t newIndexStartTime)
{
    SVS_TRACE();
    // ����У��
    if (NULL == strLensId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Notify camera overwirting record failed."
            "parameter strLensId is NULL."));
        return SVS_RESULT_FAILURE;
    }

    // ���豸
    CAC_Realtime_Record_Device *pDevice = get_device(strLensId);
    if (NULL == pDevice)
    {
        SVS_LOG((SVS_LM_DEBUG,
            "Notify camera overwirting record finished. "
            "The camera is not recording.cameraId[%s].",
            strLensId));
        return SVS_RESULT_OK;
    }

    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pDevice);
    (void)pDevice->overwrite_record_notify( newIndexStartTime);
    return SVS_RESULT_OK;
}

int32_t CAC_RT_Record_Device_Manager::bind2readonlyDeviceMap(CAC_Realtime_Record_Device* pDevice)
{
    if(NULL == pDevice)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Bind a record object for camera to readonly map failed. pDevice[0x%08x]",
            pDevice));
        return SVS_RESULT_FAILURE;
    }

    CAC_Realtime_Record_Device* pTempDev = pDevice;
    // bind��readonly map�У����ʧ�ܲ�Ӱ��ҵ��
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pReadonlyObj, readonly_device_map_mutex_, SVS_RESULT_FAILURE);
    int32_t bindResult = readonly_device_map_.bind(pTempDev->get_frontdev_id(), pTempDev);
    if(0 != bindResult)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Bind a record object for camera to readonly map failed. cameraId[%s]",
            pDevice->get_frontdev_id(), bindResult));
        pTempDev = NULL;
        return SVS_RESULT_FAILURE;
    }

    pTempDev = NULL;
    return SVS_RESULT_OK;
}//lint !e429 already saved in readonly_device_map
//=======================CAC_RT_Record_Device_Manager END==========================

