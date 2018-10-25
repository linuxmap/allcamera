#include "stdafx.h"
#include "vms/vms.h"
#include "svs_mb_buffer.h"
#include "svs_timer.h"
#include "svs_logging_strategy.h"
#include "svs_daemon.h"
#include "svs_auto_reference_ptr.h"
#include "svs_access_control.h"
#include "svs_access_control_task.h"
#include "svs_daemon_thread.h"
#include "svs_business_manager.h"


CAccessControlTask::CAccessControlTask()
{

    brunning_          = false;
    _thr_index         = 0;
    p_timer_reactor_   = NULL;
    remote_debug_port_ = 0;
    p_debug_reactor_   = NULL;
    p_debug_acceptor_  = NULL;
}

CAccessControlTask::~CAccessControlTask()
{


}

int32_t CAccessControlTask::open(void *pArg)
{
    SVS_TRACE();


    try
    {
        p_timer_reactor_ = new ACE_Reactor;
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to allocate AceReactor object."));
        p_timer_reactor_ = NULL;

        return SVS_ERROR_FAIL;
    }

    if (!CAC_Timer_Manager::instance()->init(p_timer_reactor_))
    {
        SVS_LOG((SVS_LM_ERROR, "Timer manager initial error."));
        return SVS_ERROR_FAIL;
    }

    if (0 != open_debug_acceptor())
    {
        return SVS_ERROR_FAIL;
    }

    brunning_ = true;

    // Activate threads.
    size_t *stack_size = NULL;
    try
    {
        stack_size = new size_t[ACCESS_TASK_THREAD_MAX];
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Create stack size for thread error."));

        delete []stack_size;
        stack_size = NULL;

        return SVS_ERROR_FAIL;
    }

    for ( int32_t i = 0; i < ACCESS_TASK_THREAD_MAX; i++)
    {
        stack_size[i] = SVS_THREAD_STATCK_MAX;
    }

    int32_t ret = activate(
                            THR_NEW_LWP ,
                            ACCESS_TASK_THREAD_MAX,
                            0,
                            ACE_DEFAULT_THREAD_PRIORITY,
                            SVS_ERROR_FAIL,
                            0,
                            0,
                            0,
                            stack_size,
                            0);

    delete []stack_size;
    stack_size = NULL;

    return 0;
}



int32_t CAccessControlTask::svc()
{
    SVS_TRACE();

    (void)signal(SIGPIPE, SIG_IGN);

    ACE_hthread_t currThreadID = ACE_OS::thr_self();

    switch (get_index())
    {

        case ACCESS_TASK_THREAD_TIMER:
        {
            SVS_LOG((SVS_LM_INFO, "TimerEventLoop thread is running."));
            return timer_event_loop();
        }

        case ACCESS_TASK_THREAD_DEBUG:
        {
            SVS_LOG((SVS_LM_INFO, "DebugThread thread is running."));
            return debug_thread();
        }

        default:
        {
            break;
        }
    }

    return 0;
}


int32_t CAccessControlTask::close(u_long)
{
    SVS_TRACE();

    (void)ACE_Task_Base::wait();
    return 0;
}

int32_t CAccessControlTask::stop()
{
    SVS_TRACE();

    brunning_ = false;
    return 0;
}


int32_t CAccessControlTask::open_debug_acceptor( void )
{
    SVS_TRACE();

    try
    {
        p_debug_reactor_ = new ACE_Reactor;
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to allocate AceReactor object."));

        delete p_debug_reactor_;
        p_debug_reactor_ = NULL;

        return SVS_ERROR_FAIL;
    }

    uint32_t ulAddr = ACE_OS::inet_addr("127.0.0.1");
    remote_debug_port_ = CAccessControlSvr::instance().getDebugListenPort();
    ACE_INET_Addr port_to_listen(remote_debug_port_, ACE_NTOHL( ulAddr ));
    try
    {
        p_debug_acceptor_ = new SVS_LOG_ACCEPTOR;
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to allocate pDebugAcceptor object."));

        delete p_debug_acceptor_;
        p_debug_acceptor_ = NULL;

        return SVS_ERROR_FAIL;
    }

    int32_t openResult = p_debug_acceptor_->open(port_to_listen, p_debug_reactor_);
    if ( SVS_ERROR_FAIL == openResult )
    {
        SVS_LOG((
            SVS_LM_ERROR,
            "Open debugging port error. "));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_DEBUG, "Open debugging port ok."));
    return 0;
}

int32_t CAccessControlTask::get_index()
{
    SVS_TRACE();

    int32_t old;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, SVS_ERROR_FAIL);
    old = _thr_index++;

    return old;
}

int32_t CAccessControlTask::timer_event_loop()
{
    SVS_TRACE();

    char threadName[MAX_THREAD_NAME] = {0};
    ACE_hthread_t currThreadID = 0;
    currThreadID = (ACE_hthread_t)ACE_OS::thr_self();
    (void)snprintf( threadName,sizeof(threadName),
                    "TimerEventLoopThread[%lu]",currThreadID);

    CThread_Stat_Reporter reporter(threadName);
    int32_t nInternal = REPORT_THREAD_STATUS_INTERVAL;
    ACE_Time_Value tvDelay(0);

    if (NULL == p_timer_reactor_)
    {
        SVS_LOG((SVS_LM_ERROR, "Invalid parameter. pTimerReactor is NULL."));
        return SVS_ERROR_FAIL;
    }

    (void)p_timer_reactor_->owner(ACE_OS::thr_self());

    int32_t ret = 0;
    while (brunning_)
    {

        tvDelay.sec(nInternal);
        ret = p_timer_reactor_->handle_events(tvDelay);
        if (ret < 0)
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,"TimerEventLoop thread occur error."));
            }
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();

    }

    return 0;
}

int32_t CAccessControlTask::debug_thread()
{
    SVS_TRACE();

    char threadName[MAX_THREAD_NAME] = {0};
    ACE_hthread_t currThreadID = 0;
    currThreadID = (ACE_hthread_t)ACE_OS::thr_self();
    (void)snprintf( threadName,sizeof(threadName),
                  "DebugThread[%lu]",currThreadID);

    CThread_Stat_Reporter reporter(threadName);
    int32_t nInternal = REPORT_THREAD_STATUS_INTERVAL;
    ACE_Time_Value tvDelay(0);

    if (NULL == p_debug_reactor_)
    {
        SVS_LOG((SVS_LM_INFO, "DebugThread exit."));
        return SVS_ERROR_FAIL;
    }

    (void)p_debug_reactor_->owner(ACE_OS::thr_self());
    int32_t ret = 0;
    while ( brunning_ )
    {
        tvDelay.sec(nInternal);
        ret = p_debug_reactor_->handle_events(tvDelay);
        if (ret < 0)
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,"DebugThread thread occur error."));
            }
            reporter.ReportStat(0);
            continue;
        }
        reporter.ReportStat();
    }

    return ret;
}



