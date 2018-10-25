#ifndef _SVS_Record_Inform_h
#define _SVS_Record_Inform_h

#include <list>
#include <vector>

#include "svs_record_front_device.h"
#include "svs_strategy_list.h"
#include "ace/Guard_T.h"
#include "ace/Recursive_Thread_Mutex.h"

using namespace std;

class CAC_Record_Inform_Manager;
class CAC_Record_Inform;

/*
 * CLASS: CAC_Record_Inform
 * DESCRIPTION: 录像通知类
 */
class CAC_Record_Inform : public CAC_Record_Front_Device
{
public:
    // const char *strFrontDevId --- 设备ID， 空字符结束的字符串
    CAC_Record_Inform(  const char *strFrontDevId,
                        uint8_t encoderType,
                        CAC_Record_Inform_Manager *pManager );
    virtual ~CAC_Record_Inform();
public:

    // 用录像策略集合<stratVector>初始化录像通知对象
    UTAPI bool init_strate(const STRAT_VECTOR& stratVector);

    // 定时器回调函数，检测镜头的录像策略
    UTAPI int32_t  on_check_strategy(int32_t eventId, int32_t timerId);

    // 执行录像策略
    UTAPI int32_t  exec_strategy();

    // 更新录像策略，重新从数据库读出设备的录像策略列表，选择有效的录像策略执行。
    // 如果没有录像策略，那么返回true, 调用者可以删除此设备。否则返回false.
    UTAPI bool renew_strategy(const STRAT_VECTOR& stratVector);

    // 处理镜头停止消息
    UTAPI int32_t  off_line();

    // 当前是否有生效的事件录像策略
    UTAPI bool effect_event_stratetegy() const
    {
        // 如果是没有策略状态，则事件录像策略肯定不生效
        if(RECORDING_NONE == record_flags_)
        {
            return false;
        }

        return (strat_list_.get_event_strategy_time() > 0);
    }

    // 停止录像
    UTAPI int32_t stop_record( );

    UTAPI int32_t stop_inform();

    // 录像策略标识
    UTAPI uint8_t record_flags()  const
    {
        return record_flags_;
    }

    // 镜头状态标识
    UTAPI uint8_t lens_state()  const
    {
        return lens_state_;
    }

    // 镜头状态标识
    UTAPI uint8_t state()  const
    {
        return state_;
    }

    //初始化镜头
    UTAPI int32_t init_lens();

    // 更新录像参数
    UTAPI int32_t update_record_parameter(    uint32_t nBeforeSeconds,
                                    uint32_t nAfterSeconds );

    // 编码类型
    UTAPI uint8_t encoder_type() const
    {
        return encoder_type_;
    }

    // 当前策略结束时间
    UTAPI time_t curr_strategy_end_time()
    {
        return strat_list_.end_time();
    }

    // 事件录像预录时长
    UTAPI uint32_t event_before_record_time() const
    {
        return event_before_record_time_;
    }

    // 事件录像后录时长
    UTAPI uint32_t event_after_record_time() const
    {
        return event_after_record_time_;
    }

    // 更新设备状态
    UTAPI void device_status( uint8_t deviceStatus )
    {
        lens_state_  = deviceStatus;
    }

    UTAPI int32_t alarm_event_req(const char * strAlarmDeviceId, uint32_t eventType, uint16_t controlCode);

protected:
    // 更新设备状态
    UTAPI void state(uint8_t byState)
    {
        ACE_GUARD(ACE_Recursive_Thread_Mutex, pObj, mutex_);
        if (FRONT_DEVICE_STATE_STOP != this->state_)
        {
            state_ = byState;
        }
    }

    // 发送录像通知，同时，或者启动录像，或者停止录像
    UTAPI int32_t  inform_recording();
    // 设置定时器
    UTAPI int32_t set_timer( uint8_t byState,
                    const ACE_Time_Value &delayTime,
                    const ACE_Time_Value &intervalTime = ACE_Time_Value::zero );

    // 释放定时器
    UTAPI int32_t  kill_timer(uint8_t byState);
    // 初始化镜头的录像参数
    UTAPI int32_t init_record_parameters();
    UTAPI int32_t informEvent();
private:
        CAC_Record_Inform();

public:
    enum SVS_FRONT_DEVICE_STATE
    {
        FRONT_DEVICE_STATE_INIT = 0,           //初始化状态
        FRONT_DEVICE_STATE_QUERY_STRATEGY, //查询录像策略
        FRONT_DEVICE_STATE_INFORMING,      //录像通知状态
    };

    enum SVS_RECORD_TYPE
    {
        RECORD_TYPE_NORMAL          = 0x00, //普通的连续时间录像
        RECORD_TYPE_MOTIONDETECT    = 0x01, //动态侦侧
        RECORD_TYPE_ALARM           = 0x02, //告警录象
        RECORD_TYPE_MAX,
    };

protected:
    // 编码器类型
    uint8_t encoder_type_;
    // 录像类型标识
    uint8_t record_flags_;
    // 镜头管理类指针
    CAC_Record_Inform_Manager *p_manager_;
    // 策略定时器ID
    int32_t timer_id_;

    // 录像策略列表
    CAC_Strategy_List strat_list_;
    //事件录像预录时长，单位：秒
    uint32_t event_before_record_time_;
    //事件录像后录时长，单位：秒
    uint32_t event_after_record_time_;

    SVS_Inform_Stat inform_stat_;

    //事件总数
    int32_t total_event_;

    //录像事件分为RECORD_EVENT_CONTROL_TYPE_MAX(=5)种, 每种录像事件可以由
    //不同告警源产生. 同一告警源可以产生几种录像事件.
    vector <EVENTMAP > event_vector_;
#ifdef UNITTEST
    friend class CAC_Record_InformAutoMock;
    friend class TEST_CAC_Record_Inform;
#endif
};

/**
 * @class CAC_Record_Inform_Manager
 * @brief 镜头录像通知管理类, 是单体类
 */
class CAC_Record_Inform_Manager
{
protected:
    typedef struct tagDistributeLensInfo
    {
        DEVICE_DISTRIBUTE_INFO DistributeInfo;
        uint32_t RespCode;
    }DistributeLensInfo;

    typedef vector <DistributeLensInfo> DISTRIBUTELENSINFO_VECTOR;

    typedef struct TagLocalLensInfo
    {
        char          LensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
        uint8_t EncoderType;
    }LocalLensInfo;

    typedef vector <LocalLensInfo> LENSINFO_VECTOR;

public:
    static CAC_Record_Inform_Manager * instance();

    UTAPI bool init(uint32_t nMaxLens);

    UTAPI void strategy_time_manager(CAC_Timer_Manager *pTimerManager)
    {
        this->p_strategy_timer_manager_ = pTimerManager;
    }

    UTAPI int32_t remove_device(uint32_t internalId);

    UTAPI const uint32_t current_device_size()
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, 0);
        return device_map_.current_size();
    }

    UTAPI int32_t check_strategy_loop(    ACE_Reactor *pReactor,
                                CAC_Timer_Manager *pTimerManager );

    // 处理录像策略更新通知消息,成功返回0，失败返回-1
    UTAPI int32_t renew_strategy(const ACE_Message_Block *mb);

    // 处理来自告警服务器的告警事件
    UTAPI int32_t alarm_event_req(const ACE_Message_Block *mb);

    // 将指定对象的引用计数减１，如果此对象已经为停止状态且引用计数为０，那么将销毁此对象。
    // 如果成功返回０，失败返回-1.
    UTAPI int32_t decrease_reference(CAC_Record_Inform *pDevice);

    UTAPI int32_t set_timer(  uint32_t eventId,
                    const ACE_Time_Value &delayTime,
                    const ACE_Time_Value &intervalTime )
    {
        int32_t allocResult =  p_strategy_timer_manager_->alloc_timer(
                                this,
                                check_strategy_timer_callback,
                                (int32_t)eventId,
                                delayTime,
                                intervalTime );

        return allocResult;
    }

    UTAPI int32_t kill_timer(int32_t timerId)
    {
        int32_t freeTimerResult = p_strategy_timer_manager_->free_timer(timerId);

        return freeTimerResult;
    }

    UTAPI int32_t reset_timer_interval(int32_t timerId, const ACE_Time_Value& intervalTime)
    {
        int32_t result = p_strategy_timer_manager_->reset_timer_interval(timerId, intervalTime);

        return result;
    }
    // 停止所有录像，并清除所有的镜头对象
    UTAPI int32_t stop_all_record();

    UTAPI int32_t on_check_strategy(int32_t eventId, int32_t timerId);

    UTAPI int32_t debug_get_all_lens_info(  char *strLensInfo, const uint32_t szLensInfo );

    UTAPI CAC_Record_Inform * get_device(const char *strFrontDevId);

    // 更新设备录像参数
    UTAPI int32_t update_record_parameter(const ACE_Message_Block *mb);

    // 处理设备录像任务消息
    UTAPI int32_t handle_record_task(const ACE_Message_Block *mb);

    UTAPI int32_t start_record(const SVS_DEVICE_DISTRIBUTE_REQ_MSG * pReqMsg);

    UTAPI int32_t stop_record(const SVS_DEVICE_DISTRIBUTE_REQ_MSG * pReqMsg);

    UTAPI void send_distribute_reponse(uint16_t msgNo,
                                    uint32_t distributeType,
                                    //DISTRIBUTELENSINFO_VECTOR & lensVect
                                    const char *pszLensId,
                                    uint32_t  uiResponseCode)const;

    // 获取指定镜头的参数
    UTAPI int32_t get_lens_param(const char* strLensId, SVS_LENS_PARAMETER& lensParameter);

protected:
    CAC_Record_Inform_Manager();
    virtual ~CAC_Record_Inform_Manager();

    // 初始化与该NRU相关联的镜头参数
    UTAPI int32_t init_lens_param();

    // 更新指定镜头的参数
    UTAPI int32_t update_lens_param(const char* strLensId,
                                    const SVS_LENS_PARAMETER & lensParameter,
                                    bool isDelLens = false);

    // 根据索引<nId>或者设备ID查询录像通知对像，如果待查询的录像通知存在，
    // 那么递增其引用计数.
    // 成功返回录像通知对像指针，否则返回NULL。
    UTAPI CAC_Record_Inform * get_device(uint32_t nId);

    static void check_strategy_timer_callback(  void *pArg,
                                                int32_t eventId,
                                                int32_t timerId,
                                                int32_t /*nDummy*/ );

    // 添加镜头<strLensId>,创建镜头对象，并添加到map中。<pDevice>返回镜头对象。
    // 成功返回0，失败返回错误代码。
    UTAPI int32_t add_device( const char *strLensId,
                    int32_t encoderType,
                    CAC_Record_Inform *&pDevice );

    // 删除镜头<strDevId>, 成功返回0，失败返回-1。
    UTAPI int32_t remove_device(const char *strDevId);

    // 生成索引。
    UTAPI uint32_t next_internal_id();

    // 查询指定编码器类型<encoderType>是否在录像编码器类型列表中。
    // 如果查询到返回true, 否则返回false.
    UTAPI bool encodertype(uint8_t encoderType);

    UTAPI int32_t check_strategy();

    // 更新指定镜头ID<strDeviceId>的录像策略。成功返回0，失败返回-1
    UTAPI int32_t renew_strategy(const char *strDeviceId,
                        const STRAT_VECTOR &stratVector);

    // 告警码转换
    UTAPI int32_t alarm_code_translate(   uint8_t alarmCode,
                                uint32_t &eventType,
                                uint8_t &alarmType ) const;

    UTAPI int32_t check_device_strategy(const SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg) const;

    UTAPI int32_t fill_strategy_list(uint32_t uiStrategyCount,
                                const RECORD_STRATEGY_INFO *pStrategyInfo,
                                STRAT_VECTOR &stratVector) const;
    UTAPI void sdcard_alarm_code_translate( uint8_t  alarmCode,
                                                    uint32_t &eventType,
                                                    uint8_t &alarmType ) const;

     UTAPI void intelligence_alarm_code_translate( uint8_t  alarmCode,
                                                    uint32_t &eventType,
                                                    uint8_t &alarmType ) const;

public:
    typedef ACE_Hash_Map_Manager < const char *, CAC_Record_Inform *, ACE_Null_Mutex >  SVS_DEVICE_MAP;
    typedef ACE_Hash_Map_Manager <uint32_t, CAC_Record_Inform *, ACE_Null_Mutex>  SVS_STREAM_MAP;
    typedef ACE_Hash_Map_Iterator < const char *, CAC_Record_Inform *, ACE_Null_Mutex > SVS_DEVICE_MAP_ITER;
    typedef ACE_Hash_Map_Entry < const char *, CAC_Record_Inform * > SVS_DEVICE_MAP_ENTRY;
    typedef ACE_Hash_Map_Manager <ACE_CString, SVS_LENS_PARAMETER, ACE_Null_Mutex> SVS_LENS_PARAMETER_MAP;
protected:
    // 最大镜头数
    uint32_t max_lens_;

    // 设备状态
    uint8_t state_;
    enum FRONTDEV_MANAGER_STATE
    {
        FDM_STATE_CLOSE = 0,                //初始化状态
        FDM_STATE_QUERY_DEVICE,         //查询设备
        FDM_STATE_QUERY_DEVSTRATEGY,    //查询设备的策略
        FDM_STATE_CHECK_STRATEGY,       //检测录像策略
        FDM_STATE_START_RECORD,         //启动录像
        FDM_STATE_RUNNING,              //运行状态
        FDM_STATE_SHUTDOWN             //销毁状态
    };

    // 存储录像文件的前端设备列表
    SVS_DEVICE_MAP device_map_;
    SVS_LENS_PARAMETER_MAP lens_parameter_map_;
    ACE_Recursive_Thread_Mutex lens_parameter_map_mutex_;

    // 下一个内部ID
    ACE_Reactor *p_strategy_reactor_;
    CAC_Timer_Manager *p_strategy_timer_manager_;
    uint32_t next_internal_id_;
    ACE_Recursive_Thread_Mutex map_mutex_;

    // 请求序号与设备对象的映射, 这用于定时器
    SVS_STREAM_MAP request_map_;

#ifdef UNITTEST
    friend class CAC_Record_Inform_ManagerAutoMock;
    friend class TEST_CAC_Record_Inform_Manager;
#endif
};
#endif //_SVS_Record_Inform_h



