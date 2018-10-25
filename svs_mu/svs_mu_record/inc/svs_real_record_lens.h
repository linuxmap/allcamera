#ifndef _Real_Record_Lens_h
#define _Real_Record_Lens_h
#include <vector>
#include "svs_rt_record_common.h"
#include "svs_strategy_list.h"
#include "svs_record_index.h"
#include "svs_record_lens.h"

class CAC_Recv_Stream;

class SVS_File_Egress;

/**
 * @class CAC_Realtime_Record_Device
 * @brief 实时录像设备类
 */
class CAC_Realtime_Record_Device : public CSVS_Record_Lens
{
public:

    /**
     * 构造函数，镜头ID<strLensId>,编码器类型为<encoderType>,
     */
    CAC_Realtime_Record_Device( const char *strLensId,
                                uint32_t encoderType,
                                uint32_t nIndex );

    virtual ~CAC_Realtime_Record_Device();

    enum SVS_FRONT_DEVICE_STATE
    {
        // 值的大小按照转换先后顺序排列
        RECORD_STATE_INIT = 0,           //初始化状态
        RECORD_STATE_SLEEP,          //等待策略生效
        RECORD_STATE_REQUEST_ROUTER, //请求调度子系统分配VTDU
        RECORD_STATE_CONNECT_VTDU,   //连接VTDU，通知接收端口
        RECORD_STATE_RECEIVING_FILE, //接收文件流
        RECORD_STATE_DISK_FULL,
        RECORD_STATE_WAITING_OVER,   //等待结束
    };

    enum SVS_RECORD_TYPE
    {
        RECORD_TYPE_PROBID = 0, //禁止录像
        RECORD_TYPE_ALWAYS = 1, //连续时间录像
        RECORD_TYPE_EVENT = 2, //事件录象
        RECORD_TYPE_MANUAL = 3, // 手动录像
        RECORD_TYPE_MAX,
    };

    enum SVS_TIMER_TYPE
    {
        TIMER_TYPE_EVENT_RECORD_OVER = 1,   // 事件录像结束处理定时器
        TIMER_TYPE_EVENT_RECORD_LOST = 2,   // 录像事件丢失处理定时器
        TIMER_TYPE_RECORD_STATUS     = 3,   // 录像事件丢失处理定时器
    };

    // 获取一个随机请求路由时间
    UTAPI int32_t get_rand_request_router_time() const
    {
        ACE_OS::srand( (uint32_t)ACE_OS::time() );
        int32_t randTime = REQUEST_ROUTER_INTERVAL_TIME + ACE_OS::rand() % REQUEST_ROUTER_INTERVAL_TIME;
        return randTime;
    }

    // 复位设备对象
    UTAPI int32_t reset();


    // 初始化设备对象
    UTAPI bool init();

    // 设备下线处理 --- 关闭路由，将状态设置为停止状态，向媒体流
    // 接收线程和文件输出线程发送退出消息。
    UTAPI int32_t off_line();

    /**
     * 当镜头下线或者镜头没有设置录像策略时被被调用，停止录像，并通知媒体流
     * 接收线程和文件输出线程释放资源。
     * 成功返回0，否则返回-1.
     */
    UTAPI int32_t stop_device( bool bSendOverMsg = true );

    // 请求调度子系统分配路由
    UTAPI int32_t request_router();

    // 向调度子系统发送结束调度请求
    UTAPI int32_t shutdown_router(bool bGetlastError = false);

    /**
     * 处理路由消息，<pbyMsg>指向路由消息.
     * 成功返回0，失败返回-1
     */
    UTAPI int32_t handle_routerinfo(const void *pbyMsg);

    // 定时器时间处理
    UTAPI int32_t on_timer(int32_t eventId, int32_t timerId, int32_t nDummy);

    // 启动或者停止录像
    UTAPI int32_t control_record(uint8_t recordFlag, time_t endTime, time_t eventStratEndTime );

    // 录像事件发生
    UTAPI int32_t event_happen(   const char *strAlarmDeviceId,
                        uint32_t eventType,
                        time_t endTime, bool bIsRecord );

    // 录像事件结束
    UTAPI int32_t event_over(const char *strAlarmDeviceId, uint32_t eventType);

    // 更新录像镜头参数信息
    UTAPI int32_t update_record_parameter(    uint32_t nBeforeSeconds,
                                    uint32_t nAfterSeconds );

    // 调试镜头设备详细信息
    UTAPI int32_t debug_get_device_info( char *strDeviceInfo,
                                            uint32_t ulBuffLen );

    // 功能: 停止录像, 置设备状态为请求路由状态
    UTAPI int32_t stop_record();

    // 取路由id
    UTAPI routerid_t router_id()  const
    {
        return stream_id_;
    }

    // 取录像标志
    UTAPI int32_t record_flag()  const
    {
        return record_flag_;
    }

    // 取录像状态
    UTAPI uint8_t state()  const
    {
        return state_;
    }

    // 更新事件录像最大有效时常
    UTAPI int32_t refresh_event_record_outtime(int32_t shiftValue);

    //清除事件录像信息
    UTAPI int32_t clear_event_record_info();

    int32_t overwrite_record_notify(time_t newIndexStartTime);
protected:

    // 获取设备状态--输出参数 string
    UTAPI void debug_get_device_status(string & strDeviceStatus)const;

    // 获取设备录像状态标识--输出参数 string
    UTAPI void debug_get_record_flag(string & strRecordFlag)const;

    // 事件录像超时
    UTAPI void proc_event_record_over_time();

    // 事件录像丢失
    UTAPI int32_t proc_event_record_lost();

    // 取消事件结束处理定时器
    UTAPI int32_t kill_event_record_over_timer();

    // 设置事件结束处理定时器
    UTAPI int32_t set_event_record_over_timer(const ACE_Time_Value &delayTime );

    // 取消录像事件丢失处理定时器
    UTAPI int32_t kill_event_record_lost_timer();

    // 设置录像事件丢失处理定时器
    UTAPI int32_t set_event_record_lost_timer(const ACE_Time_Value &delayTime );

    // 录像类型转换
    UTAPI int32_t record_conversion(uint8_t newRecordFlag, bool & bStartRecord);

    // 预录结束
    UTAPI void prerecord_over(uint8_t newRecordFlag);

    // 连续录像或手动录像结束
    UTAPI void always_record_over(uint8_t newRecordFlag);

    // 是否存在有效的事件录像
    UTAPI bool is_event_record_effective()const
    {
        if ( (0 == event_record_before_seconds_)
          && (0 == total_event_)
          && (0 == event_over_time_))
        {
            return false;
        }

        return true;
    }

    // 手动录像结束
    UTAPI int32_t manual_record_over(const uint8_t newRecordFlag);

    /**
     * 启动录像，recordType -- 录像类型。
     * 被always_recording()和int evnet_recording()调用
     */
    UTAPI int32_t start_record(uint8_t recordType);

    // 禁止录像
    UTAPI int32_t probid_recording();

    // 向接收视频流的本地UDP SOCKET发送停止接收消息
    UTAPI int32_t send_stop_recv_msg();

    // 启动接收实时媒体流的UDP服务器，并向VTDU发送连接数据报，通知
    // VTDU将媒体流转发到实时录像服务器
    UTAPI int32_t notify_vtdu();

    // 发送手动录像状态通知消息给录像管理服务器
    UTAPI int32_t send_status_notify_msg(const int32_t recordStatus,uint8_t recordType);

private:
    // 接收流定时器回调
    UTAPI void recv_stream_on_timer();

    // 请求路由定时器回调
    UTAPI void request_router_on_timer();

    // 检测handle是否释放状态
    UTAPI void waiting_over_on_timer();

    // 连接VTDU定时器回调
    UTAPI void conn_vtdu_on_timer();

    // 设置录像状态
    UTAPI int32_t set_status(uint8_t newStatus);

    // 更新镜头的策略录像任务信息
    UTAPI void updateRecordTaskStartTime(uint8_t oldRecordFlag, uint8_t newRecordFlag, int32_t oldEventNum);

private:
    CAC_Realtime_Record_Device();
protected:
    //编码器类型
    uint32_t encoder_type_;

    //录像标志
    volatile uint8_t record_flag_;

    //事件开始时间
    volatile time_t event_begin_time_;

    //事件结束时间
    volatile time_t event_over_time_;

    //作为流ID，是关键字
    routerid_t stream_id_;

    // 路由申请超时时间，随机生成的，定时器检测的时候需要用到
    int32_t request_route_overtime_len_;

    //VTDU地址
    ACE_INET_Addr vtdu_addr_;

    uint32_t    m_ulIOHandlerIndex;

    // 状态起始时间
    time_t status_start_time_;

    //事件总数
    int32_t total_event_;

    vector <EVENTMAP > event_vector_;

    volatile uint32_t request_route_times_; //请求路由次数
    volatile uint32_t connect_vtdu_times_;  //连接VTDU次数
    volatile uint32_t connect_vtdu_timeout_times_;  //连接VTDU超时次数

    volatile uint32_t recv_media_data_timeout_times_;

    //===========事件录像相关参数======

    //事件前录像秒数
    time_t event_record_before_seconds_;

    //事件后录像秒数
    time_t event_record_after_seconds_;

    //事件开始时间
    time_t event_record_start_time_;

    //事件结束时间
    time_t event_record_end_time_;

    //事件录像结束定时器ID
    int32_t event_record_over_timer_id_;

    //录像事件丢失定时器ID
    int32_t event_record_lost_timer_id_;

    // 录像开始时间
    time_t  record_start_time_;

    SVS_Device_Stat device_stat_;

};

/**
 * @class CAC_RT_Record_Device_Manager
 * @brief 前端设备录像管理类
 */
class CAC_RT_Record_Device_Manager
{
protected:
    // 各种状态的设备个数
    typedef struct ST_SVS_STATUS_DEV_NUM
    {
        int32_t initStatusDeviceNum;             //初始化状态设备个数
        int32_t sleepStatusDeviceNum;            //休眠状态设备个数
        int32_t diskFullWaitingDeviceNum;         //磁盘满等待磁盘空间状态
        int32_t requestRouterStatusDeviceNum;   //请求路由状态设备个数
        int32_t connectVtduStatusDeviceNum;     //连接VTDU状态设备个数
        int32_t recvFileStatusDeviceNum;         //接收数据状态设备个数
        int32_t waittingOverStatusDeviceNum;    //等待结束状态设备个数
        int32_t stopStatusDeviceNum;             //设备停止状态设备个数
    }SVS_STATUS_DEV_NUM_T;

    // 镜头id和录像设备对象的map
    typedef ACE_Hash_Map_Manager < const char *, CAC_Realtime_Record_Device *, ACE_Null_Mutex >
        SVS_DEVICE_MAP;

    // 路由id和录像设备对象的map
    typedef ACE_Hash_Map_Manager <uint32_t, CAC_Realtime_Record_Device *, ACE_Null_Mutex>
        SVS_STREAM_MAP;

    // 镜头id和录像设备对象的容器
    typedef ACE_Hash_Map_Iterator < const char *, CAC_Realtime_Record_Device *, ACE_Null_Mutex >
        SVS_DEVICE_MAP_ITER;

    typedef ACE_Hash_Map_Entry < const char *, CAC_Realtime_Record_Device * >
        SVS_DEVICE_MAP_ENTRY;

public:
    // 实时录像设备管理类的单例
    static CAC_RT_Record_Device_Manager *instance()
    {
        static CAC_RT_Record_Device_Manager szDeviceManager;
        return &szDeviceManager;
    }
    /**
     * 初始化录像镜头管理类对象，nMaxLens指定录像最大镜头数
     * 成功返回true,失败返回false.
     */
    UTAPI bool init(uint32_t nMaxLens);

    // 通过前端id获取实时录像设备对象
    UTAPI CAC_Realtime_Record_Device * get_device(const char *strFrontDevId);

    // 通过内部id获取实时录像设备对象
    UTAPI CAC_Realtime_Record_Device * get_device(uint32_t nId);

    /**
     * 启动或者停止<strLensId>的录像，由<recordFlag>值决定。
     */
    UTAPI int32_t  control_record( const char * const strLensId,
                         uint8_t encoderType,
                         uint8_t recordFlag,
                         time_t endTime,
                         uint32_t eventBeforeSeconds,
                         uint32_t eventAfterSeconds,
                         time_t eventStratEndTime);

    /**
     * 来自事件源<strAlarmDeviceId>类型为<eventType>的录像事件，将启动镜头<strLensId>录像.
     */
    UTAPI int32_t event_happen(   const char * const strLensId,
                        const char *strAlarmDeviceId,
                        uint32_t eventType,
                        uint8_t encoderType,
                        time_t endTime,
                        uint32_t eventBeforeSeconds,
                        uint32_t eventAfterSeconds,
                        bool bIsRecord);

    /**
     * 来自<strAlarmDeviceId>类型为<eventType>的录像事件结束，将停止镜头<strLensId>录像.
     */
    UTAPI int32_t event_over( const char * const strLensId,
                    const char *strAlarmDeviceId,
                    uint32_t eventType );

    // 功能：取指定设备的基本信息
    UTAPI int32_t debug_get_device_info(  const char *strFrontDevId,
                                char *strDeviceInfo,
                                uint32_t szDeviceInfo );

    // 功能：取所有设备的基本信息
    UTAPI int32_t debug_get_all_device_info(  char *strDeviceInfo,uint32_t szDeviceInfo );

    // 功能：取所有设备的详细信息
    UTAPI int32_t debug_get_all_device_detail_info( char *strDeviceInfo, uint32_t szDeviceInfo );

    // 功能:关闭指定路由的录像路由
    UTAPI int32_t shutdown_router( const uint32_t RouterID );

    // 功能:根据指定的路由ID查找设备
    UTAPI CAC_Realtime_Record_Device * get_device_by_routerId( uint32_t RouterId );

    // 录像索引记录处理
    //UTAPI int32_t handle_record_index( list<ACE_Message_Block*> &recordIndexList ) const;

    /*
     * FUNCTION: void upload_routerinfo()
     * DESCRIPTION: 处理前端设备上传录像文件的路由请求的应答消息
     * INPUT:
     *    ACE_Message_Block *mb -- 消息体
     * OUTPUT:
     *   none
     */
    UTAPI int32_t handle_routerinfo(const ACE_Message_Block *mb);


   // 关闭所有设备的路由
    UTAPI void shutdown_router();


   //  启动指定设备录像
    int32_t start_device_record(const char * const strDevId);

    /*
     * FUNCTION: int32_t stop_device_record()
     * DESCRIPTION: 停止指定设备录像 --- 由于没有录像策略了
     * INPUT:
     *   const char * const strDevId  --- 设备ID
     * OUTPUT:
     *   成功返回0，失败返回错误代码
     */
    UTAPI int32_t stop_device_record( const char * const strDevId,
                            bool bSendOverMsg = true );

    /*
     * FUNCTION: int32_t device_off_line()
     * DESCRIPTION: 处理设备下线.
     * INPUT:
     *   const char * const strDevId  --- 设备ID
     * OUTPUT:
     *   成功返回0，失败返回错误代码
     */
    UTAPI int32_t device_off_line(const char * const strDevId);


   // 将指定对象的引用计数减１，如果此对象已经为停止状态且引用计数为０，
   // 那么将销毁此对象。
    UTAPI int32_t decrease_reference(CAC_Realtime_Record_Device *pDevice);

    //功能：获取当前正在录像的设备个数
    UTAPI uint32_t get_recording_device_num( void )
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, 0);
        return device_map_.current_size();
    }

    // 盘满覆盖录像时修改了数据库后通知正在录像的对象
    UTAPI int32_t overwrite_record_notify(char * strLensId,
                                         time_t newIndexStartTime);

    //定时器时间处理
    UTAPI int32_t on_timer(int32_t eventId, int32_t timerId, int32_t nDummy);

    // 定时器回调函数
    static void timer_callback( void *pArg,
                                int32_t eventId,
                                int32_t timerId,
                                int32_t nDummy );


    // 最大事件录像动态更新通知接口
    UTAPI int32_t refresh_event_record_outtime(int32_t shiftValue);

protected:
    // 单例模式的构造函数，使用protected类型
    CAC_RT_Record_Device_Manager();
    UTAPI ~CAC_RT_Record_Device_Manager();

    // 添加一个实时录像设备对象
    UTAPI int32_t add_device( const char *strFrontDevId,
                    uint8_t encoderType,
                    CAC_Realtime_Record_Device *&pDevice );

    // 根据传入的录像索引消息生成SQL语句
    UTAPI int32_t build_update_record_index_sql(  const ACE_Message_Block* mb,
                                        char *sqlBuff,
                                        uint32_t sqlBuffSize) const;

    // 根据设备状态归类失败
    UTAPI void count_dev_num_by_status(
                                    const CAC_Realtime_Record_Device * pDevice,
                                    SVS_STATUS_DEV_NUM_T & devStateNum)const;

    UTAPI int32_t bind2readonlyDeviceMap(CAC_Realtime_Record_Device* pDevice);
protected:
    // 最大镜头数
    uint32_t max_lens_;

    // 镜头ID与录像对象映射表
    SVS_DEVICE_MAP device_map_;

    // 正在实时录像的设备对象数据
    vector <CAC_Realtime_Record_Device *> using_vector_;
    ACE_DLList <char> free_list_;

    ACE_Recursive_Thread_Mutex map_mutex_;

    // 用于定时器检测用map，不会释放对象，与device map分开，减小锁的竞争
    SVS_DEVICE_MAP readonly_device_map_;
    ACE_Recursive_Thread_Mutex readonly_device_map_mutex_;

#ifdef UNITTEST
    friend class TEST_CAC_RT_Record_Device_Manager;
    friend class CAC_RT_Record_Device_ManagerAutoMock;
#endif
};
#endif //_Real_Record_Lens_h


