#include "svs_ace_header.h"
#include "ace/OS_NS_stdio.h"
#include "ace/OS_NS_string.h"
#include "ace/OS_NS_unistd.h"
#include "ace/OS_NS_sys_stat.h"
#include "ace/Auto_Ptr.h"
#include "ace/OS_NS_time.h"
//#include "ace/Service_Config.h"
#include "ace/Thread_Manager.h"

//#include "ace/Logging_Strategy.h"
#include "ace/ACE.h"
//#include "ace/Get_Opt.h"

#include "ace/streams.h"

#include "ace/Lib_Find.h"
#include "ace/Log_Msg.h"
#include "ace/Reactor.h"
//#include "ace/OS_NS_string.h"
//#include "ace/OS_NS_stdio.h"
//#include "ace/OS_NS_unistd.h"

#if defined (ACE_AS_STATIC_LIBS)    \
    ||(!defined (ACE_WIN32) && !defined (ACE_HAS_SVR4_DYNAMIC_LINKING)    \
            &&!defined (__hpux))
//#include "ace/Logging_Strategy.h"
#endif

//#include "ace/Auto_Ptr.cpp"
//#include "ace/Get_Opt.h"

//#include "ace/Logging_Strategy.h"
//#include "ace/Get_Opt.h"
//#include "ace/Dynamic_Service.h"

#include "svs_logging_strategy.h"
#include "svs_config.h"
#include "svs_log_msg.h"
#include "svs_utility.h"

CLogging_Cfg_Param::CLogging_Cfg_Param()
{
    //日志文件，全路径
    (void)ACE_OS::strcpy(m_strLogFile, "/var/log/SVS/NoSetLogFile.log");

    //文件大小检测时间(Secs)
    m_iChkInterval = 600;

    //每个日志文件最大大小(KB), 默认10M
    m_iLogFileMaxSize = 10240;

    //日志文件最大个数
    m_iLogFileMaxCnt = 10;

    //是否向终端发送
    m_bSendTerminal = 0;
}

CLogging_Cfg_Param::~CLogging_Cfg_Param()
{}

//日志策略
CSVS_Logging_Strategy::CSVS_Logging_Strategy()
{
    running_ = true;
    log_check_interval_time_ = LOG_CHECK_DEFAULT_INTERVAL_TIME;
    run_check_thread_flag_ = false; //默认不启动日志检测线程

    pLogStraReactor = NULL;
    pLogStrategy = NULL;

    (void)memset(config_file_,0,sizeof(config_file_));
}

CSVS_Logging_Strategy::~CSVS_Logging_Strategy()
{
    if (pLogStraReactor != NULL)
    {
        try
        {
            delete pLogStraReactor;
            pLogStraReactor =  NULL;
        }
        catch(...)
        {

pLogStraReactor =  NULL;
        }

    }

    if (pLogStrategy != NULL)
    {
        try
        {
            pLogStrategy->reactor(NULL);
            delete pLogStrategy;
            pLogStrategy = NULL;
        }
        catch(...)
        {
            pLogStrategy = NULL;
        }
    }
}

typedef struct _CfgArgvList
{
    char *option;
    char  value[256];
} CfgArgvList;

#define LOG_CFG_ARGV_COUNT 6

static CfgArgvList gCfgArgvList[LOG_CFG_ARGV_COUNT] =
{
    {(char*)"-f", "OSTREAM"                      },
    {(char*)"-s", "/var/log/SVS/NoSetLogFile.log"},
    {(char*)"-i", "600"                          },
    {(char*)"-m", "10240"                        },
    {(char*)"-N", "2"                            },
    {(char*)"-o", ""                             },
};

//static void *run_reactor (void *pReactor)
//{
//    ACE_hthread_t currThreadID = ACE_OS::thr_self();
//    ACE_UNUSED_ARG(currThreadID);
//
//    ACE_Reactor *pLogReactor = (ACE_Reactor *)pReactor;
//
//    pLogReactor->owner(ACE_Thread_Manager::instance ()->thr_self ());
//
//    SVS_LOG((SVS_LM_DEBUG, "log file check thread start..."));
//    while (pLogReactor->reactor_event_loop_done() == 0)
//    {
//        pLogReactor->run_reactor_event_loop ();
//    }
//
//    return 0;
//}


//动态检测日志参数线程
static void *run_check_log_config_thread( void *pParam )
{
    ACE_hthread_t currThreadID = (ACE_hthread_t)ACE_OS::thr_self();
   // ACE_UNUSED_ARG(currThreadID);

    SVS_LOG((SVS_LM_DEBUG, "Log debug params dynamic check thread start, threadid[%u].", currThreadID));

    CSVS_Logging_Strategy *pStrateqy = (CSVS_Logging_Strategy*)pParam;
    int32_t log_check_interval_time = pStrateqy->log_check_interval_time();
    char *pConfigFile = pStrateqy->config_file();

    int32_t nDefaultLogLM = SVS_LM_WARNING;//默认告警级别

    int32_t nLastLogLM = SVS_LM_WARNING;//上次一次检查的时候告警的级别

//    int32_t checkLogFileExistsTime = 0;
    while (pStrateqy->running())
    {
        int32_t runType   = 0;
        int32_t logEnable = 0;
        int32_t logLM = 0;

            //取时间戳
        char day_and_time[35] = {0};
        ACE_Time_Value cur_time = ACE_OS::gettimeofday ();
        time_t secs = ACE_OS::time(NULL);
        struct tm tmv;
        (void)ACE_OS::localtime_r( &secs, &tmv );
         (void)ACE_OS::snprintf (day_and_time,
          sizeof( day_and_time ),
          "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
          tmv.tm_year + 1900,
          tmv.tm_mon + 1,
          tmv.tm_mday,
          tmv.tm_hour,
          tmv.tm_min,
          tmv.tm_sec,
          cur_time.usec());
        static uint8_t ucDay = (uint8_t)(tmv.tm_mday);
        if(ucDay != tmv.tm_mday)
        {
            SVS_LOG((SVS_LM_CRITICAL,
                "======DATE CHANGE[%s],ProcessID[%u]======",
                day_and_time,
                ACE_OS::getpid()));

            ucDay = (uint8_t)(tmv.tm_mday);
        }

        if (0 != getDebugConfig(pConfigFile, runType, logEnable, logLM))
        {
            SVS_LOG((SVS_LM_ERROR, "Reload log debug params  fail, configfile[%s].", pConfigFile));
        }
        else
        {
            SVS_LOG((SVS_LM_INFO, "Reload log debug params success, configfile[%s] LogLevel[%d] LogEnable[%d].", pConfigFile, logLM, logEnable));

            if(nLastLogLM == logLM)
            {
                nLastLogLM = logLM;
                if(logLM < nDefaultLogLM)
                {
                    logLM = nDefaultLogLM;
                }
            }
            else
            {
                nLastLogLM = logLM;
            }

            //设置日志等级
            SVS_LOG_LM( logLM );

            //打开／关闭日志开关
            if (logEnable)
            {
                SVS_LOG_ENABLE();
            }
            else
            {
                SVS_LOG_DISABLE();
            }
        }

        CAC_Config config(pConfigFile);
        ACE_TCHAR strTemp[32] = {0};
        int32_t logHexDump = 1;

        if (config.get("SVS_DEBUG", "HexDump", strTemp ) == 0)
        {
            logHexDump = atoi ( strTemp ) ;
        }

        if ((0 != logHexDump) && (1 != logHexDump))
        {
            logHexDump = 1;
        }

        memset(strTemp,0,32);
        if (config.get("SVS_DEBUG", "DefaultLogLM", strTemp ) == 0)
        {
            nDefaultLogLM = atoi ( strTemp ) ;
        }

        if ((SVS_LM_TRACE > logHexDump) || (SVS_LM_CRITICAL < logHexDump))
        {
            nDefaultLogLM = SVS_LM_WARNING;
        }

        SVS_HEXDUMP_ENABLE(logHexDump);


        //等待
        for (int32_t i = 0; i < log_check_interval_time; i++)
        {
            if (!pStrateqy->running())
            {
                break;
            }

//            if( 0 == ( checkLogFileExistsTime % CHECK_LOG_FILE_EXISTS_INTERVAL_TIME ) )
//            {
//                pStrateqy->check_log_file();
//            }
//            checkLogFileExistsTime++;

            (void)ACE_OS::sleep(1);
        }
    }
    SVS_LOG((SVS_LM_WARNING, "Log debug params dynamic check thread end, threadid[%u]", ACE_Thread::self ()));

    return 0;
}


//初始化日志策略
int32_t CSVS_Logging_Strategy::initLogStrategy(const CLogging_Cfg_Param &cfgParam)
{
    //Set Arg List
    (void)ACE_OS::strncpy(gCfgArgvList[1].value, cfgParam.m_strLogFile, 256);
    (void)ACE_OS::sprintf(gCfgArgvList[2].value, "%d", cfgParam.m_iChkInterval);
    (void)ACE_OS::sprintf(gCfgArgvList[3].value, "%d", cfgParam.m_iLogFileMaxSize);
    (void)ACE_OS::sprintf(gCfgArgvList[4].value, "%d", cfgParam.m_iLogFileMaxCnt);
    int32_t iCfgParamCnt = LOG_CFG_ARGV_COUNT;

    if (cfgParam.m_bSendTerminal)
    {
        (void)ACE_OS::strcpy(gCfgArgvList[0].value, "STDERR");
        iCfgParamCnt = 1;
    }

   // ACE_NEW_RETURN(pLogStraReactor, ACE_Reactor, -1);
    try
    {
        pLogStraReactor = new ACE_Reactor;
    }
    catch(...)
    {
        pLogStraReactor = NULL;
        return -1;
    }
    //ACE_NEW_RETURN(pLogStrategy, ACE_Logging_Strategy, -1);
    // ACE_NEW_RETURN(pLogStrategy, SVS_Logging_Strategy, -1);
    try
    {
        pLogStrategy = new SVS_Logging_Strategy;
    }
    catch(...)
    {
        pLogStrategy = NULL;
        return -1;
    }

    //Set Reactor
    pLogStrategy->reactor(pLogStraReactor);

    //char ls_argc = LOG_CFG_ARGV_COUNT * 2;
    uint32_t ls_argc = LOG_CFG_ARGV_COUNT * 2;
    //ACE_Auto_Basic_Ptr <ACE_TCHAR *> ls_argv (new ACE_TCHAR *[ls_argc]);
    // ***END*** 2008-7-26  代码review  liangzhen Deleted
    ACE_Auto_Basic_Array_Ptr <ACE_TCHAR *> ls_argv (new ACE_TCHAR *[ls_argc]);


    int32_t of = 0, i;

    for (i = 0; i < iCfgParamCnt; i++)
    {
        (ls_argv.get ())[of++] = gCfgArgvList[i].option;

        //ls_argv[of++] = gCfgArgvList[i].option;

        if (gCfgArgvList[i].value[0] != '\0')
        {
            (ls_argv.get ())[of++] = gCfgArgvList[i].value;

            //ls_argv[of++] = gCfgArgvList[i].value;
        }
    }

    ls_argv.get()[of] = NULL;

    //(void)ACE_OS::mkdir( "/var", 0755 );
    //(void)ACE_OS::mkdir( "../log", 0755 );
    //(void)ACE_OS::mkdir( "/var/log/SVS", 0755 );
    SVS_Log_Msg::make_log_dir();

    (void)pLogStrategy->init(of, ls_argv.get());

    //取时间戳
    char day_and_time[35] = {0};
    ACE_Time_Value cur_time = ACE_OS::gettimeofday ();
    time_t secs = ACE_OS::time(NULL);
    struct tm tmv;
    (void)ACE_OS::localtime_r( &secs, &tmv );
    (void)ACE_OS::snprintf (day_and_time,
      sizeof( day_and_time ),
      "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
      tmv.tm_year + 1900,
      tmv.tm_mon + 1,
      tmv.tm_mday,
      tmv.tm_hour,
      tmv.tm_min,
      tmv.tm_sec,
      cur_time.usec());

    ACE_OS::last_error(0);
    SVS_LOG((SVS_LM_CRITICAL,
        "\n\t\t======LOGFILE START[%s],ProcessID[%u]======\t\n",
        day_and_time,
        ACE_OS::getpid()));

//    // launch a new Thread
//    if (ACE_Thread_Manager::instance ()->spawn(ACE_THR_FUNC (run_reactor), (void *)pLogStraReactor) == -1)
//    {
//        ACE_ERROR_RETURN ((LM_ERROR, "Spawning Reactor.\n"), 1 );
//    }


    //启动日志检测线程
    if (run_check_thread_flag_)
    {
        int32_t result = ACE_Thread_Manager::instance ()->spawn(
                ACE_THR_FUNC (run_check_log_config_thread),
                (void *)this );
        if (result == -1)
        {
             SVS_LOG((SVS_LM_ERROR, "Start log debug params check thread fail."));
             return 1;
        }
    }


    return 0;
}

int32_t CSVS_Logging_Strategy::initLogStrategy(const char *strConfigFile,
        const char *strLogFile,
        bool        run_check_thread_flag )
{
    CLogging_Cfg_Param cfgParam;

    strcpy(cfgParam.m_strLogFile, strLogFile); //必须修改对应日志文件
    /*char *pszPos = strchr(cfgParam.m_strLogFile,'.');

    if(NULL != pszPos)
    {
        *pszPos = 0;
    }

    ACE_OS::sprintf(cfgParam.m_strLogFile, "%s_%u.log",
                    cfgParam.m_strLogFile,
                    ACE_OS::getpid());
   */

    strcpy( config_file_, strConfigFile);

    if (-1 == read_log_config(strConfigFile, cfgParam))
    {
        return -1;
    }

    run_check_thread_flag_ = run_check_thread_flag;

    SVS_Log_Msg::set_log_file_name( cfgParam.m_strLogFile );
    SVS_Log_Msg::set_log_file_max_size( cfgParam.m_iLogFileMaxSize*1024 );

    SVS_Log_Msg::set_log_file_max_count(cfgParam.m_iLogFileMaxCnt);

    return initLogStrategy(cfgParam);
}

int32_t CSVS_Logging_Strategy::read_log_config(
    const char *strConfigFile,
    CLogging_Cfg_Param &cfgParam )
{
    CAC_Config config((char *)strConfigFile);
//    ACE_TCHAR strSendTerminal[32], strChkInterval[32], strLogFileMaxSize[32], strLogFileMaxCnt[32];
    //ACE_TCHAR strSendTerminal[32];
    ACE_TCHAR strChkInterval[32];
    ACE_TCHAR strLogFileMaxSize[32];
    ACE_TCHAR strLogFileMaxCnt[32];


    cfgParam.m_bSendTerminal = 0;

    if (config.get("LOG_CFG", "ChkInterval", strChkInterval) != 0)
    {
        //ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) Fail to get LOG_CFG | ChkInterval.\n")));
        SVS_LOG((SVS_LM_WARNING, "Get LogCfg ChkInterval fail."));
        return -1;
    }

    // cfgParam.m_iChkInterval = atoi(strChkInterval);
    int32_t Iinterval = atoi(strChkInterval);
    cfgParam.m_iChkInterval = ((Iinterval < 60) ? Iinterval : 60);

    if (config.get("LOG_CFG", "LogFileMaxSize", strLogFileMaxSize) != 0)
    {
        //ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) Fail to get LOG_CFG | LogFileMaxSize.\n")));
        SVS_LOG((SVS_LM_WARNING, "Get LogCfg LogFileMaxSize fail."));
        return -1;
    }

    cfgParam.m_iLogFileMaxSize = atoi(strLogFileMaxSize);

    if (cfgParam.m_iLogFileMaxSize > 2000 * 1000 )
    {
        cfgParam.m_iLogFileMaxSize = 2000 * 1000;
    }
    if (config.get("LOG_CFG", "LogFileMaxCnt", strLogFileMaxCnt) != 0)
    {
        //ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) Fail to get LOG_CFG | LogFileMaxCnt.\n")));
        //SVS_LOG((SVS_LM_WARNING, "Get LogCfg LogFileMaxCnt fail."));
        memcpy(strLogFileMaxCnt, "10", sizeof("10"));
        //cfgParam.m_iLogFileMaxCnt = LOG_FILE_DEFAULT_COUNT;
        //return -1;
    }
    cfgParam.m_iLogFileMaxCnt = (uint32_t)atoi(strLogFileMaxCnt);

    //最少个数
    if (LOG_FILE_MIN_COUNT > cfgParam.m_iLogFileMaxCnt)
    {
        cfgParam.m_iLogFileMaxCnt = LOG_FILE_MIN_COUNT;
    }

    //最多个数
    if (LOG_FILE_MAX_COUNT < cfgParam.m_iLogFileMaxCnt)
    {
        cfgParam.m_iLogFileMaxCnt = LOG_FILE_MAX_COUNT;
    }


    char strLogCheckTime[STRING_TIME_BUF_LEN];
    if (config.get("SVS_DEBUG", "LogCheckTime", strLogCheckTime) == 0)
    {
        log_check_interval_time_ = atoi( strLogCheckTime ) * SECOND_NUM;
    }

    //最小间隔
    if (LOG_CHECK_MIN_INTERVAL_TIME > log_check_interval_time_)
    {
        log_check_interval_time_ = LOG_CHECK_MIN_INTERVAL_TIME;
    }

    //最大间隔
    if (LOG_CHECK_MAX_INTERVAL_TIME < log_check_interval_time_)
    {
        log_check_interval_time_ = LOG_CHECK_MAX_INTERVAL_TIME;
    }

    ACE_TCHAR strTemp[32] = {0};
    int32_t logHexDump = 1;

    if (config.get("SVS_DEBUG", "HexDump", strTemp ) == 0)
    {
        logHexDump = atoi ( strTemp ) ;
    }

    if ((0 != logHexDump) && (1 != logHexDump))
    {
        logHexDump = 1;
    }

    SVS_HEXDUMP_ENABLE(logHexDump);

    return 0;
}

//结束策略
int32_t CSVS_Logging_Strategy::endLogStrategy()
{
    running_ = false;

    if (NULL == pLogStraReactor)
    {
        SVS_LOG((SVS_LM_ERROR,"Log reactor is null."));
        return 0;
    }
    if (pLogStraReactor->end_reactor_event_loop () == -1)
    {
        SVS_LOG((SVS_LM_ERROR,
                    "Error ending reactor."));
    }

    return 0;
}

//获取日志策略参数
int32_t initLogStrategy(const ACE_TCHAR *svrCfgFile,
        CLogging_Cfg_Param &cfgParam,
        const char *        strLogFileName )
{
    CAC_Config config(svrCfgFile);
    ACE_TCHAR strSendTerminal[32], strChkInterval[32], strLogFileMaxSize[32], strLogFileMaxCnt[32];

    strcpy(cfgParam.m_strLogFile, strLogFileName); //必须修改对应日志文件

    if (config.get("LOG_CFG", "SendTerminal", strSendTerminal) != 0)
    {
        SVS_LOG((SVS_LM_INFO, "Fail to get LOG_CFG | SendTerminal."));
        return -1;
    }

    cfgParam.m_bSendTerminal = atoi(strSendTerminal);

    if (config.get("LOG_CFG", "ChkInterval", strChkInterval) != 0)
    {
        SVS_LOG((SVS_LM_INFO, "Fail to get LOG_CFG | ChkInterval."));
        return -1;
    }

    cfgParam.m_iChkInterval = atoi(strChkInterval);

    if (config.get("LOG_CFG", "LogFileMaxSize", strLogFileMaxSize) != 0)
    {
        SVS_LOG((SVS_LM_INFO, "Fail to get LOG_CFG | LogFileMaxSize."));
        return -1;
    }

    cfgParam.m_iLogFileMaxSize = atoi(strLogFileMaxSize);

    if (config.get("LOG_CFG", "LogFileMaxCnt", strLogFileMaxCnt) != 0)
    {
        SVS_LOG((SVS_LM_INFO,"Fail to get LOG_CFG | LogFileMaxCnt."));
        return -1;
    }

    cfgParam.m_iLogFileMaxCnt = (uint32_t)atoi(strLogFileMaxCnt);

    return 0;
}

//功能:检测日志文件,如不存在则重新初始化日志策略
int32_t CSVS_Logging_Strategy::check_log_file()
{
    static int32_t logFileNotExist = 0;
    //检测日志文件是否存在
    ACE_stat statBuf;
    int32_t statResult = ACE_OS::stat( gCfgArgvList[1].value, &statBuf );
    if( 0 == statResult )
    {
        //文件存在直接返回
        logFileNotExist = 0;
        return 0;
    }

    //ENOENT A component of the path file_name does not exist, or the path is an empty string.
    if( ENOENT != ACE_OS::last_error() )
    {
        //非文件不存在错误直接返回
        return 0;
    }
    //连续检测CHECK_LOG_FILE_MAX_TIMES次日志文件都不存在
    //重新初始化日志服务
    if( CHECK_LOG_FILE_MAX_TIMES > logFileNotExist )
    {
        logFileNotExist++;
        return 0;
    }

    logFileNotExist = 0;
    if(NULL == pLogStrategy)
    {
        return 0;
    }
    //重新打开日志策略
    (void)pLogStrategy->reopen_log_file();

    return 0;
}

//定时检测日志文件大小,超过大小进行日志文件备份
int32_t SVS_Logging_Strategy::handle_timeout ( const ACE_Time_Value &, const void * )
{
#if 0
#if defined (ACE_LACKS_IOSTREAM_TOTALLY)
    int32_t pos = ACE_OS::ftell (this->log_msg_->msg_ostream () );
    //取当前位置出错,返回
    if( -1 == pos )
    {
        return 0;
    }

    if ((size_t) pos > this->max_size_)
#else
        int32_t pos = this->log_msg_->msg_ostream ()->tellp ();
        //取当前位置出错,返回
        if( -1 == pos )
        {
            return 0;
        }
        if ((size_t) pos  > this->max_size_)
#endif /* ACE_LACKS_IOSTREAM_TOTALLY */
        {
            // Lock out any other logging.
            if (this->log_msg_->acquire ())
            {
                ACE_ERROR_RETURN (( LM_ERROR,
                                    ACE_TEXT ("Cannot acquire lock!\n")),
                                    -1 );
             }
            // Close the current ostream.
#if defined (ACE_LACKS_IOSTREAM_TOTALLY)
            FILE *output_file = (FILE *) this->log_msg_->msg_ostream ();
            if( NULL == output_file )
            {
               return -1;
            }
            ACE_OS::fclose (output_file);
            // We'll call msg_ostream() modifier later.
#else
            ofstream *output_file = (ofstream *) this->log_msg_->msg_ostream ();
            if( NULL == output_file )
            {
               return -1;
            }
            output_file->close ();
#endif /* ACE_LACKS_IOSTREAM_TOTALLY */

            //自定义的日志文件名
            ACE_TCHAR backup[MAXPATHLEN+1];
            ACE_Time_Value startupTime = ACE_OS::gettimeofday();
            ACE_Date_Time currTime(startupTime);

            ACE_OS::sprintf (   backup,
                                ACE_TEXT ("%s_%u_%04d_%02d_%02d_%02d_%02d_%02d.%05d"),
                                this->filename_,
                                ACE_OS::getpid(),
                                currTime.year(),
                                currTime.month(),
                                currTime.day(),
                                currTime.hour(),
                                currTime.minute(),
                                currTime.second(),
                                currTime.microsec() );

            ACE_OS::rename (this->filename_, backup);

            // Open a new log file with the same name.
#if defined (ACE_LACKS_IOSTREAM_TOTALLY)
            output_file = ACE_OS::fopen (this->filename_, ACE_TEXT ("wt"));
            if ( NULL == output_file )
            {
                return -1;
            }

            this->log_msg_->msg_ostream (output_file);
#else
            output_file->open (ACE_TEXT_ALWAYS_CHAR (this->filename_), ios::out);
#endif /* ACE_LACKS_IOSTREAM_TOTALLY */

            // Release the lock previously acquired.
            this->log_msg_->release ();
        }
#endif

    return 0;

}

//当日志文件删除,重新打开日志文件
int32_t SVS_Logging_Strategy::reopen_log_file()
{

//    (void)ACE_OS::mkdir( "/var", 0755 );
//    (void)ACE_OS::mkdir( "/var/log", 0755 );
//    (void)ACE_OS::mkdir( "/var/log/SVS", 0755 );
    SVS_Log_Msg::make_log_dir();

    // Lock out any other logging.
    if (this->log_msg_->acquire ())
    {
        SVS_LOG((SVS_LM_ERROR,
                            "Cannot acquire lock!"));
         return -1;
    }

    // Close the current ostream.
#if defined (ACE_LACKS_IOSTREAM_TOTALLY)
    FILE *output_file = (FILE *) this->log_msg_->msg_ostream ();
    if( NULL == output_file )
    {
       return -1;
    }
    ACE_OS::fclose (output_file);
    // We'll call msg_ostream() modifier later.
#else
    //ofstream *output_file = (ofstream *) this->log_msg_->msg_ostream ();
    ofstream *output_file = dynamic_cast<ofstream * >(this->log_msg_->msg_ostream ());
    if( NULL == output_file )
    {
       return -1;
    }
    output_file->close ();
#endif /* ACE_LACKS_IOSTREAM_TOTALLY */


    // Open a new log file with the same name.
#if defined (ACE_LACKS_IOSTREAM_TOTALLY)
    output_file = ACE_OS::fopen (this->filename_, ACE_TEXT ("wt"));
    if ( NULL == output_file )
    {
        return -1;
    }

    this->log_msg_->msg_ostream (output_file);
#else
    output_file->open (ACE_TEXT_ALWAYS_CHAR (this->filename_), ios::out );
#endif /* ACE_LACKS_IOSTREAM_TOTALLY */

    //取时间戳
    char day_and_time[35] = {0};
    ACE_Time_Value cur_time = ACE_OS::gettimeofday ();
    time_t secs = ACE_OS::time(NULL);
    struct tm tmv;
    (void)ACE_OS::localtime_r( &secs, &tmv );
    (void)ACE_OS::snprintf (day_and_time,
      sizeof( day_and_time ),
      "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
      tmv.tm_year + 1900,
      tmv.tm_mon + 1,
      tmv.tm_mday,
      tmv.tm_hour,
      tmv.tm_min,
      tmv.tm_sec,
      cur_time.usec());

      SVS_LOG((SVS_LM_CRITICAL,
                "\n\t\t======LOGFILE START[%s],ProcessID[%u]======\t\n",
                day_and_time,
                ACE_OS::getpid()));
    // Release the lock previously acquired.
    (void)this->log_msg_->release ();

    return 0;
}



