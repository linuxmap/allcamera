#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_time.h"

#include "svs_adapter_ehome_session.h"
#include "svs_adapter_port_manager.h"
#include "svs_adapter_rtp_tcp_connect.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_exchange.h"
#include "svs_adapter_session_factory.h"

#include "svs_adapter_sdp.h"
#include "svs_rtsp_describe_message.h"
#include "svs_rtsp_play_message.h"
#include "svs_rtsp_pause_message.h"
#include "svs_rtsp_teardown_message.h"
#include "svs_rtsp_announce_message.h"
#include "svs_rtsp_options_message.h"
#include "svs_adapter_vms_media.h"
#include "svs_vms_msg_factory.h"
#include "svs_vms_playback_control_req.h"
#include "svs_adapter_business_manager.h"

CStreamEhomeSession::CStreamEhomeSession()
{
    m_pPeerSession         = NULL;
    m_lEhomeSessionID      = -1;
}

CStreamEhomeSession::~CStreamEhomeSession()
{
}

int32_t CStreamEhomeSession::initSesssion(PEER_TYPE unPeerType)
{
    m_stSessionInfo.SessionType    = RTP_SESSION;
    m_stSessionInfo.PeerType       = unPeerType;
    m_stSessionInfo.TransDirection = TRANS_DIRECTION_RECVONLY;
    m_stSessionInfo.MediaTransType = MEDIA_TRANS_TYPE_TCP;
    m_stSessionInfo.TransProtocol  = TRANS_PROTOCAL_UDP;
    m_stSessionInfo.SpecifyIp      = CStreamConfig::instance()->getInternalMediaIp();
    return RET_OK;
}


int32_t CStreamEhomeSession::sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount)
{
    return RET_OK;
}

int32_t CStreamEhomeSession::handleInnerMessage(const STREAM_INNER_MSG &innerMsg,
                                       uint32_t unMsgSize,
                                       CStreamSession &peerSession)
{
    if( INNER_MSG_RTCP == innerMsg.usMsgType )
    {
        SVS_LOG((SVS_LM_INFO,"rtp session[%Q] handle rtcp message success.",
                         getStreamId()));
        return RET_OK;
    }

    return RET_OK;
}

int32_t CStreamEhomeSession::sendVcrMessage(CRtspPacket &rtspPack)
{
    return RET_OK;
}

int32_t CStreamEhomeSession::dealSccVcrResp(const SVS_MSG_PLAYBACK_CONTROL_RESP* pMsg)
{
    return RET_OK;
}
int32_t CStreamEhomeSession::sendSessionStopMessage(uint32_t unStopType)
{
    return RET_OK;
}


ACE_INET_Addr CStreamEhomeSession::getPeerAddr()const
{

    return m_EhomePeerAddr;
}

ACE_INET_Addr CStreamEhomeSession::getMediaAddr()const
{
    return getPeerAddr();
}




int32_t CStreamEhomeSession::sendNatResponse(CStreamNatMessage &natMsg)
{
    return RET_OK;
}


int32_t CStreamEhomeSession::allocMediaPort()
{
    CHandle* pHandle = NULL;
    int32_t nRet = CStreamPortManager::instance()->allocEhomePort(getSpecifyIp(),pHandle);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"stream[%Q] alloc video media port fail.",
                getStreamId()));
        return RET_FAIL;
    }
    m_pEhomeHandle = (CEhomeHandle*)pHandle;
    m_VideoAddr.set(m_pEhomeHandle->getLocalAddr());
    m_AudioAddr.set(m_pEhomeHandle->getLocalAddr());

    return RET_OK;
}

int32_t CStreamEhomeSession::startMediaPort()
{
    int32_t iRet = m_pEhomeHandle->startHandle(getStreamId(),m_EhomePeerAddr);
    if (RET_OK != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,"Start session[%Q] audio rtcp port fail.", getStreamId()));
        return RET_FAIL;
    }
    else
    {
        m_pEhomeHandle->setEhomeSessionID(getStreamId(),m_lEhomeSessionID);
    }
    return iRet;
}

int32_t CStreamEhomeSession::stopMediaPort()
{

    if (NULL != m_pEhomeHandle)
    {
        m_pEhomeHandle->stopHandle(getStreamId());
    }
    SVS_LOG((SVS_LM_INFO,"ehome session stop port success. stream id[%Q].",
            getStreamId()));

    return RET_OK;
}

bool CStreamEhomeSession::checkMediaChannelStatus()
{
    bool bRet = true;


    return bRet;
}

int32_t CStreamEhomeSession::setRemoteAddress()
{
    return RET_OK;
}

int32_t CStreamEhomeSession::sendStartRequest()
{
    return RET_OK;
}


