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


int32_t             SVS_Log_Msg::log_priority_ = 0;//Ĭ�����������־
int32_t             SVS_Log_Msg::log_enable_ = 1;   //Ĭ�������־

bool            SVS_Log_Msg::bWriteLog = false;
int32_t            SVS_Log_Msg::lPreSize = 0;


/* ��־Ŀ¼��ɾ��ʱ�ݹ���ɳ������������*/
string          SVS_Log_Msg::log_file_name_ = "../log/svs_moudle_name.log";   //��־�ļ���
int32_t            SVS_Log_Msg::log_file_max_size_ = (1024 * 1024 * 100);  //��־�ļ�Ĭ������С
string          SVS_Log_Msg::log_file_path_ = "../log";

uint32_t    SVS_Log_Msg::log_file_max_count_ = 10;
vector<string>  SVS_Log_Msg::log_file_name_vec_;
/* ��ӡ����������*/
int32_t             SVS_Log_Msg::log_hexdump_enable_ = 1;   //Ĭ�ϴ�ӡ����������
int32_t             CSVS_Trace::_trace_enable = 0;

/* ��־Ŀ¼��ɾ��ʱ�ݹ���ɳ������������*/
vector <string>  SVS_Log_Msg::log_file_path_vec;  //��־�ļ�·��


int32_t SVS_Log_Msg::check_log_file()
{
#ifndef WIN32
    //�����̿��пռ�Ĵ�С
    struct statvfs  hdstat;
    int32_t             statvfsResult = statvfs(log_file_path_.c_str(), &hdstat);
    if (-1 == statvfsResult)
    {
        //        if( (ENOTDIR == statvfsResult) || (ENOENT ==statvfsResult) )
        int32_t lastError = ACE_OS::last_error();
        if ((ENOTDIR == lastError) || (ENOENT == lastError))
        {
            /* ��־Ŀ¼��ɾ��ʱ�ݹ���ɳ������������*/
            //Ŀ¼������,���´���
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

    //�����־�ļ��Ƿ����
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
            //�ٴμ��
            statResult = ACE_OS::stat(log_file_name_.c_str(), &statBuf);
            lastError = ACE_OS::last_error();
            if ((-1 == statResult) && (ENOENT == lastError))
            {
                /* ��־Ŀ¼��ɾ��ʱ�ݹ���ɳ������������*/
                //�ļ�������,���´��ļ�
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
                //ȡʱ���
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

    /* �ļ����� ����С*/

    if (statBuf.st_size > log_file_max_size_)
    {
        // Lock out any other logging.
        if (ace__->acquire())
        {
            return SVS_LOG_ERROR_CODE_LOCK;
        }
        //�ٴμ���ļ���С,��ֹ��һ���߳�Ҳ��⵽���ļ�����,��ͬʱ�л���־�ļ�
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

            //�Զ������־�ļ���
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


            /** ����ѭ����־��������ʱ��������ɵ���־ */
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
            //ȡʱ���
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
        /* δд����־ʱֱ�ӷ��� */
        if (!bWriteLog)
        {
            return 0;
        }

        /* �Ѿ�д����־����־�ļ���Сȷʵ������ ˢ��ǰһ����־��С */
        if (lPreSize < statBuf.st_size)
        {
            lPreSize = statBuf.st_size;
            return 0;
        }

        /* �Ѿ�д����־����־�ļ���Сȴ���� ˢ��ǰһ����־��С ���´�һ����־�ļ����� */
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

/* ��ӡ����������*/
int32_t SVS_Log_Msg::hexDump(const char *buffer,
                           size_t size, const ACE_TCHAR *text)
{

    //����ӡ��־���������DEBUG������ӡ����
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
        /* ����Ϊδд����־ */
        bWriteLog = false;
        return -1;
    }

    //��־�ȼ����ڶ�����־�ȼ��������
    if (log_priority < log_priority_)
    {
        /* ����Ϊδд����־ */
        bWriteLog = false;

        return -2;
    }

    char                logLMName[SVS_LOGLM_NAME_LEN];
    ACE_Log_Priority    logLM;

    /** ������־����ǰ׺����Ϊ3���ַ���������һ�� */
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
            /* ����Ϊδд����־ */
            bWriteLog = false;
            return -3;
            //break;
    }
    //����̶�����־ǰ׺
    char        logbuf[SVS_LOGBUF_LEN] = {0};

    //ȡʱ���
    char        day_and_time[35] = {0};

    /** ʱ��ǰ׺����Ҫ���� */
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

        //�����־�ļ���С,����ʱ�����л�
        int32_t checkResult = check_log_file();
        if (0 != checkResult)
        {
            if (SVS_LOG_ERROR_CODE_DISK_FULL == checkResult)
            {
                /* ����Ϊδд����־ */
                bWriteLog = false;

                return -1;
            }
        }

        ACE_Log_Msg *ace___ = ACE_Log_Msg::instance();

        uint32_t threadID = ACE_Thread::self();

        /** ������Ϊ0ʱ����ӡ����0ʱ����������Ҳ��ӡ����*/
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

        //���������־����С��2������ӡ����
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
        //argp����ʱ�Ѿ���ʼ������Ҫ�ٸ�ֵ
        va_list argp;

        /* �������澯 */
        /* va_start ʹ��������gcc�������»��Զ���������Ӱ��*/
        //va_start(argp, logbuf);
        va_start(argp, format);
        int32_t iLogRet = ace___->log(logbuf, logLM, argp);
        va_end(argp);
#endif
        (void)ace___->release();

        bWriteLog = true;

        return iLogRet;
    }

    /** SVS_TRACE ��*/
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


    /** SQL��䳬ʱ��ӡ�� */
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

