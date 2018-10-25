#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_svs_def.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_session.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_exchange.h"
#include "svs_adapter_config.h"
#include "svs_daemon_thread.h"

CMduMediaExchange* CMduMediaExchange::g_pMediaExchange = NULL;

CMduMediaExchange::CMduMediaExchange()
{
    m_ulThreadIdx = 0;
    m_ulThreadNum = 0;
    m_bRunning = false;
    m_pDataExchangeQueue = NULL;
    m_pMediaExchangeMap  = NULL;
}

CMduMediaExchange::~CMduMediaExchange()
{
    try
    {
        Destroy();
    }
    catch(...)
    {
        ;
    }

    m_pDataExchangeQueue = NULL;
    m_pMediaExchangeMap  = NULL;
}


int32_t CMduMediaExchange::Init(uint32_t ulThreadNum)
{
    if (0 == ulThreadNum)
    {
        SVS_LOG((SVS_LM_CRITICAL,"CMduMediaExchange::Init fail, thread num is zero."));
        return RET_ERR_PARAM;
    }

    try
    {
        m_pDataExchangeQueue = new CMediaDataQueue*[ulThreadNum];
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,"CMduMediaExchange::Init fail, alloc queue fail. thread num[%u].",
            ulThreadNum));
        return RET_ERR_SYS_NEW;
    }
    memset(m_pDataExchangeQueue, 0x0, sizeof(m_pDataExchangeQueue) * ulThreadNum);

    try
    {
        m_pMediaExchangeMap = new MediaExchangeMap*[ulThreadNum];
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,"CMduMediaExchange::Init fail, alloc map fail. thread num[%u].",
            ulThreadNum));
        return RET_ERR_SYS_NEW;
    }
    memset(m_pMediaExchangeMap, 0x0, sizeof(m_pMediaExchangeMap) * ulThreadNum);

    size_t *pStackSize = NULL;
    try
    {
        pStackSize = new size_t[ulThreadNum];;
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,"CMduMediaExchange::Init fail, alloc stack fail. thread num[%u].",
            ulThreadNum));
        return RET_ERR_SYS_NEW;
    }

    for (uint32_t i = 0; i < ulThreadNum; i++)
    {
        pStackSize[i] = DEFAULT_THREAD_STACK_SIZE;
    }

    m_ulThreadNum = ulThreadNum;
    m_bRunning = true;

    int32_t nRet = activate(THR_NEW_LWP  | THR_JOINABLE,
                        (int32_t)ulThreadNum,
                        0,
                        ACE_DEFAULT_THREAD_PRIORITY,
                        -1,
                        0,
                        0,
                        0,
                        pStackSize,
                        0);

    if (NULL != pStackSize)
    {
        delete[] pStackSize;
        pStackSize = NULL;
    }

    if (-1 == nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"CMduMediaExchange::Init fail, thread activate fail. thread num[%d].",
            ulThreadNum));

        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"CMduMediaExchange::Init success, thread num[%d].",
        ulThreadNum));
    return RET_OK;
}


void CMduMediaExchange::Destroy()
{
    m_bRunning = false;

    (void)wait();

    if (NULL != m_pDataExchangeQueue)
    {
        delete [] m_pDataExchangeQueue;
        m_pDataExchangeQueue = NULL;
    }

    if (NULL != m_pMediaExchangeMap)
    {
        delete [] m_pMediaExchangeMap;
        m_pMediaExchangeMap = NULL;
    }
}


int32_t CMduMediaExchange::svc()
{
    uint32_t unThreadIndex = getThreadIndex();
    SVS_LOG((SVS_LM_INFO,"start media exchange thread:[%d].", unThreadIndex));

    MediaExchangeThread(unThreadIndex);

    SVS_LOG((SVS_LM_INFO,"media exchange thread [%d] exit.", unThreadIndex));
    return RET_OK;
}

int32_t CMduMediaExchange::addData(ACE_Message_Block* pDataBlock)const
{
    if (NULL == pDataBlock)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::addData fail, data block is null."));
        return RET_ERR_PARAM;
    }

    MDU_TRANSMIT_PACKET* pPacket = (MDU_TRANSMIT_PACKET*) (void*) pDataBlock->rd_ptr();
    if (NULL == pPacket)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::addData fail, data block is null."));
        return RET_FAIL;
    }


    int32_t lRet = RET_OK;
    uint32_t ulThreadIdx = 0;
    lRet = GenProcessThreadIdx(pPacket->PuStreamId, ulThreadIdx);
    if (RET_OK != lRet)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::addData fail, "
                "get thread index fail. pu stream id[%Q].",
                pPacket->PuStreamId));
        return RET_FAIL;
    }

    lRet = EnqueuePacket(ulThreadIdx, pDataBlock);
    if (RET_OK != lRet)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::addData fail, "
                "enqueue data block fail, pu stream id[%Q].",
                pPacket->PuStreamId));

        return RET_FAIL;
    }

    return RET_OK;
}
//lint -e429
int32_t CMduMediaExchange::addMediaProcessor(uint64_svs PuStreamId,CMduMediaProcessor* pstProcessor,
                                             uint32_t   unRecvTranType,uint32_t unSendTranType)const
{
    if (NULL == pstProcessor)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::addMediaProcessor fail, processor is null. pu stream id[%Q].",
             PuStreamId));
        return RET_FAIL;
    }

    int32_t lRet;
    uint32_t ulThreadIdx = 0;
    lRet = GenProcessThreadIdx(PuStreamId, ulThreadIdx);
    if (RET_OK != lRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::addMediaProcessor fail, get thread index fail."
            " pu stream id[%Q] processor[%p].",
            PuStreamId,
            pstProcessor));
        return RET_FAIL;
    }

    ACE_Message_Block* pMb = NULL;
    size_t size = sizeof(MDU_TRANSMIT_PACKET) - 1 + sizeof(MDU_PROCESSOR_INFO);
    try
    {
        pMb = new ACE_Message_Block(size);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::addMediaProcessor fail, alloc mb fail. "
            "pu stream id[%Q] processor[%p].",
            PuStreamId,
            pstProcessor));
        return RET_ERR_SYS_NEW;
    }

    MDU_TRANSMIT_PACKET* pPacket = (MDU_TRANSMIT_PACKET*) (void*) pMb->rd_ptr();
    if (NULL == pPacket)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::addMediaProcessor fail, data block is null."
            " pu stream id[%Q] processor[%p].",
            PuStreamId,
            pstProcessor));
        return RET_FAIL;
    }

    pPacket->enPacketType = MDU_PACKET_TYPE_ADD_SESSION;
    pPacket->PuStreamId = PuStreamId;
    MDU_PROCESSOR_INFO* pSessionInfo = (MDU_PROCESSOR_INFO*)&pPacket->cData[0];
    pSessionInfo->pMediaProcessor = pstProcessor;
    pSessionInfo->unRecvTranType  = unRecvTranType;
    pSessionInfo->unSendTranType  = unSendTranType;

    pMb->wr_ptr(size);

    lRet = EnqueuePacket(ulThreadIdx, pMb);
    if (RET_OK != lRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::addMediaProcessor fail, enqueue fail."
            " pu stream id[%Q] processor[%p].",
            PuStreamId,
            pstProcessor));

        delete pMb;
        pMb = NULL;

        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,
        "CMduMediaExchange::addMediaProcessor success. enqueue success."
        " pu stream id[%Q] processor[%p] thread index [%u].",
        PuStreamId,
        pstProcessor,
        ulThreadIdx));

    return RET_OK;
}

int32_t CMduMediaExchange::delMediaProcessor(uint64_svs PuStreamId,CMduMediaProcessor* pstProcessor,
                                             uint32_t   unRecvTranType)const
{
    if (NULL == pstProcessor)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::delMediaProcessor fail, processor is null. pu stream id[%Q].",
            PuStreamId));
        return RET_FAIL;
    }

    int32_t lRet;
    uint32_t ulThreadIdx = 0;
    lRet = GenProcessThreadIdx(PuStreamId, ulThreadIdx);
    if (RET_OK != lRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::delMediaProcessor fail, get thread index fail."
            " pu stream id[%Q] processor[%p].",
            PuStreamId,
            pstProcessor));
        return RET_FAIL;
    }

    ACE_Message_Block* pMb = NULL;
    size_t size = sizeof(MDU_TRANSMIT_PACKET) - 1 + sizeof(MDU_PROCESSOR_INFO);
    try
    {
        pMb = new ACE_Message_Block(size);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::delMediaProcessor fail, alloc mb fail. "
            "pu stream id[%Q] processor[%p].",
            PuStreamId,
            pstProcessor));
        return RET_ERR_SYS_NEW;
    }

    MDU_TRANSMIT_PACKET* pPacket = (MDU_TRANSMIT_PACKET*) (void*) pMb->rd_ptr();
    if (NULL == pPacket)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::delMediaProcessor fail, data block is null."
            " pu stream id[%Q] processor[%p].",
            PuStreamId,
            pstProcessor));
        return RET_FAIL;
    }

    pPacket->enPacketType = MDU_PACKET_TYPE_DEL_SESSION;
    pPacket->PuStreamId = PuStreamId;
    MDU_PROCESSOR_INFO* pSessionInfo = (MDU_PROCESSOR_INFO*)&pPacket->cData[0];
    pSessionInfo->pMediaProcessor = pstProcessor;
    pSessionInfo->unRecvTranType  = unRecvTranType;

    pMb->wr_ptr(size);

    lRet = EnqueuePacket(ulThreadIdx, pMb);
    if (RET_OK != lRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::delMediaProcessor fail, enqueue fail."
            " pu stream id[%Q] processor[%p].",
            PuStreamId,
            pstProcessor));

        delete pMb;
        pMb = NULL;

        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,
        "CMduMediaExchange::delMediaProcessor success. enqueue success."
        " pu stream id[%Q] processor[%p] thread index [%u].",
        PuStreamId,
        pstProcessor,
        ulThreadIdx));

    return RET_OK;
}
//lint +e429
int32_t CMduMediaExchange::GenProcessThreadIdx(uint64_svs PuStreamId, uint32_t& ulThreadIdx)const
{
    if (0 == m_ulThreadIdx)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::GenProcessThreadIdx fail, thread num is zero."));
        return RET_ERR_SYS;

    }

    ulThreadIdx = PuStreamId % m_ulThreadIdx;
    return RET_OK;
}
//lint -e429
int32_t CMduMediaExchange::EnqueuePacket
(
    uint32_t        ulThreadIdx,
    ACE_Message_Block*        pMb
)const
{
    if (NULL == pMb)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::EnqueuePacket fail, mb is null."));
        return RET_ERR_PARAM;
    }

    if (NULL == m_pDataExchangeQueue)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::EnqueuePacket fail, data queue is null."));
        return RET_ERR_SYS;
    }

    if (NULL == m_pDataExchangeQueue[ulThreadIdx])
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::EnqueuePacket fail, queue [%d] is null.",
                ulThreadIdx));
        return RET_ERR_SYS;
    }

    int32_t lRet = m_pDataExchangeQueue[ulThreadIdx]->enqueue_tail(pMb);

    return lRet;
}
//lint +e429


uint32_t CMduMediaExchange::getThreadIndex()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ExchangeMutex);
    return m_ulThreadIdx++;
}

void CMduMediaExchange::MediaExchangeThread(uint32_t ulThreadIdx)
{
    if (NULL == m_pDataExchangeQueue)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::MediaExchangeThread start fail. "
            "queue is null. index[%u]",
            ulThreadIdx));
        return;
    }

    if (NULL == m_pMediaExchangeMap)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::MediaExchangeThread start fail. "
            "map is null. index[%u]",
            ulThreadIdx));
        return;
    }

    try
    {
        m_pDataExchangeQueue[ulThreadIdx] = new CMediaDataQueue;
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::MediaExchangeThread start fail. "
                "alloc queue fail. index[%u]",
            ulThreadIdx));
        return;
    }

    uint32_t unBlockCnt = CMduConfig::instance()->getServiceCapacity() * MDU_BLOCK_NUM_PER_CHANNEL;
    unBlockCnt /= (m_ulThreadNum + 1);
    if (RET_OK != m_pDataExchangeQueue[ulThreadIdx]->init(unBlockCnt))
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::MediaExchangeThread start fail. "
                "init queue fail. index[%u]",
            ulThreadIdx));
        return;
    }

    try
    {
        m_pMediaExchangeMap[ulThreadIdx] = new MediaExchangeMap;
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::MediaExchangeThread start fail. "
                "alloc map fail. index[%u]",
            ulThreadIdx));
        return;
    }

    ACE_Message_Block* pMb = NULL;
    MDU_TRANSMIT_PACKET* pPacket = NULL;
    CThread_Stat_Reporter report("MediaExchangeThread");
    ACE_Time_Value timeout(0, MDU_MAX_QUEUE_DELAY);
    while (m_bRunning)
    {
        (void)m_pDataExchangeQueue[ulThreadIdx]->dequeue_head(pMb, &timeout);
        if (NULL == pMb)
        {
            report.ReportStat();
            continue;
        }

        pPacket = (MDU_TRANSMIT_PACKET*)(void*)pMb->rd_ptr();
        if (NULL == pPacket)
        {
            report.ReportStat();
            continue;
        }

        switch(pPacket->enPacketType)
        {
            case MDU_PACKET_TYPE_MEDIA_DATA:
            {
                ProcessMediaData(ulThreadIdx, pMb);
                CMediaBlockBuffer::instance().freeMediaBlock(pMb);
                break;
            }
            case MDU_PACKET_TYPE_ADD_SESSION:
            {
                ProcessAddSession(ulThreadIdx, pMb);
                pMb->release();
                break;
            }
            case MDU_PACKET_TYPE_DEL_SESSION:
            {
                ProcessDelSession(ulThreadIdx, pMb);
                pMb->release();
                break;
            }
            case MDU_PACKET_TYPE_SESSION_EOS:
            case MDU_PACKET_TYPE_SESSION_BOS:
            case MDU_PACKET_TYPE_MAX:
            default :
            {
                SVS_LOG((SVS_LM_WARNING,
                    "MediaExchangeThread deal an unspported msg. msg type[%u].",
                    pPacket->enPacketType));
                CMediaBlockBuffer::instance().freeMediaBlock(pMb);
                break;
            }
        }

        pMb = NULL;

        report.ReportStat();
    }

    delete m_pDataExchangeQueue[ulThreadIdx];
    m_pDataExchangeQueue[ulThreadIdx] = NULL;

    delete m_pMediaExchangeMap[ulThreadIdx];
    m_pMediaExchangeMap[ulThreadIdx] = NULL;

    return;
}

void CMduMediaExchange::ProcessMediaData(uint32_t ulThreadIdx, ACE_Message_Block* pMb)const
{
    if (NULL == pMb)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessMediaData fail, mb is null."));
        return;
    }

    MDU_TRANSMIT_PACKET* pDataPacket = NULL;
    pDataPacket = (MDU_TRANSMIT_PACKET*)(void*)pMb->rd_ptr();
    if (NULL == pDataPacket)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessMediaData fail, data packet is null."));
        return;
    }

    if (NULL == m_pMediaExchangeMap)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessMediaData fail, exchange map ptr is null."));
        return;
    }

    if (ulThreadIdx >= m_ulThreadNum)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessMediaData fail, thread index is invalid."));
        return;
    }

    if (NULL == m_pMediaExchangeMap[ulThreadIdx])
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessMediaData fail, exchange map is null."));
        return;
    }

    CMduMediaProcessorSet* pMPSet = NULL;
    MediaExchangeIter iter;
    iter = m_pMediaExchangeMap[ulThreadIdx]->find(pDataPacket->PuStreamId);
    if (m_pMediaExchangeMap[ulThreadIdx]->end() == iter)
    {
        return;
    }

    pMPSet = iter->second;
    if (NULL == pMPSet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::ProcessMediaData fail, media processor set is null."
            " pu stream id[%Q].",
            pDataPacket->PuStreamId));
        return;
    }
    /* skip the transmit head */
    pMb->rd_ptr(sizeof(MDU_TRANSMIT_PACKET) - 1);

    (void)pMPSet->Send(pMb);

    return;
}

//lint -e429
void CMduMediaExchange::ProcessAddSession(uint32_t ulThreadIdx, const ACE_Message_Block* pMb)const
{
    if (NULL == pMb)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessAddSession fail, mb is null."));
        return;
    }

    MDU_TRANSMIT_PACKET* pAddSessionPacket = NULL;
    pAddSessionPacket = (MDU_TRANSMIT_PACKET*)(void*)pMb->rd_ptr();
    if (NULL == pAddSessionPacket)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessAddSession fail, add session packet is null."));
        return;
    }
    MDU_PROCESSOR_INFO* pSessionInfo = (MDU_PROCESSOR_INFO*)&pAddSessionPacket->cData[0];

    uint64_svs PuStreamId   = pAddSessionPacket->PuStreamId;

    if (NULL == pSessionInfo->pMediaProcessor)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::ProcessAddSession fail, media processor is null."
            " pu stream id[%Q].",
            PuStreamId));
        return;
    }

    if (NULL == m_pMediaExchangeMap)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessAddSession fail, exchange map ptr is null."));
        return;
    }

    if (ulThreadIdx >= m_ulThreadNum)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessAddSession fail, thread index is invalid."));
        return;
    }

    if (NULL == m_pMediaExchangeMap[ulThreadIdx])
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessAddSession fail, exchange map is null."));
        return;
    }

    SVS_LOG((SVS_LM_INFO,
                "CMduMediaExchange::ProcessAddSession, PUID:[%u],recvType:[%d] sendType:[%d].",
                PuStreamId,
                pSessionInfo->unRecvTranType,
                pSessionInfo->unSendTranType));

    bool bNewSet = false;
    CMduMediaProcessorSet* pMPSet = NULL;
    MediaExchangeIter iter;
    iter = m_pMediaExchangeMap[ulThreadIdx]->find(PuStreamId);
    if (m_pMediaExchangeMap[ulThreadIdx]->end() == iter)
    {
        try
        {
            if((MEDIA_TRANS_TYPE_PS == pSessionInfo->unRecvTranType)
                &&(MEDIA_TRANS_TYPE_RTP == pSessionInfo->unSendTranType))
            {
                pMPSet = new CMduPsMediaProcessorSet;
            }
            else
            {
                pMPSet = new CMduMediaProcessorSet;
            }
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_ERROR,
                "CMduMediaExchange::ProcessAddSession fail, alloc media processor set fail."
                " pu stream id[%Q].",PuStreamId));
            return;
        }

        m_pMediaExchangeMap[ulThreadIdx]->insert(std::make_pair(PuStreamId, pMPSet));
        bNewSet = true;
    }
    else
    {
        pMPSet = iter->second;
    }

    int32_t lRet = pMPSet->AddMediaProcessor(pSessionInfo->pMediaProcessor);
    if (RET_OK != lRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::ProcessAddSession fail, add media processor fail."
            " pu stream id[%Q].",
            PuStreamId));
        delete pSessionInfo->pMediaProcessor;
        pSessionInfo->pMediaProcessor = NULL;

        if (bNewSet)
        {
            delete pMPSet;
            pMPSet = NULL;
        }
    }

    return;
}
//lint +e429

void CMduMediaExchange::ProcessDelSession(uint32_t ulThreadIdx, const ACE_Message_Block* pMb)const
{
    if (NULL == pMb)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessDelSession fail, mb is null."));
        return;
    }

    MDU_TRANSMIT_PACKET* pDelSessionPacket = NULL;
    pDelSessionPacket = (MDU_TRANSMIT_PACKET*)(void*)pMb->rd_ptr();
    if (NULL == pDelSessionPacket)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessDelSession fail, del session packet is null."));
        return;
    }
    MDU_PROCESSOR_INFO* pSessionInfo = (MDU_PROCESSOR_INFO*)&pDelSessionPacket->cData[0];

    if (NULL == m_pMediaExchangeMap)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessDelSession fail, exchange map ptr is null."));
        return;
    }

    if (ulThreadIdx >= m_ulThreadNum)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessDelSession fail, thread index is invalid."));
        return;
    }

    if (NULL == m_pMediaExchangeMap[ulThreadIdx])
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaExchange::ProcessDelSession fail, exchange map is null."));
        return;
    }
    uint64_svs PuStreamId = pDelSessionPacket->PuStreamId;

    CMduMediaProcessorSet* pMPSet = NULL;
    MediaExchangeIter iter;

    iter = m_pMediaExchangeMap[ulThreadIdx]->find(PuStreamId);
    if (m_pMediaExchangeMap[ulThreadIdx]->end() == iter)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::ProcessDelSession fail, can't find media processor set."
            " pu stream id[%Q].",
            PuStreamId));
        return;
    }
    else
    {
        pMPSet = iter->second;
    }

    if (NULL == pMPSet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CMduMediaExchange::ProcessDelSession fail, media processor set is null."
            " pu stream id[%Q].",
            PuStreamId));
        return;
    }

    int32_t lRet = pMPSet->DelMediaProcessor(pSessionInfo->pMediaProcessor);

    if (RET_ERR_LIST_EMPTY == lRet)
    {
        m_pMediaExchangeMap[ulThreadIdx]->erase(iter);

        delete pMPSet;
        pMPSet = NULL;
    }

    return;
}
