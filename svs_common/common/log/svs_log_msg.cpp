#include "ace/Log_Msg.h"
#include "ace/OS.h"
#include "ace/Thread.h"
#include "ace/ACE.h"
#include <string.h>
#include <stdio.h>
#include "svs_log_msg.h"
#include "ace/Logging_Strategy.h"
#include "ace/ACE.h"
#include "ace/Get_Opt.h"

#include "ace/streams.h"
#include "ace/Lib_Find.h"
#include "ace/Log_Msg.h"
#include "ace/Reactor.h"
#include "ace/OS_NS_string.h"
#include "ace/OS_NS_stdio.h"
#include "ace/OS_NS_unistd.h"
#include "ace/Date_Time.h"

#ifndef WIN32
#include <sys/statvfs.h>
#endif


int32_t             SVS_Log_Msg::log_priority_ = 0;//默认输出所有日志
int32_t             SVS_Log_Msg::log_enable_ = 1;   //默认输出日志

bool            SVS_Log_Msg::bWriteLog = false;
int32_t            SVS_Log_Msg::lPreSize = 0;


/* 日志目录被删除时递归造成程序崩溃的问题*/
string          SVS_Log_Msg::log_file_name_ = "../log/svs_moudle_name.log";   //日志文件名
int32_t            SVS_Log_Msg::log_file_max_size_ = (1024 * 1024 * 100);  //日志文件默认最大大小
string          SVS_Log_Msg::log_file_path_ = "../log";

uint32_t    SVS_Log_Msg::log_file_max_count_ = 10;
vector<string>  SVS_Log_Msg::log_file_name_vec_;
/* 打印二进制码流*/
int32_t             SVS_Log_Msg::log_hexdump_enable_ = 1;   //默认打印二进制码流
int32_t             CSVS_Trace::_trace_enable = 0;

/* 日志目录被删除时递归造成程序崩溃的问题*/
vector <string>  SVS_Log_Msg::log_file_path_vec;  //日志文件路径


int32_t SVS_Log_Msg::check_log_file()
{
#ifndef WIN32
    //检测磁盘空闲空间的大小
    struct statvfs  hdstat;
    int32_t             statvfsResult = statvfs(log_file_path_.c_str(), &hdstat);
    if (-1 == statvfsResult)
    {
        //        if( (ENOTDIR == statvfsResult) || (ENOENT ==statvfsResult) )
        int32_t lastError = ACE_OS::last_error();
        if ((ENOTDIR == lastError) || (ENOENT == lastError))
        {
            /* 日志目录被删除时递归造成程序崩溃的问题*/
            //目录不存在,重新创建
            //(void)ACE_OS::mkdir("/var", 0755);
            //(void)ACE_OS::mkdir("/var/log", 0755);
            //(void)ACE_OS::mkdir("../log", 0755);
            make_log_dir();
        }

        lPreSize = 0;

        return SVS_LOG_ERROR_CODE_SYSTEM_CALL;
    }

    uint64_t freeDiskSize = (uint64_t)
                                      hdstat.f_bsize * hdstat.f_bavail;

    if (SVS_LOG_DISK_LIMIT_SIZE > freeDiskSize)
    {
        return SVS_LOG_ERROR_CODE_DISK_FULL;
    }
#endif // WIN32

    ACE_Log_Msg *ace__ = ACE_Log_Msg::instance();

    //检测日志文件是否存在
    ACE_stat    statBuf;
    int32_t         statResult = ACE_OS::stat(log_file_name_.c_str(), &statBuf);
    if (-1 == statResult)
    {
        //ENOENT A component of the path file_name does not exist, or the path is an empty string.
        int32_t lastError = ACE_OS::last_error();

        if (ENOENT == lastError)
        {
            // Lock out any other logging.
            if (ace__->acquire())
            {
                lPreSize = 0;

                return SVS_LOG_ERROR_CODE_LOCK;
            }
            //再次检查
            statResult = ACE_OS::stat(log_file_name_.c_str(), &statBuf);
            lastError = ACE_OS::last_error();
            if ((-1 == statResult) && (ENOENT == lastError))
            {
                /* 日志目录被删除时递归造成程序崩溃的问题*/
                //文件不存在,重新打开文件
                //(void)ACE_OS::mkdir("/var", 0755);
                //(void)ACE_OS::mkdir("../log", 0755);
                //(void)ACE_OS::mkdir("/var/log/SVS", 0755);
                make_log_dir();

                // Close the current ostream.
                ofstream*output_file = dynamic_cast<ofstream* >(ace__->msg_ostream());

                if (NULL == output_file)
                {
                    // Release the lock previously acquired.
                    (void)ace__->release();
                    return SVS_LOG_ERROR_CODE_CODE;
                }
                output_file->close();
                output_file->open(log_file_name_.c_str(), ios::out);
                //取时间戳
                char            day_and_time[35] = {0};
                ACE_Time_Value  cur_time = ACE_OS::gettimeofday();
                time_t          secs = ACE_OS::time(NULL);
                struct tm       tmv;
                (void)ACE_OS::localtime_r(&secs, &tmv);
                (void)ACE_OS::snprintf(day_and_time,
                                 sizeof(day_and_time),
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
            }
            // Release the lock previously acquired.
            (void)ace__->release();
            return 0;
        }
        lPreSize = 0;
        return SVS_LOG_ERROR_CODE_SYSTEM_CALL;
    }

    /* 文件存在 检测大小*/

    if (statBuf.st_size > log_file_max_size_)
    {
        // Lock out any other logging.
        if (ace__->acquire())
        {
            return SVS_LOG_ERROR_CODE_LOCK;
        }
        //再次检测文件大小,防止另一个线程也检测到了文件过大,而同时切换日志文件
        statResult = ACE_OS::stat(log_file_name_.c_str(), &statBuf);
        if ((-1 != statResult) && (statBuf.st_size > log_file_max_size_))
        {
            // Close the current ostream.
            ofstream*output_file = dynamic_cast<ofstream* >(ace__->msg_ostream());

            if (NULL == output_file)
            {
                // Release the lock previously acquired.
                (void)ace__->release();
                return SVS_LOG_ERROR_CODE_CODE;
            }

            output_file->close();

            //自定义的日志文件名
            ACE_TCHAR       backup[MAXPATHLEN + 1];
            ACE_Time_Value  cur_time = ACE_OS::gettimeofday();
            time_t          secs = ACE_OS::time(NULL);
            struct tm       tmv;
            (void)ACE_OS::localtime_r(&secs, &tmv);

            (void)ACE_OS::snprintf(backup,
                             MAXPATHLEN,
                             ACE_TEXT("%s_%u_%04d_%02d_%02d_%02d_%02d_%02d.%06ld"),
                             log_file_name_.c_str(),
                             ACE_OS::getpid(),
                             tmv.tm_year + 1900,
                             tmv.tm_mon + 1,
                             tmv.tm_mday,
                             tmv.tm_hour,
                             tmv.tm_min,
                             tmv.tm_sec,
                             cur_time.usec());


            /** 到达循环日志数量限制时，覆盖最旧的日志 */
            if (log_file_name_vec_.size() >= (log_file_max_count_ - 1))
            {
                vector<string>::iterator    iter = log_file_name_vec_.begin();
                if (iter != log_file_name_vec_.end())
                {
                    (void)ACE_OS::unlink((*iter).c_str());
                    (void)log_file_name_vec_.erase(iter);
                }
            }

            (void)ACE_OS::rename(log_file_name_.c_str(), backup);

            string  strBackup = backup;
            log_file_name_vec_.push_back(strBackup);

            // Open a new log file with the same name.
            output_file->open(log_file_name_.c_str(), ios::out);
            //取时间戳
            char            day_and_time[35] = {0};

            (void)ACE_OS::snprintf(day_and_time,
                             sizeof(day_and_time),
                             "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
                             tmv.tm_year + 1900,
                             tmv.tm_mon + 1,
                             tmv.tm_mday,
                             tmv.tm_hour,
                             tmv.tm_min,
                             tmv.tm_sec,
                             cur_time.usec());

            lPreSize = 0;

            SVS_LOG((SVS_LM_CRITICAL,
                       "\n\t\t======LOGFILE START[%s],ProcessID[%u]======\n\n",
                       day_and_time,
                       ACE_OS::getpid()));
        }
        // Release the lock previously acquired.
        (void)ace__->release();
    }
    else
    {
        /* 未写入日志时直接返回 */
        if (!bWriteLog)
        {
            return 0;
        }

        /* 已经写入日志，日志文件大小确实增长了 刷新前一次日志大小 */
        if (lPreSize < statBuf.st_size)
        {
            lPreSize = statBuf.st_size;
            return 0;
        }

        /* 已经写入日志，日志文件大小却不变 刷新前一次日志大小 重新打开一次日志文件附加 */
        lPreSize = statBuf.st_size;

        // Lock out any other logging.
        if (ace__->acquire())
        {
            return SVS_LOG_ERROR_CODE_LOCK;
        }

        // Close the current ostream.
        ofstream*output_file = dynamic_cast<ofstream* >(ace__->msg_ostream());

        if (NULL == output_file)
        {
            // Release the lock previously acquired.
            (void)ace__->release();
            return SVS_LOG_ERROR_CODE_CODE;
        }

        output_file->close();
        output_file->open(log_file_name_.c_str(), ios::out | ios::app);

        // Release the lock previously acquired.
        (void)ace__->release();
        return 0;
    }

    return 0;
}

/* 打印二进制码流*/
int32_t SVS_Log_Msg::hexDump(const char *buffer,
                           size_t size, const ACE_TCHAR *text)
{

    //不打印日志、级别大于DEBUG、不打印码流
    if (0 == log_enable_
        || SVS_LM_DEBUG < log_priority_
        || 0 == log_hexdump_enable_)
    {
        return 0;
    }

    ACE_TCHAR buf[(SVS_LOGBUF_LEN - SVS_VERBOSE_LEN) - 58];
    // 58 for the HEXDUMP header;

    ACE_TCHAR *msg_buf;
    const size_t text_sz = text ? ACE_OS::strlen(text) : 0;
    try
    {
        msg_buf = new ACE_TCHAR[text_sz + 58];
    }
    catch (...)
    {
        return 0;
    }

    buf[0] = 0; // in case size = 0

    const size_t len = ACE::format_hexdump
    (buffer, size, buf, sizeof (buf) / sizeof (ACE_TCHAR) - text_sz);

    int32_t sz = 0;

    if (text)
    sz = ACE_OS::sprintf (msg_buf,
                          ACE_TEXT ("%s - "),
                          text);

    sz += ACE_OS::sprintf (msg_buf + sz,
                         ACE_TEXT ("HEXDUMP ")
                         ACE_SIZE_T_FORMAT_SPECIFIER
                         ACE_TEXT (" bytes"),
                         size);

    if (len < size)
    ACE_OS::sprintf (msg_buf + sz,
                     ACE_TEXT (" (showing first ")
                     ACE_SIZE_T_FORMAT_SPECIFIER
                     ACE_TEXT (" bytes)"),
                     len);

    // Now print out the formatted buffer.
    SVS_LOG((SVS_LM_DEBUG, "%s\n%s", msg_buf, buf));
    //this->log (log_priority,
    //         ACE_TEXT ("%s\n%s"),
    //         msg_buf,
    //         buf);

    delete [] msg_buf;
    return 0;
}

int32_t SVS_Log_Msg::log(int32_t log_priority, const char *format, ...)
{
    if (0 == log_enable_)
    {
        /* 重置为未写入日志 */
        bWriteLog = false;
        return -1;
    }

    //日志等级低于定义日志等级，不输出
    if (log_priority < log_priority_)
    {
        /* 重置为未写入日志 */
        bWriteLog = false;

        return -2;
    }

    char                logLMName[SVS_LOGLM_NAME_LEN];
    ACE_Log_Priority    logLM;

    /** 调整日志级别前缀长度为3个字符，各级别一致 */
    switch (log_priority)
    {
        case SVS_LM_INFO:
            logLM = LM_INFO;
            (void)ACE_OS::snprintf(logLMName, SVS_LOGLM_NAME_LEN, "INF");
            break;
        case SVS_LM_DEBUG:
        case SVS_LM_TRACE:
            logLM = LM_DEBUG;
            (void)ACE_OS::snprintf(logLMName, SVS_LOGLM_NAME_LEN, "DBG");
            break;
        case SVS_LM_WARNING:
            logLM = LM_WARNING;
            (void)ACE_OS::snprintf(logLMName, SVS_LOGLM_NAME_LEN, "WAR");
            break;
        case SVS_LM_ERROR:
            logLM = LM_ERROR;
            (void)ACE_OS::snprintf(logLMName, SVS_LOGLM_NAME_LEN, "ERR");
            break;
        case SVS_LM_CRITICAL:
            logLM = LM_CRITICAL;
            (void)ACE_OS::snprintf(logLMName, SVS_LOGLM_NAME_LEN, "CRI");
            break;
        default:
            /* 重置为未写入日志 */
            bWriteLog = false;
            return -3;
            //break;
    }
    //加入固定的日志前缀
    char        logbuf[SVS_LOGBUF_LEN] = {0};

    //取时间戳
    char        day_and_time[35] = {0};

    /** 时间前缀不需要日期 */
#ifdef WIN32
    SYSTEMTIME  local;
    ::GetLocalTime(&local);

    if (SVS_LM_INFO != log_priority)
    {
        ACE_OS::snprintf(day_and_time,
                         sizeof(day_and_time),
                                   //"%04d-%02d-%02d %02d:%02d:%02d.%06d",
                         "%02d:%02d:%02d.%06d",
                                   //(int32_t) local.wYear,
                         //(int32_t) local.wMonth,
          //(int32_t) local.wDay,
          (int32_t) local.wHour,
                         (int32_t) local.wMinute,
                         (int32_t) local.wSecond,
                         (int32_t) (local.wMilliseconds * 1000));
    }
    else
    {
        ACE_OS::snprintf(day_and_time,
        sizeof( day_and_time ),
        //"%04d-%02d-%02d %02d:%02d:%02d.%06d",
        "%02d:%02d:%02d",
        //(int32_t) local.wYear,
        //(int32_t) local.wMonth,
        //(int32_t) local.wDay,
        (int32_t) local.wHour,
        (int32_t) local.wMinute,
        (int32_t) local.wSecond);
        //(int32_t) (local.wMilliseconds * 1000));

        }
#else
        ACE_Time_Value  cur_time = ACE_OS::gettimeofday();
        time_t          secs = ACE_OS::time(NULL);
        struct tm       tmv;
        //    ACE_OS::memset(&tmv, 0, sizeof(tmv));
        //    ACE_OS::memcpy(&tmv, ACE_OS::localtime(&secs), sizeof(tmv));
        (void)ACE_OS::localtime_r(&secs, &tmv);

        //    if(SVS_LM_INFO != log_priority)
        //    {

        (void)ACE_OS::snprintf(day_and_time,
                         sizeof(day_and_time),
                         "%02d-%02d %02d:%02d:%02d.%06ld",
                         tmv.tm_mon + 1,
                         tmv.tm_mday,
                         tmv.tm_hour,
                         tmv.tm_min,
                         tmv.tm_sec,
                         cur_time.usec());
        /*
            }
            else
            {
              ACE_OS::snprintf (day_and_time,
                  sizeof( day_and_time ),
                   //"%04d-%02d-%02d %02d:%02d:%02d.%06d",
                  "%02d:%02d:%02d",
                  //tmv.tm_year + 1900,
                  //tmv.tm_mon + 1,
                  //tmv.tm_mday,
                  tmv.tm_hour,
                  tmv.tm_min,
                  tmv.tm_sec);
            }
        */
#endif

        //检测日志文件大小,过大时进行切换
        int32_t checkResult = check_log_file();
        if (0 != checkResult)
        {
            if (SVS_LOG_ERROR_CODE_DISK_FULL == checkResult)
            {
                /* 重置为未写入日志 */
                bWriteLog = false;

                return -1;
            }
        }

        ACE_Log_Msg *ace___ = ACE_Log_Msg::instance();

        uint32_t threadID = ACE_Thread::self();

        /** 错误码为0时不打印，非0时将错误描述也打印出来*/
        char    szErrorMsg[128] = {0};
        int32_t     iErrorCode = ACE_OS::last_error();

        (0 == iErrorCode)
        ? ((void)ACE_OS::snprintf(szErrorMsg, 128, "%s", ""))
        : ((void)ACE_OS::snprintf(szErrorMsg,
                          128,
                          " ErrorCode:%d[%s]",
                          iErrorCode,
                          ACE_OS::strerror(iErrorCode)));


        if (SVS_LM_INFO == log_priority && SVS_LM_INFO == log_priority_)
        {
            (void)ACE_OS::snprintf(logbuf,
                             SVS_LOGBUF_LEN,
                             "%s|%u|%u|%s|%s ",
                             day_and_time,
                             ACE_OS::getpid(),
                             threadID,
                             logLMName,
                             szErrorMsg);
        }
        else
        {
            (void)ACE_OS::snprintf(logbuf,
                             SVS_LOGBUF_LEN,
                             "%s|%u|%u|%s|%s:%d|%s ",
                             day_and_time,
                             ACE_OS::getpid(),
                             threadID,
                             logLMName,
                             ace___->file(),
                             ace___->linenum(),
                             szErrorMsg);
        }

        size_t  logLen = strlen(logbuf);

#ifdef WIN32
        va_list argp;
        va_start(argp, format);
        _vsnprintf(&logbuf[logLen], SVS_LOGBUF_LEN - logLen, format, argp);
        va_end(argp);
#else
        strncpy(&logbuf[logLen], format, SVS_LOGBUF_LEN - logLen);
#endif
        logLen = strlen(logbuf);

        //如果空闲日志缓冲小于2将不打印换行
        if (2 <= (SVS_LOGBUF_LEN - logLen))
        {
            logbuf[logLen] = '\n';
            logbuf[logLen + 1] = '\0';
        }
        // Lock out any other logging.
        if (ace___->acquire())
        {
            return SVS_LOG_ERROR_CODE_LOCK;
        }

#ifdef WIN32
        int32_t iLogRet = ace___->log(logLM, logbuf);
#else
        //argp定义时已经初始化不需要再赋值
        va_list argp;

        /* 清除编译告警 */
        /* va_start 使用有误，在gcc编译器下会自动修正，不影响*/
        //va_start(argp, logbuf);
        va_start(argp, format);
        int32_t iLogRet = ace___->log(logbuf, logLM, argp);
        va_end(argp);
#endif
        (void)ace___->release();

        bWriteLog = true;

        return iLogRet;
    }

    /** SVS_TRACE 宏*/
    CSVS_Trace::CSVS_Trace(const ACE_TCHAR * n, int32_t line, const ACE_TCHAR * file)
    {
        if( 0 == _trace_enable)
        {
            return ;
        }

        this->name_ = n;

        // If ACE has not yet been initialized, don't try to trace... there's
        // too much stuff not yet initialized.
        if (!ACE_OS_Object_Manager::starting_up())
        {
            ACE_Log_Msg *lm = ACE_LOG_MSG;
            if (lm->tracing_enabled() && lm->trace_active() == 0)
            {
                lm->trace_active(1);
                SVS_LOG((SVS_LM_TRACE,
                           ACE_TEXT("%*s FUNC_START calling %s in file %s:%d..."),
                           3 * lm->inc(),
                           ACE_TEXT("*"),
                           this->name_,
                           file,
                           line));
                lm->trace_active(0);
            }
        }
    }


    CSVS_Trace::~CSVS_Trace(void)
    {
        // If ACE has not yet been initialized, don't try to trace... there's
        // too much stuff not yet initialized.
        try
        {
            if( 0 == _trace_enable)
            {
                return ;
            }

            if (!ACE_OS_Object_Manager::starting_up())
            {
                ACE_Log_Msg *lm = ACE_LOG_MSG;
                if (lm->tracing_enabled() && lm->trace_active() == 0)
                {
                    lm->trace_active(1);
                    SVS_LOG((SVS_LM_TRACE,
                               ACE_TEXT("%*s FUNC_END leaving %s..."),
                               3 * lm->dec(),
                               ACE_TEXT("*"),
                               this->name_));
                    lm->trace_active(0);
                }
            }

            name_ = NULL;
        }
        catch(...)
        {
             name_ = NULL;
        }
        name_ = NULL;
    }


    /** SQL语句超时打印宏 */
    void    SVS_SqlCost::SqlStart(const ACE_TCHAR *s)
    {
        m_tvStart = ACE_Time_Value(ACE_OS::gettimeofday());
    sql = s;
    return;
}

void SVS_SqlCost::SqlEnd()const
{
    ACE_Time_Value  timecost = ACE_Time_Value(ACE_OS::gettimeofday());
    timecost -= m_tvStart;
    uint32_t nUsecs = (uint32_t)timecost.sec() * 1000 * 1000
                                + (uint32_t)timecost.usec();

    if ((500 * 1000) < nUsecs)
    {
        SVS_LOG((SVS_LM_WARNING,
                   "[TIMECOST]DB operation cost [%d]usecs, exec sql[%s].",
                   nUsecs,
                   sql));
    }
    return;
}

