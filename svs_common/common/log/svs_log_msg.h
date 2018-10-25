#ifndef __SVS_LOG_MSG_
#define __SVS_LOG_MSG_
#include "ace/Log_Msg.h"
#include "ace/OS.h"
#include "ace/Thread.h"
#include "ace/ACE.h"
#include "ace/Trace.h"
#include <vector>
#include <string>

using namespace std;

//日志等级
#define SVS_LM_TRACE 0
#define SVS_LM_DEBUG 1
#define SVS_LM_INFO 3
#define SVS_LM_WARNING 5
#define SVS_LM_ERROR 7
#define SVS_LM_CRITICAL 9


/* 打印二进制码流*/
//日志输出最大长度
#define SVS_LOGBUF_LEN (4 * 1024) // 因为需要打印二进制码流 将最大长度调整为4K

//可变长度 包含日期/PID/TID等
#define SVS_VERBOSE_LEN 128

//日志等级名最大长度
#define SVS_LOGLM_NAME_LEN 64


//磁盘空闲空间最低值
#define SVS_LOG_DISK_LIMIT_SIZE (200 * 1024 * 1024)

//日志文件检测错误码
#define SVS_LOG_ERROR_CODE_DISK_FULL 1
#define SVS_LOG_ERROR_CODE_SYSTEM_CALL -1
#define SVS_LOG_ERROR_CODE_LOCK -2
#define SVS_LOG_ERROR_CODE_CODE -3

class   SVS_Log_Msg;
class   SVS_TRACE;

#ifndef SVS_NDEBUG

#define SVS_LOG(X) \
   do { \
       int32_t __ace_error = ACE_Log_Msg::last_error_adapter (); \
       ACE_Log_Msg *ace___ = ACE_Log_Msg::instance (); \
       ace___->conditional_set (__FILE__, __LINE__, 0, __ace_error); \
       ace___->file( __FILE__ ); \
       ace___->linenum( __LINE__ ); \
       (void) SVS_Log_Msg::log X; \
       ACE_OS::last_error( 0 ); \
   } while (0)

/* 打印二进制码流*/
#define SVS_HEXDUMP(X) \
    do { \
        (void) SVS_Log_Msg::hexDump X; \
    } while (0)

#define SVS_HEXDUMP_ENABLE(X) \
   do {\
       SVS_Log_Msg::set_log_hexdump( X ); \
   } while (0)

#define SVS_LOG_LM(X) \
   do {\
        SVS_Log_Msg::set_log_priority( X ); \
        if ( SVS_LM_TRACE == X )             \
        { \
            CSVS_Trace::set_trace_enable(); \
        } \
        else \
        { \
            CSVS_Trace::set_trace_disable(); \
        } \
   } while (0)

#define SVS_LOG_ENABLE() \
   do {\
       SVS_Log_Msg::set_log_enabled(); \
   } while (0)

#define SVS_LOG_DISABLE() \
   do {\
       SVS_Log_Msg::set_log_disabled(); \
   } while (0)

#define SVS_TRACE_IMPL() CSVS_Trace ____ (ACE_TEXT (__FUNCTION__), __LINE__, ACE_TEXT (__FILE__))
#define SVS_TRACE() SVS_TRACE_IMPL()

#else

#define SVS_LOG(X) \
   do { \
   } while (0)

#define SVS_LOG_LM(X) \
   do { \
   } while (0)

#define SVS_LOG_ENABLE() \
   do { \
   } while (0)

#define SVS_LOG_DISABLE() \
   do { \
   } while (0)

#endif

#ifndef AFX_EXT_CLASS
#define AFX_EXT_CLASS
#endif

class SVS_Log_Msg
{
public:
    AFX_EXT_CLASS static int32_t hexDump(const char *buffer,
                           size_t size, const ACE_TCHAR *text = 0);

    static void set_log_hexdump(int32_t hexdump_enable)
    {
        log_hexdump_enable_ = hexdump_enable;
    }


    //写日志信息
    AFX_EXT_CLASS static int32_t log(int32_t log_priority, const char *format, ...);

    //设置日志等级
    static void set_log_priority(int32_t log_priority)
    {
        log_priority_ = log_priority;
    }

    //读取日志等级
    static int32_t get_log_priority()
    {
        return log_priority_;
    }

    //设置日志开关
    static void set_log_enabled()
    {
        log_enable_ = 1;
    }

    static void set_log_disabled()
    {
        log_enable_ = 0;
    }

    //读取日志开关
    static int32_t get_log_flag()
    {
        return log_enable_;
    }

    static void set_log_file_name(const char *fileName)
    {
        if (NULL == fileName)
        {
            return;
        }

        log_file_name_ = fileName;

        string::size_type   posFind = log_file_name_.rfind('/');
        if (string::npos != posFind)
        {
            log_file_path_ = log_file_name_.substr(0, posFind);
        }

        /*日志目录被删除时递归造成程序崩溃的问题*/
        log_file_path_vec.clear();
        char szPath[PATH_MAX] = {0};
        (void)ACE_OS::getcwd(szPath, PATH_MAX);

        string strPath = szPath;
        posFind = strPath.rfind('/');
        bool bFirst = true;
        while(string::npos != posFind && 1 != posFind)
        {

            if(bFirst)
            {
                strPath = strPath.substr(0, posFind);
                string strTmpPath = strPath;
                posFind = strTmpPath.rfind('/');

                if(string::npos == posFind)
                {
                    break;
                }
                //strTmpPath = strTmpPath.substr(0, posFind);
                strTmpPath += "/log";
                bFirst = false;
                log_file_path_vec.push_back(strTmpPath);
                log_file_path_vec.push_back(strPath);
            }
            else
            {
                log_file_path_vec.push_back(strPath);
            }

            strPath = strPath.substr(0, posFind);
            posFind = strPath.rfind('/');
        }

        if(log_file_path_vec.size() > 0)
        {
            log_file_path_ = log_file_path_vec[0];
        }

    }

    static void make_log_dir()
    {
        int32_t iVecSize = log_file_path_vec.size();

        if(iVecSize <= 0 )
        {
            return ;
        }

        for (int32_t i = iVecSize - 1; i >= 0; i--)
        {
            (void)ACE_OS::mkdir(log_file_path_vec[i].c_str(), 0755);
        }

    }

    static void set_log_file_max_size(const int32_t fileSize)
    {
        log_file_max_size_ = fileSize;
    }


    static void set_log_file_max_count(const uint32_t filecount)
    {
        log_file_max_count_ = filecount;
    }


protected:
    static int32_t check_log_file();

private:
    static int32_t              log_priority_;   //日志等级
    static int32_t              log_enable_;     //日志开关

    static int32_t              log_hexdump_enable_; // 打印二进制码流开关

    static string           log_file_name_;   //日志文件名
    static int32_t             log_file_max_size_; //日志文件最大大小
    static string           log_file_path_;   //日志文件路径

    static uint32_t     log_file_max_count_; //循环日志数
    static vector <string>  log_file_name_vec_;   //保存日志文件名的队列

    static vector <string>  log_file_path_vec;  //日志文件路径

    static bool             bWriteLog;
    static int32_t             lPreSize;
};

class CSVS_Trace
{
public:
    CSVS_Trace(const ACE_TCHAR *n,
              int32_t line = 0,
              const ACE_TCHAR *file = ACE_TEXT(""));
    virtual ~CSVS_Trace();

    static void set_trace_enable()
    {
        _trace_enable = 1;
    }

    static void set_trace_disable()
    {
        _trace_enable = 0;
    }

private:
    const ACE_TCHAR *name_;
    static int32_t      _trace_enable;
};

class SVS_SqlCost
{
public:
    SVS_SqlCost()
    {
    }

    ~SVS_SqlCost()
    {
    }

public:
    void SqlStart(const ACE_TCHAR *s);

    void SqlEnd() const;
private:
    ACE_Time_Value  m_tvStart;
    const ACE_TCHAR *sql;
};

#endif
