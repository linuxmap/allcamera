/*
 * RtcpPacket.cpp
 *
 *  Created on: 2010-12-28
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_rtcp_packet.h"
#include <svs_ace_header.h>


CRtcpPacket::CRtcpPacket()
{
    m_unReporterSSRC    = 0;
    m_unReporteeSSRC    = 0;
    m_strCname          = "";
    m_ullNtpTime        = 0;
}

CRtcpPacket::~CRtcpPacket()
{
}

void CRtcpPacket::setLocalSSRC(uint32_t unSSRC)
{
    m_unReporterSSRC = unSSRC;
    return;
}

uint32_t CRtcpPacket::getLocalSSRC() const
{
    return m_unReporterSSRC;
}

void CRtcpPacket::setPeerSSRC(uint32_t unSSRC)
{
    m_unReporteeSSRC = unSSRC;
    return;
}

uint32_t CRtcpPacket::getPeerSSRC() const
{
    return m_unReporteeSSRC;
}

// 设置本地Ip,将用来构造CNAME:  stream@local_ip
void CRtcpPacket::setLocalIp(uint32_t unLocalIp)
{
    ACE_INET_Addr addr((uint16_t)0, unLocalIp);
    m_strCname = "stream@";
    m_strCname += addr.get_host_addr();

    return;
}

// 创建接收者报告
int32_t CRtcpPacket::createReceiverReport(char *pBuffer,
                                      uint32_t unMaxLength,
                                      uint32_t &reportLength) const
{
    if ((NULL == pBuffer) || (unMaxLength <= sizeof(RTCP_RR_PACKET)))
    {
        SVS_LOG((SVS_LM_WARNING,"create receiver report fail, invalid param."));
        return RET_FAIL;
    }

    reportLength = 0;
    RTCP_RR_PACKET *pReceiverReport = (RTCP_RR_PACKET*)(void*)pBuffer;
    pReceiverReport->stHeader.Version = RTCP_PACKET_VERSION;
    pReceiverReport->stHeader.Padding = 0;
    pReceiverReport->stHeader.Count   = 1;
    pReceiverReport->stHeader.PacketType = RTCP_PACKET_RR;
    pReceiverReport->stHeader.Length  =  htons(sizeof(RTCP_RR_PACKET) / RTCP_LEN_BASE - 1);  // RR报文长度是固定的

    // 目前所有统计数据都填成0
    pReceiverReport->unReporterSSRC   = htonl(m_unReporterSSRC);
    pReceiverReport->unReporteeSSRC   = htonl(m_unReporteeSSRC);
    pReceiverReport->unMaxReceivedSeq = htonl(0);
    pReceiverReport->unDelayFromLastSR = htonl(0);
    pReceiverReport->unJitter          = htonl(0);
    pReceiverReport->unLastSRTimestamp = htonl(0);
    pReceiverReport->unLossPercent     = htonl(0);
    pReceiverReport->unLostCount       = htonl(0);
    pReceiverReport->unMaxReceivedSeq  = htonl(0);

    reportLength += sizeof(RTCP_RR_PACKET);

    uint32_t unSdesLen = 0;
    if (RET_OK != createSdesPacket(pBuffer + reportLength, unMaxLength - reportLength, unSdesLen))
    {
        reportLength = 0;
        SVS_LOG((SVS_LM_WARNING,"create recevier report fail."));
        return RET_FAIL;
    }

    reportLength += unSdesLen;
    return RET_OK;
}

// 创建发送者报告
int32_t CRtcpPacket::createSenderReport(char *pBuffer,
                                    uint32_t unMaxLength,
                                    uint32_t &reportLength)
{
    if ((NULL == pBuffer) || (unMaxLength <= sizeof(RTCP_SR_PACKET)))
    {
        SVS_LOG((SVS_LM_WARNING,"create receiver report fail, invalid param."));
        return RET_FAIL;
    }

    reportLength = 0;
    RTCP_SR_PACKET *pSenderReport = (RTCP_SR_PACKET*) (void*) pBuffer;
    pSenderReport->stHeader.Version = RTCP_PACKET_VERSION;
    pSenderReport->stHeader.Padding = 0;
    pSenderReport->stHeader.Count = 1;
    pSenderReport->stHeader.PacketType = RTCP_PACKET_SR;
    pSenderReport->stHeader.Length = htons(sizeof(RTCP_SR_PACKET) / RTCP_LEN_BASE - 1); // RR报文长度是固定的



    // 目前所有统计数据都填成0
    if(0 == m_ullNtpTime)
    {
        m_ullNtpTime = getNtpTimeforRtcp();
    }
    pSenderReport->unReporterSSRC = htonl(m_unReporterSSRC);
    pSenderReport->unNtpTimestamp = m_ullNtpTime;
    pSenderReport->unRtpTimestamp = htonl(0);
    pSenderReport->unPacketCount = htonl(0);
    pSenderReport->unByteCount = htonl(0);

    //lastRtpTime = unPacketTime;

    reportLength += sizeof(RTCP_SR_PACKET);

    uint32_t unSdesLen = 0;
    if (RET_OK != createSdesPacket(pBuffer + reportLength, unMaxLength - reportLength, unSdesLen))
    {
        reportLength = 0;
        SVS_LOG((SVS_LM_WARNING,"create sender report fail."));
        return RET_FAIL;
    }

    reportLength += unSdesLen;
    return RET_OK;
}

int32_t CRtcpPacket::createSdesPacket(char *pBuffer,
                                  uint32_t unMaxLength,
                                  uint32_t &reportLength) const
{
    if ((NULL == pBuffer) || (0 == unMaxLength))
    {
        SVS_LOG((SVS_LM_WARNING,"create sdes packet fail, invalid param."));
        return RET_FAIL;
    }

    // 先计算SDES包的长度
    uint32_t unSdesLen = sizeof(RTCP_SDES_PACKET);
    unSdesLen += (SDES_ITEM_TYPE_SIZE + SDES_ITEM_LEN_SIZE + m_strCname.length()); // CNAME ITEM长度
    unSdesLen += SDES_ITEM_END_SIZE;

    // 填满4字节整数倍
    if (0 != unSdesLen % RTCP_LEN_BASE)
    {
        unSdesLen += (RTCP_LEN_BASE - (unSdesLen % RTCP_LEN_BASE));
    }

    if (unMaxLength < unSdesLen)
    {
        SVS_LOG((SVS_LM_WARNING,"create sdes packet fail, buffer length[%u] less [%u].",
                unMaxLength, unSdesLen));
        return RET_FAIL;
    }

    RTCP_SDES_PACKET *pSdes = (RTCP_SDES_PACKET*) (void*) pBuffer;
    pSdes->stHeader.Version = RTCP_PACKET_VERSION;
    pSdes->stHeader.Padding = 0;
    pSdes->stHeader.PacketType = RTCP_PACKET_SDES;
    pSdes->stHeader.Count   = 1;
    pSdes->stHeader.Length  = htons((uint16_t)(unSdesLen / RTCP_LEN_BASE));

    pSdes->unReporterSSRC   = htonl(m_unReporterSSRC);
    char *pData = (char*)(void*)(pSdes + 1);
    *pData = SDES_TYPE_CNAME;
    pData++;

    *pData = (char)m_strCname.length();
    pData++;

    (void)memcpy(pData, m_strCname.c_str(), m_strCname.length());
    pData += m_strCname.length();

    *pData = SDES_TYPE_END;
    pData++;

    *pData = 0;

    // 设置长度
    reportLength   = unSdesLen;

    return RET_OK;
}

uint64_t CRtcpPacket::getNtpTimeforRtcp() const
{
    //ACE_Time_Value tvNTPTime = ACE_OS::gettimeofday();

    //uint64_t timestamp = (uint64_t)(tvNTPTime.sec() * 1000000 + tvNTPTime.usec());

    //timestamp= (timestamp / 1000) * 1000 + NTP_OFFSET_US;

    uint64_t timestamp =100;

    return (htonl((( timestamp & 0xFFFFFFFF)<< 32)) | htonl(((timestamp>>32)&0xFFFFFFFF)));
}



