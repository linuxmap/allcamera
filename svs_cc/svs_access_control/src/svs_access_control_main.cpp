#include "stdafx.h"
#include "svs_print_version.h"
#include "svs_timer.h"
#include "svs_print_version.h"
#include "svs_logging_strategy.h"
#include "svs_log_msg.h"
#include "svs_daemon.h"
#include "svs_access_control.h"
#include "svs_daemon_thread.h"
#include "svs_server_manager.h"
#include "svs_business_manager.h"
#include "vms/vms.h"
#include "svs_device_stack.h"
#include "svs_access_control_manager.h"
#include "svs_server_stack.h"
#include "svs_control_stack.h"

static void ignore_signal();

time_t g_program_start_time;
CSVS_Logging_Strategy g_svrLogStrategy;


static const char* const strDescription = "svs_access_control";


//static int32_t exit_on_starting()
static void exit_on_starting()
{
    SVS_TRACE();

    static bool bSignal = false;
    static ACE_Recursive_Thread_Mutex mutex;
    {
        ACE_GUARD(ACE_Recursive_Thread_Mutex, pObj, mutex);
        if (bSignal)
        {
            return;
        }
        bSignal = true;
    }

    (void)g_svrLogStrategy.endLogStrategy();
    (void)ACE_Thread_Manager::instance()->wait();
}


void server_run(void)
{
    SVS_TRACE();

    (void)SVS_SS_UTILITIES::backup_log(LOG_FILE);
    if (0 != g_svrLogStrategy.initLogStrategy(CONF_FILE, LOG_FILE, true))
    {
        send_sigquit_to_deamon();
        return ;
    }

    ignore_signal();

    /*if (NULL == SVS_AC_CTRLSVR::instance()
        || NULL == CAC_Timer_Manager::instance())
    {

        (void)g_svrLogStrategy.endLogStrategy();
        send_sigquit_to_deamon();
        return;
    }
    */

    if (!CAccessControlSvr::instance().init_configure())
    {
         (void)g_svrLogStrategy.endLogStrategy();
        send_sigquit_to_deamon();
        return;
    }

    uint32_t ulCheckInterval =  DEFAULT_INTERVAL;// SVS_SCHEDULER::instance()->get_check_thread_status_interval();
    uint32_t ulRestartFlag   =  0;//SVS_SCHEDULER::instance()->get_restart_server_flag();
    uint32_t ulDumpFlag      =  0;//SVS_SCHEDULER::instance()->get_dump_server_flag();
    int32_t initResult = CSVS_Daemon_Thread::instance()->Init(ulCheckInterval, ulRestartFlag, ulDumpFlag);
    if (0 != initResult)
    {
        send_sigquit_to_deamon();
        return;
    }

    if(SVS_ERROR_OK !=CAccessControlTask::instance().open(NULL))
    {
        (void)g_svrLogStrategy.endLogStrategy();
        send_sigquit_to_deamon();
        return;
    }

    if( (0 != IAccessControlManager::instance().initialize())
        ||(0 != IDeviceStackMgr::instance().initialize())
        ||(0 != IServerStack::instance().initialize())
        ||(0 != CServerManager::instance().initialize())
        ||(0!= CBusinessManager::instance().initialize()))
    {
        (void)g_svrLogStrategy.endLogStrategy();
        send_sigquit_to_deamon();
        return;
    }



    if(SVS_ERROR_OK !=CAccessControlStack::instance().open(NULL))
    {
        (void)g_svrLogStrategy.endLogStrategy();
        send_sigquit_to_deamon();
        return;
    }

    if((0 != CServerManager::instance().start())
        ||(0 != CBusinessManager::instance().start()))
    {
        (void)g_svrLogStrategy.endLogStrategy();
        send_sigquit_to_deamon();
        (void)ACE_Thread_Manager::instance()->wait();
        return;
    }

    (void)ACE_Thread_Manager::instance()->wait();

    (void)g_svrLogStrategy.endLogStrategy();

    send_sigquit_to_deamon();
    return;
}


int32_t main(int32_t argc, char *argv[])
{
    (void)time(&g_program_start_time);
    if (argc > 1)
    {
        print_server_info(argc, argv, strDescription);
    }

    int32_t runType = 0;
    int32_t logEnable = 0;
    int32_t logLM = 0;
    if (0 != getDebugConfig(CONF_FILE, runType, logEnable, logLM))
    {
        return SVS_ERROR_FAIL;
    }

    SVS_LOG_LM(logLM);
    if (logEnable)
    {
        SVS_LOG_ENABLE();
    }
    else
    {
        SVS_LOG_DISABLE();
    }

    if (FRONT_RUN != runType)
    {
        runType = DAEMON_RUN;
    }

    svs_run_service(
        server_run,
        runType,
        exit_on_starting,
        CONF_FILE,
        SERVER_TYPE_ACCESS_CONTROL);
    return 0;
}

static void ignore_signal()
{
    SVS_TRACE();

    if (SIG_ERR == signal(SIGPIPE, SIG_IGN))
    {
        exit(SVS_ERROR_FAIL);
    }
}

