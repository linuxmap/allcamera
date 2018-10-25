/*
 * PsEcapProcessor.cpp
 *
 *  Created on: 2012-2-17
 *      Author:
 */

#include "svs_log_msg.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_ps_rtp_encap.h"
#include "svs_adapter_rtp_packet.h"

CPsRtpEncap::CPsRtpEncap()
{
    m_pRtpHandle    = NULL;
    m_pUserData     = NULL;

    memset(m_RtpBuffer, 0x0, MDU_MSG_BLOCK_SIZE);
    m_usRtpSeq      = 0;
}

CPsRtpEncap::~CPsRtpEncap()
{
    m_pRtpHandle    = NULL;
    m_pUserData     = NULL;
}

int32_t CPsRtpEncap::init(CPsRtpEncapHandle* pHandle, void *pUserData)
{
    if (NULL == pHandle)
    {
        return RET_FAIL;
    }

    m_pRtpHandle    = pHandle;
    m_pUserData     = pUserData;

    if (0 != m_MpegPsEncap.init(this, NULL))
    {
        SVS_LOG((SVS_LM_WARNING,"failed to init ps rtp encap."));
        return RET_FAIL;
    }

    srand((uint32_t)time(NULL));
    m_usRtpSeq = (uint16_t)rand();    // 随机超始序列号

    SVS_LOG((SVS_LM_INFO,"success to init ps rtp encap."));
    return RET_OK;
}

int32_t CPsRtpEncap::insertVideoFrame(const char* pData, uint32_t unDataSize, uint32_t unTimestamp)
{
    if ((NULL == pData) || (0 == unDataSize))
    {
        return RET_FAIL;
    }

    if (0 != m_MpegPsEncap.writeVideoEsStream(pData, unDataSize, unTimestamp))
    {
        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CPsRtpEncap::insertAudioFrame(const char* pData, uint32_t unDataSize, uint32_t unTimestamp)
{
    if ((NULL == pData) || (0 == unDataSize))
    {
        return RET_FAIL;
    }

    if (0 != m_MpegPsEncap.writeAudioEsStream(pData, unDataSize, unTimestamp))
    {
        return RET_FAIL;
    }

    return RET_OK;
}

void CPsRtpEncap::handlePsStream(const char* pPSData, uint32_t unDataSize,
                                 uint32_t unTimestamp, void* /*pUserData*/,bool endFlag)
{
    if (NULL == pPSData || (0 == unDataSize))
    {
        return;
    }

    if (NULL == m_pRtpHandle)
    {
        return;
    }

    // 完成RTP切片
    CRtpPacket  rtpPacket;
    (void)rtpPacket.GeneratePacket(m_RtpBuffer, MDU_MSG_BLOCK_SIZE);

    uint32_t unLeastSize = unDataSize;
    char* pDataPos = (char*)pPSData;

    while (0 < unLeastSize)
    {
        rtpPacket.SetPayloadType(PS_RTP_PAYLOAD_TYPE);
        rtpPacket.SetSeqNum(m_usRtpSeq++);
        rtpPacket.SetTimeStamp(unTimestamp);
        rtpPacket.SetSSRC(999999999);          //写死
        rtpPacket.SetMarker(false);

        uint32_t unCopySize = MAX_ENCAP_RTP_SIZE - sizeof(RTP_FIXED_HEADER);
        if (unCopySize >= unLeastSize)
        {
            if(true == endFlag)
            {
                // 最后一个数据包
                rtpPacket.SetMarker(true);
            }
            unCopySize = unLeastSize;
        }

        memcpy(m_RtpBuffer + sizeof(RTP_FIXED_HEADER),
               pDataPos,
               unCopySize);
        unLeastSize -= unCopySize;
        pDataPos += unCopySize;

        // 回调处理RTP数据
        m_pRtpHandle->handlePsRtpPacket(m_RtpBuffer,
                                        unCopySize + sizeof(RTP_FIXED_HEADER),
                                        m_pUserData);
    }

    return;
}

void CPsRtpEncap::release()
{
    m_MpegPsEncap.release();

    SVS_LOG((SVS_LM_INFO,"success to release ps rtp encap."));
    return;
}



