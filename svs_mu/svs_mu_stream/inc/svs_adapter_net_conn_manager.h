#ifndef __CNetConnManager_H__
#define __CNetConnManager_H__

#include <svs_ace_header.h>

class CNetConnManager : public ACE_Task<ACE_MT_SYNCH>
{
public:
    virtual ~CNetConnManager();

    static CNetConnManager * instance()
    {
        static CNetConnManager netconnManager;
        return &netconnManager;
    }

    int32_t openManager(uint32_t unThreadNum);

    int32_t closeManager();

    int32_t svc();

    int32_t registHandle(ACE_Event_Handler *pHandle, uint32_t mask);

    int32_t unRegistHandle(const ACE_Event_Handler *pHandle, uint32_t mask);

private:
    CNetConnManager();

    int32_t createResources();

    int32_t createServiceThreads();

    uint32_t getThreadIndex();

    /// 查找负载最轻的线程,把线程的负载加1,并返回线程索引号
    uint32_t getLoadleastThread();

    /// 释放连接时,释放Reactor对应的线程负载
    void releaseThreadLoad(const ACE_Reactor *pReactor);

private:
    bool             m_bRunFlag;                     /// 线程运行标志

    uint32_t     m_unThreadCount;                /// 运行线程个数

    uint32_t     *m_pThreadLoadArray;            /// 线程负载列表
    ACE_Thread_Mutex m_ThreadLoadMutex;              /// 线程负载列表锁

    ACE_Reactor **   m_pReactorArray;                /// 反应器数组

    uint32_t     m_unThreadIndex;                /// 线程索引号
    ACE_Thread_Mutex m_ThreadIndexMutex;             /// 线程索引号锁
};

#endif // __CNetConnManager_H__
