#include "vms/vms.h"
#include "svs_private_msg.h"
#include "svs_config.h"
#include "svs_timer.h"
#include "svs_log_msg.h"

#include "svs_stat_manager.h"
#include "svs_record_disk.h"
#include "svs_recv_stream.h"
#include "svs_recv_stream_factory.h"
#include "svs_record_index.h"
#include "svs_real_record_task.h"
#include "svs_record_index.h"
#include "svs_real_record_lens.h"
#include "svs_record_inform.h"
#include "svs_real_record_server.h"
#include "svs_mb_buffer.h"
#include "svs_rt_record_common.h"
#include "svs_bakup_front_record_lens.h"
#include "svs_daemon_thread.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"
#include "svs_index_resend_mgr.h"
#include "svs_media_recv_manager.h"
#include "svs_console_server.h"

ACE_TSS <CSVS_Thread_Msg_Buffer_Manager> g_p_msg_buffer;

CAC_Signal_Task::CAC_Signal_Task() : thr_index_(0)
{
    brunning_           = true;

    p_connect_reactor_  = NULL;
    p_timer_reactor_    = NULL;

    p_debug_reactor_    = NULL;
    p_debug_acceptor_   = NULL;

    p_cond_             = NULL;

    inited_flag_        = false;

    p_debug_epoll_reactor_      = NULL;
    p_connect_epoll_reactor_    = NULL;
    p_strategy_reactor_         = NULL;
    p_strategy_timer_manager_   = NULL;

    remote_debug_port_          = SVS_DEFALT_REMOTE_DEBUG_PORT;

    time_control_count = 0;
    (void)ACE_OS::memset( msg_total_count_, 0, sizeof(msg_total_count_) );
    (void)ACE_OS::memset( msg_sort_msg_count, 0, sizeof(msg_sort_msg_count) );
    (void)ACE_OS::memset( thread_id_list_, 0, sizeof(thread_id_list_) );
    (void)ACE_OS::memset( p_msg_buffer_manager_, 0, sizeof(p_msg_buffer_manager_) );
    m_recordStatusCollectTimer = -1;
}

CAC_Signal_Task::~CAC_Signal_Task()
{
    try
    {
        (void)monitor_event_queue_.close();
        (void)renew_strategy_queue_.close();
        // 关闭备份任务消息队列
        (void)bakup_task_queue_.close();

        // 释放内存
        SVS_DELETE( p_connect_reactor_ );
        SVS_DELETE( p_timer_reactor_ );
        SVS_DELETE( p_debug_acceptor_ );
        SVS_DELETE( p_debug_reactor_ );
        SVS_DELETE( p_cond_ );

        SVS_DELETE( p_debug_epoll_reactor_ );
        SVS_DELETE( p_connect_epoll_reactor_ );

        SVS_DELETE( p_strategy_reactor_ );
        SVS_DELETE( p_strategy_timer_manager_ );
    }
    catch(...)
    {
    }
}

/*****************************************************************************
 函 数 名  : init_task
 功能描述  : 初始化各个反应器,消息队列
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0，失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::init_task()
{
    SVS_TRACE();

    // 打开消息队列，整理圈复杂度提取函数
    if (SVS_RESULT_OK != open_queue())
    {
        return SVS_RESULT_FAILURE;
    }

    // 创建连接存储管理服务器epoll反应器
    try
    {
        p_connect_epoll_reactor_ = new ACE_Dev_Poll_Reactor(SVS_EPOLL_MAX_HANDLE_NUM);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Create the epoll reactor for connecting StroreSvr failed."));
        return SVS_RESULT_FAILURE;
    }

    try
    {
        p_connect_reactor_ = new ACE_Reactor(p_connect_epoll_reactor_);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Create the reactor for connecting StoreSvr failed."));

        return SVS_RESULT_FAILURE;
    }

    // 注册反应器
    (void)CAC_RT_Record_Server::instance()->connect_reactor(
        p_connect_reactor_,
        msg_queue());

    // 创建定时器反应器
    try
    {
        p_timer_reactor_ = new ACE_Reactor;
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Create the reactor of timer failed."));

        return SVS_RESULT_FAILURE;
    }

    // 注册定期器反应器
    bool initResult = CAC_Timer_Manager::instance()->init(p_timer_reactor_);
    if ( !initResult )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Initialize the reactor of timer failed."));

        return SVS_RESULT_FAILURE;
    }

    // 新增索引重发类
    int32_t iRet = CIndexResendMgr::instance()->Init(CAC_Timer_Manager::instance());
    if (0 != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Open signal task failed, as init index resend manager error, "
            "return code[%d].",
            iRet));
        return SVS_RESULT_FAILURE;
    }

    //打开调试监听器
    if ( SVS_RESULT_OK != open_debug_acceptor())
    {
        return SVS_RESULT_FAILURE;
    }

    // 打开策略更新反应器
    if (SVS_RESULT_OK != open_record_reactor())
    {
        return SVS_RESULT_FAILURE;
    }

    // 打开删除定时器
    if (SVS_RESULT_OK != create_timer())
    {
        return SVS_RESULT_FAILURE;
    }

    // 创建条件锁
    try
    {
        p_cond_ = new ACE_Condition<ACE_Thread_Mutex>(cond_mutex_);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Create the Condition object failed."));
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_DEBUG,"Initiate signal task success."));

    return SVS_RESULT_OK;

}

/*****************************************************************************
 函 数 名  : open
 功能描述  : 初始化各个反应器,启动服务线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回非-1，失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::open(void *)
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO, "Start all threads..."));

    // 初始化消息接收管理器,包含接收线程和数据缓冲区初始化
    if (0 != SVS_Media_Recv_Manager::instance().open(NULL))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the media receive manager failed."));

        return SVS_RESULT_FAILURE;
    }

    // 初始化控制台服务类
    if(SVS_RESULT_OK != SVS_Console_Server::instance()->open(NULL))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the Console server failed."));

        return SVS_RESULT_FAILURE;
    }

    // 打开磁盘管理器的处理线程
    if (SVS_RESULT_OK != SVS_Disk_Manager::instance().open(NULL))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the disk  manager failed."));

        return SVS_RESULT_FAILURE;
    }

    // 打开统计模块线程
    if(SVS_RESULT_OK != SVS_Stat_Manager::instance().open(NULL))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the stat  manager failed."));

        return SVS_RESULT_FAILURE;
    }

    //启动服务线程
    uint32_t runThreadNum = (uint32_t)MAX_THREAD + (uint32_t)SVC_NUM;
    //修改线程栈大小-----------------
    size_t *stack_size = NULL;
    try
    {
        stack_size = new size_t[runThreadNum];
    }
    catch(...)
    {
        SVS_LOG( (SVS_LM_CRITICAL, "Allocate memory failed." ) );
        SVS_DELETE(stack_size, SVS_DELETE_MULTI);
        return SVS_RESULT_FAILURE;
    }

    // 初始化线程栈最大值
    uint32_t i = 0;
    for ( i=0; i<runThreadNum; i++)
    {
        stack_size[i] = SVS_THREAD_STATCK_MAX;
    }

    //创建接收线程和发送线程
    int32_t activateResult = activate(
         THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED,
         (int32_t)runThreadNum,
         0,
         ACE_DEFAULT_THREAD_PRIORITY,
         -1,
         0,
         0,
         0,
         stack_size,
         0 );

    SVS_DELETE( stack_size, SVS_DELETE_MULTI );

    inited_flag_ = true;

    return activateResult;
}

/*****************************************************************************
 函 数 名  : stop
 功能描述  : 初始化各个反应器,启动服务线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : NA
 修改历史  :
*****************************************************************************/
void CAC_Signal_Task::stop()
{
    SVS_TRACE();
    if( !inited_flag_ )
    {
        SVS_LOG((SVS_LM_ERROR, "Stop services failed.System has not been initialized."));
        return ;
    }

    SVS_LOG((SVS_LM_WARNING, "Start tp stop services..."));

    brunning_ = false;
    if (NULL != p_cond_)
    {
        (void)p_cond_->signal();
    }

    SVS_LOG((SVS_LM_WARNING, "Close the first message queue of timer callback."));
    (void)timer_call_back_queue_.close();

    SVS_LOG((SVS_LM_WARNING, "Close the second message queue of timer callback."));
    (void)timer_call_back_queue_second_.close();

    SVS_LOG((SVS_LM_WARNING, "Close the record message queue of timer callback."));
    (void)timer_call_back_queue_record_.close();

    if (NULL != p_connect_reactor_)
    {
        SVS_LOG((SVS_LM_WARNING, "Stop the reactor for connecting StoreSvr ."));
        (void)p_connect_reactor_->end_reactor_event_loop();
        (void)p_connect_reactor_->notify();
    }

    if (NULL != p_timer_reactor_)
    {
        SVS_LOG((SVS_LM_WARNING, "Stop the reactor of timer."));
        (void)p_timer_reactor_->end_reactor_event_loop();
        (void)p_timer_reactor_->notify();
    }

    //停止调试监听器
    if (NULL != p_debug_acceptor_)
    {
        (void)p_debug_acceptor_->close();
    }

    if (NULL != p_debug_reactor_)
    {
        SVS_LOG((SVS_LM_WARNING, "Stop the reactor for remote debugging."));
        (void)p_debug_reactor_->end_reactor_event_loop();
        (void)p_debug_reactor_->notify();
    }

    if (NULL != p_strategy_reactor_)
    {
        SVS_LOG((SVS_LM_WARNING, "Stop the reactor of record strategy."));
        (void)p_strategy_reactor_->end_reactor_event_loop();
        (void)p_strategy_reactor_->notify();
    }

    SVS_LOG((SVS_LM_WARNING, "Close the queue for renewing record strategy."));
    (void)renew_strategy_queue_.close();

    SVS_LOG((SVS_LM_WARNING, "Close the queue for monitoring event."));
    (void)monitor_event_queue_.close();

    SVS_LOG((SVS_LM_WARNING, "Close the queue for backuping front record task."));
    (void)bakup_task_queue_.close();

    SVS_LOG((SVS_LM_WARNING, "Close the thread for handling message."));
    (void)msg_queue()->close();

    SVS_LOG((SVS_LM_WARNING, "Stop the static reactor."));
    (void)ACE_Reactor::instance()->end_reactor_event_loop();
    (void)ACE_Reactor::instance()->notify();


    SVS_LOG((SVS_LM_WARNING, "Close the queue for realtime recording index."));
    (void)rindex_queue_.close();

    SVS_LOG((SVS_LM_WARNING, "Close the queue for load report."));
    (void)load_report_queue_.close();

    //等待所有线程退出
    (void)wait();
    SVS_LOG((SVS_LM_WARNING, "Stop RtRecordSvr succeeded."));

}

int32_t CAC_Signal_Task::close(u_long)
{
    return 0;
}

/*****************************************************************************
 函 数 名  : svc
 功能描述  : 启动线程，所有服务线程入口
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  : 增加B035需求，增加前端录像备份消息处理线程
*****************************************************************************/
int32_t CAC_Signal_Task::svc()
{
    SVS_TRACE();

    (void)signal(SIGPIPE, SIG_IGN);

    g_p_msg_buffer->init();

    ACE_hthread_t currThreadID = (ACE_hthread_t)ACE_OS::thr_self();

    SVS_LOG((SVS_LM_WARNING, "The current thread id is [%u, %d]",
        currThreadID,
        currThreadID ));

    int32_t threadIndex = add_index();

    p_msg_buffer_manager_[threadIndex] = g_p_msg_buffer;
    thread_id_list_[threadIndex] = currThreadID;

    int32_t runResult = 0;

    //系统线程
    runResult = create_thread_system(threadIndex);

    //匹配是否为录像相关线程
    if(RT_THREAD_INDEX_UNFOUND == runResult)
    {
        runResult = create_threads_Record( threadIndex);
    }

    //匹配是否是删除相关线程
    if (RT_THREAD_INDEX_UNFOUND == runResult)
    {
        runResult = create_threads_del(threadIndex);
    }

    //匹配是否为media接收相关线程
    if(RT_THREAD_INDEX_UNFOUND == runResult)
    {
        runResult = create_threads_media( threadIndex);
    }

    g_p_msg_buffer->clear();

    return runResult;
}

/*****************************************************************************
 函 数 名  : create_thread_system
 功能描述  : 创建系统类的线程，如定时器回调、通讯与StoreSvr等线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::create_thread_system(int32_t threadIndex)
{
    SVS_TRACE();

    int32_t findResult = RT_THREAD_INDEX_UNFOUND;

    switch (threadIndex)
    {
        // 连接录像控制子系统线程: 同时接收来自录像控制子系统的消息
        case CONNECT_THREAD:
            findResult = start_connect();
            break;

        // 定时器回调处理线程
        case TIMER_CALL_BACK_THREAD:
            findResult = timer_call_back_thread();
            break;

        // 定时器线程
        case TIMER_THREAD:
            findResult = timer_event_loop();
            break;

        // 监控事件处理线程
        case MONITOR_EVENT_PROC_THREAD:
            findResult = monitor_event_proc_thread();
            break;

        // 调试线程
        case DEBUG_THREAD:
            findResult = debug_thread();
            break;

        // 回调消息处理线程
        case TIMER_CALL_BACK_SECOND_THREAD:
            findResult = timer_call_back_second_thread();
            break;

        // 定时器回调处理录像状态检测线程
        case TIMER_CALL_BACK_RECORD_THREAD:
            findResult = timer_call_back_record_thread();
            break;

        // 负载上报线程
        case LOAD_REPORT_THREAD:
            findResult = load_report_thread();
            break;

        default:
            break;
    }

    return findResult;
}

/*****************************************************************************
 函 数 名  : create_threads_Record
 功能描述  : 创建录像相关线程，如策略更新线程、磁盘空间检测线程、备份任务处理线程等
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::create_threads_Record(int32_t threadIndex)
{
    SVS_TRACE();
    int32_t findResult = RT_THREAD_INDEX_UNFOUND;

    switch(threadIndex)
    {   // 更新录像策略线程
        case RENEW_STRATEGY_THREAD:
            findResult = renew_strategy_loop();
            break;

        // 策略检测线程
        case STRATEGY_CHECK_THREAD:
            findResult = strategy_check_thread();
            break;

        // 更新录像索引线程
        case UPDATE_RECORD_THREAD:
            findResult = update_record_thread();
            break;
        // 处理前端录像备份任务
        case PROC_BAKUP_TASK_THREAD:
            findResult = proc_bakup_msg_loop();
            break;

        case PROC_REPORT_RECORD_STATUS_TASK_THREAD:
            findResult = procReportRecordStatusTaskThread();
            break;

        default:
            break;
    }

    return findResult;
}

/*****************************************************************************
 函 数 名  : create_threads_del
 功能描述  : 创建录像删除相关线程，如删除任务处理线程、删除预录控制线程等
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::create_threads_del(int32_t threadIndex)
{
    SVS_TRACE();
    int32_t findResult = RT_THREAD_INDEX_UNFOUND;
    switch(threadIndex)
    {
        // 删除任务线程
        case PROC_DEL_RECORD_TASK_THREAD:
            findResult = proc_del_record_task_thread();
            break;

        // 删除文件线程
        case DEL_RECORD_FILE_THREAD:
            findResult = del_record_file_loop();
            break;

        // 删除预录文件线程
        case DEL_PRERECORD_FILE_THREAD:
            findResult = del_prerecord_file_loop();
            break;

        // 删除日期目录线程
        case DEL_DATE_DIR_THREAD:
            findResult = del_date_dir_loop();
            break;
        default:
            break;
    }

    return findResult;
}

/*****************************************************************************
 函 数 名  : create_threads_media
 功能描述  : 创建媒体流接收和写相关线程，包括媒体流接收和写录像文件线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::create_threads_media(int32_t threadIndex)
{
    SVS_TRACE();
    int32_t findResult = RT_THREAD_INDEX_UNFOUND;
    switch(threadIndex)
    {
        case -1: //错误
            {
                SVS_LOG((SVS_LM_CRITICAL, "The thread id is unknown ."));
                findResult = 0;
            }
            break;

        default:
            {
                uint32_t otherThreadNum = MAX_THREAD;
                otherThreadNum += SVC_NUM;
                uint32_t thrIndex = (uint32_t)threadIndex;
                if ( (MAX_THREAD <= thrIndex) && ( otherThreadNum > thrIndex ) )
                {
                    //消息处理线程 SVC_NUM个
                    //消息分发处理线程
                    thrIndex -= MAX_THREAD;
                    findResult = proc_message_thread( thrIndex );
                }
            }
            break;
    }

    return findResult;
}

/*****************************************************************************
 函 数 名  : open_queue
 功能描述  : 打开消息队列，包括系统、实时录像、前端录像备份
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::open_queue()
{
    SVS_TRACE();
    //设置消息队列水位
    (void)msg_queue()->close();
    if ( SVS_RESULT_OK != msg_queue()->open(SVS_CMD_MSG_QUEUE_WM, SVS_CMD_MSG_QUEUE_WM) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the message queue failed."));

        return SVS_RESULT_FAILURE;
    }

    (void)monitor_event_queue_.close();
    if ( SVS_RESULT_OK != monitor_event_queue_.open(SVS_MONITOR_EVENT_QUEUE_WM, SVS_MONITOR_EVENT_QUEUE_WM) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to open the queue for monitoring event."));

        return SVS_RESULT_FAILURE;
    }

    (void)renew_strategy_queue_.close();
    if ( SVS_RESULT_OK != renew_strategy_queue_.open(SVS_RENEW_STRATEGY_QUEUE_WM, SVS_RENEW_STRATEGY_QUEUE_WM) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to open the queue for updating record strategy."));

        return SVS_RESULT_FAILURE;
    }

    // 打开备份任务消息队列
    (void)bakup_task_queue_.close();
    if ( SVS_RESULT_OK != bakup_task_queue_.open(BAKUP_FRONT_RECORD_TASK_QUEUE_WM,
                                                 BAKUP_FRONT_RECORD_TASK_QUEUE_WM) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to open the task queue for backuping task_queue_ failed."));

        return SVS_RESULT_FAILURE;
    }

    (void)rindex_queue_.close();
    if ( SVS_RESULT_OK != rindex_queue_.open(RECORD_FILE_INDEX_QUEUE_WM,
                                             RECORD_FILE_INDEX_QUEUE_WM) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the queue of realtime recording index failed."));

        return SVS_RESULT_FAILURE;
    }

    // 打开负载上报队列，不用设置太大，有负载上报就OK,所以使用默认值
    (void)load_report_queue_.close();
    if ( SVS_RESULT_OK != load_report_queue_.open())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the queue of load report failed."));

        return SVS_RESULT_FAILURE;
    }

    (void)timer_call_back_queue_.close();
    if ( SVS_RESULT_OK != timer_call_back_queue_.open(  TIMER_CALL_BACK_QUEUE_WM,
                                                        TIMER_CALL_BACK_QUEUE_WM ) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the queue of timer callback failed."));

        return SVS_RESULT_FAILURE;
    }

    // D事件录像性能优化
    (void)timer_call_back_queue_second_.close();
    if ( SVS_RESULT_OK != timer_call_back_queue_second_.open(   TIMER_CALL_BACK_QUEUE_WM,
                                                                TIMER_CALL_BACK_QUEUE_WM ) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the second queue of timer callback failed."));

        return SVS_RESULT_FAILURE;
    }

    // D事件录像性能优化
    (void)timer_call_back_queue_record_.close();
    if ( SVS_RESULT_OK != timer_call_back_queue_record_.open())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the record queue of timer callback failed."));

        return SVS_RESULT_FAILURE;
    }

    // 录像状态上报定时器队列
    (void)m_recordStatusCollectQueue.close();
    if ( SVS_RESULT_OK != m_recordStatusCollectQueue.open())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the record status task queue failed."));

        return SVS_RESULT_FAILURE;
    }

    if (SVS_RESULT_OK != open_del_queue())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the queues for deleting failed."));

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO, "Open all queues succeeded."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : open_del_queue
 功能描述  : 打开录像删除相关队列
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::open_del_queue()
{
    SVS_TRACE();
    //调整录像删除相关的消息队列水位
    (void)del_task_queue_.close();
    if ( SVS_RESULT_OK != del_task_queue_.open(  DEL_RECORD_TASK_QUEUE_WM,
                                                 DEL_RECORD_TASK_QUEUE_WM ) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to open the queue of deleting task."));
        return SVS_RESULT_FAILURE;
    }

    (void)del_prerecord_task_queue_.close();
    if ( SVS_RESULT_OK != del_prerecord_task_queue_.open(  DEL_RECORD_PRERECORD_QUEUE_WM,
                                                           DEL_RECORD_PRERECORD_QUEUE_WM ) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed ot open the queue of deleting prerecord task."));
        return SVS_RESULT_FAILURE;
    }

    (void)del_record_queue_.close();
    if ( SVS_RESULT_OK != del_record_queue_.open(  DEL_RECORD_QUEUE_WM,
                                                   DEL_RECORD_QUEUE_WM ) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to open the queue of deleting record."));
        return SVS_RESULT_FAILURE;
    }

    (void)del_record_file_queue_.close();
    if ( SVS_RESULT_OK != del_record_file_queue_.open(  DEL_RECORD_FILE_QUEUE_WM,
                                                        DEL_RECORD_FILE_QUEUE_WM ) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to open the queue of deleting record file."));
        return SVS_RESULT_FAILURE;
    }

    (void)del_prerecord_file_queue_.close();
    if ( SVS_RESULT_OK != del_prerecord_file_queue_.open(  DEL_RECORD_PRERECORD_QUEUE_WM,
                                                           DEL_RECORD_PRERECORD_QUEUE_WM ) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to open the queue of deleting prerecord file."));
        return SVS_RESULT_FAILURE;
    }

    (void)del_date_dir_queue_.close();
    if (SVS_RESULT_OK != del_date_dir_queue_.open(DEL_DATE_DIR_WM, DEL_DATE_DIR_WM))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Failed to open the queue for deleting overdue date directory."));
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,"Open all deleting queues succeeded."));
    return SVS_RESULT_OK;
}

int32_t CAC_Signal_Task::create_timer()
{
    if(SVS_RESULT_OK != create_del_timer())
    {
        return SVS_RESULT_FAILURE;
    }

    // 设置实时录像状态检测定时器
    ACE_Time_Value DelayTime(SVS_CHECK_RECORD_STATUS_INTERVAL, 0);
    ACE_Time_Value IntervalTime(SVS_CHECK_RECORD_STATUS_INTERVAL, 0);
    int32_t allocTimer = CAC_Timer_Manager::instance()->alloc_timer(
                        CAC_RT_Record_Device_Manager::instance(),
                        CAC_RT_Record_Device_Manager::timer_callback,
                        0,
                        DelayTime,
                        IntervalTime,
                        CAC_Realtime_Record_Device::TIMER_TYPE_RECORD_STATUS);

    if( SVS_RESULT_FAILURE == allocTimer )
    {
        SVS_LOG( (SVS_LM_CRITICAL,
            "Failed to set the timer of checking record status deleting record.") );
        return SVS_RESULT_FAILURE;
    }

    // 设置前端录像备份状态检测定时器
    allocTimer = CAC_Timer_Manager::instance()->alloc_timer(
                        CAC_Bakup_Record_Dev_Manager::instance(),
                        CAC_Bakup_Record_Dev_Manager::timer_callback,
                        0,
                        DelayTime,
                        IntervalTime,
                        CAC_Bakup_Record_Dev::TIMER_TYPE_CHECK_STATUS);

    if( SVS_RESULT_FAILURE == allocTimer )
    {
        SVS_LOG( (SVS_LM_CRITICAL,
            "Failed to set the timer of checking bakup record status deleting record.") );
        return SVS_RESULT_FAILURE;
    }

    // 刷新配置文件周期
    ACE_Time_Value delayTime( REFRESH_CONFIG_INTERVAL);
    ACE_Time_Value intervalTime( REFRESH_CONFIG_INTERVAL );
    allocTimer = CAC_Timer_Manager::instance()->alloc_timer(
            this,
            CAC_RT_Record_Server::timer_callback,
            REFRESH_CONFIG_EVENT_TYPE,
            delayTime,
            intervalTime );
    if( SVS_RESULT_FAILURE == allocTimer )
    {
        SVS_LOG( (SVS_LM_CRITICAL, "Set the timer of heartbeat failed.") );
        return SVS_RESULT_FAILURE;
    }

    // 设置录像状态上报定时器
    ACE_Time_Value statusDelayTime( REPORT_RECORD_STATUS_INTERVAL);
    ACE_Time_Value statusIntervalTime( REPORT_RECORD_STATUS_INTERVAL );
    allocTimer = CAC_Timer_Manager::instance()->alloc_timer(
            this,
            CAC_RT_Record_Server::timer_callback,
            REPORT_RECORD_STATUS_INTERVAL_EVNET_TYPE,
            statusDelayTime,
            statusIntervalTime );
    if( SVS_RESULT_FAILURE == allocTimer )
    {
        SVS_LOG( (SVS_LM_CRITICAL, "Set the timer of heartbeat failed.") );
        return SVS_RESULT_FAILURE;
    }




    if(SVS_RESULT_OK != CAC_RT_Record_Server::instance()->set_load_report_interval_timer())
    {
        SVS_LOG( (SVS_LM_CRITICAL, "Set load report timer failed.") );
        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}


/*****************************************************************************
 函 数 名  : create_del_timer
 功能描述  : 创建删除相关定时器，如定时删除、删除预录控制、删除流控、删除过期的
             日期目录等
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::create_del_timer()
{
    SVS_TRACE();

    // 设置删除流控定时器
    ACE_Time_Value flowControlDelayTime(0, SVS_DEL_FLOW_CONTROL_INTERVAL_TIME);
    ACE_Time_Value flowControlIntervalTime(0, SVS_DEL_FLOW_CONTROL_INTERVAL_TIME);

    int32_t allocTimer = CAC_Timer_Manager::instance()->alloc_timer(
        this,
        CAC_Signal_Task::del_record_timer_callback,
        SVS_DEL_TIMER_CONTROL,
        flowControlDelayTime,
        flowControlIntervalTime,
        0);

    if( SVS_RESULT_FAILURE == allocTimer )
    {
        SVS_LOG( (SVS_LM_CRITICAL, "Failed to set the timer of flow controling deleting record file .") );
        return SVS_RESULT_FAILURE;
    }


    // 设置删除预录定时器
    ACE_Time_Value delProrecordDelayTime(RECORDFILE_TIME_SLICE, 0);
    ACE_Time_Value delProrecordIntervalTime(RECORDFILE_TIME_SLICE, 0);

    allocTimer = CAC_Timer_Manager::instance()->alloc_timer(
        this,
        CAC_Signal_Task::del_record_timer_callback,
        SVS_DEL_TIMER_PRERECORD,
        delProrecordDelayTime,
        delProrecordIntervalTime,
        0);

    if( SVS_RESULT_FAILURE == allocTimer )
    {
        SVS_LOG( (SVS_LM_CRITICAL,
            "Failed to set the timer of deleting prerecord.") );
        return SVS_RESULT_FAILURE;
    }

    // 设置删除日期目录定时器
    ACE_Time_Value delDateDirDelayTime(SVS_DEL_DATE_DIR_DELAY_TIME, 0);
    ACE_Time_Value delDateDirIntervalTime(SVS_DEL_DATE_DIR_INTERVAL_TIME, 0);

    allocTimer = CAC_Timer_Manager::instance()->alloc_timer(
        this,
        CAC_Signal_Task::del_record_timer_callback,
        SVS_DEL_TIMER_DATE_DIR,
        delDateDirDelayTime,
        delDateDirIntervalTime,
        0);

    if( SVS_RESULT_FAILURE == allocTimer )
    {
        SVS_LOG( (SVS_LM_CRITICAL, "Failed to set the timer of deleting overdue date directory.") );

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO, "Create all timers of deleting record succeeded."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : proc_message_thread
 功能描述  : 线程:消息处理线程，处理从StoreSvr接收来的消息
 输入参数  : uint32_t nIndex:消息处理线程的线程ID
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::proc_message_thread( uint32_t nIndex )
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING,
        "The thread for processing message start running... The thread id[%d]",
        nIndex));

    ACE_Message_Block *mb = NULL;
    ACE_Message_Block *mbHandle = NULL;
    char threadName[MAX_THREAD_NAME] = {0};
    (void)snprintf(threadName,sizeof(threadName),"ProcessMessageThread[%lu]",nIndex);
    CThread_Stat_Reporter reporter(threadName);
    ACE_Time_Value tvDelay(1);

    while ( brunning_ )
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;

        if ( -1 == getq(mb,&tv) )            //等待消息
        {
            // 如果不是超时
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR, "Failed to get message from message queue."));
                (void)ACE_OS::sleep(1);
                mb = NULL;
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();

        //消息批量处理
        while ( brunning_ && (NULL != mb) )
        {
            mbHandle = mb;
            mb = mb->cont();

            mbHandle->cont( NULL );

            handle_message(nIndex, mbHandle);  //分发处理消息
        }
    }

    SVS_LOG((SVS_LM_WARNING, "The thread of processing message exited."
        "The thread index is [%d]",
        nIndex));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : start_connect
 功能描述  : 线程:录像管理服务器通讯服务处理线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::start_connect()
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_WARNING, "The thread for communicating with StoreSvr start running..."));

    if( NULL == p_connect_reactor_ )
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "The reactor for communicating with StoreSvr is NULL."
            "The thread for communicating with StoreSvr exited."));

        return SVS_RESULT_FAILURE;
    }

    CThread_Stat_Reporter reporter("CommunicatingWithStoreSvrThread");
    (void)p_connect_reactor_->owner(ACE_OS::thr_self());

    int32_t ret = 0;
    while (brunning_)
    {
        ACE_Time_Value tv(1);
        ret = p_connect_reactor_->handle_events(tv);
        if ( 0 > ret )
        {
            SVS_LOG((SVS_LM_WARNING,
                "The reactor for communicating with StoreSvr handle event failed.",
                p_connect_reactor_));

            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();
    }

    SVS_LOG((SVS_LM_WARNING, "The thread for communicating with StoreSvr exited."));
    return ret;
}

/*****************************************************************************
 函 数 名  : timer_event_loop
 功能描述  : 线程:定时器处理线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::timer_event_loop()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "The thread for processing timer start running..."));

    if ( NULL == p_timer_reactor_ )
    {
        SVS_LOG((SVS_LM_CRITICAL, "The reactor of timer is NULL."
            "The thread for processing timer exited."));

        return SVS_RESULT_FAILURE;
    }
    (void)p_timer_reactor_->owner(ACE_OS::thr_self());
    CThread_Stat_Reporter reporter("TimerEventThread");
    int32_t ret = 0;
    while (brunning_)
    {
        ACE_Time_Value procTime(0, SVS_REACTOR_PROC_TIME);
        ret = p_timer_reactor_->handle_events( &procTime);
        if ( 0 > ret )
        {
            SVS_LOG((SVS_LM_WARNING,
                "The reactor of timer handle event failed.[0x%08x]",
                p_timer_reactor_));

            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();
    }

    SVS_LOG((SVS_LM_WARNING, "The thread for processing timer exited."));
    return ret;
}

/*****************************************************************************
 函 数 名  : handle_monitor_event
 功能描述  : 监控事件处理回调函数
 输入参数  : const ACE_Message_Block *mb:监控到的事件内容
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
//监控事件处理回调函数
int32_t CAC_Signal_Task::handle_monitor_event(const ACE_Message_Block *mb) const
{
    SVS_TRACE();

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR, "Handle monitoring event failed, The parameter mb is NULL."));

        return SVS_RESULT_FAILURE;
    }

    SVS_EVENT_HEADER *pHdr = (SVS_EVENT_HEADER *)(void*)mb->rd_ptr();

    SVS_LOG((SVS_LM_INFO,
        "Begin to process monitoring event, The event type is [0x%04x].",
        pHdr->Event));

    switch (pHdr->Event)
    {
        case RT_MONITOR_EVENT_START_RECORD:   //重新连接Store Svr
            {
                SVS_LOG( (SVS_LM_WARNING,
                    "Porcess RtRecordSvr monitor event, Begin to start recording.") );

                CAC_RT_Record_Server::instance()->add_load_report_task();

                return SVS_RESULT_OK;
            }

        case RT_MONITOR_EVENT_STOP_RECORD:
            {
                SVS_LOG( (SVS_LM_WARNING,
                    "Porcess RtRecordSvr monitor event, Begin to stop recording.") );

                //停止所有实时录像
                int32_t stopRtRecordRet = CAC_Record_Inform_Manager::instance()->stop_all_record();

                // 停止所用前端录像备份镜头
                int32_t stopBackupRecordRet = CAC_Bakup_Record_Dev_Manager::instance()->stop_all_device();

                // 如果停止录像失败
                if((SVS_RESULT_OK != stopRtRecordRet) || (SVS_RESULT_OK != stopBackupRecordRet))
                {
                    return SVS_RESULT_FAILURE;
                }

                // 停止录像成功
                return SVS_RESULT_OK;
            }


        case RT_MONITOR_EVENT_DISK_ALARM:
            {
                SVS_LOG((SVS_LM_WARNING, "Monitor a disk alarm event."));

                SVS_EXCEPTION_ALARM *pAlaramMsg = (SVS_EXCEPTION_ALARM *)(void*)mb->rd_ptr();

                if (NULL != pAlaramMsg->DevIDVec)
                {
                    SVS_DELETE(pAlaramMsg->DevIDVec);
                }

                return SVS_RESULT_OK;
            }

        default:
            SVS_LOG((SVS_LM_WARNING, "Monitor a unkown type[0x%04x] event.", pHdr->Event));
            return 0;
    }

}

/*****************************************************************************
 函 数 名  : strategy_check_thread
 功能描述  : 线程:录像策略更新检测线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::strategy_check_thread()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "The thread for checking record strategy start running..."));

    if ( NULL == p_strategy_reactor_ )
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "The reactor of checking record strategy is NULL."
            "The thread of checking record strategy exited.."));

        return SVS_RESULT_FAILURE;
    }

    (void)p_strategy_reactor_->owner(ACE_OS::thr_self());

    CAC_Record_Inform_Manager* pManager = CAC_Record_Inform_Manager::instance();
    int32_t ret = pManager->check_strategy_loop( p_strategy_reactor_, p_strategy_timer_manager_);

    SVS_LOG((SVS_LM_WARNING, "The thread of checking record strategy exited."));
    return ret;
}

/*****************************************************************************
 函 数 名  : strategy_check_thread
 功能描述  : 把从StoreSvr接收到的消息进行分发处理
 输入参数  : uint32_t nThrIndex:处理消息的线程id标识
             ACE_Message_Block *mb:消息内容
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  : 增加B035需求，增加对备份前端录像任务通知消息的处理
*****************************************************************************/
void CAC_Signal_Task::handle_message(uint32_t nThrIndex, ACE_Message_Block *mb)
{
    SVS_TRACE();

    //消息个数统计
    ++msg_total_count_[nThrIndex];

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Handle message failed.The parameter message block pointer is NULL."));

        return;
    }

    CAC_RT_Record_Device_Manager *pRecordManager = CAC_RT_Record_Device_Manager::instance();
    CAC_Record_Inform_Manager *pInfromManager = CAC_Record_Inform_Manager::instance();
    CAC_RT_Record_Server* pServer = CAC_RT_Record_Server::instance();
    CAC_Bakup_Record_Dev_Manager *pBakupDevManager = CAC_Bakup_Record_Dev_Manager::instance();

    PSVS_HEADER pHdr = (PSVS_HEADER)(void*)mb->rd_ptr();

    SVS_LOG((SVS_LM_INFO, "[MSG]Received message from StoreSvr, "
        "msgtype[0x%04x], transmission No[0x%04x], packet length[%u].",
        pHdr->MsgType,
        pHdr->TransactionNo,
        pHdr->PacketLength));

    bool freeMsgByHandleFunc = false;

    switch (pHdr->MsgType)
    {
        //实时录像路由消息处理
        case SVS_MSG_TYPE_STORESERVER_ROUTER_INFO_REQ:
            ++msg_sort_msg_count[nThrIndex][MSG_SORT_ROUTER_INFO];

            //发送响应消息
            (void)pRecordManager->handle_routerinfo(mb);
            break;

        //停止路由消息处理，不发送响应消息
        case SVS_MSG_TYPE_STORESERVER_SCHEDULE_OVER_REQ:
            ++msg_sort_msg_count[nThrIndex][MSG_SORT_SCHEDULE_OVER_REQ];
            (void)pServer->stop_record( mb );
            break;

        //更新录像策略，发送响应消息
        case SVS_MSG_TYPE_STORESVR_RENEW_STRATEGY_REQ:
            (void)putinto_renew_strategy(mb);
            break;

        //更新镜头录像参数,不发送响应消息
        case SVS_MSG_TYPE_CLIENTSVR_SETDEVICERECORDERPARAM_REQ:
            (void)pInfromManager->update_record_parameter(mb);
            break;

        //告警事件处理,不发送响应消息
        case SVS_ALARM_KINESCOPE_CONTROL_REQ:
            ++msg_sort_msg_count[nThrIndex][MSG_SORT_ALARM_INFO];
            (void)pInfromManager->alarm_event_req(mb);
            break;

        // 错误消息处理,不发送响应消息
        case SVS_MSG_TYPE_ERROR:
            handle_error_msg(mb);
            break;

        // 如果是前端录像备份通知消息(本消息不发送响应消息)
        case SVS_MSG_TYPE_DISTRIBUTE_RECORDBAK_TASK_REQ:
            (void)putinto_bakup_msg(mb);
            freeMsgByHandleFunc = true; //由消息处理函数释放消息内存
            break;

        // 如果是前端录像回放路由响应消息
        case SVS_MSG_TYPE_CLIENT_SETUP_MEDIA_STREAM_RESP:
            (void)pBakupDevManager->handle_routerinfo(mb);
            break;

        // 设备录像任务下发请求
        case SVS_MSG_TYPE_DEVICE_DISTRIBUTE_REQ:
            (void)pInfromManager->handle_record_task(mb);
            break;


        case SVS_MSG_TYPE_STORESVR_DELETE_RECORD_REQ:
        case SVS_MSG_TYPE_STORESVR_QUERY_INDEX_RESP:
        case SVS_MSG_TYPE_STORESVR_QUERY_LENS_MINTIME_RESP:
            handle_del_msg(mb, freeMsgByHandleFunc);
            break;

        // 添加对录像索引上报响应消息的处理
        case SVS_MSG_TYPE_STORESVR_RECORD_INDEX_INFO_RESP:
            CIndexResendMgr::instance()->HandleReportConfirm(mb);
            break;

        default:
            SVS_LOG((SVS_LM_WARNING,
                "[MSG]Received invalid message form StoreSvr, "
                "Unknown msgtype[0x%04x].",
                pHdr->MsgType));
            break;
    }

    if ( !freeMsgByHandleFunc )
    {
        g_p_msg_buffer->free_mb( mb );
    }

    SVS_LOG((SVS_LM_INFO,
        "[MSG]Process message finished, "
        "msgtype[0x%04x] transmission No[0x%04x] pachet length[%u].",
        pHdr->MsgType,
        pHdr->TransactionNo,
        pHdr->PacketLength));
}

/*****************************************************************************
 函 数 名  : putinto_renew_strategy
 功能描述  : 将更新录像策略的消息插入到策略更新队列中
 输入参数  : ACE_Message_Block *mb:策略更新消息内容
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::putinto_renew_strategy(const ACE_Message_Block *mb)
{
    SVS_TRACE();

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Handle the message of renewing strategy failed, "
            "The parameter message block pointer is NULL."));

        return SVS_RESULT_FAILURE;
    }

    PSVS_STORESVR_RENEW_STRATEGY_REQ_MSG pReq =
        (PSVS_STORESVR_RENEW_STRATEGY_REQ_MSG)(void*)mb->rd_ptr();

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
    int32_t nRet = SVS_RESULT_OK;
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
            nRet = SVS_RESULT_FAILURE;
            break;
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

            nRet = SVS_RESULT_FAILURE;
            break;
        }

        uint32_t ulStragegyMbLen = sizeof(SVS_STORESVR_RENEW_STRATEGY_REQ_MSG)
            + ((uint32_t)pDevInfo->RecordStrategyCount - 1) * sizeof(RECORD_STRATEGY_INFO);
        ACE_Message_Block * pStragegyMb =
            g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_NORMAL_EVENT ,ulStragegyMbLen);

        if(NULL == pStragegyMb)
        {
            SVS_LOG((SVS_LM_ERROR,
                "Fail to new record strategy as allocate message block for stragety failed."
                "stratCount[%u].",
                pDevInfo->RecordStrategyCount));

            nRet = SVS_RESULT_FAILURE;
            break;
        }

        pStragegyMb->wr_ptr(ulStragegyMbLen);

        // 构造消息内容
        SVS_STORESVR_RENEW_STRATEGY_REQ_MSG *pStrategyMsg =
            (SVS_STORESVR_RENEW_STRATEGY_REQ_MSG*)(void*)pStragegyMb->rd_ptr();
        memset(pStrategyMsg, 0, ulStragegyMbLen);
        FillCommonHeader(pStrategyMsg->Header,
                        SVS_MSG_TYPE_STORESVR_RENEW_STRATEGY_REQ,
                        0,
                        (uint16_t)ulStragegyMbLen);

        pStrategyMsg->DeviceCount = 1;
        memcpy(pStrategyMsg->DeviceStrategyInfo, pDevInfo, ulStragegyMbLen );

        // 把策略更新消息分到策略更新队列中
        ACE_Time_Value tv(ACE_OS::gettimeofday());
        tv.set(tv.sec(), tv.usec() + SVS_ENQUEUE_TIMEOUT);
        if(-1 == enqueue_record_strategy(pStragegyMb, &tv))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Fail to update record stragety as failed to enqueue mb."
                "uStrategyCount[%u].",
                pDevInfo->RecordStrategyCount));
            g_p_msg_buffer->free_mb(pStragegyMb);

            nRet = SVS_RESULT_FAILURE;
            break;
        }

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

    // 保存策略更新请求消息的消息头信息
    SVS_STORESVR_RENEW_STRATEGY_REQ_MSG szReq;
    memcpy(&szReq, (PSVS_STORESVR_RENEW_STRATEGY_REQ_MSG)(void*)mb->rd_ptr(), sizeof(szReq));

    // 构造策略更新响应消息
    uint16_t respMsgLen = sizeof(SVS_STORESVR_RENEW_STRATEGY_RESP_MSG);
    ACE_Message_Block *respMsgMb =
        g_p_msg_buffer->alloc_mb(  CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, respMsgLen );

    if ( NULL == respMsgMb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "[MSG]failed to response renewing strategy message, "
            "Allocate mb failed, "
            "mb type[%d], mb size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
            respMsgLen ));

        return SVS_RESULT_FAILURE;
    }

    SVS_STORESVR_RENEW_STRATEGY_RESP_MSG* pRespMsg =
        (SVS_STORESVR_RENEW_STRATEGY_RESP_MSG*)(void*)respMsgMb->wr_ptr();
    respMsgMb->wr_ptr( respMsgLen );

    FillCommonHeader(pRespMsg->Header,
        SVS_MSG_TYPE_STORESVR_RENEW_STRATEGY_RESP,
        transactionno_respond(szReq.Header.TransactionNo),
        respMsgLen);

    pRespMsg->Time = szReq.Time;
    pRespMsg->RespCode = 0;
    // 如果更新策略失败，则给StoreSvr返回策略更新失败消息
    if(SVS_RESULT_OK != nRet)
    {
        pRespMsg->RespCode = (uint32_t)-1;
    }

    int32_t sendResult =  CAC_RT_Record_Server::instance()->send_msg( respMsgMb );

    if ( 0 != sendResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "[MSG]Enqueue the message of renewing record strategy failed, "
            "transmission No[0x%04x], enqueueing return code[%d].",
            szReq.Header.TransactionNo,
            sendResult ));

        g_p_msg_buffer->free_mb( respMsgMb );
        respMsgMb = NULL;

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "[MSG]Enqueue the message of renewing record strategy succeeded, "
        "msgtype[0x%04x], "
        "transmission No[0x%04x], "
        "timetick[%d], response code[%d].",
        pRespMsg->Header.MsgType,
        pRespMsg->Header.TransactionNo,
        pRespMsg->Time.tv_sec,
        pRespMsg->RespCode));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : renew_strategy_loop
 功能描述  : 线程:更新录像策略线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::renew_strategy_loop()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "The thread for renewing strategy start running..."));

    ACE_Message_Block *mb = NULL;

    CThread_Stat_Reporter reporter("RenewStrategyThread");
    ACE_Time_Value tvDelay(1);
    while (brunning_)
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        if (-1 == renew_strategy_queue_.dequeue_head(mb,&tv))
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "The queue of renewing record strategy dequeue failed."));
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();

        (void)CAC_Record_Inform_Manager::instance()->renew_strategy(mb);

        g_p_msg_buffer->free_mb(mb);
    }

    SVS_LOG((SVS_LM_WARNING, "The thread for renewing strategy exited."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : proc_bakup_msg_loop
 功能描述  : 线程:处理前端录像备份任务消息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::proc_bakup_msg_loop()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING,
        "The thread for processing bakuping front record task start running ..."));

    CThread_Stat_Reporter reporter("ProcessBackupFrontRecordTaskThread");
    ACE_Time_Value tvDelay(1);
    ACE_Message_Block *mb = NULL;
    while (brunning_)
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        // 从备份前端录像消息队列中取任务消息
        if (-1 ==bakup_task_queue_.dequeue_head(mb,&tv))
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "The queue of bakuping front record task dequeue failed."));
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();

        // 调用CAC_Bakup_Record_Dev_Manager备份任务通知接口请求处理备份任务
        (void)CAC_Bakup_Record_Dev_Manager::instance()->bakup_record_notify(mb);

        // 取出消息后，释放存放消息的空间，在入队列的地方申请
        g_p_msg_buffer->free_mb( mb );
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING,
        "The thread for processing bakuping front record task exited."));

    return SVS_RESULT_OK;
}

/******************************************************************************
  函 数 名  : putinto_bakup_msg
  功能描述  : 把备份前端录像通知消息加入消息队列等待处理
  输入参数  : const ACE_Message_Block *mb:备份前端录像通知消息内容
  输出参数  : NA
  返回值    : 成功返回:0,失败返回:-1
*******************************************************************************/
int32_t CAC_Signal_Task::putinto_bakup_msg( ACE_Message_Block *mb)
{
    SVS_TRACE();
    // 参数合法性判断
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "The queue of bakuping front record task enqueue faied."
            "The parameter message block is NULL."));
        return SVS_RESULT_FAILURE;
    }

    //将[备份前端录像]消息放入备份任务队列中
    if (SVS_RESULT_FAILURE == bakup_task_queue_.enqueue_tail( mb ))
    {
        // 失败打印日志，队列当前使用情况
        SVS_LOG(( SVS_LM_ERROR,
            "The queue of bakuping front record task enqueue faied. "
            "queue pointer address[0x%08x], queue full flag[%d] ,queue state[%d] "
            "queue count[%d],queue length[%d] ,queue size[%d].",
            &bakup_task_queue_,
            bakup_task_queue_.is_full(),
            bakup_task_queue_.state(),
            bakup_task_queue_.message_count(),
            bakup_task_queue_.message_length(),
            bakup_task_queue_.message_bytes()));

        g_p_msg_buffer->free_mb( mb );
        return SVS_RESULT_FAILURE;
    }

    // 打印成功日志
    return SVS_RESULT_OK;
}

void CAC_Signal_Task::handle_del_msg(ACE_Message_Block* mb, bool& bFree)
{
    if (NULL == mb)
    {
        SVS_LOG(( SVS_LM_ERROR, "Del msg is null. handle del msg fail."));
        return;
    }
    PSVS_HEADER pHdr = (PSVS_HEADER)(void*)mb->rd_ptr();
    switch (pHdr->MsgType)
    {
        case SVS_MSG_TYPE_STORESVR_DELETE_RECORD_REQ:
            proc_del_record_msg(mb);
            break;

        case SVS_MSG_TYPE_STORESVR_QUERY_INDEX_RESP:
            proc_del_prerecord_msg(mb);
            break;
        case SVS_MSG_TYPE_STORESVR_QUERY_LENS_MINTIME_RESP:
            proc_del_date_dir_msg(mb);
            bFree = true;                 // 由删除线程自己释放
            break;
        default:
            SVS_LOG((SVS_LM_WARNING,
                "[MSG]Message is not a del msg, "
                "Unknown msgtype[0x%04x].",
                pHdr->MsgType));
            break;
    }
    return;
}

/******************************************************************************
  函 数 名  : proc_del_record_msg
  功能描述  : 存储管理服务器下发的录像删除请求处理
  输入参数  : mb:录像删除请求消息内容
  输出参数  : NA
  返回值    : 成功返回:0,失败返回:-1
*******************************************************************************/
void CAC_Signal_Task::proc_del_record_msg(const ACE_Message_Block *mb)
{
    SVS_TRACE();

    //参数合法性判断
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "The delete record message deal faied, "
            "The parameter message block is NULL."));

        return ;
    }

    SVS_STORESVR_DELETE_RECORD_REQ *pReqMsg =
            (SVS_STORESVR_DELETE_RECORD_REQ *)(void *)mb->rd_ptr();

    // 判断消息长度
    if (sizeof(SVS_STORESVR_DELETE_RECORD_REQ) != pReqMsg->Header.PacketLength)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The delete record message len is abnormal, "
            "expected[%u], actual[%u].",
            sizeof(SVS_STORESVR_DELETE_RECORD_REQ),
            pReqMsg->Header.PacketLength));

        return ;
    }

    uint8_t strLensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
    SetLocalizeDeviceId(strLensId,
        sizeof(strLensId),
        (const char *)pReqMsg->DeviceID,
        DEVICEID_LEN);

    if (CAC_Bakup_Record_Dev_Manager::instance()->is_lens_bakup_record((const char *)strLensId))
    {
        SVS_LOG((SVS_LM_WARNING,
            "The delete record request cancel via lens being bakup, "
            "camera[%s], starttime[%u], endtime[%u].",
            strLensId, pReqMsg->StartTime, pReqMsg->EndTime));

        //在录像备份时不执行删除，同时也注意不能返回错误消息
        return ;
    }

    // 判断事件是否合法
    // 起始时间和结束时间必须相差一小时
    // 且必须都是整点
    if (pReqMsg->StartTime + SECONDS_PER_HOUR != pReqMsg->EndTime)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The delete record message is invalid, interval is not an hour. "
            "camera[%s], starttime[%u], endtime[%u].",
            strLensId, pReqMsg->StartTime, pReqMsg->EndTime));

        return ;
    }

    if (0 != pReqMsg->StartTime % SECONDS_PER_HOUR)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The delete record message is invalid, starttime is not the whole hour. "
            "camera[%s], starttime[%u].",
            strLensId, pReqMsg->StartTime));
        return;
    }

    // 对删除任务作预判断,如果索引文件不存在直接返回
    // 生成文件名
    char strDateTime[TIME_STRING_LEN] = {0};
    char strTime[TIME_STRING_LEN] = {0};
    char strIndexFileName[TIME_STRING_LEN]={0};
    (void)SVS_SS_UTILITIES::time2str(strTime, sizeof(strTime), (time_t)pReqMsg->StartTime);

    (void)ACE_OS::memcpy( strDateTime, strTime, 8 );
    (void)ACE_OS::memcpy( strIndexFileName, strTime, 10 );

    // 获取索引根路径列表，如果新旧索引路径都不存在该时段的所有，则返回索引存在的消息给存储管理服务器
    INDEX_FILE_VECTOR indexRootPathList;
    del_manager_.get_index_root_path_list((time_t)pReqMsg->StartTime, indexRootPathList);
    bool bFindIndexFileRet = false;
    for(uint32_t uLoop = 0; uLoop < indexRootPathList.size(); uLoop++)
    {
        // 生成索引文件全路径名称
        char strFullIndexFiledir[MAXPATHLEN]={0};
        (void)ACE_OS::snprintf(strFullIndexFiledir, sizeof(strFullIndexFiledir),
            "%s/%s/%s/%s.idx",
//            CAC_RT_Record_Server::instance()->get_record_indexfile_path(),
            indexRootPathList[uLoop].indexfile,
            strLensId,
            strDateTime,
            strIndexFileName);
        struct stat st;
        if (0 != stat(strFullIndexFiledir, &st))
        {
            SVS_LOG((SVS_LM_WARNING,
                "[DEL]Stat record indexfile failed.index file[%s].",
                strFullIndexFiledir));
        }
        else
        {
            bFindIndexFileRet = true;
            break;
        }
    }

    // 如果所有的索引路径都没有找到该索引文件，则发送索引不存在相应
    if (!bFindIndexFileRet)
    {

        SVS_LOG((SVS_LM_WARNING,
            "Index file is not exist, proc del record msg fail."));
        // 返回索引文件不存在的响应
        (void)del_manager_.send_record_delete_resp((char*)strLensId,
                                         (time_t)pReqMsg->StartTime,
                                         (uint64_t)0,
                                         STORESVR_DELETE_RECORD_INDEX_NOT_EXIST);
        return;
    }

    //将删除消息入队列
    ACE_Message_Block *pDelMb = NULL;
    try
    {
        pDelMb = new ACE_Message_Block(sizeof(SVS_DEL_RECORD_FILE));
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create deleting record file message failed."));

        return ;
    }

    //填充定时删除任务消息
    SVS_DEL_RECORD_FILE *pDelRecordFile = (SVS_DEL_RECORD_FILE *)((void *)pDelMb->rd_ptr());
    pDelRecordFile->MsgType = DEL_RECORD_TYPE_NORMAL;
    SetLocalizeDeviceId((uint8_t *)pDelRecordFile->LensId,
        sizeof(pDelRecordFile->LensId),
        (const char *)pReqMsg->DeviceID,
        DEVICEID_LEN);
    pDelRecordFile->StartTime  = (time_t)pReqMsg->StartTime;
    pDelRecordFile->EndTime    = (time_t)pReqMsg->EndTime;
    pDelRecordFile->DeleteMode = DEL_FLAG_SELECT;
    pDelMb->wr_ptr(sizeof(SVS_DEL_RECORD_FILE));

    SVS_LOG((SVS_LM_INFO,
        "[MSG]Deal record delete request message, "
        "camera[%s], starttime[%u], endtime[%u].",
        pDelRecordFile->LensId,
        pDelRecordFile->StartTime,
        pDelRecordFile->EndTime));

    ACE_Time_Value tvTimeout(ACE_OS::gettimeofday());
    tvTimeout.set(tvTimeout.sec(), tvTimeout.usec() + DEL_RECORD_TASK_ENQUEUE_TIMEOUT);

    // 消息入队列
    int32_t iRet = del_record_queue_.enqueue_tail(pDelMb, &tvTimeout);

    if (-1 == iRet)
    {
        SVS_LOG((SVS_LM_ERROR, "Deleting record file message enqueue failed."));
        (void)pDelMb->release();

        return ;//lint !e429 already enqueue
    }

    return ;//lint !e429 already enqueue
}//lint !e429 already enqueue

void CAC_Signal_Task::proc_del_prerecord_msg(const ACE_Message_Block* mb)const
{
    SVS_TRACE();
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "The delete prerecord message deal failed, "
            "The parameter message block is NULL."));
        return;
    }

    PSVS_STORESVR_QUERY_INDEX_RESP_MSG pMsg = NULL;
    pMsg = (PSVS_STORESVR_QUERY_INDEX_RESP_MSG)(void*)mb->rd_ptr();


    // 先判断至少包含查询镜头数字段
    uint16_t usExpectedMsgLen = sizeof(SVS_STORESVR_QUERY_INDEX_RESP_MSG) - sizeof(SVS_LENS_INDEX_FLAG);
    if (pMsg->Header.PacketLength < usExpectedMsgLen)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The delete prerecord message len lesvs than fixed len, "
            "fixed len[%u], actual[%u].",
            usExpectedMsgLen,
            pMsg->Header.PacketLength));
        return;
    }

    // 根据查询镜头数计算总长度
    usExpectedMsgLen += (uint16_t)(pMsg->DeviceCount * sizeof(SVS_LENS_INDEX_FLAG));

    // 判断消息长度
    if (usExpectedMsgLen != pMsg->Header.PacketLength)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The delete prerecord message len is abnormal, "
            "expected[%u], actual[%u].",
            usExpectedMsgLen,
            pMsg->Header.PacketLength));
        return;
    }


    uint32_t i;
    int32_t iRet = 0;
    uint8_t strLensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
    for (i = 0; i < pMsg->DeviceCount; i++)
    {
        SetLocalizeDeviceId(strLensId,
            sizeof(strLensId),
            pMsg->IndexFlag[i].LensId.ID,
            DEVICEID_LEN);

        // 预录消息中起止时间相差一个小时
        if (pMsg->IndexFlag[i].StartTime + SECONDS_PER_HOUR != pMsg->IndexFlag[i].EndTime)
        {
            SVS_LOG((SVS_LM_ERROR,
                "Del prerecord task is abnormal. "
                "camera[%s], starttime[%u], endtime[%u].",
                strLensId,
                pMsg->IndexFlag[i].StartTime,
                pMsg->IndexFlag[i].EndTime));
            continue;
        }

        if (0 != pMsg->IndexFlag[i].StartTime % SECONDS_PER_HOUR)
        {
            SVS_LOG((SVS_LM_ERROR,
                "Del prerecord task is abnormal. start time is not the whole hour. "
                "camera[%s], starttime[%u].",
                strLensId,
                pMsg->IndexFlag[i].StartTime));
            continue;
        }

        if (0 != pMsg->IndexFlag[i].HasIndex)
        {
            SVS_LOG((SVS_LM_INFO,
                "Del prerecord task has index. "
                "camera[%s], starttime[%u], endtime[%u].",
                strLensId,
                pMsg->IndexFlag[i].StartTime,
                pMsg->IndexFlag[i].EndTime));
            continue;
        }

        iRet = del_manager_.enqueue_del_record(DEL_RECORD_TYPE_PRERECORD,
                                                       strLensId,
                                                       (time_t)pMsg->IndexFlag[i].StartTime,
                                                       (time_t)pMsg->IndexFlag[i].EndTime,
                                                       DEL_FLAG_SELECT);
        if (-1 == iRet)
        {
            SVS_LOG((SVS_LM_ERROR,
                    "Enqueue del prerecord task fail. "
                    "camera[%s], starttime[%u], endtime[%u].",
                    strLensId,
                    pMsg->IndexFlag[i].StartTime,
                    pMsg->IndexFlag[i].EndTime));
            continue;
        }

        SVS_LOG((SVS_LM_DEBUG,
            "Enqueue del prerecord task success. "
            "camera[%s], starttime[%u], endtime[%u].",
            strLensId,
            pMsg->IndexFlag[i].StartTime,
            pMsg->IndexFlag[i].EndTime));
    }
    return;
}

void CAC_Signal_Task::proc_del_date_dir_msg(ACE_Message_Block* mb)
{
    SVS_TRACE();
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "The delete date dir message deal failed, "
            "The parameter message block is NULL."));
        return;
    }

    PSVS_STORESVR_QUERY_LENS_MINTIME_RESP_MSG pMsg = NULL;
    pMsg = (PSVS_STORESVR_QUERY_LENS_MINTIME_RESP_MSG)(void*)mb->rd_ptr();

    // 先判断至少包含查询镜头数字段
    uint16_t usExpectedMsgLen = sizeof(SVS_STORESVR_QUERY_LENS_MINTIME_RESP_MSG) - sizeof(SVS_LENS_MINTIME);
    if (pMsg->Header.PacketLength < usExpectedMsgLen)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The delete date dir message len lesvs than fixed len, "
            "fixed len[%u], actual[%u].",
            usExpectedMsgLen,
            pMsg->Header.PacketLength));
        return;
    }

    // 根据查询镜头数计算总长度
    usExpectedMsgLen += (uint16_t)(pMsg->DeviceCount * sizeof(SVS_LENS_MINTIME));

    // 判断消息长度
    if (usExpectedMsgLen != pMsg->Header.PacketLength)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The delete date dir message len is abnormal, "
            "expected[%u], actual[%u].",
            usExpectedMsgLen,
            pMsg->Header.PacketLength));
        return;
    }

    // 检验路径是否正确
    // 目前简单检查是否为空,后面可以依次与各磁盘目录和索引目录作比较
    if (0 == strlen(pMsg->FilePath))
    {
        SVS_LOG((SVS_LM_WARNING,
            "The delete date dir message is invalid, "
            "file path is null."));
        return;
    }

    ACE_Time_Value tvTimeout(ACE_OS::gettimeofday());
    tvTimeout.set(tvTimeout.sec(), tvTimeout.usec() + DEL_RECORD_TASK_ENQUEUE_TIMEOUT);


    if( -1 ==   del_date_dir_queue_.enqueue_tail(mb, &tvTimeout) )
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]The queue of deleting overdue date directory enqueue failed."
            "queue len[%u] count[%u].",
            del_date_dir_queue_.message_length(),
            del_date_dir_queue_.message_count()));
        return;
    }

    return;

}

/******************************************************************************
  函 数 名  : handle_error_msg
  功能描述  : 处理输入的错误消息，保留接口
  输入参数  : const ACE_Message_Block *mb:消息内容
  输出参数  : NA
  返回值    : 成功返回:0,失败返回:-1
  修订记录1 :
*******************************************************************************/
void CAC_Signal_Task::handle_error_msg(ACE_Message_Block * /*mb*/) const
{
    SVS_TRACE();
    return;
}

/******************************************************************************
  函 数 名  : update_record_thread
  功能描述  : 线程:更新实时录像索引线程，批量刷新索引到db
  输入参数  : NA
  输出参数  : NA
  返回值    : 成功返回:0,失败返回:-1
  修订记录1 :
*******************************************************************************/
int32_t CAC_Signal_Task::update_record_thread()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "The thread for updating realtime record index start running..."));

    ACE_Message_Block *mb = NULL;

    //录像文件索引消息列表
    list<ACE_Message_Block *> recordIndexList;
    uint32_t recordIndexTaskCount = 0;

    //最后更新时间
    time_t lastUpdateTime = ACE_OS::time( NULL );
    CThread_Stat_Reporter reporter("UpdateRealtimeRecordIndexThread");
    while ( brunning_ )
    {
        //获取更新录像文件索引消息超时时间
        ACE_Time_Value getTaskTimeout(ACE_OS::gettimeofday().sec() + SVS_GET_PROC_TASK_TIMEOUT, 0);

        //获取更新录像文件索引消息
        if (SVS_RESULT_FAILURE == rindex_queue_.dequeue_head(mb, &getTaskTimeout) )
        {
            //获取消息出错，重新打开消息队列
            if ( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "The queue of updating realtime record index dequeue failed."
                    "Try to reopen the queue."));

                (void)rindex_queue_.close();
                int32_t enqueueResult = rindex_queue_.open( RECORD_FILE_INDEX_QUEUE_WM,
                                                        RECORD_FILE_INDEX_QUEUE_WM);
                if( SVS_RESULT_FAILURE ==  enqueueResult )
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Failed to reopen the queue of updating realtime record index."));
                }
            }
            else //获取消息超时
            {
                ACE_OS::last_error(0);
                time_t waitTime = ACE_OS::time( NULL ) - lastUpdateTime;
                //还未到总超时时间，继续等待下一条记录的到来
                if ( SVS_MAX_DB_OPERATION_TIMEOUT > waitTime )
                {
                    continue;
                }
            }
            reporter.ReportStat();
            //没有任何消息需要处理，进行下一次等待
            if ( 0 == recordIndexTaskCount )
            {
                lastUpdateTime = ACE_OS::time( NULL );
                continue;
            }
        }
        else
        {
            Update_Record_Index *pIndex = (Update_Record_Index *)(void*)(mb->rd_ptr());

            ACE_Message_Block *pInfoMb = g_p_msg_buffer->alloc_mb(
                        CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
                        sizeof(SVS_STORESVR_RECORD_INDEX_INFO_REQ_MSG));

            if (pInfoMb == NULL)
            {
                SVS_LOG((SVS_LM_WARNING, "Allocate send message block failed."));
            }
            else
            {
                //设置索引上报消息大小
                SVS_STORESVR_RECORD_INDEX_INFO_REQ_MSG *pInfoMsg =
                    (SVS_STORESVR_RECORD_INDEX_INFO_REQ_MSG *)(void *)pInfoMb->wr_ptr();
                pInfoMb->wr_ptr(sizeof(SVS_STORESVR_RECORD_INDEX_INFO_REQ_MSG));

                FillCommonHeader(&pInfoMsg->Header,
                                         SVS_MSG_TYPE_STORESVR_RECORD_INDEX_INFO_REQ,
                                         CAC_RT_Record_Server::instance()->transaction_no(),
                                         sizeof(SVS_STORESVR_RECORD_INDEX_INFO_REQ_MSG));

                SetNLSDeviceId((uint8_t *)pInfoMsg->DeviceID,
                                sizeof(pInfoMsg->DeviceID),
                                (const char *)pIndex->LensId);
                SetNLSDeviceId((uint8_t *)pInfoMsg->NRUID,
                    sizeof(pInfoMsg->NRUID),
                    CAC_RT_Record_Server::instance()->str_id());
                pInfoMsg->IndexStartTime = (uint32_t)pIndex->StartTime;
                pInfoMsg->IndexEndTime = (uint32_t)pIndex->EndTime;
                strncpy((char *)pInfoMsg->StorePath, pIndex->RecordFilePath, FILEPATHLEN);
                pInfoMsg->VideoFlux = pIndex->VideoFlux;
                pInfoMsg->ReportIndexType = (uint8_t)pIndex->RecordReportType;
                int32_t iRet = CAC_RT_Record_Server::instance()->send_msg(pInfoMb);

                if (0 != iRet)
                {
                    SVS_LOG((SVS_LM_WARNING,
                        "Send record index info to svs_cc_store failed, "
                        "camera[%s], timeRange[%u-%u], storePath[%s], reportType[%d], ret[%d].",
                        pIndex->LensId, pIndex->StartTime, pIndex->EndTime,
                        pIndex->RecordFilePath, pIndex->RecordReportType, iRet));

                    g_p_msg_buffer->free_mb(pInfoMb);
                }

                SVS_LOG((SVS_LM_DEBUG,
                    "Send record index info to svs_cc_store success, "
                    "camera[%s], timeRange[%u-%u], storePath[%s], reportType[%d].",
                    pIndex->LensId, pIndex->StartTime, pIndex->EndTime,
                    pIndex->RecordFilePath, pIndex->RecordReportType));
            }

            //recordIndexTaskCount++;
            //recordIndexList.push_back( mb );
            //time_t waitTime = ACE_OS::time( NULL ) - lastUpdateTime;
            //还未到总超时时间并且未到指定最大记录条数，继续等待下一条记录的到来
            //if ( (SVS_MAX_OPERATION_PER_COMMIT > recordIndexTaskCount)
            //    && (SVS_MAX_DB_OPERATION_TIMEOUT > waitTime))
            //{
            //    continue;
            //}
        }

        reporter.ReportStat();

        //批量处理实时录像索引入库消息
        //(void)CAC_RT_Record_Device_Manager::instance()->handle_record_index( recordIndexList );

        //释放消息缓冲区
        //list<ACE_Message_Block*>::iterator itList = recordIndexList.begin();

        //for( ; itList != recordIndexList.end(); ++itList )
        //{
        //    g_p_msg_buffer->free_mb( *itList );
        //}

        lastUpdateTime = ACE_OS::time( NULL );
        //recordIndexList.clear();
        //recordIndexTaskCount = 0;

        g_p_msg_buffer->free_mb(mb);

        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "The thread for updating realtime record index exited."));

    return SVS_RESULT_OK;
}

/******************************************************************************
  函 数 名  : debug_thread
  功能描述  : 线程:telnet远程调试服务线程
  输入参数  : NA
  输出参数  : NA
  返回值    : 成功返回:0,失败返回:-1
  修订记录1 :
*******************************************************************************/
int32_t CAC_Signal_Task::debug_thread()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "The thread of remote debugging start running..."));

    //不启动调试线程
    if (NULL == p_debug_reactor_)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "The thread of remote debugging exited.The reactor is NULL." ));
        return SVS_RESULT_FAILURE;
    }

    (void)p_debug_reactor_->owner(ACE_OS::thr_self());

    int32_t ret = 0;
    while ( brunning_ )
    {
        ret = p_debug_reactor_->handle_events();
        if (ret < 0)
        {
          SVS_LOG((SVS_LM_WARNING,
                "Occur error while the reactor of remote debugging handling event."
                "The reactor address[0x%08x]",
                p_debug_reactor_));
        }
    }

    SVS_LOG((SVS_LM_WARNING, "The thread of remote debugging exited."));
    return ret;
}

/******************************************************************************
  函 数 名  : open_debug_acceptor
  功能描述  : 打开调试处理监听器
  输入参数  : NA
  输出参数  : NA
  返回值    : 成功返回:0,失败返回:-1
  修订记录1 :
*******************************************************************************/
int32_t CAC_Signal_Task::open_debug_acceptor()
{
    SVS_TRACE();
    // 使用epoll机制，创建epoll反应器
    try
    {
        p_debug_epoll_reactor_ = new ACE_Dev_Poll_Reactor(SVS_EPOLL_MAX_HANDLE_NUM);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to create the epoll reactor of remote debugging."));
        return SVS_RESULT_FAILURE;
    }

    // 创建反应器
    try
    {
        p_debug_reactor_ = new ACE_Reactor(p_debug_epoll_reactor_);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to create the reactor of remote debugging."));
        return SVS_RESULT_FAILURE;
    }

    // 创建远程调试监听器
    uint32_t ulAddr =  ACE_OS::inet_addr( "127.0.0.1");
    ACE_INET_Addr port_to_listen( remote_debug_port_, ACE_NTOHL( ulAddr ));

    try
    {
        p_debug_acceptor_ = new SVS_LOG_ACCEPTOR;
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to create the acceptor of remote debugging."));
        return SVS_RESULT_FAILURE;
    }

//    int32_t flags = 0;
//    ACE_SET_BITS (flags, ACE_NONBLOCK);  // Enable non-blocking

    // 打开远程调试监听服务
    int32_t openResult = p_debug_acceptor_->open( port_to_listen, p_debug_reactor_);
    if ( SVS_RESULT_FAILURE == openResult )
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Failed to open the acceptor of remote debugging."
            "The ip and port info: %s:%u.",
            port_to_listen.get_host_addr(),
            port_to_listen.get_port_number()));
        return SVS_RESULT_FAILURE;
    }
    ACE_OS::last_error(0);
    SVS_LOG((SVS_LM_INFO, "Open the remote debugging service succeeded."));
    return SVS_RESULT_OK;
}

/******************************************************************************
  函 数 名  : open_record_reactor
  功能描述  : 打开录像相关反应器
  输入参数  : NA
  输出参数  : NA
  返回值    : 成功返回:0,失败返回:-1
  修订记录1 :
*******************************************************************************/
int32_t CAC_Signal_Task::open_record_reactor()
{
    SVS_TRACE();
    // 创建策略更新反应器
    try
    {
        p_strategy_reactor_ = new ACE_Reactor;
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to create the reactor for updating record strategy."));
        return SVS_RESULT_FAILURE;
    }

    uint32_t maxLens = CAC_RT_Record_Server::instance()->max_lens();
    try
    {
        p_strategy_timer_manager_ =
            new CAC_Timer_Manager( (int32_t)(maxLens * RT_LENS_TIMER_MULTIPLE) );
     }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to create the timer of record strategy."));
        return SVS_RESULT_FAILURE;
    }

    bool initResult = p_strategy_timer_manager_->init(p_strategy_reactor_);
    if ( !initResult )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to initialize the timer of record strategy."));
        return SVS_RESULT_FAILURE;
    }

    CAC_Record_Inform_Manager::instance()->strategy_time_manager(p_strategy_timer_manager_);

    SVS_LOG((SVS_LM_INFO, "Initialize the timer and the reactor of record strategy succeeded."));
    return SVS_RESULT_OK;
}

/******************************************************************************
  函 数 名  : timer_call_back_thread
  功能描述  : 线程:定时器回调处理线程，需要访问数据库的一类定时器
  输入参数  : NA
  输出参数  : NA
  返回值    : 成功返回:0,失败返回:-1
  修订记录1 :
*******************************************************************************/
int32_t CAC_Signal_Task::timer_call_back_thread()
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_WARNING, "The thread of timer callback start running..."));

    ACE_Message_Block *mb = NULL;
    SVS_TIMER_CALL_BACK_MSG *pMsg = NULL;
    CThread_Stat_Reporter reporter("TimerCallbackThread");
    ACE_Time_Value tvDelay(1);

    while ( brunning_ )
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        if (SVS_RESULT_FAILURE == timer_call_back_queue_.dequeue_head(mb, &tv) )
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Occur error when the queue of timer callback dequeue."
                    "Try to reopen the queue."));

                (void)timer_call_back_queue_.close();
                int32_t openResult = timer_call_back_queue_.open( TIMER_CALL_BACK_QUEUE_WM, TIMER_CALL_BACK_QUEUE_WM );
                if( SVS_RESULT_FAILURE == openResult )
                {
                    SVS_LOG((SVS_LM_ERROR, "Occur error when reopen the queue of timer callback."));
                }
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();

        //回调处理
        pMsg = (SVS_TIMER_CALL_BACK_MSG *)(void*)mb->rd_ptr();
        switch ( pMsg->MsgType )
        {
            // 增加备份镜头的定时器回调处理
            case SVS_TIMER_CALL_BACK_MSG::RECORDBAK_Dev_Manager_TYPE:
                {
                    (void)CAC_Bakup_Record_Dev_Manager::instance()->on_timer(pMsg->EventId, pMsg->TimerId,pMsg->Dummy);
                }
                break;
            default:
                break;
        }

        //释放消息块内存
        g_p_msg_buffer->free_mb( mb );
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "The thread of timer callback exited."));

    return SVS_RESULT_OK;
}

/******************************************************************************
  函 数 名  : timer_call_back_second_thread
  功能描述  : 线程:定时器回调处理线程，不需要访问数据库的一类定时器
  输入参数  : NA
  输出参数  : NA
  返回值    : 成功返回:0,失败返回:-1
  修订记录1 :
*******************************************************************************/
int32_t CAC_Signal_Task::timer_call_back_second_thread()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "The second thread of timer callback start running..."));

    ACE_Message_Block *mb = NULL;
    SVS_TIMER_CALL_BACK_MSG *pMsg = NULL;
    CThread_Stat_Reporter reporter("TimerCallbackSecondThread");
    ACE_Time_Value tvDelay(1);
    while ( brunning_ )
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        if (SVS_RESULT_FAILURE == timer_call_back_queue_second_.dequeue_head(mb, &tv) )
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_CRITICAL,
                    "Occur error when the second queue of timer callback dequeue."
                    "Try to reopen the queue."));
                (void)timer_call_back_queue_second_.close();
                int32_t openResult = timer_call_back_queue_second_.open(    TIMER_CALL_BACK_QUEUE_WM,
                                                                        TIMER_CALL_BACK_QUEUE_WM );
                if( SVS_RESULT_FAILURE == openResult )
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Occur error when reopen the second queue of timer callback."));
                }
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();

        //回调处理
        pMsg = (SVS_TIMER_CALL_BACK_MSG *)(void*)mb->rd_ptr();
        switch ( pMsg->MsgType )
        {
            case SVS_TIMER_CALL_BACK_MSG::RT_RECORD_DEVICE_MANAGER_TYPE:
                {
                    CAC_RT_Record_Device_Manager *pManager =
                        (CAC_RT_Record_Device_Manager*)pMsg->ProcObject;
                    (void)pManager->on_timer( pMsg->EventId, pMsg->TimerId, pMsg->Dummy );
                }
                break;

            case SVS_TIMER_CALL_BACK_MSG::RECORD_INFORM_MANAGER_TYPE:
                {
                    CAC_Record_Inform_Manager* pManager =
                        (CAC_Record_Inform_Manager*)pMsg->ProcObject;
                    (void)pManager->on_check_strategy( pMsg->EventId, pMsg->TimerId );
                }
                break;

            case SVS_TIMER_CALL_BACK_MSG::REOCRD_SERVER:
                {
                    if(REFRESH_LOAD_REPORT_INTERVAL_EVENT_TYPE == pMsg->EventId)
                    {
                        CAC_RT_Record_Server::instance()->add_load_report_task();
                    }
                    else if(REPORT_RECORD_STATUS_INTERVAL_EVNET_TYPE == pMsg->EventId)
                    {
                        addReportRecordStatusTask();
                    }
                    else
                    {
                        (void)CAC_RT_Record_Server::instance()->refresh_configure();
                    }
                }
                break;

            case SVS_TIMER_CALL_BACK_MSG::UNACKED_INDEX_TIME_CHECK:
                {
                    if (CHECK_UNACKED_INDEX_EVENTID == pMsg->EventId)
                    {
                        (void)CIndexResendMgr::instance()->CheckUnackedIndex();
                    }
                }
                break;

            default:
                break;
        }

        //释放消息块内存
        g_p_msg_buffer->free_mb( mb );
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "The second thread of timer callback exited."));

    return SVS_RESULT_OK;
}

/******************************************************************************
  函 数 名  : timer_call_back_record_thread
  功能描述  : 线程:定时器回调处理线程，有关录像的状态检测的线程
  输入参数  : NA
  输出参数  : NA
  返回值    : 成功返回:0,失败返回:-1
  修订记录1 :
*******************************************************************************/
int32_t CAC_Signal_Task::timer_call_back_record_thread()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "The record status thread of timer callback start running..."));

    ACE_Message_Block *mb = NULL;
    SVS_TIMER_CALL_BACK_MSG *pMsg = NULL;
    CThread_Stat_Reporter reporter("TimerCallbackRecordStatusThread");
    ACE_Time_Value tvDelay(1);
    while ( brunning_ )
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        if (SVS_RESULT_FAILURE == timer_call_back_queue_record_.dequeue_head(mb, &tv) )
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_CRITICAL,
                    "Occur error when the record status queue of timer callback dequeue."
                    "Try to reopen the queue."));
                (void)timer_call_back_queue_record_.close();
                int32_t openResult = timer_call_back_queue_record_.open();
                if( SVS_RESULT_FAILURE == openResult )
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Occur error when reopen the record status queue of timer callback."));
                }
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();

        //回调处理
        pMsg = (SVS_TIMER_CALL_BACK_MSG *)(void*)mb->rd_ptr();
        switch ( pMsg->MsgType )
        {
            case SVS_TIMER_CALL_BACK_MSG::REALTIME_RECORD_STATUS_TYPE:
                {
                    CAC_RT_Record_Device_Manager *pManager =
                        (CAC_RT_Record_Device_Manager*)pMsg->ProcObject;
                    (void)pManager->on_timer( pMsg->EventId, pMsg->TimerId,pMsg->Dummy);
                }
                break;

            case SVS_TIMER_CALL_BACK_MSG::BACKUP_RECORD_STATUS_TYPE:
                {
                    CAC_Bakup_Record_Dev_Manager *pManager =
                        (CAC_Bakup_Record_Dev_Manager*)pMsg->ProcObject;
                    (void)pManager->on_timer( pMsg->EventId, pMsg->TimerId, pMsg->Dummy);
                }
                break;

            default:
                break;
        }

        //释放消息块内存
        g_p_msg_buffer->free_mb( mb );
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "The record status thread of timer callback exited."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : add_timer_callback_msg
功能描述  : 将回调消息放入到相应的回调消息队列中
输入参数  : ACE_Message_Block* msg : 回调消息内容
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::add_timer_callback_msg( ACE_Message_Block* msg )
{
    SVS_TRACE();
    //设置超时时间,防止队列满时死锁
    ACE_Time_Value tv(ACE_OS::gettimeofday());
    tv.set(tv.sec() + SVS_ENQUEUE_TIMEOUT, 0);

    SVS_TIMER_CALL_BACK_MSG *pMsg = (SVS_TIMER_CALL_BACK_MSG*)(void*)msg->rd_ptr();
    ACE_Message_Queue <ACE_SYNCH> *pCallbackQueue = NULL;

    //根据回调消息类型，放入不同的回调线程中进行处理
    switch ( pMsg->MsgType )
    {
        case SVS_TIMER_CALL_BACK_MSG::RT_RECORD_DEVICE_MANAGER_TYPE:    //回调线程2处理
        case SVS_TIMER_CALL_BACK_MSG::RECORD_INFORM_MANAGER_TYPE:       //回调线程2处理
        case SVS_TIMER_CALL_BACK_MSG::REOCRD_SERVER:
        case SVS_TIMER_CALL_BACK_MSG::UNACKED_INDEX_TIME_CHECK:
            {
                pCallbackQueue = &timer_call_back_queue_second_;
            }
            break;

        case SVS_TIMER_CALL_BACK_MSG::RECORDBAK_Dev_Manager_TYPE:
            {
                pCallbackQueue = &timer_call_back_queue_;
            }
            break;
        case SVS_TIMER_CALL_BACK_MSG::BACKUP_RECORD_STATUS_TYPE:
        case SVS_TIMER_CALL_BACK_MSG::REALTIME_RECORD_STATUS_TYPE:
            {
                // 这个定时器比较频繁，且任务单一，可以返回快速一些
                ACE_Time_Value timeout(ACE_OS::gettimeofday());
                timeout.set(timeout.sec(), timeout.usec() + 100*000);
                pCallbackQueue = &timer_call_back_queue_record_;
                int32_t result = pCallbackQueue->enqueue_tail( msg, &timeout );

                //入队列失败处理
                if ( SVS_RESULT_FAILURE == result )
                {
                    SVS_LOG( (SVS_LM_ERROR,
                        "Fail to put message to timer callback record enqueue."
                        "type[%d] ProcObject[0x%08x] EventId[%d] TimerId[%d] "
                        "queue full flag[%d] state[%d] msg count[%u] len[%u] size[%u].",
                        pMsg->MsgType,
                        pMsg->ProcObject,
                        pMsg->EventId,
                        pMsg->TimerId,
                        pCallbackQueue->is_full(),
                        pCallbackQueue->state(),
                        pCallbackQueue->message_count(),
                        pCallbackQueue->message_length(),
                        pCallbackQueue->message_bytes() ));

                    //释放消息块内存
                    g_p_msg_buffer->free_mb( msg );
                    return SVS_RESULT_FAILURE;
                }

                return result;
            }

        default:
            //非法消息
            SVS_LOG((SVS_LM_ERROR, "Handle the message of timer callback failed."
                "Unsuppored message type[%d].",
                pMsg->MsgType));

            //释放消息块内存
            g_p_msg_buffer->free_mb( msg );
            return SVS_RESULT_FAILURE;

    }

    int32_t result = pCallbackQueue->enqueue_tail( msg, &tv );

    //入队列失败处理
    if ( SVS_RESULT_FAILURE == result )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Fail to put message to timer callback enqueue."
            "type[%d] ProcObject[0x%08x] EventId[%d] TimerId[%d] "
            "queue full flag[%d] state[%d] msg count[%u] len[%u] size[%u].",
            pMsg->MsgType,
            pMsg->ProcObject,
            pMsg->EventId,
            pMsg->TimerId,
            pCallbackQueue->is_full(),
            pCallbackQueue->state(),
            pCallbackQueue->message_count(),
            pCallbackQueue->message_length(),
            pCallbackQueue->message_bytes() ));

        //释放消息块内存
        g_p_msg_buffer->free_mb( msg );
        return SVS_RESULT_FAILURE;
    }

    return result;
}

/*****************************************************************************
函 数 名  : monitor_event_proc_thread
功能描述  : 监控事件处理线程
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::monitor_event_proc_thread()
{
    SVS_LOG((SVS_LM_WARNING, "The thread for processing monitored event start running..."));

    CThread_Stat_Reporter reporter("MonitorEventThread");
    ACE_Time_Value tvDelay(1);
    ACE_Message_Block *mb = NULL;
    int32_t procResult = 0;
    while ( brunning_ )
    {
        //获取消息
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        if (SVS_RESULT_FAILURE == monitor_event_queue_.dequeue_head(mb, &tv) )
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "The queue of processing monitored event dequeue failed."
                    "Try to reopen the queue."));

                (void)monitor_event_queue_.close();

                if ( SVS_RESULT_OK != monitor_event_queue_.open(SVS_MONITOR_EVENT_QUEUE_WM,
                                                                SVS_MONITOR_EVENT_QUEUE_WM) )
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Failed to reopen the queue of processing monitored event."));
                    break;
                }
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();

        //处理
        procResult = handle_monitor_event( mb );
        if( 0 == procResult )
        {
            //释放消息块内存
            g_p_msg_buffer->free_mb( mb );
            continue;
        }

        //还没有处理成功再次处理
        if (-1 == monitor_event_queue_.enqueue_tail(mb) )
        {
            SVS_LOG((SVS_LM_ERROR,
                "The queue of processing monitored event enqueue failed."));
            break;
        }

        // 处理失败，等待一会
        (void)sleep( SVS_PROC_MONITOR_EVENT_INTERVAL_TIME );
    }

    SVS_LOG((SVS_LM_WARNING, "The thread for processing monitored event exited."));

    return SVS_RESULT_OK;
}


/* 总共四个线程，四个队列
 *  第一个线程  定时期线程  产生删除任务消息放入 del_task_queue_ 队列中
 *  第二个线程  任务处理线程 从 del_task_queue_ 取消息，处理后放入 del_record_queue_
 *  流控定时器周期从 del_record_queue_ 中取消息，根据消息类型放入 del_record_file_queue_
 *  或者 del_prerecord_file_queue_中
 *  第三个线程  定时删除文件消息和磁盘满删除文件消息处理线程  从 del_record_file_queue_ 取消息
 *  第四个线程  预录文件删除消息 从 del_prerecord_file_queue_
 */
/*****************************************************************************
函 数 名  : proc_del_record_task_thread
功能描述  : 线程:循环处理删除任务队列中的任务消息
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::proc_del_record_task_thread()
{
    SVS_LOG((SVS_LM_WARNING,
        "[DEL]The thread for processing the tasks of deleting record start running..."));

    ACE_Message_Block* mb = NULL;
    CThread_Stat_Reporter reporter("ProcessDeletingTaskThread");
    ACE_Time_Value tvDelay(1);
    while(brunning_)
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        if(-1 == del_task_queue_.dequeue_head(mb, &tv))
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "[DEL]The queue of deleting record task failed to dequeue."));
                break;
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }

        reporter.ReportStat();

        //分发处理消息
        del_manager_.handle_del_record(mb);

        // 释放消息所占内存
        (void)mb->release();
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING,
        "[DEL]The thread for processing the tasks of deleting record exited."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : del_record_file_loop
功能描述  : 线程:循环处理删除文件消息队列中的消息
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::del_record_file_loop()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "[DEL]The thread for deleting record file start running..."));

    CThread_Stat_Reporter reporter("DeletingRecordFileThread");
    ACE_Time_Value tvDelay(1);
    ACE_Message_Block* mb = NULL;

    while ( brunning_ )
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        // 获取删除任务
        if ( -1 == del_record_file_queue_.dequeue_head(mb, &tv) )
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR, "[DEL]The queue of deleting record file dequeue failed."));
                break;
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();
        // 分发处理消息
        del_manager_.handle_del_record_file(mb);

        // 释放消息所占内存
        (void)mb->release();
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "[DEL]The thread for deleting record file exited."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : del_prerecord_file_loop
功能描述  : 线程:循环处理预录文件删除消息队列中的消息
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::del_prerecord_file_loop()
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_WARNING, "[DEL]The thread for processing prerecord file start running..."));

    ACE_Message_Block* mb = NULL;
    CThread_Stat_Reporter reporter("DeletingPrerecordFileThread");
    ACE_Time_Value tvDelay(1);

    while ( brunning_ )
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        if (-1 == del_prerecord_file_queue_.dequeue_head(mb, &tv))
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR, "[DEL]The queue of processing prerecord file dequeue failed."));
                break;
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();
        del_manager_.handle_del_prerecord_file(mb);  //分发处理消息

        (void)mb->release();       //释放消息所占内存
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "[DEL]The thread for processing prerecord file exited."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : del_date_dir_loop
功能描述  : 线程:删除日期空目录线程以及垃圾文件，定时器回调添加任务
输入参数  : NA
输出参数  : NA
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::del_date_dir_loop()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "[DEL]The thread for processing overdue date directory start running..."));

    ACE_Message_Block* mb = NULL;
    while ( brunning_ )
    {
        // 删除过期目录消息出队列
        if (-1 == del_date_dir_queue_.dequeue_head(mb))
        {
            SVS_LOG((SVS_LM_ERROR, "[DEL]The queue of processing overdue date dequeue failed."));
            break;
        }

        del_manager_.handle_del_date_dir(mb);
        g_p_msg_buffer->free_mb(mb);
        mb = NULL;


        // 更新前端录像备份记录－－为方便把这个功能放在这里
        (void)CAC_Bakup_Record_Dev_Manager::instance()->update_backup_device_flag();
    }

    SVS_LOG((SVS_LM_WARNING, "[DEL]The thread for processing overdue date directory exited."));

    return SVS_RESULT_OK;
}

int32_t CAC_Signal_Task::procReportRecordStatusTaskThread()
{
    SVS_LOG((SVS_LM_WARNING,  "The thread for report record status start running..."));

    ACE_Message_Block *mb = NULL;
    CThread_Stat_Reporter reporter("procReportRecordStatusTaskThread");
    ACE_Time_Value tvDelay(1);
    while ( brunning_ )
    {
        //获取更新录像文件索引消息超时时间
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        //获取更新录像文件索引消息
        if (SVS_RESULT_FAILURE == m_recordStatusCollectQueue.dequeue_head(mb, &tv) )
        {
            //获取消息出错，重新打开消息队列
            if ( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "The queue of report record status task dequeue failed."
                    "Try to reopen the queue."));
                (void)m_recordStatusCollectQueue.close();
                int32_t enqueueResult = m_recordStatusCollectQueue.open();
                if( SVS_RESULT_FAILURE ==  enqueueResult )
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Failed to reopen the report record status task queue."));
                }
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }

        reporter.ReportStat(1);

        // 录像状态上报负载
        (void)SVS_Stat_Manager::instance().reportRecordStatus(mb);

        g_p_msg_buffer->free_mb(mb);
    }

    SVS_LOG((SVS_LM_WARNING, "The thread of report record status exited."));

    return SVS_RESULT_OK;
}
/*****************************************************************************
函 数 名  : del_record_timer_callback
功能描述  : 处理定时器回调消息
输入参数  : int32_t eventId:定时器事件ID,标识定时器事件类型
输出参数  : NA
返 回 值  : NA
修改历史  :
*****************************************************************************/
void CAC_Signal_Task::del_record_timer_callback( void *pArg,
                            int32_t eventId,
                            int32_t /*timerId*/,
                            int32_t /*nDummy*/ )
{
    SVS_TRACE();

    switch ( eventId )
    {
        case SVS_DEL_TIMER_CONTROL: // 任务流控定时器 到时间
        {
            ((CAC_Signal_Task *)pArg)->handle_del_timer_control();

            return;
        }

        case SVS_DEL_TIMER_PRERECORD: // 删除预录定时器
        {
            ((CAC_Signal_Task *)pArg)->handle_del_timer_prerecord();

            return;
        }
        case SVS_DEL_TIMER_DATE_DIR: // 删除日期目录定时器
        {
            ((CAC_Signal_Task *)pArg)->handle_del_timer_date_dir();

            return;
        }

        default:
            SVS_LOG((SVS_LM_WARNING,
                "[DEL]Process timer event failed.Unknown timer event id [%d]",
                eventId));

            break;
    }

    return;
}

/*****************************************************************************
函 数 名  : handle_del_timer_control
功能描述  : 处理删除流控定时期触发
输入参数  : NA
输出参数  : NA
返 回 值  : NA
修改历史  :
*****************************************************************************/
void CAC_Signal_Task::handle_del_timer_control()
{
    SVS_TRACE();

    time_control_count++;

    if ( time_control_count % 30 == 0 )
    {
        SVS_LOG((SVS_LM_INFO,
            "[DEL]The thread of flow control have processed messages count[%d] "
            "and the queue size is [%d] now.",
            time_control_count,
            del_record_queue_.message_count()));
    }

    //删除队列为空直接返回
    if ( del_record_queue_.is_empty() )
    {
        return;
    }

    // 从队列 del_record_queue_ 中取出一个消息，判断其消息类型；
    // 如果是定时删除消息或者磁盘满删除消息， 则放到队列 del_record_file_queue_ 中
    // 如果是预录文件删除消息，则放到队列 del_prerecord_file_queue_ 中
    ACE_Message_Block *mb = NULL;
    ACE_Time_Value tvDeTimeOut(ACE_OS::gettimeofday().sec(), 0);
    if ( -1 == del_record_queue_.dequeue_head(mb, &tvDeTimeOut) )
    {
        SVS_LOG((SVS_LM_ERROR, "[DEL]The queue of deleting task dequeue failed."));
        return;
    }

    SVS_DEL_RECORD_FILE *pDelRecordFile = (SVS_DEL_RECORD_FILE *)((void *)mb->rd_ptr());
    CAC_RT_Record_Server::instance()->print_msg_del_record(pDelRecordFile);

    int32_t nInQueueResult = -1;
    ACE_Time_Value tvEnTimeOut(ACE_OS::gettimeofday().sec(), 0);

    // 如果是预录删除任务
    if ( DEL_RECORD_TYPE_PRERECORD == pDelRecordFile->MsgType )
    {
        nInQueueResult = del_prerecord_file_queue_.enqueue_tail(mb, &tvEnTimeOut);
    }
    else // 其他删除任务
    {
        nInQueueResult = del_record_file_queue_.enqueue_tail(mb, &tvEnTimeOut);
    }

    // 消息入队列失败，尝试将任务重新放回
    if ( -1 == nInQueueResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Process flow control deleting task failed:."
            "The queue enqueue failed.msgtype[%d]",
            pDelRecordFile->MsgType));

        if ( -1 == del_record_queue_.enqueue_tail(mb, &tvEnTimeOut))
        {
            // 失败后,消息放回队列
            SVS_LOG((SVS_LM_ERROR,
                "[DEL]The queue of flow control deleting task enqueue failed."));
            delete mb;
            mb = NULL;
        }
    }

    return;
}

/*****************************************************************************
函 数 名  : handle_del_timer_prerecord
功能描述  : 删除预录文件定时检测线程
输入参数  : NA
输出参数  : NA
返 回 值  : NA
修改历史  :
*****************************************************************************/
void CAC_Signal_Task::handle_del_timer_prerecord()
{
    SVS_TRACE();

    uint32_t msgCount = del_prerecord_task_queue_.message_count();

    SVS_LOG((SVS_LM_DEBUG,"[DEL]The queue size of deleting prerecord task is [%d]", msgCount));

    //没有删除预录任务
    if ( del_prerecord_task_queue_.is_empty() )
    {
        return;
    }

    int32_t tempRet = 0;
    SVS_LENS_PARAMETER lensParam;
    ACE_Message_Block *mb = NULL;
    ACE_Time_Value tvTimeout(ACE_OS::gettimeofday());
    tvTimeout.set(tvTimeout.sec(), tvTimeout.usec() + DEL_RECORD_TASK_ENQUEUE_TIMEOUT);
    time_t currTime = time(NULL);

    while(msgCount > 0)
    {
        --msgCount;
        mb = NULL;
        ACE_Time_Value tvDeTimeOut(ACE_OS::gettimeofday().sec(), 0);

        if ( -1 == del_prerecord_task_queue_.dequeue_head(mb, &tvDeTimeOut) )
        {
            SVS_LOG((SVS_LM_ERROR, "[DEL]The queue of deleting prerecord task dequeue failed."));

            return;
        }

        SVS_DEL_RECORD *pDelPrerecordTask = (SVS_DEL_RECORD *)((void *)mb->rd_ptr());

        tempRet = CAC_Record_Inform_Manager::instance()->get_lens_param(
                                            (char *)pDelPrerecordTask->LensId,
                                            lensParam);

        //如果获取镜头参数失败，则任务是最大时间
        if (SVS_RESULT_FAILURE == tempRet)
        {
            SVS_LOG((SVS_LM_WARNING, "[DEL]Get camera[%s]'s parameter failed.",
                pDelPrerecordTask->LensId));

            lensParam.BeforeSeconds = SVS_PRORECORD_MAX_TIME;
        }

        // 如果没有到时间，则重新入删除预录任务队列
        // 判断当前时间是否超过整点加预录时间,如果没有超过,消息回队列继续等待处理
        // 作此判断是因为如果原Rt部分生成预录文件删除消息,再等待预录时长入Del的队列,
        // 在等待的这段时间内,如果预录时间被改短了, Rt就无法把消息放入Del的队列
        if ( currTime <= ( ADJUST_FORWARD(pDelPrerecordTask->DeleteTime, SECONDS_PER_HOUR)
                            + (time_t)lensParam.BeforeSeconds))
        {
            if ( -1 == CAC_RT_Record_Server::instance()->enqueue_prerecord_del_task(mb, &tvTimeout))
            {
                SVS_LOG((SVS_LM_ERROR,
                    "[DEL]The queue of deleting prerecord task retry to enqueue failed.Camera[%s]",
                    pDelPrerecordTask->LensId));
                (void)mb->release();
                mb = NULL;
            }
            continue;
        }

        // 如果到时间了，则把删除预录任务加入删除任务队列
        if ( -1 == CAC_RT_Record_Server::instance()->enqueue_del_task(mb, &tvTimeout))
        {
            SVS_LOG((SVS_LM_ERROR,"The queue of deleting task enqueue failed.Camera[%s]",
                    pDelPrerecordTask->LensId));
            (void)mb->release();
            mb = NULL;
        }
    }
    return;
}

/*****************************************************************************
函 数 名  : handle_del_timer_date_dir
功能描述  : 处理删除过期目录定时任务
输入参数  : NA
输出参数  : NA
返 回 值  : NA
修改历史  :
*****************************************************************************/
void CAC_Signal_Task::handle_del_timer_date_dir()
{
    SVS_LOG((SVS_LM_DEBUG, "[DEL]Process the timer of deleting overdue date directory."));

    //生成一个定时删除任务，放入到队列 del_task_queue_ 中
    ACE_Message_Block *mb = NULL;
    uint32_t msgLen = sizeof(SVS_STORESVR_QUERY_LENS_MINTIME_REQ_MSG);
    mb = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG , msgLen);
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR, "[DEL]Alloc mb for deleting overdue date directory fail."));
        return;
    }


    PSVS_STORESVR_QUERY_LENS_MINTIME_REQ_MSG pMsg = NULL;
    pMsg = (PSVS_STORESVR_QUERY_LENS_MINTIME_REQ_MSG)(void*)mb->rd_ptr();

    FillCommonHeader(&pMsg->Header,
                             SVS_MSG_TYPE_STORESVR_QUERY_LENS_MINTIME_REQ,
                             CAC_RT_Record_Server::instance()->transaction_no(),
                             sizeof(SVS_STORESVR_QUERY_LENS_MINTIME_REQ_MSG));

    // 填充定时删除任务消息
    //SVS_DEL_RECORD *pDelRecord = (SVS_DEL_RECORD *)((void *)mb->rd_ptr());
    //pDelRecord->MsgType = DEL_RECORD_TYPE_DATE_DIR;
    //pDelRecord->DeleteTime = time(NULL);
    //pDelRecord->LensId[0] = '\0';

    //CAC_RT_Record_Server::instance()->print_msg_del_task(pDelRecord);

    // 定时删除任务入队列
    ACE_Time_Value tvEnTimeOut(ACE_OS::gettimeofday().sec(), 0);
    if( -1 ==   del_date_dir_queue_.enqueue_tail(mb, &tvEnTimeOut) )
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]The queue of deleting overdue date directory enqueue failed." ));

        g_p_msg_buffer->free_mb(mb);
        return;
    }

    SVS_LOG((SVS_LM_INFO,
        "[DEL]Process the timer of deleting overdue date directory succeeded."));

    //申请的消息已经放入消息队列，由处理者释放。屏蔽lint告警
    return;  //lint !e429
}

/*****************************************************************************
函 数 名  : debug_get_msg_info
功能描述  : 支撑telnet调试用，消息处理个数信息
输入参数  : const uint32_t szBufferInfo:缓存区大小
输出参数  : char * strBufferInfo:调试结果缓存区
返 回 值  : NA
修改历史  :
*****************************************************************************/
int32_t CAC_Signal_Task::debug_get_msg_info(char * strBufferInfo,
                                       const uint32_t szBufferInfo)const
{
    (void)ACE_OS::snprintf( strBufferInfo,
                            szBufferInfo,
                            "\tThreadIndex         ThreadId          UsedBufferTotalNum\n" );

    size_t writeLen = ACE_OS::strlen( strBufferInfo );

    int32_t i = 0;
    int32_t msgCount[CSVS_Mb_Buffer_Manager::MB_TYPE_MAX_VALUE][CSVS_Mb_Buffer_Manager::MB_LEN_MAX_VALUE] = {{0}};

    for ( i = 0; i < SVS_THREAD_MAX_NUM; i++ )
    {
        if ( NULL == p_msg_buffer_manager_[i] )
        {
            continue;
        }
        writeLen = ACE_OS::strlen( strBufferInfo );

        //缓冲区已用完，退出循环
        if( 0 == szBufferInfo-writeLen )
        {
            break;
        }

        (void)ACE_OS::snprintf(  &strBufferInfo[writeLen],
                                 szBufferInfo-writeLen,
                                 "\n\t    %d    %u(%d)  ",
                                 i,
                                 thread_id_list_[i],
                                 thread_id_list_[i] );

        for ( uint8_t j = 0; j < CSVS_Mb_Buffer_Manager::MB_TYPE_MAX_VALUE; j++ )
        {
            for (uint8_t k = 0; k < CSVS_Mb_Buffer_Manager::MB_LEN_MAX_VALUE; k++ )
            {

                int32_t allocMsgBufferCount = p_msg_buffer_manager_[i]->alloced_msg_buffer_count(j,  k);
                if ( 0 == allocMsgBufferCount )
                {
                    continue;
                }
                writeLen = ACE_OS::strlen( strBufferInfo );

                msgCount[j][k] += allocMsgBufferCount;



                (void)ACE_OS::snprintf(  &strBufferInfo[writeLen],
                                         szBufferInfo-writeLen,
                                         "[%d,%d](%d) ",
                                         j,
                                         k,
                                         allocMsgBufferCount );
            }
        }
    }

    writeLen = ACE_OS::strlen( strBufferInfo );
    (void)ACE_OS::snprintf(  &strBufferInfo[writeLen],
                             szBufferInfo-writeLen,
                             "\n\n\tUsedBufferTotalNum\n" );
    for ( int32_t j = 0; j < CSVS_Mb_Buffer_Manager::MB_TYPE_MAX_VALUE; j++ )
    {
        for (int32_t k = 0; k < CSVS_Mb_Buffer_Manager::MB_LEN_MAX_VALUE; k++ )
        {
            if ( 0 == msgCount[j][k] )
            {
                continue;
            }
            writeLen = ACE_OS::strlen( strBufferInfo );

            (void)ACE_OS::snprintf(  &strBufferInfo[writeLen],
                                     szBufferInfo-writeLen,
                                     "\n\t[%d,%d]    %d ",
                                     j,
                                     k,
                                     msgCount[j][k] );
        }
    }

    writeLen = ACE_OS::strlen( strBufferInfo );
    (void)ACE_OS::snprintf(  &strBufferInfo[writeLen],
                             szBufferInfo-writeLen,
                              "\n\n\tHandleMsgThreadIndex  MsgTotalNum  RouteNotify RouteClose  DiviceStatusNotify(NEW) AlarmEvent\n" );

    for ( i = 0; i < SVC_NUM; i++ )
    {
            writeLen = ACE_OS::strlen( strBufferInfo );

            //缓冲区已用完，退出循环
            if( 0 == szBufferInfo-writeLen )
            {
                break;
            }

            (void)ACE_OS::snprintf(  &strBufferInfo[writeLen],
                                     szBufferInfo-writeLen,
                                     "\t      %d         %8d   %8d   %8d   %8d     %8d \n",
                                     i,
                                     msg_total_count_[i],
                                     msg_sort_msg_count[i][MSG_SORT_ROUTER_INFO],
                                     msg_sort_msg_count[i][MSG_SORT_SCHEDULE_OVER_REQ],
                                     msg_sort_msg_count[i][MSG_SORT_PLATDEV_INFORM_REQ],
                                     msg_sort_msg_count[i][MSG_SORT_ALARM_INFO] );

    }
    return 0;
}

int32_t CAC_Signal_Task::load_report_thread()
{
    SVS_LOG((SVS_LM_WARNING,  "The thread for load report start running..."));

    ACE_Message_Block *mb = NULL;
    CThread_Stat_Reporter reporter("LoadReportThread");
    ACE_Time_Value tvDelay(1);
    while ( brunning_ )
    {
        //获取更新录像文件索引消息超时时间
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        //获取更新录像文件索引消息
        if (SVS_RESULT_FAILURE == load_report_queue_.dequeue_head(mb, &tv) )
        {
            //获取消息出错，重新打开消息队列
            if ( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "The queue of load report dequeue failed."
                    "Try to reopen the queue."));
                (void)load_report_queue_.close();
                int32_t enqueueResult = load_report_queue_.open();
                if( SVS_RESULT_FAILURE ==  enqueueResult )
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Failed to reopen the load report queue."));
                }
            }

            ACE_OS::last_error(0);
            reporter.ReportStat(0);
            continue;
        }

        reporter.ReportStat(1);

        // 上报负载
        (void)SVS_Stat_Manager::instance().loadReport(mb);

        g_p_msg_buffer->free_mb(mb);
    }

    SVS_LOG((SVS_LM_WARNING, "The thread of load reportexited."));

    return SVS_RESULT_OK;
}

void CAC_Signal_Task::addReportRecordStatusTask()
{
    uint32_t msgLen = sizeof(SVS_REPORT_RECORD_STATUS_TASK);
    ACE_Message_Block* mb = g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_EVENT_MSG,
                                                       msgLen );

    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create record status report task failed.Fail to allocate mb. type[%d], size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_EVENT_MSG,
            msgLen ));

        return ;
    }

    // 构造消息,只需要构造消息类型即可，改消息类型遍历所有磁盘
    SVS_REPORT_RECORD_STATUS_TASK* pMsg = (SVS_REPORT_RECORD_STATUS_TASK*)(void*)mb->wr_ptr();
    pMsg->ReportType = 0;

    // 消息入队列
    // 把心跳消息加入队头
    ACE_Time_Value tv(ACE_OS::gettimeofday());
    tv.sec(tv.sec() + 10);

    if(SVS_RESULT_FAILURE == m_recordStatusCollectQueue.enqueue_head( mb,&tv))
    {
        SVS_LOG((SVS_LM_ERROR, "Record status report task message enqueue failed."));
        g_p_msg_buffer->free_mb(mb);
        return ;
    }

    SVS_LOG((SVS_LM_INFO, "Add record status report task successfully."));

    return ;
}


