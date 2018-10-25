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
        // �رձ���������Ϣ����
        (void)bakup_task_queue_.close();

        // �ͷ��ڴ�
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
 �� �� ��  : init_task
 ��������  : ��ʼ��������Ӧ��,��Ϣ����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::init_task()
{
    SVS_TRACE();

    // ����Ϣ���У�����Ȧ���Ӷ���ȡ����
    if (SVS_RESULT_OK != open_queue())
    {
        return SVS_RESULT_FAILURE;
    }

    // �������Ӵ洢���������epoll��Ӧ��
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

    // ע�ᷴӦ��
    (void)CAC_RT_Record_Server::instance()->connect_reactor(
        p_connect_reactor_,
        msg_queue());

    // ������ʱ����Ӧ��
    try
    {
        p_timer_reactor_ = new ACE_Reactor;
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Create the reactor of timer failed."));

        return SVS_RESULT_FAILURE;
    }

    // ע�ᶨ������Ӧ��
    bool initResult = CAC_Timer_Manager::instance()->init(p_timer_reactor_);
    if ( !initResult )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Initialize the reactor of timer failed."));

        return SVS_RESULT_FAILURE;
    }

    // ���������ط���
    int32_t iRet = CIndexResendMgr::instance()->Init(CAC_Timer_Manager::instance());
    if (0 != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Open signal task failed, as init index resend manager error, "
            "return code[%d].",
            iRet));
        return SVS_RESULT_FAILURE;
    }

    //�򿪵��Լ�����
    if ( SVS_RESULT_OK != open_debug_acceptor())
    {
        return SVS_RESULT_FAILURE;
    }

    // �򿪲��Ը��·�Ӧ��
    if (SVS_RESULT_OK != open_record_reactor())
    {
        return SVS_RESULT_FAILURE;
    }

    // ��ɾ����ʱ��
    if (SVS_RESULT_OK != create_timer())
    {
        return SVS_RESULT_FAILURE;
    }

    // ����������
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
 �� �� ��  : open
 ��������  : ��ʼ��������Ӧ��,���������߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ����ط�-1��ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::open(void *)
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO, "Start all threads..."));

    // ��ʼ����Ϣ���չ�����,���������̺߳����ݻ�������ʼ��
    if (0 != SVS_Media_Recv_Manager::instance().open(NULL))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the media receive manager failed."));

        return SVS_RESULT_FAILURE;
    }

    // ��ʼ������̨������
    if(SVS_RESULT_OK != SVS_Console_Server::instance()->open(NULL))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the Console server failed."));

        return SVS_RESULT_FAILURE;
    }

    // �򿪴��̹������Ĵ����߳�
    if (SVS_RESULT_OK != SVS_Disk_Manager::instance().open(NULL))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the disk  manager failed."));

        return SVS_RESULT_FAILURE;
    }

    // ��ͳ��ģ���߳�
    if(SVS_RESULT_OK != SVS_Stat_Manager::instance().open(NULL))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the stat  manager failed."));

        return SVS_RESULT_FAILURE;
    }

    //���������߳�
    uint32_t runThreadNum = (uint32_t)MAX_THREAD + (uint32_t)SVC_NUM;
    //�޸��߳�ջ��С-----------------
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

    // ��ʼ���߳�ջ���ֵ
    uint32_t i = 0;
    for ( i=0; i<runThreadNum; i++)
    {
        stack_size[i] = SVS_THREAD_STATCK_MAX;
    }

    //���������̺߳ͷ����߳�
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
 �� �� ��  : stop
 ��������  : ��ʼ��������Ӧ��,���������߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
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

    //ֹͣ���Լ�����
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

    //�ȴ������߳��˳�
    (void)wait();
    SVS_LOG((SVS_LM_WARNING, "Stop RtRecordSvr succeeded."));

}

int32_t CAC_Signal_Task::close(u_long)
{
    return 0;
}

/*****************************************************************************
 �� �� ��  : svc
 ��������  : �����̣߳����з����߳����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  : ����B035��������ǰ��¼�񱸷���Ϣ�����߳�
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

    //ϵͳ�߳�
    runResult = create_thread_system(threadIndex);

    //ƥ���Ƿ�Ϊ¼������߳�
    if(RT_THREAD_INDEX_UNFOUND == runResult)
    {
        runResult = create_threads_Record( threadIndex);
    }

    //ƥ���Ƿ���ɾ������߳�
    if (RT_THREAD_INDEX_UNFOUND == runResult)
    {
        runResult = create_threads_del(threadIndex);
    }

    //ƥ���Ƿ�Ϊmedia��������߳�
    if(RT_THREAD_INDEX_UNFOUND == runResult)
    {
        runResult = create_threads_media( threadIndex);
    }

    g_p_msg_buffer->clear();

    return runResult;
}

/*****************************************************************************
 �� �� ��  : create_thread_system
 ��������  : ����ϵͳ����̣߳��綨ʱ���ص���ͨѶ��StoreSvr���߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::create_thread_system(int32_t threadIndex)
{
    SVS_TRACE();

    int32_t findResult = RT_THREAD_INDEX_UNFOUND;

    switch (threadIndex)
    {
        // ����¼�������ϵͳ�߳�: ͬʱ��������¼�������ϵͳ����Ϣ
        case CONNECT_THREAD:
            findResult = start_connect();
            break;

        // ��ʱ���ص������߳�
        case TIMER_CALL_BACK_THREAD:
            findResult = timer_call_back_thread();
            break;

        // ��ʱ���߳�
        case TIMER_THREAD:
            findResult = timer_event_loop();
            break;

        // ����¼������߳�
        case MONITOR_EVENT_PROC_THREAD:
            findResult = monitor_event_proc_thread();
            break;

        // �����߳�
        case DEBUG_THREAD:
            findResult = debug_thread();
            break;

        // �ص���Ϣ�����߳�
        case TIMER_CALL_BACK_SECOND_THREAD:
            findResult = timer_call_back_second_thread();
            break;

        // ��ʱ���ص�����¼��״̬����߳�
        case TIMER_CALL_BACK_RECORD_THREAD:
            findResult = timer_call_back_record_thread();
            break;

        // �����ϱ��߳�
        case LOAD_REPORT_THREAD:
            findResult = load_report_thread();
            break;

        default:
            break;
    }

    return findResult;
}

/*****************************************************************************
 �� �� ��  : create_threads_Record
 ��������  : ����¼������̣߳�����Ը����̡߳����̿ռ����̡߳������������̵߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::create_threads_Record(int32_t threadIndex)
{
    SVS_TRACE();
    int32_t findResult = RT_THREAD_INDEX_UNFOUND;

    switch(threadIndex)
    {   // ����¼������߳�
        case RENEW_STRATEGY_THREAD:
            findResult = renew_strategy_loop();
            break;

        // ���Լ���߳�
        case STRATEGY_CHECK_THREAD:
            findResult = strategy_check_thread();
            break;

        // ����¼�������߳�
        case UPDATE_RECORD_THREAD:
            findResult = update_record_thread();
            break;
        // ����ǰ��¼�񱸷�����
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
 �� �� ��  : create_threads_del
 ��������  : ����¼��ɾ������̣߳���ɾ���������̡߳�ɾ��Ԥ¼�����̵߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::create_threads_del(int32_t threadIndex)
{
    SVS_TRACE();
    int32_t findResult = RT_THREAD_INDEX_UNFOUND;
    switch(threadIndex)
    {
        // ɾ�������߳�
        case PROC_DEL_RECORD_TASK_THREAD:
            findResult = proc_del_record_task_thread();
            break;

        // ɾ���ļ��߳�
        case DEL_RECORD_FILE_THREAD:
            findResult = del_record_file_loop();
            break;

        // ɾ��Ԥ¼�ļ��߳�
        case DEL_PRERECORD_FILE_THREAD:
            findResult = del_prerecord_file_loop();
            break;

        // ɾ������Ŀ¼�߳�
        case DEL_DATE_DIR_THREAD:
            findResult = del_date_dir_loop();
            break;
        default:
            break;
    }

    return findResult;
}

/*****************************************************************************
 �� �� ��  : create_threads_media
 ��������  : ����ý�������պ�д����̣߳�����ý�������պ�д¼���ļ��߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::create_threads_media(int32_t threadIndex)
{
    SVS_TRACE();
    int32_t findResult = RT_THREAD_INDEX_UNFOUND;
    switch(threadIndex)
    {
        case -1: //����
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
                    //��Ϣ�����߳� SVC_NUM��
                    //��Ϣ�ַ������߳�
                    thrIndex -= MAX_THREAD;
                    findResult = proc_message_thread( thrIndex );
                }
            }
            break;
    }

    return findResult;
}

/*****************************************************************************
 �� �� ��  : open_queue
 ��������  : ����Ϣ���У�����ϵͳ��ʵʱ¼��ǰ��¼�񱸷�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::open_queue()
{
    SVS_TRACE();
    //������Ϣ����ˮλ
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

    // �򿪱���������Ϣ����
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

    // �򿪸����ϱ����У���������̫���и����ϱ���OK,����ʹ��Ĭ��ֵ
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

    // D�¼�¼�������Ż�
    (void)timer_call_back_queue_second_.close();
    if ( SVS_RESULT_OK != timer_call_back_queue_second_.open(   TIMER_CALL_BACK_QUEUE_WM,
                                                                TIMER_CALL_BACK_QUEUE_WM ) )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the second queue of timer callback failed."));

        return SVS_RESULT_FAILURE;
    }

    // D�¼�¼�������Ż�
    (void)timer_call_back_queue_record_.close();
    if ( SVS_RESULT_OK != timer_call_back_queue_record_.open())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open the record queue of timer callback failed."));

        return SVS_RESULT_FAILURE;
    }

    // ¼��״̬�ϱ���ʱ������
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
 �� �� ��  : open_del_queue
 ��������  : ��¼��ɾ����ض���
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::open_del_queue()
{
    SVS_TRACE();
    //����¼��ɾ����ص���Ϣ����ˮλ
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

    // ����ʵʱ¼��״̬��ⶨʱ��
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

    // ����ǰ��¼�񱸷�״̬��ⶨʱ��
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

    // ˢ�������ļ�����
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

    // ����¼��״̬�ϱ���ʱ��
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
 �� �� ��  : create_del_timer
 ��������  : ����ɾ����ض�ʱ�����綨ʱɾ����ɾ��Ԥ¼���ơ�ɾ�����ء�ɾ�����ڵ�
             ����Ŀ¼��
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::create_del_timer()
{
    SVS_TRACE();

    // ����ɾ�����ض�ʱ��
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


    // ����ɾ��Ԥ¼��ʱ��
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

    // ����ɾ������Ŀ¼��ʱ��
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
 �� �� ��  : proc_message_thread
 ��������  : �߳�:��Ϣ�����̣߳������StoreSvr����������Ϣ
 �������  : uint32_t nIndex:��Ϣ�����̵߳��߳�ID
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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

        if ( -1 == getq(mb,&tv) )            //�ȴ���Ϣ
        {
            // ������ǳ�ʱ
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

        //��Ϣ��������
        while ( brunning_ && (NULL != mb) )
        {
            mbHandle = mb;
            mb = mb->cont();

            mbHandle->cont( NULL );

            handle_message(nIndex, mbHandle);  //�ַ�������Ϣ
        }
    }

    SVS_LOG((SVS_LM_WARNING, "The thread of processing message exited."
        "The thread index is [%d]",
        nIndex));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : start_connect
 ��������  : �߳�:¼����������ͨѶ�������߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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
 �� �� ��  : timer_event_loop
 ��������  : �߳�:��ʱ�������߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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
 �� �� ��  : handle_monitor_event
 ��������  : ����¼�����ص�����
 �������  : const ACE_Message_Block *mb:��ص����¼�����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
//����¼�����ص�����
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
        case RT_MONITOR_EVENT_START_RECORD:   //��������Store Svr
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

                //ֹͣ����ʵʱ¼��
                int32_t stopRtRecordRet = CAC_Record_Inform_Manager::instance()->stop_all_record();

                // ֹͣ����ǰ��¼�񱸷ݾ�ͷ
                int32_t stopBackupRecordRet = CAC_Bakup_Record_Dev_Manager::instance()->stop_all_device();

                // ���ֹͣ¼��ʧ��
                if((SVS_RESULT_OK != stopRtRecordRet) || (SVS_RESULT_OK != stopBackupRecordRet))
                {
                    return SVS_RESULT_FAILURE;
                }

                // ֹͣ¼��ɹ�
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
 �� �� ��  : strategy_check_thread
 ��������  : �߳�:¼����Ը��¼���߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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
 �� �� ��  : strategy_check_thread
 ��������  : �Ѵ�StoreSvr���յ�����Ϣ���зַ�����
 �������  : uint32_t nThrIndex:������Ϣ���߳�id��ʶ
             ACE_Message_Block *mb:��Ϣ����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  : ����B035�������ӶԱ���ǰ��¼������֪ͨ��Ϣ�Ĵ���
*****************************************************************************/
void CAC_Signal_Task::handle_message(uint32_t nThrIndex, ACE_Message_Block *mb)
{
    SVS_TRACE();

    //��Ϣ����ͳ��
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
        //ʵʱ¼��·����Ϣ����
        case SVS_MSG_TYPE_STORESERVER_ROUTER_INFO_REQ:
            ++msg_sort_msg_count[nThrIndex][MSG_SORT_ROUTER_INFO];

            //������Ӧ��Ϣ
            (void)pRecordManager->handle_routerinfo(mb);
            break;

        //ֹͣ·����Ϣ������������Ӧ��Ϣ
        case SVS_MSG_TYPE_STORESERVER_SCHEDULE_OVER_REQ:
            ++msg_sort_msg_count[nThrIndex][MSG_SORT_SCHEDULE_OVER_REQ];
            (void)pServer->stop_record( mb );
            break;

        //����¼����ԣ�������Ӧ��Ϣ
        case SVS_MSG_TYPE_STORESVR_RENEW_STRATEGY_REQ:
            (void)putinto_renew_strategy(mb);
            break;

        //���¾�ͷ¼�����,��������Ӧ��Ϣ
        case SVS_MSG_TYPE_CLIENTSVR_SETDEVICERECORDERPARAM_REQ:
            (void)pInfromManager->update_record_parameter(mb);
            break;

        //�澯�¼�����,��������Ӧ��Ϣ
        case SVS_ALARM_KINESCOPE_CONTROL_REQ:
            ++msg_sort_msg_count[nThrIndex][MSG_SORT_ALARM_INFO];
            (void)pInfromManager->alarm_event_req(mb);
            break;

        // ������Ϣ����,��������Ӧ��Ϣ
        case SVS_MSG_TYPE_ERROR:
            handle_error_msg(mb);
            break;

        // �����ǰ��¼�񱸷�֪ͨ��Ϣ(����Ϣ��������Ӧ��Ϣ)
        case SVS_MSG_TYPE_DISTRIBUTE_RECORDBAK_TASK_REQ:
            (void)putinto_bakup_msg(mb);
            freeMsgByHandleFunc = true; //����Ϣ�������ͷ���Ϣ�ڴ�
            break;

        // �����ǰ��¼��ط�·����Ӧ��Ϣ
        case SVS_MSG_TYPE_CLIENT_SETUP_MEDIA_STREAM_RESP:
            (void)pBakupDevManager->handle_routerinfo(mb);
            break;

        // �豸¼�������·�����
        case SVS_MSG_TYPE_DEVICE_DISTRIBUTE_REQ:
            (void)pInfromManager->handle_record_task(mb);
            break;


        case SVS_MSG_TYPE_STORESVR_DELETE_RECORD_REQ:
        case SVS_MSG_TYPE_STORESVR_QUERY_INDEX_RESP:
        case SVS_MSG_TYPE_STORESVR_QUERY_LENS_MINTIME_RESP:
            handle_del_msg(mb, freeMsgByHandleFunc);
            break;

        // ��Ӷ�¼�������ϱ���Ӧ��Ϣ�Ĵ���
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
 �� �� ��  : putinto_renew_strategy
 ��������  : ������¼����Ե���Ϣ���뵽���Ը��¶�����
 �������  : ACE_Message_Block *mb:���Ը�����Ϣ����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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
    int32_t nRet = SVS_RESULT_OK;
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
            nRet = SVS_RESULT_FAILURE;
            break;
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

        // ������Ϣ����
        SVS_STORESVR_RENEW_STRATEGY_REQ_MSG *pStrategyMsg =
            (SVS_STORESVR_RENEW_STRATEGY_REQ_MSG*)(void*)pStragegyMb->rd_ptr();
        memset(pStrategyMsg, 0, ulStragegyMbLen);
        FillCommonHeader(pStrategyMsg->Header,
                        SVS_MSG_TYPE_STORESVR_RENEW_STRATEGY_REQ,
                        0,
                        (uint16_t)ulStragegyMbLen);

        pStrategyMsg->DeviceCount = 1;
        memcpy(pStrategyMsg->DeviceStrategyInfo, pDevInfo, ulStragegyMbLen );

        // �Ѳ��Ը�����Ϣ�ֵ����Ը��¶�����
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

    // ������Ը���������Ϣ����Ϣͷ��Ϣ
    SVS_STORESVR_RENEW_STRATEGY_REQ_MSG szReq;
    memcpy(&szReq, (PSVS_STORESVR_RENEW_STRATEGY_REQ_MSG)(void*)mb->rd_ptr(), sizeof(szReq));

    // ������Ը�����Ӧ��Ϣ
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
    // ������²���ʧ�ܣ����StoreSvr���ز��Ը���ʧ����Ϣ
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
 �� �� ��  : renew_strategy_loop
 ��������  : �߳�:����¼������߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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
 �� �� ��  : proc_bakup_msg_loop
 ��������  : �߳�:����ǰ��¼�񱸷�������Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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
        // �ӱ���ǰ��¼����Ϣ������ȡ������Ϣ
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

        // ����CAC_Bakup_Record_Dev_Manager��������֪ͨ�ӿ�������������
        (void)CAC_Bakup_Record_Dev_Manager::instance()->bakup_record_notify(mb);

        // ȡ����Ϣ���ͷŴ����Ϣ�Ŀռ䣬������еĵط�����
        g_p_msg_buffer->free_mb( mb );
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING,
        "The thread for processing bakuping front record task exited."));

    return SVS_RESULT_OK;
}

/******************************************************************************
  �� �� ��  : putinto_bakup_msg
  ��������  : �ѱ���ǰ��¼��֪ͨ��Ϣ������Ϣ���еȴ�����
  �������  : const ACE_Message_Block *mb:����ǰ��¼��֪ͨ��Ϣ����
  �������  : NA
  ����ֵ    : �ɹ�����:0,ʧ�ܷ���:-1
*******************************************************************************/
int32_t CAC_Signal_Task::putinto_bakup_msg( ACE_Message_Block *mb)
{
    SVS_TRACE();
    // �����Ϸ����ж�
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "The queue of bakuping front record task enqueue faied."
            "The parameter message block is NULL."));
        return SVS_RESULT_FAILURE;
    }

    //��[����ǰ��¼��]��Ϣ���뱸�����������
    if (SVS_RESULT_FAILURE == bakup_task_queue_.enqueue_tail( mb ))
    {
        // ʧ�ܴ�ӡ��־�����е�ǰʹ�����
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

    // ��ӡ�ɹ���־
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
            bFree = true;                 // ��ɾ���߳��Լ��ͷ�
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
  �� �� ��  : proc_del_record_msg
  ��������  : �洢����������·���¼��ɾ��������
  �������  : mb:¼��ɾ��������Ϣ����
  �������  : NA
  ����ֵ    : �ɹ�����:0,ʧ�ܷ���:-1
*******************************************************************************/
void CAC_Signal_Task::proc_del_record_msg(const ACE_Message_Block *mb)
{
    SVS_TRACE();

    //�����Ϸ����ж�
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "The delete record message deal faied, "
            "The parameter message block is NULL."));

        return ;
    }

    SVS_STORESVR_DELETE_RECORD_REQ *pReqMsg =
            (SVS_STORESVR_DELETE_RECORD_REQ *)(void *)mb->rd_ptr();

    // �ж���Ϣ����
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

        //��¼�񱸷�ʱ��ִ��ɾ����ͬʱҲע�ⲻ�ܷ��ش�����Ϣ
        return ;
    }

    // �ж��¼��Ƿ�Ϸ�
    // ��ʼʱ��ͽ���ʱ��������һСʱ
    // �ұ��붼������
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

    // ��ɾ��������Ԥ�ж�,��������ļ�������ֱ�ӷ���
    // �����ļ���
    char strDateTime[TIME_STRING_LEN] = {0};
    char strTime[TIME_STRING_LEN] = {0};
    char strIndexFileName[TIME_STRING_LEN]={0};
    (void)SVS_SS_UTILITIES::time2str(strTime, sizeof(strTime), (time_t)pReqMsg->StartTime);

    (void)ACE_OS::memcpy( strDateTime, strTime, 8 );
    (void)ACE_OS::memcpy( strIndexFileName, strTime, 10 );

    // ��ȡ������·���б�����¾�����·���������ڸ�ʱ�ε����У��򷵻��������ڵ���Ϣ���洢���������
    INDEX_FILE_VECTOR indexRootPathList;
    del_manager_.get_index_root_path_list((time_t)pReqMsg->StartTime, indexRootPathList);
    bool bFindIndexFileRet = false;
    for(uint32_t uLoop = 0; uLoop < indexRootPathList.size(); uLoop++)
    {
        // ���������ļ�ȫ·������
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

    // ������е�����·����û���ҵ��������ļ�������������������Ӧ
    if (!bFindIndexFileRet)
    {

        SVS_LOG((SVS_LM_WARNING,
            "Index file is not exist, proc del record msg fail."));
        // ���������ļ������ڵ���Ӧ
        (void)del_manager_.send_record_delete_resp((char*)strLensId,
                                         (time_t)pReqMsg->StartTime,
                                         (uint64_t)0,
                                         STORESVR_DELETE_RECORD_INDEX_NOT_EXIST);
        return;
    }

    //��ɾ����Ϣ�����
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

    //��䶨ʱɾ��������Ϣ
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

    // ��Ϣ�����
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


    // ���ж����ٰ�����ѯ��ͷ���ֶ�
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

    // ���ݲ�ѯ��ͷ�������ܳ���
    usExpectedMsgLen += (uint16_t)(pMsg->DeviceCount * sizeof(SVS_LENS_INDEX_FLAG));

    // �ж���Ϣ����
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

        // Ԥ¼��Ϣ����ֹʱ�����һ��Сʱ
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

    // ���ж����ٰ�����ѯ��ͷ���ֶ�
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

    // ���ݲ�ѯ��ͷ�������ܳ���
    usExpectedMsgLen += (uint16_t)(pMsg->DeviceCount * sizeof(SVS_LENS_MINTIME));

    // �ж���Ϣ����
    if (usExpectedMsgLen != pMsg->Header.PacketLength)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The delete date dir message len is abnormal, "
            "expected[%u], actual[%u].",
            usExpectedMsgLen,
            pMsg->Header.PacketLength));
        return;
    }

    // ����·���Ƿ���ȷ
    // Ŀǰ�򵥼���Ƿ�Ϊ��,������������������Ŀ¼������Ŀ¼���Ƚ�
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
  �� �� ��  : handle_error_msg
  ��������  : ��������Ĵ�����Ϣ�������ӿ�
  �������  : const ACE_Message_Block *mb:��Ϣ����
  �������  : NA
  ����ֵ    : �ɹ�����:0,ʧ�ܷ���:-1
  �޶���¼1 :
*******************************************************************************/
void CAC_Signal_Task::handle_error_msg(ACE_Message_Block * /*mb*/) const
{
    SVS_TRACE();
    return;
}

/******************************************************************************
  �� �� ��  : update_record_thread
  ��������  : �߳�:����ʵʱ¼�������̣߳�����ˢ��������db
  �������  : NA
  �������  : NA
  ����ֵ    : �ɹ�����:0,ʧ�ܷ���:-1
  �޶���¼1 :
*******************************************************************************/
int32_t CAC_Signal_Task::update_record_thread()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "The thread for updating realtime record index start running..."));

    ACE_Message_Block *mb = NULL;

    //¼���ļ�������Ϣ�б�
    list<ACE_Message_Block *> recordIndexList;
    uint32_t recordIndexTaskCount = 0;

    //������ʱ��
    time_t lastUpdateTime = ACE_OS::time( NULL );
    CThread_Stat_Reporter reporter("UpdateRealtimeRecordIndexThread");
    while ( brunning_ )
    {
        //��ȡ����¼���ļ�������Ϣ��ʱʱ��
        ACE_Time_Value getTaskTimeout(ACE_OS::gettimeofday().sec() + SVS_GET_PROC_TASK_TIMEOUT, 0);

        //��ȡ����¼���ļ�������Ϣ
        if (SVS_RESULT_FAILURE == rindex_queue_.dequeue_head(mb, &getTaskTimeout) )
        {
            //��ȡ��Ϣ�������´���Ϣ����
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
            else //��ȡ��Ϣ��ʱ
            {
                ACE_OS::last_error(0);
                time_t waitTime = ACE_OS::time( NULL ) - lastUpdateTime;
                //��δ���ܳ�ʱʱ�䣬�����ȴ���һ����¼�ĵ���
                if ( SVS_MAX_DB_OPERATION_TIMEOUT > waitTime )
                {
                    continue;
                }
            }
            reporter.ReportStat();
            //û���κ���Ϣ��Ҫ����������һ�εȴ�
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
                //���������ϱ���Ϣ��С
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
            //��δ���ܳ�ʱʱ�䲢��δ��ָ������¼�����������ȴ���һ����¼�ĵ���
            //if ( (SVS_MAX_OPERATION_PER_COMMIT > recordIndexTaskCount)
            //    && (SVS_MAX_DB_OPERATION_TIMEOUT > waitTime))
            //{
            //    continue;
            //}
        }

        reporter.ReportStat();

        //��������ʵʱ¼�����������Ϣ
        //(void)CAC_RT_Record_Device_Manager::instance()->handle_record_index( recordIndexList );

        //�ͷ���Ϣ������
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
  �� �� ��  : debug_thread
  ��������  : �߳�:telnetԶ�̵��Է����߳�
  �������  : NA
  �������  : NA
  ����ֵ    : �ɹ�����:0,ʧ�ܷ���:-1
  �޶���¼1 :
*******************************************************************************/
int32_t CAC_Signal_Task::debug_thread()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "The thread of remote debugging start running..."));

    //�����������߳�
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
  �� �� ��  : open_debug_acceptor
  ��������  : �򿪵��Դ��������
  �������  : NA
  �������  : NA
  ����ֵ    : �ɹ�����:0,ʧ�ܷ���:-1
  �޶���¼1 :
*******************************************************************************/
int32_t CAC_Signal_Task::open_debug_acceptor()
{
    SVS_TRACE();
    // ʹ��epoll���ƣ�����epoll��Ӧ��
    try
    {
        p_debug_epoll_reactor_ = new ACE_Dev_Poll_Reactor(SVS_EPOLL_MAX_HANDLE_NUM);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to create the epoll reactor of remote debugging."));
        return SVS_RESULT_FAILURE;
    }

    // ������Ӧ��
    try
    {
        p_debug_reactor_ = new ACE_Reactor(p_debug_epoll_reactor_);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Failed to create the reactor of remote debugging."));
        return SVS_RESULT_FAILURE;
    }

    // ����Զ�̵��Լ�����
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

    // ��Զ�̵��Լ�������
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
  �� �� ��  : open_record_reactor
  ��������  : ��¼����ط�Ӧ��
  �������  : NA
  �������  : NA
  ����ֵ    : �ɹ�����:0,ʧ�ܷ���:-1
  �޶���¼1 :
*******************************************************************************/
int32_t CAC_Signal_Task::open_record_reactor()
{
    SVS_TRACE();
    // �������Ը��·�Ӧ��
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
  �� �� ��  : timer_call_back_thread
  ��������  : �߳�:��ʱ���ص������̣߳���Ҫ�������ݿ��һ�ඨʱ��
  �������  : NA
  �������  : NA
  ����ֵ    : �ɹ�����:0,ʧ�ܷ���:-1
  �޶���¼1 :
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

        //�ص�����
        pMsg = (SVS_TIMER_CALL_BACK_MSG *)(void*)mb->rd_ptr();
        switch ( pMsg->MsgType )
        {
            // ���ӱ��ݾ�ͷ�Ķ�ʱ���ص�����
            case SVS_TIMER_CALL_BACK_MSG::RECORDBAK_Dev_Manager_TYPE:
                {
                    (void)CAC_Bakup_Record_Dev_Manager::instance()->on_timer(pMsg->EventId, pMsg->TimerId,pMsg->Dummy);
                }
                break;
            default:
                break;
        }

        //�ͷ���Ϣ���ڴ�
        g_p_msg_buffer->free_mb( mb );
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "The thread of timer callback exited."));

    return SVS_RESULT_OK;
}

/******************************************************************************
  �� �� ��  : timer_call_back_second_thread
  ��������  : �߳�:��ʱ���ص������̣߳�����Ҫ�������ݿ��һ�ඨʱ��
  �������  : NA
  �������  : NA
  ����ֵ    : �ɹ�����:0,ʧ�ܷ���:-1
  �޶���¼1 :
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

        //�ص�����
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

        //�ͷ���Ϣ���ڴ�
        g_p_msg_buffer->free_mb( mb );
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "The second thread of timer callback exited."));

    return SVS_RESULT_OK;
}

/******************************************************************************
  �� �� ��  : timer_call_back_record_thread
  ��������  : �߳�:��ʱ���ص������̣߳��й�¼���״̬�����߳�
  �������  : NA
  �������  : NA
  ����ֵ    : �ɹ�����:0,ʧ�ܷ���:-1
  �޶���¼1 :
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

        //�ص�����
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

        //�ͷ���Ϣ���ڴ�
        g_p_msg_buffer->free_mb( mb );
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "The record status thread of timer callback exited."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : add_timer_callback_msg
��������  : ���ص���Ϣ���뵽��Ӧ�Ļص���Ϣ������
�������  : ACE_Message_Block* msg : �ص���Ϣ����
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::add_timer_callback_msg( ACE_Message_Block* msg )
{
    SVS_TRACE();
    //���ó�ʱʱ��,��ֹ������ʱ����
    ACE_Time_Value tv(ACE_OS::gettimeofday());
    tv.set(tv.sec() + SVS_ENQUEUE_TIMEOUT, 0);

    SVS_TIMER_CALL_BACK_MSG *pMsg = (SVS_TIMER_CALL_BACK_MSG*)(void*)msg->rd_ptr();
    ACE_Message_Queue <ACE_SYNCH> *pCallbackQueue = NULL;

    //���ݻص���Ϣ���ͣ����벻ͬ�Ļص��߳��н��д���
    switch ( pMsg->MsgType )
    {
        case SVS_TIMER_CALL_BACK_MSG::RT_RECORD_DEVICE_MANAGER_TYPE:    //�ص��߳�2����
        case SVS_TIMER_CALL_BACK_MSG::RECORD_INFORM_MANAGER_TYPE:       //�ص��߳�2����
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
                // �����ʱ���Ƚ�Ƶ����������һ�����Է��ؿ���һЩ
                ACE_Time_Value timeout(ACE_OS::gettimeofday());
                timeout.set(timeout.sec(), timeout.usec() + 100*000);
                pCallbackQueue = &timer_call_back_queue_record_;
                int32_t result = pCallbackQueue->enqueue_tail( msg, &timeout );

                //�����ʧ�ܴ���
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

                    //�ͷ���Ϣ���ڴ�
                    g_p_msg_buffer->free_mb( msg );
                    return SVS_RESULT_FAILURE;
                }

                return result;
            }

        default:
            //�Ƿ���Ϣ
            SVS_LOG((SVS_LM_ERROR, "Handle the message of timer callback failed."
                "Unsuppored message type[%d].",
                pMsg->MsgType));

            //�ͷ���Ϣ���ڴ�
            g_p_msg_buffer->free_mb( msg );
            return SVS_RESULT_FAILURE;

    }

    int32_t result = pCallbackQueue->enqueue_tail( msg, &tv );

    //�����ʧ�ܴ���
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

        //�ͷ���Ϣ���ڴ�
        g_p_msg_buffer->free_mb( msg );
        return SVS_RESULT_FAILURE;
    }

    return result;
}

/*****************************************************************************
�� �� ��  : monitor_event_proc_thread
��������  : ����¼������߳�
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
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
        //��ȡ��Ϣ
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

        //����
        procResult = handle_monitor_event( mb );
        if( 0 == procResult )
        {
            //�ͷ���Ϣ���ڴ�
            g_p_msg_buffer->free_mb( mb );
            continue;
        }

        //��û�д���ɹ��ٴδ���
        if (-1 == monitor_event_queue_.enqueue_tail(mb) )
        {
            SVS_LOG((SVS_LM_ERROR,
                "The queue of processing monitored event enqueue failed."));
            break;
        }

        // ����ʧ�ܣ��ȴ�һ��
        (void)sleep( SVS_PROC_MONITOR_EVENT_INTERVAL_TIME );
    }

    SVS_LOG((SVS_LM_WARNING, "The thread for processing monitored event exited."));

    return SVS_RESULT_OK;
}


/* �ܹ��ĸ��̣߳��ĸ�����
 *  ��һ���߳�  ��ʱ���߳�  ����ɾ��������Ϣ���� del_task_queue_ ������
 *  �ڶ����߳�  �������߳� �� del_task_queue_ ȡ��Ϣ���������� del_record_queue_
 *  ���ض�ʱ�����ڴ� del_record_queue_ ��ȡ��Ϣ��������Ϣ���ͷ��� del_record_file_queue_
 *  ���� del_prerecord_file_queue_��
 *  �������߳�  ��ʱɾ���ļ���Ϣ�ʹ�����ɾ���ļ���Ϣ�����߳�  �� del_record_file_queue_ ȡ��Ϣ
 *  ���ĸ��߳�  Ԥ¼�ļ�ɾ����Ϣ �� del_prerecord_file_queue_
 */
/*****************************************************************************
�� �� ��  : proc_del_record_task_thread
��������  : �߳�:ѭ������ɾ����������е�������Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
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

        //�ַ�������Ϣ
        del_manager_.handle_del_record(mb);

        // �ͷ���Ϣ��ռ�ڴ�
        (void)mb->release();
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING,
        "[DEL]The thread for processing the tasks of deleting record exited."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : del_record_file_loop
��������  : �߳�:ѭ������ɾ���ļ���Ϣ�����е���Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
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
        // ��ȡɾ������
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
        // �ַ�������Ϣ
        del_manager_.handle_del_record_file(mb);

        // �ͷ���Ϣ��ռ�ڴ�
        (void)mb->release();
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "[DEL]The thread for deleting record file exited."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : del_prerecord_file_loop
��������  : �߳�:ѭ������Ԥ¼�ļ�ɾ����Ϣ�����е���Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
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
        del_manager_.handle_del_prerecord_file(mb);  //�ַ�������Ϣ

        (void)mb->release();       //�ͷ���Ϣ��ռ�ڴ�
        mb = NULL;
    }

    SVS_LOG((SVS_LM_WARNING, "[DEL]The thread for processing prerecord file exited."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : del_date_dir_loop
��������  : �߳�:ɾ�����ڿ�Ŀ¼�߳��Լ������ļ�����ʱ���ص��������
�������  : NA
�������  : NA
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Signal_Task::del_date_dir_loop()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "[DEL]The thread for processing overdue date directory start running..."));

    ACE_Message_Block* mb = NULL;
    while ( brunning_ )
    {
        // ɾ������Ŀ¼��Ϣ������
        if (-1 == del_date_dir_queue_.dequeue_head(mb))
        {
            SVS_LOG((SVS_LM_ERROR, "[DEL]The queue of processing overdue date dequeue failed."));
            break;
        }

        del_manager_.handle_del_date_dir(mb);
        g_p_msg_buffer->free_mb(mb);
        mb = NULL;


        // ����ǰ��¼�񱸷ݼ�¼����Ϊ�����������ܷ�������
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
        //��ȡ����¼���ļ�������Ϣ��ʱʱ��
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        //��ȡ����¼���ļ�������Ϣ
        if (SVS_RESULT_FAILURE == m_recordStatusCollectQueue.dequeue_head(mb, &tv) )
        {
            //��ȡ��Ϣ�������´���Ϣ����
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

        // ¼��״̬�ϱ�����
        (void)SVS_Stat_Manager::instance().reportRecordStatus(mb);

        g_p_msg_buffer->free_mb(mb);
    }

    SVS_LOG((SVS_LM_WARNING, "The thread of report record status exited."));

    return SVS_RESULT_OK;
}
/*****************************************************************************
�� �� ��  : del_record_timer_callback
��������  : ����ʱ���ص���Ϣ
�������  : int32_t eventId:��ʱ���¼�ID,��ʶ��ʱ���¼�����
�������  : NA
�� �� ֵ  : NA
�޸���ʷ  :
*****************************************************************************/
void CAC_Signal_Task::del_record_timer_callback( void *pArg,
                            int32_t eventId,
                            int32_t /*timerId*/,
                            int32_t /*nDummy*/ )
{
    SVS_TRACE();

    switch ( eventId )
    {
        case SVS_DEL_TIMER_CONTROL: // �������ض�ʱ�� ��ʱ��
        {
            ((CAC_Signal_Task *)pArg)->handle_del_timer_control();

            return;
        }

        case SVS_DEL_TIMER_PRERECORD: // ɾ��Ԥ¼��ʱ��
        {
            ((CAC_Signal_Task *)pArg)->handle_del_timer_prerecord();

            return;
        }
        case SVS_DEL_TIMER_DATE_DIR: // ɾ������Ŀ¼��ʱ��
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
�� �� ��  : handle_del_timer_control
��������  : ����ɾ�����ض�ʱ�ڴ���
�������  : NA
�������  : NA
�� �� ֵ  : NA
�޸���ʷ  :
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

    //ɾ������Ϊ��ֱ�ӷ���
    if ( del_record_queue_.is_empty() )
    {
        return;
    }

    // �Ӷ��� del_record_queue_ ��ȡ��һ����Ϣ���ж�����Ϣ���ͣ�
    // ����Ƕ�ʱɾ����Ϣ���ߴ�����ɾ����Ϣ�� ��ŵ����� del_record_file_queue_ ��
    // �����Ԥ¼�ļ�ɾ����Ϣ����ŵ����� del_prerecord_file_queue_ ��
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

    // �����Ԥ¼ɾ������
    if ( DEL_RECORD_TYPE_PRERECORD == pDelRecordFile->MsgType )
    {
        nInQueueResult = del_prerecord_file_queue_.enqueue_tail(mb, &tvEnTimeOut);
    }
    else // ����ɾ������
    {
        nInQueueResult = del_record_file_queue_.enqueue_tail(mb, &tvEnTimeOut);
    }

    // ��Ϣ�����ʧ�ܣ����Խ��������·Ż�
    if ( -1 == nInQueueResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Process flow control deleting task failed:."
            "The queue enqueue failed.msgtype[%d]",
            pDelRecordFile->MsgType));

        if ( -1 == del_record_queue_.enqueue_tail(mb, &tvEnTimeOut))
        {
            // ʧ�ܺ�,��Ϣ�Żض���
            SVS_LOG((SVS_LM_ERROR,
                "[DEL]The queue of flow control deleting task enqueue failed."));
            delete mb;
            mb = NULL;
        }
    }

    return;
}

/*****************************************************************************
�� �� ��  : handle_del_timer_prerecord
��������  : ɾ��Ԥ¼�ļ���ʱ����߳�
�������  : NA
�������  : NA
�� �� ֵ  : NA
�޸���ʷ  :
*****************************************************************************/
void CAC_Signal_Task::handle_del_timer_prerecord()
{
    SVS_TRACE();

    uint32_t msgCount = del_prerecord_task_queue_.message_count();

    SVS_LOG((SVS_LM_DEBUG,"[DEL]The queue size of deleting prerecord task is [%d]", msgCount));

    //û��ɾ��Ԥ¼����
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

        //�����ȡ��ͷ����ʧ�ܣ������������ʱ��
        if (SVS_RESULT_FAILURE == tempRet)
        {
            SVS_LOG((SVS_LM_WARNING, "[DEL]Get camera[%s]'s parameter failed.",
                pDelPrerecordTask->LensId));

            lensParam.BeforeSeconds = SVS_PRORECORD_MAX_TIME;
        }

        // ���û�е�ʱ�䣬��������ɾ��Ԥ¼�������
        // �жϵ�ǰʱ���Ƿ񳬹������Ԥ¼ʱ��,���û�г���,��Ϣ�ض��м����ȴ�����
        // �����ж�����Ϊ���ԭRt��������Ԥ¼�ļ�ɾ����Ϣ,�ٵȴ�Ԥ¼ʱ����Del�Ķ���,
        // �ڵȴ������ʱ����,���Ԥ¼ʱ�䱻�Ķ���, Rt���޷�����Ϣ����Del�Ķ���
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

        // �����ʱ���ˣ����ɾ��Ԥ¼�������ɾ���������
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
�� �� ��  : handle_del_timer_date_dir
��������  : ����ɾ������Ŀ¼��ʱ����
�������  : NA
�������  : NA
�� �� ֵ  : NA
�޸���ʷ  :
*****************************************************************************/
void CAC_Signal_Task::handle_del_timer_date_dir()
{
    SVS_LOG((SVS_LM_DEBUG, "[DEL]Process the timer of deleting overdue date directory."));

    //����һ����ʱɾ�����񣬷��뵽���� del_task_queue_ ��
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

    // ��䶨ʱɾ��������Ϣ
    //SVS_DEL_RECORD *pDelRecord = (SVS_DEL_RECORD *)((void *)mb->rd_ptr());
    //pDelRecord->MsgType = DEL_RECORD_TYPE_DATE_DIR;
    //pDelRecord->DeleteTime = time(NULL);
    //pDelRecord->LensId[0] = '\0';

    //CAC_RT_Record_Server::instance()->print_msg_del_task(pDelRecord);

    // ��ʱɾ�����������
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

    //�������Ϣ�Ѿ�������Ϣ���У��ɴ������ͷš�����lint�澯
    return;  //lint !e429
}

/*****************************************************************************
�� �� ��  : debug_get_msg_info
��������  : ֧��telnet�����ã���Ϣ���������Ϣ
�������  : const uint32_t szBufferInfo:��������С
�������  : char * strBufferInfo:���Խ��������
�� �� ֵ  : NA
�޸���ʷ  :
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

        //�����������꣬�˳�ѭ��
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

            //�����������꣬�˳�ѭ��
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
        //��ȡ����¼���ļ�������Ϣ��ʱʱ��
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        //��ȡ����¼���ļ�������Ϣ
        if (SVS_RESULT_FAILURE == load_report_queue_.dequeue_head(mb, &tv) )
        {
            //��ȡ��Ϣ�������´���Ϣ����
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

        // �ϱ�����
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

    // ������Ϣ,ֻ��Ҫ������Ϣ���ͼ��ɣ�����Ϣ���ͱ������д���
    SVS_REPORT_RECORD_STATUS_TASK* pMsg = (SVS_REPORT_RECORD_STATUS_TASK*)(void*)mb->wr_ptr();
    pMsg->ReportType = 0;

    // ��Ϣ�����
    // ��������Ϣ�����ͷ
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


