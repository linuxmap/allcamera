/*
 * RtpFrame.cpp
 *
 *  Created on: 2012-2-23
 *      Author:
 */
#include "svs_log_msg.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_rtp_frame_organizer.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_rtp_packet.h"
#include "svs_adapter_common.h"
#include "svs_adapter_sdp.h"

CRtpFrameOrganizer::CRtpFrameOrganizer()
{
    m_unMaxCacheFrameNum    = 0;
    m_bParserH264           = false;
    m_pRtpFrameHandler      = NULL;

}

CRtpFrameOrganizer::~CRtpFrameOrganizer()
{
    try
    {
        release();
    }
    catch(...){}

    m_pRtpFrameHandler = NULL;
}

int32_t CRtpFrameOrganizer::init(IRtpFrameHandler* pHandler,
                             uint32_t unMaxFrameCache /*= MAX_RTP_FRAME_CACHE_NUM*/,
                             bool     bParserH264 /* = false */)
{
    if ((NULL == pHandler) || (0 == unMaxFrameCache))
    {
        return RET_FAIL;
    }

    m_pRtpFrameHandler   = pHandler;
    m_unMaxCacheFrameNum = unMaxFrameCache;
    m_bParserH264        = bParserH264;

    RTP_FRAME_INFO_S *pFramInfo = NULL;

    for(uint32_t i = 0;i < m_unMaxCacheFrameNum;i++)
    {
        pFramInfo = M_NEW(pFramInfo);
        if(NULL == pFramInfo)
        {
            return RET_FAIL;
        }
        m_RtpFrameFreeList.push_back(pFramInfo);
    }



    SVS_LOG((SVS_LM_INFO,"success to init rtp frame organizer[%p].", this));
    return RET_OK;
}

int32_t CRtpFrameOrganizer::insertRtpPacket( ACE_Message_Block* pRtpBlock)
{

    if (NULL == pRtpBlock)
    {
        return RET_FAIL;
    }

    CRtpPacket rtpPacket;
    if (RET_OK != rtpPacket.ParsePacket(pRtpBlock->rd_ptr(), pRtpBlock->length()))
    {
        SVS_LOG((SVS_LM_ERROR, "fail to insert rtp packet, parse rtp packet fail."));
        return RET_FAIL;
    }

    if((m_bParserH264)&&(PT_TYPE_H264 == rtpPacket.GetPayloadType()))
    {
        RTP_H264_FU_INDICATOR* pFu = (RTP_H264_FU_INDICATOR*)(void*)(pRtpBlock->rd_ptr() + rtpPacket.GetHeadLen());
        uint8_t ucFuType = pFu->TYPE;
        if(ucFuType ==RTP_H264_NALU_TYPE_UNDEFINED || ucFuType >= RTP_H264_NALU_TYPE_END)
        {
            SVS_LOG((SVS_LM_ERROR, "convert H264 fail, fu type[%d] invalid.", ucFuType));
            return RET_FAIL;
        }

        if(ucFuType >0 && ucFuType < RTP_H264_NALU_TYPE_STAP_A)
        {
            RTP_FRAME_INFO_S stFrameInfo;
            stFrameInfo.bMarker = true;
            stFrameInfo.unTimestamp = rtpPacket.GetTimeStamp();
            RTP_PACK_INFO_S  rtpInfo;
            rtpInfo.bMarker = true;
            rtpInfo.usSeq   = rtpPacket.GetSeqNum();
            rtpInfo.unTimestamp = rtpPacket.GetTimeStamp();
            rtpInfo.pRtpMsgBlock = pRtpBlock;
            stFrameInfo.PacketQueue.push_back(rtpInfo);
            handleFinishedFrame(&stFrameInfo);
            stFrameInfo.PacketQueue.clear();
            return RET_OK;
        }
    }

    ACE_Message_Block* pRtpMb = pRtpBlock->duplicate();
    if (NULL == pRtpMb)
    {
        SVS_LOG((SVS_LM_ERROR,"fail to insert rtp packet, duplicate mb fail."));
        return RET_FAIL;
    }

    RTP_FRAME_INFO_S* pFrameInfo = NULL;


    RTP_PACK_INFO_S  rtpInfo;
    rtpInfo.bMarker      = rtpPacket.GetMarker();
    rtpInfo.usSeq        = rtpPacket.GetSeqNum();
    rtpInfo.unTimestamp  = rtpPacket.GetTimeStamp();
    rtpInfo.pRtpMsgBlock = pRtpMb;



    RTP_FRAME_MAP_S::iterator iter = m_RtpFrameMap.find(rtpInfo.unTimestamp);

    if(iter == m_RtpFrameMap.end())
    {
        pFrameInfo = InsertFrame(rtpInfo.unTimestamp);
    }
    else
    {
        pFrameInfo = iter->second;
    }


    if(NULL == pFrameInfo)
    {
         SVS_LOG((SVS_LM_ERROR,"Insert Frame fail,becaus there is no free frame info."));
         CMediaBlockBuffer::instance().freeMediaBlock(pRtpMb);
         RTP_FRAME_INFO_S *pFrameinfo = GetSmallFrame();
         releaseRtpPacket(pFrameinfo);
         return RET_FAIL;
    }
    if(false == pFrameInfo->bMarker)
    {
        pFrameInfo->bMarker = rtpInfo.bMarker;
    }

    if (RET_OK != insert(pFrameInfo,rtpInfo))
    {
        CMediaBlockBuffer::instance().freeMediaBlock(pRtpMb);
        return RET_FAIL;
    }
    /*
    SVS_LOG((SVS_LM_DEBUG,"success to insert rtp packet: seq[%u] timestamp[%u] marker[%d].",
            rtpInfo.usSeq, rtpInfo.unTimestamp, rtpInfo.bMarker));
            */
    checkFrame();


    return RET_OK;
}

void CRtpFrameOrganizer::release()
{
    RTP_FRAME_MAP_S::iterator iter = m_RtpFrameMap.begin();
    RTP_FRAME_INFO_S *pFramInfo = NULL;
    for(;iter != m_RtpFrameMap.end();++iter)
    {
        pFramInfo = iter->second;
        if(NULL == pFramInfo)
        {
            continue;
        }
        while (!pFramInfo->PacketQueue.empty())
        {
            CMediaBlockBuffer::instance().freeMediaBlock(pFramInfo->PacketQueue[0].pRtpMsgBlock);
            pFramInfo->PacketQueue.pop_front();
        }

        M_DELETE(pFramInfo);
    }

    m_RtpFrameMap.clear();

    while(!m_RtpFrameFreeList.empty())
    {
        pFramInfo = m_RtpFrameFreeList.front();
        m_RtpFrameFreeList.pop_front();
        M_DELETE(pFramInfo);
    }

    SVS_LOG((SVS_LM_INFO,"success to release rtp frame organizer[%p].", this));
    return;
}


int32_t CRtpFrameOrganizer::insert(RTP_FRAME_INFO_S *pFrameinfo,const RTP_PACK_INFO_S &info)
{

    if(NULL == pFrameinfo)
    {
        return RET_FAIL;
    }

    if (0 == pFrameinfo->PacketQueue.size())
    {
        pFrameinfo->PacketQueue.push_back(info);
        return RET_OK;
    }

    uint16_t usFirstSeq = pFrameinfo->PacketQueue[0].usSeq;
    uint16_t usLastSeq = pFrameinfo->PacketQueue[pFrameinfo->PacketQueue.size() - 1].usSeq;


    if (info.usSeq >= usLastSeq)
    {
        pFrameinfo->PacketQueue.push_back(info);
    }
    else if(info.usSeq < usFirstSeq)
    {
        pFrameinfo->PacketQueue.push_front(info);
    }
    else
    {
        return insertRange(pFrameinfo,info);
    }
    return RET_OK;
}

int32_t CRtpFrameOrganizer::insertRange(RTP_FRAME_INFO_S *pFrameinfo ,const RTP_PACK_INFO_S &info)
{
    uint32_t i = 0;
    uint32_t unSize = pFrameinfo->PacketQueue.size();
    while (i <= unSize)
    {
        if (info.usSeq <= pFrameinfo->PacketQueue[i].usSeq)
        {
            pFrameinfo->PacketQueue.insert(pFrameinfo->PacketQueue.begin() + (int32_t)i, info);
            return RET_OK;
        }

        i++;
    }

    SVS_LOG((SVS_LM_WARNING,"fail to insert rtp packet[%u : %u].",info.usSeq, info.unTimestamp));
    return RET_FAIL;
}

void CRtpFrameOrganizer::checkFrame()
{

    RTP_FRAME_INFO_S *pFrameinfo = GetSmallFrame();

    if(NULL == pFrameinfo)
    {
        SVS_LOG((SVS_LM_DEBUG,"Get Small Frame fail."));
        return;
    }
    if (pFrameinfo->PacketQueue.empty())
    {
        SVS_LOG((SVS_LM_DEBUG,"Get Small Frame is empty!."));
        return;
    }

    if(!pFrameinfo->bMarker)
    {
        return;
    }

    uint16_t usSeq = (uint16_t)pFrameinfo->PacketQueue[0].usSeq;
    for (uint32_t i = 0; i < pFrameinfo->PacketQueue.size(); i++)
    {
        if (usSeq != pFrameinfo->PacketQueue[i].usSeq)
        {
            SVS_LOG((SVS_LM_DEBUG,"the Frame is not full,need Seq:[%d],Current Seq:[%d],Index:[%d]!.",usSeq,pFrameinfo->PacketQueue[i].usSeq,i));
            return;
        }
        usSeq++;
    }
    handleFinishedFrame(pFrameinfo);

    releaseRtpPacket(pFrameinfo);

    return;
}


void CRtpFrameOrganizer::handleFinishedFrame(RTP_FRAME_INFO_S *pFrameinfo)
{
    if(NULL == pFrameinfo)
    {
        SVS_LOG((SVS_LM_WARNING," Handle finish frame ,the Frame info is NULL"));
        return;
    }
    if (pFrameinfo->PacketQueue.empty())
    {
        SVS_LOG((SVS_LM_WARNING," Handle finish frame ,the Frame queue is empty"));
        return;
    }

    if (NULL == m_pRtpFrameHandler)
    {
        SVS_LOG((SVS_LM_WARNING," Handle finish frame ,the RTP Handle is NULL"));
        return;
    }

    IRtpFrameHandler::RTP_FRAME_LIST   frameList;
    for (uint32_t i = 0; i < pFrameinfo->PacketQueue.size(); i++)
    {
        if (NULL != pFrameinfo->PacketQueue[i].pRtpMsgBlock)
        {
            frameList.push_back(pFrameinfo->PacketQueue[i].pRtpMsgBlock);
        }
    }

    m_pRtpFrameHandler->handleRtpFrame(frameList);

    return;
}

void CRtpFrameOrganizer::releaseRtpPacket(RTP_FRAME_INFO_S *pFrameinfo)
{
    if(NULL == pFrameinfo)
    {
        return;
    }

    RTP_FRAME_MAP_S::iterator iter = m_RtpFrameMap.find(pFrameinfo->unTimestamp);

    if(iter == m_RtpFrameMap.end())
    {
        return;
    }


    while (0 < pFrameinfo->PacketQueue.size())
    {
        if (NULL != pFrameinfo->PacketQueue[0].pRtpMsgBlock)
        {
             CMediaBlockBuffer::instance().freeMediaBlock(pFrameinfo->PacketQueue[0].pRtpMsgBlock);
        }
        pFrameinfo->PacketQueue.pop_front();
    }
    pFrameinfo->PacketQueue.clear();

    m_RtpFrameMap.erase(iter);
    m_RtpFrameFreeList.push_back(pFrameinfo);


    return;
}
RTP_FRAME_INFO_S* CRtpFrameOrganizer::InsertFrame(uint32_t  unTimestamp)
{
    if(0 == m_RtpFrameFreeList.size())
    {
        return NULL;
    }
    RTP_FRAME_INFO_S* pFrame = m_RtpFrameFreeList.front();
    m_RtpFrameFreeList.pop_front();
    pFrame->bMarker = false;
    pFrame->unTimestamp = unTimestamp;
    m_RtpFrameMap.insert(RTP_FRAME_MAP_S::value_type(unTimestamp,pFrame));
    return pFrame;
}

RTP_FRAME_INFO_S* CRtpFrameOrganizer::GetSmallFrame()
{
    RTP_FRAME_MAP_S::iterator iter = m_RtpFrameMap.begin();
    RTP_FRAME_INFO_S *pCurFramInfo = NULL;
    RTP_FRAME_INFO_S *pSmallFramInfo = NULL;
    for(;iter != m_RtpFrameMap.end();++iter)
    {
        pCurFramInfo = iter->second;
        if(NULL == pCurFramInfo)
        {
            continue;
        }
        if(NULL == pSmallFramInfo)
        {
            pSmallFramInfo = pCurFramInfo;
            continue;
        }


        if(pSmallFramInfo->unTimestamp >  pCurFramInfo->unTimestamp)
        {
            pSmallFramInfo = pCurFramInfo;
        }
    }

    return pSmallFramInfo;
}

