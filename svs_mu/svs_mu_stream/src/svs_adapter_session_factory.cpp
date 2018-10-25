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
    CStreamSessionFactory::instance()->checkSessionStatus();

    return 0;
}


CStreamSessionFactory* CStreamSessionFactory::g_sessionFactory = NULL;

CStreamSessionFactory::CStreamSessionFactory()
{
    m_pStatusTimer = NULL;
    m_SessionIndex = 1; // the session id from 1
}

CStreamSessionFactory::~CStreamSessionFactory()
{
    m_pStatusTimer = NULL;
}

int32_t CStreamSessionFactory::init()
{
    m_SessionMap.clear();

    SVS_LOG((SVS_LM_INFO,"CStreamSessionFactory::init success."));
    return RET_OK;
}

void CStreamSessionFactory::close()
{
    if (m_SessionMap.size() > 0)
    {
        SVS_LOG((SVS_LM_ERROR,"CStreamSessionFactory::close fail, still has session in factory."));
        return;
    }

    stopStatusCheckTimer();
    SVS_LOG((SVS_LM_INFO,"CStreamSessionFactory::close success."));
    return;
}

CStreamSession* CStreamSessionFactory::createSourceSession(std::string& strContent,
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
    CStreamSession* pStreamSession = NULL;
    STREAM_SESSION_ITER iter;
    CONTENT_SESSION_ITER coniter;


    sessionIndx = m_SessionIndex++;

    iter = m_SessionMap.find(sessionIndx);
    if (m_SessionMap.end() != iter)
    {
        pStreamSession = iter->second;
        (void)pStreamSession->addReference();
    }
    else
    {
        pStreamSession = createConcreteSession(unPeerType,unSessionType, bLocalFlag);
        if (NULL != pStreamSession)
        {
            pStreamSession->setSessionId(sessionIndx);
            m_SessionMap.insert(std::make_pair(sessionIndx, pStreamSession));
            m_ContentMap.insert(std::make_pair(strContent, sessionIndx));
        }
    }

    return pStreamSession;
}

CStreamSession* CStreamSessionFactory::createSession(PEER_TYPE unPeerType,
                                                SESSION_TYPE unSessionType,
                                                bool bLocalFlag)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    if (NULL == m_pStatusTimer)
    {
        startStatusCheckTimer();
    }

    uint64_svs sessionIndx = m_SessionIndex++;
    CStreamSession* pStreamSession = NULL;
    STREAM_SESSION_ITER iter;


    iter = m_SessionMap.find(sessionIndx);
    if (m_SessionMap.end() != iter)
    {
        pStreamSession = iter->second;
        (void)pStreamSession->addReference();
    }
    else
    {

        pStreamSession = createConcreteSession(unPeerType,unSessionType, bLocalFlag);
        if (NULL != pStreamSession)
        {
            pStreamSession->setSessionId(sessionIndx);
            m_SessionMap.insert(std::make_pair(sessionIndx, pStreamSession));
        }
    }

    return pStreamSession;
}


void CStreamSessionFactory::releaseSession(CStreamSession* &pStreamSession)
{
    if (NULL == pStreamSession)
    {
        SVS_LOG((SVS_LM_ERROR,"CStreamSessionFactory::releaseSession fail, session is null."));
        return;
    }

    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);

    if (pStreamSession->decReference() > 0)
    {
        return;
    }

    STREAM_SESSION_ITER iter;
    iter = m_SessionMap.find(pStreamSession->getStreamId());
    if (iter == m_SessionMap.end())
    {
        SVS_LOG((SVS_LM_ERROR,"CStreamSessionFactory::releaseSession can't find session. stream id[%Q].",
            pStreamSession->getStreamId()));
        return;
    }
    else
    {
        m_SessionMap.erase(iter);
    }

    std::string strContentID = pStreamSession->getContentID();
    CONTENT_SESSION_MAP::iterator Contentiter = m_ContentMap.find(strContentID);

    while(Contentiter != m_ContentMap.end())
    {
        uint64_svs ulStreamID = Contentiter->second;
        if(ulStreamID == pStreamSession->getStreamId())
        {
            m_ContentMap.erase(Contentiter);
            break;
        }
        ++Contentiter;
    }

    pStreamSession->close();

    SVS_LOG((SVS_LM_INFO,"session factory release session[%Q] success.",
            pStreamSession->getStreamId()));

    delete pStreamSession;
    pStreamSession = NULL;

    return;
}

void CStreamSessionFactory::releaseSession(uint64_svs streamID)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);

    STREAM_SESSION_ITER iter = m_SessionMap.find(streamID);
    if (m_SessionMap.end() == iter)
    {
        SVS_LOG((SVS_LM_ERROR,"CStreamSessionFactory::releaseSession can't find session. stream id[%Q].",
                        streamID));
        return;
    }

    CStreamSession *pStreamSession = iter->second;
    if (pStreamSession->decReference() > 0)
    {
        return;
    }

    m_SessionMap.erase(iter);

    std::string strContentID = pStreamSession->getContentID();
    CONTENT_SESSION_MAP::iterator Contentiter = m_ContentMap.find(strContentID);

    while(Contentiter != m_ContentMap.end())
    {
        uint64_svs ulStreamID = Contentiter->second;
        if(ulStreamID == pStreamSession->getStreamId())
        {
            m_ContentMap.erase(Contentiter);
            break;
        }
        ++Contentiter;
    }

    pStreamSession->close();
    delete pStreamSession;

    SVS_LOG((SVS_LM_INFO,"session factory release session[%Q] success.",
                    streamID));
    return;
}

CStreamSession* CStreamSessionFactory::findSession(uint64_svs streamID)
{
    CStreamSession* pStreamSession = NULL;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    STREAM_SESSION_ITER iter;
    iter = m_SessionMap.find(streamID);
    if (iter == m_SessionMap.end())
    {
        return NULL;
    }
    else
    {
        pStreamSession = iter->second;
    }

    (void)pStreamSession->addReference();

    return pStreamSession;
}

CStreamSession* CStreamSessionFactory::findSession(std::string& strContent)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    uint64_svs sessionIndx = 0;
    CStreamSession* pStreamSession = NULL;
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
        pStreamSession = iter->second;
    }

    (void)pStreamSession->addReference();

    return pStreamSession;
}


CStreamSession* CStreamSessionFactory::findSessionNotAddRef(uint64_svs streamID)
{
    CStreamSession* pStreamSession = NULL;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    STREAM_SESSION_ITER iter;
    iter = m_SessionMap.find(streamID);
    if (iter == m_SessionMap.end())
    {
        return NULL;
    }
    else
    {
        pStreamSession = iter->second;
    }

    return pStreamSession;
}

void CStreamSessionFactory::getAllSession(CStreamSessionList& sessionList)
{
    CStreamSession* pStreamSession = NULL;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    STREAM_SESSION_ITER iter;
    for (iter = m_SessionMap.begin(); iter != m_SessionMap.end(); ++iter)
    {
        pStreamSession = iter->second;
        sessionList.push_back(pStreamSession);
        (void)pStreamSession->addReference();
    }

    return;
}

void CStreamSessionFactory::getSessionCount
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

    CStreamSession* pStreamSession = NULL;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    STREAM_SESSION_ITER iter;
    for (iter = m_SessionMap.begin(); iter != m_SessionMap.end(); ++iter)
    {
        pStreamSession = iter->second;

        switch (pStreamSession->getTransDirection())
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

void CStreamSessionFactory::checkSessionStatus()
{
    CStreamSession* pStreamSession = NULL;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_SessionMapMutex);
    STREAM_SESSION_ITER iter;
    for (iter = m_SessionMap.begin(); iter != m_SessionMap.end(); ++iter)
    {
        pStreamSession = iter->second;
        pStreamSession->checkSessionStatus();
    }

    return;
}


CStreamSession* CStreamSessionFactory::createConcreteSession(PEER_TYPE unPeerType,SESSION_TYPE unSessionType, bool bLocalFlag) const
{
    CStreamSession *pSession = NULL;
    if (bLocalFlag)
    {
        if(RTSP_SESSION == unSessionType)
        {
            pSession = new CStreamStdRtpSession();
        }
        else if(RTMP_SESSION == unSessionType)
        {
            pSession = new CStreamRtmpSession();
        }
    }
    else
    {
        if(EHOME_SESSION == unSessionType)
        {
            pSession = new CStreamEhomeSession();
        }
        else {
            pSession = new CStreamRtpSession();
        }
        SVS_LOG((SVS_LM_INFO,"CStreamSessionFactory ::create CStreamRtpSession."));
    }

    if(pSession)
    {
        pSession->initSesssion(unPeerType);
    }
    return pSession;
}

void CStreamSessionFactory::startStatusCheckTimer()
{
    ACE_Reactor* pReactor = CStreamServiceTask::instance()->getTimerReactor();
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


    ACE_Time_Value tv(STREAM_STATUS_CHECK_INTERVAL, 0);
    int32_t TimerId = pReactor->schedule_timer(m_pStatusTimer, this, tv, tv);
    if (-1 == TimerId)
    {
        SVS_LOG((SVS_LM_WARNING,"start session status check timer fail."));
        return;
    }

    SVS_LOG((SVS_LM_INFO,"start session status check timer success, timer id[%d].", TimerId));
    return;
}

void CStreamSessionFactory::stopStatusCheckTimer()
{
    if (NULL == m_pStatusTimer)
    {
        return;
    }

    ACE_Reactor* pReactor = CStreamServiceTask::instance()->getTimerReactor();
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



