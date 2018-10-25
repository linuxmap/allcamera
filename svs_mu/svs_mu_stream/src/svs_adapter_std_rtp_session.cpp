/*
 * StreamStdRtpSession.cpp
 *
 *  Created on: 2016-5-20
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_time.h"
#include "svs_adapter_std_rtp_session.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_rtp_packet.h"
#include "svs_adapter_rtp_tcp_connect.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_block_buffer.h"

#include "svs_rtsp_announce_message.h"

CStreamStdRtpSession::CStreamStdRtpSession()
{
    m_rtspHandle      = ACE_INVALID_HANDLE;
    m_strRtspUrl      = "";
    m_strRtspSessionId = "";

    m_unVideoInterleaveNum = 0;
    m_unAudioInterleaveNum = 2;
    memset(m_pUdpHandle, 0x0, sizeof(CNetworkHandle*) * HANDLE_TYPE_MAX);
    m_ullPeerStreamID = 0;

    m_ulLastRecvTime  = SVS_GetSecondTime();
    m_unStartTime     = 0;
    m_unTransType     = TRANS_PROTOCAL_UDP;
}

CStreamStdRtpSession::~CStreamStdRtpSession()
{
    try
    {
        (void) CStreamPortManager::instance()->releaseRtpUdpPort(getSpecifyIp(),
                                                                m_pUdpHandle[VIDEO_RTP_HANDLE],
                                                                m_pUdpHandle[VIDEO_RTCP_HANDLE]);
        (void) CStreamPortManager::instance()->releaseRtpUdpPort(getSpecifyIp(),
                                                                m_pUdpHandle[AUDIO_RTP_HANDLE],
                                                                m_pUdpHandle[AUDIO_RTCP_HANDLE]);
        while (!m_TcpSendList.empty())
        {
            ACE_Message_Block *pMsg = m_TcpSendList.front();
            m_TcpSendList.pop_front();
            CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
        }
    }
    catch(...)
    {}
}

int32_t CStreamStdRtpSession::initStdRtpSession(uint64_t ullPeerStreamID,
                                         PLAY_TYPE      enPlayType,
                                         const ACE_INET_Addr &localAddr,
                                         const ACE_INET_Addr &/*peerAddr*/)
{
    m_ullPeerStreamID = ullPeerStreamID;
    CStreamSession *pPeerSession = CStreamSessionFactory::instance()->findSession(m_ullPeerStreamID);
    if (!pPeerSession)
    {
        SVS_LOG((SVS_LM_WARNING,"init std rtp session[%Q] fail, can't find peer session[%Q].",
                getStreamId(), ullPeerStreamID));
        return RET_FAIL;
    }

    m_stSessionInfo.SessionType = RTSP_SESSION;
    m_enPlayType   = enPlayType;
    m_ulVideoCodeType = pPeerSession->getVideoCodeType();
    m_stSessionInfo.PeerType = PEER_TYPE_CU;
    m_stSessionInfo.RecvStreamID = ullPeerStreamID;
    m_stSessionInfo.TransDirection = TRANS_DIRECTION_SENDONLY;
    m_stSessionInfo.SpecifyIp  = localAddr.get_ip_address();


    m_ulVideoCodeType = pPeerSession->getVideoCodecType();

    CStreamSessionFactory::instance()->releaseSession(pPeerSession);

    setStatus(STREAM_SESSION_STATUS_WAIT_START);

    SVS_LOG((SVS_LM_INFO,"init std rtp session[%Q] service type[%d] success.",
                    getStreamId(), getPlayType()));
    return RET_OK;
}

int32_t CStreamStdRtpSession::startStdRtpSession(const CRtspSetupMessage &rtspMessage)
{
    if (1 < m_unStartTime)
    {
        return RET_FAIL;
    }

    if (0 == m_unStartTime)
    {
        m_stSessionInfo.TransProtocol = rtspMessage.getTransType();
        m_strRtspSessionId = rtspMessage.getSession();
        if ("" == m_strRtspUrl)
        {
            m_strRtspUrl = rtspMessage.getRtspUrl();
        }

        m_unTransType = rtspMessage.getTransType();

        SVS_LOG((SVS_LM_INFO,"session[%Q] start video channel transtype[%d].",
                                    getStreamId(),m_unTransType));

        if (TRANS_PROTOCAL_TCP == rtspMessage.getTransType())
        {
            m_unVideoInterleaveNum  = rtspMessage.getInterleaveNum();
        }
        else
        {
            if (RET_OK != allocMediaPort())
            {
                return RET_FAIL;
            }

            if (RET_OK != startMediaPort())
            {
                return RET_FAIL;
            }

            m_UdpPeerAddr[VIDEO_RTP_HANDLE].set(rtspMessage.getClientPort(),
                                                rtspMessage.getDestinationIp());
            m_UdpPeerAddr[VIDEO_RTCP_HANDLE].set(rtspMessage.getClientPort() + 1,
                                                 rtspMessage.getDestinationIp());
            SVS_LOG((SVS_LM_INFO,"session[%Q] start video channel[%s:%d].",
                            getStreamId(),
                            m_UdpPeerAddr[VIDEO_RTP_HANDLE].get_host_addr(),
                            m_UdpPeerAddr[VIDEO_RTP_HANDLE].get_port_number()));
        }
    }
    else
    {
        if (TRANS_PROTOCAL_TCP == rtspMessage.getTransType())
        {
            m_unAudioInterleaveNum = rtspMessage.getInterleaveNum();
        }
        else
        {
            m_UdpPeerAddr[AUDIO_RTP_HANDLE].set(rtspMessage.getClientPort(),
                                                rtspMessage.getDestinationIp());
            m_UdpPeerAddr[AUDIO_RTCP_HANDLE].set(rtspMessage.getClientPort() + 1,
                                                 rtspMessage.getDestinationIp());
            SVS_LOG((SVS_LM_INFO,"session[%Q] start audio channel[%s:%d].",
                            getStreamId(),
                            m_UdpPeerAddr[AUDIO_RTP_HANDLE].get_host_addr(),
                            m_UdpPeerAddr[AUDIO_RTP_HANDLE].get_port_number()));
        }
    }

    m_unStartTime++;

    setStatus(STREAM_SESSION_STATUS_WAIT_CHANNEL_REDAY);
    SVS_LOG((SVS_LM_INFO,"start std rtp session[%Q]  success.", getStreamId()));
    return RET_OK;
}

int32_t CStreamStdRtpSession::sendStartRequest()
{
    setStatus(STREAM_SESSION_STATUS_DISPATCHING);
    return RET_OK;
}

void CStreamStdRtpSession::setRtspHandle(ACE_HANDLE handle, const ACE_INET_Addr &addr)
{
    m_rtspHandle = handle;
    m_rtspAddr   = addr;
    return;
}

void CStreamStdRtpSession::setPlayLoad(uint16_t unVedioPT,uint16_t unAudioPT )
{
    m_unVedioPT = unVedioPT;
    m_unAudioPT = unAudioPT;
    return;
}

void CStreamStdRtpSession::setSessionId(uint64_t ullSessionId)
{
    m_stSessionInfo.StreamID = ullSessionId;
    return;
}

int32_t CStreamStdRtpSession::sendMessage(const char* pData, uint32_t unDataSize)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_TcpSendMutex);
    if (TRANS_PROTOCAL_UDP == m_unTransType)
    {
        ACE_Time_Value timeout(1);
        int32_t nSendSize = ACE::send_n(m_rtspHandle, pData, unDataSize, &timeout);
        if (unDataSize != (uint32_t) nSendSize)
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] send message fail, error[%d] close handle[%d].",
                            getStreamId(), ACE_OS::last_error(), m_rtspHandle));

            m_rtspHandle = ACE_INVALID_HANDLE;
            setStatus(STREAM_SESSION_STATUS_ABNORMAL);
            return RET_FAIL;
        }

        return RET_OK;
    }

    if (!m_TcpSendList.empty() && (RET_OK != sendLeastData()))
    {
        ACE_Message_Block *pMsgBlock = CMediaBlockBuffer::instance().allocMediaBlock();
        if (NULL == pMsgBlock)
        {
            SVS_LOG((SVS_LM_WARNING,"tcp connect send stream[%Q] message data fail, "
                    "alloc cache buffer fail, close handle.",
                    getStreamId()));
            m_rtspHandle = ACE_INVALID_HANDLE;
            setStatus(STREAM_SESSION_STATUS_ABNORMAL);
            return RET_ERR_DISCONNECT;
        }

        pMsgBlock->copy(pData, unDataSize);
        m_TcpSendList.push_back(pMsgBlock);

        return RET_OK;
    }

    int32_t nSendSize = ACE::send(m_rtspHandle, pData, unDataSize);
    if (0 >= nSendSize)
    {
        int32_t iErrorCode = ACE_OS::last_error();
        if (checkIsDisconnect(iErrorCode))
        {
            SVS_LOG((SVS_LM_WARNING,"stream[%Q] connect send message fail, errno[%d] "
                            ", close handle[%d].",
                            getStreamId(),
                            iErrorCode,
                            m_rtspHandle));

            m_rtspHandle = ACE_INVALID_HANDLE;
            setStatus(STREAM_SESSION_STATUS_ABNORMAL);
            return RET_ERR_DISCONNECT;
        }

        nSendSize = nSendSize > 0 ? nSendSize : 0;
    }
    else
    {
        /* tcp send for media heartbeat */
        m_ulLastRecvTime = SVS_GetSecondTime();
    }

    if (unDataSize != (uint32_t) nSendSize)
    {
        ACE_Message_Block *pMsgBlock =
                CMediaBlockBuffer::instance().allocMediaBlock();
        if (NULL == pMsgBlock)
        {
            SVS_LOG((SVS_LM_WARNING,"tcp connect send stream[%Q] media data fail, "
                    "alloc cache buffer fail, close handle.",
                    getStreamId()));
            m_rtspHandle = ACE_INVALID_HANDLE;
            setStatus(STREAM_SESSION_STATUS_ABNORMAL);
            return RET_ERR_DISCONNECT;
        }

        pMsgBlock->copy(pData + nSendSize, unDataSize - (uint32_t) nSendSize);
        m_TcpSendList.push_back(pMsgBlock);
    }

    return RET_OK;
}
int32_t CStreamStdRtpSession::initSesssion(PEER_TYPE unPeerType)
{
    m_stSessionInfo.SessionType    = RTSP_SESSION;
    m_stSessionInfo.PeerType       = unPeerType;
    m_stSessionInfo.TransDirection = TRANS_DIRECTION_SENDONLY;
    m_stSessionInfo.MediaTransType = MEDIA_TRANS_TYPE_RTP;
    m_stSessionInfo.TransProtocol  = TRANS_PROTOCAL_TCP;
    return RET_OK;
}

int32_t CStreamStdRtpSession::sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount)
{

    if (STREAM_SESSION_STATUS_DISPATCHING != getStatus())
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] discard media data, the status[%d] invalid.",
                        getStreamId(), getStatus()));
        return RET_OK;
    }

    if (TRANS_PROTOCAL_TCP == m_unTransType)
    {
        return sendTcpMediaData(pMbArray, MsgCount);
    }

    (void) sendUdpMediaData(pMbArray, MsgCount);

    return RET_OK;
}

uint32_t CStreamStdRtpSession::getMediaTransType()const
{
    return MEDIA_TRANS_TYPE_RTP;
}

int32_t CStreamStdRtpSession::sendVcrMessage(CRtspPacket &rtspPack)
{
    rtspPack.setSessionID((uint64_t)atoll(m_strRtspSessionId.c_str()));

    std::string strRtpInfo;
    rtspPack.getRtpInfo(strRtpInfo);
    if ("" != strRtpInfo)
    {
        string strSeq;
        string strRtptime;

        string::size_type nStartPos = strRtpInfo.find(";");
        if (string::npos == nStartPos)
        {
            SVS_LOG((SVS_LM_INFO,"session[%Q] send vcr message fail, RTP-Info[%s] invalid.",
                            getStreamId(), strRtpInfo.c_str()));
            return RET_FAIL;
        }
        strRtpInfo = strRtpInfo.substr(nStartPos);

        string strNewRtpInfo = "url=";
        strNewRtpInfo += m_strRtspUrl;
        strNewRtpInfo += strRtpInfo;
        rtspPack.setRtpInfo(strNewRtpInfo);
    }

    std::string strMsg;
    if (0 != rtspPack.generateRtspResp(strMsg))
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] send vcr message fail, generate rtsp resp message fail.",
                getStreamId()));
        return RET_FAIL;
    }

    uint32_t unTmp = 0;
    if (0 == rtspPack.getRangeTime(unTmp, unTmp, unTmp))
    {
        strMsg.erase(strMsg.length() - 2, strMsg.length());
        strMsg += "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, ANNOUNCE\r\n\r\n";
    }

    int32_t nRet = sendMessage(strMsg.c_str(), strMsg.length());
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send vcr message fail.", getStreamId()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] send vcr message success.", getStreamId()));
    SVS_LOG((SVS_LM_DEBUG,"%s", strMsg.c_str()));
    return RET_OK;
}

int32_t CStreamStdRtpSession::sendSessionStopMessage(uint32_t unStopType)
{
    CRtspAnnounceMessage msg;
    msg.setMsgType(RTSP_MSG_REQ);
    msg.setRtspUrl(m_strRtspUrl);
    msg.setSession(m_strRtspSessionId);
    msg.setCSeq(1);

    std::string strMsg;
    if (RET_OK != msg.encodeMessage(strMsg))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send vos message fail, encode announce msg fail.",
                        getStreamId()));
        return RET_FAIL;
    }


    return sendMessage(strMsg.c_str(), strMsg.length());
}

void CStreamStdRtpSession::setSdpInfo(CMediaSdp& rtspSdp)
{
    m_rtspSdp.copy(rtspSdp);
}
void CStreamStdRtpSession::getSdpInfo(CMediaSdp& rtspSdp)
{
    rtspSdp.copy(m_rtspSdp);
}
ACE_INET_Addr CStreamStdRtpSession::getPeerAddr()const
{
    return m_rtspAddr;
}

ACE_INET_Addr CStreamStdRtpSession::getMediaAddr()const
{
    if (TRANS_PROTOCAL_TCP == m_unTransType)
    {
        return m_rtspAddr;
    }
    else
    {
        return m_UdpPeerAddr[VIDEO_RTP_HANDLE];
    }
}

int32_t CStreamStdRtpSession::handleInnerMessage(const STREAM_INNER_MSG &innerMsg,
                                          uint32_t unMsgSize,
                                          CStreamSession&  peerSession)
{
    m_ulLastRecvTime = SVS_GetSecondTime();
    SVS_LOG((SVS_LM_INFO,"session[%Q] ,handle inner message,time:[%u],type:[%d].",
                                        getStreamId(), m_ulLastRecvTime,innerMsg.usMsgType));

    if (INNER_MSG_RTSP == innerMsg.usMsgType)
    {
        (void)peerSession.handleRecvedNatRequest();

        setStatus(STREAM_SESSION_STATUS_DISPATCHING);

        SVS_LOG((SVS_LM_INFO,"session[%Q] handle inner rtsp message success.", getStreamId()));
        return RET_OK;
    }

    if (INNER_MSG_RTCP == innerMsg.usMsgType)
    {
        return RET_OK;
    }
    if ((INNER_MSG_RTPDUMMY == innerMsg.usMsgType)
        ||(INNER_MSG_RTCPDUMMY == innerMsg.usMsgType))
    {
        int32_t nHandleIndex = 0;
        for (nHandleIndex = 0; nHandleIndex < HANDLE_TYPE_MAX; nHandleIndex++)
        {
            if (innerMsg.pRecvHandle == m_pUdpHandle[nHandleIndex])
            {
                break;
            }
        }

        if ((HANDLE_TYPE_MAX <= nHandleIndex) || (NULL == m_pUdpHandle[nHandleIndex]))
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] handle dummy message fail, recv handle invalid.",
                    getStreamId()));
            return RET_FAIL;
        }

        m_UdpPeerAddr[nHandleIndex].set(innerMsg.usRemotePort, innerMsg.unRemoteIp);
        SVS_LOG((SVS_LM_INFO,"session[%Q] handle inner [%d]dummy message success,remoteAddr:[%s] remoteport:[%d].",
                                            getStreamId(),innerMsg.usMsgType,
                                            m_UdpPeerAddr[nHandleIndex].get_host_addr(),
                                            m_UdpPeerAddr[nHandleIndex].get_port_number()));
        return RET_OK;
    }

    SVS_LOG((SVS_LM_WARNING,"session[%Q] handle not accepted inner message[%d].",
                    getStreamId(), innerMsg.usMsgType));
    return RET_FAIL;
}

int32_t CStreamStdRtpSession::allocMediaPort()
{
    if (TRANS_PROTOCAL_TCP == m_unTransType)
    {
        return RET_FAIL;
    }

    int32_t nRet = CStreamPortManager::instance()->allocRtpUdpPort(getSpecifyIp(),
            m_pUdpHandle[VIDEO_RTP_HANDLE], m_pUdpHandle[VIDEO_RTCP_HANDLE]);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"session[%Q] alloc video media port fail.", getStreamId()));
        return RET_FAIL;
    }
    m_VideoAddr.set(m_pUdpHandle[VIDEO_RTP_HANDLE]->getLocalAddr());

    nRet = CStreamPortManager::instance()->allocRtpUdpPort(getSpecifyIp(),
            m_pUdpHandle[AUDIO_RTP_HANDLE], m_pUdpHandle[AUDIO_RTCP_HANDLE]);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"session[%Q] alloc audio media port fail.",  getStreamId()));
        return RET_FAIL;
    }
    m_AudioAddr.set(m_pUdpHandle[AUDIO_RTP_HANDLE]->getLocalAddr());

    SVS_LOG((SVS_LM_INFO,
            "Rtp session alloc media port success. stream id[%Q] "
            "video rtp handle[%p] video rtcp handle[%p] "
            "audio rtp handle[%p] audio rtcp handle[%p].",
            getStreamId(),
            m_pUdpHandle[VIDEO_RTP_HANDLE],
            m_pUdpHandle[VIDEO_RTCP_HANDLE],
            m_pUdpHandle[AUDIO_RTP_HANDLE],
            m_pUdpHandle[AUDIO_RTCP_HANDLE]));

    return RET_OK;
}

int32_t CStreamStdRtpSession::startMediaPort()
{
    if (TRANS_PROTOCAL_TCP == m_unTransType)
    {
        return RET_FAIL;
    }


    if ((NULL == m_pUdpHandle[VIDEO_RTP_HANDLE])
            || (NULL == m_pUdpHandle[VIDEO_RTCP_HANDLE]))
    {
        SVS_LOG((SVS_LM_ERROR,"session[%Q] start video port fail, handle  is null.",
                getStreamId()));
        return RET_FAIL;
    }
    int32_t iRet = m_pUdpHandle[VIDEO_RTP_HANDLE]->startHandle(getStreamId(),
                                                m_UdpPeerAddr[VIDEO_RTP_HANDLE]);
    if (RET_OK != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,"session[%Q] start video rtp port fail.", getStreamId()));
        return RET_FAIL;
    }

    iRet = m_pUdpHandle[VIDEO_RTCP_HANDLE]->startHandle(getStreamId(),
                                                m_UdpPeerAddr[VIDEO_RTCP_HANDLE]);
    if (RET_OK != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,"session[%Q] start video rtcp port fail.", getStreamId()));
        return RET_FAIL;
    }

    if ((NULL == m_pUdpHandle[AUDIO_RTP_HANDLE])
            || (NULL == m_pUdpHandle[AUDIO_RTCP_HANDLE]))
    {
        SVS_LOG((SVS_LM_ERROR,"session[%Q] start audio port fail, handle  is null.",
                getStreamId()));
        return RET_FAIL;
    }
    iRet = m_pUdpHandle[AUDIO_RTP_HANDLE]->startHandle(getStreamId(),
                                                        m_UdpPeerAddr[AUDIO_RTP_HANDLE]);
    if (RET_OK != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,"session[%Q] start audio rtp port fail.", getStreamId()));
        return RET_FAIL;
    }

    iRet = m_pUdpHandle[AUDIO_RTCP_HANDLE]->startHandle(getStreamId(),
                                                        m_UdpPeerAddr[AUDIO_RTCP_HANDLE]);
    if (RET_OK != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,"session[%Q] start audio rtcp port fail.", getStreamId()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"Start media port success. stream id[%Q].",
            getStreamId()));
    return RET_OK;
}

int32_t CStreamStdRtpSession::stopMediaPort()
{
    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] stop tcp media port success.",
                getStreamId()));
        return RET_OK;
    }

    for (int32_t i = 0; i < HANDLE_TYPE_MAX; i++)
    {
        if (NULL != m_pUdpHandle[i])
        {
            (void) m_pUdpHandle[i]->stopHandle(getStreamId());
        }
    }
    SVS_LOG((SVS_LM_INFO,"Rtp udp session stop port success. stream id[%Q].",
                    getStreamId()));
    return RET_OK;
}

bool CStreamStdRtpSession::checkIsDisconnect(int32_t nErrNo) const
{
    if (EAGAIN == nErrNo
            || ETIME == nErrNo
            || EWOULDBLOCK == nErrNo
            || ETIMEDOUT == nErrNo
            || EINTR == nErrNo)
    {
        return false;
    }
    return true;
}

int32_t CStreamStdRtpSession::saveLeastData(ACE_Message_Block ** const pMbArray, uint32_t MsgCount,
                                     uint32_t nSendSize, uint32_t nSendCount)
{
    if (NULL == pMbArray)
    {
        return RET_FAIL;
    }

    uint32_t i = nSendCount;
    for (; i < MsgCount; i++)
    {
        if (NULL == pMbArray[i])
        {
            return RET_FAIL;
        }

        ACE_Message_Block *pMsgBlock = CMediaBlockBuffer::instance().allocMediaBlock();
        if (!pMsgBlock)
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] alloc media data block fail, close handle[%d]",
                    getStreamId(), m_rtspHandle));

            m_rtspHandle = ACE_INVALID_HANDLE;
            setStatus(STREAM_SESSION_STATUS_ABNORMAL);
            return RET_ERR_DISCONNECT;
        }

        if ((0 == nSendSize) || (RTP_INTERLEAVE_LENGTH > nSendSize))
        {
            CRtpPacket rtpPack;
            if (RET_OK != rtpPack.ParsePacket(pMbArray[i]->rd_ptr(),
                    pMbArray[i]->length()))
            {
                CMediaBlockBuffer::instance().freeMediaBlock(pMsgBlock);
                SVS_LOG((SVS_LM_WARNING,"session[%Q] save send meida fail, parse data as rtp fail, "
                        "msg[%p] len[%u], close handle[%d].",
                        getStreamId(), pMbArray[i], pMbArray[i]->length(), m_rtspHandle));

                m_rtspHandle = ACE_INVALID_HANDLE;
                setStatus(STREAM_SESSION_STATUS_ABNORMAL);
                return RET_ERR_DISCONNECT;
            }

            char cPayloadType = rtpPack.GetPayloadType();
            char dataBuf[RTP_INTERLEAVE_LENGTH] = { 0 };
            dataBuf[0] = RTP_INTERLEAVE_FLAG;
            if ((char) m_unVedioPT == cPayloadType)
            {
                dataBuf[1] = (char) m_unVideoInterleaveNum;
            }
            else
            {
                dataBuf[1] = (char) m_unAudioInterleaveNum;
            }
            *(uint16_t*) &dataBuf[2] = htons(
                    (uint16_t) pMbArray[i]->length());

            (void) pMsgBlock->copy(dataBuf, RTP_INTERLEAVE_LENGTH - nSendSize);
            nSendSize = 0;
        }

        if (0 != nSendSize)
        {
            nSendSize -= RTP_INTERLEAVE_LENGTH;
        }

        (void) pMsgBlock->copy(pMbArray[i]->rd_ptr() + nSendSize,
                                pMbArray[i]->length() - nSendSize);
        m_TcpSendList.push_back(pMsgBlock);

        nSendSize = 0;
    }

    return RET_OK;    //lint !e818
} //lint !e818

int32_t CStreamStdRtpSession::sendLeastData()
{
    while (!m_TcpSendList.empty())
    {
        ACE_Message_Block *pMsg = m_TcpSendList.front();
        int32_t nRet = ACE::send(m_rtspHandle, pMsg->rd_ptr(), pMsg->length());
        if (0 >= nRet)
        {
            int32_t iErrorCode = ACE_OS::last_error();
            if (checkIsDisconnect(iErrorCode))
            {
                SVS_LOG((SVS_LM_WARNING,"stream[%Q] send tcp least data fail, "
                                    "errno[%d], close handle[%d].",
                                    getStreamId(),
                                    iErrorCode,
                                    m_rtspHandle));

                m_rtspHandle = ACE_INVALID_HANDLE;
                setStatus(STREAM_SESSION_STATUS_ABNORMAL);

                return RET_ERR_DISCONNECT;
            }
            nRet = nRet > 0 ? nRet : 0;
        }
        else
        {
            /* tcp send for media heartbeat */
            m_ulLastRecvTime = SVS_GetSecondTime();
        }

        if (pMsg->length() != (uint32_t) nRet)
        {
            pMsg->rd_ptr((uint32_t) nRet);
            return RET_FAIL;
        }

        CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
        m_TcpSendList.pop_front();
    }

    return RET_OK;
}

/*lint -e818*/
int32_t CStreamStdRtpSession::sendUdpMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount)
{
    if (NULL == pMbArray)
    {
        return RET_FAIL;
    }

    for (uint32_t unSendCount = 0; unSendCount < MsgCount; unSendCount++)
    {
        if (NULL == pMbArray[unSendCount])
        {
            return RET_FAIL;
        }

        CRtpPacket rtpPack;
        if (RET_OK != rtpPack.ParsePacket(pMbArray[unSendCount]->rd_ptr(),
                                          pMbArray[unSendCount]->length()))
        {
            SVS_LOG((SVS_LM_ERROR,
                "session[%Q] send media data fail, parse rtp packet fail.",
                getStreamId()));
            return -1;
        }

        char cPt = rtpPack.GetPayloadType();
        if ((char)m_unVedioPT == cPt )
        {
            return m_pUdpHandle[VIDEO_RTP_HANDLE]->sendMessage(getStreamId(),
                                                                pMbArray[unSendCount]->rd_ptr(),
                                                                pMbArray[unSendCount]->length(),
                                                                m_UdpPeerAddr[VIDEO_RTP_HANDLE]);
        }
        else
        {
            return m_pUdpHandle[AUDIO_RTP_HANDLE]->sendMessage(getStreamId(),
                                                                pMbArray[unSendCount]->rd_ptr(),
                                                                pMbArray[unSendCount]->length(),
                                                                m_UdpPeerAddr[AUDIO_RTP_HANDLE]);
        }
    }

    return -1;
}

int32_t CStreamStdRtpSession::sendTcpMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_TcpSendMutex);
    if (!m_TcpSendList.empty() && (RET_OK != sendLeastData()))
    {
        return RET_ERR_SEND_FAIL;
    }

    uint32_t unCount = 0;
    int32_t nSendSize = 0;
    for (; unCount < MsgCount; unCount++)
    {
        if (NULL == pMbArray[unCount])
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] send meida fail, data block is null, ",
                             getStreamId()));
            return RET_FAIL;
        }

        CRtpPacket rtpPack;
        if (RET_OK != rtpPack.ParsePacket(pMbArray[unCount]->rd_ptr(), pMbArray[unCount]->length()))
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] send meida fail, parse data as rtp fail, "
                        "msg[%p] len[%u].",
                       getStreamId(), pMbArray[unCount], pMbArray[unCount]->length()));
            return RET_FAIL;
        }

        char interleaveData[RTP_INTERLEAVE_LENGTH] = {0};
        char cPt = rtpPack.GetPayloadType();
        interleaveData[0] = RTP_INTERLEAVE_FLAG;

        if ((char)m_unVedioPT == cPt)
        {
            interleaveData[1] = (char)m_unVideoInterleaveNum;
        }
        else
        {
            interleaveData[1] = (char)m_unAudioInterleaveNum;
        }
        *(uint16_t*)&interleaveData[2] = htons((uint16_t)pMbArray[unCount]->length());

        struct iovec dataVec[2];
        dataVec[0].iov_len  = RTP_INTERLEAVE_LENGTH;
        dataVec[0].iov_base = interleaveData;
        dataVec[1].iov_len  = pMbArray[unCount]->length();
        dataVec[1].iov_base = pMbArray[unCount]->rd_ptr();

        nSendSize = ACE::sendv(m_rtspHandle, dataVec, 2);
        if (0 >= nSendSize)
        {
            int32_t iErrorCode = ACE_OS::last_error();
            if (checkIsDisconnect(iErrorCode))
            {
                SVS_LOG((SVS_LM_WARNING,
                    "stream[%Q] send tcp least data fail, errno[%d], close handle[%d].",
                    getStreamId(), iErrorCode, m_rtspHandle));
                m_rtspHandle = ACE_INVALID_HANDLE;
                setStatus(STREAM_SESSION_STATUS_ABNORMAL);
                return RET_ERR_DISCONNECT;
            }

            nSendSize = nSendSize > 0 ? nSendSize : 0;
        }
        else
        {
            /* tcp send for media heartbeat */
            m_ulLastRecvTime = SVS_GetSecondTime();
        }

        if (pMbArray[unCount]->length() + RTP_INTERLEAVE_LENGTH != (uint32_t)nSendSize)
        {
            break;
        }
    }

    if (unCount < MsgCount)
    {
        return saveLeastData(pMbArray, MsgCount, (uint32_t)nSendSize, unCount);
    }

    return RET_OK;
}
/*lint +e818*/

bool CStreamStdRtpSession::checkMediaChannelStatus()
{
    uint32_t ulCostTime = SVS_GetSecondTime() - m_ulLastRecvTime;
    if (ulCostTime > STREAM_MEDIA_CHANNEL_INVAILD_INTERVAL)
    {
        SVS_LOG((SVS_LM_WARNING,"std session[%Q] not recv data at [%u]s, check media channel status fail.",
                         getStreamId(), ulCostTime));
        return false;
    }

    sendRtcpReport();

    return true;
}

void CStreamStdRtpSession::sendRtcpReport()
{
    char buf[KILO] = { 0 };
    char* pRtcpBuff = buf + RTP_INTERLEAVE_LENGTH;
    uint32_t unRtcpLen = 0;
    (void) m_rtcpPacket.createSenderReport(pRtcpBuff, KILO - RTP_INTERLEAVE_LENGTH,unRtcpLen);

    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        buf[0] = RTP_INTERLEAVE_FLAG;
        buf[1] = (char)m_unVideoInterleaveNum + 1;
        *(uint16_t*) &buf[2] = htons((uint16_t)unRtcpLen);
        (void)sendMessage(buf, unRtcpLen + RTP_INTERLEAVE_LENGTH);

        buf[0] = RTP_INTERLEAVE_FLAG;
        buf[1] = (char)m_unAudioInterleaveNum + 1;
        *(uint16_t*) &buf[2] = htons((uint16_t)unRtcpLen);
        (void)sendMessage(buf, unRtcpLen + RTP_INTERLEAVE_LENGTH);
    }
    else
    {
        if (!m_pUdpHandle[VIDEO_RTCP_HANDLE])
        {
            SVS_LOG((SVS_LM_INFO,"session[%Q] send rtcp fail, video rtcp handle invalid.", getStreamId()));
            return;
        }

        (void) m_pUdpHandle[VIDEO_RTCP_HANDLE]->sendMessage(getStreamId(), pRtcpBuff,
                unRtcpLen, m_UdpPeerAddr[VIDEO_RTCP_HANDLE]);

        if (!m_pUdpHandle[AUDIO_RTCP_HANDLE])
        {
            SVS_LOG((SVS_LM_INFO,"session[%Q] send rtcp fail, audio rtcp handle invalid.", getStreamId()));
            return;
        }
        (void) m_pUdpHandle[AUDIO_RTCP_HANDLE]->sendMessage(getStreamId(), pRtcpBuff,
                unRtcpLen, m_UdpPeerAddr[AUDIO_RTCP_HANDLE]);
    }

    SVS_LOG((SVS_LM_INFO,"std session[%Q] send rtcp sender report", getStreamId()));
    return;
}
