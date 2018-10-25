#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_svs_def.h"
#include "svs_daemon_thread.h"

#include "svs_adapter_def.h"
#include "svs_adapter_net_conn_manager.h"

CNetConnManager::CNetConnManager()
{
    m_bRunFlag         = true;

    m_unThreadCount    = 0;
    m_pThreadLoadArray = NULL;
    m_pReactorArray    = NULL;

    m_unThreadIndex    = 0;
}


CNetConnManager::~CNetConnManager()
{
    m_bRunFlag         = true;

    m_unThreadCount    = 0;
    m_pThreadLoadArray = NULL;
    m_pReactorArray    = NULL;

    m_unThreadIndex    = 0;
}

int32_t CNetConnManager::openManager(uint32_t unThreadNum)
{
    if (0 == unThreadNum)
    {
        return RET_ERR_PARAM;
    }

    m_unThreadCount  = unThreadNum;

    SVS_LOG((SVS_LM_INFO,"open connect manager: thread num[%d].",
                    m_unThreadCount));

    int32_t nRet = createResources();
    if (RET_OK != nRet)
    {
        return RET_FAIL;
    }

    nRet = createServiceThreads();
    if (RET_OK != nRet)
    {
        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CNetConnManager::closeManager()
{
    m_bRunFlag  = false;
    (void)wait();

    ACE_Guard<ACE_Thread_Mutex> locker(m_ThreadLoadMutex);
    if (NULL != m_pThreadLoadArray)
    {
        delete[] m_pThreadLoadArray;
        m_pThreadLoadArray = NULL;
    }

    if (NULL != m_pReactorArray)
    {
        for (uint32_t i = 0; i < m_unThreadCount; i++)
        {
            if (NULL != m_pReactorArray[i])
            {
                delete m_pReactorArray[i];
                m_pReactorArray[i]  = NULL;
            }
        }

        delete[] m_pReactorArray;
        m_pReactorArray = NULL;
    }

    return RET_OK;
}

int32_t CNetConnManager::svc()
{
    (void)signal(SIGPIPE, SIG_IGN);

    uint32_t unThreadIndex = getThreadIndex();
    SVS_LOG((SVS_LM_INFO,"media connect manager thread[%d] start.", unThreadIndex));

    if ((NULL == m_pReactorArray) || (NULL == m_pReactorArray[unThreadIndex]))
    {
        SVS_LOG((SVS_LM_WARNING,"media connect manager thread[%d] exit, resource not created.",
                        unThreadIndex));
        return RET_FAIL;
    }

    (void)m_pReactorArray[unThreadIndex]->owner(ACE_OS::thr_self());
    (void)m_pReactorArray[unThreadIndex]->restart(true);

    ACE_Time_Value delaytime;
    CThread_Stat_Reporter report("NetConnectManagerThread");
    while (m_bRunFlag)
    {
        delaytime.set(0, MDU_MAX_TIME_PER_THREAD);
        if (m_pReactorArray[unThreadIndex]->handle_events(&delaytime) < 0)
        {
            report.ReportStat();
            continue;
        }

        report.ReportStat();
    }

    SVS_LOG((SVS_LM_INFO,"media connect manager thread[%d] exit.", unThreadIndex));
    return RET_OK;
}

int32_t CNetConnManager::registHandle(ACE_Event_Handler *pHandle, uint32_t mask)
{
    if ((NULL == pHandle) || (NULL == m_pReactorArray))
    {
        return RET_FAIL;
    }

    uint32_t unIndex = getLoadleastThread();
    if ((unIndex >= m_unThreadCount) || (NULL == m_pReactorArray[unIndex]))
    {
        return RET_FAIL;
    }

    int32_t nRet = m_pReactorArray[unIndex]->register_handler(pHandle->get_handle(),
                                                         pHandle,
                                                         mask);
    if (RET_OK != nRet)
    {
        releaseThreadLoad(m_pReactorArray[unIndex]);
        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CNetConnManager::unRegistHandle(const ACE_Event_Handler *pHandle, uint32_t mask)
{
    if ((NULL == pHandle) || (NULL == m_pReactorArray))
    {
        return RET_FAIL;
    }

    ACE_Reactor *pReactor = pHandle->reactor();
    if (NULL == pReactor)
    {
        return RET_FAIL;
    }

    releaseThreadLoad(pReactor);

    return pReactor->remove_handler(pHandle->get_handle(), mask);
}

int32_t CNetConnManager::createResources()
{
    try
    {
        m_pThreadLoadArray = new uint32_t[m_unThreadCount];
    }
    catch(...)
    {
        return RET_ERR_SYS_NEW;
    }
    memset(m_pThreadLoadArray, 0x0, sizeof(uint32_t) * m_unThreadCount);

    try
    {
        m_pReactorArray    = new ACE_Reactor*[m_unThreadCount];
    }
    catch(...)
    {
        return RET_ERR_SYS_NEW;
    }

    if (NULL == m_pReactorArray)
    {
        return RET_FAIL;
    }

    memset(m_pReactorArray, 0x0, sizeof(ACE_Reactor*) * m_unThreadCount);

    for (uint32_t i = 0; i < m_unThreadCount; i++)
    {
        ACE_Reactor_Impl *pImpl = NULL;
        try
        {
            pImpl = new ACE_Dev_Poll_Reactor(MDU_MAX_EPOLL_SIZE);
        }
        catch(...)
        {
            delete pImpl;
            pImpl = NULL;
            return RET_ERR_SYS_NEW;
        }

        try
        {
            m_pReactorArray[i] = new ACE_Reactor(pImpl, true);
        }
        catch(...)
        {
            delete pImpl;
            pImpl = NULL;
            return RET_ERR_SYS_NEW;
        }
    }//lint !e429

    return RET_OK;
}

int32_t CNetConnManager::createServiceThreads()
{
    size_t *pStackSize = NULL;
    try
    {
        pStackSize = new size_t[m_unThreadCount];
    }
    catch(...)
    {
        delete[] pStackSize;

        SVS_LOG((SVS_LM_CRITICAL,"create media connect manager threads fail, thread num[%d], retcode[%d].",
                        m_unThreadCount,
                        errno));
        return RET_ERR_SYS_NEW;
    }

    if (NULL == pStackSize)
    {
        return RET_FAIL;
    }

    for (uint32_t i = 0; i < m_unThreadCount; i++)
    {
        pStackSize[i] = DEFAULT_THREAD_STACK_SIZE;
    }

    int32_t nRet = activate(THR_NEW_LWP  | THR_JOINABLE,
                        (int32_t)m_unThreadCount,
                        0,
                        ACE_DEFAULT_THREAD_PRIORITY,
                        -1,
                        0,
                        0,
                        0,
                        pStackSize,
                        0 );

    delete[] pStackSize;
    pStackSize = NULL;

    if (-1 == nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"create media connect manager threads fail, thread num[%d], retcode[%d].",
                        m_unThreadCount,
                        errno));

        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"create media connect manager threds success, thread num[%d].",
                    m_unThreadCount));
    return RET_OK;
}

uint32_t CNetConnManager::getThreadIndex()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ThreadIndexMutex);
    return m_unThreadIndex++;
}


uint32_t CNetConnManager::getLoadleastThread()
{
    if ((NULL == m_pThreadLoadArray) || (NULL == m_pReactorArray))
    {
        return 0;
    }

    uint32_t unThreadIndex = 0;
    uint32_t unThreadLoad  = m_pThreadLoadArray[0];
    ACE_Guard<ACE_Thread_Mutex> locker(m_ThreadLoadMutex);
    for (uint32_t i = 0; i < m_unThreadCount; i++)
    {
        if (0 == m_pThreadLoadArray[i])
        {
            unThreadIndex = i;
            break;
        }

        if (unThreadLoad > m_pThreadLoadArray[i])
        {
            unThreadLoad  = m_pThreadLoadArray[i];
            unThreadIndex = i;
        }
    }

    m_pThreadLoadArray[unThreadIndex]++;
    return unThreadIndex;
}

void CNetConnManager::releaseThreadLoad(const ACE_Reactor *pReactor)
{
    if ((NULL == m_pThreadLoadArray) || (NULL == m_pReactorArray))
    {
        return;
    }

    ACE_Guard<ACE_Thread_Mutex> locker(m_ThreadLoadMutex);
    for (uint32_t i = 0; i < m_unThreadCount; i++)
    {
        if (pReactor == m_pReactorArray[i])
        {
            if(0 == m_pThreadLoadArray[i])
            {
                SVS_LOG((SVS_LM_WARNING,"release thread [%d] load fail.", i));
                return;
            }

            m_pThreadLoadArray[i]--;
            return;
        }
    }

    return;
}

