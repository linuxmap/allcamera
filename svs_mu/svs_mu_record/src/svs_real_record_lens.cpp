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

// 事件类型
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
// 录像类型
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

    //初始化事件类型VECTOR
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

    // ==========远程调试相关参数=========
    request_route_times_        = 0;      // 请求路由次数
    connect_vtdu_times_         = 0;      // 连接VTDU次数
    connect_vtdu_timeout_times_ = 0;      // 连接VTDU超时次数
    recv_media_data_timeout_times_ = 0;


    // ===========事件录像相关参数======
    // 事件前录像秒数
    event_record_before_seconds_ = 0;
    // 事件后录像秒数
    event_record_after_seconds_ = 0;
    // 事件开始时间
    event_record_start_time_ = 0;
    // 事件结束时间
    event_record_end_time_ = 0;

    m_ulIOHandlerIndex = (uint32_t)IO_INVALID_HANDLE;
    // 事件录像最大时长定时器
    event_record_over_timer_id_ = -1;
    // 事件录像结束丢失定时器
    event_record_lost_timer_id_ = -1;
    device_stat_.init(str_frontdev_id_);
    request_route_overtime_len_ = REQUEST_ROUTER_INTERVAL_TIME;
}

CAC_Realtime_Record_Device::~CAC_Realtime_Record_Device()
{
    try
    {
        // 清除以前的录像事件丢失定时器
        (void)kill_event_record_lost_timer();
    }
    catch(...)
    {
    }
}

/*****************************************************************************
 函 数 名  : init
 功能描述  : 初始化镜头信息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回true,失败返回false
 修改历史  :
*****************************************************************************/
bool CAC_Realtime_Record_Device::init()
{
    // 设置新的录像事件丢失处理定时器,解决问题:A/C同时联动B告警
    ACE_Time_Value tv(SVS_EVENT_RECORD_LOST_INTERVAL, 0);
    (void)set_event_record_lost_timer(tv);

    //已经初始化，不用再次初始化
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
 函 数 名  : reset
 功能描述  : 复位镜头信息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::reset()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO,"Begin to reset camera.CameraId[%s].",str_frontdev_id_));

    // 清除以前的录像事件丢失定时器,解决问题:A/C同时联动B告警
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
 函 数 名  : update_record_parameter
 功能描述  : 更新录像参数 事件前录像时间/事件后录像时间
 输入参数  : uint32_t nBeforeSeconds:预录时长
             uint32_t nAfterSeconds:后录时长
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
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
 函 数 名  : send_stop_recv_msg
 功能描述  : 功能:发送停止消息给接收线程与写录像线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::send_stop_recv_msg()
{
    SVS_TRACE();

    // 从媒体处理器中移除注册的句柄
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
 函 数 名  : off_line
 功能描述  : 设备下线处理
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::off_line()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_DEBUG,"Camera off line.CameraId[%s].",str_frontdev_id_));
    // 策略重叠 事件丢失
    (void)clear_event_record_info();

    int32_t stopResult = stop_device();
    return stopResult;
}

/*****************************************************************************
 函 数 名  : stop_device
 功能描述  : 停止设备录像
 输入参数  : bool bSendOverMsg: 是否给调度发送结束路由消息, 默认为发送
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::stop_device( bool bSendOverMsg )
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    SVS_LOG((SVS_LM_INFO, "Start to stop recording.CameraId[%s], state[0x%04x].",
        str_frontdev_id_, state_));

    // 设备设备为休眠状态
    (void)set_status(RECORD_STATE_SLEEP);

    // 如果已经没有事件录像了，则设备设备为停止状态
    if ((0 >= total_event_) && (0 == event_over_time_))
    {
        (void)set_status(FRONT_DEVICE_STATE_STOP);
    }

    // 因为下线，没有媒体流数据报发送过来，需要发送退出消息激活媒体流接
    // 收线程和文件输出线程退出。
    (void)send_stop_recv_msg();

    // 如果需要发送停止录像消息
    if( bSendOverMsg )
    {
        (void)shutdown_router();
    }
    else
    {
        // 手动录像状态通知
        if (RECORDING_ON_MANUAL == record_flag_)
        {
            (void)send_status_notify_msg(STORESVR_MANAUL_RECORD_STATE_STOP,
                                        (uint8_t)(record_flag_ + 1));
        }
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : request_router
 功能描述  : 向调度子系统请求路由，通过StoreSvr发送该路由请求消息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::request_router()
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to request route for realtime recording, cameraId[%s].",
        str_frontdev_id_));
    (void)set_status(RECORD_STATE_REQUEST_ROUTER);
    request_route_overtime_len_ = get_rand_request_router_time();

    // 存储子系统路由请求消息
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

    //发送路由消息
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
 函 数 名  : shutdown_router
 功能描述  : 调度结束请求，通过StoreSvr发送该路由结束消息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
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

    // 因为此时录像标识已经被改变，不判断录像类型是否为手动录像就发送停止手动录像消息
    (void)send_status_notify_msg(  STORESVR_MANAUL_RECORD_STATE_STOP,
                                    (uint8_t)(record_flag_ + 1));

    // 生成结束路由消息
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

    //发送消息
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

    //清除流ID
    stream_id_ = 0;

    return SVS_RESULT_OK;

}

/*****************************************************************************
 函 数 名  : notify_vtdu
 功能描述  : 向VTDU发送注册消息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::notify_vtdu()
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    //从media processor申请io handler
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

    // 如果为非事件录像或者事件录像的开始时间不为0，需要通知IO即时开始记录索引
    // 如果为事件录像并且事件录像开始时间未设置，则在事件发生时才开始记录索引
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

    // 开始录像
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

    // 启动定时器定时检查接收状态
    (void)set_status(RECORD_STATE_CONNECT_VTDU);
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : handle_routerinfo
 功能描述  : 处理分配下来的路由信息
 输入参数  : const void *pbyMsg:路由响应消息内容
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
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

    //已经获得路由,直接返回
    if ((RECORD_STATE_REQUEST_ROUTER != state_)
        && (RECORD_STATE_REQUEST_ROUTER < state_))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Process route for camera failed, The camera already has route"
            "cameraId[%s],routeId[%d].",
            str_frontdev_id_,
            stream_id_));

        //已经获得路由，流ID不可能为 0
        if (0 == stream_id_)
        {
            SVS_LOG((SVS_LM_ERROR,
                "The camera already has route, but route id is 0, cameraId[%s].",
                str_frontdev_id_));
        }

        return SVS_RESULT_FAILURE;
    }

    // 如果申请路由失败
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

    //向VTDU注册
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
    // 如果是请求路由状态，则设置请求路由时长
    if(RECORD_STATE_REQUEST_ROUTER == newStatus)
    {
        request_route_overtime_len_= REQUEST_ROUTER_DELAY_TIME;
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : on_timer
 功能描述  : 定时器回调函数
 输入参数  : int32_t nDummy:定时器事件类型
 输出参数  : NA
 返 回 值  : NA
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::on_timer(int32_t, int32_t, int32_t nDummy)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    // 事件录像结束处理
    if ( TIMER_TYPE_EVENT_RECORD_OVER == nDummy )
    {
        SVS_LOG((SVS_LM_DEBUG, "Camera[%s] on timer, dummy[%d] stat[%d].",
           str_frontdev_id_, nDummy, state_));
        proc_event_record_over_time();
        return SVS_RESULT_OK;
    }
    else if ( TIMER_TYPE_EVENT_RECORD_LOST == nDummy ) // 事件丢失定时器
    {
        SVS_LOG((SVS_LM_DEBUG, "Camera[%s] on timer, dummy[%d] stat[%d].",
           str_frontdev_id_, nDummy, state_));
         return proc_event_record_lost();
    }

    // 镜头状态定时器到时处理
    switch (state_)
    {
        case RECORD_STATE_RECEIVING_FILE:   //正在接收媒体流
            {
                // 如果接收流超时
                recv_stream_on_timer();
            }
            break;

        case RECORD_STATE_SLEEP:          // 等待状态
            break;

        case RECORD_STATE_DISK_FULL:          // 等待状态
            {
                // 检查是否有可用磁盘
                time_t nowTime = ACE_OS::time(NULL);
                if ((SVS_RETRY_SLEEP_LENS_TIME <= nowTime - status_start_time_)
                    && (SVS_RESULT_OK == SVS_Disk_Manager::instance().checkUseableRaid()))
                {
                    (void)set_status(RECORD_STATE_REQUEST_ROUTER);
                }
            }
            break;

        case RECORD_STATE_REQUEST_ROUTER:   //正在请求路由
            {
                request_router_on_timer();  //再次请求
            }
            break;

        case RECORD_STATE_CONNECT_VTDU:     //向VTDU发送媒体注册消息
            {
               conn_vtdu_on_timer();
            }
            break;

        case RECORD_STATE_WAITING_OVER:    // 如果是等待底层关闭结束状态
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

    // 更新录像状态
    device_stat_.updateRecordInfo(record_flag_, state_, stream_id_);
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : recv_stream_on_timer
 功能描述  : 定时器回调，录像状态是接收流状态，判断接流和写录像是否超时
 输入参数  : NA
 输出参数  : NA
 返 回 值  : NA
 修改历史  :
*****************************************************************************/
void CAC_Realtime_Record_Device::recv_stream_on_timer()
{
    SVS_TRACE();
    // 检查数据写入是否正常
    //接收流超时或写录像文件超时,重新请求路由
    int32_t nRet = SVS_Media_Processor::instance().get_handle_status(m_ulIOHandlerIndex);
    if (SVS_RESULT_OK == nRet)
    {
        // 状态正常
        return;
    }

    ++recv_media_data_timeout_times_;

    // 录像状态异常，重新请求路由
    SVS_LOG((SVS_LM_ERROR,
        "camera[%s] check record status invalid, try request route again.",
        str_frontdev_id_));
    (void)shutdown_router(true);
    (void)send_stop_recv_msg();

    if (SVS_ERR_NORAID == nRet)
    {
        // 无磁盘可用，状态转为waiting
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
    // 如果句柄为-1，且没有超时，则不用查询状态
    time_t currTime = time(NULL);
    if(((uint32_t)IO_INVALID_HANDLE == m_ulIOHandlerIndex)
    && (VTDU_SEND_DATA_MIN_TIMEOUT > (currTime - status_start_time_)))
    {
        return;
    }

    // 判断连接VTDU是否超时
    // 如果已经连接成功了，则修改录像状态
    int32_t result = SVS_Media_Processor::instance().get_handle_status(m_ulIOHandlerIndex);
    if (SVS_RESULT_OK == result)
    {
        (void)set_status(RECORD_STATE_RECEIVING_FILE);

        // 如果是手动录像，则发送手动录像通知消息
        if (RECORDING_ON_MANUAL == record_flag_)
        {
            (void)send_status_notify_msg(STORESVR_MANAUL_RECORD_STATE_RUNNING,
                                  (uint8_t)( record_flag_ + 1));
        }

        device_stat_.setRecordStartTime(time(NULL));
        return ;
    }
    else if(SVS_ERR_WAIT_RESP == result ) // 如果正在等待VTDU响应，并且没有超时
    {
        return ;
    }

    // 连接VTDU超时，重新请求路由
    // 如果接收对象为空或者接收对象状态异常，则需要重新请求路由
    SVS_LOG((SVS_LM_ERROR, "Connect with VTDU timeout.cameraId[%s].",
        str_frontdev_id_));

    (void)shutdown_router(true);
    (void)send_stop_recv_msg();

    connect_vtdu_timeout_times_++;

    (void)set_status(RECORD_STATE_REQUEST_ROUTER);
}

void CAC_Realtime_Record_Device::request_router_on_timer()
{
    // 判断改镜头的底层handle是否已经释放，如果还没有释放，则等待
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
    // 如果时间已经超时，则重新申请路由,如果没有超时，则不用理会
    if ((currTime - status_start_time_) < request_route_overtime_len_)
    {
        return ;
    }

    SVS_LOG((SVS_LM_WARNING,
        "Request router timeout,Request router again.cameraId[%s]",
        str_frontdev_id_));

    // 请求路由前，先尝试关闭路由
    if(0 < stream_id_)
    {
        (void)shutdown_router();
    }

    // 请求路由
    if (SVS_RESULT_OK != request_router())
    {
        SVS_LOG((SVS_LM_ERROR, "Request router failed."));
        return ;
    }

    return ;
}



void CAC_Realtime_Record_Device::waiting_over_on_timer()
{
    // 避免查询太频繁，限制至少5s以后再查询一次
    time_t currTime = time(NULL);
    // 如果时间已经超时，则重新申请路由,如果没有超时，则不用理会
    if ((currTime - status_start_time_) < REQUEST_ROUTER_DELAY_TIME)
    {
        return ;
    }

    // 判断改镜头的底层handle是否已经释放，如果还没有释放，则等待
    uint32_t handleIndex = (uint32_t)IO_INVALID_HANDLE;
    int32_t nRet = SVS_Media_Processor::instance().get_handle_close_status(str_frontdev_id_,
                                                                    LENS_TYPE_REALTIME,
                                                                    0, handleIndex);
    // 如果handle已经释放，则重新请求路由
    if(SVS_RESULT_OK == nRet)
    {
        // 请求路由前，先尝试关闭路由
        if(0 < stream_id_)
        {
            (void)shutdown_router();
        }

        // 请求路由
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
 函 数 名  : proc_event_record_over_time
 功能描述  : 事件录像超出最大允许的时长
 输入参数  : NA
 输出参数  : NA
 返 回 值  : NA
 修改历史  :
*****************************************************************************/
void CAC_Realtime_Record_Device::proc_event_record_over_time()
{
    SVS_TRACE();
    device_stat_.updateEventInfo(event_begin_time_, event_over_time_, (uint32_t)total_event_);

    //还有事件，不停止录像
    if ( 0 < total_event_ )
    {
        return;
    }

    //如果还是事件录像，停止更新录像索引
    if ( RECORD_TYPE_EVENT == record_flag_ )
    {
        //更新录像文件索引列表结束时间
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

        //不需要预录，停止录像
        if ( 0 == event_record_before_seconds_)
        {
            (void)stop_device();
        }
    }
    // 如果是禁止录像策略，则停止录像
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
 函 数 名  : proc_event_record_lost
 功能描述  : 录像事件丢失定时器
 输入参数  : NA
 输出参数  : NA
 返 回 值  : NA
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::proc_event_record_lost()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    int32_t totalEvent = total_event_;
    // 遍历所有告警事件,把有效时长减一个周期
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

    // 如果还有事件录像对象，则继续录像
    for (size_t j = 0; j < eventVecSize; j++ )
    {
        if (0 != event_vector_[j].size())
        {
            return SVS_RESULT_OK;
        }
    }
    // 如果最后一批事件到期
    if (0 < totalEvent)
    {
        SVS_LOG((SVS_LM_DEBUG,
            "Alarm event lost timer take effect."
            "cameraId[%s],alarm event total number[%d].",
            str_frontdev_id_,
            total_event_));
        //如果还是事件录像，且最后一个事件录像到期，则停止更新录像索引
        if (RECORD_TYPE_EVENT == record_flag_)
        {
            //更新录像文件索引列表结束时间
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

            //不需要预录，停止录像
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
        // 如果是禁止录像策略，则停止录像
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
 函 数 名  : probid_recording
 功能描述  : 启动禁止录像
 输入参数  : NA
 输出参数  : NA
 返 回 值  : NA
 修改历史  :
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
 函 数 名  : clear_event_record_info
 功能描述  : 清除所有事件录像信息
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功,返回 0
 修改历史  :
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
 函 数 名  : start_record
 功能描述  : 根据设备当前的录像状态，启动或停止录像
 输入参数  : recordType :   进行录像的录像类型
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::start_record(uint8_t recordType)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
        "The camera start recording, cameraId[%s], "
        "new record type flag[%d].",
        str_frontdev_id_,
        recordType));

    //录像类型不正确，直接返回
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

    //录像类型转换
    if(SVS_RESULT_OK != record_conversion(recordType, bStartRecord))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Conversion record type failed, "
            "camera[%s], recordType[%d].",
            str_frontdev_id_,
            recordType));

        return SVS_RESULT_FAILURE;
    }

    //如果新的录像类型是禁止录像
    if(RECORD_TYPE_PROBID == record_flag_)
    {
        return SVS_RESULT_OK;
    }

    //如果是手动录像，发送开始手动录像通知
    if ((RECORD_TYPE_MANUAL == record_flag_)&&(0 != router_id()))
    {
        (void)send_status_notify_msg(STORESVR_MANAUL_RECORD_STATE_RUNNING,
                                    (uint8_t)(recordType + 1));
    }

    // 设置事件录像结束定时器
    time_t eventRecordTime = event_over_time_ - time(NULL);
    if((RECORD_TYPE_EVENT == record_flag_) && (0 < eventRecordTime))
    {
        (void)set_event_record_over_timer(ACE_Time_Value(eventRecordTime));
    }

    //如不是从禁止录像到其它录像，直接返回
    if ( !bStartRecord )
    {
        return 0;
    }

    //发送路由请求消息
    (void)init();

    int32_t reqResult = request_router();

    return reqResult;
}

/*****************************************************************************
 函 数 名  : control_record
 功能描述  : 控制镜头对象进行指定录像策略类型进行录像
 输入参数  : recordFlag :   进行录像的策略类型
             endTime    :   结束时间
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::control_record( uint8_t recordFlag,
                                                   time_t /*endTime*/,
                                                   time_t eventStratEndTime )
{
    SVS_TRACE();

    //当设备没有任何录像策略有效时,将录像状态清除
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
        case FROBID_RECORDING:      //禁止录像
            result = probid_recording();
            break;

        case RECORDING_ON_EVENT:    //事件录像
            result = start_record(RECORD_TYPE_EVENT);
            break;

        case ALWAYS_RECORDING:      //连续录像
            result = start_record(RECORD_TYPE_ALWAYS);
            break;

        case RECORDING_ON_MANUAL:   //手动录像
            result = start_record(RECORD_TYPE_MANUAL);
            break;
        default :
            //没有录像策略时间段作为禁止录像
            result = probid_recording();
            break;
    };

    updateRecordTaskStartTime(oldRecordFlag,record_flag_, oldEventNum);


    return result;
}

/*****************************************************************************
 函 数 名  : event_happen
 功能描述  : 录像告警事件发生处理
 输入参数  : const char *strAlarmDeviceId:告警触发设备
             uint32_t eventType    :   事件类型
             time_t endTime :事件预计结束时间
             bool bIsRecord:标识该事件发生时，是否真正开始录像
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::event_happen(   const char *strAlarmDeviceId,
                                                uint32_t eventType,
                                                time_t endTime,
                                                bool bIsRecord)
{
    SVS_TRACE();
    //判事件类型是否正确,不正确直接返回
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

    // 如果该设备已产生了该类事件的事件录像,则直接返回
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

        //更新事件录像最大有效时长
        event_vector_[eventType][strAlarmDeviceId] =
            (int32_t)CAC_RT_Record_Server::instance()->event_record_lost_timeout();

        SVS_LOG((SVS_LM_INFO,
            "Update recording time length of this alarm event type to [%d]s, cameraId[%s].",
            event_vector_[eventType][strAlarmDeviceId],
            str_frontdev_id_));

        return SVS_RESULT_FAILURE;
    }

    //给该事件类型的该告警设备置标志
    event_vector_[eventType][strAlarmDeviceId] =
        (int32_t)CAC_RT_Record_Server::instance()->event_record_lost_timeout();

    SVS_LOG((SVS_LM_INFO,
        "Recording time length of this alarm event type is [%d]s, cameraId[%s].",
        event_vector_[eventType][strAlarmDeviceId],
        str_frontdev_id_));

    //事件总数加1
    total_event_++;

    // 如果还没有开始录像，并且需要录像，则开始事件录像
    if (RECORD_TYPE_PROBID == record_flag_ && bIsRecord)
    {
        SVS_LOG((SVS_LM_INFO,
            "Start alarm event recording, cameraId[%s].",
            str_frontdev_id_));

        //设置事件开始时间
        event_begin_time_ = time(NULL);

        //取消以前设置的停止事件录像定时器
        (void)kill_event_record_over_timer();

        (void)control_record( RECORDING_ON_EVENT, endTime ,1);
        total_event_ = 1;

        device_stat_.setRecordTaskStartTime(time(NULL));
    }
    else
    {
        //已经开始录像，但事件还未开始时，需要通知Egress开始记录索引
        if (0 == event_begin_time_)
        {
            event_begin_time_ = time(NULL) - event_record_before_seconds_;

            //取消以前设置的停止事件录像定时器
            (void)kill_event_record_over_timer();

            // 如果已经在记录索引，则不用再通知Egress开始记录索引
            if(0 == record_start_time_)
            {
                // 通知Egress开始记录索引
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
 函 数 名  : event_over
 功能描述  : 告警结束事件处理
 输入参数  : const char *strAlarmDeviceId:告警触发设备
             uint32_t eventType:告警事件类型
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::event_over( const char *strAlarmDeviceId,
                                            uint32_t eventType)
{
    SVS_TRACE();
    //判事件类型是否正确
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

    //查找指定告警设备的录像事件列表，如没有返回
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

    // 如果响应的告警事件不存在，返回
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
    // 避免后录断录像，状态上报不准问题
    device_stat_.updateEventInfo(event_begin_time_, event_over_time_, (uint32_t)total_event_);

    total_event_--;

    // 所有类型的事件数为0是才可以停止录像
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

    //获取事件结束时间
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

    //设置事件录像结束处理定时器
    (void)set_event_record_over_timer( ACE_Time_Value(event_record_after_seconds_) );

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : debug_get_device_status
 功能描述  : 获取该录像设备的状态，由INT型转换成string型，telent调试用
 输入参数  : NA
 输出参数  : string &strDeviceStatus:录像设备状态string表示
 返 回 值  : NA
 修改历史  :
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
 函 数 名  : debug_get_record_flag
 功能描述  : 获取该录像设备的录像类型，由INT型转换成string型，telent调试用
 输入参数  : NA
 输出参数  : string &strRecordFlag:录像类型状态string表示
 返 回 值  : NA
 修改历史  :
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

//1 TODO实现镜头详细调试信息
/*****************************************************************************
 函 数 名  : debug_get_device_info
 功能描述  : 获取录象设备的详细信息
 输入参数  : uint32_t szDeviceInfo:缓存区长度
 输出参数  : char *strDeviceInfo:telnet结束缓存区
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::debug_get_device_info(
    char *strDeviceInfo,
    uint32_t ulBuffLen )
{
    SVS_TRACE();
    string strRecordFlag;
    string strDeviceStatus;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, obj, mutex_, false);
    //录像类型
    debug_get_record_flag(strRecordFlag);

    //设备状态
    debug_get_device_status(strDeviceStatus);

    char strTime[SVS_STR_TIME_MAX_LEN];

    //事件开始时间
    (void)time2string(strTime, sizeof(strTime), event_begin_time_);
    string strEventBeginTime = strTime;

    //事件结束时间
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
                      event_record_before_seconds_, //事件前秒数
                      event_record_after_seconds_,
                      strEventBeginTime.c_str(), event_begin_time_,
                      strEventOverTime.c_str(),  event_over_time_,
                      stream_id_,
                      total_event_, //事件总数
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
 函 数 名  : stop_record
 功能描述  : 停止录像, 置设备状态为请求路由状态(不发送路由通知消息)
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::stop_record()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO,
        "Camera begin to release route.cameraId[%s],reoute id[%u],current state[%d].",
        str_frontdev_id_,
        stream_id_,
        state_ ));

    //状态不为接收流或连接VTDU状态,直接返回
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

    // 发送手动录像状态通知消息
    if (RECORDING_ON_MANUAL == record_flag_)
    {
        (void)send_status_notify_msg(STORESVR_MANAUL_RECORD_STATE_STOP,
                                    (uint8_t)(record_flag_ + 1));
    }

    //设置设备的状态为请求路由状态
    (void)set_status(RECORD_STATE_REQUEST_ROUTER);

    //发送退出消息激活媒体流接收线程和文件输出线程退出。
    (void)send_stop_recv_msg();

    return 0;
}


/*****************************************************************************
 函 数 名  : send_status_notify_msg
 功能描述  : 发送手动录像状态通知消息给录像管理服务器
 输入参数  : const int32_t recordStatus:录像状态
             uint8_t recordType:录像类型，保留参数接口
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
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

    // 录像状态通知消息,每次只有一个录像设备
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

    //发送消息
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
函 数 名  : kill_event_record_over_timer
功能描述  : 取消事件结束处理定时器
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
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
函 数 名  : set_event_over_timer
功能描述  : 设置事件结束处理定时器
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
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

    // 释放旧的定时器
    if ( 0 <= event_record_over_timer_id_ )
    {
        (void)CAC_Timer_Manager::instance()->free_timer(event_record_over_timer_id_);
        event_record_over_timer_id_= -1;
    }

    // 如果延时为0，则直接创建定时器任务
    if(0 == delayTime.sec())
    {
        CAC_RT_Record_Device_Manager::timer_callback(CAC_RT_Record_Device_Manager::instance(),
                                                                 (int32_t)internal_id_,
                                                                 -1, // 不关注timerid，可以为-1
                                                                 TIMER_TYPE_EVENT_RECORD_OVER);
        SVS_LOG((SVS_LM_INFO,
            "The alarm event over delay time is 0."
            "Create timer task for alarm event overing succeeded."
            "cameraId[%s].",
            str_frontdev_id_));
    }
    else
    {
        // 一次性定时器周期为0
        event_record_over_timer_id_ = CAC_Timer_Manager::instance()->alloc_timer(
                        CAC_RT_Record_Device_Manager::instance(),
                        CAC_RT_Record_Device_Manager::timer_callback,
                        (int32_t)internal_id_,
                        delayTime,
                        ACE_Time_Value::zero,
                        TIMER_TYPE_EVENT_RECORD_OVER);

        // 如果申请定时器失败
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
函 数 名  : kill_event_record_lost_timer
功能描述  : 取消录像事件丢失处理定时器
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
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
函 数 名  : set_event_record_lost_timer
功能描述  : 设置录像事件丢失处理定时器
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
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

        // 释放旧的定时器
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
函 数 名  : refresh_event_record_outtime
功能描述  : 事件丢失后最大录像时长配置动态更新通知接口
输入参数  : int32_t shiftValue:相对原来新值的偏移量，可正，可负
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::refresh_event_record_outtime(int32_t shiftValue)
{
    SVS_TRACE();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    // 遍历所有告警事件,把有效时长增加上偏移量
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
函 数 名  : record_conversion
功能描述  : 录像类型转换
输入参数  : uint8_t newRecordFlag:新的录像类型标识
输出参数  : bool &bStartRecord:是否需要重新启动录像，禁止录像转换成其它录像时需要
            启动录像
返 回 值  : 成功返回0;失败返回-1
修改历史  :
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
        case RECORD_TYPE_PROBID:    //禁止录像
        {
            //由禁止录像状态到事件录像状态
            if ((RECORD_TYPE_EVENT == newRecordFlag) && (!is_event_record_effective()))
            {
                return SVS_RESULT_OK;
            }

            //之前处于禁止录像状态, 启动录像
            bStartRecord        = true;

            if (RECORD_STATE_SLEEP == state_)
            {
                (void)set_status(RECORD_STATE_REQUEST_ROUTER);
            }
        }
        break;

        case RECORD_TYPE_ALWAYS:    //连续录像
        {
            //由连续录像状态到事件录像状态
            if ((RECORD_TYPE_EVENT == newRecordFlag) && (!is_event_record_effective()))
            {
                return probid_recording();
            }

            always_record_over(newRecordFlag);
        }
        break;

        case RECORD_TYPE_EVENT:     //事件录像
        {
            //由事件录像状态到事件录像状态
            if ((RECORD_TYPE_EVENT == newRecordFlag) && (!is_event_record_effective()))
            {
                return probid_recording();
            }

            // 如果是预录到连续录像或手动录像状态
            prerecord_over(newRecordFlag);
        }
        break;

        case RECORD_TYPE_MANUAL:    //手动录像
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
函 数 名  : prerecord_over
功能描述  : 如果是预录到手动录像或连续录像，则通知egress插入索引记录
输入参数  : const uint8_t newRecordFlag:新的录像类型标识
输出参数  : NA
返 回 值  : NA
修改历史  :
*****************************************************************************/
void CAC_Realtime_Record_Device::prerecord_over(uint8_t newRecordFlag)
{
    // 如果是预录到连续录像或手动录像状态,并且还没有开始记录索引
    if ((0 != event_record_before_seconds_) && (0 == record_start_time_))
    {
        if((RECORD_TYPE_ALWAYS == newRecordFlag)
        || (RECORD_TYPE_MANUAL == newRecordFlag))
        {
            // 通知Egress开始记录索引
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
函 数 名  : manual_record_over
功能描述  : 手动录像结束时的处理
输入参数  : const uint8_t newRecordFlag:新的录像类型标识
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_Realtime_Record_Device::manual_record_over(const uint8_t newRecordFlag)
{
    SVS_TRACE();
    switch ( newRecordFlag )
    {
        // 如果策略由[手动录像]转向[非手动录像]，发送手动录像停止消息
        case RECORD_TYPE_PROBID:
        case RECORD_TYPE_ALWAYS:
            // 如果策略由[手动录像]转向[非手动录像]，发送手动录像停止消息
            (void)send_status_notify_msg(
                        STORESVR_MANAUL_RECORD_STATE_STOP,
                        (uint8_t)(newRecordFlag + 1));
            break;
        case RECORD_TYPE_EVENT:
            //由手动录像状态到事件录像状态，如果预录时间为0，停止录像
            if (!is_event_record_effective())
            {
                return probid_recording();
            }

            // 如果策略由[手动录像]转向[非手动录像]，发送手动录像停止消息
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

// 如果是连续录像或手动录像到预录，则通知索引停止
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

    // 如果策略类型没有变化，则不用更新
    if(oldRecordFlag == newRecordFlag)
    {
        return ;
    }

    // 如果刚刚是禁止录像策略,到需要启动录像的类型，则更新task
    if(FROBID_RECORDING == oldRecordFlag)
    {
        if((ALWAYS_RECORDING == newRecordFlag) || (RECORDING_ON_MANUAL == newRecordFlag))
        {
            device_stat_.setRecordTaskStartTime(ulRecordTaskStartTime);
        }
        // 如果转换到事件录像，且事件个数大于0，则更新task
        else if((ALWAYS_RECORDING == newRecordFlag) && 0 < total_event_)
        {
            device_stat_.setRecordTaskStartTime(ulRecordTaskStartTime);
        }
    }
    // 如果刚刚是事件录像策略，且没有录像任务，现在需要启动录像，则更新task
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
函 数 名  : init
功能描述  : 初始化录像对象，一次性分配，服务结束时释放
输入参数  : uint32_t nMaxLens:系统支持的最大镜头数
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
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

        *((uint32_t *)pch) = lensIndex;   //保存索引
        (void)free_list_.insert_tail(pch);

        //为什么要在已经使用的里面push空对象
        using_vector_.push_back(NULL);
    }

    //申请缓冲失败，清空以前申请的缓冲区
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
        //while循环中已经调用SVS_DELETE释放了，这里直接屏蔽lint告警
        return false;    //lint !e429
    }

    SVS_LOG((SVS_LM_INFO,
        "Initialize the camera manager succeeded, "
        "Maximal support camera number is [%u].",
        nMaxLens));

    //初始化成功，申请的空间放入链表，应用退出时才释放，这里直接屏蔽lint告警
    return true;        //lint !e429
}

/*****************************************************************************
函 数 名  : add_device
功能描述  : 添加录像镜头对象.
输入参数  : const char * strLensId:镜头ID
            uint8_t encoderType:镜头编码类型
输出参数  : CAC_Realtime_Record_Device *&pDevice:镜头对象指针地址
返 回 值  : 成功返回0;失败返回-1
修改历史  :
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
        // SVS_DELETE catch,导致重复catch，可以关闭
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
            // 屏蔽pclint告警，是placement new，内存需要放回队列，没有内存泄漏
            pTmpDevice = NULL;//lint !e423

            *((uint32_t *)(void*)pch) = nIndex;
            (void)free_list_.insert_tail(pch);

            return SVS_RESPCODE_OTHER_ERROR;
        }

        // 把对象bind到readonly device map中，如果失败，不影响业务
        (void)bind2readonlyDeviceMap(pTmpDevice);

        // 没有第一次绑定镜头对象，规避日志和ACE问题，恢复error code
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
函 数 名  : get_device
功能描述  : 根据指定的设备ID字串查找设备对象
输入参数  : const char *strFrontDevId : 要查找的设备ID
输出参数  : CAC_Realtime_Record_Device *&pDevice:镜头对象指针地址
返 回 值  : 成功返回查找到的设备对象指针, 失败返回NULL
修改历史  :
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

    //引用计数加1
    int32_t increaseResult = pDevice->increase_reference();
    if( SVS_RESULT_FAILURE == increaseResult )
    {
        pDevice = NULL;
        return NULL;
    }

    return pDevice;

}

/*****************************************************************************
函 数 名  : get_device
功能描述  : 根据指定的设备ID字串查找设备对象
输入参数  : uint32_t nId:根据内部ID
输出参数  : CAC_Realtime_Record_Device *&pDevice:镜头对象指针地址
返 回 值  : 成功返回查找到的设备对象指针, 失败返回NULL
修改历史  :
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

    //引用计数加1
    int32_t increaseResult = pDevice->increase_reference();
    //计数加1失败,返回NULL
    if( SVS_RESULT_FAILURE == increaseResult )
    {
        pDevice = NULL;
        return NULL;
    }

    return pDevice;
}

/*****************************************************************************
函 数 名  : control_record
功能描述  : 控制录像
输入参数  : const char * const strLensId:镜头ID
            uint8_t encoderType:镜头编码类型
            uint8_t recordFlag:镜头录像类型
            time_t endTime:策略结束时间
            uint32_t eventBeforeSeconds:预录时长
            uint32_t eventAfterSeconds:后录时长
            time_t eventStratEndTime:录像结束时间
输出参数  : NA
返 回 值  : 成功返回0,失败返回-1
修改历史  :
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

    //查找录像设备对象
    CAC_Realtime_Record_Device *pLens = get_device(strLensId);

    if (NULL == pLens)
    {
        //如果是禁止录像直接返回
        if (FROBID_RECORDING == recordFlag)
        {
            SVS_LOG((SVS_LM_INFO,
                "Start recording finished, "
                "The camera's record type is forbid record, cameraId[%s].",
                strLensId ));

            return 0;
        }

        //如果是事件录像，且预录像时间为0，直接返回
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

        //创建录像设备
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

    //更新录像参数
    (void)pLens->update_record_parameter(eventBeforeSeconds, eventAfterSeconds);

    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pLens);

    int32_t recordResult = pLens->control_record(recordFlag, endTime, eventStratEndTime);

    return recordResult;
}

/*****************************************************************************
函 数 名  : event_happen
功能描述  : 事件录像处理
输入参数  : const char * const strLensId:镜头ID
            const char * strAlarmDeviceId:告警镜头ID
            uint32_t eventType:事件类型
            uint8_t encoderType:编码器类型
            time_t endTime:策略结束事件
            uint32_t eventBeforeSeconds:预录时长
            uint32_t eventAfterSeconds:后录时长
            bool bIsRecord:事件发生时，是否真正需要录像
输出参数  : NA
返 回 值  : 成功返回0,失败返回-1
修改历史  :
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

    //查找进行事件录像设备对象
    CAC_Realtime_Record_Device *pLens = get_device(strLensId);

    if (NULL == pLens )
    {
        //创建录像设备
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
函 数 名  : event_over
功能描述  : 事件结束处理
输入参数  : const char * const strLensId:镜头ID
            const char * strAlarmDeviceId:告警镜头ID
            uint32_t eventType:事件类型
输出参数  : NA
返 回 值  : 成功返回0,失败返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::event_over(
    const char * const strLensId,
    const char * strAlarmDeviceId,
    uint32_t eventType )
{
    SVS_TRACE();
    //查找进行事件录像设备对象
    CAC_Realtime_Record_Device *pLens = get_device(strLensId);
    if (NULL == pLens)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Process alarm event overing failed.Camera is unfound."
            "cameraId[%s].",
            strLensId));
        return SVS_RESULT_FAILURE;
    }

    // 找到镜头后，通知镜头告警结束事件
    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pLens);
    int32_t eventProcResult = pLens->event_over(strAlarmDeviceId, eventType);

    return eventProcResult;
}

/*****************************************************************************
函 数 名  : handle_routerinfo
功能描述  : 路由消息处理
输入参数  : const ACE_Message_Block *mb:路由消息内容
输出参数  : NA
返 回 值  : 成功返回0,失败返回-1
修改历史  :
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

    //申请消息缓冲区
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

    CAC_Realtime_Record_Device *pFrontDev = get_device(strDevId); //查找设备

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

        if (0 != pReq->RespCode) //分配路由失败
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

        //路由分配成功，处理路由消息
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
函 数 名  : shutdown_router
功能描述  : 停止所有的设备路由
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0,失败返回-1
修改历史  :
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
函 数 名  : stop_device_record
功能描述  : 停止指定设备的录像
输入参数  : const char * const strDevId : 要停止录像的设备ＩＤ
            bool bSendOverMsg ：发送结束路由消息标志，默认为true(发送)
输出参数  : NA
返 回 值  : 成功返回0,失败返回-1
修改历史  :
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
函 数 名  : stop_device_record
功能描述  : 设备下线处理
输入参数  : const char * const strDevId :设备ID
输出参数  : NA
返 回 值  : 成功返回0,失败返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::device_off_line(const char * const strDevId)
{
    SVS_TRACE();
    // 找镜头是否在录像
    CAC_Realtime_Record_Device *pDevice = get_device(strDevId);

    // 如果没有找到镜头
    if (NULL == pDevice)
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Process camera offline failed.CameraId is unfound.cameraId[%s].",
            strDevId ) );
        return SVS_RESPCODE_OTHER_ERROR;
    }

    // 找到镜头后，通知镜头下线
    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pDevice);
    int32_t offLineResult = pDevice->off_line();

    // 镜头下线成功后从map表中删除
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
函 数 名  : on_timer
功能描述  : 定时器回调函数
输入参数  : int32_t eventId:事件ID
            int32_t timerId:定时器ID
            int32_t nDummy:定时器事件类型
输出参数  : NA
返 回 值  : NA
修改历史  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::on_timer(int32_t eventId, int32_t timerId, int32_t nDummy )
{
    SVS_TRACE();

    // 如果是录像状态检测定时器，则遍历所有的正在录像的镜头，检测其状态
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

    // 找具体的录像设备并通知其定时器事件
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
函 数 名  : decrease_reference
功能描述  : 引用计数自动减
输入参数  : CAC_Realtime_Record_Device *pDevice:录像设备对象的指针
输出参数  : NA
返 回 值  : 成功返回0,失败返回-1
修改历史  :
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
            // unbind from readonly map中，如果失败不影响业务
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
函 数 名  : debug_get_device_info
功能描述  : telnet获取设备详细信息
输入参数  : const char *strFrontDevId:镜头ID
            uint32_t szDeviceInfo :缓存区长度
输出参数  : char *strDeviceInfo:telnet结果缓存区
返 回 值  : 成功返回0,失败返回-1
修改历史  :
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
函 数 名  : debug_get_all_device_info
功能描述  : 取所有设备的基本信息
输入参数  : uint32_t szDeviceInfo :缓存区长度
输出参数  : char *strDeviceInfo:telnet结果缓存区
返 回 值  : 成功返回0,失败返回-1
修改历史  :
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

    int32_t probidDeviceNum = 0;    //禁止录像设备个数
    int32_t alwaysDeviceNum = 0;    //连续录像设备个数
    int32_t eventDeviceNum = 0;     //事件录像设备个数
    int32_t manualDeviceNum = 0;    // 手动录像设备个数

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

        //缓冲区已用完，退出循环
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

        //读取录像标志的设备个数
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

        //读取设备状态设备个数
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
函 数 名  : debug_get_all_device_detail_info
功能描述  : 取所有设备的详细信息
输入参数  : uint32_t szDeviceInfo :缓存区长度
输出参数  : char *strDeviceInfo:telnet结果缓存区
返 回 值  : 成功返回0,失败返回-1
修改历史  :
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

        //缓冲区已用完，退出循环
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
函 数 名  : count_dev_num_by_status
功能描述  :
输入参数  : CAC_Realtime_Record_Device* pDevice:当先需要解析的设备对象指针
输出参数  : SVS_STATUS_DEV_NUM_T & devStateNum:累计结果
返 回 值  : 成功返回0;失败返回-1
修改历史  :
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
函 数 名  : get_device_by_routerId
功能描述  : 根据指定的路由ID查找设备对象
输入参数  : uint32_t RouterId : 要查找的路由ID
输出参数  : NA
返 回 值  : 成功返回查找到的设备对象,失败返回NULL
修改历史  :
*****************************************************************************/
CAC_Realtime_Record_Device *
CAC_RT_Record_Device_Manager::get_device_by_routerId( uint32_t RouterId )
{
    SVS_TRACE();
    CAC_Realtime_Record_Device *pDevice = NULL;
    CAC_Realtime_Record_Device *pDeviceTmp = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);

    // 根据路由ID查找设备
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

    //没有找到,返回NULL
    if( NULL == pDevice )
    {
        return NULL;
    }

    //查找成功,引用计数加1后,返回该对象指针
    int32_t increaseResult = pDevice->increase_reference();
    if( SVS_RESULT_FAILURE == increaseResult )
    {
        pDevice = NULL;
        return NULL;
    }

    return pDevice;

}

/*****************************************************************************
函 数 名  : shutdown_router
功能描述  : 关闭指定路由的录像路由
输入参数  : const uint32_t RouterID : 要关闭的路由ID
输出参数  : NA
返 回 值  : 成功:0 失败:-1
修改历史  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::shutdown_router( const uint32_t RouterID )
{
    SVS_TRACE();
    //查找指定路由的设备
    CAC_Realtime_Record_Device *pDevice = get_device_by_routerId( RouterID );
    if( NULL == pDevice )
    {
        SVS_LOG( ( SVS_LM_WARNING,
            "Shutdown route failed.The route id is unfound.routeId[%u].",
            RouterID ) );
        return SVS_RESULT_FAILURE;
    }

    //停止录像
    SVS_RT_RECORD_DEVICE_AUTO_PTR ptr(this, pDevice);
    int32_t stopRecordResult = pDevice->stop_record();

    return stopRecordResult;
}

/*****************************************************************************
函 数 名  : timer_callback
功能描述  : 定时器回调函数
输入参数  : const uint32_t RouterID : 要关闭的路由ID
输出参数  : NA
返 回 值  : 成功:0 失败:-1
修改历史  :
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
函 数 名  : build_update_record_index_sql
功能描述  : 根据传入的录像索引消息生成SQL语句
输入参数  : mb :        录像索引消息
            sqlBuff:    SQL语句缓冲区
            sqlBuffSize: SQL语句缓冲区大小
输出参数  : sqlBuff:    SQL语句缓冲区
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::build_update_record_index_sql(
    const ACE_Message_Block* mb,
    char *sqlBuff,
    uint32_t sqlBuffSize) const
{
    SVS_TRACE();
    // 参数校验
    if (( NULL == mb ) || ( NULL == sqlBuff ))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Build update record index sql failed.The parameter is invalid."));
        return SVS_RESULT_FAILURE;
    }

    const Update_Record_Index *pIndex = (Update_Record_Index *)(void*)mb->rd_ptr();

    switch ( pIndex->Type )
    {
        // 如果是更新类型
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

        // 如果是插入类型
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

        // 如果是覆盖类型
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
函 数 名  : handle_record_index
功能描述  : 根据传入的录像索引消息列表更新数据库
输入参数  : recordIndexList :   录像索引消息列表
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
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
//            //如果是更新类型
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
//            //如果是插入类型
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
//            //如果是覆盖类型
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
函 数 名  : refresh_event_record_outtime
功能描述  : 事件丢失最大录像时长动态刷新
输入参数  : int32_t shiftValue:偏移时长
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::refresh_event_record_outtime(
                                                             int32_t shiftValue)
{
    SVS_TRACE();
    CAC_Realtime_Record_Device *pDev = NULL;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);
    SVS_DEVICE_MAP_ITER iter(device_map_);
    SVS_DEVICE_MAP_ENTRY *pEntry = NULL;
    //遍历所有录像对象
    for (; 0 != iter.next(pEntry); (void)iter.advance() )
    {
        pDev = pEntry->int_id_;
        (void)pDev->refresh_event_record_outtime(shiftValue);
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : overwrite_record_notify
功能描述  : 当磁盘满覆盖写的时候，选择删除系统中最早的录像，如果被删除录像的镜头
            正在录像，则通知该镜头
输入参数  : char* strLensId: 镜头id,
            time_t oldIndexStartTime: 删除前db中索引的开始时间，
            time_t newIndexStartTime: 删除后db中索引的起始时间
输出参数  :
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_RT_Record_Device_Manager::overwrite_record_notify(
                                                char* strLensId,
                                                time_t newIndexStartTime)
{
    SVS_TRACE();
    // 参数校验
    if (NULL == strLensId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Notify camera overwirting record failed."
            "parameter strLensId is NULL."));
        return SVS_RESULT_FAILURE;
    }

    // 找设备
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
    // bind到readonly map中，如果失败不影响业务
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

