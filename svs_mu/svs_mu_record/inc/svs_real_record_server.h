#ifndef _Real_Record_Server_h
#define _Real_Record_Server_h
#include "svs_config.h"
#include "svs_record_server.h"
#include "svs_rt_client_handler.h"
#include "svs_rt_record_common.h"
#include "svs_real_record_task.h"
#include "svs_recv_rt_media_buffer.h"
#include "svs_mb_buffer.h"
#include <vector>
#include "svs_recv_rt_media_buffer.h"
using namespace std;

enum DELETE_RECORD_STRATEGY
{
    DELETE_RECORD_NONE = 0,
    DELETE_EXPIRE_RECORD,
    OVERWRITE_RECORD,
    DELETE_RECORD_MAX,  //在此之前添加新的类型.
};

enum RECORD_SERVER_EVENT_TYPE
{
    REFRESH_CONFIG_EVENT_TYPE = 0,
    REFRESH_LOAD_REPORT_INTERVAL_EVENT_TYPE,
    REPORT_RECORD_STATUS_INTERVAL_EVNET_TYPE,
    RECORD_SERVER_EVENT_MAX,
};

/**
 * @class CAC_RT_Record_Server
 * @brief 实时录像服务器类, 是单体类
 */
class CAC_RT_Record_Server : public CSVS_Record_Server
{
private:
    // 读取配置项时，使用的默认值类型,读取失败时的值有高位直接决定，超出范围值的部分，由低位字节决定
    typedef enum enConfDefaultValueType
    {
        // 如果获取到的值超出有效范围，可以有以下一种情况
        USE_NOTHING_OVER_RANGE    = 0x0001,
        USE_DEFAULT_VALUE_OVER_RANGE    = 0x0002,
        USE_LIKE_VALUE_OVER_RANGE       = 0x0003,

        // 如果配置项失败，可以有以下一种情况
        USE_NOTHING_GET_FAIL      = 0x0100,
        USE_DEFAULT_VALUE_GET_FAIL      = 0x0200,
        USE_DEFAULT_MAX,
    }ConfDefaultValueType;

    // 配置项属性
    typedef struct stConfAttribute
    {
        int32_t lMaxValue;
        int32_t lMinValue;
        int32_t lDefaultValue;
        int32_t enDefaultValueType;
    }ConfAttribute;

public: // 通用类公共接口
    UTAPI ~CAC_RT_Record_Server();
    // 单例
    static CAC_RT_Record_Server *instance()
    {
        static CAC_RT_Record_Server szServerManager;
        return &szServerManager;
    }

    /**
     * 启动实时录像服务：初始化所有资源 --- 内存、对象、线程等
     * 成功返回true, 否则返回false.
     */
    UTAPI bool open_service();

public:// 业务类共用接口

    // 磁盘管理部分上报告警通知
    int32_t   notify_exception(std::vector<string> &lens_vec, int32_t raid_id, int32_t error_code);

    // 添加负载上报任务
    void add_load_report_task();

    // 设置负载上报定时器
    UTAPI int32_t set_load_report_interval_timer();

    // 配置动态更新
    UTAPI int32_t refresh_configure();

    // 结束反应器
    UTAPI void end_reactor_event_loop();

public:// 获取配置类接口和入队列类接口
    // 获取注册消息
    void get_register_info(SVS_RECORDSVR_INTERNAL_REGISTER_REQ_MSG &szRegMsg);

    //功能:返回支持的录像最大镜头数
    UTAPI uint32_t max_lens() const
    {
        return this->max_lens_;
    }

    // 每个raid组所能支持的最大镜头书
    UTAPI uint32_t max_lens_per_raid()const
    {
        return this->max_lens_per_raid_;
    }

    // 本RtRecordSvr是否为默认服务器
    UTAPI bool is_default() const
    {
        return (1 == default_server_);
    }

    // 实时录像索引入库任务入队列
    UTAPI int32_t enqueue_record_index(   ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = signal_task_.enqueue_record_index(new_item, timeout);
        return enqueueResult;
    }


    // 重新打开索引队列
    UTAPI int32_t reopen_record_index_queue( void )
    {
        //重新打开队列
        int32_t openResult = signal_task_.reopen_record_index_queue();
        return openResult;
    }

    // 将来自录像控制子系统的更新录像策略消息插入到策略更新队列中
    UTAPI int32_t  enqueue_record_strategy(ACE_Message_Block *mb, ACE_Time_Value *timeout = 0)
    {
        return signal_task_.enqueue_record_strategy(mb, timeout);
    }

    // 系统监控到的事件入监控队列
    UTAPI int32_t enqueue_monitor_event(  ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = signal_task_.enqueue_monitor_event(new_item, timeout);
        return enqueueResult;
    }

    // 录像删除模式，等待路线过期删除或盘满覆盖
    UTAPI uint8_t delete_record_strategy() const
    {
        return this->delete_record_strategy_;
    }

    // 存储类型
    UTAPI uint8_t storage_type() const
    {
        return this->storage_type_;
    }

    // 录像调试模式，允许不写录像文件的录像模式，用于磁盘空间不充足的录像测试
    UTAPI int32_t debug_write_file() const
    {
        return debug_write_file_;
    }

    // 停止指定路由的录像
    UTAPI int32_t stop_record( const ACE_Message_Block *mb ) const;



    // 是否处于连接状态
    UTAPI bool is_store_svr_conn( void ) const
    {
        if(NULL == p_connector_)
        {
            return false;
        }

        return p_connector_->is_ready_status();
    }

    // 取服务器状态
    UTAPI int32_t state() const
    {
        if(NULL == p_connector_)
        {
            return 0;
        }

        return p_connector_->conn_status();
    }

    // 取服务器类型
    UTAPI uint8_t server_type() const
    {
        return server_type_;
    }

    // 功能:生成传输号
    UTAPI uint16_t transaction_no()
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, transaction_no_mutex_, 1);
        transaction_no_++;
        if( MSG_TRANSACTION_MAX_NUM < transaction_no_ )
        {
            transaction_no_ = 1;
        }
        return transaction_no_;
    }

    // 取接收媒体数据线程数
    UTAPI uint32_t recv_media_thread_num() const
    {
        return recv_media_thread_num_;
    }

    // 获取每个raid组写录像文件线程个数
    UTAPI uint32_t output_thread_num_per_raid() const
    {
        return output_thread_num_per_raid_;
    }

    // 获取总的写录像文件线程个数
    UTAPI uint32_t sync_work_raid_group_num() const
    {
        return sync_work_raid_group_num_;
    }

    //接收缓冲区大小
    uint32_t recv_media_buffer_size() const
    {
        return recv_media_buffer_size_;
    }

    //接收缓冲区个数
    uint32_t recv_media_buffer_num() const
    {
        return recv_media_buffer_num_;
    }

    // 媒体条带大小
    uint32_t write_media_data_stripe_width() const
    {
        return write_media_data_stripe_width_;
    }

    // 临时缓存区的缓存块数
    uint32_t tmpfs_recv_media_buffer_num() const
    {
        return tmpfs_recv_media_buffer_num_;
    }

    const char * tmpfs_path() const
    {
        return tmpfs_path_;
    }

    // 添加定时器回调处理消息
    UTAPI int32_t add_timer_callback_msg( ACE_Message_Block* msg )
    {
        return signal_task_.add_timer_callback_msg( msg );
    }

    // 获取最大事件录像时长
    UTAPI uint32_t event_record_lost_timeout() const
    {
        return event_record_lost_timeout_;
    }

    UTAPI uint32_t GetRecordTimetickFlag()const
    {
        return m_ulRecordTimeTick;
    }

    // 心跳消息回调
    static void timer_callback(  void *pArg,
                                           int32_t eventId,
                                           int32_t timerId,
                                           int32_t /*nDummy*/ )
    {
        uint32_t msgLen = sizeof(SVS_TIMER_CALL_BACK_MSG);
        ACE_Message_Block *mb =
            g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_EVENT_MSG, msgLen );
        if ( NULL == mb )
        {
            SVS_LOG((SVS_LM_ERROR,
                "failed to alloc mb. type[%d] size[%d]",
                CSVS_Mb_Buffer_Manager::MB_TYPE_EVENT_MSG,
                msgLen ));
            return ;
        }
        SVS_TIMER_CALL_BACK_MSG* pMsg = (SVS_TIMER_CALL_BACK_MSG*)(void*)mb->wr_ptr();
        pMsg->MsgType = SVS_TIMER_CALL_BACK_MSG::REOCRD_SERVER;
        pMsg->ProcObject = pArg;
        pMsg->EventId = eventId;
        pMsg->TimerId = timerId;
        mb->wr_ptr( msgLen );
        (void)CAC_RT_Record_Server::instance()->add_timer_callback_msg( mb );
   }

    // 获取录像索引文件存放根路径
    UTAPI const char * get_record_indexfile_path() const
    {
        return (char*)record_indexfile_path_;
    }

    // 获取旧录像索引文件存放根路径
    UTAPI const char * get_old_index_path() const
    {
        return (char*)old_index_path_;
    }

    // 获取录像索引文件存放根路径
    UTAPI const time_t get_index_path_update_time() const
    {
        return index_path_update_time_;
    }

    // 删除任务入队列
    UTAPI int32_t enqueue_del_task(ACE_Message_Block * new_item, ACE_Time_Value * timeout = 0)
    {
        int32_t enqueueResult = signal_task_.enqueue_del_task(new_item, timeout);
        return enqueueResult;
    }

    // 删除录像消息入队列
    UTAPI int32_t enqueue_del_record(ACE_Message_Block * new_item, ACE_Time_Value * timeout = 0)
    {
        int32_t enqueueResult = signal_task_.enqueue_del_record(new_item, timeout);
        return enqueueResult;
    }

    // 删除预录任务入队列
    UTAPI int32_t enqueue_prerecord_del_task(ACE_Message_Block * new_item, ACE_Time_Value * timeout = 0)
    {
        int32_t enqueueResult = signal_task_.enqueue_prerecord_del_task(new_item, timeout);
        return enqueueResult;
    }
    UTAPI void print_msg_del_task(const SVS_DEL_RECORD *del_task)const;

    UTAPI void print_msg_del_record(const SVS_DEL_RECORD_FILE *del_record) const;

    // 打印消息处理信息
    int32_t debug_get_msg_info(  char *strBufferInfo, const uint32_t szBufferInfo )const
    {
        (void)signal_task_.debug_get_msg_info( strBufferInfo, szBufferInfo  );
        return SVS_RESULT_OK;
    }

    UTAPI uint32_t get_check_thread_status_interval()const
    {
        return check_thread_status_interval_;
    }

    UTAPI uint32_t get_restart_server_flag()const
    {
        return restart_server_flag_;
    }

    UTAPI uint32_t get_dump_server_flag()const
    {
        return dump_server_flag_;
    }

    UTAPI uint32_t get_use_direct_io_flag() const
    {
        return use_direct_io_flag_;
    }

    UTAPI uint32_t get_write_media_flowctrl_flag()const
    {
        return write_media_flowctrl_flag_;
    }
    UTAPI uint32_t get_check_disk_interval()const
    {
        return check_disk_interval_;
    }

    UTAPI uint32_t test_bandwidth_total()const
    {
        return test_bandwidth_total_;
    }

    UTAPI uint32_t get_load_report_interval()const
    {
        return load_report_interval_;
    }
protected:
    // 单例模式的类，构造函数隐藏
    CAC_RT_Record_Server();

    /**
     *  初始化配置参数 -- 从配置文件读取参数并保存到内存中
     *  成功返回true,失败返回false.
     */
    UTAPI bool init_configure(const char * const) ;

    UTAPI bool init_system_configure();

    // 初始化录像删除相关配置项
    UTAPI bool init_delete_record_configure();

    // 初始化媒体接受和写线程配置项
    UTAPI bool init_media_thread_configure();

    // 初始化事件录像相关配置项
    UTAPI bool init_record_event_configure();

    // 初始化录像存储相关配置项
    UTAPI bool init_record_storagy_configure();

    // 初始化媒体缓存区相关配置项
    UTAPI bool init_media_buffer_configure();

    // 初始化录像索引路径相关配置项
    UTAPI bool init_record_index_configure();

    // 获取整数类型的配置型方法
    UTAPI bool get_conf_integer(const char * pUpperConfName,
                                    const char * pSubConfName,
                                    const ConfAttribute &szConfAttribute,
                                    int32_t & nOutPutValue)const;

    UTAPI int32_t get_conf_type(int32_t failValueType, int32_t defaultValueType)const;

protected:
    //运行标志
    bool b_running_;

    //录像删除策略
    uint8_t delete_record_strategy_;

    //存储类型
    uint8_t storage_type_;

    enum STORAGE_TYPE_EN
    {
        STORAGE_TYPE_MASTER = 0,  // 主用存储类型
        STORAGE_TYPE_SLAVE = 1,   // 备用存储类型
    };

    //最大镜头数
    uint32_t max_lens_;

    // 每个raid组最大支持的镜头数可配置
    uint32_t max_lens_per_raid_;

    // 配置文件被修改的最新时间
    time_t config_file_last_moded_time_;

    CAC_Signal_Task signal_task_;

    int32_t debug_write_file_;      //调试用--是否真正写录像文件标志 0:不写，其它:写

    uint16_t transaction_no_;

    ACE_Recursive_Thread_Mutex transaction_no_mutex_;

    uint32_t recv_media_buffer_size_;  //接收缓冲区大小
    uint32_t write_media_data_stripe_width_;// 条带大小

    uint32_t recv_media_buffer_num_;   //接收缓冲区个数
    uint32_t tmpfs_recv_media_buffer_num_;   // tmpfs接收缓冲区个数
    char tmpfs_path_[DISK_NAME_SIZE];   // tmpfs路径

    uint32_t recv_media_thread_num_;   //接收媒体流线程个数

    // 系统支持的同时工作的raid数
    uint32_t sync_work_raid_group_num_;

    // 每个raid组写录像线程数
    uint32_t output_thread_num_per_raid_;

    //录像事件丢失后，超时时间，单位：秒
    uint32_t event_record_lost_timeout_;

    // 录像索引和存储相关配置信息
    char record_indexfile_path_[DISK_NAME_SIZE + 1];
    // 升级前的索引路径
    char old_index_path_[DISK_NAME_SIZE + 1];

    // 升级时间
    time_t index_path_update_time_;

    char record_storage_path_[DISK_NAME_SIZE * SVS_LUN_MAX_NUM * SVS_RAID_MAX_NUM];

    // 停止录像的磁盘阈值，单位：兆
    uint32_t disk_lower_limit_;
    // 磁盘告警恢复阈值，单位：兆
    uint32_t disk_alarm_resume_limit_;
    // 磁盘告警阈值，单位：兆
    uint32_t disk_alarm_limit_;
    // 磁盘检测周期，单位：秒
    uint32_t check_disk_interval_;
    // 磁盘检测间隔
    uint32_t check_thread_status_interval_;


    // 重启服务标志
    uint32_t restart_server_flag_;
    // Dump服务标志
    uint32_t dump_server_flag_;
    // 使用DirectIO写文件的标志
    uint32_t use_direct_io_flag_;

    // 写录像流控开关,0为流控，非0为写录像不流控，默认不流控
    uint32_t write_media_flowctrl_flag_;

    // 负载上报检测周期
    uint32_t load_report_interval_;
    int32_t load_report_interval_timer_;

    // 测试开关，解决虚拟机获取不到网卡总带宽问题
    uint32_t test_bandwidth_total_;

    char str_conf_buffer_[DISK_NAME_SIZE * SVS_LUN_MAX_NUM * SVS_RAID_MAX_NUM];

    uint32_t m_ulRecordTimeTick;
#ifdef UNITTEST
    friend class TEST_CAC_RT_Record_Server;
    friend class CAC_RT_Record_Device_ManagerAutoMock;
    friend class CAC_RT_Record_ServerAutoMock;
#endif
};
#endif //_Real_Record_Server_h


