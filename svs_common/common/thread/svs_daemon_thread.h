/*****************************************************************************

 ******************************************************************************
  �ļ���          : SVS_Daemon_Thead.h
  �汾��          : 1.0
  ����            :
  ��������        : 2009-6-18
  ����޸�        :
  ��������        : ҵ���̵߳ļ���̣߳����ڼ��ҵ���̵߳Ļ״̬��������Ҫ���
                    ��ҵ���߳�����ʱ�����򱾼���߳�ע�ᣬ�������������ҵ����
                    ��ÿ����һ��ҵ���һ���ϱ����ڵ���ʱ�򣬻������߳��ϱ�һ��
                    �Լ��Ļ״̬��һ��������ڵ��󣬼���̱߳����������Լ����
                    ���̣߳��������ĳ�߳��Ѿ�һ���������û���ϱ��Լ���״̬�ˣ�
                    ��������߳̽�������������RtRecordSvr��һ��һ��������ڴ�С
                    ����3���ϱ�����
  �����б�        :
  �޸���ʷ        :
  1 ����          : 2009-6-18
    ����          :
    �޸�����      : ����
 *******************************************************************************/
#ifndef __SVS_DAEMON_THREAD_H__
#define __SVS_DAEMON_THREAD_H__

#include <svs_ace_header.h>
#include <list>

// �����߳�״���ϱ����ڣ���λ����
#define REPORT_THREAD_STATUS_INTERVAL                10
// ����߳��̼߳��=����*״̬�ϱ���
#define MULTIPLE_OF_REPORT_THREAD_STATUS_INTERVAL   3

#define THREAD_NUM_EXCEPT_TASK                        0
#define COMMON_BUFFER_SIZE                            512
// ˢ�������ļ�����,��λ:��
#define REFRESH_CONFIG_INTERVAL             60


enum CHECK_INTERVAL
{
    MIN_INTERVAL         = 180,              // ��С���
    MAX_INTERVAL         = 3600,           // �����
    DEFAULT_INTERVAL    = 1800,            // Ĭ�ϼ��

    DEFAULT_REPORT_INTERVAL        = 10,    // �����߳�״̬�ϱ����������,��λ��
    DEFAULT_THREAD_CHECK_INTERVAL = 60     // �����̼߳���������λ��
};

// �����߳����Ƶ���󳤶�
#define MAX_THREAD_NAME               64

// ����������ܹ�����������߳���
#define MAX_THREAD_NUM                 512
class CThread_Stat_Reporter;
/*

*/
class CSVS_Daemon_Thread : public ACE_Task<ACE_MT_SYNCH>
{
public:
    static CSVS_Daemon_Thread* instance()
    {
        static CSVS_Daemon_Thread svsDaemonThread;
        return &svsDaemonThread;
    }

    virtual ~CSVS_Daemon_Thread();

    // ��ʼ��
    int32_t Init(uint32_t ulMaxCheckInterval = DEFAULT_INTERVAL,
             uint32_t ulRestartServer = 0,
             uint32_t ulCoreDump = 0);

    int32_t reConfig(uint32_t ulMaxCheckInterval,
                     uint32_t ulRestartServer,
                     uint32_t ulCoreDump);
    // �ͷ���Դ
    void Destroy();

    // �߳�ע��
    int32_t RegistThread(CThread_Stat_Reporter* pReporter,const char* pszThreadName);

    // �߳�ȥע��
    int32_t UnregistThread(int32_t nThreadIndex);

    // �رռ���߳�
    int32_t close(u_long);

    // ����̴߳����߼�
    int32_t  svc(void);

    // �����߳�
    int32_t open(void *);
    void get_registered_thread_info(string& strThreadInfo) const;

public:
    // �����߳���Ϣ�ṹ������������ע����߳�
    typedef struct tagThreadInfo
    {
        int32_t            m_nThreadIndex;                        // �ڲ��߳����
        uint32_t  m_ulThreadID;                           // ϵͳ������߳�ID
        uint32_t  m_ulProcessNum;                       // �߳�ҵ�������
        char           m_szThreadName[MAX_THREAD_NAME + 1];  // �߳�����
        ACE_Time_Value m_tvStartTime;                        // �߳̿�ʼ����ʱ��
        ACE_Time_Value m_tvAliveTime;                        // �߳�������ʱ��
        CThread_Stat_Reporter* m_pReporter;
    }ThreadInfo;

    typedef std::list<int32_t>            FreeIndexList;
    typedef std::list<int32_t>::iterator  FreeIndexListIter;

private:
    CSVS_Daemon_Thread();

    volatile bool     m_nThreadFlag;                      // �߳��˳���־
    uint32_t     m_ulMaxThreadCheckInterval;        // �̼߳����
    uint32_t     m_ulRestartServer;                  // ������̱�־
    uint32_t     m_ulCoreDump;                        // CoreDump��־

    ThreadInfo        m_ThreadArray[MAX_THREAD_NUM];     // �߳����ݽṹ

    ACE_Thread_Mutex  m_FreeIndexListMutex;
    FreeIndexList     m_FreeIndexList;                   // ����ID�б�

    ACE_Time_Value    m_tvLastCheckTime;                 // ���һ�μ��ʱ��
    int32_t m_RegisteredThreadNum;
};

class CThread_Stat_Reporter
{
public:
    CThread_Stat_Reporter(const char *pszThreadName);

    virtual ~CThread_Stat_Reporter();

    ACE_Time_Value getLastReportTime()const;

    uint32_t getProcessNum()const;

    void ReportStat(uint32_t ulProcessNum = 1);

private:
    CThread_Stat_Reporter();

    int32_t             m_nThreadIndex; 

    ACE_Time_Value      m_tvLastReportTime; 

    uint32_t            m_ulProcessNum;
};

#endif /* __SVS_DEAMON_THREAD_H__ */



