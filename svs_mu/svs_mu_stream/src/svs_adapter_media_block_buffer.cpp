#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_media_block_buffer.h"

CMediaBlockBuffer* CMediaBlockBuffer::g_MediaBuffer = NULL;
CMediaBlockBuffer::CMediaBlockBuffer()
{
    m_unSize    = 0;
    m_unCount   = 0;
    m_FailureTimes = 0;
    m_pMediaQueue  = NULL;
}

CMediaBlockBuffer::~CMediaBlockBuffer()
{
    try
    {
        close();
    }
    catch(...)
    {

    }
    m_pMediaQueue = NULL;
}

int32_t CMediaBlockBuffer::init(uint32_t unBlockSize, uint32_t unBlockCount)
{
    ACE_Message_Block* pMb = NULL;
    int32_t lRet = RET_OK;

    try
    {
        m_pMediaQueue = new CMediaDataQueue;
    }
    catch(...)
    {
        return RET_FAIL;
    }


    lRet = m_pMediaQueue->init(unBlockCount);
    if (RET_OK != lRet)
    {
        SVS_LOG((SVS_LM_ERROR,"CMediaBlockBuffer::init fail, init media queue fail."));
        return RET_FAIL;
    }

    for (uint32_t i = 0; i < unBlockCount; i++)
    {
        pMb = createMediaBlock(unBlockSize);
        if (NULL == pMb)
        {
            SVS_LOG((SVS_LM_ERROR,"CMediaBlockBuffer::init fail, create mb fail, index[%u].", i));
            return RET_FAIL;
        }

        lRet = m_pMediaQueue->enqueue_tail(pMb);
        if (RET_OK != lRet)
        {
            SVS_LOG((SVS_LM_ERROR,"CMediaBlockBuffer::init fail, enqueue mb fail, index[%u].", i));
            return RET_FAIL;
        }
    }

    SVS_LOG((SVS_LM_INFO,"CMediaBlockBuffer::init success. block size[%u] block count[%u].",
        unBlockSize,
        unBlockCount));
    m_unSize = unBlockSize;
    m_unCount = unBlockCount;
    return RET_OK;
}

void CMediaBlockBuffer::close()
{
    if (NULL != m_pMediaQueue)
    {
        ACE_Message_Block* pMb = NULL;
        while(!m_pMediaQueue->empty())
        {
            (void)m_pMediaQueue->dequeue_head(pMb);
            if (NULL != pMb)
            {
                delete pMb;
                pMb = NULL;
            }
        }

        m_pMediaQueue->close();

        delete m_pMediaQueue;
        m_pMediaQueue = NULL;
    }

    SVS_LOG((SVS_LM_CRITICAL,"CMediaBlockBuffer::close success."));
    return;
}

ACE_Message_Block *CMediaBlockBuffer::allocMediaBlock()
{
    if (NULL == m_pMediaQueue)
    {
        return NULL;
    }

    if (m_pMediaQueue->empty())
    {
        m_FailureTimes++;
        return NULL;
    }

    ACE_Message_Block *pMb = NULL;
    ACE_Time_Value tv(0, 100 * 1000);
    int32_t lRet = m_pMediaQueue->dequeue_head(pMb, &tv);
    if (RET_OK != lRet)
    {
        m_FailureTimes++;
        return NULL;
    }

    return pMb;
}

void CMediaBlockBuffer::freeMediaBlock(ACE_Message_Block *&pMb)
{
    if (NULL == pMb)
    {
        SVS_LOG((SVS_LM_ERROR,"CMediaBlockBuffer::freeMediaBlock fail, mb is null."));
        return;
    }

    if (pMb->reference_count() > 1)
    {
        pMb->release();
        return;
    }

    pMb->reset();

    int32_t lRet = RET_FAIL;
    if (NULL != m_pMediaQueue)
    {
        lRet = m_pMediaQueue->enqueue_tail(pMb);
    }

    if (RET_OK != lRet)
    {
        delete pMb;
        pMb = NULL;

        SVS_LOG((SVS_LM_ERROR,"CMediaBlockBuffer::freeMediaBlock fail, enqueue fail."));
        return;
    }

    return;
}


uint32_t CMediaBlockBuffer::getMaxBlockNum() const
{
    return m_unCount;
}

uint32_t CMediaBlockBuffer::getRemainBlockNum() const
{
    if (NULL == m_pMediaQueue)
    {
        return 0;
    }
    else
    {
        return m_pMediaQueue->message_count();
    }
}


uint32_t CMediaBlockBuffer::getFailureTimes()const
{
    return m_FailureTimes;
}

uint32_t CMediaBlockBuffer::getBlockSize()const
{
    return m_unSize;
}

/*lint -e429*/
ACE_Message_Block* CMediaBlockBuffer::createMediaBlock(uint32_t unBlockSize)const
{
    ACE_Message_Block* pMb = NULL;
    try
    {
        pMb = new ACE_Message_Block(unBlockSize);
    }
    catch(...)
    {
        delete pMb;
        pMb = NULL;

        SVS_LOG((SVS_LM_ERROR,"CMediaBlockBuffer::create media block fail, alloc message block fail."));
        return NULL;
    }

    if (NULL == pMb->base())
    {
        SVS_LOG((SVS_LM_ERROR,"CMediaBlockBuffer::create media block fail, data block is NULL."));
        delete pMb;
        return NULL;
    }

    return pMb;
}
/*lint +e429*/

