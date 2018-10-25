/*
 * RtpPacket.cpp
 *
 *  Created on: 2010-12-28
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_rtp_packet.h"

CRtpPacket::CRtpPacket()
{
    m_pRtpData      = NULL;
    m_ulPacketLen   = 0;
    m_ulHeadLen     = 0;
    m_ulTailLen     = 0;

    m_pFixedHead    = NULL;
    m_pExtHead      = NULL;
}

CRtpPacket::~CRtpPacket()
{
    m_pRtpData      = NULL;
    m_pFixedHead    = NULL;
    m_pExtHead      = NULL;
}

int32_t CRtpPacket::ParsePacket
(
    const char*     pRtpData,
    uint32_t    ulLen
)
{
    if (NULL == pRtpData)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::Parse fail, rtp data is null."));
        return RET_ERR_PARAM;
    }

    // 不能小于固定头的长度
    if (ulLen < sizeof(RTP_FIXED_HEADER))
    {
        SVS_LOG((SVS_LM_DEBUG,"CRtpPacket::Parse fail, rtp data len is shorter than fixed head len."));
        return RET_ERR_PARAM;
    }
    m_pRtpData = (char*)pRtpData;
    m_ulPacketLen = ulLen;

    // 先指定固定头
    m_pFixedHead = (RTP_FIXED_HEADER*)(void*)m_pRtpData;

    if(1 == m_pFixedHead->padding)
    {
        const char* pTail = pRtpData + ulLen -1;

         m_ulTailLen = (uint32_t)(*(uint8_t*)pTail);
        if(RET_OK != SetPadding(0))
        {
            SVS_LOG((SVS_LM_ERROR,"CRtpPacket::Parse fail, SetPadding fail."));
        }
    }
    uint32_t ulHeadLen = sizeof(RTP_FIXED_HEADER) + m_pFixedHead->csrc_len * RTP_CSRC_LEN;
    if (ulLen < ulHeadLen)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::Parse fail, rtp data len is shorter than fixed head len."));
        return RET_ERR_PARAM;
    }

    // 检查版本号
    if (RET_OK != CheckVersion())
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::Parse fail, check version fail."));
        return RET_FAIL;
    }

    // 判断是否有扩展头
    if (1 != m_pFixedHead->extension)
    {
        // 没有扩展头，直接返回
        m_ulHeadLen = ulHeadLen;
        return RET_OK;
    }

    if (ulLen < ulHeadLen + sizeof(RTP_EXTEND_HEADER))
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::Parse fail, packet len is too int16_t to contain extend head."));
        return RET_ERR_PARAM;
    }

    // 指定扩展头
    m_pExtHead = (RTP_EXTEND_HEADER*)(void*)(m_pRtpData + ulHeadLen);


    // 有扩展头，判断包的长度是否足够包含扩展头
    ulHeadLen += sizeof(RTP_EXTEND_HEADER) + ntohs(m_pExtHead->usLength) * RTP_EXTEND_PROFILE_LEN;//DTS2012101503412
    if (ulLen < ulHeadLen)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::Parse fail, packet len is too int16_t."));
        return RET_ERR_PARAM;
    }

    // 至此，所有的头长度已经计算完毕了
    m_ulHeadLen = ulHeadLen;

    return RET_OK;
}

int32_t CRtpPacket::GeneratePacket
(
    char*           pRtpPacket,
    uint32_t    ulLen
)
{
    if (NULL == pRtpPacket)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::GeneratePacket fail, packet is null."));
        return RET_ERR_PARAM;
    }


    memset(pRtpPacket, 0x0, ulLen);
    m_pRtpData = pRtpPacket;
    m_ulPacketLen = ulLen;

    // 所有项设置一个默认值，外层不想改变默认值的可以不用再设置
    m_pFixedHead = (RTP_FIXED_HEADER*)(void*)m_pRtpData;

    m_pFixedHead->version = RTP_PACKET_VERSION;
    m_pFixedHead->marker  = 0;
    m_pFixedHead->payload = 96;
    m_pFixedHead->extension = 0;

    m_ulHeadLen = sizeof(RTP_FIXED_HEADER);

    // 外层根据包类型，再调用具体的设置函数设置其他项

    return RET_OK;
}
int32_t CRtpPacket::GetVersion(char& cVersion)const
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::GetVersion fail, fixed head is null."));
        return RET_FAIL;
    }

    cVersion = m_pFixedHead->version;

    return RET_OK;
}

int32_t CRtpPacket::CheckVersion()const
{
    char cVersion = 0;
    if (RET_OK != GetVersion(cVersion))
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::CheckVersion fail, get packet version fail."));
        return RET_FAIL;
    }

    if (RTP_PACKET_VERSION != cVersion)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::CheckVersion fail, version [%d] is invalid.",
            cVersion));
        return RET_FAIL;
    }

    return RET_OK;
}

uint16_t CRtpPacket::GetSeqNum()const
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::GetSeqNum fail, packet is null."));
        return 0;
    }

    return ntohs(m_pFixedHead->seq_no);
}

uint32_t  CRtpPacket::GetTimeStamp()const
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::GetTimeStamp fail, packet is null."));
        return 0;
    }

    return ntohl(m_pFixedHead->timestamp);
}

char  CRtpPacket::GetPayloadType()const
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::GetPayloadType fail, packet is null."));
        return 0;
    }

    return m_pFixedHead->payload;
}

bool CRtpPacket::GetMarker()const
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::GetMarker fail, packet is null."));
        return false;
    }

    return m_pFixedHead->marker;
}

uint32_t CRtpPacket::GetSSRC()const
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::GetSSRC fail, packet is null."));
        return 0;
    }

    return m_pFixedHead->ssrc;
}


uint32_t CRtpPacket::GetHeadLen()const
{
    return m_ulHeadLen;
}
uint32_t CRtpPacket::GetTailLen()const
{
    return m_ulTailLen;
}

int32_t CRtpPacket::SetVersion(uint8_t ucVersion)
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::SetVersion fail, packet is null."));
        return RET_FAIL;
    }

    m_pFixedHead->version = ucVersion;

    return RET_OK;
}
int32_t CRtpPacket::SetPadding(uint8_t ucPadding)
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::SetPadding fail, packet is null."));
        return RET_FAIL;
    }

    m_pFixedHead->padding= ucPadding;

    return RET_OK;
}

int32_t CRtpPacket::SetSeqNum(uint16_t usSeqNum)
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::SetSeqNum fail, packet is null."));
        return RET_FAIL;
    }

    m_pFixedHead->seq_no = ntohs(usSeqNum);

    return RET_OK;
}

int32_t CRtpPacket::SetTimeStamp(uint32_t ulTimeStamp)
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::SetTimeStamp fail, packet is null."));
        return RET_FAIL;
    }

    m_pFixedHead->timestamp = ntohl(ulTimeStamp);

    return RET_OK;
}

void CRtpPacket::SetSSRC(uint32_t unSsrc)
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::SetTimeStamp fail, packet is null."));
        return;
    }

    m_pFixedHead->ssrc = unSsrc;
    return;
}

int32_t CRtpPacket::SetPayloadType(uint8_t ucPayloadType)
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::SetPayloadType fail, packet is null."));
        return RET_FAIL;
    }

    m_pFixedHead->payload = ucPayloadType;

    return RET_OK;
}

int32_t CRtpPacket::SetMarker(bool bMarker)
{
    if (NULL == m_pFixedHead)
    {
        SVS_LOG((SVS_LM_ERROR,"CRtpPacket::SetMarker fail, packet is null."));
        return RET_FAIL;
    }

    m_pFixedHead->marker = bMarker;

    return RET_OK;
}

uint16_t CRtpPacket::GetSeqNum(const ACE_Message_Block* pMb)
{

    if (NULL == pMb)
    {
        SVS_LOG((SVS_LM_ERROR, "CRtpPacket::GetSeqNum fail, mb is null."));
        return 0;
    }

    // 获取RTP包的序列号
    CRtpPacket rtpPacket;
    if (RET_OK != rtpPacket.ParsePacket(pMb->rd_ptr(), pMb->length()))
    {
        SVS_LOG((SVS_LM_ERROR, "CRtpPacket::GetSeqNum fail, parse rtp packet fail."));
        return 0;
    }

    return rtpPacket.GetSeqNum();
}

