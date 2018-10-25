#include "svs_ace_header.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_rtp_session.h"
#include "svs_adapter_service_task.h"
#include "svs_adapter_std_rtp_session.h"
#include "svs_adapter_rtmp_session.h"
#include "svs_adapter_ehome_session.h"
int32_t CSessionStatusTimer::handle_timeout(const ACE_Time_Value &tv, const void *arg)
{
    CMduSessionFactory::instance()->checkSessionStatus();

    return 0;
}


CMduSessionFactory* CMduSessionFactory::g_sessionFactory = NULL;

CMduSessionFactory::CMduSessionFactory()
{
    m_pStatusTimer = NULL;
    m_SessionIndex = 1; // the session id from 1
}

CMduSessionFactory::~CMduSessionFactory()
{
    m_pStatusTimer = NULL;
}

int32_t CMduSessionFactory::init()
{
    m_SessionMap.clear();

    SVS_LOG((SVS_LM_INFO,"CMduSessionFactory::init success."));
    return RET_OK;
}

void CMduSessionFactory::close()
{
    if (m_SessionMap.size() > 0)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduSessionFactory::close fail, still has session in factory."));
        return;
    }

    stopStatusCheckTimer();
    SVS_LOG((SVS_LM_INFO,"CMduSessionFactory::close success."));
    return;
}

CMduSession* CMduSessionFactory::createSourceSession(std::string& strContent,
                                                PEER_TYPE unPeerType,
                                                SESSION_TYPE unSessionType,
                                                bool bLocalFlag)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    if (NULL == m_pStatusTimer)
    {
        startStatusCheckTimer();
    }

    uint64_svs sessionIndx = 0;
    CMduSession* pMduSession = NULL;
    STREAM_SESSION_ITER iter;
    CONTENT_SESSION_ITER coniter;


    sessionIndx = m_SessionIndex++;

    iter = m_SessionMap.find(sessionIndx);
    if (m_SessionMap.end() != iter)
    {
        pMduSession = iter->second;
        (void)pMduSession->addReference();
    }
    else
    {
        pMduSession = createConcreteSession(unPeerType,unSessionType, bLocalFlag);
        if (NULL != pMduSession)
        {
            pMduSession->setSessionId(sessionIndx);
            m_SessionMap.insert(std::make_pair(sessionIndx, pMduSession));
            m_ContentMap.insert(std::make_pair(strContent, sessionIndx));
        }
    }

    return pMduSession;
}

CMduSession* CMduSessionFactory::createSession(PEER_TYPE unPeerType,
                                                SESSION_TYPE unSessionType,
                                                bool bLocalFlag)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    if (NULL == m_pStatusTimer)
    {
        startStatusCheckTimer();
    }

    uint64_svs sessionIndx = m_SessionIndex++;
    CMduSession* pMduSession = NULL;
    STREAM_SESSION_ITER iter;


    iter = m_SessionMap.find(sessionIndx);
    if (m_SessionMap.end() != iter)
    {
        pMduSession = iter->second;
        (void)pMduSession->addReference();
    }
    else
    {

        pMduSession = createConcreteSession(unPeerType,unSessionType, bLocalFlag);
        if (NULL != pMduSession)
        {
            pMduSession->setSessionId(sessionIndx);
            m_SessionMap.insert(std::make_pair(sessionIndx, pMduSession));
        }
    }

    return pMduSession;
}


void CMduSessionFactory::releaseSession(CMduSession* &pMduSession)
{
    if (NULL == pMduSession)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduSessionFactory::releaseSession fail, session is null."));
        return;
    }

    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);

    if (pMduSession->decReference() > 0)
    {
        return;
    }

    STREAM_SESSION_ITER iter;
    iter = m_SessionMap.find(pMduSession->getStreamId());
    if (iter == m_SessionMap.end())
    {
        SVS_LOG((SVS_LM_ERROR,"CMduSessionFactory::releaseSession can't find session. stream id[%Q].",
            pMduSession->getStreamId()));
        return;
    }
    else
    {
        m_SessionMap.erase(iter);
    }

    std::string strContentID = pMduSession->getContentID();
    CONTENT_SESSION_MAP::iterator Contentiter = m_ContentMap.find(strContentID);

    while(Contentiter != m_ContentMap.end())
    {
        uint64_svs ulStreamID = Contentiter->second;
        if(ulStreamID == pMduSession->getStreamId())
        {
            m_ContentMap.erase(Contentiter);
            break;
        }
        ++Contentiter;
    }

    pMduSession->close();

    SVS_LOG((SVS_LM_INFO,"session factory release session[%Q] success.",
            pMduSession->getStreamId()));

    delete pMduSession;
    pMduSession = NULL;

    return;
}

void CMduSessionFactory::releaseSession(uint64_svs streamID)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);

    STREAM_SESSION_ITER iter = m_SessionMap.find(streamID);
    if (m_SessionMap.end() == iter)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduSessionFactory::releaseSession can't find session. stream id[%Q].",
                        streamID));
        return;
    }

    CMduSession *pMduSession = iter->second;
    if (pMduSession->decReference() > 0)
    {
        return;
    }

    m_SessionMap.erase(iter);

    std::string strContentID = pMduSession->getContentID();
    CONTENT_SESSION_MAP::iterator Contentiter = m_ContentMap.find(strContentID);

    while(Contentiter != m_ContentMap.end())
    {
        uint64_svs ulStreamID = Contentiter->second;
        if(ulStreamID == pMduSession->getStreamId())
        {
            m_ContentMap.erase(Contentiter);
            break;
        }
        ++Contentiter;
    }

    pMduSession->close();
    delete pMduSession;

    SVS_LOG((SVS_LM_INFO,"session factory release session[%Q] success.",
                    streamID));
    return;
}

CMduSession* CMduSessionFactory::findSession(uint64_svs streamID)
{
    CMduSession* pMduSession = NULL;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    STREAM_SESSION_ITER iter;
    iter = m_SessionMap.find(streamID);
    if (iter == m_SessionMap.end())
    {
        return NULL;
    }
    else
    {
        pMduSession = iter->second;
    }

    (void)pMduSession->addReference();

    return pMduSession;
}

CMduSession* CMduSessionFactory::findSession(std::string& strContent)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    uint64_svs sessionIndx = 0;
    CMduSession* pMduSession = NULL;
    STREAM_SESSION_ITER iter;
    CONTENT_SESSION_ITER coniter;

    coniter = m_ContentMap.find(strContent);
    if (m_ContentMap.end() != coniter)
    {
        sessionIndx = coniter->second;
    }
    else
    {
        return NULL;
    }

    iter = m_SessionMap.find(sessionIndx);
    if (iter == m_SessionMap.end())
    {
        return NULL;
    }
    else
    {
        pMduSession = iter->second;
    }

    (void)pMduSession->addReference();

    return pMduSession;
}


CMduSession* CMduSessionFactory::findSessionNotAddRef(uint64_svs streamID)
{
    CMduSession* pMduSession = NULL;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    STREAM_SESSION_ITER iter;
    iter = m_SessionMap.find(streamID);
    if (iter == m_SessionMap.end())
    {
        return NULL;
    }
    else
    {
        pMduSession = iter->second;
    }

    return pMduSession;
}

void CMduSessionFactory::getAllSession(CMduSessionList& sessionList)
{
    CMduSession* pMduSession = NULL;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    STREAM_SESSION_ITER iter;
    for (iter = m_SessionMap.begin(); iter != m_SessionMap.end(); ++iter)
    {
        pMduSession = iter->second;
        sessionList.push_back(pMduSession);
        (void)pMduSession->addReference();
    }

    return;
}

void CMduSessionFactory::getSessionCount
(
    uint32_t& inputNum,
    uint32_t& outputNum,
    uint32_t& bidirectionNum,
    uint32_t& totalNum
)
{
    inputNum = 0;
    outputNum = 0;
    bidirectionNum = 0;

    CMduSession* pMduSession = NULL;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    STREAM_SESSION_ITER iter;
    for (iter = m_SessionMap.begin(); iter != m_SessionMap.end(); ++iter)
    {
        pMduSession = iter->second;

        switch (pMduSession->getTransDirection())
        {
            case TRANS_DIRECTION_RECVONLY:
            {
                inputNum++;
                break;
            }
            case TRANS_DIRECTION_SENDONLY:
            {
                outputNum++;
                break;
            }
            case TRANS_DIRECTION_SENDRECV:
            {
                bidirectionNum++;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    totalNum = m_SessionMap.size();

    return;
}

void CMduSessionFactory::checkSessionStatus()
{
    CMduSession* pMduSession = NULL;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    STREAM_SESSION_ITER iter;
    for (iter = m_SessionMap.begin(); iter != m_SessionMap.end(); ++iter)
    {
        pMduSession = iter->second;
        pMduSession->checkSessionStatus();
    }

    return;
}


CMduSession* CMduSessionFactory::createConcreteSession(PEER_TYPE unPeerType,SESSION_TYPE unSessionType, bool bLocalFlag) const
{
    CMduSession *pSession = NULL;
    if (bLocalFlag)
    {
        if(RTSP_SESSION == unSessionType)
        {
            pSession = new CMduStdRtpSession();
        }
        else if(RTMP_SESSION == unSessionType)
        {
            pSession = new CMduRtmpSession();
        }
    }
    else
    {
        if(EHOME_SESSION == unSessionType)
        {
            pSession = new CMduEhomeSession();
        }
        else {
            pSession = new CMduRtpSession();
        }
        SVS_LOG((SVS_LM_INFO,"CMduSessionFactory ::create CMduRtpSession."));
    }

    if(pSession)
    {
        pSession->initSesssion(unPeerType);
    }
    return pSession;
}

void CMduSessionFactory::startStatusCheckTimer()
{
    ACE_Reactor* pReactor = CMduServiceTask::instance()->getTimerReactor();
    if (NULL == pReactor)
    {
        return;
    }

    try
    {
        m_pStatusTimer = new CSessionStatusTimer;
    }
    catch(...)
    {
        return;
    }


    ACE_Time_Value tv(MDU_STATUS_CHECK_INTERVAL, 0);
    int32_t TimerId = pReactor->schedule_timer(m_pStatusTimer, this, tv, tv);
    if (-1 == TimerId)
    {
        SVS_LOG((SVS_LM_WARNING,"start session status check timer fail."));
        return;
    }

    SVS_LOG((SVS_LM_INFO,"start session status check timer success, timer id[%d].", TimerId));
    return;
}

void CMduSessionFactory::stopStatusCheckTimer()
{
    if (NULL == m_pStatusTimer)
    {
        return;
    }

    ACE_Reactor* pReactor = CMduServiceTask::instance()->getTimerReactor();
    if (NULL == pReactor)
    {
        return;
    }

    (void)pReactor->cancel_timer(m_pStatusTimer);
    delete m_pStatusTimer;
    m_pStatusTimer = NULL;

    SVS_LOG((SVS_LM_INFO,"stop session status check timer success."));

    return;
}



