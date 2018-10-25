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

    /// ���Ҹ���������߳�,���̵߳ĸ��ؼ�1,�������߳�������
    uint32_t getLoadleastThread();

    /// �ͷ�����ʱ,�ͷ�Reactor��Ӧ���̸߳���
    void releaseThreadLoad(const ACE_Reactor *pReactor);

private:
    bool             m_bRunFlag;                     /// �߳����б�־

    uint32_t     m_unThreadCount;                /// �����̸߳���

    uint32_t     *m_pThreadLoadArray;            /// �̸߳����б�
    ACE_Thread_Mutex m_ThreadLoadMutex;              /// �̸߳����б���

    ACE_Reactor **   m_pReactorArray;                /// ��Ӧ������

    uint32_t     m_unThreadIndex;                /// �߳�������
    ACE_Thread_Mutex m_ThreadIndexMutex;             /// �߳���������
};

#endif // __CNetConnManager_H__
