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

    //Ĭ�ϲ������¼�¼��Ԥ¼�ͺ�¼
    event_before_record_time_ = 0;
    event_after_record_time_ = 0;
    inform_stat_.init(str_frontdev_id_);
    //��ʼ���¼�����VECTOR
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
�� �� ��  : init_strate
��������  : ��ʼ����ͷ�Ĳ����б�
�������  : STRAT_VECTOR& stratVector:��ͷ�����б�
�������  : ��
�� �� ֵ  : �ɹ�: true ʧ��:false
�޸���ʷ  :
*****************************************************************************/
bool CAC_Record_Inform::init_strate(const STRAT_VECTOR& stratVector)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to initialize camera record strategy list, camera[%s].",
        str_frontdev_id_));

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, false);

    record_flags_ = RECORDING_NONE;

    //����¼������б�
    (void)strat_list_.update_strategy_vector(stratVector);

    //�������Ƚ�Ҫִ�е�¼�����
    (void)strat_list_.search_earliest_time();

    state_ = FRONT_DEVICE_STATE_INFORMING;

    uint8_t flags = 0;

    record_flags_ = strat_list_.record_type(flags);

    return true;
}
/*****************************************************************************
 �� �� ��  : exec_strategy
 ��������  : ִ�о�ͷ��¼�����
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ����� 0
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform::exec_strategy()
{
    SVS_TRACE();

    //��ȡ�����Ѿ���Ч��ʱ�䳤��
    time_t tlong = strat_list_.howlong();
    time_t endTime = strat_list_.end_time();

    SVS_LOG((SVS_LM_INFO, "Begin to execute record strategy, "
        "camera[%s], strategy effective time[%d].",
        str_frontdev_id_, tlong));


    if (0 >= tlong)
    {
        int32_t ret = SVS_RESULT_OK;

        // ��������ִ�е�¼�����
        {
            ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
            ret = strat_list_.search_earliest_time();
        }

        // 1.��ǰû����Ч�Ĳ��ԣ�����Ҳû����Ч�Ĳ����ˣ���Ҫ���ý������ʱ���Ķ�ʱ��
        // �����ǰû����Ч�Ĳ��ԣ������ö�ʱ��Ϊ�������
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
            // 1.��ǰ�����Ѿ���Ч����Ҫ����ִ�в��ԣ�ͬʱ��Ҫ������һ�����Ա��ʱ���
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
        // 1.��ǰ���Ի�δ��Ч����Ҫ������һ�����Ա��ʱ���
        ACE_Time_Value delay(endTime, 0);
        ACE_Time_Value intervalTime(SVS_TIMER_INTERVAL_TIMEOUT, 0);
        if (SVS_RESULT_FAILURE == set_timer(state_, delay, intervalTime))
        {
            SVS_LOG((SVS_LM_WARNING, "Fail to set record timer, camera[%s].",
                str_frontdev_id_));
        }
    }


    (void)strat_list_.print_selected_strategy(str_frontdev_id_);

    //����¼��ʼ֪ͨ
    (void)inform_recording();

    //ͳ��
    time_t startTime = time(NULL);
    bool isEventEffective = (strat_list_.get_event_strategy_time() > 0);

    inform_stat_.updateStrategyTime(startTime, endTime, record_flags_, isEventEffective);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : set_timer
 ��������  : ���ö�ʱ��
 �������  : uint8_t byState:��ͷ�µ�״̬
             const ACE_Time_Value &delayTime:��ʱ����Ч����ʱʱ��
             const ACE_Time_Value &intervalTime:��ʱ��������ʱ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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

        // �����ͷû��ֹͣ¼���������״̬
        if (FRONT_DEVICE_STATE_STOP != state_)
        {
            state_ = byState;
        }

        // �����ͷԭ���Ѿ��趨�˶�ʱ������ɾ��ԭ���Ķ�ʱ��
        if (0 <= nId)
        {
            SVS_LOG((SVS_LM_DEBUG,
                "Kill old timer. camera[%s], old timer[%d].",
                str_frontdev_id_,
                nId));

            (void)p_manager_->kill_timer(nId);
        }

        // �����µĲ�������������ʱ���������ö�ʱʱ��
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
 �� �� ��  : kill_timer
 ��������  : ɾ����ʱ��
 �������  : uint8_t byState:��ͷ�µ�״̬
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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

    // ���¾�ͷ��״̬
    state_ = byState;

    // �����ͷ��Ӧ�Ķ�ʱ�����ڣ���ɾ���ö�ʱ��
    if (0 <= timerId)
    {
        return p_manager_->kill_timer(timerId);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : on_check_strategy
 ��������  : ��ʱ���Ļص�����
 �������  : int32_t eventId:�¼���
             int32_t
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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

    // ����¼�id����ͬ�����账��ֱ�ӷ���
    if ((routerid_t)eventId != internal_id_)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "EnventID given by callback function is not consistent with camera's internal eventID, "
            "given eventID[%d], internal eventID[%d].",
            eventId,
            internal_id_));

        return SVS_RESULT_OK;
    }

    // ��ȡ��ͷ�����翪ʼִ�еĲ�������ʱ��
    time_t timeDiff = -1;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, SVS_RESULT_FAILURE);

        timeDiff = strat_list_.howlong();
    }

    // �����ͷ��û�п�ʼ����¼����ôҪ�������ö�ʱ��
    if (0 < timeDiff)
    {
        SVS_LOG((SVS_LM_INFO,
            "Camera does not reach record time and will set timer, "
            "camera[%s], wait time[%d].",
            str_frontdev_id_,
            timeDiff));

        //�������ö�ʱ��
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

        // ����û�в��ԣ�����0�㶨ʱ��
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

    // ��������Ĳ��Ի���Ҫ�೤ʱ����Ч�������ö�ʱ���ڲ�����Ч֮ʱҲ��Ч
    ACE_Time_Value delay;
    timeDiff = strat_list_.howlong();
    delay.set((int32_t)timeDiff, 0);
    ACE_Time_Value intervalTime(SVS_TIMER_INTERVAL_TIMEOUT, 0);
    if (SVS_RESULT_FAILURE == set_timer(state_, delay, intervalTime))
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to set record timer, camera[%s].", str_frontdev_id_));
    }

    //����¼��֪ͨ��������ѡ��ʱ��
    int32_t informResult = inform_recording();

    return informResult;
}

/*****************************************************************************
 �� �� ��  : inform_recording
 ��������  : ����¼��֪ͨ
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform::inform_recording()
{
    SVS_TRACE();

    // ��ȡ¼�����ͣ������ͷû������¼�����ͣ�����Ϊ�ǽ�ֹ¼��
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

    //��������״̬����¼��
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

    // ����¼�¼�������Ч����֪ͨ�¼�
    recordResult = informEvent();
    return recordResult;
}

/*****************************************************************************
 �� �� ��  : renew_strategy
 ��������  : ���²����б�
 �������  : STRAT_VECTOR& stratVector:�µĲ����б�
 �������  : ��
 �� �� ֵ  : �ɹ�:true ʧ��:false
 �޸���ʷ  :
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

        // ����û�в��ԣ�����0�㶨ʱ��
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

        (void)CAC_RT_Record_Device_Manager::instance()->stop_device_record(str_frontdev_id_);  //ֹͣ¼��

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
 �� �� ��  : off_line
 ��������  : ��ͷ����
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform::off_line()
{
    SVS_TRACE();

    // ֹͣ�����ֶ�¼��
    (void)kill_timer( FRONT_DEVICE_STATE_STOP );

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : stop_record
 ��������  : ֹͣ¼��
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform::stop_record( )
{
    SVS_TRACE();

    // ����״̬Ϊֹͣ
    (void)kill_timer( FRONT_DEVICE_STATE_STOP );

    (void)CAC_RT_Record_Device_Manager::instance()->stop_device_record( str_frontdev_id_, false );

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : init_lens
 ��������  : ��ʼ����ͷ����
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ���ʼ��,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform::init_lens()
{
    SVS_TRACE();

    //��ʼ��¼�����
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
 �� �� ��  : init_record_parameters
 ��������  : �����ݿ��ж�ȡ��ͷ��¼�����,����ʼ���������¼�¼��Ԥ¼ʱ������¼ʱ��
             ������ݿ���û�иþ�ͷ��¼���������Ĭ�ϵ��¼�¼��Ԥ¼ʱ��Ϊ0��
             ��¼ʱ��Ϊ0
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ���ȡ¼�����������ʼ��,���� 0 ���򷵻�-1
 �޸���ʷ  :
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
 �� �� ��  : update_record_parameter
 ��������  : ���¾�ͷ�����¼�����
 �������  : nBeforeSeconds :   �¼�¼��Ԥ¼ʱ��
             nAfterSeconds  :   �¼�¼���¼ʱ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform::update_record_parameter(uint32_t nBeforeSeconds,
                                               uint32_t nAfterSeconds )
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    event_before_record_time_ = nBeforeSeconds;
    event_after_record_time_ = nAfterSeconds;

    // ����¼��֪ͨ
    (void)inform_recording();

    SVS_LOG((SVS_LM_INFO,
        "Success to update record parameter, "
        "camera[%s].",
        str_frontdev_id_));

    return SVS_RESULT_OK;
}

// ֹͣinform�������û��������ʹ���ˣ�Ӧ�ü���Ϊ0�����سɹ������ڷ���-1��ʾ������������
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
    // ������¼�������ˢ���¼�����ʱ��Ϊ��ǰʱ�����������¼�¼��ʱ��
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
    // ����¼�¼�������Ч������֪ͨ�ײ��¼�
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
        // �������е��¼����Ƚϵ�ǰ��ʱ����¼��Ľ���ʱ�䣬�����ǰ�¼��Ѿ������ˣ���ɾ���¼���¼
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

                // ����¼����ڣ���ɾ��
                if (pIter->second <= currTime)
                {
                    event_vector_[j].erase(pIter);
                }
                else// �¼���δ���ڣ����¼������֪ͨ�ײ㣬�����¼�¼��
                {
                    tempEventVct[j][pIter->first.c_str()] = pIter->second;
                }
            }
        }
    }

    // �жϾ�ͷ��ǰ��״̬�Ƿ����¼�񣬲������͸������ͷ������
    bool bIsRecord = false;
    if (FROBID_RECORDING != record_flags())
    {
        bIsRecord = true;
    }

    // �����¼�
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
 �� �� ��  : instance
 ��������  : ��ȡ¼��֪ͨ������ʵ��
 �������  : ��
 �������  : ��
 �� �� ֵ  : CAC_Record_Inform_Manager* ¼��֪ͨ����������ָ��
 �޸���ʷ  :
*****************************************************************************/
CAC_Record_Inform_Manager *CAC_Record_Inform_Manager::instance()
{
    static CAC_Record_Inform_Manager recordInformManager;

    return &recordInformManager;
}

/*****************************************************************************
 �� �� ��  : init
 ��������  : ��ʼ��¼��֪ͨ������ʵ��
 �������  : uint32_t nMaxLens:���ͷ����
 �������  : ��
 �� �� ֵ  : �ɹ�:true ʧ��:false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_Record_Inform_Manager::init(uint32_t nMaxLens)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to init record inform manager. max camera num[%d].",
        nMaxLens));
    max_lens_ = nMaxLens;

    // �����˳������ڻ�ȡ¼�������ʱ�򣬻�����ʵʱ��ѯ
    if (SVS_RESULT_OK != init_lens_param())
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to init camera parameter."));
    }

    return true;
}

/*****************************************************************************
 �� �� ��  : add_device
 ��������  : ���ݾ�ͷID�ͱ�����������һ����ͷ����
 �������  : const char *strFrontDevId:ǰ�˾�ͷID
             int32_t encoderType:��������
             CAC_Record_Inform *&pDevice:����ӵľ�ͷ����ָ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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

    // �жϾ�ͷ�����Ƿ��Ѵﵽ����,�ﵽ������ֱ�ӷ���SVS_RESULT_FAILURE
    uint32_t currCameraNum = device_map_.current_size();
    uint32_t backupCameraNum = CAC_Bakup_Record_Dev_Manager::instance()->get_bakup_device_num();
    size_t lensSize = currCameraNum + backupCameraNum;

    // ���ݾ�ͷ��ʵʱ¼��ͷһ�����
    lensSize += CAC_Bakup_Record_Dev_Manager::instance()->get_bakup_device_num();

    SVS_LOG((SVS_LM_INFO,
        "Total camera number info. Realtime record camera num[%u], backup camera num[%u].",
        currCameraNum,
        backupCameraNum));

    // �����Ƿ����ָ����ͷID�ľ�ͷ��������ڣ��򷵻ظþ�ͷ����
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

    // ����һ���µľ�ͷ����
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

    //��ȡ�ڲ�����
    uint32_t internalId = next_internal_id();

    if (0 == internalId)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to allocate an internal id for camera, camera[%s].",
            strFrontDevId));

        SVS_DELETE( pTmpDevice );

        return SVS_RESPCODE_OTHER_ERROR;
    }

    pTmpDevice->internal_id(internalId);

    //���´����ľ�ͷ������뵽MAP����
    if (0 != device_map_.bind(pTmpDevice->get_frontdev_id(), pTmpDevice))
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to add camera to map, camera[%s].", strFrontDevId));

        SVS_DELETE( pTmpDevice );

        return SVS_RESPCODE_OTHER_ERROR;
    }

    //����ͷ���ڲ������������
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
 �� �� ��  : get_device
 ��������  : �����豸ID����ȡ¼��֪ͨ����ָ��
 �������  : const char *strFrontDevId:ǰ���豸ID
 �������  : ��
 �� �� ֵ  : �ɹ�:��ͷ����ָ�� ʧ��:NULL
 �޸���ʷ  :
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
 �� �� ��  : get_device
 ��������  : ��������ID����ȡ¼��֪ͨ����ָ��
 �������  : uint32_t nId:�������
 �������  : ��
 �� �� ֵ  : �ɹ�:��ͷ����ָ�� ʧ��:NULL
 �޸���ʷ  :
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
 �� �� ��  : remove_device
 ��������  : ��map����ɾ����Ӧ�ľ�ͷ
 �������  : uint32_t nId:�ڲ�������
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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
 �� �� ��  : remove_device
 ��������  : �Ӿ�ͷmap����ɾ����Ӧ��ǰ���豸
 �������  : uint32_t nId:ǰ�˾�ͷID
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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
 �� �� ��  : query_device_strategy
 ��������  : ��ѯ��ͷ����
 �������  : const char *strLensId:��ͷID
             STRAT_VECTOR& stratVector:�����б�
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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
//        // ������ֶ�¼����ǿ��Ϊһ����¼��
//        if (SVS_RECORDER_MANUAL == iter->ucTriggerFlag)
//        {
//            iStragegyFlag = 1;
//        }
//
//        // ������ֶ�¼�񣬲�����Ҫ�ж�¼��ʼ����ʱ�䣬��Ҫ�ж�����
//        if (1 == iStragegyFlag)
//        {
//            stratProperty.startTime = SVS_SS_UTILITIES::str2time(iter->szExecuteTime);
//
//            char szEndTime[SVS_STR_TIME_MAX_LEN] = {0};
//            (void)ACE_OS::memcpy(szEndTime, iter->szExecuteTime, RT_DATE_STR_LEN);
//
//            //��Ҫ��¼�����ʱ���ǰ��λȥ��?
//            (void)ACE_OS::memcpy(szEndTime + RT_DATE_STR_LEN, iter->szEndTime, RT_DATE_STR_LEN);
//
//            stratProperty.endTime = SVS_SS_UTILITIES::str2time(szEndTime);
//
//            // ���һ����¼������Ѿ����ڣ�ֱ�ӷ��ؾ�ok
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
//        // �жϿ�ʼ�ͽ���ʱ���Ƿ�Ϸ�
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
//        //�ж��ܱ�־�ַ����ĳ����Ƿ�����Ҫ��
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
//        // ��һ����ÿ���¼���־�����õ�stratProprety.weekFlag��
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
 �� �� ��  : check_strategy
 ��������  : ��鲢ִ��¼�����
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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

        // ��ֹ��������ʱ�����ʹ���������·����Ϣ
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
 �� �� ��  : next_internal_id
 ��������  : ������һ���ڲ�����ID
 �������  : ��
 �������  : ��
 �� �� ֵ  : ����ڲ�IDû�дﵽ���ֵ:��һ���ڲ�ID, ����ڲ�ID�ﵽ���ֵ:0
 �޸���ʷ  :
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
 �� �� ��  : decrease_reference
 ��������  : ����ͷ�����ü�����һ������ͷû���ٱ�����ʱ��ɾ����ͷ
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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
 �� �� ��  : check_strategy_loop
 ��������  : ���¼������߳�
 �������  : ACE_Reactor *pReactor: ¼��֪ͨ��Ӧ��
             CAC_Timer_Manager *
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::check_strategy_loop(ACE_Reactor *pReactor, CAC_Timer_Manager * )
{
    SVS_TRACE();

    p_strategy_reactor_ = pReactor;
    int32_t ret = 0;
    CThread_Stat_Reporter reporter("CheckStrategyThread");
    // �޸���handle_eventsʧ���߳��˳�����whileѭ���ڲ�����break
    // ��ʱӦ�޸�whileѭ�����ж�����������ѭ�������˳�*/
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
 �� �� ��  : on_check_strategy
 ��������  : ��ʱ���ص�����
 �������  : int32_t eventId:�¼�ID
             int32_t timerId:��ʱ��ID
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::on_check_strategy(int32_t eventId, int32_t timerId)
{
    SVS_TRACE();

    // �����¼�ID�����Ҷ�Ӧ�ľ�ͷ
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
    CAC_Record_Inform *pDevice = get_device((uint32_t)eventId);
    if (NULL != pDevice)
    {
        // ִ�о�ͷ�Ļص�����
        CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pDevice);
        int32_t checkResult = pDevice->on_check_strategy(eventId, timerId);

        return checkResult;
    }

    (void)kill_timer( timerId );
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : renew_strategy
 ��������  : ����ָ���豸��¼�����
 �������  : const uint8_t * const strDeviceId:��ͷID
 �������  :
 �� �� ֵ  : �ɹ�:0  ʧ��:-1
 �޸���ʷ  :
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
 �� �� ��  : renew_strategy
 ��������  : ���¾�ͷ¼�����
 �������  : const ACE_Message_Block *mb:��Ϣ��
 �������  :
 �� �� ֵ  : �ɹ�:0  ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::renew_strategy(const ACE_Message_Block *mb)
{
    SVS_TRACE();

    PSVS_STORESVR_RENEW_STRATEGY_REQ_MSG pReq =
        (PSVS_STORESVR_RENEW_STRATEGY_REQ_MSG)(void*)mb->rd_ptr();

//    const size_t msgSize = sizeof(SVS_STORESVR_RENEW_STRATEGY_REQ_MSG)
//                           + (pReq->DeviceCount - 1) * (DEVICEID_LEN);
//
//    //������Ϣ�е��豸ID��ĿУ������Ϣ�����Ƿ�Ϸ�
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

    // ��У��һ�¹̶����ȣ���ֹԽ�����
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

    // �ڽ���ʱ����������ʱ������ͼ��������ľ�ͷ,�����Ѿ��ɹ������ľ�ͷ��Ҫ�Ѳ��Ը��µ�����
    uint32_t    uiLeftLen = (uint32_t)iDeviceStrategyInfoLen;
    uint8_t     *pInfoPtr = (uint8_t *)&pReq->DeviceStrategyInfo[0];

    // ������������ͷ��ͷ��¼�����
    for (int32_t i = 0; i < pReq->DeviceCount; i++)
    {
        // �Ե�����ͷУ��̶����ֵĳ���
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

        //��ÿ����ͷ���������б�ͬ���㳤�ȣ����Բ����б���н�����������һ���豸
        uint32_t   uiDeviceInfoLen = offsetof(DEVICE_STRATEGY_INFO, RecordSvrStrategyInfo)
                + (uint32_t)pDevInfo->RecordStrategyCount * sizeof(RECORD_STRATEGY_INFO);
        if (uiDeviceInfoLen > uiLeftLen)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Device strategy renew info length invalid, bufLen[%d], stratCount[%d], infoLen[%d].",
                uiLeftLen, pDevInfo->RecordStrategyCount, uiDeviceInfoLen));
            return SVS_RESULT_FAILURE;
        }

        // ����������ͷ��¼�����֮ǰ���Ѿ��Գ��Ƚ�����У�飬���԰�ȫ�ĵ��������
        STRAT_VECTOR  statVector;
        if (SVS_RESULT_OK != fill_strategy_list((uint32_t)pDevInfo->RecordStrategyCount,
                        pDevInfo->RecordSvrStrategyInfo, statVector))
        {
            SVS_LOG((SVS_LM_WARNING, "Fetch strategy list from message failed."));
            statVector.clear();
            return SVS_RESULT_FAILURE;
        }

        // �����޸ĺ󣬸��²���ʱ����Ϣ�н��������ԣ��������ݿ��ȡ
        (void)renew_strategy(szDeviceId, statVector);

        // ƫ�Ƶ���һ����ͷ��ʼ��λ�ã�����ǰ���Ѿ������˳������ƣ��˴����ᷭת
        uiLeftLen -= uiDeviceInfoLen;
        pInfoPtr += uiDeviceInfoLen;
    }

    // ��������ɺ�Ӧ��û�ж������Ϣ��δ����
    if (0 != uiLeftLen)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Update strategy message length invalid, devCount[%d], msgLen[%d], leftLen[%d].",
            pReq->DeviceCount, pReq->Header.PacketLength, uiLeftLen));
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : alarm_code_translate
 ��������  : ת���澯��Ϊ�¼����ͺ͸澯����
 �������  : uint8_t alarmCode:�澯��
 �������  : uint32_t &eventType:�¼�����
             uint8_t &alarmType:�澯����
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
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
            //SD�쳣�澯��ת��
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
 �� �� ��  : sdcard_alarm_code_translate
 ��������  : ת��sdcard�澯��Ϊ�¼����ͺ͸澯����
 �������  : uint8_t alarmCode:�澯��
 �������  : uint32_t &eventType:�¼�����
             uint8_t &alarmType:�澯����
 �� �� ֵ  : ��
 �޸���ʷ  :
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
 �� �� ��  : intelligence_alarm_code_translate
 ��������  : ת�����ܷ����澯��Ϊ�¼����ͺ͸澯����
 �������  : uint8_t alarmCode:�澯��
 �������  : uint32_t &eventType:�¼�����
             uint8_t &alarmType:�澯����
 �� �� ֵ  : ��
 �޸���ʷ  :
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
 �� �� ��  : alarm_event_req
 ��������  : ����澯¼������������Ϣ
 �������  : const ACE_Message_Block *mb
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
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

    // ת���澯��
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

    //���Ҿ�ͷ����
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

    // ��inform�����¼���¼���������ʧ�ܣ���Ӱ�����µ�����
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

    //����Ƿ�����Ч���¼�¼�����
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

    // ��ʼ�¼�¼��
    if ( SVS_DEVICE_CONTROL_REC_START == pReq->ControlCode )
    {
        SVS_LOG((SVS_LM_INFO,
            "Event happend. camera[%s], eventType[0x%04x], alarm deviceID[%s].",
            pInform->get_frontdev_id(),
            eventType,
            strAlarmDeviceId));

        // �жϾ�ͷ��ǰ��״̬�Ƿ����¼�񣬲������͸������ͷ������
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
    else//ֹͣ�¼�¼��
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
 �� �� ��  : stop_record
 ��������  : ֹͣ����¼�񣬲�������еľ�ͷ����
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
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

        // ֹͣ����¼��
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

        // �ȴ�����¼�����
        while (0 < recording_device_num)
        {
            recording_device_num = CAC_RT_Record_Device_Manager::instance()->get_recording_device_num();

            SVS_LOG((SVS_LM_DEBUG,
                "There are still [%u] cameras record now.",
                recording_device_num));

            ACE_Time_Value tv( 0, SVS_CHECK_STOP_RECORD_INTERVAL_TIME );
            (void)ACE_OS::sleep( tv );

            // ����ֹͣSVS_RETRY_STOP_RECORD_TIMES��
            if (SVS_RETRY_STOP_RECORD_TIMES < retryTime++)
            {
                break;
            }
        }
    }

    //ɾ������¼�����
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

    // �õ�ǰ״̬Ϊ��ѯ�豸״̬
    state_ = FDM_STATE_QUERY_DEVICE;
    SVS_LOG((SVS_LM_WARNING, "Success to stop all cameras."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : check_strategy_timer_callback
 ��������  : �����Զ�ʱ���ص�����
 �������  : void *pArg,
             int32_t eventId:�¼�ID
             int32_t timerId:�ڲ���ʱ��ID
             int32_t
 �������  : ��
 �� �� ֵ  : ��
 �޸���ʷ  :
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

    // ���ص���Ϣ��ӵ���ʱ���ص�������Ϣ������
    (void)CAC_RT_Record_Server::instance()->add_timer_callback_msg( mb );
}

/*****************************************************************************
 �� �� ��  : debug_get_all_lens_info
 ��������  : ����״̬�£���ȡ���о�ͷ��Ϣ
 �������  : const uint32_t szLensInfo:��Ϣ�ܳ���
 �������  : char *strLensInfo:�������о�ͷ������Ϣ��ָ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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

        //�����������꣬�˳�ѭ��
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

        // ���ֲ��Լ���
        switch(uRecordType)
        {
            // ��ֹ¼��
            case FROBID_RECORDING:
            {
                ++uForbidRecordCameraNum;
                break;
            }

            // ����¼��
            case ALWAYS_RECORDING:
            {
                ++uAlwaysRecordCameraNum;
                break;
            }

            // �¼�¼��
            case RECORDING_ON_EVENT:
            {
                ++uEventRecordCameraNum;
                break;
            }

            // �ֶ�¼��
            case RECORDING_ON_MANUAL:
            {
                ++uManualRecordCameraNum;
                break;
            }

            // ��ǰû����
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
 �� �� ��  : update_record_parameter
 ��������  : ���ݴ���Ĳ��������豸¼�����
 �������  : mb :   �����豸¼�������Ϣָ��
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
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

    //���ڴ�ά���ĸ��¾�̬����
    (void)update_lens_param(strLensId, lensParam);

    //����ָ���ľ�ͷ����,�����¾�ͷ�����е�Ԥ¼���¼ʱ��
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
 �� �� ��  : handle_record_task
 ��������  : �豸״̬�������
 �������  : const ACE_Message_Block *mb:�����豸״̬��Ϣָ��
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
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

    // ������Ϣ�����Ƿ���ȷ
    if (pReqMsg->Header.PacketLength < unCheckLen)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to deal device record distribute request message, because message len is invalid."
            "packetLen[%d], minLen[%d].",
            pReqMsg->Header.PacketLength, unCheckLen));
//1 TODO ������Ϣ����ͳһ��������Ͽ�����

        return SVS_RESULT_FAILURE;
    }

    // ʵ��Ӧ���в��������ͬһ���ӹ�������Ϣ��Я�������ͷ�����
    if (1 != pReqMsg->DeviceCount)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Not support distribute task take more than one camera, cameraNum[%d].",
            pReqMsg->DeviceCount));

        return SVS_RESULT_FAILURE;
    }

    // ����Ǿ�ͷ¼��������Ϣ
    if(DISTRIBUTE_TYPE_DISTRIBUTED == pReqMsg->DistributeType)
    {
        return start_record(pReqMsg);
    }
    else if(DISTRIBUTE_TYPE_CANCEL == pReqMsg->DistributeType) // ����Ǿ�ͷ¼��ֹͣ��Ϣ
    {
        return stop_record(pReqMsg);
    }
    else // ��������Ϊ���󣬷���ʧ��ЧӦ��Storesvr
    {
        SVS_LOG((SVS_LM_WARNING,
            "Handle record distribute task failed. distribute Type invalid[%u]",
            pReqMsg->DistributeType));

        char    szDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0};
        SetLocalizeDeviceId((uint8_t *)szDeviceId,
                    sizeof(szDeviceId),
                    (const char *)pReqMsg->DeviceDistributeInfo[0].DeviceID,
                    NLS_DEVICE_ID_MAX_LEN);

        //������Ӧ��Ϣ
        //��ȷ���͵Ĵ�����Ϊ�ӹ���Ϣ�е����Ͳ���ȷ
        send_distribute_reponse(pReqMsg->Header.TransactionNo,
                    pReqMsg->DistributeType,
                    szDeviceId,
                    SVS_RESPCODE_LENS_DISTRIBUTE_TYPE_INVALID);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : start_record
 ��������  : �ӹ�StoreSvr�ַ������ľ�ͷ¼�񡣴���inform���󣬲�ѯ¼����ԣ����
             ��ǰ�в��������¼�񣬷���ȴ�¼��
 �������  : SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg:�ַ����ݣ���Ҫ�ӹܵľ�ͷ�б�
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::start_record(const SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg)
{
    //�����StoreSvr�����ˣ��򲻴������Ϣ
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

    // ����Ϣ���н�������ͷ��¼������б�����¼�����Ϊ��
    int32_t iResult = check_device_strategy(pReqMsg);

    // ����Ϣ����Я����¼������������ȷ�����ʹ�����Ӧ
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

//    //��ʹû�в���ҲҪ����inform������й���
//    if (0 == stratVector.size())
//    {
//        SVS_LOG((SVS_LM_INFO, "Camera's strategy number is 0 at today, CameraId[%s]", strDeviceId));
//    }

    //���¾�ͷ������Ϣ
    SVS_LENS_PARAMETER lensParam;
    lensParam.RecordStoreDays = (uint32_t)pReqMsg->DeviceDistributeInfo[0].StoreDays;
    lensParam.BeforeSeconds = pReqMsg->DeviceDistributeInfo[0].BeforeSeconds;
    lensParam.AfterSeconds = pReqMsg->DeviceDistributeInfo[0].AfterSeconds;
    (void)update_lens_param(strDeviceId, lensParam);

    //���Ӿ�ͷ����
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

    //��ʼ����ͷ
    int32_t initLensResult = pInform->init_lens();

    if (SVS_RESULT_OK != initLensResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to initialize camera. camera[%s].",strDeviceId));

        //lensInfo.RespCode = SVS_RESPCODE_OTHER_ERROR;
        //lensInfoVect.push_back(lensInfo);
        // ���������룬��ʾ�ڲ���������е����⣬�����������
        send_distribute_reponse(pReqMsg->Header.TransactionNo,
                pReqMsg->DistributeType, strDeviceId,
                SVS_RESPCODE_LENS_DISTRIBUTE_INNER_ERROR);

        return SVS_RESULT_OK;
    }

//    //��ʼ����ͷ¼������б�
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
//    //ִ�о�ͷ¼�����
//    (void)pInform->exec_strategy();
//
//    //�ӹܳɹ�
//    //lensInfo.RespCode = SVS_RESPCODE_SUCCESS;
//    //lensInfoVect.push_back(lensInfo);
//
//    SVS_LOG((SVS_LM_DEBUG,
//        "Handle lens distribute task success, cameraId[%s]",
//        strDeviceId));
//    //}
//
//    state_ = FDM_STATE_CHECK_STRATEGY;

    // �������Ը�����Ϣ
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

    // ������Ϣ����
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

    // �Ѳ��Ը�����Ϣ�ֵ����Ը��¶�����
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

    //������Ӧ��Ϣ
    send_distribute_reponse(pReqMsg->Header.TransactionNo,
            pReqMsg->DistributeType,
            strDeviceId,
            SVS_RESPCODE_SUCCESS);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : stop_record
 ��������  : ȡ���ӹ�StoreSvr�ַ������ľ�ͷ¼�������ͷ����¼���򲻴���þ�ͷ��
             �����ͷ����¼����ֹͣ�þ�ͷ¼��
 �������  : SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg:�ַ����ݣ���Ҫȡ���ӹܵľ�ͷ�б�
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
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
//        // ��ѯ��ͷ�����б�
//        STRAT_VECTOR stratVector;

    SetLocalizeDeviceId((uint8_t *)strDeviceId,
                sizeof(strDeviceId),
                (const char *)pReqMsg->DeviceDistributeInfo[0].DeviceID,
                NLS_DEVICE_ID_MAX_LEN);

    // ���Ҿ�ͷ����������ҵ�����������
    pInform = get_device(strDeviceId);

    // ���û���ҵ��豸��Ҳ����ֹͣ�ɹ�
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

    // ��ͷ����
    CAC_Auto_Reference_Ptr <CAC_Record_Inform_Manager, CAC_Record_Inform *> ptr(this, pInform);

    // ���þ�ͷ״̬��������ͷ����
    (void)pInform->off_line();

    (void)CAC_RT_Record_Device_Manager::instance()->device_off_line( strDeviceId );
    //    lensInfo.RespCode = SVS_RESPCODE_SUCCESS;

    //    lensInfoVect.push_back(lensInfo);
    //}

    // ������Ӧ��Ϣ
    //send_distribute_reponse(pReqMsg->Header.TransactionNo,pReqMsg->DistributeType, lensInfoVect);
    send_distribute_reponse(pReqMsg->Header.TransactionNo,
                    pReqMsg->DistributeType,
                    strDeviceId,
                    SVS_RESPCODE_SUCCESS);
    //lensInfoVect.clear();

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : send_distribute_reponse
 ��������  : ��StoreSvr���ͷַ���Ӧ��Ϣ;
 �������  : uint16_t msgNo:��Ϣ�����
             uint32_t distributeType:�ַ����ͣ�0�ӹ����1ȡ���ӹ�����
             LENSINFO_VECTOR &lensVect:��ͷ�б�
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
*****************************************************************************/
void CAC_Record_Inform_Manager::send_distribute_reponse(uint16_t msgNo ,
                                                              uint32_t distributeType,
                                                              //DISTRIBUTELENSINFO_VECTOR &lensVect
                                                              const char *pszLensId,
                                                              uint32_t  uiResponseCode)const
{
    // ���뻺�������ɱ䳤�ȣ���ȡ��󳤶�
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

    // ��װ��Ϣͷ
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
 �� �� ��  : init_lens_param
 ��������  : ��ʼ�����кͱ�NRU������ľ�ͷ�Ĳ���
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻� -1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::init_lens_param()
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lens_parameter_map_mutex_, SVS_RESULT_FAILURE);

    // ��ʼ��
    (void)lens_parameter_map_.unbind_all();

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : get_lens_param
 ��������  : ��ȡָ����ͷ��¼�����
 �������  : const char* strLensId:��ͷID
 �������  : SVS_LENS_PARAMETER& lensParameter:��ͷ¼�������Ϣ
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻� -1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::get_lens_param(const char* strLensId,
                                        SVS_LENS_PARAMETER& lensParameter)
{
    SVS_TRACE();

    SVS_LENS_PARAMETER pLensParam;

    // �����ڵ�ǰmap�в���ָ����ͷ�Ĳ���
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, lens_parameter_map_mutex_, SVS_RESULT_FAILURE);

    //������ڴ����Ҳ�����ͷ��¼����������ٴ����ݿ��л�ȡ
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
 �� �� ��  : update_lens_param
 ��������  : ����ָ����ͷ�Ĳ���,�������bool isDelLensΪtrue,
             ��Ѿ�ͷ������map��ɾ��
 �������  : char * strLensId:��ͷID
             SVS_LENS_PARAMETER lensParameter:��������
             bool isDelLens:�Ƿ�Ϊɾ����ͷ
 �������  : NA
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻� -1
 �޸���ʷ  :
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

    // �����ɾ����ͷ
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
//    // ��ѯ�ÿͻ������о�ͷ
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
//        // ���ݾ�ͷid��ȡ��ͷ����ָ�룬�����ͷ�����ڣ��򴴽�
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
//            // ��ʼ����ͷ
//            int32_t initLensResult = pInform->init_lens();
//            if (SVS_RESULT_OK != initLensResult)
//            {
//                SVS_LOG((SVS_LM_ERROR,
//                    "Fail to initialize camera. cameraID[%s].",
//                    lensVect[i].LensId));
//                continue;
//            }
//
//            // ��ʼ��ͷ¼������б�
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
//    // ��ѯ�ÿͻ������о�ͷ
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
//        // ���ݾ�ͷid��ȡ��ͷ����ָ�룬�����ͷ���ڣ���ֹͣ¼��
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
 �� �� ��  : check_device_strategy
 ��������  : �����ԺϷ���
 �������  : pReqMsg:������Ϣ
 �������  : stratVector:¼������б�
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻� -1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Record_Inform_Manager::check_device_strategy(const SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg) const
{
    SVS_TRACE();

    if (NULL == pReqMsg)
    {
        SVS_LOG((SVS_LM_WARNING, "Request message is NULL."));
        return SVS_RESULT_FAILURE;
    }

    // �Ա䳤�Ĳ��ֽ��г��ȵ�У��
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

    // �Ծ�ͷ��Ϣ�Ĺ̶����ֳ��Ƚ���У��
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

    // �Ծ�ͷ���Եı䳤���ֽ���У��
    uint32_t uiStrategyLen = pDistriInfo->RecordStrategyCount * sizeof(RECORD_STRATEGY_INFO);
    if ((uint32_t)iStrategyLen != uiStrategyLen)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Device strategy info length invalid, bufLen[%d], stratCount[%d], stratLen[%d].",
            iStrategyLen, pDistriInfo->RecordStrategyCount, uiStrategyLen));
        return SVS_RESULT_FAILURE;
    }

//    // ������ת��Ϊ�ڴ�ṹ�壬��ͬʱУ����Ч��
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
 �� �� ��  : fill_strategy_list
 ��������  : ��RECORD_STRATEGY_INFO�ṹ���л�ȡ����ת��ΪSTRAT_VECTOR
 �������  : uiStrategyCount:���Ը���
             pStrategyInfo:���Խṹ����ʼ��ַ
 �������  : stratVector:¼������б�
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻� -1
 �޸���ʷ  :
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
        // ���Խ���ʱ�䲻��С�ڲ��Կ�ʼʱ��
        if (pStrategyInfo[i].StartTime >= pStrategyInfo[i].EndTime)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Strategy starttime less than endtime, startime[%u], endtime[%u].",
                pStrategyInfo[i].StartTime, pStrategyInfo[i].EndTime));
            return SVS_RESULT_FAILURE;
        }
        // ¼�����������Ч�Լ��
        if (pStrategyInfo[i].TriggerType > SVS_RECORDER_MANUAL
            || pStrategyInfo[i].TriggerType < SVS_RECORDER_DENY)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Strategy type invalid, strategyType[%d], valid[%d-%d].",
                pStrategyInfo[i].StrategyType,
                SVS_RECORDER_DENY, SVS_RECORDER_MANUAL));
            return SVS_RESULT_FAILURE;
        }
        // ���ڱ�־��8��������λ��ʾ�����ܴ���0xFF
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

