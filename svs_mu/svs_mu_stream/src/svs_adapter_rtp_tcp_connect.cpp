/*
 * RtpTcpConnect.cpp
 *
 *  Created on: 2016-1-6
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_rtp_tcp_connect.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_block_buffer.h"

#include "svs_adapter_session.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_exchange.h"
#include "svs_adapter_inner_msg.h"
#include "svs_adapter_service_task.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_rtsp_packet.h"
#include "svs_adapter_tcp_handle.h"
#include "svs_adapter_rtp_packet.h"

CRtpTcpConnect::CRtpTcpConnect()
{
    m_unSize            = 0;
    m_unDataOffset      = 0;
    m_usInnerMsgType    = 0;
    m_bServiceType      = false;
}

CRtpTcpConnect::~CRtpTcpConnect()
{
}

int32_t CRtpTcpConnect::handleRecvedData(const char* pData, uint32_t unDataSize)
{
    int32_t nRet = parseDataInfo(pData, unDataSize);
    if (PARSE_RET_FAIL == nRet)
    {
        // 解析失败
        return -1;
    }

    if (PARSE_RET_DATA_LACK == nRet)
    {
        // 数据未接收完整
        return 0;
    }

    // 多级时，如果主动发起连接，则只有流ID没有业务类型，需要主动获取业务类型
    if ((!m_bServiceType) && (0 != m_ullStreamID))
    {
        if (RET_OK == getPlayType())
        {
            m_bServiceType = true;
        }
    }

    if (INNER_MSG_RTP == m_usInnerMsgType)
    {
        // 处理RTP处理
        return handleMediaData(pData, unDataSize);
    }

    return  handleControlMessage(pData, unDataSize);
}

int32_t CRtpTcpConnect::parseDataInfo(const char* pData, uint32_t unDataSize)
{
    m_unSize            = 0; // 真实数据的长度
    m_unDataOffset      = 0; // 真实数据的偏移
    m_usInnerMsgType    = INNER_MSG_RTP;

    int32_t iRet;
    if (RTP_INTERLEAVE_FLAG != pData[0])
    {
        iRet = parseRtspPacket(pData, unDataSize);
    }
    else
    {
        iRet = parseRtpRtcpPacket(pData, unDataSize);
    }

    return iRet;
}

int32_t CRtpTcpConnect::parseRtspPacket(const char* pData, uint32_t unDataSize)
{
    // 确定是RTSP消息，但需要解析出RTSP消息长度
    if (0 != CRtspPacket::checkRtsp(pData, unDataSize, m_unSize))
    {
        SVS_LOG((SVS_LM_WARNING,"tcp connect[%d] check rtsp fail.", m_localAddr.get_port_number()));
        return PARSE_RET_FAIL;
    }

    if ((0 == m_unSize) || (unDataSize < m_unSize))
    {
        // 数据没有接收完
        m_unSize = 0;
        return PARSE_RET_DATA_LACK;
    }

    if (0 == m_ullStreamID)
    {
        // 解析出流ID来
        CRtspPacket rtspPack;
        if (RET_OK != rtspPack.parse(pData, m_unSize))
        {
            SVS_LOG((SVS_LM_WARNING,"tcp connect[%d] parse rtsp fail.", m_localAddr.get_port_number()));
            return PARSE_RET_FAIL;
        }
        m_ullStreamID = rtspPack.getSessionID();

        // 需要向TcpHandle注册本连接
        if (NULL == m_pTcpHandle)
        {
            return -1;
        }

        if (RET_OK != m_pTcpHandle->addTcpConnect(this))
        {
            SVS_LOG((SVS_LM_ERROR,"stream[%Q] add connect to tcp handle fail.",
                    m_ullStreamID));
            return -1;
        }

        // 取得会话的业务类型
        if (RET_OK != getPlayType())
        {
            SVS_LOG((SVS_LM_ERROR,"stream[%Q] connect get service type fail.",
                             m_ullStreamID));
            return -1;
        }
        m_bServiceType = true;      // 已经成功取到业务类型
        }

    m_usInnerMsgType = INNER_MSG_RTSP;
    m_unDataOffset   = 0; // RTSP没有交织头，所以不需要偏移数据

    return PARSE_RET_OK;
}

int32_t CRtpTcpConnect::parseRtpRtcpPacket(const char* pData, uint32_t unDataSize)
{
    if (unDataSize < RTP_INTERLEAVE_LENGTH)
    {
        // 剩余数据不够解析，返回0
        return PARSE_RET_DATA_LACK;
    }

    // 根据交织长度解析出RTP或RTCP包的长度
    m_unSize = (uint32_t) ACE_NTOHS(*(uint16_t*)(void*)&pData[2]);
    if (unDataSize - RTP_INTERLEAVE_LENGTH < m_unSize)
    {
        // 数据没有接收完，暂时返回
        return PARSE_RET_DATA_LACK;
    }

    if ((getVideoInterleaveNum() == pData[1])
    || (getAudioInterleaveNum() == pData[1]))
    {
         m_usInnerMsgType = INNER_MSG_RTP;
    }
    else
    {
        m_usInnerMsgType = INNER_MSG_RTCP;
    }

    m_unDataOffset = RTP_INTERLEAVE_LENGTH; // RTP及RTCP有交织头，偏移4字节数据

    return PARSE_RET_OK;
}

int32_t CRtpTcpConnect::handleMediaData(const char* pData, uint32_t unDataSize)
{
   // 申请数据缓冲块,保存数据
    ACE_Message_Block *pMsg = CMediaBlockBuffer::instance().allocMediaBlock();
    if (NULL == pMsg)
    {
        SVS_LOG((SVS_LM_WARNING,"rtp tcp port[%d] alloc media block fail.",
                m_localAddr.get_port_number()));
        return 0;
    }

    // TODO 后期可能需要考虑优化数据拷贝
    STREAM_TRANSMIT_PACKET *pPacket = (STREAM_TRANSMIT_PACKET *) (void*) pMsg->base();
    pMsg->wr_ptr(sizeof(STREAM_TRANSMIT_PACKET) - 1); // 包头中有一个字节是数据

    CRtpPacket rtpPacket;
    (void)rtpPacket.ParsePacket(pData + m_unDataOffset,m_unSize);


    uint32_t  unTailLen = rtpPacket.GetTailLen();


    pMsg->copy(pData + m_unDataOffset, m_unSize - unTailLen);

    pPacket->PuStreamId = m_ullStreamID;
    pPacket->enPacketType = STREAM_PACKET_TYPE_MEDIA_DATA;

    int32_t nRet = RET_OK;

    nRet = CStreamMediaExchange::instance()->addData(pMsg);

    if (RET_OK != nRet)
    {
        CMediaBlockBuffer::instance().freeMediaBlock(pMsg);

        // 添加数据失败，可能队列已满
        return 0;
    }

    // 返回实际处理的数据长度
    return (int32_t)(m_unSize + m_unDataOffset);
}

int32_t CRtpTcpConnect::handleControlMessage(const char* pData, uint32_t unDataSize)
{
    // 申请数据缓冲块,保存数据
    ACE_Message_Block *pMsg = CMediaBlockBuffer::instance().allocMediaBlock();
    if (NULL == pMsg)
    {
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] rtp tcp port[%d] alloc media block fail.",
                        m_ullStreamID, m_localAddr.get_port_number()));
        return 0;
    }

    fillStreamInnerMsg(pMsg->base(),
                    m_ullStreamID,
                    this,
                    m_remoteAddr.get_ip_address(),
                    m_remoteAddr.get_port_number(),
                    (uint16_t) m_usInnerMsgType,
                    sizeof(STREAM_INNER_MSG));
    pMsg->wr_ptr(sizeof(STREAM_INNER_MSG));
    if (-1 == pMsg->copy(pData + m_unDataOffset, m_unSize))
    {
        CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] rtp tcp port[%d] copy control message fail.",
                m_ullStreamID, m_localAddr.get_port_number()));

        // 返回实际处理的数据长度
        return (int32_t)(m_unSize + m_unDataOffset);
    }

    if (RET_OK != CStreamServiceTask::instance()->enqueueInnerMessage(pMsg))
    {
        CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] rtp tcp port[%d] enqueue inner message fail.",
                         m_ullStreamID, m_localAddr.get_port_number()));
        return 0;
    }

    // 返回实际处理的数据长度
    return (int32_t)(m_unSize + m_unDataOffset);
}

int32_t CRtpTcpConnect::getPlayType()
{
    if (0 == m_ullStreamID)
    {
        SVS_LOG((SVS_LM_WARNING,"tcp connect[%s:%d] get service type fail, stream id is 0.",
                m_localAddr.get_host_addr(),
                m_localAddr.get_port_number()));

        return RET_FAIL;
    }

    CStreamSession *pSession = CStreamSessionFactory::instance()->findSession(m_ullStreamID);
    if (NULL == pSession)
    {
        SVS_LOG((SVS_LM_WARNING,"tcp connect[%s:%d] get service type fail, can't find session[%Q].",
                        m_localAddr.get_host_addr(),
                        m_localAddr.get_port_number(),
                        m_ullStreamID));
        return RET_FAIL;
    }

    PLAY_TYPE enPlayType = pSession->getPlayType();
    CStreamSessionFactory::instance()->releaseSession(pSession);

    SVS_LOG((SVS_LM_INFO,"session[%Q] connect[%s:%d] get service type [%d].",
                    m_ullStreamID,
                    m_localAddr.get_host_addr(),
                    m_localAddr.get_port_number(),
                    enPlayType ));
    return RET_OK;
}
