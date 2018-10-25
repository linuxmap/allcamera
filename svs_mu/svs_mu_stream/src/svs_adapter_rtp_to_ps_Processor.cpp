/*
 * Rtp2PsProcessor.cpp
 *
 *  Created on: 2012-2-24
 *      Author:
 */
#include "svs_log_msg.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_rtp_to_ps_Processor.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_rtp_packet.h"
#include "svs_adapter_business_manager.h"
#include "svs_adapter_business.h"
#include "svs_adapter_session_factory.h"


CRtp2PsProcessor::CRtp2PsProcessor()
{
    m_pRtpFrameCache    = NULL;
    m_pWritePos         = NULL;
    m_ucRecvVideoPT     = 0;
}

CRtp2PsProcessor::~CRtp2PsProcessor()
{
    if (NULL != m_pRtpFrameCache)
    {
        delete[] m_pRtpFrameCache;
    }
    m_pRtpFrameCache = NULL;
    m_pWritePos      = NULL;
    m_ucRecvVideoPT     = 0;
}

int32_t CRtp2PsProcessor::Init(uint64_t ullSendSessionID)
{
    if (RET_OK != CStreamMediaProcessor::Init(ullSendSessionID))
    {
        return RET_FAIL;
    }

    if (RET_OK != m_PsRtpEncap.init(this, NULL))
    {
        return RET_FAIL;
    }

    if (RET_OK != m_VideoRtpFrameOrganizer.init(this,MAX_RTP_FRAME_CACHE_NUM,true))
    {
        return RET_FAIL;
    }
    if (RET_OK != m_AudioRtpFrameOrganizer.init(this))
    {
        return RET_FAIL;
    }

    try
    {
        if (NULL == m_pRtpFrameCache)
        {
            m_pRtpFrameCache = new char[RTP_FRAME_CACHE_SIZE];
            m_pWritePos      = m_pRtpFrameCache;
        }
    }
    catch(...)
    {
    }
    SVS_LOG((SVS_LM_INFO,"success to init rtp2ps processor, session[%Q].",  ullSendSessionID));
    return RET_OK;
}

void CRtp2PsProcessor::Send (ACE_Message_Block* pMb)
{
    if (NULL == pMb)
    {
        return;
    }

    if (NULL == m_pSendSession)
    {
        return;
    }

    ACE_Message_Block* pMediaBlock = CMediaBlockBuffer::instance().allocMediaBlock();
    if(NULL == pMediaBlock)
    {
        return;
    }

    pMediaBlock->copy(pMb->rd_ptr(), pMb->length());

    CRtpPacket rtpPacket;
    (void)rtpPacket.ParsePacket(pMediaBlock->rd_ptr(), pMediaBlock->length());

    uint8_t ucPT = (uint8_t)rtpPacket.GetPayloadType();


    if (ucPT == GetRecvVideoPT())
    {
        if (RET_OK != m_VideoRtpFrameOrganizer.insertRtpPacket(pMediaBlock))
        {
            CMediaBlockBuffer::instance().freeMediaBlock(pMediaBlock);
            return;
        }
    }
    else
    {
        if (RET_OK != m_AudioRtpFrameOrganizer.insertRtpPacket(pMediaBlock))
        {
            CMediaBlockBuffer::instance().freeMediaBlock(pMediaBlock);
            return;
        }
    }

    CMediaBlockBuffer::instance().freeMediaBlock(pMediaBlock);
    return;
}

void CRtp2PsProcessor::handlePsRtpPacket(const char* pRtpData, uint32_t unDataSize, void* /*pUserData*/)
{
    if (NULL == pRtpData || 0 == unDataSize)
    {
        return;
    }

    ACE_Message_Block *pRtpBlock = CMediaBlockBuffer::instance().allocMediaBlock();
    if (NULL == pRtpBlock)
    {
        return;
    }
    pRtpBlock->copy(pRtpData, unDataSize);

    CDirectProcessor::Send(pRtpBlock);

    CMediaBlockBuffer::instance().freeMediaBlock(pRtpBlock);
    return;
}

uint32_t CRtp2PsProcessor::getWritePos()const
{
    if (NULL == m_pWritePos)
    {
        return 0;
    }

    if (NULL == m_pRtpFrameCache)
    {
        return 0;
    }

    uint32_t unPos = (uint32_t)(m_pWritePos - m_pRtpFrameCache);
    return unPos;
}

int32_t CRtp2PsProcessor::checkVideoRtpFrame(RTP_FRAME_LIST &rtpFrameList)
{
    if (NULL == m_pWritePos)
    {
        return RET_FAIL;
    }


    ACE_Message_Block* pRtpBlock = NULL;
    CRtpPacket rtpPacket;
    uint32_t unCacheSize = RTP_FRAME_CACHE_SIZE - 4;
    bool  bRemoveHead = true;
    for (RTP_FRAME_LIST_ITER iter = rtpFrameList.begin(); iter != rtpFrameList.end(); iter++)
    {
        pRtpBlock = *iter;
        (void)rtpPacket.ParsePacket(pRtpBlock->rd_ptr(), pRtpBlock->length());

        // �Ƴ�RTP��Ϣͷ
        pRtpBlock->rd_ptr(rtpPacket.GetHeadLen());

        FU_INDICATOR* pFu = (FU_INDICATOR*)(void*)pRtpBlock->rd_ptr();
        // ���˵������֡��ֻ֧��FUA�ְ�ʽ
        if (H264_NALU_TYPE_FU_A == pFu->TYPE)
        {
            pRtpBlock->rd_ptr(sizeof(FU_INDICATOR));

            //FU_HEADER *pFuHeader = (FU_HEADER*)(void*)pRtpBlock->rd_ptr();

            if (bRemoveHead)
            {
                H264_NALU_HEADER* pHead = (H264_NALU_HEADER*) (void*) pRtpBlock->rd_ptr();
                H264_NALU_HEADER* pWriteHead = (H264_NALU_HEADER*) (void*)m_pWritePos;
                pWriteHead->F = pFu->F;
                pWriteHead->NRI = pFu->NRI;
                pWriteHead->TYPE = pHead->TYPE;

                pRtpBlock->rd_ptr(sizeof(H264_NALU_HEADER));
                m_pWritePos += sizeof(H264_NALU_HEADER);
                unCacheSize--;

                bRemoveHead = false;
            }
            else
            {
                pRtpBlock->rd_ptr(sizeof(FU_HEADER));
            }
        }

        if (unCacheSize >= pRtpBlock->length())
        {
            memcpy(m_pWritePos, pRtpBlock->rd_ptr(), pRtpBlock->length());
            m_pWritePos += pRtpBlock->length();
            unCacheSize -= pRtpBlock->length();
        }
        else
        {
            m_pWritePos = m_pRtpFrameCache;
            return RET_FAIL;
        }

    }
    return RET_OK;
}

int32_t CRtp2PsProcessor::checkAudioRtpFrame(RTP_FRAME_LIST &rtpFrameList)
{
    if (NULL == m_pWritePos)
    {
        return RET_FAIL;
    }

    ACE_Message_Block* pRtpBlock = NULL;
    CRtpPacket rtpPacket;
    uint32_t unCacheSize = RTP_FRAME_CACHE_SIZE;
    bool  bCheckFrame = false;
    bool  bFuStart = false;
    bool  bFuStop  = false;
    for (RTP_FRAME_LIST_ITER iter = rtpFrameList.begin(); iter != rtpFrameList.end(); iter++)
    {
        pRtpBlock = *iter;
        (void)rtpPacket.ParsePacket(pRtpBlock->rd_ptr(), pRtpBlock->length());

        // �Ƴ�RTP��Ϣͷ
        pRtpBlock->rd_ptr(rtpPacket.GetHeadLen());

        if (unCacheSize >= pRtpBlock->length())
        {
            memcpy(m_pWritePos, pRtpBlock->rd_ptr(), pRtpBlock->length());
            m_pWritePos += pRtpBlock->length();
            unCacheSize -= pRtpBlock->length();
        }
        else
        {
            // ������Ȳ����������������
            m_pWritePos = m_pRtpFrameCache;
            return RET_FAIL;
        }

    }

    // ���֡�Ƿ�����
    if (bCheckFrame && (!bFuStart || !bFuStop) )
    {
        SVS_LOG((SVS_LM_WARNING,"rtp2ps processor recved incomplete frame, timestamp[%u].",
                rtpPacket.GetTimeStamp()));

        // �����������
        m_pWritePos = m_pRtpFrameCache;
        return RET_FAIL;
    }

    return RET_OK;
}

// ����PS��װ���RTP��ص��ӿ�
void CRtp2PsProcessor::handleRtpFrame(RTP_FRAME_LIST &rtpFrameList)
{
    if (rtpFrameList.empty())
    {
        return;
    }

    // �µ�һ֡����
    if (m_pWritePos != m_pRtpFrameCache)
    {
        return;
    }

    RTP_FRAME_LIST_ITER iter = rtpFrameList.begin();
    ACE_Message_Block* pRtpBlock = *iter;

    CRtpPacket rtpPacket;
    (void)rtpPacket.ParsePacket(pRtpBlock->rd_ptr(), pRtpBlock->length());

    uint8_t ucPT = (uint8_t)rtpPacket.GetPayloadType();


    // ��ݹ���ɹ�������PS��װ
    if (ucPT == GetRecvVideoPT())
    {
        // ��Ƶ��
        HandleVideoRtpFrame(rtpFrameList);
    }
    else
    {
        // ��Ƶ��
        HandleAudioRtpFrame(rtpFrameList);
    }

    // ���û�����дλ��
    m_pWritePos = m_pRtpFrameCache;
    return;
}

void CRtp2PsProcessor::HandleVideoRtpFrame(RTP_FRAME_LIST &rtpFrameList)
{
    if (NULL == m_pWritePos)
    {
        return;
    }

    *m_pWritePos++ = 0x0;
    *m_pWritePos++ = 0x0;
    *m_pWritePos++ = 0x0;
    *m_pWritePos++ = 0x1;

    RTP_FRAME_LIST_ITER iter = rtpFrameList.begin();
    ACE_Message_Block* pRtpBlock = *iter;

    CRtpPacket rtpPacket;
    (void)rtpPacket.ParsePacket(pRtpBlock->rd_ptr(), pRtpBlock->length());
    uint32_t ulTimeTick = rtpPacket.GetTimeStamp();

    if (RET_OK != checkVideoRtpFrame(rtpFrameList))
    {
        return;
    }

    if (NULL == m_pSendSession)
    {
        return;
    }

    // ��Ƶ��
    (void)m_PsRtpEncap.insertVideoFrame(m_pRtpFrameCache, getWritePos(), ulTimeTick);
    return;
}

void CRtp2PsProcessor::HandleAudioRtpFrame(RTP_FRAME_LIST &rtpFrameList)
{
    if (NULL == m_pWritePos)
    {
        return;
    }

    RTP_FRAME_LIST_ITER iter = rtpFrameList.begin();
    ACE_Message_Block* pRtpBlock = *iter;

    CRtpPacket rtpPacket;
    (void)rtpPacket.ParsePacket(pRtpBlock->rd_ptr(), pRtpBlock->length());
    uint32_t ulTimeTick = rtpPacket.GetTimeStamp();

    if (RET_OK != checkAudioRtpFrame(rtpFrameList))
    {
        return;
    }

    if (NULL == m_pSendSession)
    {
        return;
    }

    // ��Ƶ��
    (void)m_PsRtpEncap.insertAudioFrame(m_pRtpFrameCache, getWritePos(), ulTimeTick);


    return;
}
uint8_t CRtp2PsProcessor::GetRecvVideoPT()
{
    if(0 != m_ucRecvVideoPT)
    {
        return m_ucRecvVideoPT;
    }

    CStreamBusiness* pBusiness = CStreamBusinessManager::instance()->findBusiness(m_SendSessionId);
    if(NULL == pBusiness )
    {
        SVS_LOG((SVS_LM_WARNING,"fine send Session:[%ull]'s Business fail.",m_SendSessionId));
        return m_ucRecvVideoPT;
    }

    CStreamSession* pRecvSession = CStreamSessionFactory::instance()->findSession(pBusiness->getRecvStreamID());
    if(NULL == pRecvSession )
    {
        SVS_LOG((SVS_LM_WARNING,"fine recv Session:[%ull] fail,send session:[%ull]",
                                    pBusiness->getRecvStreamID(),m_SendSessionId));
    }
    else
    {
        m_ucRecvVideoPT = pRecvSession->getVideoPayload();
    }

    CStreamSessionFactory::instance()->releaseSession(pRecvSession);
    CStreamBusinessManager::instance()->releaseBusiness(pBusiness);

    return m_ucRecvVideoPT;
}
