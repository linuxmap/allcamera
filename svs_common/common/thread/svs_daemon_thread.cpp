/*****************************************************************************

 ******************************************************************************
  �ļ���          : SVS_Daemon_Thead.cpp
  �汾��          : 1.0
  ����            :
  �������        : 2009-6-18
  ����޸�        :
  ��������        : ҵ���̵߳ļ���̣߳����ڼ��ҵ���̵߳Ļ״̬��������Ҫ���
                    ��ҵ���߳�������ʱ�����򱾼���߳�ע�ᣬ�����������ҵ����
                    ��ÿ����һ��ҵ���һ���ϱ����ڵ���ʱ�򣬻������߳��ϱ�һ��
                    �Լ��Ļ״̬��һ��������ڵ��󣬼���̱߳����������Լ����
                    ���̣߳������ĳ�߳��Ѿ�һ���������û���ϱ��Լ���״̬�ˣ�
                    ��������߳̽�������������RtRecordSvr��һ��һ��������ڴ�С
                    ����3���ϱ�����
  �����б�        :
  �޸���ʷ        :
  1 ����          : 2009-6-18
    ����          :
    �޸�����      : ���
 *******************************************************************************/
#include "svs_ace_header.h"
#include "svs_log_msg.h"
#include "svs_daemon_thread.h"


CSVS_Daemon_Thread::CSVS_Daemon_Thread()
{
    m_ulMaxThreadCheckInterval = DEFAULT_THREAD_CHECK_INTERVAL;
    m_ulRestartServer          = 0;
    m_ulCoreDump               = 0;
    m_nThreadFlag             = false;
    m_tvLastCheckTime         = ACE_Time_Value::zero;
    m_RegisteredThreadNum     = 0;
}

CSVS_Daemon_Thread::~CSVS_Daemon_Thread()
{
    try
    {
        m_FreeIndexList.clear();
    }
    catch(...)
    {
    }
}

/*****************************************************************************
 �� �� ��  : Init
 ��������  : ��ʼ������ʼ��������ڣ���ʼ���߳���Ϣ�б�
 �������  : �������
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Daemon_Thread::Init(uint32_t ulMaxCheckInterval,
                             uint32_t ulRestartServer,
                             uint32_t ulCoreDump)
{
    m_tvLastCheckTime          = ACE_Time_Value::zero;
    m_nThreadFlag              = false;
    m_ulMaxThreadCheckInterval = ulMaxCheckInterval;
    m_ulRestartServer          = ulRestartServer;
    m_ulCoreDump               = ulCoreDump;

    // ������Ч���ж�
    if ((MIN_INTERVAL > m_ulMaxThreadCheckInterval)
          || (MAX_INTERVAL < m_ulMaxThreadCheckInterval))
    {
        m_ulMaxThreadCheckInterval = DEFAULT_INTERVAL;
    }

    // ��ʼ���߳���Ϣ�ṹ������ID�б�
    ThreadInfo *  pReporter = NULL;
    ACE_Guard<ACE_Thread_Mutex> locker(m_FreeIndexListMutex);
    for (int32_t i = 0; i < MAX_THREAD_NUM; i++)
    {
        pReporter = m_ThreadArray + i;
        pReporter->m_ulThreadID      = 0;
        pReporter->m_nThreadIndex    = 0;
        pReporter->m_ulProcessNum    = 0;
        memset(pReporter->m_szThreadName, 0x0, MAX_THREAD_NAME + 1);
        pReporter->m_pReporter = NULL;
        m_FreeIndexList.push_back(i);
    }

    // ��������߳�
    int32_t nRet = open(NULL);
    if (0 != nRet)
    {
        SVS_LOG((SVS_LM_ERROR, "[deamon thread]Init SVS_Daemon_Thread fail."));
        return nRet;
    }

    SVS_LOG((SVS_LM_INFO, "[deamon thread]Init SVS_Daemon_Thread success."));
    return 0;
}

/*****************************************************************************
 �� �� ��  : reConfig
 ��������  : �Լ�����ý���������
 �������  : ������ڣ��Ƿ��������������Ƿ�CoreDump���
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Daemon_Thread::reConfig(uint32_t ulMaxCheckInterval,
                                     uint32_t ulRestartServer,
                                     uint32_t ulCoreDump)
{
    SVS_LOG((SVS_LM_INFO, "CSVS_Daemon_Thread reconfig: check interval[%d:%d]"
               " restart[%d:%d] core dump[%d:%d]",
               m_ulMaxThreadCheckInterval, ulMaxCheckInterval,
               m_ulRestartServer, ulRestartServer,
               m_ulCoreDump, ulCoreDump));

    // ������Ч���ж�
    if ((MIN_INTERVAL > m_ulMaxThreadCheckInterval)
          || (MAX_INTERVAL < m_ulMaxThreadCheckInterval))
    {
        m_ulMaxThreadCheckInterval = DEFAULT_INTERVAL;
    }

    m_ulMaxThreadCheckInterval = ulMaxCheckInterval;
    m_ulRestartServer          = ulRestartServer;
    m_ulCoreDump               = ulCoreDump;

    return 0;
}

/*****************************************************************************
 �� �� ��  : Destroy
 ��������  : �ͷ���Դ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
void CSVS_Daemon_Thread::Destroy()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_FreeIndexListMutex);
    m_FreeIndexList.clear();

    (void) close(0);
    SVS_LOG((SVS_LM_INFO, "[deamon thread]Destroy SVS_Daemon_Thread success."));
}

/*****************************************************************************
 �� �� ��  : RegistThread
 ��������  : ҵ���߳������߳�ע���Լ�
 �������  : ҵ���̵߳����
 �������  : NA
 �� �� ֵ  : �ɹ�,���� ����̷߳����ID,Ҳ���߳���Ϣ�ṹ���±�ֵ �� ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Daemon_Thread::RegistThread(CThread_Stat_Reporter* pReporter,const char* pszThreadName)
{
    if((NULL == pReporter) || (NULL == pszThreadName))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Thread Regist to Daemon failed.The parameter is invalid."
            "pReporter[0x%08x] pszThreadName[0x%08x]",
            pReporter,
            pszThreadName));

        return -1;
    }
    int32_t nThreadIndex = 0;
    {
        ACE_Guard<ACE_Thread_Mutex> locker(m_FreeIndexListMutex);
        if (m_FreeIndexList.empty())
        {
            return -1;
        }

        ++m_RegisteredThreadNum;

        nThreadIndex = m_FreeIndexList.front();
        m_FreeIndexList.pop_front();
    }

    // ����ҵ���߳���Ϣ
    m_ThreadArray[nThreadIndex].m_ulThreadID   = ACE_OS::thr_self();
    m_ThreadArray[nThreadIndex].m_nThreadIndex = nThreadIndex;
    m_ThreadArray[nThreadIndex].m_ulProcessNum = 0;
    (void)strncpy(m_ThreadArray[nThreadIndex].m_szThreadName,
                  pszThreadName,
                  MAX_THREAD_NAME);
    m_ThreadArray[nThreadIndex].m_tvStartTime = ACE_OS::gettimeofday();
    m_ThreadArray[nThreadIndex].m_tvAliveTime = m_ThreadArray[nThreadIndex].m_tvStartTime;
    m_ThreadArray[nThreadIndex].m_pReporter = pReporter;

    SVS_LOG((SVS_LM_INFO, "[deamon thread]regist thread[%s:%u], index[%d],pReporter[0x%08x]",
              m_ThreadArray[nThreadIndex].m_szThreadName,
              m_ThreadArray[nThreadIndex].m_ulThreadID,
              m_ThreadArray[nThreadIndex].m_nThreadIndex,
              pReporter));
    return nThreadIndex;
}

/*****************************************************************************
 �� �� ��  : RegistThread
 ��������  : ҵ���߳������߳�ȥע���Լ���֮�����ɼ���̹߳���
 �������  : ��ע��ʱ����̸߳�ҵ���̷߳����IDֵ
 �������  : NA
 �� �� ֵ  : �ɹ�,���� ����̷߳����ID,Ҳ���߳���Ϣ�ṹ���±�ֵ �� ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Daemon_Thread::UnregistThread(int32_t nThreadIndex)
{
    // ������Ч���ж�
    if ((0 > nThreadIndex)
         || (MAX_THREAD_NUM <= nThreadIndex))
    {
        return -1;
    }

    // ID���
    int32_t nIndex = m_ThreadArray[nThreadIndex].m_nThreadIndex;
    if (nIndex != nThreadIndex)
    {
        return -1;
    }

    SVS_LOG((SVS_LM_INFO, "[deamon thread]unregist thread[%s:%u], index[%d]",
              m_ThreadArray[nThreadIndex].m_szThreadName,
              m_ThreadArray[nThreadIndex].m_ulThreadID,
              m_ThreadArray[nThreadIndex].m_nThreadIndex));

    // ����߳���Ϣ
    m_ThreadArray[nThreadIndex].m_ulThreadID   = 0;
    m_ThreadArray[nThreadIndex].m_nThreadIndex = 0;
    m_ThreadArray[nThreadIndex].m_ulProcessNum = 0;
    memset(m_ThreadArray[nThreadIndex].m_szThreadName, 0x0, MAX_THREAD_NAME + 1);
    m_ThreadArray[nThreadIndex].m_tvStartTime  = ACE_Time_Value::zero;
    m_ThreadArray[nThreadIndex].m_tvAliveTime  = ACE_Time_Value::zero;
    m_ThreadArray[nThreadIndex].m_pReporter = NULL;

    // ��Thread index�黹��δʹ���б�
    ACE_Guard<ACE_Thread_Mutex> locker(m_FreeIndexListMutex);
    m_FreeIndexList.push_back(nThreadIndex);

    --m_RegisteredThreadNum;
    return 0;
}

/*****************************************************************************
 �� �� ��  : open
 ��������  : ��������߳�
 �������  : void *args
 �������  : NA
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Daemon_Thread::open(void *args)
{
    if (NULL == args)
    {
       ; // only for compile warning
    }

    // ����һ��deamon�̣߳�ÿ��һ��ʱ��������ע���������߳��Ƿ�����
    size_t stack_size = 128 * 1024;
    int32_t nRet = activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED,
                        1,
                        0,
                        ACE_DEFAULT_THREAD_PRIORITY,
                        -1,
                        0,
                        0,
                        0,
                        &stack_size,
                        0 );
    if (0 != nRet)
    {
        SVS_LOG((SVS_LM_ERROR, "[deamon thread]open deamon thread fail."));
    }

    return nRet;
}

/*****************************************************************************
 �� �� ��  : svc
 ��������  : ����̣߳���������ע�����ܼ�ص�ҵ���̣߳����ĳ�߳��Ѿ�һ������û
             �и����Լ������ʱ���ˣ���������̹߳���������������Rt����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Daemon_Thread::svc(void)
{
    ACE_thread_t threadID = ACE_OS::thr_self();
    SVS_LOG((SVS_LM_INFO, "[deamon thread]deamon thread[%u] run.", threadID));

    ACE_Time_Value delay(1);
    ACE_Time_Value curtime;
    ThreadInfo *pThreadInfo = NULL;
    m_tvLastCheckTime         = ACE_OS::gettimeofday();

    while (!m_nThreadFlag)
    {
        ACE_Time_Value maxinterval((int32_t)m_ulMaxThreadCheckInterval);
        // ÿ��1min���һ�������߳��Ƿ���
        for (int32_t i = 0; i < DEFAULT_THREAD_CHECK_INTERVAL; i++)
        {
             // Ϊ��ֹsleepʱ�����ܼ�ʱ��Ӧ�˳���Ϣ������ֻsleep 1��
             (void)ACE_OS::sleep(delay);

             if (m_nThreadFlag)
             {
                 return 0;
             }
        }

        // �鿴ϵͳʱ���Ƿ���
        curtime = ACE_OS::gettimeofday();
        if ((curtime < m_tvLastCheckTime)
            || (curtime - m_tvLastCheckTime > maxinterval))
        {
            SVS_LOG((SVS_LM_WARNING, "[deamon thread]systime is abnormal."));
            // ϵͳʱ������쳣������ʱ�䷢�����,���μ��,���̺߳�����Լ�����ʱ��
            m_tvLastCheckTime = curtime;   // ���¼��ʱ�䣬�ȴ���һ��״̬���
            continue;
        }

        // �������ע����߳��Ƿ�������
        SVS_LOG((SVS_LM_INFO, "[deamon thread]deamon thread begin"));
        ACE_Time_Value tvLastReportInterval(DEFAULT_THREAD_CHECK_INTERVAL);

        for (int32_t nIndex = 0; nIndex < MAX_THREAD_NUM; nIndex++)
        {
            pThreadInfo = &m_ThreadArray[nIndex];

            // IDΪ0��ʾδ��ʹ��
            if ((0 == pThreadInfo->m_ulThreadID) || (NULL == pThreadInfo->m_pReporter))
            {
                continue;
            }

            ACE_Time_Value lastReportTime = pThreadInfo->m_pReporter->getLastReportTime();
            SVS_LOG((SVS_LM_INFO,
                "[deamon thread]Check thread active status."
                "thread[%s:%u], index[%d],last report at last check period, thread last report[%d].",
                pThreadInfo->m_szThreadName,
                pThreadInfo->m_ulThreadID,
                pThreadInfo->m_nThreadIndex,
                pThreadInfo->m_tvAliveTime.sec(),
                lastReportTime.sec()));

            // ��ȡ���µĸ���ʱ��
            pThreadInfo->m_tvAliveTime = lastReportTime;
            pThreadInfo->m_ulProcessNum = pThreadInfo->m_pReporter->getProcessNum();
            // ����߳��ڱ��μ��������û�и���״̬���澯
            if (curtime - pThreadInfo->m_tvAliveTime > tvLastReportInterval)
            {
                SVS_LOG((SVS_LM_WARNING, "[deamon thread]thread[%s:%u], index[%d] has"
                               " not update stat 60s, last report[%d].",
                               pThreadInfo->m_szThreadName,
                               pThreadInfo->m_ulThreadID,
                               pThreadInfo->m_nThreadIndex,
                               pThreadInfo->m_tvAliveTime.sec()));
            }

            // �����߳��Ѿ��ܳ�ʱ��δ��������״̬������������
            if (curtime - pThreadInfo->m_tvAliveTime > maxinterval)
            {
                if (ACE_Time_Value::zero != pThreadInfo->m_tvAliveTime)
                {
                    SVS_LOG((SVS_LM_CRITICAL, "[deamon thread]thread[%s:%u], index[%d] has"
                               " not update stat too long time, last report[%d],"
                               "restart flag[%d] dump flag[%d].",
                               pThreadInfo->m_szThreadName,
                               pThreadInfo->m_ulThreadID,
                               pThreadInfo->m_nThreadIndex,
                               pThreadInfo->m_tvAliveTime.sec(),
                               m_ulRestartServer,
                               m_ulCoreDump));

                    (void)system("dmesg -c");
                    (void)system("echo \"1\" >/proc/sys/kernel/sysrq");
                    (void)system("echo \"m\" >/proc/sysrq-trigger");
                    (void)system("dmesg");

                    if (1 <= m_ulRestartServer)
                    {
                        if (1 <= m_ulCoreDump)
                        {
                            (void)kill(getpid(), SIGABRT);
                        }
                        else
                        {
                            (void)kill(getpid(), SIGKILL);
                        }
                    }
                }
            }
        }

        SVS_LOG((SVS_LM_INFO, "[deamon thread]deamon thread end"));
        // ���¼��ʱ�䣬�ȴ���һ��״̬���
        m_tvLastCheckTime = curtime;
    }

    SVS_LOG((SVS_LM_INFO, "[deamon thread]deamon thread[%u] exit.", threadID));
    return 0;
}

/*****************************************************************************
 �� �� ��  : close
 ��������  : �ü���߳����б�ʾΪfalse���ü���߳��˳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Daemon_Thread::close(u_long flags)
{
    m_nThreadFlag = true;
    (void)wait();
    SVS_LOG((SVS_LM_INFO, "[deamon thread]close deamon thread, flags[%d]", flags));
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// function : get_registered_thread_info
// desc     : ��nru����������л�ȡnru��ص���Ϣ.
// input    : strThreadInfo:string&
// output   : strThreadInfo:string&
// calls    :
// called by:
///////////////////////////////////////////////////////////////////////////////
void CSVS_Daemon_Thread::get_registered_thread_info(string& strThreadInfo) const
{
    const ThreadInfo *pThreadInfo = NULL;
    int32_t nThreadIndex = 0;
    char    csBuffer[COMMON_BUFFER_SIZE] = {0};
    int32_t nThreadNum = 0;

    for (nThreadIndex = 0; nThreadIndex < MAX_THREAD_NUM; nThreadIndex++)
    {
        pThreadInfo = &m_ThreadArray[nThreadIndex];

        // IDΪ0��ʾδ��ʹ��
        if (0 == pThreadInfo->m_ulThreadID)
        {
            continue;
        }
        ++nThreadNum;
        (void)snprintf(csBuffer, sizeof(csBuffer),
                 "Index:%5d ThreadName:%s\t ThreadId:%lu starttime:%d alivetime:%d\n",
                 m_ThreadArray[nThreadIndex].m_nThreadIndex,
                 m_ThreadArray[nThreadIndex].m_szThreadName,
                 m_ThreadArray[nThreadIndex].m_ulThreadID,
                 (int32_t)m_ThreadArray[nThreadIndex].m_tvStartTime.sec(),
                 (int32_t)m_ThreadArray[nThreadIndex].m_tvAliveTime.sec());
        strThreadInfo += csBuffer;
    }

    (void)snprintf(csBuffer, sizeof(csBuffer),
             "Total thread num:%d real num(got):%d \n",
             m_RegisteredThreadNum, nThreadNum);
             //SVS_SCHEDULER::instance()->get_signal_task().total_thread_num()
             //+ THREAD_NUM_EXCEPT_TASK);
    strThreadInfo += csBuffer;
}




CThread_Stat_Reporter::CThread_Stat_Reporter()
{
    m_ulProcessNum       = 0;
    m_nThreadIndex       = -1;
    m_tvLastReportTime   = ACE_Time_Value::zero;
}

/*****************************************************************************
 �� �� ��  : CThread_Stat_Reporter
 ��������  : ҵ���̴߳����ö��󣬲������߳�ע���Լ�
 �������  : const char *pszThreadName:ҵ���߳����
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
CThread_Stat_Reporter::CThread_Stat_Reporter(const char *pszThreadName)
{
    m_ulProcessNum       = 0;
    m_tvLastReportTime   = ACE_Time_Value::zero;

    // �����߳�ע���Լ�
    m_nThreadIndex       = CSVS_Daemon_Thread::instance()->RegistThread(this,pszThreadName);
    SVS_LOG((SVS_LM_INFO, "CThread_Stat_Reporter::CThread_Stat_Reporter(), index[%d]",
                  m_nThreadIndex));
}

CThread_Stat_Reporter::~CThread_Stat_Reporter()
{
    try
    {
        (void)CSVS_Daemon_Thread::instance()->UnregistThread(m_nThreadIndex);
        SVS_LOG((SVS_LM_INFO, "CThread_Stat_Reporter::~CThread_Stat_Reporter(), index[%d]",
                  m_nThreadIndex));
    }
    catch(...)
    {
    }
}

/*****************************************************************************
 �� �� ��  : ReportStat
 ��������  : �����߳��ϱ��Լ���ҵ��״̬
 �������  : uint32_t ulProcessNum:ҵ�������
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CThread_Stat_Reporter::ReportStat(uint32_t ulProcessNum)
{
    ACE_Time_Value curtime = ACE_OS::gettimeofday();
    m_tvLastReportTime = curtime;
    m_ulProcessNum += ulProcessNum ;
}

/*****************************************************************************
 �� �� ��  : getLastReportTime
 ��������  : ��ȡ�߳�������ʱ��
 �������  : NA
 �������  : NA
 �� �� ֵ  : ACE_Time_Value:�߳�������ʱ��
 �޸���ʷ  :
*****************************************************************************/
ACE_Time_Value CThread_Stat_Reporter::getLastReportTime()const
{
    return m_tvLastReportTime;
}

/*****************************************************************************
 �� �� ��  : getProcessNum
 ��������  : ��ȡ�̴߳���ҵ����
 �������  : NA
 �������  : NA
 �� �� ֵ  : uint32_t:�̴߳���ҵ����
 �޸���ʷ  :
*****************************************************************************/
uint32_t CThread_Stat_Reporter::getProcessNum()const
{
    return m_ulProcessNum;
}

