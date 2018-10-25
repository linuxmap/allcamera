/*
 * MduStdRtpSession.cpp
 *
 *  Created on: 2016-5-20
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_time.h"
#include "svs_adapter_rtmp_session.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_rtp_packet.h"
#include "svs_adapter_rtp_tcp_connect.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_block_buffer.h"

CMduRtmpSession::CMduRtmpSession()
{
    m_pHandle         = NULL;
    m_strRtspUrl      = "";

    m_unVideoInterleaveNum = 0;
    m_unAudioInterleaveNum = 2;
    m_ullPeerStreamID = 0;

    m_ulLastRecvTime  = SVS_GetSecondTime();
}

CMduRtmpSession::~CMduRtmpSession()
{

}

int32_t CMduRtmpSession::initRtmpSession(uint64_t ullPeerStreamID,
                                         PLAY_TYPE      enPlayType,
                                         const ACE_INET_Addr &localAddr,
                                         const ACE_INET_Addr &/*peerAddr*/)
{
    m_ullPeerStreamID = ullPeerStreamID;
    CMduSession *pPeerSession = CMduSessionFactory::instance()->findSession(m_ullPeerStreamID);
    if (!pPeerSession)
    {
        SVS_LOG((SVS_LM_WARNING,"init rtmp session[%Q] fail, can't find peer session[%Q].",
                getStreamId(), ullPeerStreamID));
        return RET_FAIL;
    }

    m_stSessionInfo.SessionType = RTMP_SESSION;
    m_enPlayType   = enPlayType;
    m_ulVideoCodeType = pPeerSession->getVideoCodeType();
    m_stSessionInfo.PeerType = PEER_TYPE_CU;
    m_stSessionInfo.RecvStreamID = ullPeerStreamID;
    m_stSessionInfo.TransDirection = TRANS_DIRECTION_SENDONLY;
    m_stSessionInfo.SpecifyIp  = localAddr.get_ip_address();


    m_ulVideoCodeType = pPeerSession->getVideoCodecType();

    CMduSessionFactory::instance()->releaseSession(pPeerSession);

    setStatus(MDU_SESSION_STATUS_WAIT_START);

    SVS_LOG((SVS_LM_INFO,"init rtmp session[%Q] service type[%d] success.",
                    getStreamId(), getPlayType()));
    return RET_OK;
}

int32_t CMduRtmpSession::startRtmpSession()
{

    m_stSessionInfo.TransProtocol = TRANS_PROTOCAL_TCP;

    setStatus(MDU_SESSION_STATUS_WAIT_CHANNEL_REDAY);
    SVS_LOG((SVS_LM_INFO,"start rtmp session[%Q]  success.", getStreamId()));
    return RET_OK;
}

int32_t CMduRtmpSession::sendStartRequest()
{
    setStatus(MDU_SESSION_STATUS_DISPATCHING);
    return RET_OK;
}

void CMduRtmpSession::setRtmpHandle(CMduRtmpSendHandle* pHandle)
{
    m_pHandle    = pHandle;
    m_rtmpAddr   = pHandle->getPeerAddr();
    return;
}

void CMduRtmpSession::setPlayLoad(uint16_t unVedioPT,uint16_t unAudioPT )
{
    m_unVedioPT = unVedioPT;
    m_unAudioPT = unAudioPT;
    return;
}

void CMduRtmpSession::setSessionId(uint64_t ullSessionId)
{
    m_stSessionInfo.StreamID = ullSessionId;
    return;
}

int32_t CMduRtmpSession::initSesssion(PEER_TYPE unPeerType)
{
    m_stSessionInfo.SessionType = RTMP_SESSION;
    m_stSessionInfo.PeerType = unPeerType;
    m_stSessionInfo.TransDirection = TRANS_DIRECTION_SENDONLY;
    m_stSessionInfo.MediaTransType = MEDIA_TRANS_TYPE_RTMP;
    m_stSessionInfo.TransProtocol = TRANS_PROTOCAL_TCP;
    return RET_OK;
}

int32_t CMduRtmpSession::sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount)
{

    if (MDU_SESSION_STATUS_DISPATCHING != getStatus())
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] discard media data, the status[%d] invalid.",
                        getStreamId(), getStatus()));
        return RET_OK;
    }
    if(NULL == m_pHandle) {
        return RET_FAIL;
    }

    return m_pHandle->sendMediaData(pMbArray, MsgCount);
}

uint32_t CMduRtmpSession::getMediaTransType()const
{
    return MEDIA_TRANS_TYPE_RTMP;
}

int32_t CMduRtmpSession::sendVcrMessage(CRtspPacket &rtspPack)
{
    SVS_LOG((SVS_LM_INFO,"session[%Q] send vcr message success.", getStreamId()));
    return RET_OK;
}

int32_t CMduRtmpSession::sendSessionStopMessage(uint32_t unStopType)
{
    SVS_LOG((SVS_LM_INFO,"session[%Q] send stop message success.", getStreamId()));
    return RET_OK;
}

void CMduRtmpSession::setSdpInfo(CMediaSdp& rtspSdp)
{
    m_rtspSdp.copy(rtspSdp);
}
void CMduRtmpSession::getSdpInfo(CMediaSdp& rtspSdp)
{
    rtspSdp.copy(m_rtspSdp);
}
ACE_INET_Addr CMduRtmpSession::getPeerAddr()const
{
    return m_rtmpAddr;
}

ACE_INET_Addr CMduRtmpSession::getMediaAddr()const
{
    return m_rtmpAddr;
}

int32_t CMduRtmpSession::handleInnerMessage(const MDU_INNER_MSG &innerMsg,
                                          uint32_t unMsgSize,
                                          CMduSession&  peerSession)
{
    m_ulLastRecvTime = SVS_GetSecondTime();

    SVS_LOG((SVS_LM_WARNING,"session[%Q] handle accepted inner message[%d].",
                    getStreamId(), innerMsg.usMsgType));
    return RET_OK;
}

int32_t CMduRtmpSession::allocMediaPort()
{
    return RET_OK;
}

int32_t CMduRtmpSession::startMediaPort()
{
    SVS_LOG((SVS_LM_INFO,"Start media port success. stream id[%Q].",
            getStreamId()));
    return RET_OK;
}

int32_t CMduRtmpSession::stopMediaPort()
{
    SVS_LOG((SVS_LM_INFO,"Rtp udp session stop port success. stream id[%Q].",
                    getStreamId()));
    return RET_OK;
}


/*lint +e818*/

bool CMduRtmpSession::checkMediaChannelStatus()
{
    uint32_t ulCostTime = SVS_GetSecondTime() - m_ulLastRecvTime;
    if (ulCostTime > MDU_MEDIA_CHANNEL_INVAILD_INTERVAL)
    {
        SVS_LOG((SVS_LM_WARNING,"std session[%Q] not recv data at [%u]s, check media channel status fail.",
                         getStreamId(), ulCostTime));
        return false;
    }

    return true;
}

