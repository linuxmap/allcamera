#ifndef _SVS_Logging_Strategy_H
#define _SVS_Logging_Strategy_H

#include "ace/Logging_Strategy.h"

//检测日志文件存在最大次数,超过该次数则重启日志服务
#define CHECK_LOG_FILE_MAX_TIMES 3
//检测日志文件存在间隔,单位秒
#define CHECK_LOG_FILE_EXISTS_INTERVAL_TIME 3
class SVS_Logging_Strategy;

class CLogging_Cfg_Param
{
    public:
        CLogging_Cfg_Param();
        ~CLogging_Cfg_Param();

        //日志文件，全路径
        char m_strLogFile[256];

        //文件大小检测时间(Secs)
        int32_t m_iChkInterval;

        //每个日志文件最大大小(KB)
        int32_t m_iLogFileMaxSize;

        //日志文件最大个数
        uint32_t m_iLogFileMaxCnt;

        //是否向终端发送
        int32_t m_bSendTerminal;
};

class CSVS_Logging_Strategy
{
    public:
        CSVS_Logging_Strategy();
        ~CSVS_Logging_Strategy();

        //初始化日志策略
        int32_t initLogStrategy(const CLogging_Cfg_Param &cfgParam);
        int32_t initLogStrategy(    const char *strConfigFile,
                const char *strLogFile,
                bool        run_check_thread_flag = false );

        //结束策略
        int32_t endLogStrategy();


        //功能:取运行状态
        bool running()
        {
            return running_;
        }

        //功能:取日志检测时间
        int32_t log_check_interval_time()
        {
            return log_check_interval_time_;
        }

        //功能:取日志文件名
        char * config_file()
        {
            return config_file_;
        }
        int32_t check_log_file();   //检测日志文件,如不存在则重新初始化日志策略

    protected:
        enum
        {
            CONFIG_FILE_NAME_LEN = 512,             //配置文件名最大长度
            LOG_CHECK_DEFAULT_INTERVAL_TIME = 5 * 60, //默认检测时间,单位:秒
            LOG_CHECK_MIN_INTERVAL_TIME = 60,       //最小检测时间,单位:秒
            LOG_CHECK_MAX_INTERVAL_TIME = 10 * 60,    //最大检测时间,单位:秒
            STRING_TIME_BUF_LEN = 64,               //时间字串最大长度
            SECOND_NUM = 60,                        //60秒时间
            LOG_FILE_MAX_COUNT = 100,        /**循环日志文件保存最大个数*/
            LOG_FILE_DEFAULT_COUNT = 10,       /** 循环日志文件默认保存个数*/
            LOG_FILE_MIN_COUNT = 3,        /**循环日志文件保存最小个数*/
        };
    protected:
        int32_t read_log_config(const char *strConfigFile, CLogging_Cfg_Param& cfgParam);
    private:
        ACE_Reactor *pLogStraReactor;

        //ACE_Logging_Strategy *pLogStrategy;
        SVS_Logging_Strategy *pLogStrategy;

        bool running_;  //正在运行标志
        int32_t log_check_interval_time_;   //日志参数检测间隔
        bool run_check_thread_flag_;    //启动日志检测标志
        char config_file_[CONFIG_FILE_NAME_LEN];         //配置文件名
};

extern int32_t initLogStrategy(const ACE_TCHAR *svrCfgFile, CLogging_Cfg_Param &cfgParam, const char *strLogFileName);

//日志处理策略
class SVS_Logging_Strategy: public ACE_Logging_Strategy
{
public:
    SVS_Logging_Strategy()
    {
    }
    ~SVS_Logging_Strategy()
    {
    }

    //定时检测日志文件大小,超过大小进行日志文件备份
    virtual int32_t handle_timeout ( const ACE_Time_Value &, const void * );

    //当日志文件删除,重新打开日志文件
    int32_t reopen_log_file();

    //检查文件句柄是否可用
//    int32_t check_file_handle();

};
#endif //_SVS_Logging_Strategy_H

