/*
 * StreamBusinessManager.cpp
 *
 *  Created on: 2016-1-21
 *      Author:
 */

#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"

#include "svs_adapter_config.h"
#include "svs_adapter_business_manager.h"
#include "svs_adapter_service_task.h"

int32_t CSessionFluxTimer::handle_timeout(const ACE_Time_Value &tv, const void *arg)
{
    CStreamBusinessManager::instance()->statFlux();

    return 0;
}


CStreamBusinessManager* CStreamBusinessManager::m_pStreamBusinessManager = NULL;

CStreamBusinessManager::CStreamBusinessManager()
{
    m_unMaxBusinessNum = 0;
    m_InputRate   = 0;
    m_OutputRate  = 0;
    m_pFluxTimer  = NULL;
}

CStreamBusinessManager::~CStreamBusinessManager()
{
}

CStreamBusinessManager* CStreamBusinessManager::instance()
{
    if (NULL == m_pStreamBusinessManager)
    {
        try
        {
            m_pStreamBusinessManager = new CStreamBusinessManager;
        }
        catch (...)
        {
        }
    }

    return m_pStreamBusinessManager;
}

int32_t CStreamBusinessManager::initManager()
{
    m_BusinessMap.clear();
    SVS_LOG((SVS_LM_INFO,"init stream business manager success."));
    return RET_OK;
}

void CStreamBusinessManager::closeManager()
{
    stopFluxTimer();

    m_BusinessMap.clear();
    SVS_LOG((SVS_LM_INFO,"close stream business manager success."));
    return;
}

CStreamBusiness* CStreamBusinessManager::createBusiness(uint64_svs recvSessionId,
                                                  uint64_svs sendSessionId,
                                                  PLAY_TYPE enPlayType)
{
    CStreamBusiness *pBusiness = NULL;
    try
    {
        pBusiness = new CStreamBusiness;
    }
    catch(...)
    {
        return NULL;
    }

    if (RET_OK != pBusiness->init(recvSessionId, sendSessionId, enPlayType))
    {
        SVS_LOG((SVS_LM_WARNING,"business manager init business fail."));
        delete pBusiness;
        return NULL;
    }

    {
        ACE_Guard<ACE_Thread_Mutex> locker(m_BusinessMapMutex);
        if (NULL == m_pFluxTimer)
        {
            startFluxTimer();
        }

        if (m_unMaxBusinessNum >= CStreamConfig::instance()->getServiceCapacity())
        {
            SVS_LOG((SVS_LM_WARNING,"business manager create business fail, "
                    "business count[%u] up to service capacity.",
                    m_unMaxBusinessNum));
            (void)pBusiness->stop();
            delete pBusiness;
            return NULL;
        }

        m_BusinessMap.insert(std::make_pair(recvSessionId, pBusiness));
        m_BusinessMap.insert(std::make_pair(sendSessionId, pBusiness));

        m_unMaxBusinessNum++;
    }

    return pBusiness;
}

CStreamBusiness* CStreamBusinessManager::findBusiness(uint64_svs streamID)
{
    CStreamBusiness *pBusiness = NULL;
    ACE_Guard<ACE_Thread_Mutex> locker(m_BusinessMapMutex);
    STREAM_BUSINESS_MAP_ITER  iter = m_BusinessMap.find(streamID);
    if (m_BusinessMap.end() == iter)
    {
        return NULL;
    }

    pBusiness = iter->second;
    (void)pBusiness->addReference();

    return pBusiness;
}


uint32_t CStreamBusinessManager::getAttachedBusinessCount(uint64_svs streamID)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_BusinessMapMutex);
    return m_BusinessMap.count(streamID);
}

void CStreamBusinessManager::findBusiness(uint64_svs streamID, BUSINESS_LIST &list)
{
    list.clear();
    CStreamBusiness *pBusiness = NULL;
    ACE_Guard<ACE_Thread_Mutex> locker(m_BusinessMapMutex);
    STREAM_BUSINESS_MAP_ITER iter = m_BusinessMap.find(streamID);
    for(;m_BusinessMap.end() != iter; ++iter)
    {
        if (streamID != iter->first)
        {
            break;
        }

        pBusiness = iter->second;
        (void)pBusiness->addReference();

        list.push_back(pBusiness);
    }

    return;
}

void CStreamBusinessManager::getAllBusiness(BUSINESS_LIST& businessList)
{
    businessList.clear();
    CStreamBusiness *pBusiness = NULL;
    ACE_Guard<ACE_Thread_Mutex> locker(m_BusinessMapMutex);
    STREAM_BUSINESS_MAP_ITER iter;

    for(iter = m_BusinessMap.begin(); iter != m_BusinessMap.end(); ++iter)
    {
        pBusiness = iter->second;
        if (isBusinessInList(pBusiness, businessList))
        {
            continue;
        }

        (void)pBusiness->addReference();

        businessList.push_back(pBusiness);
    }

    return;
}

bool CStreamBusinessManager::isBusinessInList
(
    const CStreamBusiness *pBusiness,
    BUSINESS_LIST& businessList
)const
{
    BUSINESS_LIST_ITER iter;
    for (iter = businessList.begin(); iter != businessList.end(); ++iter)
    {
        if (pBusiness == *iter)
        {
            return true;
        }
    }

    return false;
}

uint32_t CStreamBusinessManager::getBusinessCount()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_BusinessMapMutex);
    return m_BusinessMap.size();
}

uint32_t CStreamBusinessManager::getBusinessCount(uint64_svs ullStreamId)
{
    uint32_t unCount = 0;
    ACE_Guard<ACE_Thread_Mutex> locker(m_BusinessMapMutex);
    STREAM_BUSINESS_MAP_ITER iter = m_BusinessMap.find(ullStreamId);
    for(;m_BusinessMap.end() != iter; ++iter)
    {
        if (ullStreamId != iter->first)
        {
            break;
        }
        unCount++;
    }

    return unCount;
}

void CStreamBusinessManager::statFlux()
{
    m_InputRate  = 0;
    m_OutputRate = 0;

    CStreamBusiness *pBusiness = NULL;
    ACE_Guard<ACE_Thread_Mutex> locker(m_BusinessMapMutex);
    STREAM_BUSINESS_MAP_ITER iter;
    for (iter = m_BusinessMap.begin(); iter != m_BusinessMap.end(); ++iter)
    {
        pBusiness = iter->second;
        m_InputRate  += pBusiness->getInputRate();
        m_OutputRate += pBusiness->getOutputRate();
    }

    m_InputRate  /= KILO;
    m_OutputRate /= KILO;

    return;
}

uint32_t CStreamBusinessManager::getInputRate()const
{
    return m_InputRate;
}

uint32_t CStreamBusinessManager::getOutputRate()const
{
    return m_OutputRate;
}

void CStreamBusinessManager::releaseBusiness(CStreamBusiness* &pBusiness)
{
    if (NULL == pBusiness)
    {
        return;
    }

    ACE_Guard<ACE_Thread_Mutex> locker(m_BusinessMapMutex);
    int32_t nRefCnt = pBusiness->decReference();
    if (0 >= nRefCnt)
    {
        destroyBusiness(pBusiness);
        m_unMaxBusinessNum--;
    }

    return;
}

void CStreamBusinessManager::releaseBusiness(BUSINESS_LIST &list)
{
    CStreamBusiness *pBusiness = NULL;
    for (BUSINESS_LIST_ITER iter = list.begin();
            iter != list.end(); iter++)
    {
        pBusiness = *iter;
        releaseBusiness(pBusiness);
    }

    return;
}



void CStreamBusinessManager::destroyBusiness(CStreamBusiness *&pBusiness)
{
    if (NULL == pBusiness)
    {
        return;
    }

    uint64_svs recvStream = pBusiness->getRecvStreamID();
    uint64_svs sendStream = pBusiness->getSendStreamID();

    STREAM_BUSINESS_MAP_ITER iter = m_BusinessMap.find(recvStream);
    while ((m_BusinessMap.end() != iter) && (recvStream == iter->first))
    {
        if (iter->second != pBusiness)
        {
            iter++;
            continue;
        }

        m_BusinessMap.erase(iter);
        SVS_LOG((SVS_LM_INFO,"erase recv stream[%Q] in business map.", recvStream));
        break;
    }

    iter = m_BusinessMap.find(sendStream);
    while ((m_BusinessMap.end() != iter) && (sendStream == iter->first))
    {
        if (iter->second != pBusiness)
        {
            iter++;
            continue;
        }

        m_BusinessMap.erase(iter);
        SVS_LOG((SVS_LM_INFO,"erase send stream[%Q] in business map.", sendStream));
        break;
    }


    //(void)pBusiness->stop();

    SVS_LOG((SVS_LM_INFO,"business manager release business success, "
            "recv session[%Q] send session[%Q].",
            recvStream, sendStream));
    delete pBusiness;
    pBusiness = NULL;
    return;
}

void CStreamBusinessManager::startFluxTimer()
{
    ACE_Reactor* pReactor = CStreamServiceTask::instance()->getTimerReactor();
    if (NULL == pReactor)
    {
        return;
    }

    try
    {
        m_pFluxTimer = new (CSessionFluxTimer);
    }
    catch(...)
    {
        return;
    }

    ACE_Time_Value tv((int32_t)CStreamConfig::instance()->getAccountPeriod(), 0);
    int32_t TimerId = pReactor->schedule_timer(m_pFluxTimer, this, tv, tv);
    if (-1 == TimerId)
    {
        SVS_LOG((SVS_LM_WARNING,"start session flux timer fail."));
        return;
    }

    SVS_LOG((SVS_LM_INFO,"start session flux timer success, timer id[%d].", TimerId));
    return;
}

void CStreamBusinessManager::stopFluxTimer()
{
    if (NULL == m_pFluxTimer)
    {
        return;
    }

    ACE_Reactor* pReactor = CStreamServiceTask::instance()->getTimerReactor();
    if (NULL == pReactor)
    {
        return;
    }

    (void)pReactor->cancel_timer(m_pFluxTimer);
    delete m_pFluxTimer;
    m_pFluxTimer = NULL;

    SVS_LOG((SVS_LM_INFO,"stop session flux timer success."));

    return;
}

