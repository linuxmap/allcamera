#ifndef _Real_Record_Task_h
#define _Real_Record_Task_h
#include "svs_monitor_event.h"
#include "svs_debug_accept_handler.h"
#include "svs_recv_stream_factory.h"
#include "svs_timer.h"
#include "ace/Dev_Poll_Reactor.h"
#include "svs_rt_client_handler.h"
#include "svs_mb_buffer.h"
#include "svs_delete_record_manager.h"
#include <vector>
using namespace std;

extern ACE_TSS <CSVS_Thread_Msg_Buffer_Manager> g_p_msg_buffer;

class CAC_Signal_Task  : public ACE_Task <ACE_MT_SYNCH>
{
public:
    typedef ACE_Acceptor <CSVS_Debug_Accept_Handler, ACE_SOCK_ACCEPTOR> SVS_LOG_ACCEPTOR;
    CAC_Signal_Task();
    virtual ~CAC_Signal_Task();

    UTAPI int32_t close(u_long);

    // 当在open方法中激活(activate)了服务后，便启动线程。
    UTAPI int32_t  svc(void);

    UTAPI int32_t open(void *);

    //处理监控事件回调函数
    UTAPI int32_t  handle_monitor_event(const ACE_Message_Block *mb) const;

    //停止所有线程
    UTAPI void stop();

    //监控事件入队列
    UTAPI int32_t enqueue_monitor_event( ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = monitor_event_queue_.enqueue_tail(new_item, timeout);
        return enqueueResult;
    }

    // 删除任务入队列
    UTAPI int32_t enqueue_del_task( ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = del_task_queue_.enqueue_tail(new_item, timeout);
        return enqueueResult;
    }

    // 删除入队列
    UTAPI int32_t enqueue_del_record( ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = del_record_queue_.enqueue_tail(new_item, timeout);
        return enqueueResult;
    }

    // 删除入队列
    UTAPI int32_t enqueue_prerecord_del_task( ACE_Message_Block *new_item,
                                                 ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = del_prerecord_task_queue_.enqueue_tail(new_item, timeout);
        return enqueueResult;
    }

    //录像索引入队列
    UTAPI int32_t enqueue_record_index(   ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = rindex_queue_.enqueue_tail(new_item, timeout);

        return enqueueResult;
    }

    // 重新打开录像索引队列
    UTAPI int32_t reopen_record_index_queue( void )
    {
        //重新打开队列
        (void)rindex_queue_.close();
        int32_t openResult = rindex_queue_.open(RECORD_FILE_INDEX_QUEUE_WM,
                                            RECORD_FILE_INDEX_QUEUE_WM);
        return openResult;
    }

    //录像索引入队列
    UTAPI int32_t enqueue_load_report( ACE_Message_Block *new_item,
                                        ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = load_report_queue_.enqueue_tail(new_item, timeout);
        return enqueueResult;
    }

    // 将来自录像控制子系统的更新录像策略消息插入到策略更新队列中
    UTAPI int32_t  enqueue_record_strategy(ACE_Message_Block *mb, ACE_Time_Value *timeout = 0)
    {
        return renew_strategy_queue_.enqueue_tail( mb, timeout);
    }

    // 重新打开负载上报队列
    UTAPI int32_t reopen_load_report_queue( void )
    {
        //重新打开队列
        (void)load_report_queue_.close();
        int32_t openResult = load_report_queue_.open();
        return openResult;
    }

    //功能：设置远程调试端口
    UTAPI void remote_debug_port( uint16_t debug_port )
    {
        remote_debug_port_ = debug_port;
    }

    // 添加定时器回调消息
    UTAPI int32_t add_timer_callback_msg( ACE_Message_Block* msg );

    // 消息处理调试信息
    UTAPI int32_t debug_get_msg_info(  char *strBufferInfo,
                             const uint32_t szBufferInfo )const;

    //初始化一些定时器和消息队列
    int32_t init_task();
protected:
    // 根据配置文件中配置的周期，定时操作数据库，达到定期保活数据库连接的作用
    int32_t db_heartbeart_thread()const;

    // 负载上报任务处理线程
    int32_t load_report_thread();

    // 分发接收到的消息
    UTAPI void handle_message(uint32_t nThrIndex, ACE_Message_Block *mb);

    // 处理来自StoreSvr的错误消息
    UTAPI void handle_error_msg(ACE_Message_Block *mb) const;

    // 将来自录像控制子系统的更新录像策略消息插入到策略更新队列中
    UTAPI int32_t  putinto_renew_strategy(const ACE_Message_Block *mb);

    UTAPI void proc_del_record_msg(const ACE_Message_Block *mb);

    // 连接中心服务平台中的其他服务器
    UTAPI int32_t  start_connect();

    //定时器处理线程
    UTAPI int32_t  timer_event_loop();

    //监控事件处理线程
    UTAPI int32_t  monitor_event_proc_thread();

    //更新录像策略线程
    UTAPI int32_t  renew_strategy_loop();

    //录像策略检测线程。实时检测策略是否生效，
    UTAPI int32_t  strategy_check_thread();

    //更新录像文件索引线程
    UTAPI int32_t  update_record_thread();

    //功能:消息处理线程
    UTAPI int32_t proc_message_thread( uint32_t nIndex );

    //调试线程
    UTAPI int32_t  debug_thread();

    //定时器回调处理线程
    UTAPI int32_t timer_call_back_thread();

    // 将来自StoreSvr前端录像备份消息，加入前端录像备份任务消息队列
    UTAPI int32_t putinto_bakup_msg( ACE_Message_Block *mb);

    // 前端录像备份任务处理线程
    UTAPI int32_t proc_bakup_msg_loop();

    // 处理录像状态上报定时任务线程
    UTAPI int32_t procReportRecordStatusTaskThread();
    UTAPI void addReportRecordStatusTask();

    //定时器处理线程2
    UTAPI int32_t timer_call_back_second_thread();

    // 定时回调录像状态检测线程
    UTAPI int32_t  timer_call_back_record_thread();

    // 线程：处理删除任务线程，初始化删除任务，未经流控处理的删除任务
    UTAPI int32_t proc_del_record_task_thread();

    // 线程：删除录像文件线程，流控后的删除任务
    UTAPI int32_t del_record_file_loop();

    // 线程：处理删除预录文件
    UTAPI int32_t del_prerecord_file_loop();

    // 线程：删除过期日期目录
    UTAPI int32_t del_date_dir_loop();

    // 定时器处理函数:过期录像删除，间隔周期到后检测是否有需要删除的录像
    //UTAPI void handle_del_timer_timetask();

    // 定时器处理函数:处理具体的删除任务，流控周期到了执行
    UTAPI void handle_del_timer_control();

    // 定时器处理函数:处理删除预录任务
    UTAPI void handle_del_timer_prerecord();

    // 定时器处理函数:处理过期日期目录删除定时器到期后，执行删除任务
    UTAPI void handle_del_timer_date_dir();

    // 定时器回调删除，根据定时器事件类型分流
    static void del_record_timer_callback( void *pArg,
                            int32_t eventId,
                            int32_t timerId,
                            int32_t nDummy );

    // 打开录像和系统相关消息队列
    UTAPI int32_t open_queue();

    // 打开删除相关消息队列
    UTAPI int32_t open_del_queue();

    // 创建定时器
    UTAPI int32_t create_timer();

    // 创建删除相关定时器
    UTAPI int32_t create_del_timer();

    // 创建接收媒体反应器组
    UTAPI int32_t create_recv_media_reactor_vector();

    // 打开
    UTAPI int32_t open_record_reactor();

    //功能:生成线程号
    UTAPI int32_t add_index()
    {
        int32_t oldIndex;

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, SVS_RESULT_FAILURE);
        oldIndex = thr_index_++;
        return oldIndex;
    }

    //功能:打开远程调试接收器
    UTAPI int32_t open_debug_acceptor();

    // 创建系统线程
    UTAPI int32_t create_thread_system(int32_t threadIndex);

    // 创建录像相关线程
    UTAPI int32_t create_threads_Record(int32_t threadIndex);

    // 创建删除相关线程
    UTAPI int32_t create_threads_del(int32_t threadIndex);

    // 创建媒体接收和发送相关线程
    UTAPI int32_t create_threads_media(int32_t threadIndex);

    void handle_del_msg(ACE_Message_Block* mb, bool& bFree);
    void proc_del_prerecord_msg(const ACE_Message_Block* mb)const;
    void proc_del_date_dir_msg(ACE_Message_Block* mb);

protected:
    enum THREAD_INDEX
    {
        CONNECT_THREAD = 0,
        TIMER_CALL_BACK_THREAD,     //定时器回调处理线程
        TIMER_THREAD,
        MONITOR_EVENT_PROC_THREAD,
        RENEW_STRATEGY_THREAD,
        STRATEGY_CHECK_THREAD,     //策略检测线程
        UPDATE_RECORD_THREAD,
        DEBUG_THREAD,              //调试线程
        PROC_BAKUP_TASK_THREAD,   //前端录像备份任务处理线程
        PROC_REPORT_RECORD_STATUS_TASK_THREAD,   // 处理录像状态上报定时任务
        TIMER_CALL_BACK_SECOND_THREAD,    //定时器回调处理线程2
        TIMER_CALL_BACK_RECORD_THREAD,    //定时器回调处理录像状态检测线程
        //PROC_ALARM_EVENT_THREAD,    //告警事件处理线程
        PROC_DEL_RECORD_TASK_THREAD,  // 构造删除任务线程
        DEL_RECORD_FILE_THREAD,   //  执行删除任务线程
        DEL_PRERECORD_FILE_THREAD, // 删除预录文件线程
        DEL_DATE_DIR_THREAD, // 删除日期目录线程
        LOAD_REPORT_THREAD,
        MAX_THREAD
    };

    enum RT_THREAD_INDEX_MATCH
    {
        RT_THREAD_INDEX_UNFOUND = 99
    };

    enum
    {
        USE_DB_CONN_THREAD_NUM = 12  //需要使用数据库连接的线程数
    };

    enum SVC_THREAD_CONST
    {
        SVC_NUM = 1
    };

    enum
    {
        RT_LENS_TIMER_MULTIPLE = 3    // 每个前端需要的定时器个数
    };

    enum  SVS_DEL_TIMER_TYPE
    {
        SVS_DEL_TIMER_TIMETASK  = 1,     // 删除定时器之定时删除任务
        SVS_DEL_TIMER_CONTROL   = 2,     // 删除定时器之流量控制
        SVS_DEL_TIMER_PRERECORD = 3,
        SVS_DEL_TIMER_DATE_DIR = 4,
        SVS_DEL_TIMER_TYPE_MAX
    };

    enum
    {
        SVS_THREAD_MAX_NUM = 50
    };

    enum
    {
        MSG_SORT_ROUTER_INFO = 0,   // 路由通知
        MSG_SORT_SCHEDULE_OVER_REQ, // 路由结束
        MSG_SORT_PLATDEV_INFORM_REQ,// 设备状态通知
        MSG_SORT_ALARM_INFO,        // 告警事件
        SVS_MSG_SORT_MAX_NUM
    };

protected:

    // 线程序号
    int32_t thr_index_;

    // 服务运行状态
    volatile bool brunning_;

    ACE_Recursive_Thread_Mutex mutex_;

    // 监控消息队列
    ACE_Message_Queue <ACE_SYNCH> monitor_event_queue_;

    // 策略更新消息队列
    ACE_Message_Queue <ACE_SYNCH> renew_strategy_queue_;

    // 备份前端录像任务消息队列
    ACE_Message_Queue <ACE_SYNCH> bakup_task_queue_;

    // 实时录像索引队列
    ACE_Message_Queue <ACE_SYNCH> rindex_queue_;

    // 负载上报任务队列
    ACE_Message_Queue <ACE_SYNCH> load_report_queue_;

    // 连接反应器，用于连接StoreSvr
    ACE_Reactor *p_connect_reactor_;
    // 连接epoll反应器，用于连接StoreSvr
    ACE_Dev_Poll_Reactor* p_connect_epoll_reactor_;

    // 定时器反应器
    ACE_Reactor *p_timer_reactor_;

    // 策略反应器，用于录像通知
    ACE_Reactor *p_strategy_reactor_;

    // 用于录像通知
    CAC_Timer_Manager *p_strategy_timer_manager_;

    ACE_Condition <ACE_Thread_Mutex> *p_cond_;
    ACE_Thread_Mutex cond_mutex_;

    // 调试线程反应器
    ACE_Reactor *p_debug_reactor_;
    // 远程调试接收器
    SVS_LOG_ACCEPTOR *p_debug_acceptor_;
    // 远程调试epoll反应器
    ACE_Dev_Poll_Reactor* p_debug_epoll_reactor_;
    //远程调试端口
    uint16_t remote_debug_port_;

    //已初始化标志
    volatile bool inited_flag_;

    // 定时器回调处理队列
    ACE_Message_Queue <ACE_SYNCH> timer_call_back_queue_;
    // 定时器回调处理队列
    ACE_Message_Queue <ACE_SYNCH> timer_call_back_queue_second_;
    // 录像状态检测定时器
    ACE_Message_Queue <ACE_SYNCH> timer_call_back_queue_record_;

    // 告警事件队列
    //ACE_Message_Queue <ACE_SYNCH> alarm_event_queue_;

    // 消息个数统计
    uint32_t msg_total_count_[SVC_NUM];
    // 分类消息个数统计
    uint32_t msg_sort_msg_count[SVC_NUM][SVS_MSG_SORT_MAX_NUM];
    CSVS_Thread_Msg_Buffer_Manager* p_msg_buffer_manager_[SVS_THREAD_MAX_NUM];
    // 线程ID列表
    ACE_hthread_t thread_id_list_[SVS_THREAD_MAX_NUM];

    // 所有删除任务加入此队列
    ACE_Message_Queue <ACE_SYNCH> del_task_queue_;

    // 存放删除预录任务暂时缓存队列
    ACE_Message_Queue <ACE_SYNCH> del_prerecord_task_queue_;

    // 对初始删除任务处理后，生成对具体镜头的删除消息加入此队列
    ACE_Message_Queue <ACE_SYNCH> del_record_queue_;

    // 加入流控后消息加入此队列
    ACE_Message_Queue <ACE_SYNCH> del_record_file_queue_;

    // 删除预录文件放入此队列
    ACE_Message_Queue <ACE_SYNCH> del_prerecord_file_queue_;

    // 清理日期目录定时器
    ACE_Message_Queue <ACE_SYNCH> del_date_dir_queue_;

    int32_t m_recordStatusCollectTimer;
    ACE_Message_Queue <ACE_SYNCH> m_recordStatusCollectQueue;

    // 删除任务管理类
    CAC_Delete_Record_Manager del_manager_;

    // 删除流控任务统计
    uint32_t time_control_count;

#ifdef UNITTEST
    friend class CAC_Signal_TaskAutoMock;
    friend class TEST_CAC_Signal_Task;
#endif
};

#endif //_Real_Record_Task_h

