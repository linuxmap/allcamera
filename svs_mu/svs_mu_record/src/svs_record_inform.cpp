#include "vms/vms.h"
#include "svs_config.h"
#include "svs_timer.h"
#include "svs_auto_reference_ptr.h"
#include "svs_log_msg.h"

#include "svs_stat_manager.h"
#include "svs_real_record_task.h"
#include "svs_record_inform.h"
#include "svs_record_index.h"
#include "svs_real_record_lens.h"
#include "svs_real_record_server.h"
#include "svs_rt_record_common.h"
#include "svs_mb_buffer.h"
#include "svs_daemon_thread.h"
#include "svs_bakup_front_record_lens.h"

using namespace SVS_SS_UTILITIES;

//===========================CAC_Record_Inform BEGIN===========================
CAC_Record_Inform::CAC_Record_Inform(
    const char *               strFrontDevId,
    uint8_t              encoderType,
    CAC_Record_Inform_Manager *pManager )
    : CAC_Record_Front_Device(strFrontDevId), encoder_type_(encoderType), strat_list_(STRATEGY_LIST_DEFAULT_SIZE)
{
    referenc_count_ = 1;
    record_flags_ = RECORDING_NONE;
    p_manager_ = pManager;
    state_ = FRONT_DEVICE_STATE_INIT;
    timer_id_ = -1;

    //默认不进行事件录像预录和后录
    event_before_record_time_ = 0;
    event_after_record_time_ = 0;
    inform_stat_.init(str_frontdev_id_);
    //初始化事件类型VECTOR
    EVENTMAP emap;
    for (uint32_t i = 0; i < RECORD_EVENT_TYPE_MAX; i++)
    {
        event_vector_.push_back(emap);
    }
}

CAC_Record_Inform::~CAC_Record_Inform()
{
    SVS_LOG((SVS_LM_DEBUG,
        "Begin to clear alarm event list for inform object.CameraId[%s].",
        str_frontdev_id_));
    ACE_GUARD(ACE_Recursive_Thread_Mutex, pObj, mutex_);
    for (size_t i = 0; i < event_vector_.size() && i < RECORD_EVENT_TYPE_MAX; i++)
    {
        event_vector_[i].clear();
    }

    event_vector_.clear();

    p_manager_ = NULL;
}

/*****************************************************************************
函 数 名  : init_strate
功能描述  : 初始化镜头的策略列表
输入参数  : STRAT_VECTOR& stratVector:镜头策略列表
输出参数  : 无
返 回 值  : 成功: true 失败:false
修改历史  :
*****************************************************************************/
bool CAC_Record_Inform::init_strate(const STRAT_VECTOR& stratVector)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to initialize camera record strategy list, camera[%s].",
        str_frontdev_id_));

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, false);

    record_flags_ = RECORDING_NONE;

    //更新录像策略列表
    (void)strat_list_.update_strategy_vector(stratVector);

    //查找最先将要执行的录像策略
    (void)strat_list_.search_earliest_time();

    state_ = FRONT_DEVICE_STATE_INFORMING;

    uint8_t flags = 0;

    record_flags_ = strat_list_.record_type(flags);

    return true;
}
/*****************************************************************************
 函 数 名  : exec_strategy
 功能描述  : 执行镜头的录像策略
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功返回 0
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform::exec_strategy()
{
    SVS_TRACE();

    //获取策略已经生效的时间长度
    time_t tlong = strat_list_.howlong();
    time_t endTime = strat_list_.end_time();

    SVS_LOG((SVS_LM_INFO, "Begin to execute record strategy, "
        "camera[%s], strategy effective time[%d].",
        str_frontdev_id_, tlong));


    if (0 >= tlong)
    {
        int32_t ret = SVS_RESULT_OK;

        // 查找最早执行的录像策略
        {
            ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
            ret = strat_list_.search_earliest_time();
        }

        // 1.当前没有生效的策略，今天也没有生效的策略了，需要设置今天结束时间点的定时器
        // 如果当前没有生效的策略，则设置定时器为今天结束
        if (SVS_RESULT_OK != ret)
        {
            record_flags_ = RECORDING_NONE;

            SVS_LOG((SVS_LM_WARNING,
                "Camera does not have recording strategy, "
                "Wait for tomorrow record, camera[%s].",
                str_frontdev_id_));

            time_t currTime =  time(NULL);

            time_t todayEnd = SECONDS_PER_DAY - (currTime % SECONDS_PER_DAY);
            ACE_Time_Value delay(todayEnd, 0);
            ACE_Time_Value intervalTime(SVS_TIMER_INTERVAL_TIMEOUT, 0);
            if (SVS_RESULT_FAILURE == set_timer(state_, delay, intervalTime))
            {
                SVS_LOG((SVS_LM_WARNING,
                    "Fail to set record timer, camera[%s].",
                    str_frontdev_id_));
            }
        }
        else
        {
            // 1.当前策略已经生效，需要立即执行策略，同时需要设置下一个策略变更时间点
            ACE_Time_Value delay(endTime, 0);
            ACE_Time_Value intervalTime(SVS_TIMER_INTERVAL_TIMEOUT, 0);
            if (SVS_RESULT_FAILURE == set_timer(state_, delay, intervalTime))
            {
                SVS_LOG((SVS_LM_WARNING, "Fail to set record timer, camera[%s].",
                    str_frontdev_id_));
            }
        }
    }
    else
    {
        // 1.当前策略还未生效，需要设置下一个策略变更时间点
        ACE_Time_Value delay(endTime, 0);
        ACE_Time_Value intervalTime(SVS_TIMER_INTERVAL_TIMEOUT, 0);
        if (SVS_RESULT_FAILURE == set_timer(state_, delay, intervalTime))
        {
            SVS_LOG((SVS_LM_WARNING, "Fail to set record timer, camera[%s].",
                str_frontdev_id_));
        }
    }


    (void)strat_list_.print_selected_strategy(str_frontdev_id_);

    //发送录像开始通知
    (void)inform_recording();

    //统计
    time_t startTime = time(NULL);
    bool isEventEffective = (strat_list_.get_event_strategy_time() > 0);

    inform_stat_.updateStrategyTime(startTime, endTime, record_flags_, isEventEffective);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : set_timer
 功能描述  : 设置定时器
 输入参数  : uint8_t byState:镜头新的状态
             const ACE_Time_Value &delayTime:定时器生效的延时时间
             const ACE_Time_Value &intervalTime:定时器的周期时间
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform::set_timer(uint8_t         byState,
                                  const ACE_Time_Value &delayTime,
                                  const ACE_Time_Value &intervalTime )
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, -1);

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to set timer, "
        "camera[%s], current status[0x%04x], new status[0x%04x], delay time[%d], interval time[%d].",
        str_frontdev_id_,
        state_,
        byState,
        delayTime.sec(),
        intervalTime.sec()));

    {
        int32_t nId = timer_id_;
        timer_id_ = CAC_Timer::TIMER_VALUE_SETTING;

        // 如果镜头没有停止录像，则更新器状态
        if (FRONT_DEVICE_STATE_STOP != state_)
        {
            state_ = byState;
        }

        // 如果镜头原来已经设定了定时器，则删除原来的定时器
        if (0 <= nId)
        {
            SVS_LOG((SVS_LM_DEBUG,
                "Kill old timer. camera[%s], old timer[%d].",
                str_frontdev_id_,
                nId));

            (void)p_manager_->kill_timer(nId);
        }

        // 根据新的参数重新生产定时器，并设置定时时间
        timer_id_ = p_manager_->set_timer(internal_id_, delayTime, intervalTime);
        if (0 > timer_id_)
        {
            SVS_LOG((SVS_LM_ERROR, "Fail to set timer, camera[%s].",
                str_frontdev_id_));

            return -1;
        }
        else
        {
            SVS_LOG((SVS_LM_DEBUG,
                "Success to set timer, timer[%d], camera[%s].",
                timer_id_,
                str_frontdev_id_));
        }
    }

    return 1;
}

/*****************************************************************************
 函 数 名  : kill_timer
 功能描述  : 删除定时器
 输入参数  : uint8_t byState:镜头新的状态
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform::kill_timer(uint8_t byState)
{
    SVS_TRACE();

    int32_t timerId = -1;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    SVS_LOG((SVS_LM_DEBUG,
        "Begin to kill timer and change its status. "
        "camera[%s], ""timer[%d=>%d], status[0x%04x=>0x%04x].",
        str_frontdev_id_,
        timer_id_,
        -1,
        state_,
        byState));

    timerId   = timer_id_;
    timer_id_ = -1;

    // 更新镜头的状态
    state_ = byState;

    // 如果镜头对应的定时器存在，就删除该定时器
    if (0 <= timerId)
    {
        return p_manager_->kill_timer(timerId);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : on_check_strategy
 功能描述  : 定时器的回调函数
 输入参数  : int32_t eventId:事件号
             int32_t
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform::on_check_strategy(int32_t eventId, int32_t)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "Begin to check record strategy, camera[%s].",
        str_frontdev_id_));

    if (FRONT_DEVICE_STATE_INFORMING != state_)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Can not check record strategy as status is invalid, "
            "camera[%s], status[%d].",
            str_frontdev_id_,
            state_));

        return SVS_RESULT_OK;
    }

    // 如果事件id不相同，不需处理，直接返回
    if ((routerid_t)eventId != internal_id_)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "EnventID given by callback function is not consistent with camera's internal eventID, "
            "given eventID[%d], internal eventID[%d].",
            eventId,
            internal_id_));

        return SVS_RESULT_OK;
    }

    // 获取镜头离最早开始执行的策略相差的时间
    time_t timeDiff = -1;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, SVS_RESULT_FAILURE);

        timeDiff = strat_list_.howlong();
    }

    // 如果镜头还没有开始启动录像，那么要重新设置定时器
    if (0 < timeDiff)
    {
        SVS_LOG((SVS_LM_INFO,
            "Camera does not reach record time and will set timer, "
            "camera[%s], wait time[%d].",
            str_frontdev_id_,
            timeDiff));

        //重新设置定时器
        ACE_Time_Value delay((int32_t)timeDiff, 0);
        ACE_Time_Value intervalTime(SVS_TIMER_INTERVAL_TIMEOUT, 0);
        int32_t setTimerResult = set_timer(state_, delay, intervalTime);
        if (SVS_RESULT_FAILURE == setTimerResult)
        {
            SVS_LOG((SVS_LM_ERROR, "Fail to set timer, camera[%s].",
                str_frontdev_id_));
        }

        return SVS_RESULT_OK;
    }

    int32_t ret = SVS_RESULT_OK;
    {
        SVS_LOG((SVS_LM_INFO,
            "Begin to search for the earliest record strategy, camera[%s].",
            str_frontdev_id_));

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, 0);
        ret = strat_list_.search_earliest_time();
    }

    if (SVS_RESULT_OK != ret)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Can not find record strategy .. camera[%s].",
            str_frontdev_id_));

        // 今天没有策略，设置0点定时器
        SVS_LOG((SVS_LM_WARNING,
            "Camera does not have recording strategy, Wait for tomorrow record. camera[%s].",
            str_frontdev_id_));

        record_flags_ = RECORDING_NONE;
        time_t currTime =  time(NULL);
        time_t todayEnd = SECONDS_PER_DAY - (currTime % SECONDS_PER_DAY);
        ACE_Time_Value delay(todayEnd, 0);
        ACE_Time_Value intervalTime(SVS_TIMER_INTERVAL_TIMEOUT, 0);
        if (SVS_RESULT_FAILURE == set_timer(state_, delay, intervalTime))
        {
            SVS_LOG((SVS_LM_WARNING, "Fail to set record timer, camera[%s].", str_frontdev_id_));
        }
        (void)inform_recording();
        (void)CAC_RT_Record_Device_Manager::instance()->device_off_line(str_frontdev_id_);

        return SVS_RESULT_OK;
    }

    (void)strat_list_.print_selected_strategy(str_frontdev_id_);

    // 计算最早的策略还需要多长时间生效，并设置定时器在策略生效之时也生效
    ACE_Time_Value delay;
    timeDiff = strat_list_.howlong();
    delay.set((int32_t)timeDiff, 0);
    ACE_Time_Value intervalTime(SVS_TIMER_INTERVAL_TIMEOUT, 0);
    if (SVS_RESULT_FAILURE == set_timer(state_, delay, intervalTime))
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to set record timer, camera[%s].", str_frontdev_id_));
    }

    //发送录像通知，并重新选择时间
    int32_t informResult = inform_recording();

    return informResult;
}

/*****************************************************************************
 函 数 名  : inform_recording
 功能描述  : 发送录像通知
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform::inform_recording()
{
    SVS_TRACE();

    // 获取录像类型，如果镜头没有设置录像类型，则认为是禁止录像
    uint8_t recordFlag = 0;
    uint8_t flags = strat_list_.record_type( recordFlag );

    if (RECORDING_NONE == flags)
    {
        flags = FROBID_RECORDING;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Inform recording deal, camera[%s], recordFlag[%d], "
        "flags[%d], strat endtime[%u], event strat time[%u].",
        str_frontdev_id_, recordFlag, flags,
        strat_list_.end_time(), strat_list_.get_event_strategy_time()));

    record_flags_ = flags;

    //根据最新状态设置录像
    int32_t recordResult = CAC_RT_Record_Device_Manager::instance()->control_record(
        str_frontdev_id_,
        encoder_type_,
        flags,
        strat_list_.end_time(),
        event_before_record_time_,
        event_after_record_time_,
        strat_list_.get_event_strategy_time());
    if(SVS_RESULT_OK != recordResult)
    {
        return recordResult;
    }

    // 如果事件录像策略生效，则通知事件
    recordResult = informEvent();
    return recordResult;
}

/*****************************************************************************
 函 数 名  : renew_strategy
 功能描述  : 更新策略列表
 输入参数  : STRAT_VECTOR& stratVector:新的策略列表
 输出参数  : 无
 返 回 值  : 成功:true 失败:false
 修改历史  :
*****************************************************************************/
bool CAC_Record_Inform::renew_strategy(const STRAT_VECTOR& stratVector)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "Begin to update record strategy. camera[%s].", str_frontdev_id_));

    size_t stratSize = stratVector.size();
    if ( 0 == stratSize )
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera has no record strategy. camera[%s].",
            str_frontdev_id_));

        // 今天没有策略，设置0点定时器
        SVS_LOG((SVS_LM_WARNING,
            "Camera does not have recording strategy .Wait for tomorrow record. camera[%s].",
            str_frontdev_id_));

        record_flags_ = RECORDING_NONE;
        time_t currTime =  time(NULL);
        time_t todayEnd = SECONDS_PER_DAY - (currTime % SECONDS_PER_DAY);
        ACE_Time_Value delay(todayEnd, 0);
        ACE_Time_Value intervalTime(SVS_TIMER_INTERVAL_TIMEOUT, 0);

        if (SVS_RESULT_FAILURE == set_timer(state_, delay, intervalTime))
        {
            SVS_LOG((SVS_LM_WARNING, "Fail to set record timer. camera[%s].", str_frontdev_id_));
        }

        (void)CAC_RT_Record_Device_Manager::instance()->stop_device_record(str_frontdev_id_);  //停止录像

        return true;
    }

    bool initResult = init_strate(stratVector);

    if (initResult)
    {
        (void)exec_strategy();
    }

    return true;
}

/*****************************************************************************
 函 数 名  : off_line
 功能描述  : 镜头下线
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform::off_line()
{
    SVS_TRACE();

    // 停止所有手动录像
    (void)kill_timer( FRONT_DEVICE_STATE_STOP );

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : stop_record
 功能描述  : 停止录像
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform::stop_record( )
{
    SVS_TRACE();

    // 设置状态为停止
    (void)kill_timer( FRONT_DEVICE_STATE_STOP );

    (void)CAC_RT_Record_Device_Manager::instance()->stop_device_record( str_frontdev_id_, false );

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : init_lens
 功能描述  : 初始化镜头对象
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功初始化,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform::init_lens()
{
    SVS_TRACE();

    //初始化录像参数
    int32_t initResult = init_record_parameters();

    if (SVS_RESULT_OK != initResult)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to initialize camera record parameters, camera[%s].",
            str_frontdev_id_));

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO, "Success to initialize camera, camera[%s].", str_frontdev_id_));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : init_record_parameters
 功能描述  : 从数据库中读取镜头的录像参数,并初始化，包括事件录像预录时长、后录时长
             如果数据库中没有该镜头的录像参数，则默认的事件录像预录时长为0，
             后录时长为0
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功读取录像参数，并初始化,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform::init_record_parameters()
{
    SVS_TRACE();

    SVS_LENS_PARAMETER lensParam;

    if (SVS_RESULT_OK !=
        CAC_Record_Inform_Manager::instance()->get_lens_param(str_frontdev_id_, lensParam))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to get record parameter, camera[%s].",
            str_frontdev_id_));

        return SVS_RESULT_FAILURE;
    }

    event_before_record_time_ = lensParam.BeforeSeconds;
    event_after_record_time_ = lensParam.AfterSeconds;

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : update_record_parameter
 功能描述  : 更新镜头对象的录像参数
 输入参数  : nBeforeSeconds :   事件录像预录时长
             nAfterSeconds  :   事件录像后录时长
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform::update_record_parameter(uint32_t nBeforeSeconds,
                                               uint32_t nAfterSeconds )
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    event_before_record_time_ = nBeforeSeconds;
    event_after_record_time_ = nAfterSeconds;

    // 发送录像通知
    (void)inform_recording();

    SVS_LOG((SVS_LM_INFO,
        "Success to update record parameter, "
        "camera[%s].",
        str_frontdev_id_));

    return SVS_RESULT_OK;
}

// 停止inform对象，如果没有流程再使用了，应用计算为0，返回成功，否在返回-1表示还有流程在用
int32_t CAC_Record_Inform::stop_inform()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, false);
    SVS_LOG((SVS_LM_INFO,
        "Being to stop camera inform.cameraId[%s],state[%d]",
        str_frontdev_id_,
        state_));
    (void)kill_timer(FRONT_DEVICE_STATE_STOP);
    if(0 == referenc_count_)
    {
        return SVS_RESULT_OK;
    }

    return SVS_RESULT_FAILURE;
}

int32_t CAC_Record_Inform::alarm_event_req(const char *strAlarmDeviceId,
                                            uint32_t eventType,
                                            uint16_t controlCode)
{
    // 如果是事件发生，刷新事件结束时间为当前时间向后推最大事件录像时长
    time_t ulEventEndTime = time(NULL);
    if(SVS_DEVICE_CONTROL_REC_START == controlCode)
    {
       ulEventEndTime += (time_t)CAC_RT_Record_Server::instance()->event_record_lost_timeout();

    }

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    event_vector_[eventType][strAlarmDeviceId] = ulEventEndTime;

    return SVS_RESULT_OK;
}

int32_t CAC_Record_Inform::informEvent()
{
    // 如果事件录像策略无效，则不用通知底层事件
    if (!effect_event_stratetegy())
    {
        SVS_LOG((SVS_LM_INFO,
            "Inform event finished.Event strategy is noneffective."
            "cameraID[%s].",
            str_frontdev_id_));

        return SVS_RESULT_OK;
    }

    vector <EVENTMAP > tempEventVct;
    EVENTMAP emap;
    for (size_t i = 0; i < RECORD_EVENT_TYPE_MAX; i++)
    {
        tempEventVct.push_back(emap);
    }
    {
        // 遍历所有的事件，比较当前的时间和事件的结束时间，如果当前事件已经过期了，则删除事件记录
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
        vector <EVENTMAP >::size_type eventVecSize = event_vector_.size();
        EVENTMAP::iterator pIter;
        time_t currTime = time(NULL);
        for ( size_t j = 0; j < eventVecSize; j++ )
        {
            for (pIter = event_vector_[j].begin( ) ; pIter != event_vector_[j].end( ) ; pIter++)
            {
                SVS_LOG((SVS_LM_DEBUG,
                    "Inform Event.CameraId[%s], alarmCameraId[%s],"
                    "alarmControleCode[%d], alarmEndTime[%u],currTime[%u].",
                    str_frontdev_id_,
                    pIter->first.c_str(),
                    j,
                    pIter->second,
                    currTime));

                // 如果事件过期，则删除
                if (pIter->second <= currTime)
                {
                    event_vector_[j].erase(pIter);
                }
                else// 事件还未过期，则记录，后面通知底层，启动事件录像
                {
                    tempEventVct[j][pIter->first.c_str()] = pIter->second;
                }
            }
        }
    }

    // 判断镜头当前的状态是否可以录像，并将结果透传给镜头管理器
    bool bIsRecord = false;
    if (FROBID_RECORDING != record_flags())
    {
        bIsRecord = true;
    }

    // 启动事件
    {
        vector <EVENTMAP >::size_type eventVecSize = tempEventVct.size();
        EVENTMAP::iterator pIter;
        for ( size_t k = 0; k < eventVecSize; k++ )
        {
            for (pIter = tempEventVct[k].begin( ) ; pIter != tempEventVct[k].end( ) ; pIter++)
            {
                (void)CAC_RT_Record_Device_Manager::instance()->event_happen(
                            str_frontdev_id_,
                            pIter->first.c_str(),
                            (uint32_t)k,
                            encoder_type(),
                            curr_strategy_end_time(),
                            event_before_record_time(),
                            event_after_record_time(),
                            bIsRecord);
            }
        }
    }
    for (size_t l = 0; l < tempEventVct.size() && l < RECORD_EVENT_TYPE_MAX; l++)
    {
        tempEventVct[l].clear();
    }
    tempEventVct.clear();

    return SVS_RESULT_OK;
}

//========================CAC_Record_Inform END================================


//========================CAC_Record_Inform_Manager BEGIN======================
CAC_Record_Inform_Manager::CAC_Record_Inform_Manager()
{
    max_lens_ = 0;
    state_ = FDM_STATE_QUERY_DEVICE;
    next_internal_id_   = 1;
    p_strategy_reactor_ = NULL;
    p_strategy_timer_manager_ = NULL;
}

CAC_Record_Inform_Manager::~CAC_Record_Inform_Manager()
{
    p_strategy_reactor_ = NULL;
    p_strategy_timer_manager_ = NULL;
}

/*****************************************************************************
 函 数 名  : instance
 功能描述  : 获取录像通知管理器实例
 输入参数  : 无
 输出参数  : 无
 返 回 值  : CAC_Record_Inform_Manager* 录像通知管理器对象指针
 修改历史  :
*****************************************************************************/
CAC_Record_Inform_Manager *CAC_Record_Inform_Manager::instance()
{
    static CAC_Record_Inform_Manager recordInformManager;

    return &recordInformManager;
}

/*****************************************************************************
 函 数 名  : init
 功能描述  : 初始化录像通知管理器实例
 输入参数  : uint32_t nMaxLens:最大镜头数量
 输出参数  : 无
 返 回 值  : 成功:true 失败:false
 修改历史  :
*****************************************************************************/
bool CAC_Record_Inform_Manager::init(uint32_t nMaxLens)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to init record inform manager. max camera num[%d].",
        nMaxLens));
    max_lens_ = nMaxLens;

    // 不用退出程序，在获取录像参数的时候，还可以实时查询
    if (SVS_RESULT_OK != init_lens_param())
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to init camera parameter."));
    }

    return true;
}

/*****************************************************************************
 函 数 名  : add_device
 功能描述  : 根据镜头ID和编码类型增加一个镜头对象
 输入参数  : const char *strFrontDevId:前端镜头ID
             int32_t encoderType:编码类型
             CAC_Record_Inform *&pDevice:所添加的镜头对象指针
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::add_device(const char  * strFrontDevId,
                                          int32_t                 encoderType,
                                          CAC_Record_Inform *&pDevice )
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO,
        "Begin to add camera, camera[%s], encodeType[%d].",
        strFrontDevId, encoderType));

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);

    // 判断镜头个数是否已达到上限,达到上限则直接返回SVS_RESULT_FAILURE
    uint32_t currCameraNum = device_map_.current_size();
    uint32_t backupCameraNum = CAC_Bakup_Record_Dev_Manager::instance()->get_bakup_device_num();
    size_t lensSize = currCameraNum + backupCameraNum;

    // 备份镜头和实时录像镜头一起管理
    lensSize += CAC_Bakup_Record_Dev_Manager::instance()->get_bakup_device_num();

    SVS_LOG((SVS_LM_INFO,
        "Total camera number info. Realtime record camera num[%u], backup camera num[%u].",
        currCameraNum,
        backupCameraNum));

    // 查找是否存在指定镜头ID的镜头对象，如存在，则返回该镜头对象
    CAC_Record_Inform *pTmpDevice = get_device(strFrontDevId);
    if (NULL != pTmpDevice)
    {
        pDevice = pTmpDevice;

        SVS_LOG((SVS_LM_ERROR,
            "The camera is already exist and success to add. camera[%s].",
            strFrontDevId));
        return SVS_RESULT_OK;
    }

    if(max_lens_ <= lensSize)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Create inform objector for camera failed.Load is over."
            "Maximal camera number is [%u], current load camera number[%u](include backup camera),cameraId[%s].",
            max_lens_,
            lensSize,
            strFrontDevId));

        return SVS_ERR_OVER_LOAD;
    }

    // 创建一个新的镜头对象
    try
    {
        pTmpDevice = new CAC_Record_Inform(  strFrontDevId,
                                             (uint8_t )encoderType,
                                             this);
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Fail to allocate memory to create a camera object."));

        SVS_DELETE(pTmpDevice);

        return SVS_RESPCODE_MEM_ERROR;
    }

    SVS_LOG((SVS_LM_DEBUG, "Success to create camera. camera[%s], current camera num[%d].",
        strFrontDevId,
        device_map_.current_size()));

    //获取内部索引
    uint32_t internalId = next_internal_id();

    if (0 == internalId)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to allocate an internal id for camera, camera[%s].",
            strFrontDevId));

        SVS_DELETE( pTmpDevice );

        return SVS_RESPCODE_OTHER_ERROR;
    }

    pTmpDevice->internal_id(internalId);

    //将新创建的镜头对象插入到MAP表中
    if (0 != device_map_.bind(pTmpDevice->get_frontdev_id(), pTmpDevice))
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to add camera to map, camera[%s].", strFrontDevId));

        SVS_DELETE( pTmpDevice );

        return SVS_RESPCODE_OTHER_ERROR;
    }

    //将镜头和内部索引号相关联
    if (0 != request_map_.bind(internalId, pTmpDevice))
    {
        if (0 != device_map_.unbind(pTmpDevice->get_frontdev_id()))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Fail to delete camera from map, camera[%s].",
                strFrontDevId));
            SVS_DELETE( pTmpDevice );
            return SVS_RESULT_FAILURE;
        }

        SVS_LOG((SVS_LM_ERROR,
            "Fail to insert camera to map, cameraId[%s].",
            strFrontDevId));
        SVS_DELETE( pTmpDevice );

        return SVS_RESPCODE_OTHER_ERROR;
    }

    pDevice = pTmpDevice;

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : get_device
 功能描述  : 根据设备ID来获取录像通知对象指针
 输入参数  : const char *strFrontDevId:前端设备ID
 输出参数  : 无
 返 回 值  : 成功:镜头对象指针 失败:NULL
 修改历史  :
*****************************************************************************/
CAC_Record_Inform* CAC_Record_Inform_Manager::get_device(const char *strFrontDevId)
{
    SVS_TRACE();

    CAC_Record_Inform *pDevice = NULL;
    ACE_GUARD_RETURN(   ACE_Recursive_Thread_Mutex,
                        guard,
                        map_mutex_,
                        NULL );

    if ( 0 != device_map_.find(strFrontDevId, pDevice) )
    {
        return NULL;
    }

    if ( -1 == pDevice->increase_reference() )
    {
        return NULL;
    }

    return pDevice;
}

/*****************************************************************************
 函 数 名  : get_device
 功能描述  : 根据索引ID来获取录像通知对象指针
 输入参数  : uint32_t nId:索引序号
 输出参数  : 无
 返 回 值  : 成功:镜头对象指针 失败:NULL
 修改历史  :
*****************************************************************************/
CAC_Record_Inform *CAC_Record_Inform_Manager::get_device(uint32_t nId)
{
    SVS_TRACE();

    CAC_Record_Inform *pDevice = NULL;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);
    if (0 == request_map_.find(nId, pDevice))
    {
        if (-1 == pDevice->increase_reference() )
        {
            return NULL;
        }
        return pDevice;
    }

    SVS_LOG((SVS_LM_ERROR, "Fail to get camera. camera[%d].", nId));
    return NULL;
}

/*****************************************************************************
 函 数 名  : remove_device
 功能描述  : 从map表中删除对应的镜头
 输入参数  : uint32_t nId:内部索引号
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::remove_device(uint32_t nId)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
    if (0 != request_map_.unbind(nId))
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to delete camera from map. camrea[%d].", nId));
        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : remove_device
 功能描述  : 从镜头map表中删除对应的前端设备
 输入参数  : uint32_t nId:前端镜头ID
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::remove_device(const char *strFrontDevId)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
    if (0 != device_map_.unbind(strFrontDevId))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to delete camera from map. camera[%s].",
            strFrontDevId));
        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : query_device_strategy
 功能描述  : 查询镜头策略
 输入参数  : const char *strLensId:镜头ID
             STRAT_VECTOR& stratVector:策略列表
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
//int32_t CAC_Record_Inform_Manager::query_device_strategy( const char *strLensId, STRAT_VECTOR& stratVector ) const
//{
//    SVS_TRACE();
//
//    DTRecordStrategyList stStrategyList;
//    int32_t iRet = DBAPI::instance()->ReadRecordStrategy(strLensId, stStrategyList);
//
//    if (iRet != DBAPI_RETCODE_SUCCESS)
//    {
//        SVS_LOG((SVS_LM_ERROR, "Fail to query record strategy, "
//            "camera[%s], ret[%d].", strLensId, iRet));
//
//        return SVS_RESPCODE_DB_ERROR;
//    }
//
//    std::list<DTRecordStrategy>::iterator iter = stStrategyList.begin();
//    int32_t iStrategyCount = 0;
//
//    for (; iter != stStrategyList.end(); iter++)
//    {
//        struct STRAT_PROPERTY stratProperty;
//
//        int32_t iStragegyFlag = iter->ucStrategyFlag;
//
//        // 如果是手动录像，则强制为一次性录像
//        if (SVS_RECORDER_MANUAL == iter->ucTriggerFlag)
//        {
//            iStragegyFlag = 1;
//        }
//
//        // 如果是手动录像，不仅需要判断录像开始结束时间，还要判断日期
//        if (1 == iStragegyFlag)
//        {
//            stratProperty.startTime = SVS_SS_UTILITIES::str2time(iter->szExecuteTime);
//
//            char szEndTime[SVS_STR_TIME_MAX_LEN] = {0};
//            (void)ACE_OS::memcpy(szEndTime, iter->szExecuteTime, RT_DATE_STR_LEN);
//
//            //需要把录像结束时间的前几位去掉?
//            (void)ACE_OS::memcpy(szEndTime + RT_DATE_STR_LEN, iter->szEndTime, RT_DATE_STR_LEN);
//
//            stratProperty.endTime = SVS_SS_UTILITIES::str2time(szEndTime);
//
//            // 如果一次性录像策略已经过期，直接返回就ok
//            if (stratProperty.endTime < time(NULL))
//            {
//                SVS_LOG((SVS_LM_DEBUG,
//                    "Manual record strategy has been overdue, "
//                    "strategyID[%s], strategyTime[%s-%s], "
//                    "date[%s], strategy exec time[%s].",
//                    iter->szStrategyId,
//                    iter->szStartTime,
//                    iter->szEndTime,
//                    iter->szExecuteTime,
//                    szEndTime));
//
//                continue;
//            }
//        }
//        else
//        {
//            stratProperty.endTime   = SVS_SS_UTILITIES::hh24miss2second(iter->szEndTime);
//            stratProperty.startTime = SVS_SS_UTILITIES::hh24miss2second(iter->szStartTime);
//        }
//
//        (void)ACE_OS::strcpy(stratProperty.stratId, iter->szStrategyId);
//
//        // 判断开始和结束时间是否合法
//        if (stratProperty.endTime <= stratProperty.startTime)
//        {
//            SVS_LOG((SVS_LM_WARNING,
//                "Record strategy is invalid, "
//                "because its start time is later than end time, "
//                "record strategy[%s], startTime[%s:%d], endTime[%s:%d].",
//                iter->szStrategyId,
//                iter->szStartTime,
//                stratProperty.startTime,
//                iter->szEndTime,
//                stratProperty.endTime));
//
//            continue;
//        }
//
//        stratProperty.stratFlag   = iter->ucStrategyFlag;
//        stratProperty.triggerFlag = iter->ucTriggerFlag;
//        stratProperty.weekFlag = 0;
//        stratProperty.recordStatus = (uint8_t)0x01;
//
//        //判断周标志字符串的长度是否满足要求
//        const uint32_t weekFlagLen = 8;
//
//        if (strlen(iter->szWeekFlag) != weekFlagLen)
//        {
//            SVS_LOG((SVS_LM_CRITICAL,
//                "Record strategy is invalid, "
//                "because its week flag is not enougth, "
//                "strategyID[%s], week flag[%s].",
//                iter->szStrategyId,
//                iter->szWeekFlag));
//
//            continue;
//        }
//
//        // 将一周中每天的录像标志都设置到stratProprety.weekFlag中
//        for (uint32_t flagIndex = 0; flagIndex < weekFlagLen; flagIndex++)
//        {
//            if ('1' == iter->szWeekFlag[flagIndex])
//            {
//                stratProperty.weekFlag |= (uint8_t)(0x01 << flagIndex);
//            }
//        }
//
//        char startTime[SVS_STR_TIME_MAX_LEN] = {0,};
//        char endTime[SVS_STR_TIME_MAX_LEN] = {0,};
//        (void)SVS_SS_UTILITIES::time2str(startTime, SVS_STR_TIME_MAX_LEN, stratProperty.startTime);
//        (void)SVS_SS_UTILITIES::time2str(endTime, SVS_STR_TIME_MAX_LEN, stratProperty.endTime);
//
//        SVS_LOG((SVS_LM_DEBUG,
//            "Success to add record strategy, "
//            "strategyID[%s], recordType[%d], timeRange[%s-%s].",
//            iter->szStrategyId,
//            iStragegyFlag,
//            startTime,
//            endTime));
//
//        stratVector.push_back(stratProperty);
//        iStrategyCount++;
//    }
//
//    SVS_LOG((SVS_LM_INFO,
//        "Query device record strategy success, "
//        "camera[%s], strategyCount[%d].",
//        strLensId, iStrategyCount));
//
//    return SVS_RESULT_OK;
//}

/*****************************************************************************
 函 数 名  : check_strategy
 功能描述  : 检查并执行录像策略
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::check_strategy()
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
    SVS_DEVICE_MAP_ITER iter(device_map_);

    SVS_DEVICE_MAP_ENTRY *pEntry    = NULL;
    CAC_Record_Inform *pDev         = NULL;
    int32_t execResult                  = 0;

    int32_t execNum = 0;
    ACE_Time_Value sleepTv( 0, EXEC_STRATEGY_SLEEP_TIME );

    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pDev = pEntry->int_id_;
        execResult = pDev->exec_strategy();

        if ( SVS_RESULT_OK != execResult )
        {
            return SVS_RESULT_FAILURE;
        }

        // 防止服务启动时，发送大量的请求路由消息
        execNum++;
        if( 0 == (execNum % EXEC_STRATEGY_SLEEP_DEVICE_NUM) )
        {
            (void)ACE_OS::sleep( sleepTv );
        }
    }

    SVS_LOG((SVS_LM_INFO, "Success to execute record strategy."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : next_internal_id
 功能描述  : 生成下一个内部索引ID
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 如果内部ID没有达到最大值:下一个内部ID, 如果内部ID达到最大值:0
 修改历史  :
*****************************************************************************/
uint32_t CAC_Record_Inform_Manager::next_internal_id()
{
    SVS_TRACE();

    uint32_t internalId = 0;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, 0);
    for (uint32_t i = 1; i < UINT_MAX; i++)
    {
        internalId = next_internal_id_++;
        if (-1 == request_map_.find(internalId))
        {
            return internalId;
        }

        if (UINT_MAX == next_internal_id_)
        {
            next_internal_id_ = 1;
        }
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : decrease_reference
 功能描述  : 将镜头的引用计数减一，当镜头没有再被引用时，删除镜头
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::decrease_reference(CAC_Record_Inform *pDevice)
{
    SVS_TRACE();

    if ( NULL == pDevice )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to decreace reference, because the pointer pDevice is null."));
        return SVS_RESULT_FAILURE;
    }

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
    if (1 != pDevice->decrease_reference())
    {
        pDevice = NULL;
        return SVS_RESULT_OK;
    }

    if ((SVS_RESULT_OK == remove_device(pDevice->internal_id()))
      && (SVS_RESULT_OK == remove_device(pDevice->get_frontdev_id())))
    {
        SVS_LOG((SVS_LM_INFO, "Success to delete camera. cameraID[%s].",
            pDevice->get_frontdev_id()));
        SVS_DELETE( pDevice );
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : check_strategy_loop
 功能描述  : 检测录像策略线程
 输入参数  : ACE_Reactor *pReactor: 录像通知反应器
             CAC_Timer_Manager *
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::check_strategy_loop(ACE_Reactor *pReactor, CAC_Timer_Manager * )
{
    SVS_TRACE();

    p_strategy_reactor_ = pReactor;
    int32_t ret = 0;
    CThread_Stat_Reporter reporter("CheckStrategyThread");
    // 修改邋handle_events失败线程退出问题while循环内部不会break
    // 此时应修改while循环的判断条件，否则循环不能退出*/
    while ( 0 == p_strategy_reactor_->reactor_event_loop_done())
    {
        ACE_Time_Value procTime(0, SVS_REACTOR_PROC_TIME);
        ret = p_strategy_reactor_->handle_events( &procTime );
        if ( 0 > ret )
        {
            SVS_LOG((SVS_LM_WARNING,
                "Fail to deal event record strategy. reactor[0x%08x].",
                p_strategy_reactor_));

            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();
    }

    SVS_LOG((SVS_LM_WARNING, "Check record strategy thread exit."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : on_check_strategy
 功能描述  : 定时器回调函数
 输入参数  : int32_t eventId:事件ID
             int32_t timerId:定时器ID
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::on_check_strategy(int32_t eventId, int32_t timerId)
{
    SVS_TRACE();

    // 根据事件ID来查找对应的镜头
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
    CAC_Record_Inform *pDevice = get_device((uint32_t)eventId);
    if (NULL != pDevice)
    {
        // 执行镜头的回调函数
        CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pDevice);
        int32_t checkResult = pDevice->on_check_strategy(eventId, timerId);

        return checkResult;
    }

    (void)kill_timer( timerId );
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : renew_strategy
 功能描述  : 更新指定设备的录像策略
 输入参数  : const uint8_t * const strDeviceId:镜头ID
 输出参数  :
 返 回 值  : 成功:0  失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::renew_strategy(const char *strDeviceId,
                                    const STRAT_VECTOR &stratVector)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "Update camera record strategy, camera[%s].", strDeviceId));

    CAC_Record_Inform *pInform = get_device((const char *)strDeviceId);

    if (NULL == pInform)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Update camera record strategy failed, Can't find the camera, cameraId[%s].",
            strDeviceId));

        return SVS_RESULT_FAILURE;
    }

    CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pInform);

    (void)pInform->renew_strategy(stratVector);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : renew_strategy
 功能描述  : 更新镜头录像策略
 输入参数  : const ACE_Message_Block *mb:消息块
 输出参数  :
 返 回 值  : 成功:0  失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::renew_strategy(const ACE_Message_Block *mb)
{
    SVS_TRACE();

    PSVS_STORESVR_RENEW_STRATEGY_REQ_MSG pReq =
        (PSVS_STORESVR_RENEW_STRATEGY_REQ_MSG)(void*)mb->rd_ptr();

//    const size_t msgSize = sizeof(SVS_STORESVR_RENEW_STRATEGY_REQ_MSG)
//                           + (pReq->DeviceCount - 1) * (DEVICEID_LEN);
//
//    //根据消息中的设备ID数目校验总消息长度是否合法
//    if (msgSize != mb->length())
//    {
//        SVS_LOG((SVS_LM_WARNING,
//            "Fail to update recording strategy as recv msg size is invalid, "
//            "recv msg size[%d], expected msg size[%d].",
//            mb->length(),
//            msgSize));
//
//        return SVS_RESULT_OK;
//    }

    // 先校验一下固定长度，防止越界访问
    int32_t iDeviceStrategyInfoLen = pReq->Header.PacketLength -
            offsetof(SVS_STORESVR_RENEW_STRATEGY_REQ_MSG, DeviceStrategyInfo);

    if (iDeviceStrategyInfoLen < 0)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Request message length invalid, msgLen[%d], minLen[%d].",
            pReq->Header.PacketLength,
            offsetof(SVS_STORESVR_RENEW_STRATEGY_REQ_MSG, DeviceStrategyInfo)));
        return SVS_RESULT_OK;
    }

    SVS_LOG((SVS_LM_INFO,
        "Recv update strategy message, update strategy camera num[%d].",
        pReq->DeviceCount));

    // 在解析时当遇到错误时，不试图解析后面的镜头,但是已经成功解析的镜头需要把策略更新到本地
    uint32_t    uiLeftLen = (uint32_t)iDeviceStrategyInfoLen;
    uint8_t     *pInfoPtr = (uint8_t *)&pReq->DeviceStrategyInfo[0];

    // 更新所有请求镜头镜头的录像策略
    for (int32_t i = 0; i < pReq->DeviceCount; i++)
    {
        // 对单个镜头校验固定部分的长度
        if (uiLeftLen < offsetof(DEVICE_STRATEGY_INFO, RecordSvrStrategyInfo))
        {
            SVS_LOG((SVS_LM_WARNING,
                "Device strategy renew info length invalid, bufLen[%d], minLen[%d].",
                uiLeftLen,
                offsetof(DEVICE_STRATEGY_INFO, RecordSvrStrategyInfo)));
            return SVS_RESULT_FAILURE;
        }

        DEVICE_STRATEGY_INFO *pDevInfo = (DEVICE_STRATEGY_INFO *)pInfoPtr;
        char  szDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0};
        SetLocalizeDeviceId((uint8_t *)szDeviceId,
                            sizeof(szDeviceId),
                            (const char *)pDevInfo->DeviceID,
                            NLS_DEVICE_ID_MAX_LEN);

        //对每个镜头包括策略列表共同计算长度，并对策略列表进行解析后跳到下一个设备
        uint32_t   uiDeviceInfoLen = offsetof(DEVICE_STRATEGY_INFO, RecordSvrStrategyInfo)
                + (uint32_t)pDevInfo->RecordStrategyCount * sizeof(RECORD_STRATEGY_INFO);
        if (uiDeviceInfoLen > uiLeftLen)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Device strategy renew info length invalid, bufLen[%d], stratCount[%d], infoLen[%d].",
                uiLeftLen, pDevInfo->RecordStrategyCount, uiDeviceInfoLen));
            return SVS_RESULT_FAILURE;
        }

        // 解析单个镜头的录像策略之前，已经对长度进行了校验，可以安全的当数组访问
        STRAT_VECTOR  statVector;
        if (SVS_RESULT_OK != fill_strategy_list((uint32_t)pDevInfo->RecordStrategyCount,
                        pDevInfo->RecordSvrStrategyInfo, statVector))
        {
            SVS_LOG((SVS_LM_WARNING, "Fetch strategy list from message failed."));
            statVector.clear();
            return SVS_RESULT_FAILURE;
        }

        // 流程修改后，更新策略时从消息中解析出策略，不从数据库获取
        (void)renew_strategy(szDeviceId, statVector);

        // 偏移到下一个镜头开始的位置，由于前面已经进行了长度限制，此处不会翻转
        uiLeftLen -= uiDeviceInfoLen;
        pInfoPtr += uiDeviceInfoLen;
    }

    // 最后解析完成后，应该没有多余的消息体未解析
    if (0 != uiLeftLen)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Update strategy message length invalid, devCount[%d], msgLen[%d], leftLen[%d].",
            pReq->DeviceCount, pReq->Header.PacketLength, uiLeftLen));
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : alarm_code_translate
 功能描述  : 转化告警码为事件类型和告警类型
 输入参数  : uint8_t alarmCode:告警码
 输出参数  : uint32_t &eventType:事件类型
             uint8_t &alarmType:告警类型
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::alarm_code_translate(uint8_t  alarmCode,
                                                    uint32_t &eventType,
                                                    uint8_t &alarmType ) const
{
    SVS_TRACE();
    switch (alarmCode)
     {
         case SVS_ALARM_CODE_ALARMIN_HAPPEN:
             eventType = RECORD_EVENT_TYPE_ALARM;
             alarmType = RECORD_EVENT_TYPE_ALARMIN_HAPPEN;
             break;

         case SVS_ALARM_CODE_ALARMIN_PASS:
             eventType = RECORD_EVENT_TYPE_ALARM;
             alarmType = RECORD_EVENT_TYPE_ALARMIN_PASS;
             break;

         case SVS_ALARM_CODE_VIDEO_LOST:
         case SVS_ALARM_CODE_VIDEO_SIGN_BREAK:
             eventType = RECORD_EVENT_TYPE_VIDEO;
             alarmType = RECORD_EVENT_TYPE_VIDEO_LOST;
             break;

         case SVS_ALARM_CODE_VIDEO_RESUME:
         case SVS_ALARM_CODE_VIDEO_SIGN_COMEBACK:
             eventType = RECORD_EVENT_TYPE_VIDEO;
             alarmType = RECORD_EVENT_TYPE_VIDEO_RESUME;
             break;

         case SVS_ALARM_CODE_VIDEO_MOTION_HAPPEN:
             eventType = RECORD_EVENT_TYPE_MOTION;
             alarmType = RECORD_EVENT_TYPE_VIDEO_MOTION_HAPPEN;
             break;

         case SVS_ALARM_CODE_VIDEO_MOTION_PASS:
             eventType = RECORD_EVENT_TYPE_MOTION;
             alarmType = RECORD_EVENT_TYPE_VIDEO_MOTION_PASS;
             break;

         case SVS_ALARM_CODE_DEVICE_ONLINE:
             eventType = RECORD_EVENT_TYPE_DEVICE;
             alarmType = RECORD_EVENT_TYPE_DEVICE_ONLINE;
             break;

         case SVS_ALARM_CODE_DEVICE_OFFLINE:
             eventType = RECORD_EVENT_TYPE_DEVICE;
             alarmType = RECORD_EVENT_TYPE_DEVICE_OFFLINE;
             break;

         case SVS_ALARM_CODE_VIDEO_MASK:
             eventType = RECORD_EVENT_TYPE_MASK;
             alarmType = RECORD_EVENT_TYPE_VIDEO_MASK;
             break;

         case SVS_ALARM_CODE_VIDEO_UNMASK:
             eventType = RECORD_EVENT_TYPE_MASK;
             alarmType = RECORD_EVENT_TYPE_VIDEO_UNMASK;
             break;

        // case SVS_ALARM_CODE_DISK_BREAKDOWN:
        //     eventType = RECORD_EVENT_TYPE_DISK;
        //     alarmType = RECORD_EVENT_TYPE_DISK_BREAKDOWN;
        //     break;

        // case SVS_ALARM_CODE_DISK_FAULT_RECOVERY:
        //     eventType = RECORD_EVENT_TYPE_DISK;
        //     alarmType = RECORD_EVENT_TYPE_DISK_FAULT_RECOVERY;
        //     break;
        case SVS_ALARM_CODE_DISK_BREAKDOWN:
        case SVS_ALARM_CODE_DISK_FAULT_RECOVERY:
        case SVS_ALARM_CODE_SDCARD_FAILED:
        case SVS_ALARM_CODE_SDCARD_FAILED_RECOVERY:
        case SVS_ALARM_CODE_SDCARD_EXIST_BAD_BLOCK:
        case SVS_ALARM_CODE_SDCARD_NO_BAD_BLOCK:
        case SVS_ALARM_CODE_PRESENCE_DETECTION:
        case SVS_ALARM_CODE_PRESENCE_DETECTION_RECOVERY:
        {
            //SD异常告警码转换
            sdcard_alarm_code_translate(alarmCode,eventType,alarmType);
            break;
        }

        case SVS_ALARM_CODE_INTRUSION_DETECTION:
        case SVS_ALARM_CODE_INTRUSION_DETECTION_RECOVERY:
        case SVS_ALARM_CODE_TRIP_LINE_DETECTION:
        case SVS_ALARM_CODE_TRIP_LINE_DETECTION_RECOVERY:
        case SVS_ALARM_CODE_STATIC_OBJECT_DETECTION:
        case SVS_ALARM_CODE_STATIC_OBJECT_DETECTION_RECOVERY:
        case SVS_ALARM_CODE_OBJECT_REMOVAL_DETECTION:
        case SVS_ALARM_CODE_OBJECT_REMOVAL_DETECTION_RECOVERY:
        case SVS_ALARM_CODE_PATH_DETECTION:
        case SVS_ALARM_CODE_PATH_DETECTION_RECOVERY:
        case SVS_ALARM_CODE_SPEED_DETECTION:
        case SVS_ALARM_CODE_SPEED_DETECTION_RECOVERY:
        {
            intelligence_alarm_code_translate(alarmCode,eventType,alarmType);
            break;
        }

        default:
            SVS_LOG((SVS_LM_ERROR,
            "Fail to translate alarm code, alarm code[%u].",
            alarmCode));
        return SVS_RESULT_FAILURE ;
     }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : sdcard_alarm_code_translate
 功能描述  : 转化sdcard告警码为事件类型和告警类型
 输入参数  : uint8_t alarmCode:告警码
 输出参数  : uint32_t &eventType:事件类型
             uint8_t &alarmType:告警类型
 返 回 值  : 无
 修改历史  :
*****************************************************************************/
void CAC_Record_Inform_Manager::sdcard_alarm_code_translate( uint8_t  alarmCode,
                                                    uint32_t &eventType,
                                                    uint8_t &alarmType ) const
{
    SVS_TRACE();
    switch(alarmCode)
    {
        case SVS_ALARM_CODE_SDCARD_FAILED:
        {
            eventType  = RECORD_EVENT_TYPE_SDCARD_FAILED;
            alarmType = RECORD_EVENT_TYPE_SDCARD_FAIL;
            break;
        }

        case SVS_ALARM_CODE_SDCARD_FAILED_RECOVERY:
        {
            eventType = RECORD_EVENT_TYPE_SDCARD_FAILED;
            alarmType = RECORD_EVENT_TYPE_SDCARD_RECOVERY;
            break;
        }

        case SVS_ALARM_CODE_SDCARD_EXIST_BAD_BLOCK:
        {
            eventType = RECORD_EVENT_TYPE_SDCARD_BAD_BLOCK;
            alarmType = RECORD_EVENT_TYPE_SDCARD_EXIST_BAD_BLOCK;
            break;
        }

        case SVS_ALARM_CODE_SDCARD_NO_BAD_BLOCK:
        {
            eventType = RECORD_EVENT_TYPE_SDCARD_BAD_BLOCK;
            alarmType = RECORD_EVENT_TYPE_SDCARD_NO_BAD_BLOCK;
            break;
        }
        case SVS_ALARM_CODE_PRESENCE_DETECTION:
        {
            eventType = RECORD_EVENT_INNER_TYPE_PRESENCE_DETECTION;
            alarmType = RECORD_EVENT_TYPE_PRESENCE_DETECTION;
        }
        break;

        case SVS_ALARM_CODE_PRESENCE_DETECTION_RECOVERY:
        {
            eventType = RECORD_EVENT_INNER_TYPE_PRESENCE_DETECTION;
            alarmType = RECORD_EVENT_TYPE_PRESENCE_DETECTION_RECOVERY;
        }
        break;

        case SVS_ALARM_CODE_DISK_BREAKDOWN:
        {
            eventType = RECORD_EVENT_TYPE_DISK;
            alarmType = RECORD_EVENT_TYPE_DISK_BREAKDOWN;
        }
        break;

        case SVS_ALARM_CODE_DISK_FAULT_RECOVERY:
        {
            eventType = RECORD_EVENT_TYPE_DISK;
            alarmType = RECORD_EVENT_TYPE_DISK_FAULT_RECOVERY;
        }
        break;
        default:
            break;
    }

    return;
}

/*****************************************************************************
 函 数 名  : intelligence_alarm_code_translate
 功能描述  : 转化智能分析告警码为事件类型和告警类型
 输入参数  : uint8_t alarmCode:告警码
 输出参数  : uint32_t &eventType:事件类型
             uint8_t &alarmType:告警类型
 返 回 值  : 无
 修改历史  :
*****************************************************************************/

void CAC_Record_Inform_Manager::intelligence_alarm_code_translate( uint8_t  alarmCode,
                                                                        uint32_t &eventType,
                                                                       uint8_t &alarmType ) const
{
    SVS_TRACE();

    switch(alarmCode)
    {
        case SVS_ALARM_CODE_INTRUSION_DETECTION:
        {
            eventType = RECORD_EVENT_INNER_TYPE_INTRUSION_DETECTION;
            alarmType = RECORD_EVENT_TYPE_INTRUSION_DETECTION;
        }
        break;

        case SVS_ALARM_CODE_INTRUSION_DETECTION_RECOVERY:
        {
            eventType = RECORD_EVENT_INNER_TYPE_INTRUSION_DETECTION;
            alarmType = RECORD_EVENT_TYPE_INTRUSION_DETECTION_RECOVERY;
        }
        break;

        case SVS_ALARM_CODE_TRIP_LINE_DETECTION:
        {
            eventType = RECORD_EVENT_INNER_TYPE_TRIP_LINE_DETECTION;
            alarmType = RECORD_EVENT_TYPE_TRIP_LINE_DETECTION;
        }
        break;

        case SVS_ALARM_CODE_TRIP_LINE_DETECTION_RECOVERY:
        {
            eventType = RECORD_EVENT_INNER_TYPE_TRIP_LINE_DETECTION;
            alarmType = RECORD_EVENT_TYPE_TRIP_LINE_DETECTION_RECOVERY;
        }
        break;

        case SVS_ALARM_CODE_STATIC_OBJECT_DETECTION:
        {
            eventType = RECORD_EVENT_INNER_TYPE_STATIC_OBJECT_DETECTION;
            alarmType = RECORD_EVENT_TYPE_STATIC_OBJECT_DETECTION;
        }
        break;

        case SVS_ALARM_CODE_STATIC_OBJECT_DETECTION_RECOVERY:
        {
            eventType = RECORD_EVENT_INNER_TYPE_STATIC_OBJECT_DETECTION;
            alarmType = RECORD_EVENT_TYPE_STATIC_OBJECT_DETECTION_RECOVERY;
        }
        break;

        case SVS_ALARM_CODE_OBJECT_REMOVAL_DETECTION:
        {
            eventType = RECORD_EVENT_INNER_TYPE_OBJECT_REMOVAL_DETECTION;
            alarmType = RECORD_EVENT_TYPE_OBJECT_REMOVAL_DETECTION;
        }
        break;

        case SVS_ALARM_CODE_OBJECT_REMOVAL_DETECTION_RECOVERY:
        {
            eventType = RECORD_EVENT_INNER_TYPE_OBJECT_REMOVAL_DETECTION;
            alarmType = RECORD_EVENT_TYPE_OBJECT_REMOVAL_DETECTION_RECOVERY;
        }
        break;


        case SVS_ALARM_CODE_PATH_DETECTION:
        {
            eventType = RECORD_EVENT_INNER_TYPE_PATH_DETECTION;
            alarmType = RECORD_EVENT_TYPE_PATH_DETECTION;
        }
        break;

        case SVS_ALARM_CODE_PATH_DETECTION_RECOVERY:
        {
            eventType = RECORD_EVENT_INNER_TYPE_PATH_DETECTION;
            alarmType = RECORD_EVENT_TYPE_PATH_DETECTION_RECOVERY;
        }
        break;

        case SVS_ALARM_CODE_SPEED_DETECTION:
        {
            eventType = RECORD_EVENT_INNER_TYPE_SPEED_DETECTION;
            alarmType = RECORD_EVENT_TYPE_SPEED_DETECTION;
        }
        break;

        case SVS_ALARM_CODE_SPEED_DETECTION_RECOVERY:
        {
            eventType = RECORD_EVENT_INNER_TYPE_SPEED_DETECTION;
            alarmType = RECORD_EVENT_TYPEE_SPEED_DETECTION_RECOVERY;
        }
        break;

       default:
            SVS_LOG((SVS_LM_ERROR,
              "Fail to translate intelligence alarm code, alarm code[%u].",
                alarmCode));
           return;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Translate intelligence alarm code success, alarm code[%u],eventType[%u].",
        alarmCode,eventType));

    return;

}
/*****************************************************************************
 函 数 名  : alarm_event_req
 功能描述  : 处理告警录像事情请求消息
 输入参数  : const ACE_Message_Block *mb
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::alarm_event_req(const ACE_Message_Block *mb)
{
    SVS_TRACE();

    PSVS_ALARM_KINESCOPE_CONTROLREQ_MSG pReq =
        (PSVS_ALARM_KINESCOPE_CONTROLREQ_MSG)(void*)mb->rd_ptr();

    char strAlarmDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0};

    SetLocalizeDeviceId((uint8_t *)strAlarmDeviceId,
        sizeof(strAlarmDeviceId),
        (const char *)pReq->AlarmDeviceId,
        NLS_DEVICE_ID_MAX_LEN);

    char strDevId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0};

    SetLocalizeDeviceId((uint8_t *)strDevId,
        sizeof(strDevId),
        (const char *)pReq->LensId,
        NLS_DEVICE_ID_MAX_LEN);

    SVS_LOG((SVS_LM_INFO,
        "Deal event alarm msg, "
        "cameraID[%s], alarm deviceID[%s], alarm code[0x%04x], control code[0x%04x].",
        strDevId,
        strAlarmDeviceId,
        pReq->AlarmCode,
        pReq->ControlCode));

    if (CHECK_OK != CheckDeviceID(strAlarmDeviceId))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Deal event alarm msg failed, alarm deviceID invalid, "
            "cameraID[%s], alarm deviceID[%s], alarm code[0x%04x], control code[0x%04x].",
            strDevId,
            strAlarmDeviceId,
            pReq->AlarmCode,
            pReq->ControlCode));

        return SVS_RESULT_FAILURE;
    }

    // 转换告警码
    uint32_t eventType = 0;
    uint8_t byType = 0;
    if (SVS_RESULT_OK != alarm_code_translate(pReq->AlarmCode, eventType, byType ))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to deal event alarm message, "
            "cameraID[%s], alarm deviceID[%s], alarm code[0x%04x], control code[0x%04x].",
            strDevId,
            strAlarmDeviceId,
            pReq->AlarmCode,
            pReq->ControlCode));

        return SVS_RESULT_FAILURE;
    }

    //查找镜头对象
    CAC_Record_Inform *pInform = get_device(strDevId);
    if (NULL == pInform)
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Fail to deal event alarm msg as camera not found, "
            "cameraID[%s], alarm device[%s] ,alarm code[0x%04x], control code[0x%04x].",
            strDevId,
            strAlarmDeviceId ,
            pReq->AlarmCode,
            pReq->ControlCode));

        return SVS_RESULT_FAILURE;

    }
    CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pInform);

    // 让inform保存事件记录，如果保存失败，不影响向下的流程
    if(SVS_RESULT_OK != pInform->alarm_event_req(strAlarmDeviceId, eventType, pReq->ControlCode))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Update alarm event info to inform failed."
            "cameraID[%s], alarm deviceID[%s], alarm code[0x%04x], control code[0x%04x].",
            strDevId,
            strAlarmDeviceId,
            pReq->AlarmCode,
            pReq->ControlCode));
    }

    //检查是否有有效的事件录像策略
    if (!pInform->effect_event_stratetegy())
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to deal event alarm message, "
            "because effective record strategy not exist, "
            "cameraID[%s], alarm deviceID[%s], alarm code[0x%04x], control code[0x%04x].",
            strDevId,
            strAlarmDeviceId,
            pReq->AlarmCode,
            pReq->ControlCode));

        return SVS_RESULT_FAILURE;
    }

    int32_t respCode = 0;

    // 开始事件录像
    if ( SVS_DEVICE_CONTROL_REC_START == pReq->ControlCode )
    {
        SVS_LOG((SVS_LM_INFO,
            "Event happend. camera[%s], eventType[0x%04x], alarm deviceID[%s].",
            pInform->get_frontdev_id(),
            eventType,
            strAlarmDeviceId));

        // 判断镜头当前的状态是否可以录像，并将结果透传给镜头管理器
        bool bIsRecord = false;
        if (FROBID_RECORDING != pInform->record_flags())
        {
            bIsRecord = true;
        }

        respCode = CAC_RT_Record_Device_Manager::instance()->event_happen(
                        pInform->get_frontdev_id(),
                        strAlarmDeviceId,
                        eventType,
                        pInform->encoder_type(),
                        pInform->curr_strategy_end_time(),
                        pInform->event_before_record_time(),
                        pInform->event_after_record_time(),
                        bIsRecord);
    }
    else//停止事件录像
    {
        SVS_LOG((SVS_LM_INFO,
            "Event over, camera[%s], eventType[0x%04x], alarm deviceID[%s].",
            pInform->get_frontdev_id(),
            eventType,
            strAlarmDeviceId));

        respCode = CAC_RT_Record_Device_Manager::instance()->event_over(
            pInform->get_frontdev_id(),
            strAlarmDeviceId,
            eventType );
    }

    return respCode;
}

/*****************************************************************************
 函 数 名  : stop_record
 功能描述  : 停止所有录像，并清除所有的镜头对象
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::stop_all_record()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "Begin to stop all camera recording."));

    CAC_Record_Inform *pInfo = NULL;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);

    int32_t retryTime = 0;
    int32_t totalRetryTime = 0;
    uint32_t recording_device_num = CAC_RT_Record_Device_Manager::instance()->get_recording_device_num();
    while (0 < recording_device_num)
    {
        if (SVS_RETRY_STOP_RECORD_TOTAL_TIMES < ++totalRetryTime)
        {
            SVS_LOG((SVS_LM_ERROR, "Fail to stop record. total try times[%d].", totalRetryTime));
            break;
        }

        retryTime = 0;
        SVS_LOG((SVS_LM_WARNING, "Current record camera number %u.", recording_device_num));
        SVS_DEVICE_MAP_ITER iter(device_map_);
        SVS_DEVICE_MAP_ENTRY *pEntry = NULL;

        // 停止所有录像
        for (; 0 != iter.next(pEntry); (void)iter.advance())
        {
            pInfo = pEntry->int_id_;
            if (0 != pInfo->stop_record())
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Fail to stop record. camera[%s].",
                    pInfo->get_frontdev_id()));
            }
        }

        // 等待所有录像结束
        while (0 < recording_device_num)
        {
            recording_device_num = CAC_RT_Record_Device_Manager::instance()->get_recording_device_num();

            SVS_LOG((SVS_LM_DEBUG,
                "There are still [%u] cameras record now.",
                recording_device_num));

            ACE_Time_Value tv( 0, SVS_CHECK_STOP_RECORD_INTERVAL_TIME );
            (void)ACE_OS::sleep( tv );

            // 尝试停止SVS_RETRY_STOP_RECORD_TIMES次
            if (SVS_RETRY_STOP_RECORD_TIMES < retryTime++)
            {
                break;
            }
        }
    }

    //删除所有录像对象
    SVS_LOG((SVS_LM_WARNING, "Current camera number %u.", device_map_.current_size()));
    SVS_DEVICE_MAP_ITER iter(device_map_);
    SVS_DEVICE_MAP_ENTRY *pEntry = NULL;
    std::list<CAC_Record_Inform *>      infoList;

    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pInfo = pEntry->int_id_;
        if(SVS_RESULT_OK == pInfo->stop_inform())
        {
            infoList.push_back(pInfo);
        }
    }

    while (!infoList.empty())
    {
        pInfo = infoList.front();
        infoList.pop_front();
        if ((SVS_RESULT_OK == remove_device(pInfo->internal_id()))
              && (SVS_RESULT_OK == remove_device(pInfo->get_frontdev_id())))
        {
            SVS_LOG((SVS_LM_INFO,
                "Success to delete camera. cameraID[%s].",
                pInfo->get_frontdev_id()));
            SVS_DELETE( pInfo );
        }
    }

    if(0 == device_map_.current_size())
    {
        SVS_LOG((SVS_LM_INFO, "Infom map size is 0,so clear map."));
        (void)request_map_.unbind_all();
        (void)device_map_.unbind_all();
    }

    // 置当前状态为查询设备状态
    state_ = FDM_STATE_QUERY_DEVICE;
    SVS_LOG((SVS_LM_WARNING, "Success to stop all cameras."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : check_strategy_timer_callback
 功能描述  : 检查策略定时器回调函数
 输入参数  : void *pArg,
             int32_t eventId:事件ID
             int32_t timerId:内部定时器ID
             int32_t
 输出参数  : 无
 返 回 值  : 无
 修改历史  :
*****************************************************************************/
void CAC_Record_Inform_Manager::check_strategy_timer_callback(void *pArg,
                                                              int32_t   eventId,
                                                              int32_t  timerId,
                                                              int32_t /*nDummy*/ )
{
    SVS_TRACE();

    uint32_t msgLen = sizeof(SVS_TIMER_CALL_BACK_MSG);

    ACE_Message_Block *mb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK, msgLen );

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to allocate message block. msg type[%d], block size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK,
            msgLen));

        return;
    }

    SVS_TIMER_CALL_BACK_MSG* pMsg = (SVS_TIMER_CALL_BACK_MSG*)(void*)mb->wr_ptr();
    pMsg->MsgType = SVS_TIMER_CALL_BACK_MSG::RECORD_INFORM_MANAGER_TYPE;
    pMsg->ProcObject = pArg;
    pMsg->EventId = eventId;
    pMsg->TimerId = timerId;
    mb->wr_ptr( msgLen );

    // 将回调消息添加到定时器回调处理消息队列中
    (void)CAC_RT_Record_Server::instance()->add_timer_callback_msg( mb );
}

/*****************************************************************************
 函 数 名  : debug_get_all_lens_info
 功能描述  : 调试状态下，获取所有镜头信息
 输入参数  : const uint32_t szLensInfo:信息总长度
 输出参数  : char *strLensInfo:包含所有镜头调试信息的指针
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::debug_get_all_lens_info(  char *strLensInfo, const uint32_t szLensInfo )
{
    (void)ACE_OS::snprintf(   strLensInfo,
                        szLensInfo,
                        "\t       CameraID    RecordType Status CameraStatus\n" );

    CAC_Record_Inform *pDevice = NULL;

    ACE_GUARD_RETURN(   ACE_Recursive_Thread_Mutex,
                        guard,
                        map_mutex_,
                        -1 );

    SVS_DEVICE_MAP_ITER iter(device_map_);
    SVS_DEVICE_MAP_ENTRY *pEntry = NULL;
    size_t writeLen = 0;
    int32_t lensCount = 0;
    uint32_t uForbidRecordCameraNum     = 0;
    uint32_t uAlwaysRecordCameraNum     = 0;
    uint32_t uEventRecordCameraNum      = 0;
    uint32_t uManualRecordCameraNum     = 0;
    uint32_t uNoStrategyCameraNum       = 0;
    uint8_t uRecordType               = 0;

    int32_t bufLen = 0;
    for (; 0 != iter.next(pEntry); (void)iter.advance() )
    {
        pDevice = pEntry->int_id_;
        writeLen = ACE_OS::strlen( strLensInfo );

        //缓冲区已用完，退出循环
        bufLen = (int32_t)(szLensInfo-writeLen);
        if( 0 >= bufLen )
        {
            break;
        }
        uRecordType = pDevice->record_flags();
       (void)ACE_OS::snprintf(   &strLensInfo[writeLen],
                            (uint32_t)bufLen,
                            "\t%s %5d       %2d      %2d\n",
                            pDevice->get_frontdev_id(),
                            uRecordType,
                            pDevice->state(),
                            pDevice->lens_state() );

        ++lensCount;

        // 各种策略计数
        switch(uRecordType)
        {
            // 禁止录像
            case FROBID_RECORDING:
            {
                ++uForbidRecordCameraNum;
                break;
            }

            // 连续录像
            case ALWAYS_RECORDING:
            {
                ++uAlwaysRecordCameraNum;
                break;
            }

            // 事件录像
            case RECORDING_ON_EVENT:
            {
                ++uEventRecordCameraNum;
                break;
            }

            // 手动录像
            case RECORDING_ON_MANUAL:
            {
                ++uManualRecordCameraNum;
                break;
            }

            // 当前没策略
            default:
            {
                ++uNoStrategyCameraNum;
                break;
            }

        }
    }
    writeLen = ACE_OS::strlen( strLensInfo );
    bufLen = (int32_t)(szLensInfo-writeLen);
    if( 0 < bufLen )
    {
        (void)ACE_OS::snprintf(   &strLensInfo[writeLen],
                            (uint32_t)bufLen,
                            "\n\t  Total Camera                    [%5d]"
                            "\n"
                            "\n\t  ForbidRecordCameraNum           [%5d]"
                            "\n\t  AlwaysRecordCameraNum           [%5d]"
                            "\n\t  EventRecordCameraNum            [%5d]"
                            "\n\t  ManualRecordCameraNum           [%5d]"
                            "\n\t  NoStrategyCameraNum             [%5d]"
                            "\n\n",
                            lensCount,
                            uForbidRecordCameraNum,
                            uAlwaysRecordCameraNum,
                            uEventRecordCameraNum,
                            uManualRecordCameraNum,
                            uNoStrategyCameraNum);
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : update_record_parameter
 功能描述  : 根据传入的参数更新设备录像参数
 输入参数  : mb :   更新设备录像参数消息指针
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::update_record_parameter(const ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_CLIENTSVR_SETDEVICERECORDERPARAM_REQ_MSG *pReq =
        (SVS_CLIENTSVR_SETDEVICERECORDERPARAM_REQ_MSG *)(void*)mb->rd_ptr();

    char strLensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = { 0 };

    SetLocalizeDeviceId((uint8_t *)strLensId,
        sizeof(strLensId),
        (const char *)pReq->DeviceID,
        NLS_DEVICE_ID_MAX_LEN);

    SVS_LOG((SVS_LM_INFO,
        "Begin to update record parameter, "
        "cameraID[%s], "
        "before event record time[%d], "
        "after event record time[%d].",
        strLensId,
        pReq->BeforeSeconds,
        pReq->AfterSeconds));

    SVS_LENS_PARAMETER lensParam;
    lensParam.RecordStoreDays = (uint32_t)pReq->StoreDays;
    lensParam.BeforeSeconds = pReq->BeforeSeconds;
    lensParam.AfterSeconds = pReq->AfterSeconds;

    //先内存维护的更新静态参数
    (void)update_lens_param(strLensId, lensParam);

    //查找指定的镜头对象,并更新镜头对象中的预录与后录时长
    CAC_Record_Inform *pInform = get_device(strLensId);

    if ( NULL == pInform )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to update record parameter, "
            "no camera was found, "
            "camera[%s].",
            strLensId));

        return SVS_RESULT_FAILURE;
    }

    CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pInform);
    int32_t updateResult = pInform->update_record_parameter(pReq->BeforeSeconds, pReq->AfterSeconds );

    SVS_LOG((SVS_LM_INFO,
        "End to update record parameter, "
        "camera[%s], result[%d].",
        strLensId,
        updateResult));

    return updateResult;
}

/*****************************************************************************
 函 数 名  : handle_record_task
 功能描述  : 设备状态变更处理
 输入参数  : const ACE_Message_Block *mb:更新设备状态消息指针
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::handle_record_task(const ACE_Message_Block *mb)
{
    SVS_TRACE();

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to deal PLATDEV_INFORM_REQ message, because the pointer mb is NULL."));

        return SVS_RESULT_FAILURE;
    }

    SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg =
        (SVS_DEVICE_DISTRIBUTE_REQ_MSG*)(void*)mb->rd_ptr();

    uint16_t unCheckLen = (uint16_t)offsetof(SVS_DEVICE_DISTRIBUTE_REQ_MSG, DeviceDistributeInfo)
            + (uint16_t)offsetof(DEVICE_DISTRIBUTE_INFO, RecordSvrStrategyInfo);

    // 检验消息长度是否正确
    if (pReqMsg->Header.PacketLength < unCheckLen)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to deal device record distribute request message, because message len is invalid."
            "packetLen[%d], minLen[%d].",
            pReqMsg->Header.PacketLength, unCheckLen));
//1 TODO 这类消息错误，统一处理，建议断开连接

        return SVS_RESULT_FAILURE;
    }

    // 实际应用中不会出现在同一个接管请求消息中携带多个镜头的情况
    if (1 != pReqMsg->DeviceCount)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Not support distribute task take more than one camera, cameraNum[%d].",
            pReqMsg->DeviceCount));

        return SVS_RESULT_FAILURE;
    }

    // 如果是镜头录像启动消息
    if(DISTRIBUTE_TYPE_DISTRIBUTED == pReqMsg->DistributeType)
    {
        return start_record(pReqMsg);
    }
    else if(DISTRIBUTE_TYPE_CANCEL == pReqMsg->DistributeType) // 如果是镜头录像停止消息
    {
        return stop_record(pReqMsg);
    }
    else // 其它类型为错误，返回失败效应给Storesvr
    {
        SVS_LOG((SVS_LM_WARNING,
            "Handle record distribute task failed. distribute Type invalid[%u]",
            pReqMsg->DistributeType));

        char    szDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0};
        SetLocalizeDeviceId((uint8_t *)szDeviceId,
                    sizeof(szDeviceId),
                    (const char *)pReqMsg->DeviceDistributeInfo[0].DeviceID,
                    NLS_DEVICE_ID_MAX_LEN);

        //发送响应消息
        //明确发送的错误码为接管消息中的类型不正确
        send_distribute_reponse(pReqMsg->Header.TransactionNo,
                    pReqMsg->DistributeType,
                    szDeviceId,
                    SVS_RESPCODE_LENS_DISTRIBUTE_TYPE_INVALID);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : start_record
 功能描述  : 接管StoreSvr分发过来的镜头录像。创建inform对象，查询录像策略，如果
             当前有策略则进行录像，否则等待录像
 输入参数  : SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg:分发内容，将要接管的镜头列表
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::start_record(const SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg)
{
    //如果和StoreSvr断连了，则不处理该消息
    if(!CAC_RT_Record_Server::instance()->is_store_svr_conn())
    {
        SVS_LOG((SVS_LM_WARNING, "Disconnect from storeSvr, so can not start record."));

        return SVS_RESULT_OK;
    }

    char strDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0,};
    CAC_Record_Inform *pInform = NULL;

    SetLocalizeDeviceId((uint8_t *)strDeviceId,
        sizeof(strDeviceId),
        (const char *)pReqMsg->DeviceDistributeInfo[0].DeviceID,
        NLS_DEVICE_ID_MAX_LEN);

    // 从消息体中解析出镜头的录像策略列表，允许录像策略为空
    int32_t iResult = check_device_strategy(pReqMsg);

    // 在消息体中携带的录像策略如果不正确，发送错误响应
    if (SVS_RESULT_OK != iResult)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Send distribute task error response via strategy invalid, camera[%s].",
            strDeviceId));

        send_distribute_reponse(pReqMsg->Header.TransactionNo,
                pReqMsg->DistributeType, strDeviceId,
                SVS_RESPCODE_LENS_STRATEGY_INVALID);

        return SVS_RESULT_OK;
    }

//    //即使没有策略也要创建inform对象进行管理
//    if (0 == stratVector.size())
//    {
//        SVS_LOG((SVS_LM_INFO, "Camera's strategy number is 0 at today, CameraId[%s]", strDeviceId));
//    }

    //更新镜头参数信息
    SVS_LENS_PARAMETER lensParam;
    lensParam.RecordStoreDays = (uint32_t)pReqMsg->DeviceDistributeInfo[0].StoreDays;
    lensParam.BeforeSeconds = pReqMsg->DeviceDistributeInfo[0].BeforeSeconds;
    lensParam.AfterSeconds = pReqMsg->DeviceDistributeInfo[0].AfterSeconds;
    (void)update_lens_param(strDeviceId, lensParam);

    //增加镜头对象
    int32_t addResult = add_device( strDeviceId,
                        (int32_t)pReqMsg->DeviceDistributeInfo[0].EncoderType,
                        pInform);

    if (SVS_RESULT_OK != addResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to create camera. camera[%s].", strDeviceId));

        send_distribute_reponse(pReqMsg->Header.TransactionNo,
                pReqMsg->DistributeType, strDeviceId,
                SVS_RESPCODE_LENS_DISTRIBUTE_INNER_ERROR);

        return SVS_RESULT_OK;
    }

    CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pInform);

    //初始化镜头
    int32_t initLensResult = pInform->init_lens();

    if (SVS_RESULT_OK != initLensResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to initialize camera. camera[%s].",strDeviceId));

        //lensInfo.RespCode = SVS_RESPCODE_OTHER_ERROR;
        //lensInfoVect.push_back(lensInfo);
        // 新增错误码，表示内部处理过程中的问题，比如对象分配等
        send_distribute_reponse(pReqMsg->Header.TransactionNo,
                pReqMsg->DistributeType, strDeviceId,
                SVS_RESPCODE_LENS_DISTRIBUTE_INNER_ERROR);

        return SVS_RESULT_OK;
    }

//    //初始化镜头录像策略列表
//    bool initResult = pInform->init_strate(stratVector);
//
//    if (!initResult)
//    {
//        (void)pInform->off_line();
//        SVS_LOG((SVS_LM_ERROR, "Fail to initialize record strategy. camera[%s].", strDeviceId));
//
//        //lensInfo.RespCode = SVS_RESPCODE_OTHER_ERROR;
//        //lensInfoVect.push_back(lensInfo);
//
//        send_distribute_reponse(pReqMsg->Header.TransactionNo,
//                pReqMsg->DistributeType, strDeviceId,
//                SVS_RESPCODE_LENS_DISTRIBUTE_INNER_ERROR);
//
//        return SVS_RESULT_OK;
//    }
//
//    //执行镜头录像策略
//    (void)pInform->exec_strategy();
//
//    //接管成功
//    //lensInfo.RespCode = SVS_RESPCODE_SUCCESS;
//    //lensInfoVect.push_back(lensInfo);
//
//    SVS_LOG((SVS_LM_DEBUG,
//        "Handle lens distribute task success, cameraId[%s]",
//        strDeviceId));
//    //}
//
//    state_ = FDM_STATE_CHECK_STRATEGY;

    // 创建策略更新消息
    uint32_t uStrategyCount = pReqMsg->DeviceDistributeInfo[0].RecordStrategyCount;
    uint32_t ulStrategyMsgLen = sizeof(SVS_STORESVR_RENEW_STRATEGY_REQ_MSG)
                            + (uStrategyCount - 1) * sizeof(RECORD_STRATEGY_INFO);
    ACE_Message_Block * mb = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_NORMAL_EVENT ,ulStrategyMsgLen);

    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to tokeover camera as allocate message block for stragety failed."
            "cameraId[%s], uStrategyCount[%u], ulStrategyMsgLen[%u].",
            strDeviceId,
            uStrategyCount,
            ulStrategyMsgLen));

        send_distribute_reponse(pReqMsg->Header.TransactionNo,
                pReqMsg->DistributeType, strDeviceId,
                SVS_RESPCODE_LENS_DISTRIBUTE_INNER_ERROR);

        return SVS_RESULT_OK;
    }
    mb->wr_ptr(ulStrategyMsgLen);

    // 构造消息内容
    SVS_STORESVR_RENEW_STRATEGY_REQ_MSG *pStrategyMsg = (SVS_STORESVR_RENEW_STRATEGY_REQ_MSG*)(void*)mb->rd_ptr();
    memset(pStrategyMsg, 0, ulStrategyMsgLen);

    pStrategyMsg->DeviceCount = 1;
    DEVICE_STRATEGY_INFO* pDeviceStragety = pStrategyMsg->DeviceStrategyInfo;
    pDeviceStragety->RecordStrategyCount = (int32_t)uStrategyCount;
    memcpy(pDeviceStragety->DeviceID, strDeviceId, sizeof(pDeviceStragety->DeviceID));
    uint32_t uStrategyInfoLen = uStrategyCount * sizeof(RECORD_STRATEGY_INFO);
    memcpy(pDeviceStragety->RecordSvrStrategyInfo,
           pReqMsg->DeviceDistributeInfo[0].RecordSvrStrategyInfo,
           uStrategyInfoLen);

    FillCommonHeader(pStrategyMsg->Header,
            SVS_MSG_TYPE_STORESVR_RENEW_STRATEGY_REQ,
            0,
            (uint16_t)ulStrategyMsgLen);

    // 把策略更新消息分到策略更新队列中
    ACE_Time_Value tv(ACE_OS::gettimeofday());
    tv.set(tv.sec(), tv.usec() + SVS_ENQUEUE_TIMEOUT);
    if(-1 == CAC_RT_Record_Server::instance()->enqueue_record_strategy(mb, &tv))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to tokeover camera as enqueue record stragety failed."
            "cameraId[%s], uStrategyCount[%u], ulStrategyMsgLen[%u].",
            strDeviceId,
            uStrategyCount,
            ulStrategyMsgLen));
        g_p_msg_buffer->free_mb(mb);

        send_distribute_reponse(pReqMsg->Header.TransactionNo,
                pReqMsg->DistributeType, strDeviceId,
                SVS_RESPCODE_LENS_DISTRIBUTE_INNER_ERROR);
    }

    //发送响应消息
    send_distribute_reponse(pReqMsg->Header.TransactionNo,
            pReqMsg->DistributeType,
            strDeviceId,
            SVS_RESPCODE_SUCCESS);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : stop_record
 功能描述  : 取消接管StoreSvr分发过来的镜头录像。如果镜头不在录像，则不处理该镜头，
             如果镜头正在录像，则停止该镜头录像
 输入参数  : SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg:分发内容，将要取消接管的镜头列表
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::stop_record(const SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg)
{
//    uint32_t lensNum = pReqMsg->DeviceCount;
//
//    SVS_LOG((SVS_LM_DEBUG,
//        "Handle record distribute task.Begin to stop record.cameraNum[%d]",
//        lensNum));
//    DISTRIBUTELENSINFO_VECTOR lensInfoVect;
//    lensInfoVect.clear();
//    DistributeLensInfo lensInfo;
    char strDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0,};
    CAC_Record_Inform *pInform = NULL;

//    for (uint32_t i = 0; i < lensNum; i++)
//    {
//        memcpy(&lensInfo.DistributeInfo,&pReqMsg->DeviceDistributeInfo[i],sizeof(lensInfo.DistributeInfo));
//
//        // 查询镜头策略列表
//        STRAT_VECTOR stratVector;

    SetLocalizeDeviceId((uint8_t *)strDeviceId,
                sizeof(strDeviceId),
                (const char *)pReqMsg->DeviceDistributeInfo[0].DeviceID,
                NLS_DEVICE_ID_MAX_LEN);

    // 查找镜头对象，如果查找到后，让其下线
    pInform = get_device(strDeviceId);

    // 如果没有找到设备，也算作停止成功
    if (NULL == pInform)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera fail to offline as no camera object found. camera[%s].",
            strDeviceId));

        //lensInfo.RespCode = SVS_RESPCODE_SUCCESS;
        //lensInfoVect.push_back(lensInfo);
        send_distribute_reponse(pReqMsg->Header.TransactionNo,
                    pReqMsg->DistributeType,
                    strDeviceId,
                    SVS_RESPCODE_SUCCESS);

        return SVS_RESULT_OK;
    }

    // 镜头下线
    CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pInform);

    // 设置镜头状态，并处理镜头下线
    (void)pInform->off_line();

    (void)CAC_RT_Record_Device_Manager::instance()->device_off_line( strDeviceId );
    //    lensInfo.RespCode = SVS_RESPCODE_SUCCESS;

    //    lensInfoVect.push_back(lensInfo);
    //}

    // 发送响应消息
    //send_distribute_reponse(pReqMsg->Header.TransactionNo,pReqMsg->DistributeType, lensInfoVect);
    send_distribute_reponse(pReqMsg->Header.TransactionNo,
                    pReqMsg->DistributeType,
                    strDeviceId,
                    SVS_RESPCODE_SUCCESS);
    //lensInfoVect.clear();

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : send_distribute_reponse
 功能描述  : 向StoreSvr发送分发响应消息;
 输入参数  : uint16_t msgNo:消息传输号
             uint32_t distributeType:分发类型，0接管命令，1取消接管命令
             LENSINFO_VECTOR &lensVect:镜头列表
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
*****************************************************************************/
void CAC_Record_Inform_Manager::send_distribute_reponse(uint16_t msgNo ,
                                                              uint32_t distributeType,
                                                              //DISTRIBUTELENSINFO_VECTOR &lensVect
                                                              const char *pszLensId,
                                                              uint32_t  uiResponseCode)const
{
    // 申请缓存区，可变长度，采取最大长度
    uint32_t uiBuffSize = sizeof(SVS_DEVICE_DISTRIBUTE_RESP_MSG);

    ACE_Message_Block* mb = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, uiBuffSize);

    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Send record distribute reponse message failed.Allocate message block failed."));
        return ;
    }

    SVS_DEVICE_DISTRIBUTE_RESP_MSG* rspMsg = (SVS_DEVICE_DISTRIBUTE_RESP_MSG*)(void*)mb->wr_ptr();
    //uint32_t lensNum = lensVect.size();
    //uint32_t msgLen = sizeof(SVS_DEVICE_DISTRIBUTE_RESP_MSG) + ((lensNum - 1)* sizeof(DEVICE_DISTRIBUTE_RESULT));
    mb->wr_ptr( uiBuffSize );
    (void)ACE_OS::memset(rspMsg, 0, uiBuffSize);

    // 封装消息头
    FillCommonHeader(   &rspMsg->Header,
                        SVS_MSG_TYPE_DEVICE_DISTRIBUTE_RESP,
                        transactionno_respond(msgNo),
                        (uint16_t)uiBuffSize);
    rspMsg->RespCode = 0;
    rspMsg->DistributeType = distributeType;
    //rspMsg->DeviceCount = lensNum;
    rspMsg->DeviceCount = 1;

//    DistributeLensInfo *pLensInfo;
//    for(uint32_t i = 0; i < lensNum; i++)
//    {
//        pLensInfo = &lensVect[i];
//
//        SetNLSDeviceId(rspMsg->DevDistributeResult[i].DeviceID,
//            sizeof(rspMsg->DevDistributeResult[i].DeviceID),
//            (const char *)pLensInfo->DistributeInfo.DeviceID,
//            NLS_DEVICE_ID_MAX_LEN);
//
//        rspMsg->DevDistributeResult[i].RespCode = (int32_t)pLensInfo->RespCode;
//    }

    SetNLSDeviceId(rspMsg->DevDistributeResult[0].DeviceID,
        sizeof(rspMsg->DevDistributeResult[0].DeviceID),
        (const char *)pszLensId,
        NLS_DEVICE_ID_MAX_LEN);
    rspMsg->DevDistributeResult[0].RespCode = (int32_t)uiResponseCode;

    int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( mb );

    if ( SVS_RESULT_OK != sendResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Message enqueue failed.msgtype[0x%04x], TransactionNo[0x%04x],returncode[%d].",
            rspMsg->Header.MsgType,
            rspMsg->Header.TransactionNo,
            sendResult ));

        g_p_msg_buffer->free_mb( mb );
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Send distribute reponse to queue successed, cameraId[%s], msgNo[%u], respCode[%u].",
        pszLensId, msgNo, uiResponseCode));

    return;
}

/*****************************************************************************
 函 数 名  : init_lens_param
 功能描述  : 初始化所有和本NRU相关联的镜头的参数
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功,返回 0 否则返回 -1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::init_lens_param()
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lens_parameter_map_mutex_, SVS_RESULT_FAILURE);

    // 初始化
    (void)lens_parameter_map_.unbind_all();

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : get_lens_param
 功能描述  : 获取指定镜头的录像参数
 输入参数  : const char* strLensId:镜头ID
 输出参数  : SVS_LENS_PARAMETER& lensParameter:镜头录像参数信息
 返 回 值  : 成功,返回 0 否则返回 -1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::get_lens_param(const char* strLensId,
                                        SVS_LENS_PARAMETER& lensParameter)
{
    SVS_TRACE();

    SVS_LENS_PARAMETER pLensParam;

    // 尝试在当前map中查找指定镜头的参数
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lens_parameter_map_mutex_, SVS_RESULT_FAILURE);

    //如果从内存中找不到镜头的录像参数，不再从数据库中获取
    if (0 == lens_parameter_map_.find(ACE_CString(strLensId),pLensParam))
    {
        lensParameter = pLensParam;

        return SVS_RESULT_OK;
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get lens's record parameter failed, lens parameter not exists, camera[%s].",
            strLensId));

        return SVS_RESULT_FAILURE;
    }
}

/*****************************************************************************
 函 数 名  : update_lens_param
 功能描述  : 更新指定镜头的参数,如果参数bool isDelLens为true,
             则把镜头参数从map中删除
 输入参数  : char * strLensId:镜头ID
             SVS_LENS_PARAMETER lensParameter:更新内容
             bool isDelLens:是否为删除镜头
 输出参数  : NA
 返 回 值  : 成功,返回 0 否则返回 -1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::update_lens_param(const char * strLensId,
                                            const SVS_LENS_PARAMETER &lensParameter,
                                            bool isDelLens )
{
    SVS_TRACE();

    if (NULL == strLensId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to get camera parameter as the pointer strLensId is NULL."));

        return SVS_RESULT_FAILURE;
    }

    char tempLensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0,};
    SVS_LENS_PARAMETER tempLensParam = lensParameter;

    SetLocalizeDeviceId((uint8_t *)tempLensId,
        sizeof(tempLensId),
        strLensId);

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lens_parameter_map_mutex_, SVS_RESULT_FAILURE);

    // 如果是删除镜头
    if (isDelLens)
    {
        return SVS_RESULT_OK;
    }

    (void)lens_parameter_map_.rebind(ACE_CString(tempLensId), tempLensParam);

    return SVS_RESULT_OK;
}

//int32_t CAC_Record_Inform_Manager::start_client_record(const char* strClientId)
//{
//    SVS_TRACE();
//
//    if(NULL == strClientId)
//    {
//        SVS_LOG((SVS_LM_ERROR,
//            "Start client recording failed. The parameter strClientId is NULL."));
//        return SVS_RESULT_FAILURE;
//    }
//
//    LENSINFO_VECTOR lensVect;
//    lensVect.clear();
//    // 查询该客户的所有镜头
//    if(SVS_RESULT_OK != query_client_all_lens(strClientId, lensVect))
//    {
//        SVS_LOG((SVS_LM_ERROR,
//            "Start client recording failed. query client's all cameras failed."));
//        return SVS_RESULT_FAILURE;
//    }
//
//    uint32_t lensNum = lensVect.size();
//    for(uint32_t i = 0; i < lensNum; i++)
//    {
//        // 根据镜头id获取镜头对象指针，如果镜头不存在，则创建
//        CAC_Record_Inform *pInform = get_device(lensVect[i].LensId);
//        if (NULL != pInform)
//        {
//            CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pInform);
//        }
//        else
//        {
//            STRAT_VECTOR stratVector;
//            if (SVS_RESULT_OK != query_device_strategy(lensVect[i].LensId, stratVector))
//            {
//                continue;
//            }
//
//            if (SVS_RESULT_OK == stratVector.size())
//            {
//                SVS_LOG((SVS_LM_WARNING,
//                    "Camera has no record strategy. camera[%s].",
//                    lensVect[i].LensId));
//                continue;
//            }
//
//            int32_t addResult = add_device(lensVect[i].LensId, lensVect[i].EncoderType, pInform);
//            if (SVS_RESULT_OK != addResult)
//            {
//                SVS_LOG((SVS_LM_ERROR,
//                    "Fail to create camera. camera[%s], return value[%d].",
//                    lensVect[i].LensId,
//                    addResult));
//                continue;
//            }
//
//            CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pInform);
//
//            // 初始化镜头
//            int32_t initLensResult = pInform->init_lens();
//            if (SVS_RESULT_OK != initLensResult)
//            {
//                SVS_LOG((SVS_LM_ERROR,
//                    "Fail to initialize camera. cameraID[%s].",
//                    lensVect[i].LensId));
//                continue;
//            }
//
//            // 初始镜头录像策略列表
//            bool initResult = pInform->init_strate(stratVector);
//            if ( !initResult )
//            {
//                SVS_LOG((SVS_LM_ERROR,
//                    "Fail to initialize record strategy. cameraID[%s].",
//                    lensVect[i].LensId));
//                continue;
//            }
//
//            (void)pInform->exec_strategy();
//        }
//    }
//    return SVS_RESULT_OK;
//}
//
//int32_t CAC_Record_Inform_Manager::stop_client_record(const char* strClientId)
//{
//    SVS_TRACE();
//
//    if(NULL == strClientId)
//    {
//        SVS_LOG((SVS_LM_ERROR,
//            "stop client recording failed. The parameter strClientId is NULL."));
//        return SVS_RESULT_FAILURE;
//    }
//
//    LENSINFO_VECTOR lensVect;
//    lensVect.clear();
//    // 查询该客户的所有镜头
//    if(SVS_RESULT_OK != query_client_all_lens(strClientId, lensVect))
//    {
//        SVS_LOG((SVS_LM_ERROR,
//            "stop client recording failed. query all this client's cameras failed."
//            "clientId[%s]",
//            strClientId));
//        return SVS_RESULT_FAILURE;
//    }
//
//    uint32_t lensNum = lensVect.size();
//    SVS_LOG((SVS_LM_WARNING,
//        "Begin to stop all this client's cameras recording.clientId[%s],cameras number[%u]",
//        strClientId,
//        lensNum));
//    for(uint32_t i = 0; i < lensNum; i++)
//    {
//        // 根据镜头id获取镜头对象指针，如果镜头存在，则停止录像
//        CAC_Record_Inform *pInform = get_device(lensVect[i].LensId);
//        if (NULL != pInform)
//        {
//            CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pInform);
//            (void)pInform->lens_status(SVS_MSG_FRONT_INFORM_STOP);
//            (void)pInform->off_line();
//            (void)CAC_RT_Record_Device_Manager::instance()->device_off_line( lensVect[i].LensId );
//        }
//
//        (void)CAC_Bakup_Record_Dev_Manager::instance()->stop_device(lensVect[i].LensId);
//    }
//
//    return SVS_RESULT_OK;
//}
//
//int32_t CAC_Record_Inform_Manager::query_client_all_lens(const char *strClientId,
//                                                  LENSINFO_VECTOR &lensVector )
//{
//    SVS_TRACE();
//
//    DTLensInfoList stLensList;
//
//    int32_t iRet = DBAPI::instance()->ReadLensInfo(SVS_ID_TYPE_CLIENT,
//                                            strClientId,
//                                            stLensList);
//
//    if (DBAPI_RETCODE_SUCCESS != iRet)
//    {
//        SVS_LOG((SVS_LM_ERROR,
//            "Query client all lens list failed, ret[%d].",
//            iRet));
//
//        return SVS_RESULT_FAILURE;
//    }
//
//    std::list<DTLensInfo>::iterator iter = stLensList.begin();
//
//    for (; iter != stLensList.end(); iter++)
//    {
//        LocalLensInfo lf;
//        memset(&lf, 0x0, sizeof(lf));
//
//        //(void)ACE_OS::memcpy(lf.LensId, iter->szDevId, DEVICEID_LEN);
//        SetLocalizeDeviceId((uint8_t *)lf.LensId,
//            sizeof(lf.LensId),
//            iter->szDevId);
//
//        lensVector.push_back(lf);
//    }
//
//    SVS_LOG((SVS_LM_INFO,
//        "Success to query client all cameras. Its cameras num is [%d].",
//        lensVector.size()));
//
//    return SVS_RESULT_OK;
//}

/*****************************************************************************
 函 数 名  : check_device_strategy
 功能描述  : 检查策略合法性
 输入参数  : pReqMsg:请求消息
 输出参数  : stratVector:录像策略列表
 返 回 值  : 成功,返回 0 否则返回 -1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::check_device_strategy(const SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg) const
{
    SVS_TRACE();

    if (NULL == pReqMsg)
    {
        SVS_LOG((SVS_LM_WARNING, "Request message is NULL."));
        return SVS_RESULT_FAILURE;
    }

    // 对变长的部分进行长度的校验
    const DEVICE_DISTRIBUTE_INFO *pDistriInfo = &pReqMsg->DeviceDistributeInfo[0];
    int32_t iDistriInfoLen = pReqMsg->Header.PacketLength -
                offsetof(SVS_DEVICE_DISTRIBUTE_REQ_MSG, DeviceDistributeInfo);
    if (iDistriInfoLen < 0)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Request message length invalid, msgLen[%d],minLen[%d].",
            pReqMsg->Header.PacketLength,
            offsetof(SVS_DEVICE_DISTRIBUTE_REQ_MSG, DeviceDistributeInfo)));
        return SVS_RESULT_FAILURE;
    }

    // 对镜头信息的固定部分长度进行校验
    int32_t iStrategyLen = iDistriInfoLen -
                (int32_t)offsetof(DEVICE_DISTRIBUTE_INFO, RecordSvrStrategyInfo);
    if (iStrategyLen < 0)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Device distribute info length invalid, infoLen[%d], minLen[%d].",
            iDistriInfoLen,
            offsetof(DEVICE_DISTRIBUTE_INFO, RecordSvrStrategyInfo)));
        return SVS_RESULT_FAILURE;
    }

    // 对镜头策略的变长部分进行校验
    uint32_t uiStrategyLen = pDistriInfo->RecordStrategyCount * sizeof(RECORD_STRATEGY_INFO);
    if ((uint32_t)iStrategyLen != uiStrategyLen)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Device strategy info length invalid, bufLen[%d], stratCount[%d], stratLen[%d].",
            iStrategyLen, pDistriInfo->RecordStrategyCount, uiStrategyLen));
        return SVS_RESULT_FAILURE;
    }

//    // 将策略转换为内存结构体，并同时校验有效性
//    if (SVS_RESULT_OK != fill_strategy_list(pDistriInfo->RecordStrategyCount,
//                &pDistriInfo->RecordSvrStrategyInfo[0],
//                stratVector))
//    {
//        SVS_LOG((SVS_LM_WARNING, "Fetch strategy list from message failed."));
//        stratVector.clear();
//        return SVS_RESULT_FAILURE;
//    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : fill_strategy_list
 功能描述  : 从RECORD_STRATEGY_INFO结构体中获取策略转换为STRAT_VECTOR
 输入参数  : uiStrategyCount:策略个数
             pStrategyInfo:策略结构体起始地址
 输出参数  : stratVector:录像策略列表
 返 回 值  : 成功,返回 0 否则返回 -1
 修改历史  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::fill_strategy_list(uint32_t uiStrategyCount,
                                const RECORD_STRATEGY_INFO *pStrategyInfo,
                                STRAT_VECTOR &stratVector) const
{
    if (pStrategyInfo == NULL)
    {
        SVS_LOG((SVS_LM_WARNING, "Strategy info point is NULL."));
        return SVS_RESULT_FAILURE;
    }

    for (uint32_t i = 0; i < uiStrategyCount; i++)
    {
        // 策略结束时间不能小于策略开始时间
        if (pStrategyInfo[i].StartTime >= pStrategyInfo[i].EndTime)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Strategy starttime less than endtime, startime[%u], endtime[%u].",
                pStrategyInfo[i].StartTime, pStrategyInfo[i].EndTime));
            return SVS_RESULT_FAILURE;
        }
        // 录像策略类型有效性检查
        if (pStrategyInfo[i].TriggerType > SVS_RECORDER_MANUAL
            || pStrategyInfo[i].TriggerType < SVS_RECORDER_DENY)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Strategy type invalid, strategyType[%d], valid[%d-%d].",
                pStrategyInfo[i].StrategyType,
                SVS_RECORDER_DENY, SVS_RECORDER_MANUAL));
            return SVS_RESULT_FAILURE;
        }
        // 星期标志由8个二进制位表示，不能大于0xFF
        if (pStrategyInfo[i].WeekFlag > 0xFF)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Strategy weekflag invalid, weekflag[0x%x].",
                pStrategyInfo[i].WeekFlag));
            return SVS_RESULT_FAILURE;
        }

        STRAT_PROPERTY  stratInfo;
        memset(&stratInfo, 0x0, sizeof(stratInfo));
        stratInfo.stratFlag = (uint8_t)pStrategyInfo[i].StrategyType;
        stratInfo.triggerFlag = (uint8_t)pStrategyInfo[i].TriggerType;
        stratInfo.startTime = (time_t)pStrategyInfo[i].StartTime;
        stratInfo.endTime = (time_t)pStrategyInfo[i].EndTime;
        stratInfo.weekFlag = (uint8_t)pStrategyInfo[i].WeekFlag;
        stratInfo.recordStatus = (uint8_t)0x01;
        stratVector.push_back(stratInfo);

        char szStartTime[SVS_STR_TIME_MAX_LEN] = {0,};
        char szEndTime[SVS_STR_TIME_MAX_LEN] = {0,};
        (void)SVS_SS_UTILITIES::time2str(szStartTime, SVS_STR_TIME_MAX_LEN, stratInfo.startTime);
        (void)SVS_SS_UTILITIES::time2str(szEndTime, SVS_STR_TIME_MAX_LEN, stratInfo.endTime);

        SVS_LOG((SVS_LM_DEBUG,
            "Fetch record strategy from message, "
            "recordType[%d], triggerFlag[%d], timeRange[%s-%s], weekFlag[0x%x].",
            stratInfo.stratFlag,
            stratInfo.triggerFlag,
            szStartTime, szEndTime,
            stratInfo.weekFlag));
    }

    return SVS_RESULT_OK;
}
//=============================CAC_Record_Inform_Manager END===================

