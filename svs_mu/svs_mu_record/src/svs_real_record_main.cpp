
#include "vms/vms.h"
#include "svs_config.h"
#include "svs_timer.h"
#include "svs_print_version.h"
#include "svs_utility.h"
#include "svs_logging_strategy.h"
#include "svs_log_msg.h"
#include "svs_daemon.h"


#include "svs_stat_manager.h"
#include "svs_real_record_task.h"
#include "svs_recv_stream_factory.h"
#include "svs_record_index.h"
#include "svs_real_record_lens.h"
#include "svs_record_inform.h"
#include "svs_real_record_server.h"
#include "svs_recv_rt_media_buffer.h"
#include "svs_bakup_front_record_lens.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"
#include "svs_mb_buffer.h"
#include "svs_media_recv_manager.h"
#include "svs_daemon_thread.h"
#include "svs_console_server.h"

#ifdef UNITTEST
#include <cppunit/SourceLine.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/XmlOutputter.h>
#endif

const char *g_strServerName = "svs_mu_record";

time_t g_ulStartTime;

// �澯�̶���Ϣ������
int32_t g_alarmStatus[ALARM_INDEX_NUM] = {ALARM_STATUS_RESUMED,};

CSVS_Logging_Strategy g_svrLogStrategy;

/*****************************************************************************
 �� �� ��  : time2string
 ��������  : ��ʱ������ת����ʱ�䴮����:2009-04-08 10:02:00
 �������  : time_t srcTime:��1970��1��1����ʱ��������������������
             int32_t nbuflen:����������ĳ���
 �������  : char *pDestBuf:���ת�����ʱ�䴮
 �� �� ֵ  : ת�����ʱ�䴮
 �޸���ʷ  :
*****************************************************************************/
char *time2string(char *pDestBuf, int32_t nbuflen, time_t srcTime )
{
    SVS_TRACE();
    // ����У��
    if ( 0 >= nbuflen )
    {
        return NULL;
    }
    struct tm tmv;

    (void)ACE_OS::localtime_r( &srcTime, &tmv );
    (void)ACE_OS::snprintf(pDestBuf, (uint32_t)nbuflen,
        "%04d-%02d-%02d %02d:%02d:%02d",
        tmv.tm_year + 1900,
        tmv.tm_mon + 1,
        tmv.tm_mday,
        tmv.tm_hour,
        tmv.tm_min,
        tmv.tm_sec);

    return pDestBuf;
}


/*****************************************************************************
 �� �� ��  : get_all_instance
 ��������  : ��ȡϵͳ����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t get_all_instance()
{
    SVS_TRACE();
    // ��ȡ��̨�ػ��̵߳ĵ�ʵ��
    if (NULL == CSVS_Daemon_Thread::instance())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Create the sigleton of the class [CSVS_Daemon_Thread] failed."));

        return SVS_RESULT_FAILURE;
    }

    //У�鵥�崴���Ƿ�ɹ�
    if (NULL == CAC_RT_Record_Server::instance())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Create the sigleton of the class [CAC_RT_Record_Server] failed."));

        return SVS_RESULT_FAILURE;
    }

    //У�鶨ʱ���������Ƿ�ɹ�
    if (NULL == CAC_Timer_Manager::instance())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Create the sigleton of the class [CAC_Timer_Manager] failed.."));

        return SVS_RESULT_FAILURE;
    }

    // ��ȡ¼��֪ͨ�����൥��
    if (NULL == CAC_Record_Inform_Manager::instance())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Create the sigleton of the class [CAC_Record_Inform_Manager] failed."));

        return SVS_RESULT_FAILURE;
    }

    // ��ȡʵʱ¼���豸�����൥��
    if (NULL == CAC_RT_Record_Device_Manager::instance())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Create the sigleton of the class [CAC_RT_Record_Device_Manager] failed."));

        return SVS_RESULT_FAILURE;
    }

    // ��ȡ��Ϣ�����������
    if ( NULL == CSVS_Mb_Buffer_Manager::instance() )
    {
        SVS_LOG( (SVS_LM_CRITICAL, "Create the sigleton of the class [CSVS_Mb_Buffer_Manager] failed.") );

        return SVS_RESULT_FAILURE;
    }

    // ��ȡ����¼����
    if (NULL == CAC_Bakup_Record_Dev_Manager::instance())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Create the sigleton of the class [CAC_Bakup_Record_Dev_Manager] failed."));

        return SVS_RESULT_FAILURE;
    }

    // ��ȡ����̨������
    if (NULL == SVS_Console_Server::instance())
    {
        SVS_LOG((SVS_LM_CRITICAL, "Create the sigleton of the class [SVS_Console_Server] failed."));

        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

//=========================main() BEGIN=================================

/*****************************************************************************
 �� �� ��  : sig_chld
 ��������  : �ӽ�̴���
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void sig_chld(int32_t /* signo */)
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO, "Enter the function sig_chld()..."));

    int32_t statChild;
    pid_t pid = waitpid(-1, &statChild, WNOHANG);

    while ( 0 < pid )
    {
        pid = waitpid(-1, &statChild, WNOHANG);
    }

    SVS_LOG((SVS_LM_INFO, "Exit the function sig_chld()..."));
}

/*****************************************************************************
 �� �� ��  : server_exit
 ��������  : ���������˳�ʱ���ͷ���Դ
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void server_exit( )
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "Enter the function server_exit()..."));
    static bool bSignal = false;
    static ACE_Thread_Mutex mutex;
    {
        ACE_GUARD(ACE_Thread_Mutex, pObj, mutex );
        //�����˳�����,ֱ�ӷ���
        if (bSignal)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Exit the function server_exit().The function server_exit() is running now."));
            return;
        }
        bSignal = true;
    }

    //�ر�·��
    if (NULL != CAC_RT_Record_Device_Manager::instance())
    {
        CAC_RT_Record_Device_Manager::instance()->shutdown_router();
    }

    // ֹͣ���о�ͷ��ǰ��¼�񱸷�����
    (void)CAC_Bakup_Record_Dev_Manager::instance()->stop_all_device();

    (void)ACE_OS::sleep( SERVER_EXIT_WAIT_TIME );

    CAC_RT_Record_Server::instance()->end_reactor_event_loop();

    //�ر���־�߳�
    (void)g_svrLogStrategy.endLogStrategy();

    // �ر��ػ��߳�
    CSVS_Daemon_Thread::instance()->Destroy();

    (void)SVS_Disk_Manager::instance().close(0);

    (void)SVS_Media_Recv_Manager::instance().close(0);

    (void)SVS_Stat_Manager::instance().close(0);

    (void)ACE_Thread_Manager::instance()->wait();

    SVS_LOG((SVS_LM_WARNING, "Exit the function server_exit() normally."));
}

/*****************************************************************************
 �� �� ��  : server_run
 ��������  : ������������߳�������ǰ����Դ������
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void server_run(void)
{
    SVS_TRACE();
    //������һ��������־
    (void)SVS_SS_UTILITIES::backup_log(LOG_FILE);
    //��ʼ����־����
    int32_t initLogStrategyResult =
        g_svrLogStrategy.initLogStrategy( CONF_FILE, LOG_FILE, true );
    if ( SVS_RESULT_OK != initLogStrategyResult )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Initialize log strategy failed."));
        send_sigquit_to_deamon();
        return;
    }

    SVS_LOG((SVS_LM_INFO, "Initialize log strategy succeeded."));

    SVS_LOG((SVS_LM_CRITICAL,
        "\n\n%s  version: %s compile time: %s %s \n\n",
        g_strServerName,
        SVR_VERSION_INFO,
        __DATE__,
        __TIME__));

    //��ʼ�����е������
    if(SVS_RESULT_OK != get_all_instance())
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get the sigletons of classes failed."));
        send_sigquit_to_deamon();
        return ;
    }

    // ��ʼ��RT����
    int32_t initResult = CAC_RT_Record_Server::instance()->init( CONF_FILE );
    if ( SVS_RESULT_OK != initResult )
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Failed to initialize the sigleton of the class [CAC_RT_Record_Server]."));
        send_sigquit_to_deamon();
        return ;
    }

    initResult = CSVS_Mb_Buffer_Manager::instance()->init();
    if ( SVS_RESULT_OK != initResult )
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Failed to initialize the sigleton of the class [CSVS_Mb_Buffer_Manager]."));
        send_sigquit_to_deamon();

        return;
    }

    initResult = SVS_Console_Server::instance()->initConfig(CONF_FILE);
    if (SVS_RESULT_OK != initResult)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Failed to initialize the sigleton of the class [SVS_Console_Server]."));
        send_sigquit_to_deamon();

        return;
    }

    // ��ʼ����̨�ػ��߳�
    uint32_t ulCheckInterval = CAC_RT_Record_Server::instance()->get_check_thread_status_interval();
    uint32_t ulRestartFlag   = CAC_RT_Record_Server::instance()->get_restart_server_flag();
    uint32_t ulDumpFlag      = CAC_RT_Record_Server::instance()->get_dump_server_flag();
    initResult = CSVS_Daemon_Thread::instance()->Init(ulCheckInterval, ulRestartFlag, ulDumpFlag);
    if (0 != initResult)
    {
        SVS_LOG((SVS_LM_CRITICAL,
                   "Failed to initialize daemon thread."));

        send_sigquit_to_deamon();
        return;
    }

    // ��ʼ����ݿ����ӳ�

    //��������: �������Ա�Ӷ���CAC_Signal_Task signal_task_��open()����, ���������svc�߳�
    bool openServiceResult = CAC_RT_Record_Server::instance()->open_service();
    if ( !openServiceResult )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Start the recording service threads failed."));

        send_sigquit_to_deamon();
        return;
    }

    SVS_LOG((SVS_LM_INFO, "Start the recording service threads succeeded."));


    SVS_LOG((SVS_LM_INFO, "The services of RtRecordSvr are all ready ..."));

    //�ȴ�������
    (void)ACE_Thread_Manager::instance()->wait();

    SVS_LOG((SVS_LM_WARNING, "The services of RtRecordSvr have stoped."));

    send_sigquit_to_deamon();
    return;
}

// �Զ���Ԫ����
#ifdef UNITTEST
int32_t unitTestMain(int32_t argc, char *argv[])
{
    SVS_LOG_LM(1);

    const char *testClass[] = {"Main_Test"};
    int32_t testNum = sizeof(testClass) / sizeof(char *);

    int32_t i = 0;
    do
    {
        if(1 == argc)
        {
            break;
        }
        if(2 == argc)
        {
            printf("Usage:%s class name to be test\n", argv[0]);
            printf("Example:%s CAC_Realtime_Record_Device\n", argv[0]);
            printf("Supported test class:\n");

            for (i = 0; i < testNum; i++)
            {
                printf("%d.\t%s\n", i + 1, testClass[i]);
            }
            return 0;
        }

        for(i = 0; i < testNum; ++i)
        {
            if (0 == strcmp(argv[1], testClass[i]))
            {
                break;
            }
        }
        if(i >= testNum)
        {
            printf("No test class named %s\n", argv[1]);
            printf("Supported test class:\n");

            for (i = 0; i < testNum; i++)
            {
                printf("%d.\t%s\n", i + 1, testClass[i]);
            }
            return 0;
        }
    }while(0);

    CppUnit::TextUi::TestRunner runner;

    // ���TestFactoryRegister ��ʵ��
    CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry();

    // �� registry �л�ȡһ���µ� TestSuite ��ʵ��
    runner.addTest(registry.makeTest());

    // Change the default outputter to a compiler error format outputter
    CPPUNIT_NS::OFileStream outStream("ut_svs_mru_record.xml");
    runner.setOutputter( new CPPUNIT_NS::XmlOutputter(&runner.result(), outStream, "UTF-8"));

    // �������еĲ�������
    if(1 == argc)
    {
        printf("run all test.\n");
        runner.run();
    }
    else
    {
        runner.run(argv[1]);
    }

    return 0;
}
#endif

int32_t main(int32_t argc, char *argv[])
{
#ifdef UNITTEST
    return unitTestMain(argc, argv);
#endif
    (void)time(&g_ulStartTime);
    if (1 < argc)
    {
        if ('-' != argv[1][0])
        {
            printf( "USAGE: SVS_RecordSvr --- start service\n\t"
                    "SVS_RecordSvr -v|V  --- print version infomation\n");
            return SVS_RESULT_OK;
        }

        switch (argv[1][1])
        {
            case 'v':
            case 'V':
                print_server_info(argc, argv, g_strServerName );

                return SVS_RESULT_OK;

            default:
                break;
        }
    }

    //��ȡ�����ļ��еĵ�������������
    int32_t runType;
    int32_t logEnable;
    int32_t logLM;
    int32_t getConfigResult = getDebugConfig(
                            CONF_FILE,
                            runType,
                            logEnable,
                            logLM );
    if ( SVS_RESULT_OK != getConfigResult )
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Read the debug information from the configuration file failed."
            "configuration file name[%s].",
            CONF_FILE));

        return SVS_RESULT_FAILURE;
    }

    //������־�ȼ�
    SVS_LOG_LM( logLM );

    //�򿪣��ر���־����
    if (logEnable)
    {
        SVS_LOG_ENABLE();
    }
    else
    {
        SVS_LOG_DISABLE();
    }

    //��������
    svs_run_service(    server_run,
                        runType,
                        server_exit,
                        CONF_FILE,
                        SVS_PLATFORM_SERVER_TYPE_STORAGE_RECORD );
    return SVS_RESULT_OK;
}
//=============================main() END==================================

