#ifndef _SVS_Logging_Strategy_H
#define _SVS_Logging_Strategy_H

#include "ace/Logging_Strategy.h"

//�����־�ļ�����������,�����ô�����������־����
#define CHECK_LOG_FILE_MAX_TIMES 3
//�����־�ļ����ڼ��,��λ��
#define CHECK_LOG_FILE_EXISTS_INTERVAL_TIME 3
class SVS_Logging_Strategy;

class CLogging_Cfg_Param
{
    public:
        CLogging_Cfg_Param();
        ~CLogging_Cfg_Param();

        //��־�ļ���ȫ·��
        char m_strLogFile[256];

        //�ļ���С���ʱ��(Secs)
        int32_t m_iChkInterval;

        //ÿ����־�ļ�����С(KB)
        int32_t m_iLogFileMaxSize;

        //��־�ļ�������
        uint32_t m_iLogFileMaxCnt;

        //�Ƿ����ն˷���
        int32_t m_bSendTerminal;
};

class CSVS_Logging_Strategy
{
    public:
        CSVS_Logging_Strategy();
        ~CSVS_Logging_Strategy();

        //��ʼ����־����
        int32_t initLogStrategy(const CLogging_Cfg_Param &cfgParam);
        int32_t initLogStrategy(    const char *strConfigFile,
                const char *strLogFile,
                bool        run_check_thread_flag = false );

        //��������
        int32_t endLogStrategy();


        //����:ȡ����״̬
        bool running()
        {
            return running_;
        }

        //����:ȡ��־���ʱ��
        int32_t log_check_interval_time()
        {
            return log_check_interval_time_;
        }

        //����:ȡ��־�ļ���
        char * config_file()
        {
            return config_file_;
        }
        int32_t check_log_file();   //�����־�ļ�,�粻���������³�ʼ����־����

    protected:
        enum
        {
            CONFIG_FILE_NAME_LEN = 512,             //�����ļ�����󳤶�
            LOG_CHECK_DEFAULT_INTERVAL_TIME = 5 * 60, //Ĭ�ϼ��ʱ��,��λ:��
            LOG_CHECK_MIN_INTERVAL_TIME = 60,       //��С���ʱ��,��λ:��
            LOG_CHECK_MAX_INTERVAL_TIME = 10 * 60,    //�����ʱ��,��λ:��
            STRING_TIME_BUF_LEN = 64,               //ʱ���ִ���󳤶�
            SECOND_NUM = 60,                        //60��ʱ��
            LOG_FILE_MAX_COUNT = 100,        /**ѭ����־�ļ�����������*/
            LOG_FILE_DEFAULT_COUNT = 10,       /** ѭ����־�ļ�Ĭ�ϱ������*/
            LOG_FILE_MIN_COUNT = 3,        /**ѭ����־�ļ�������С����*/
        };
    protected:
        int32_t read_log_config(const char *strConfigFile, CLogging_Cfg_Param& cfgParam);
    private:
        ACE_Reactor *pLogStraReactor;

        //ACE_Logging_Strategy *pLogStrategy;
        SVS_Logging_Strategy *pLogStrategy;

        bool running_;  //�������б�־
        int32_t log_check_interval_time_;   //��־���������
        bool run_check_thread_flag_;    //������־����־
        char config_file_[CONFIG_FILE_NAME_LEN];         //�����ļ���
};

extern int32_t initLogStrategy(const ACE_TCHAR *svrCfgFile, CLogging_Cfg_Param &cfgParam, const char *strLogFileName);

//��־�������
class SVS_Logging_Strategy: public ACE_Logging_Strategy
{
public:
    SVS_Logging_Strategy()
    {
    }
    ~SVS_Logging_Strategy()
    {
    }

    //��ʱ�����־�ļ���С,������С������־�ļ�����
    virtual int32_t handle_timeout ( const ACE_Time_Value &, const void * );

    //����־�ļ�ɾ��,���´���־�ļ�
    int32_t reopen_log_file();

    //����ļ�����Ƿ����
//    int32_t check_file_handle();

};
#endif //_SVS_Logging_Strategy_H

