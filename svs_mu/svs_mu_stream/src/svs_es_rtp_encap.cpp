/*
 * EsRtpEncap.cpp
 *
 *  Created on: 2013-1-11
 */
#include "svs_log_msg.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_es_rtp_encap.h"
#include "svs_adapter_rtp_packet.h"


CEsRtpEncap::CEsRtpEncap()
{
    m_MpegPsDec.init(this,NULL);
    m_pEsRtpHandle = NULL;
    m_pUserData    = NULL;
    m_usVideoRtpSeq= 0;
    m_usAudioRtpSeq= 0;
}

CEsRtpEncap::~CEsRtpEncap()
{
    m_pEsRtpHandle = NULL;
    m_pUserData    = NULL;
    m_MpegPsDec.release();
    // TODO Auto-generated destructor stub
}
int32_t CEsRtpEncap::Init(CEsRtpEncapHandle* pEsRtpHandle,void* pUserData)
{
    m_pEsRtpHandle = pEsRtpHandle;
    m_pUserData    = pUserData;
    return RET_OK;
}
int32_t CEsRtpEncap::insertPSFrame(const char* pData, uint32_t unDataSize, uint32_t unTimestamp)
{
    SVS_LOG((SVS_LM_DEBUG,"CEsRtpEncap::insertPSFrame,DataSize:[%d]",unDataSize));
    m_MpegPsDec.writePsStream(pData,unDataSize,unTimestamp);
    return RET_OK;
}

void CEsRtpEncap::handleVideoMulNaluStream(const char* pEsData, uint32_t unDataSize,
                            uint32_t unTimestamp,uint8_t ucStreamType, void* pUserData)
{
    char * nal_start,*new_nal_start;
    uint32_t unDataLen = 0;
    char * buf_start = (char*)pEsData;
    char * buf_end   = (char *)(void*)(pEsData + unDataSize);

    //SPS PPS
    nal_start = h264_find_startcode(buf_start, buf_end);
    if (nal_start >= buf_end)
    {
        return;
    }
    while(nal_start < buf_end)
    {
        //SPS PPS
        new_nal_start = h264_find_startcode(nal_start+STARTCODE_LEN, buf_end);
        if (new_nal_start >= buf_end)
        {
            break;
        }

        unDataLen = (uint32_t)(new_nal_start - nal_start);

        handleVideoEsStream(nal_start,unDataLen,unTimestamp,ucStreamType,pUserData);

        nal_start = new_nal_start;
    }
    unDataLen = (uint32_t)(buf_end - nal_start);
    handleVideoEsStream(nal_start,unDataLen,unTimestamp,ucStreamType,pUserData);

}


void CEsRtpEncap::handleVideoEsStream(const char* pEsData, uint32_t unDataSize,
                            uint32_t unTimestamp,uint8_t ucStreamType, void* pUserData)
{
    if(NULL == pEsData)
    {
        return;
    }
    if(0 == unDataSize)
    {
        return;
    }
    if(VIDEO_STREAM_TYPE_H264 != ucStreamType)
    {
        return;
    }

    char * buf_start = (char*)pEsData;
    char * buf_end   = (char *)(void*)(pEsData + unDataSize);
    char * nal_start = NULL;
    uint32_t nal_len          = 0;
    nal_start = h264_find_startcode(buf_start, buf_end);
    if (nal_start >= buf_end)
    {
        return;
    }

    while (!*(nal_start++));

    nal_len = unDataSize - (uint32_t)(nal_start - buf_start);

    CRtpPacket  rtpPacket;
    (void)rtpPacket.GeneratePacket(m_RtpBuffer, MDU_MSG_BLOCK_SIZE);

    uint32_t unLeastSize = nal_len;
    char* pDataPos = (char*)nal_start;
   // bool bMutil = false;
    uint32_t  unHeadSize =  sizeof(RTP_FIXED_HEADER);
    uint32_t  unCount = unLeastSize/(MAX_ENCAP_RTP_SIZE - unHeadSize);
    PS_NALU_HEADER* pNalHead = (PS_NALU_HEADER*) (void*) pDataPos;

    if(0 != unLeastSize%MAX_ENCAP_RTP_SIZE)
    {
        unCount++;
    }

    if(unCount > 1)
    {
        PS_FU_INDICATOR* pFu = (PS_FU_INDICATOR*)(void*)(m_RtpBuffer + sizeof(RTP_FIXED_HEADER));
        PS_FU_HEADER* pHead = (PS_FU_HEADER*)(void*)(m_RtpBuffer + sizeof(RTP_FIXED_HEADER)+1);

        pFu->TYPE    = 28;
        pFu->NRI     = pNalHead->NRI;
        pFu->F       = pNalHead->F;
        pHead->TYPE  = pNalHead->TYPE;
        unHeadSize   +=2;
        //bMutil = true;
        pDataPos += 1;

        for (uint32_t i =0;i < unCount;i++)
        {
            rtpPacket.SetPayloadType(99);
            rtpPacket.SetSeqNum(m_usVideoRtpSeq++);
            rtpPacket.SetTimeStamp(unTimestamp);
            rtpPacket.SetSSRC(999999999);
            rtpPacket.SetMarker(false);

            uint32_t unCopySize = MAX_ENCAP_RTP_SIZE - unHeadSize;
            if (unCopySize >= unLeastSize)
            {
                rtpPacket.SetMarker(true);
                unCopySize = unLeastSize;
            }
            if(0 == i)
            {
                pHead->S = 1;
            }
            else if(i == (unCount -1))
            {
                pHead->E = 1;
            }

            memcpy(&m_RtpBuffer[unHeadSize],
                       pDataPos,
                       unCopySize);
            unLeastSize -= unCopySize;
            pDataPos += unCopySize;

            m_pEsRtpHandle->handleEsRtpPacket(m_RtpBuffer,
                                                unCopySize + unHeadSize,
                                                m_pUserData);
        }
    }
    else
    {
        rtpPacket.SetPayloadType(99);
        rtpPacket.SetSeqNum(m_usVideoRtpSeq++);
        rtpPacket.SetTimeStamp(unTimestamp);
        rtpPacket.SetSSRC(999999999);
        rtpPacket.SetMarker(true);
        memcpy(&m_RtpBuffer[unHeadSize], pDataPos,unLeastSize);
        m_pEsRtpHandle->handleEsRtpPacket(m_RtpBuffer,unLeastSize + unHeadSize,m_pUserData);
    }


    return;
}

void CEsRtpEncap::handleAudioEsStream(const char* pEsData, uint32_t unDataSize,
                            uint32_t unTimestamp,uint8_t ucStreamType, void* pUserData)
{
    CRtpPacket  rtpPacket;
    (void)rtpPacket.GeneratePacket(m_RtpBuffer, MDU_MSG_BLOCK_SIZE);

    uint32_t unLeastSize = unDataSize;
    char* pDataPos = (char*)pEsData;

    while (0 < unLeastSize)
    {
        rtpPacket.SetPayloadType(0);
        rtpPacket.SetSeqNum(m_usAudioRtpSeq++);
        rtpPacket.SetTimeStamp(unTimestamp);
        rtpPacket.SetSSRC(999999998);
        rtpPacket.SetMarker(false);

        uint32_t unCopySize = MAX_ENCAP_RTP_SIZE - sizeof(RTP_FIXED_HEADER);
        if (unCopySize >= unLeastSize)
        {
            rtpPacket.SetMarker(true);
            unCopySize = unLeastSize;
        }

        memcpy(m_RtpBuffer + sizeof(RTP_FIXED_HEADER),pDataPos,unCopySize);
        unLeastSize -= unCopySize;
        pDataPos += unCopySize;

        if( NULL != m_pEsRtpHandle)
        {
             m_pEsRtpHandle->handleEsRtpPacket(m_RtpBuffer,
                                        unCopySize + sizeof(RTP_FIXED_HEADER),
                                        m_pUserData);
        }

    }

    return;
}
char* CEsRtpEncap::h264_find_startcode_internal(char *p, char *end)
{
    char *a = p + 4 - ((int64_t)p & 3);

    for (end -= 3; p < a && p < end; p++)
    {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
        {
            return p;
        }
    }

    for (end -= 3; p < end; p += 4)
    {
        uint32_t x = *(const uint32_t*)p;
        //      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
        //      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
        if ((x - 0x01010101) & (~x) & 0x80808080)
        { // generic
            if (p[1] == 0)
            {
                if (p[0] == 0 && p[2] == 1)
                {
                    return p;
                }
                if (p[2] == 0 && p[3] == 1)
                {
                    return p+1;
                }
            }
            if (p[3] == 0)
            {
                if (p[2] == 0 && p[4] == 1)
                {
                    return p+2;
                }
                if (p[4] == 0 && p[5] == 1)
                {
                    return p+3;
                }
            }
        }
    }

    for (end += 3; p < end; p++)
    {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
        {
            return p;
        }
    }

    return end + 3;
}


char*CEsRtpEncap::h264_find_startcode(char *p, char *end)
{
    char *out= h264_find_startcode_internal(p, end);
    if(p<out && out<end && !out[-1])
    {
        out--;
    }
    return out;
}

