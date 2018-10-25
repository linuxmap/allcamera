#ifndef __CMDUSERVICETASK_H__
#define __CMDUSERVICETASK_H__

#include <svs_ace_header.h>
#include "svs_adapter_scc_connector.h"
#include "svs_adapter_time.h"
#include "svs_adapter_debugger.h"

enum _enMduServiceThreads
{
    MAIN_REACTOR_THREAD    = 0,
    SCC_MSG_HANDLER_THREAD = 1,
    MEDIA_MSG_HANDLE_THREAD= 2,
    MDU_TIMER_THREAD       = 3,
    MDU_DEBUG_THREAD       = 4,

    MAX_SERVICE_THREAD
};

class CLoadReportTimer : public ACE_Event_Handler
{
public:
    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);
};

class CMduServiceTask : public ACE_Task<ACE_MT_SYNCH>
{
public:
    virtual ~CMduServiceTask();

    static CMduServiceTask *instance()
    {
        if (NULL == g_mduServiceTask)
        {
            try
            {
                g_mduServiceTask = new CMduServiceTask;
            }
            catch(...)
            {
            }
        }

        return g_mduServiceTask;
    }

    UTAPI int32_t openServiceTask();

    UTAPI void closeServiceTask();

    UTAPI int32_t svc();

    UTAPI int32_t sendMsgToSCC(const CMduSvsMessage* pMessage);

    UTAPI int32_t enqueueInnerMessage(ACE_Message_Block* pMsg);

    UTAPI ACE_Reactor*  getTimerReactor();

    UTAPI CMduSccConnector* getSccConnector();

    UTAPI void sendSysLoadReport() const;

    void sendBusinessReport();

    UTAPI void stopSessions();

private:
    typedef enum
    {
        TIMER_TYPE_LOAD_REPORT = 1,
        TIMER_TYPE_MAX
    } TIMER_TYPE;

    UTAPI int32_t startSysStat();
private:
    CMduServiceTask();

    UTAPI int32_t openAllMsgQueue();

    UTAPI void closeAllMsgQueue();

    UTAPI int32_t createServiceThreads();

    UTAPI uint32_t getThreadIndex();

    UTAPI int32_t createSccConnect();

    UTAPI void reactorLoopThread() const;

    UTAPI void sccMessageHandleThread();

    UTAPI void mduTimerManagerThread();

    UTAPI void innerMsgHandleThread();

    UTAPI int32_t  openDeubgger();

    UTAPI void debugThread();

    UTAPI void handleSccRecvedMessage(const ACE_Message_Block *pMsg)const;

    UTAPI void handleInnerMessage(const ACE_Message_Block *pMsg) const;
 private:
    static CMduServiceTask*         g_mduServiceTask;
    bool                            m_bRunFlag;

    CMduSccConnector*               m_pSccConnect;
    ACE_Message_Queue<ACE_SYNCH>    m_SccRecvQueue;
    ACE_Message_Queue<ACE_SYNCH>    m_MediaMsgQueue;

    uint32_t                        m_unThreadIndex;
    ACE_Thread_Mutex                m_ThreadIndexMutex;

    CMduDebugAcceptor               m_debugAcceptor;
    ACE_Reactor*                    m_pDebugReactor;
    ACE_Reactor*                    m_pTimerReactor;

    CLoadReportTimer*               m_pLoadReportTimer;
#ifdef UNITTEST
    friend class MockCMduServiceTask;
#endif
};

#endif // __CMDUSERVICETASK_H__
