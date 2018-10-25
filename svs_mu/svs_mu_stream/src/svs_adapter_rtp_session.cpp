/*
 * MduRtpSession.cpp
 *
 *  Created on: 2016-2-10
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_time.h"

#include "svs_adapter_rtp_session.h"
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
//#include "svs_adapter_business.h"

CMduRtpSession::CMduRtpSession() : m_rtspInitSem(0)
{
    memset(m_pUdpHandle, 0x0, sizeof(CNetworkHandle*) * HANDLE_TYPE_MAX);

    m_pTcpHandle    = NULL;
    m_bHasPeerAddr  = false;
    m_ulLastNatTime = SVS_GetSecondTime();

    (void)m_rtspProtocol.init();

    m_pRtspHandle          = NULL;
    m_enSetupStatus        = SETUP_STATUS_NONE;
    m_strVideoSession      = "";
    m_strAudioSession      = "";
    m_bAudioPlay           = false;

    m_pPeerSession         = NULL;

    m_unRangeType          = RELATIVE_TIME;
    m_unRangeStart         = 0;
    m_unRangeStop          = 0;

    m_timeStamp               = 0;
    bPlaybackDragFlag      = false;
}

CMduRtpSession::~CMduRtpSession()
{
    try
    {
        m_pRtspHandle          = NULL;
        // �ر�TCP����
        if (NULL != m_pTcpHandle)
        {
            m_pTcpHandle->stopHandle(getStreamId());
            CNetworkHandle *pHandle = m_pTcpHandle;
            (void) CMduPortManager::instance()->releaseRtpTcpPort(getSpecifyIp(), pHandle);
            SVS_LOG((SVS_LM_INFO,"Rtp tcp session stop port success. stream id[%Q].",
                        getStreamId()));
        }

       // �ر�����UDP�˿�
        for (int32_t i = 0; i < HANDLE_TYPE_MAX; i++)
        {
            if (NULL != m_pUdpHandle[i])
            {
                (void) m_pUdpHandle[i]->stopHandle(getStreamId());
            }
        }
        (void) CMduPortManager::instance()->releaseRtpUdpPort(getSpecifyIp(),
                                                              m_pUdpHandle[VIDEO_RTP_HANDLE],
                                                              m_pUdpHandle[VIDEO_RTCP_HANDLE]);
        (void) CMduPortManager::instance()->releaseRtpUdpPort(getSpecifyIp(),
                                                              m_pUdpHandle[AUDIO_RTP_HANDLE],
                                                              m_pUdpHandle[AUDIO_RTCP_HANDLE]);
        memset(m_pUdpHandle, 0x0, sizeof(CNetworkHandle*) * HANDLE_TYPE_MAX);

        // �ͷŶԶ˻Ự
        if (NULL != m_pPeerSession)
        {
            CMduSessionFactory::instance()->releaseSession(m_pPeerSession);
            m_pPeerSession = NULL;
        }

        SVS_LOG((SVS_LM_INFO,"Rtp udp session stop port success. stream id[%Q].",
                getStreamId()));
    }
    catch(...)
    {}

    m_pTcpHandle = NULL;
}

int32_t CMduRtpSession::initSesssion(PEER_TYPE unPeerType)
{
    m_stSessionInfo.SessionType    = RTP_SESSION;
    m_stSessionInfo.PeerType       = unPeerType;
    m_stSessionInfo.TransDirection = TRANS_DIRECTION_RECVONLY;
    m_stSessionInfo.MediaTransType = MEDIA_TRANS_TYPE_RTP;
    m_stSessionInfo.TransProtocol  = TRANS_PROTOCAL_UDP;
    m_stSessionInfo.SpecifyIp      = CMduConfig::instance()->getInternalMediaIp();
    return RET_OK;
}


int32_t CMduRtpSession::sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount)
{
    if (NULL == pMbArray)
    {
        SVS_LOG((SVS_LM_ERROR,"session[%Q] send media data fail, mb is null.",
                getStreamId()));
        return RET_FAIL;
    }

    if (MDU_SESSION_STATUS_DISPATCHING != getStatus())
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] discard media data, the status[%d] invalid.",
                        getStreamId(), getStatus()));
        return RET_OK;
    }

    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        return sendTcpMediaData(pMbArray, MsgCount);
    }

    (void) sendUdpMediaData(pMbArray, MsgCount);

    return RET_OK;
}

int32_t CMduRtpSession::handleInnerMessage(const MDU_INNER_MSG &innerMsg,
                                       uint32_t unMsgSize,
                                       CMduSession &peerSession)
{
    if( INNER_MSG_RTCP == innerMsg.usMsgType )
    {
        SVS_LOG((SVS_LM_INFO,"rtp session[%Q] handle rtcp message success.",
                         getStreamId()));
        return RET_OK;
    }

    if( INNER_MSG_RTSP != innerMsg.usMsgType )
    {
        SVS_LOG((SVS_LM_INFO,"rtp session[%Q] handle not accepted inner message.",
                            getStreamId()));
        return RET_FAIL;
    }

    if (RTSP_SESSION == getSessionType())
    {
        return handleStdRtspMessage(innerMsg, unMsgSize, peerSession);
    }
    return handleRtspMessage(innerMsg, unMsgSize, peerSession);
}

int32_t CMduRtpSession::sendVcrMessage(CRtspPacket &rtspPack)
{
    rtspPack.setSessionID(getStreamId());
    rtspPack.setRtspUrl(m_strRtspUrl);

    if (RTSP_SESSION == getSessionType())
    {
        return sendStdVcrMessage(rtspPack);
    }


    if ((PEER_TYPE_STREAM == getPeerType()) && (CONN_SETUP_TYPE_ACTIVE == m_unConnSetupType))
    {
        rtspPack.setSessionID(m_ullConnStreamID);
    }

    if (PEER_TYPE_PU == getPeerType())
    {
        rtspPack.setMethodIndex(RtspSetParameterMethod);
    }

    if (PEER_TYPE_CU == getPeerType())
    {
        if (0 == rtspPack.getCseq())
        {
            SVS_LOG((SVS_LM_INFO,"session[%Q] need not send msg, seq is 0.", getStreamId()));
            return RET_OK;
        }
    }


    std::string strMsg;
    if (RtspResponseMethod != rtspPack.getMethodIndex())
    {
        char szRtspUrl[KILO] =  { 0 };
        (void) snprintf(szRtspUrl, KILO, "rtsp://%s:%d/%s.sdp",
                            m_TcpPeerAddr.get_host_addr(),
                            m_TcpPeerAddr.get_port_number(),
                            getContentID());
        rtspPack.setRtspUrl(szRtspUrl);
        if (0 != rtspPack.generateRtspReq(strMsg))
        {
            SVS_LOG((SVS_LM_INFO,"session[%Q] send vcr message fail, generate rtsp req message fail.",
                    getStreamId()));
            return RET_FAIL;
        }
    }
    else
    {
        if (0 != rtspPack.generateRtspResp(strMsg))
        {
            SVS_LOG((SVS_LM_INFO,"session[%Q] send vcr message fail, generate rtsp resp message fail.",
                    getStreamId()));
            return RET_FAIL;
        }
    }

    int32_t nRet;
    if(TRANS_PROTOCAL_UDP == getTransProtocol())
    {
       nRet =  sendMessage((void*)m_pUdpHandle[VIDEO_RTP_HANDLE], strMsg.c_str(), strMsg.size());
    }
    else
    {
        nRet = sendMessage((void*)m_pTcpHandle, strMsg.c_str(), strMsg.size());
    }
   // int32_t nRet = sendMessage((void*)m_pTcpHandle, strMsg.c_str(), strMsg.size());
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send vcr message fail.", getStreamId()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] send vcr message success.",
                    getStreamId()));
    SVS_LOG((SVS_LM_DEBUG,"%s", strMsg.c_str()));
    return RET_OK;
}

int32_t CMduRtpSession::parseRtspVcrMessage(const CRtspPacket &rtspMsg,
                                        uint32_t &unOperCode,
                                        uint32_t &unPlayTimeType,
                                        uint32_t &unPlayStartTime,
                                        uint32_t &unPlayStopTime,
                                        uint32_t &unPlayRate) const
{
    int32_t iRet;
    switch (rtspMsg.getMethodIndex())
    {
    case RtspPauseMethod:
    {
        unOperCode  = OPER_CODE_PAUSE;
        SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp vcr message: Pause.",
                        getStreamId()));
        break;
    }

    case RtspOptionsMethod:
        unOperCode = OPER_CODE_QUERY_TIME;
        SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp vcr message: OPTIONS.",
                         getStreamId()));
        break;
    case RtspPlayMethod:
    {
        unOperCode = OPER_CODE_DRAG;
        iRet = rtspMsg.getRangeTime(unPlayTimeType, unPlayStartTime, unPlayStopTime);
        if (-1 == iRet)
        {
            SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp range headerfail.",
                    getStreamId()));
            return RET_FAIL;
        }

        if (0 == iRet)
        {
            SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp vcr message: Drag, "
                    "time type[%u] start time[%u] stop time[%u].",
                    getStreamId(),
                    unPlayTimeType, unPlayStartTime, unPlayStopTime));

            break;
        }

        double dRate = rtspMsg.getScale();
        if (0 == dRate)
        {
            dRate = rtspMsg.getSpeed();
        }

        if (0 == dRate)
        {
            unOperCode = OPER_CODE_CONTINUE;
            SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp vcr message: Continue.",
                    getStreamId()));
            break;
        }
        else if (256 == dRate)
        {
            unOperCode  = OPER_CODE_SPEED;
            unPlayRate  = PLAY_RATE_SINGLE_FRAME;
            SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp vcr message: Single frame.",
                                getStreamId()));
            break;
        }

        SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp vcr rate[%f].",
                                         getStreamId(),
                                         dRate));
        if (dRate > 0)
        {
            unOperCode  = OPER_CODE_SPEED;
            SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp vcr message: Speed.",
                                getStreamId()));
        }
        else
        {
            unOperCode  = OPER_CODE_BACK;
            SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp vcr message: Slow.",
                                getStreamId()));
        }

        if (RET_OK != NumRate2PlayRate(dRate, unPlayRate))
        {
            SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp vcr message[PLAY] fail.",
                             getStreamId()));
            return RET_FAIL;
        }

        break;
    }

    default:
        SVS_LOG((SVS_LM_INFO,"session[%Q] parse rtsp vcr message[PLAY] fail, invalid method[%u].",
                        getStreamId(),
                        rtspMsg.getMethodIndex()));
        return RET_FAIL;   // ��֧�ֵķ���
    }

    return RET_OK;
}

int32_t CMduRtpSession::NumRate2PlayRate(double dNumRate, uint32_t &unPlayRate) const
{
    if (0 == dNumRate)
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] convert num rate to play rate fail, "
                "reate[%f].",
                getStreamId(),
                dNumRate));

        return RET_FAIL;
    }

    double dRate    = fabs(dNumRate);

    RATE_EQUAL_RETURN(RATE_MULT_1, NUM, dRate, PLAY, unPlayRate);
    RATE_EQUAL_RETURN(RATE_MULT_2, NUM, dRate, PLAY, unPlayRate);
    RATE_EQUAL_RETURN(RATE_MULT_4, NUM, dRate, PLAY, unPlayRate);
    RATE_EQUAL_RETURN(RATE_MULT_8, NUM, dRate, PLAY, unPlayRate);
    RATE_EQUAL_RETURN(RATE_MULT_16, NUM, dRate, PLAY, unPlayRate);
    RATE_EQUAL_RETURN(RATE_MULT_32, NUM, dRate, PLAY, unPlayRate);

    RATE_EQUAL_RETURN(RATE_DIV_2, NUM, dRate, PLAY, unPlayRate);
    RATE_EQUAL_RETURN(RATE_DIV_4, NUM, dRate, PLAY, unPlayRate);
    RATE_EQUAL_RETURN(RATE_DIV_8, NUM, dRate, PLAY, unPlayRate);
    RATE_EQUAL_RETURN(RATE_DIV_16, NUM, dRate, PLAY, unPlayRate);
    RATE_EQUAL_RETURN(RATE_DIV_32, NUM, dRate, PLAY, unPlayRate);

    // ���ǰ��û���ҵ���Ӧ�����ʣ���ת��ʧ��
    SVS_LOG((SVS_LM_INFO,"session[%Q] convert num rate to play rate fail, "
                    "reate[%f] invalid.",
                    getStreamId(),
                    dNumRate));
    return RET_FAIL;
}

void CMduRtpSession::setTimeStamp(uint32_t unTimeStamp)
{
    m_timeStamp = unTimeStamp;

    bPlaybackDragFlag = true;
}

bool  CMduRtpSession::getTimeStamp(uint32_t& unTimeStamp)
{
    if(bPlaybackDragFlag)
    {
        unTimeStamp = m_timeStamp;

        bPlaybackDragFlag = false;

        return true;
    }
    return false;
}

int32_t CMduRtpSession::dealSccVcrResp(const SVS_MSG_PLAYBACK_CONTROL_RESP* pMsg)
{
    if(NULL == pMsg)
    {
        return RET_FAIL;
    }

    CMduBusiness* pBusiness = CMduBusinessManager::instance()->findBusiness(getStreamId());

    if(NULL == pBusiness)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] find business fail.", getStreamId()));
        return RET_FAIL;
    }
    uint64_t ullPeerSessionID = pBusiness->getSendStreamID();

    CMduSession *pPeerSession = CMduSessionFactory::instance()->findSession(ullPeerSessionID);

    if(NULL == pPeerSession)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] find peersession[%Q] fail.", getStreamId(),ullPeerSessionID));
        CMduBusinessManager::instance()->releaseBusiness(pBusiness);
        return RET_FAIL;
    }
    CRtspPacket rtspPack;
    rtspPack.setMethodIndex(RtspResponseMethod);
    rtspPack.setCseq(pMsg->Sequence);
    rtspPack.setSessionID(getStreamId());

    if (0 != pMsg->RtpTimestamp)
    {
        char szRtspUrl[KILO] = { 0 };
        (void) snprintf(szRtspUrl, KILO, "rtsp://%s:%d/%s.sdp",
                pPeerSession->getVideoAddr().get_host_addr(),
                pPeerSession->getVideoAddr().get_port_number(),
                        getContentID());
        rtspPack.setRtpInfo(szRtspUrl, pMsg->RtpSequence,pMsg->RtpTimestamp);
    }

    rtspPack.setRtspStatusCode(RtspStatus_200);
    if (0 != pMsg->RespCode)
    {
        rtspPack.setRtspStatusCode(RtspStatus_500);
    }

    std::string strMessage;
    if (0 != rtspPack.generateRtspResp(strMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"Session[%Q] handle vcr response fail, generate rtsp response fail.",ullPeerSessionID));
        CMduSessionFactory::instance()->releaseSession(pPeerSession);
        CMduBusinessManager::instance()->releaseBusiness(pBusiness);
        return RET_FAIL;
    }

    if (RET_OK != pPeerSession->sendVcrMessage(rtspPack))
    {
        SVS_LOG((SVS_LM_WARNING,"Session[%Q]  send vcr response fail.",ullPeerSessionID));
        CMduSessionFactory::instance()->releaseSession(pPeerSession);
        CMduBusinessManager::instance()->releaseBusiness(pBusiness);
        return RET_FAIL;
    }
    CMduSessionFactory::instance()->releaseSession(pPeerSession);
    CMduBusinessManager::instance()->releaseBusiness(pBusiness);

    SVS_LOG((SVS_LM_INFO,"rtp session[%Q] handle vcr response message success, respcode[%x].",ullPeerSessionID, pMsg->RespCode));
    return RET_OK;
}
int32_t CMduRtpSession::sendSessionStopMessage(uint32_t unStopType)
{
    // EOS��Ϣֻ��¼��ط������в��У��ط�����ֻ��TCP����
    if (NULL == m_pTcpHandle)
    {
        return RET_FAIL;
    }

    if ((MDU_PACKET_TYPE_SESSION_EOS != unStopType)
            && (MDU_PACKET_TYPE_SESSION_BOS != unStopType))
    {
        return RET_FAIL;
    }
    std::string strMsg;
    CRtspPacket rtspPacket;
    rtspPacket.setMethodIndex(RtspSetParameterMethod);
    rtspPacket.setCseq(CRtspPacket::getRtspCseqNo());
    rtspPacket.setSessionID(getStreamId());
    char szRtspUrl[KILO] =  { 0 };
    (void) snprintf(szRtspUrl, KILO, "rtsp://%s:%d/%s.sdp",
                    m_TcpPeerAddr.get_host_addr(),
                    m_TcpPeerAddr.get_port_number(),
                    getContentID());
    rtspPacket.setRtspUrl(szRtspUrl);
    if (RET_OK != rtspPacket.generateRtspReq(strMsg))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] generate rtsp bos/eos message fail.",
                             getStreamId()));
        return RET_FAIL;
    }

    if (RET_OK != m_pTcpHandle->send_n(getStreamId(),
                                            strMsg.c_str(),
                                            strMsg.size(),
                                            m_TcpPeerAddr))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send rtsp bos/eos message fail.",
                             getStreamId()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] send session stop message success.",
                     getStreamId()));
    SVS_LOG((SVS_LM_DEBUG,"%s", strMsg.c_str()));
    return RET_OK;
}


ACE_INET_Addr CMduRtpSession::getPeerAddr()const
{
    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        return m_TcpPeerAddr;
    }

    return m_UdpPeerAddr[VIDEO_RTP_HANDLE];
}

ACE_INET_Addr CMduRtpSession::getMediaAddr()const
{
    return getPeerAddr();
}

void CMduRtpSession::getVideoInfo(SDP_MEDIA_INFO &info)
{
    MEDIA_INFO_LIST VideoinfoList;
    SDP_MEDIA_INFO* Videoinfo = NULL;
    m_rtspSdp.getVideoInfo(VideoinfoList);

    if(0 <VideoinfoList.size()) {
        Videoinfo = (SDP_MEDIA_INFO*)&VideoinfoList.front();
        info.ucPayloadType = Videoinfo->ucPayloadType;
        info.usPort        = Videoinfo->usPort;
        info.strFmtp       = Videoinfo->strFmtp;
        info.strRtpmap     = Videoinfo->strRtpmap;
        info.strControl    = Videoinfo->strControl;
    }

    return;
}

void CMduRtpSession::getAudioInfo(SDP_MEDIA_INFO &info)
{
    MEDIA_INFO_LIST AudioinfoList;
    SDP_MEDIA_INFO* Audioinfo = NULL;
    m_rtspSdp.getAudioInfo(AudioinfoList);

    if(0 <AudioinfoList.size()) {
        Audioinfo = (SDP_MEDIA_INFO*)&AudioinfoList.front();
        info.ucPayloadType = Audioinfo->ucPayloadType;
        info.usPort        = Audioinfo->usPort;
        info.strFmtp       = Audioinfo->strFmtp;
        info.strRtpmap     = Audioinfo->strRtpmap;
        info.strControl    = Audioinfo->strControl;
    }
    return;
}


int32_t CMduRtpSession::sendNatResponse(CMduNatMessage &natMsg)
{
    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        return sendTcpNatResponse(natMsg);
    }

    return sendUdpNatResponse(natMsg);
}


int32_t CMduRtpSession::allocMediaPort()
{
    if (RTSP_SESSION == getSessionType())
    {
        if (RET_OK != allocTcpMediaPort())
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] alloc rtsp handle fail.", getStreamId()));
            return RET_FAIL;
        }

        m_pRtspHandle = m_pTcpHandle;
        m_pTcpHandle  = NULL;
        SVS_LOG((SVS_LM_INFO,"session[%Q] succes to alloc rtsp handle[%s:%d].",
                        getStreamId(),
                        m_VideoAddr.get_host_addr(),
                        m_VideoAddr.get_port_number()));
        return RET_OK;
    }

    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        return allocTcpMediaPort();
    }

    return allocUdpMediaPort();
}

int32_t CMduRtpSession::startMediaPort()
{
    if (RTSP_SESSION == getSessionType())
    {
        if (NULL == m_pRtspHandle)
        {
            SVS_LOG((SVS_LM_WARNING,"start std session[%Q] fail, rtsp handle is null.", getStreamId()));
            return RET_FAIL;
        }

        CRtspPacket pack;
        RTSP_URL_INFO info;
        if (0 != pack.parseRtspUrl(m_strRtspUrl, info))
        {
            SVS_LOG((SVS_LM_WARNING,"start std active rtsp session[%Q] fail, parse url[%s] fail.",
                    getStreamId(), m_strRtspUrl.c_str()));
            return RET_FAIL;
        }

        m_rtspSdp.setUrl(m_strRtspUrl);
        m_ConnVideoAddr.set(info.Port, info.Ip);
        m_ConnAudioAddr = m_ConnVideoAddr;
        SVS_LOG((SVS_LM_INFO,"start std active rtsp session[%Q], parse addr[%s:%d].",
                getStreamId(),
                m_ConnVideoAddr.get_host_addr(),
                m_ConnVideoAddr.get_port_number()));

        ACE_INET_Addr localAddr;
        if (RET_OK != m_pRtspHandle->startActiveConnect(getStreamId(), m_ConnVideoAddr, localAddr))
        {
            SVS_LOG((SVS_LM_WARNING,"stream[%Q] connect[%s:%d] fail.",
                    getStreamId(),
                    m_ConnVideoAddr.get_host_addr(),
                    m_ConnVideoAddr.get_port_number()));

            return RET_FAIL;
        }

        SVS_LOG((SVS_LM_INFO,"start std active rtsp session[%Q] success, local addr[%s:%d].",
                        getStreamId(),
                        localAddr.get_host_addr(),
                        localAddr.get_port_number()));

        return startRtspPullSession();
    }

    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        return startTcpMediaPort();
    }

    return startUdpMediaPort();
}

int32_t CMduRtpSession::stopMediaPort()
{
    if (RTSP_SESSION == getSessionType())
    {
        stopRtspPullSession();
    }

    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        if (NULL != m_pTcpHandle)
        {
            m_pTcpHandle->stopHandle(getStreamId());
        }

        SVS_LOG((SVS_LM_INFO,"Rtp tcp session stop port success. stream id[%Q].",
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

bool CMduRtpSession::checkMediaChannelStatus()
{
    bool bRet = true;

    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        bRet = checkTcpMediaChannelStatus();
    }
    else
    {
        bRet = checkUdpMediaChannelStatus();
    }

    if (bRet)
    {
        if (RTSP_SESSION == getSessionType())
        {
            CRtspOptionsMessage options;
            options.setCSeq(m_rtspProtocol.getCseq());
            options.setMsgType(RTSP_MSG_REQ);
            options.setRtspUrl(m_strRtspUrl);
            options.setSession(m_strVideoSession);

            string strReq;
            if (RET_OK == options.encodeMessage(strReq))
            {
                (void)m_rtspProtocol.saveSendReq(options.getCSeq(), options.getMethodType());
                (void) sendMessage(m_pRtspHandle, strReq.c_str(),
                        strReq.length());
                SVS_LOG((SVS_LM_INFO,"session[%Q] send rtsp options message.", getStreamId()));
                SVS_LOG((SVS_LM_DEBUG,"%s", strReq.c_str()));
            }
        }

        sendRtcpMessage();
    }

    return bRet;
}

int32_t CMduRtpSession::setRemoteAddress()
{
    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] set remote address fail, tcp can't accept opertion.",
                        getStreamId()));
        return RET_OK;
    }

    // ���öԶ˵�ַ
    m_UdpPeerAddr[VIDEO_RTP_HANDLE] = m_ConnVideoAddr;
    SVS_LOG((SVS_LM_INFO,"session[%Q] set remote video rtp address[%s:%d]",
            getStreamId(),
            m_ConnVideoAddr.get_host_addr(),
            m_ConnVideoAddr.get_port_number()));
    m_UdpPeerAddr[VIDEO_RTCP_HANDLE].set(m_ConnVideoAddr.get_port_number() + 1,
                                         m_ConnVideoAddr.get_host_addr());
    SVS_LOG((SVS_LM_INFO,"session[%Q] set remote video rtcp address[%s:%d]",
                getStreamId(),
                m_UdpPeerAddr[VIDEO_RTCP_HANDLE].get_host_addr(),
                m_UdpPeerAddr[VIDEO_RTCP_HANDLE].get_port_number()));

    m_UdpPeerAddr[AUDIO_RTP_HANDLE] = m_ConnAudioAddr;
    SVS_LOG((SVS_LM_INFO,"session[%Q] set remote audio rtp address[%s:%d]",
                getStreamId(),
                m_ConnAudioAddr.get_host_addr(),
                m_ConnAudioAddr.get_port_number()));

    m_UdpPeerAddr[AUDIO_RTCP_HANDLE].set(m_ConnAudioAddr.get_port_number() + 1,
                                         m_ConnAudioAddr.get_host_addr());
    SVS_LOG((SVS_LM_INFO,"session[%Q] set remote audio rtcp address[%s:%d]",
                getStreamId(),
                m_UdpPeerAddr[AUDIO_RTCP_HANDLE].get_host_addr(),
                m_UdpPeerAddr[AUDIO_RTCP_HANDLE].get_port_number()));

    return RET_OK;
}

int32_t CMduRtpSession::allocUdpMediaPort()
{
    int32_t nRet = CMduPortManager::instance()->allocRtpUdpPort(getSpecifyIp(),
                                                        m_pUdpHandle[VIDEO_RTP_HANDLE],
                                                        m_pUdpHandle[VIDEO_RTCP_HANDLE]);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"stream[%Q] alloc video media port fail.",
                getStreamId()));
        return RET_FAIL;
    }
    m_VideoAddr.set(m_pUdpHandle[VIDEO_RTP_HANDLE]->getLocalAddr());

    nRet = CMduPortManager::instance()->allocRtpUdpPort(getSpecifyIp(),
                                m_pUdpHandle[AUDIO_RTP_HANDLE],
                                m_pUdpHandle[AUDIO_RTCP_HANDLE]);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"stream[%Q] alloc audio media port fail.",
                getStreamId()));
        return RET_FAIL;
    }
    m_AudioAddr.set(m_pUdpHandle[AUDIO_RTP_HANDLE]->getLocalAddr());

    SVS_LOG((SVS_LM_INFO,
        "Rtp udp session alloc media port success. stream id[%Q] "
        "video rtp handle[%p] video rtcp handle[%p] "
        "audio rtp handle[%p] audio rtcp handle[%p].",
        getStreamId(),
        m_pUdpHandle[VIDEO_RTP_HANDLE],
        m_pUdpHandle[VIDEO_RTCP_HANDLE],
        m_pUdpHandle[AUDIO_RTP_HANDLE],
        m_pUdpHandle[AUDIO_RTCP_HANDLE]));
    return nRet;
}

int32_t CMduRtpSession::allocTcpMediaPort()
{
    CNetworkHandle *pHandle = NULL;
    int32_t nRet = CMduPortManager::instance()->allocRtpTcpPort(getSpecifyIp(), pHandle);
    if ((RET_OK != nRet) || (NULL == pHandle))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] alloc tcp port fail, mdu port manager return fail.",
                    getStreamId()));
        return RET_FAIL;
    }

    m_pTcpHandle = dynamic_cast<CTcpHandle*>(pHandle);
    if (NULL == m_pTcpHandle)
    {
        return RET_FAIL;
    }
    m_VideoAddr.set(m_pTcpHandle->getLocalAddr());
    m_AudioAddr.set(m_pTcpHandle->getLocalAddr());

    return RET_OK;
}

int32_t CMduRtpSession::startUdpMediaPort()
{
    if ((uint16_t)-1 == m_unVedioPT)
    {
        (void) CMduPortManager::instance()->releaseRtpUdpPort(getSpecifyIp(),
                                        m_pUdpHandle[VIDEO_RTP_HANDLE],
                                        m_pUdpHandle[VIDEO_RTCP_HANDLE]);
        m_pUdpHandle[VIDEO_RTP_HANDLE]  = NULL;
        m_pUdpHandle[VIDEO_RTCP_HANDLE] = NULL;
    }
    else
    {
        if ((NULL == m_pUdpHandle[VIDEO_RTP_HANDLE])
                || (NULL == m_pUdpHandle[VIDEO_RTCP_HANDLE]))
        {
            SVS_LOG((SVS_LM_ERROR,"Start session[%Q] video port fail, handle  is null.",
                    getStreamId()));
            return RET_FAIL;
        }
        int32_t iRet = m_pUdpHandle[VIDEO_RTP_HANDLE]->startHandle(getStreamId(),
                                                               m_UdpPeerAddr[VIDEO_RTP_HANDLE]);
        if (RET_OK != iRet)
        {
            SVS_LOG((SVS_LM_ERROR,"Start session[%Q] video rtp port fail.", getStreamId()));
            return RET_FAIL;
        }

        iRet = m_pUdpHandle[VIDEO_RTCP_HANDLE]->startHandle(getStreamId(),
                                                            m_UdpPeerAddr[VIDEO_RTCP_HANDLE]);
        if (RET_OK != iRet)
        {
            SVS_LOG((SVS_LM_ERROR,"Start session[%Q] video rtcp port fail.", getStreamId()));
            return RET_FAIL;
        }
    }

    if ((uint16_t)-1 == m_unAudioPT)
    {
        (void) CMduPortManager::instance()->releaseRtpUdpPort(getSpecifyIp(),
                                    m_pUdpHandle[AUDIO_RTP_HANDLE],
                                    m_pUdpHandle[AUDIO_RTCP_HANDLE]);
        m_pUdpHandle[AUDIO_RTP_HANDLE]  = NULL;
        m_pUdpHandle[AUDIO_RTCP_HANDLE] = NULL;
    }
    else
    {
        if ((NULL == m_pUdpHandle[AUDIO_RTP_HANDLE]) || (NULL == m_pUdpHandle[AUDIO_RTCP_HANDLE]))
        {
            SVS_LOG((SVS_LM_ERROR,"Start session[%Q] audio port fail, handle  is null.",
                    getStreamId()));
            return RET_FAIL;
        }
        int32_t iRet = m_pUdpHandle[AUDIO_RTP_HANDLE]->startHandle(getStreamId(),
                                                               m_UdpPeerAddr[AUDIO_RTP_HANDLE]);
        if (RET_OK != iRet)
        {
            SVS_LOG((SVS_LM_ERROR,"Start session[%Q] audio rtp port fail.", getStreamId()));
            return RET_FAIL;
        }

        iRet = m_pUdpHandle[AUDIO_RTCP_HANDLE]->startHandle(getStreamId(),
                                                            m_UdpPeerAddr[AUDIO_RTCP_HANDLE]);
        if (RET_OK != iRet)
        {
            SVS_LOG((SVS_LM_ERROR,"Start session[%Q] audio rtcp port fail.", getStreamId()));
            return RET_FAIL;
        }
    }

    SVS_LOG((SVS_LM_INFO,"Start media port success. stream id[%Q].",
        getStreamId()));
    return RET_OK;
}

int32_t CMduRtpSession::startTcpMediaPort()
{
    if (NULL == m_pTcpHandle)
    {
        return RET_FAIL;
    }

    if (CONN_SETUP_TYPE_ACTIVE == m_unConnSetupType)
    {
        m_TcpPeerAddr  = m_ConnVideoAddr;
        m_bHasPeerAddr = true;
        ACE_INET_Addr localAddr;

        if(RET_OK != m_pTcpHandle->startActiveConnect(getStreamId(), m_ConnVideoAddr, localAddr))
        {
            SVS_LOG((SVS_LM_WARNING,"stream[%Q] connect[%s:%d] fail.",
                    getStreamId(),
                    m_ConnVideoAddr.get_host_addr(),
                    m_ConnVideoAddr.get_port_number()));

            return RET_FAIL;
        }

        m_VideoAddr = localAddr;
        m_AudioAddr = localAddr;

        sendNatRequest();

        return RET_OK;
    }

    return m_pTcpHandle->startHandle(getStreamId(), m_TcpPeerAddr);
}

bool CMduRtpSession::checkTcpMediaChannelStatus()
{
    if (NULL == m_pTcpHandle)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] check media channel fail, handle[%p].",
                getStreamId(), m_pTcpHandle));
        return false;
    }

    if (!m_pTcpHandle->checkHandleStatus(getStreamId(), MDU_MEDIA_CHANNEL_INVAILD_INTERVAL))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] check media channel fail, tcp handle status invalid.",
                getStreamId()));
        return false;
    }

    // �༶����ʱ������ý��ͨ��״̬��鶨ʱ�����ж���NAT��Խ
    if ((PEER_TYPE_STREAM == getPeerType())
            && (CONN_SETUP_TYPE_ACTIVE == m_unConnSetupType))
    {
        uint32_t ulCurTime = SVS_GetSecondTime();
        if (ulCurTime - m_ulLastNatTime >= MDU_MEDIA_CHANNEL_HEARTBEAT_INTERVAL)
        {
            sendNatRequest();
        }
    }

    SVS_LOG((SVS_LM_DEBUG,"session[%Q] check media channel success.",
            getStreamId()));
    return true;
}


bool CMduRtpSession::checkUdpMediaChannelStatus() const
{
    // UDP������ֻҪ��һ��ý��ͨ���������������Ϊ�Ự״̬��
    if (NULL != m_pUdpHandle[VIDEO_RTP_HANDLE])
    {
        if (m_pUdpHandle[VIDEO_RTP_HANDLE]->checkHandleStatus(getStreamId(),
                                                    MDU_MEDIA_CHANNEL_INVAILD_INTERVAL))
        {
            SVS_LOG((SVS_LM_DEBUG,"session[%Q] check video rtp channel success.",
                    getStreamId()));
            return true;
        }
    }

    if (NULL != m_pUdpHandle[VIDEO_RTCP_HANDLE])
    {
        if (m_pUdpHandle[VIDEO_RTCP_HANDLE]->checkHandleStatus(getStreamId(),
                                                              MDU_MEDIA_CHANNEL_INVAILD_INTERVAL))
        {
            SVS_LOG((SVS_LM_DEBUG,"session[%Q] check video rtcp channel success.",
                    getStreamId()));
            return true;
        }
    }

    if (NULL != m_pUdpHandle[AUDIO_RTP_HANDLE])
    {
        if (m_pUdpHandle[AUDIO_RTP_HANDLE]->checkHandleStatus(getStreamId(),
                                                              MDU_MEDIA_CHANNEL_INVAILD_INTERVAL))
        {
            SVS_LOG((SVS_LM_DEBUG,"session[%Q] check audio rtp channel success.",
                                getStreamId()));
            return true;
        }
    }

    if (NULL != m_pUdpHandle[AUDIO_RTCP_HANDLE])
    {
        if (m_pUdpHandle[AUDIO_RTCP_HANDLE]->checkHandleStatus(getStreamId(),
                                                              MDU_MEDIA_CHANNEL_INVAILD_INTERVAL))
        {
            SVS_LOG((SVS_LM_DEBUG,"session[%Q] check audio rtcp channel success.",
                                getStreamId()));
            return true;
        }
    }

    SVS_LOG((SVS_LM_DEBUG,"session[%Q] check media channel fail, all media channel abnormal.",
                getStreamId()));
    return false;
}

int32_t CMduRtpSession::sendUdpNatResponse(const CMduNatMessage &natMsg)
{
    int32_t nHandleIndex = 0;
    for (nHandleIndex = 0; nHandleIndex < HANDLE_TYPE_MAX; nHandleIndex++)
    {
        if (natMsg.m_pRecvHandle == m_pUdpHandle[nHandleIndex])
        {
            break;
        }
    }

    if ((HANDLE_TYPE_MAX <= nHandleIndex) || (NULL == m_pUdpHandle[nHandleIndex]))
    {
        // ���Ӿ��Ƿ�
        SVS_LOG((SVS_LM_WARNING,"session[%Q] handle nat message fail, recv handle invalid.",
                getStreamId()));
        return RET_FAIL;
    }

    // ��Ҫ��¼�Զ˵�ַ
    m_UdpPeerAddr[nHandleIndex].set(natMsg.m_usNatPort, natMsg.m_unNatIp);
    SVS_LOG((SVS_LM_INFO,"set session addr[%s:%d] , handle index[%d] stream id[%Q].",
            m_UdpPeerAddr[nHandleIndex].get_host_addr(),
            m_UdpPeerAddr[nHandleIndex].get_port_number(),
            nHandleIndex,
            getStreamId()));

    // ����NAT��Խ��Ӧ
    CRtspPacket rtspPack;
    rtspPack.setCseq(natMsg.m_unMsgSeq);
    rtspPack.setSessionID(getStreamId());

    RTSP_NAT_INFO natInfo;
    if ((VIDEO_RTP_HANDLE == nHandleIndex) || (AUDIO_RTP_HANDLE == nHandleIndex))
    {
        natInfo.NatType = RTSP_NAT_TYPE_RTP;
    }
    else
    {
        natInfo.NatType = RTSP_NAT_TYPE_RTCP;
    }
    natInfo.LocalIp     = natMsg.m_unLocalIp;
    natInfo.LocalPort   = natMsg.m_usLocalPort;
    natInfo.SrcIp       = m_UdpPeerAddr[nHandleIndex].get_ip_address();
    natInfo.SrcPort     = m_UdpPeerAddr[nHandleIndex].get_port_number();
    rtspPack.setNatInfo(natInfo);
    rtspPack.setRtspStatusCode(RtspStatus_200);

    std::string strResp;
    if (0 != rtspPack.generateRtspResp(strResp))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] create rtsp nat response fail.", getStreamId()));
        return RET_FAIL;
    }

    int32_t nRet = m_pUdpHandle[nHandleIndex]->sendMessage(getStreamId(),
                                                        strResp.c_str(),
                                                        strResp.size(),
                                                        m_UdpPeerAddr[nHandleIndex]);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send nat response to peer[%s:%d] fail.",
                getStreamId(),
                m_UdpPeerAddr[nHandleIndex].get_host_addr(),
                m_UdpPeerAddr[nHandleIndex].get_port_number()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] send nat response success.", getStreamId()));
    SVS_LOG((SVS_LM_DEBUG,"%s", strResp.c_str()));
    return RET_OK;
}

int32_t CMduRtpSession::sendTcpNatResponse(const CMduNatMessage &natMsg)
{
    if (NULL == m_pTcpHandle)
    {
        return RET_FAIL;
    }

    if (!m_bHasPeerAddr)
    {
        // NAT��Խ��ý��ͨ���ĵ�һ����Ϣ����ʱ��Ҫ����Զ˵�ַ��ʹ��MDU�ܹ�������Peer�������
        m_TcpPeerAddr.set(natMsg.m_usNatPort, natMsg.m_unNatIp);
        m_bHasPeerAddr = true;
    }

    // ����NAT��Խ��Ӧ
    CRtspPacket rtspPack;
    rtspPack.setCseq(natMsg.m_unMsgSeq);
    rtspPack.setSessionID(getStreamId());

    RTSP_NAT_INFO natInfo;
    natInfo.NatType   = RTSP_NAT_TYPE_RTP; // TCP��Ŀǰû��RTCP
    natInfo.LocalIp   = natMsg.m_unLocalIp;
    natInfo.LocalPort = natMsg.m_usLocalPort;
    natInfo.SrcIp     = m_TcpPeerAddr.get_ip_address();
    natInfo.SrcPort   = m_TcpPeerAddr.get_port_number();
    rtspPack.setNatInfo(natInfo);
    rtspPack.setRtspStatusCode(RtspStatus_200);

    std::string strResp;
    if (0 != rtspPack.generateRtspResp(strResp))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] create rtsp nat response fail.", getStreamId()));
        return RET_FAIL;
    }

    int32_t nRet = m_pTcpHandle->sendMessage(getStreamId(),
                                          strResp.c_str(),
                                          strResp.size(),
                                          m_TcpPeerAddr);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send nat response to peer[%s:%d] fail.",
                getStreamId(),
                m_TcpPeerAddr.get_host_addr(),
                m_TcpPeerAddr.get_port_number()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] send nat response success.", getStreamId()));
    SVS_LOG((SVS_LM_DEBUG,"%s", strResp.c_str()));

    return RET_OK;
}


void CMduRtpSession::setChannelNum(char cPt, char& channelNum)const
{
    if ( (char)m_unVedioPT == cPt )
    {
        channelNum = 0;
    }
    else
    {
        channelNum = 2;
    }
}

/*lint -e818*/
int32_t CMduRtpSession::sendUdpMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount)
{
    if (NULL == pMbArray)
    {
        return RET_FAIL;
    }

    for (uint32_t i = 0; i < MsgCount; i++)
    {
        if (NULL == pMbArray[i])
        {
            return RET_FAIL;
        }

        CRtpPacket rtpPack;
        if (RET_OK != rtpPack.ParsePacket(pMbArray[i]->rd_ptr(), pMbArray[i]->length()))
        {
            SVS_LOG((SVS_LM_ERROR,"session[%Q] send media data fail, parse rtp packet fail.",
                            getStreamId()));
            return -1;
        }

        resetRtpPt(rtpPack);
        char cPt = rtpPack.GetPayloadType();

        if ((char)m_unVedioPT == cPt )
        {
            if (!m_pUdpHandle[VIDEO_RTP_HANDLE])
            {
                return -1;
            }

            return m_pUdpHandle[VIDEO_RTP_HANDLE]->sendMessage(getStreamId(),
                                                                pMbArray[i]->rd_ptr(),
                                                                pMbArray[i]->length(),
                                                                m_UdpPeerAddr[VIDEO_RTP_HANDLE]);
        }
        else
        {
            if ((uint8_t)-1 == getAudioPayload())
            {
                return 0;
            }

            if (!m_pUdpHandle[AUDIO_RTP_HANDLE])
            {
                return -1;
            }

            return m_pUdpHandle[AUDIO_RTP_HANDLE]->sendMessage(getStreamId(),
                                                                pMbArray[i]->rd_ptr(),
                                                                pMbArray[i]->length(),
                                                                m_UdpPeerAddr[AUDIO_RTP_HANDLE]);
        }
    }

    return -1;
}


int32_t CMduRtpSession::sendTcpMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount)
{
    if ((NULL == m_pTcpHandle) || (NULL == pMbArray))
    {
        return RET_FAIL;
    }

    for (uint32_t i = 0; i < MsgCount; i++)
    {
        if (NULL == pMbArray[i])
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] send meida fail, data block is null, ",
                             getStreamId()));
            return RET_FAIL;
        }

        char interleaveData[RTP_INTERLEAVE_LENGTH] = {0};
        CRtpPacket rtpPack;
        if (RET_OK != rtpPack.ParsePacket(pMbArray[i]->rd_ptr(), pMbArray[i]->length()))
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] send meida fail, parse data as rtp fail, "
                        "msg[%p] len[%u].",
                       getStreamId(), pMbArray[i], pMbArray[i]->length()));
            return RET_FAIL;
        }
        else
        {
            resetRtpPt(rtpPack);

            char cPt = rtpPack.GetPayloadType();
            interleaveData[0] = RTP_INTERLEAVE_FLAG;

            setChannelNum(cPt, interleaveData[1]);
        }

        *(uint16_t*)&interleaveData[2] = htons((uint16_t)pMbArray[i]->length());

        struct iovec dataVec[2];
        dataVec[0].iov_len  = RTP_INTERLEAVE_LENGTH;
        dataVec[0].iov_base = interleaveData;
        dataVec[1].iov_len  = pMbArray[i]->length();
        dataVec[1].iov_base = pMbArray[i]->rd_ptr();

        if (RET_OK != m_pTcpHandle->sendv(getStreamId(),
                                          dataVec,
                                          2,
                                          pMbArray[i]->length() + RTP_INTERLEAVE_LENGTH))
        {

            return RET_ERR_SEND_FAIL;
        }
    }

    return RET_OK;
}
/*lint +e818*/

int32_t CMduRtpSession::sendMessage(const void* pPeerHandle,
                                const char* pData,
                                uint32_t unDataSize)
{
    if (RTSP_SESSION == getSessionType())
    {
        if (NULL == m_pRtspHandle)
        {
            SVS_LOG((SVS_LM_WARNING,"std session[%Q] send message fail, rtsp handle is null.",
                            getStreamId()));
            return RET_FAIL;
        }

        return m_pRtspHandle->sendMessage(getStreamId(), pData, unDataSize, m_TcpPeerAddr);
    }

    if (NULL == pPeerHandle)
    {
        return RET_FAIL;
    }

    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        if (NULL == m_pTcpHandle)
        {
            return RET_FAIL;
        }

        return m_pTcpHandle->sendMessage(getStreamId(), pData, unDataSize, m_TcpPeerAddr);
    }

    int32_t nHandleIndex = 0;
    for (nHandleIndex = 0; nHandleIndex < HANDLE_TYPE_MAX; nHandleIndex++)
    {
        if (pPeerHandle == m_pUdpHandle[nHandleIndex])
        {
            break;
        }
    }

    if ((HANDLE_TYPE_MAX <= nHandleIndex) || (NULL == m_pUdpHandle[nHandleIndex]))
    {
        // ���Ӿ��Ƿ�
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send message fail, recv handle invalid.",
                getStreamId()));
        return RET_FAIL;
    }

    if(RET_OK != m_pUdpHandle[nHandleIndex]->sendMessage(getStreamId(),
                                                    pData,
                                                    unDataSize,
                                                    m_UdpPeerAddr[nHandleIndex]))
    {
        return RET_ERR_SEND_FAIL;
    }

    return RET_OK;
}

/// ����RTSP��Ϣ
int32_t CMduRtpSession::handleRtspMessage(const MDU_INNER_MSG &innerMsg,
                                   uint32_t unMsgSize,
                                   CMduSession &peerSession)
{
    char *pRtspMsg = (char*) &innerMsg + innerMsg.unBodyOffset;
    uint32_t unRtspSize = unMsgSize - innerMsg.unBodyOffset;

    std::string strRtsp;
    strRtsp.append(pRtspMsg, unRtspSize);
    SVS_LOG((SVS_LM_DEBUG,"session[%Q] handle rtsp message:\n",getStreamId()));
    SVS_LOG((SVS_LM_DEBUG,"%s", strRtsp.c_str()));

    CRtspPacket rtspPack;
    if (0 != rtspPack.parse(pRtspMsg, unRtspSize))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] parse rtsp message fail.", getStreamId()));
        return RET_FAIL;
    }

    // ���ỰID�Ƿ���ȷ
    if ((PEER_TYPE_STREAM == getPeerType())
            && (CONN_SETUP_TYPE_ACTIVE == m_unConnSetupType))
    {
        // ��������ʱ���¼�MDU���ϼ�MDU֮����Ϣ��IDΪ�ϼ�MDU�ĻỰID
        if (m_ullConnStreamID != rtspPack.getSessionID())
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] handle rtsp message fail, invalid session id[%Q].",
                             getStreamId(),
                             rtspPack.getSessionID()));
            return RET_FAIL;
        }
    }
    else
    {
        // ������£���Ϣ�лỰIDΪMDU����ID
        if (getStreamId() != rtspPack.getSessionID())
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] handle rtsp message fail, invalid session id[%Q].",
                    getStreamId(),
                    rtspPack.getSessionID()));
            return RET_FAIL;
        }
    }

    // ��Ӧ��Ϣ����
    if (RtspResponseMethod == rtspPack.getMethodIndex())
    {
        return handleRtspRespMessage(rtspPack, innerMsg.pRecvHandle, peerSession);
    }


    if (RtspRecordMethod == rtspPack.getMethodIndex())
    {
        // ��¼PU�ط����ص�Range
        (void)rtspPack.getRangeTime(m_unRangeType, m_unRangeStart, m_unRangeStop);
    }

    // �������Ϣ��ֱ�ӻ���Ӧ
    if (RtspOptionsMethod == rtspPack.getMethodIndex())
    {
        return handleRtspHeartbeatMsg(innerMsg, rtspPack);
    }

    // ������NAT��Ϣ������Ϊ��NAT��Խ������Ϣ
    if (rtspPack.hasNetInfo())
    {
        RTSP_NAT_INFO natInfo;
        rtspPack.getNatInfo(natInfo);

        // ����NAT��Խ��Ϣ
        CMduNatMessage natMsg;
        natMsg.init(innerMsg.pRecvHandle,
                    rtspPack.getCseq(),
                    NULL,
                    natInfo.LocalIp,
                    natInfo.LocalPort,
                    innerMsg.unRemoteIp,
                    innerMsg.usRemotePort,
                    (void*) &peerSession);

        return handleNatMessage(natMsg);
    }

    if (RtspSetParameterMethod == rtspPack.getMethodIndex())
    {
        return RET_OK;
    }

    return handleRtspVcrMessage(rtspPack, innerMsg.pRecvHandle, peerSession);
}

void CMduRtpSession::sendNatRequest()
{
    if (NULL == m_pTcpHandle)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send nat request fail, tcp handle invalid.",
                getStreamId()));
        return;
    }

    CRtspPacket rtspPack;
    rtspPack.setMethodIndex(RtspPlayMethod);
    rtspPack.setCseq(CRtspPacket::getRtspCseqNo());
    rtspPack.setSessionID(m_ullConnStreamID);       // ��������NAT��Խʱ������ǶԶ˵���ID

    char szRtspUrl[KILO] = { 0 };
    (void) snprintf(szRtspUrl, KILO, "rtsp://%s:%d/%s.sdp", m_TcpPeerAddr.get_host_addr(),
            m_TcpPeerAddr.get_port_number(), getContentID());
    rtspPack.setRtspUrl(szRtspUrl);

    RTSP_NAT_INFO info;
    info.NatType    = RTSP_NAT_TYPE_RTP;
    info.LocalIp    = m_VideoAddr.get_ip_address();
    info.LocalPort  = m_VideoAddr.get_port_number();
    info.SrcIp      = 0;
    info.SrcPort    = 0;
    rtspPack.setNatInfo(info);

    std::string strRequest;
    if (0 != rtspPack.generateRtspReq(strRequest))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] create rtsp nat request fail.", getStreamId()));
        return;
    }

    int32_t nRet = m_pTcpHandle->sendMessage(getStreamId(), strRequest.c_str(),
                                         strRequest.size(), m_TcpPeerAddr);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send nat request to peer[%s:%d] fail.",
                getStreamId(),
                m_TcpPeerAddr.get_host_addr(),
                m_TcpPeerAddr.get_port_number()));
        return;
    }

    m_ulLastNatTime = SVS_GetSecondTime();
    SVS_LOG((SVS_LM_INFO,"session[%Q] send nat request success.", getStreamId()));
    SVS_LOG((SVS_LM_DEBUG,"%s", strRequest.c_str()));
    return;
}

int32_t CMduRtpSession::handleRtspVcrMessage(CRtspPacket &rtspMsg,
                                        const void* pRecvHandle,
                                        CMduSession &peerSession)
{
    SVS_LOG((SVS_LM_INFO,"session[%Q] handle rtsp vcr message.", getStreamId()));

    if (PLAY_TYPE_FRONT_RECORD != m_enPlayType)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] handle rtsp vcr message fail, ServiceType[%d] invalid.",
                getStreamId(),
                m_enPlayType));

        return RET_FAIL;
    }



    // ֪ͨ�Զ˻Ự����VCR��Ϣ
    int32_t nRet = peerSession.sendVcrMessage(rtspMsg);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] ask peer session[%Q] send vcr message fail.",
                        getStreamId(), peerSession.getStreamId()));

        // ����VCR��Ϣʧ�ܣ�����ʧ����Ӧ
        rtspMsg.setRtspStatusCode(RtspStatus_500);
        std::string strResp;
        if (0 != rtspMsg.generateRtspResp(strResp))
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] handle rtsp vcr message fail, generate response fail.",
                    getStreamId()));
            return RET_FAIL;
        }

        nRet = sendMessage(pRecvHandle, strResp.c_str(), strResp.size());
        if (RET_OK != nRet)
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] handle rtsp vcr message fail, send response fail.",
                            getStreamId()));
            return RET_FAIL;
        }

        SVS_LOG((SVS_LM_INFO,"session[%Q] handle rtsp vcr message fail, send response.",
                         getStreamId()));
        SVS_LOG((SVS_LM_DEBUG,"%s", strResp.c_str()));
        return RET_FAIL;
    }
    SVS_LOG((SVS_LM_INFO,"session[%Q] handle rtsp vcr message success.",
                     getStreamId()));

    return RET_OK;
}

// ����RTSP��Ӧ��Ϣ
int32_t CMduRtpSession::handleRtspRespMessage(CRtspPacket &rtspMsg,
                                         const void* /*pRecvHandle*/,
                                         CMduSession &peerSession)
{
    if (rtspMsg.hasNetInfo())
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] recv rtsp nat responese.", getStreamId()));

        if (PEER_TYPE_STREAM != getPeerType())
        {
            return RET_OK;
        }

        if (MDU_SESSION_STATUS_WAIT_CHANNEL_REDAY != getStatus())
        {
            return RET_OK;
        }

        setStatus(MDU_SESSION_STATUS_DISPATCHING);

        if (MDU_SESSION_STATUS_RECVED_NAT_REQ > peerSession.getStatus())
        {
            return RET_OK;
        }

        (void) peerSession.handleRecvedNatRequest();

        return RET_OK;
    }

    if (PEER_TYPE_CU == getPeerType())
    {
        return RET_OK;
    }

    if (0 != rtspMsg.getCseq())
    {
        if (RET_OK != peerSession.sendVcrMessage(rtspMsg))
        {
            SVS_LOG((SVS_LM_WARNING,"Session[%Q] handle rtps vcr response to session[%Q] fail.",
                    getStreamId(), peerSession.getStreamId()));
            return RET_FAIL;
        }
    }

    SVS_LOG((SVS_LM_INFO,"Session[%Q] handle rtps vcr response to session[%Q] success.",
                    getStreamId(), peerSession.getStreamId()));
    return RET_OK;
}

int32_t CMduRtpSession::handleRtspHeartbeatMsg(const MDU_INNER_MSG &innerMsg,
                                           CRtspPacket &rtspPack)
{
    std::string strResp;
    rtspPack.setRtspStatusCode(RtspStatus_200);
    if (RET_OK != rtspPack.generateRtspResp(strResp))
    {
        return RET_FAIL;
    }

    int32_t nRet = sendMessage(innerMsg.pRecvHandle, strResp.c_str(), strResp.size());
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send rtsp OPTIONE response fail.",
                        getStreamId()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] send rtsp option response success.", getStreamId()));
    SVS_LOG((SVS_LM_DEBUG,"Rtsp message:\n%s", strResp.c_str()));
    return RET_OK;
}

int32_t CMduRtpSession::startRtspPullSession()
{
    CRtspDescribeMessage request;
    request.setMsgType(RTSP_MSG_REQ);
    request.setRtspUrl(m_strRtspUrl);
    request.setCSeq(m_rtspProtocol.getCseq());

    std::string strMsg;
    if (RET_OK != request.encodeMessage(strMsg))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] encode describe message fail.", getStreamId()));
        return RET_FAIL;
    }

    m_rtspProtocol.saveSendReq(request.getCSeq(), request.getMethodType());

    if (RET_OK != sendMessage(m_pTcpHandle, strMsg.c_str(), strMsg.length()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] send describe message fail.", getStreamId()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"rtp session[%Q] send describe message success.", getStreamId()));

    // �ȴ�Ự������Զ�ȡSDP��Ϣ�����ȴ�1s
    ACE_Time_Value timeout = ACE_OS::gettimeofday() + ACE_Time_Value(1);
    if (-1 == m_rtspInitSem.acquire(timeout))
    {
        SVS_LOG((SVS_LM_WARNING,"start rtsp pull session[%Q] fail, wait timeout.", getStreamId()));
        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CMduRtpSession::allocStdMediaPort()
{
    int32_t nRet = RET_OK;
    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        // TCP�Ự�������ٷ��������ˣ�ֱ�Ӱ�rtspHandle��ΪtcpHandle����
        m_pTcpHandle = m_pRtspHandle;
    }
    else
    {
        nRet = allocUdpMediaPort();
    }

    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"std session[%Q] alloc media port fail.", getStreamId()));
    }
    else
    {
        SVS_LOG((SVS_LM_INFO,"std session[%Q] alloc media port success.", getStreamId()));
    }

    return nRet;
}

int32_t CMduRtpSession::startStdMediaPort()
{
    int32_t nRet = RET_OK;
    if (TRANS_PROTOCAL_UDP == getTransProtocol())
    {
        // TCP�Ự��������������
        nRet = startUdpMediaPort();
    }
    else
    {
        if (NULL != m_pTcpHandle)
        {
            m_pTcpHandle->setInterleaveNum(getStreamId(),
                                           getVideoInterleaveNum(),
                                           getAudioInterleaveNum());
        }
    }
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"std session[%Q] start media port fail.", getStreamId()));
    }
    else
    {
        SVS_LOG((SVS_LM_INFO,"std session[%Q] start media port success.", getStreamId()));
    }

    return nRet;
}

void CMduRtpSession::stopRtspPullSession()
{
    (void)m_rtspInitSem.release();
    CRtspTeardownMessage request;
    request.setMsgType(RTSP_MSG_REQ);
    request.setCSeq(m_rtspProtocol.getCseq());
    request.setRtspUrl(m_rtspSdp.getUrl());
    request.setSession(m_strVideoSession);

    std::string strMsg;
    (void)request.encodeMessage(strMsg);
    (void)sendMessage(m_pRtspHandle, strMsg.c_str(), strMsg.length());

    if (m_strVideoSession != m_strAudioSession)
    {
        request.setCSeq(m_rtspProtocol.getCseq());
        request.setRtspUrl(m_rtspSdp.getUrl());
        request.setSession(m_strAudioSession);

        (void) request.encodeMessage(strMsg);
        (void) sendMessage(m_pRtspHandle, strMsg.c_str(), strMsg.length());
    }

    return;
}

int32_t CMduRtpSession::sendStdVcrMessage(CRtspPacket &rtspPack)
{
    std::string strRtsp;
    if (RtspResponseMethod == rtspPack.getMethodIndex())
    {
        (void)rtspPack.generateRtspResp(strRtsp);
        SVS_LOG((SVS_LM_WARNING,"std session[%Q] not acceped vcr response:\n%s.",
                        getStreamId(), strRtsp.c_str()));
        return RET_FAIL;
    }

    (void)rtspPack.generateRtspReq(strRtsp);

    CRtspMessage *pMessage = NULL;
    if (RET_OK != m_rtspProtocol.DecodeRtspMessage(strRtsp.c_str(), strRtsp.length(), pMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"std session[%Q] decode vcr message fail, rtsp:\n%s.",
                        getStreamId(), strRtsp.c_str()));
        return RET_FAIL;
    }

    // �������ûỰ��URL
    pMessage->setSession(m_strVideoSession);
    pMessage->setRtspUrl(m_rtspSdp.getUrl());
    if (RTSP_METHOD_PLAY == pMessage->getMethodType())
    {
        CRtspPlayMessage *pPlayMsg = dynamic_cast<CRtspPlayMessage*>(pMessage);
        if (pPlayMsg)
        {
            if (0 == pPlayMsg->getScale())
            {
                // ����Scale�ֶ�
                pPlayMsg->setScale(1.0);
            }
        }
    }

    strRtsp = "";
    if (RET_OK != pMessage->encodeMessage(strRtsp))
    {
        delete pMessage;
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] encode play request fail,",
                getStreamId()));
        return RET_FAIL;
    }

    // ������ź���Ϣ����
    m_rtspProtocol.saveSendReq(pMessage->getCSeq(), pMessage->getMethodType());

    if (RET_OK != sendMessage(m_pRtspHandle, strRtsp.c_str(), strRtsp.length()))
    {
        delete pMessage;
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] send vcr message fail.", getStreamId()));
        return RET_FAIL;
    }

    delete pMessage;
    SVS_LOG((SVS_LM_INFO,"rtp session[%Q] send vcr message success.",  getStreamId()));
    return RET_OK;
}

int32_t CMduRtpSession::handleStdRtspMessage(const MDU_INNER_MSG &innerMsg,
                                         uint32_t unMsgSize,
                                         CMduSession &peerSession)
{
    char *pMsgData = (char*)&innerMsg + innerMsg.unBodyOffset;
    uint32_t unRtspSize = unMsgSize - innerMsg.unBodyOffset;

    std::string strMsg;
    strMsg.append(pMsgData, unRtspSize);

    CRtspMessage *pRtspMessage = NULL;
    if (RET_OK != m_rtspProtocol.DecodeRtspMessage(pMsgData, unRtspSize, pRtspMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] decode std rtsp message fail.", getStreamId()));
        return RET_FAIL;
    }

    int32_t nRet = RET_OK;
    switch (pRtspMessage->getMethodType())
    {
    case RTSP_METHOD_OPTIONS:
        SVS_LOG((SVS_LM_INFO,"session[%Q] recved options response.", getStreamId()));
        break;
    case RTSP_METHOD_DESCRIBE:
        nRet = handleStdRtspDescribeResp(*pRtspMessage);
        break;
    case RTSP_METHOD_SETUP:
        nRet = handleStdRtspSetupResp(*pRtspMessage, peerSession);
        break;
    case RTSP_METHOD_PLAY:
        nRet = handleStdRtspPlayResp(*pRtspMessage, peerSession);
        break;
    case RTSP_METHOD_PAUSE:
        nRet = handleStdRtspPauseResp(*pRtspMessage, peerSession);
        break;
    case RTSP_METHOD_TEARDOWN:
        nRet = handleStdRtspTeardownResp(*pRtspMessage);
        break;
    case RTSP_METHOD_ANNOUNCE:
        nRet = handleStdRtspAnnounceReq(*pRtspMessage, peerSession);
        break;
    default:
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] handle not accepted rtsp message[%d].",
                        getStreamId(), pRtspMessage->getMethodType()));
        return RET_FAIL;
    }

    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] failed to handle std rtsp message[%d].",
                        getStreamId(), pRtspMessage->getMethodType()));
    }
    else
    {
        SVS_LOG((SVS_LM_INFO,"rtp session[%Q] success to handle std rtsp message[%d].",
                        getStreamId(), pRtspMessage->getMethodType()));
    }

    delete pRtspMessage;

    return nRet;
}

int32_t CMduRtpSession::handleStdRtspDescribeResp(CRtspMessage &rtspMessage)
{
    CRtspDescribeMessage *pMessage = dynamic_cast<CRtspDescribeMessage *>(&rtspMessage);
    if (!pMessage)
    {
        return RET_FAIL;
    }

    if (RTSP_SUCCESS_CODE != pMessage->getStatusCode())
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] handle describe response fail, status code[%u].",
                         getStreamId(), pMessage->getStatusCode()));
        return RET_FAIL;
    }



    std::string strSdp = pMessage->getSdp();
    if (RET_OK != m_rtspSdp.decodeSdp(strSdp))
    {

        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] handle describe response fail, parse sdp[%s] fail.",
                                getStreamId(), strSdp.c_str()));
        return RET_FAIL;
    }

    MEDIA_INFO_LIST VideoinfoList;
    MEDIA_INFO_LIST AudioinfoList;
    SDP_MEDIA_INFO* Videoinfo = NULL;
    SDP_MEDIA_INFO* Audioinfo = NULL;
    m_rtspSdp.getVideoInfo(VideoinfoList);
    m_rtspSdp.getAudioInfo(AudioinfoList);
    if(0 <VideoinfoList.size()) {
        Videoinfo = (SDP_MEDIA_INFO*)&VideoinfoList.front();
    }
    if(0 <AudioinfoList.size()) {
        Audioinfo = (SDP_MEDIA_INFO*)&AudioinfoList.front();
    }
    if(NULL != Videoinfo) {
        m_unVedioPT = Videoinfo->ucPayloadType;
    }
    if(NULL != Audioinfo) {
        m_unAudioPT = Audioinfo->ucPayloadType;
    }



    SVS_LOG((SVS_LM_INFO,"std session[%Q] success get sdp info, vedio pt[%u] audio pt[%u] trans [%u].",
                    getStreamId(), m_unVedioPT, m_unAudioPT, getTransProtocol()));

    m_stSessionInfo.TransProtocol = TRANS_PROTOCAL_TCP;
    if (RET_OK != allocStdMediaPort())
    {
        SVS_LOG((SVS_LM_WARNING,"std session[%Q] alloc std media port fail.", getStreamId()));
        return RET_FAIL;
    }

    CRtspSetupMessage request;


    std::string strControl = "";


    if(NULL != Videoinfo) {
        strControl = Videoinfo->strControl;
    }


    if (string::npos == strControl.find(RTSP_PROTOCOL_HEADER))
    {
        strControl = m_strRtspUrl +"/"+ strControl;
    }

    request.setMsgType(RTSP_MSG_REQ);
    request.setRtspUrl(strControl);
    request.setTransType(getTransProtocol());
    request.setCSeq(m_rtspProtocol.getCseq());
    request.setInterleaveNum(m_unVideoInterleaveNum);

    std::string strMsg;
    if (RET_OK != request.encodeMessage(strMsg))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] encode video setup request fail.",
                          getStreamId()));
        return RET_FAIL;
    }


    m_rtspProtocol.saveSendReq(request.getCSeq(), request.getMethodType());

    if (RET_OK != sendMessage(m_pTcpHandle, strMsg.c_str(), strMsg.length()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] send video setup request fail.", getStreamId()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"rtp session[%Q] send video setup request success.", getStreamId()));

    m_enSetupStatus = SETUP_STATUS_VIDEO;

    return RET_OK;
}

int32_t CMduRtpSession::handleStdRtspSetupResp(CRtspMessage &rtspMessage, CMduSession &peerSession)
{
    CRtspSetupMessage *pMessage = dynamic_cast<CRtspSetupMessage *> (&rtspMessage);
    if (!pMessage)
    {
        return RET_FAIL;
    }

    if (RTSP_SUCCESS_CODE != pMessage->getStatusCode())
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] handle setup response fail, status code[%u].",
                        getStreamId(), pMessage->getStatusCode()));

        if ((TRANS_PROTOCAL_TCP == getTransProtocol())
                && (SETUP_STATUS_VIDEO == m_enSetupStatus))
        {
            SVS_LOG((SVS_LM_INFO,"rtp session[%Q] retry udp setup request.", getStreamId()));

            m_pTcpHandle = NULL;
            m_stSessionInfo.TransProtocol = TRANS_PROTOCAL_UDP;
            if (RET_OK != allocStdMediaPort())
            {
                SVS_LOG((SVS_LM_WARNING,"std session[%Q] alloc std media port fail.", getStreamId()));
                return RET_FAIL;
            }

            CRtspSetupMessage rtspSetup;

            MEDIA_INFO_LIST VideoinfoList;
            SDP_MEDIA_INFO* Videoinfo = NULL;
            m_rtspSdp.getVideoInfo(VideoinfoList);
            std::string strControl = "";

            if(0 <VideoinfoList.size()) {
                Videoinfo = (SDP_MEDIA_INFO*)&VideoinfoList.front();
                strControl = Videoinfo->strControl;
            }

            if (string::npos == strControl.find(RTSP_PROTOCOL_HEADER))
            {
                strControl = m_strRtspUrl + "/"+ strControl;
            }

            rtspSetup.setMsgType(RTSP_MSG_REQ);
            rtspSetup.setRtspUrl(strControl);
            rtspSetup.setTransType(getTransProtocol());
            rtspSetup.setCSeq(m_rtspProtocol.getCseq());
            rtspSetup.setClientPort(m_VideoAddr.get_port_number());
            rtspSetup.setDestinationIp(m_VideoAddr.get_ip_address());

            std::string strMsg;
            if (RET_OK != rtspSetup.encodeMessage(strMsg))
            {
                SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] encode video setup request fail.",
                        getStreamId()));
                return RET_FAIL;
            }


            m_rtspProtocol.saveSendReq(rtspSetup.getCSeq(), rtspSetup.getMethodType());

            if (RET_OK != sendMessage(m_pTcpHandle, strMsg.c_str(),
                    strMsg.length()))
            {
                SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] send video setup request fail.", getStreamId()));
                return RET_FAIL;
            }

            SVS_LOG((SVS_LM_INFO,"rtp session[%Q] send retry udp setup request success.",
                    getStreamId()));
            return RET_OK;
        }

        return RET_FAIL;
    }

    if (getTransProtocol() != pMessage->getTransType())
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] handle setup response fail,"
                " not accepted trans type[%u].",
                getStreamId(), pMessage->getTransType()));
        return RET_FAIL;
    }

    (void)peerSession.handleRecvedNatRequest();
    setStatus(MDU_SESSION_STATUS_RECVED_NAT_REQ);

    int32_t nRet = RET_OK;
    if (SETUP_STATUS_VIDEO == m_enSetupStatus)
    {
        nRet = handleVideoRtspSetupResp(*pMessage);
    }
    else if (SETUP_STATUS_AUDIO == m_enSetupStatus)
    {
        nRet = handleAudioRtspSetupResp(*pMessage);
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] handle setup response fail setup status[%d],",
                         getStreamId(), m_enSetupStatus));
        return RET_FAIL;
    }

    return nRet;
}

int32_t CMduRtpSession::sendStartRequest()
{
    if ((RTSP_SESSION == getSessionType())
            && (PLAY_TYPE_LIVE == getPlayType()) )
    {
        std::string strUrl = m_rtspSdp.getUrl();
        if ("" == strUrl)
        {
            strUrl = m_strRtspUrl;
        }

        if ("" == strUrl)
        {
            MEDIA_INFO_LIST VideoinfoList;
            SDP_MEDIA_INFO* Videoinfo = NULL;
            m_rtspSdp.getVideoInfo(VideoinfoList);
            std::string strControl = "";

            if(0 <VideoinfoList.size()) {
                Videoinfo = (SDP_MEDIA_INFO*)&VideoinfoList.front();
                strUrl = Videoinfo->strControl;
            }
        }

        CRtspPlayMessage playReq;
        playReq.setMsgType(RTSP_MSG_REQ);
        playReq.setRtspUrl(strUrl);
        playReq.setSession(m_strVideoSession);
        playReq.setCSeq(m_rtspProtocol.getCseq());

        MEDIA_RANGE_S mediaRange;
        mediaRange.enRangeType = RANGE_TYPE_NPT;
        mediaRange.MediaBeginOffset = (uint32_t) OFFSET_CUR;
        mediaRange.MediaEndOffset = (uint32_t) OFFSET_END;
        playReq.setRange(mediaRange);

        std::string strMsg;
        if (RET_OK != playReq.encodeMessage(strMsg))
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] encode play request fail,",
                    getStreamId()));
            return RET_FAIL;
        }

        m_rtspProtocol.saveSendReq(playReq.getCSeq(), playReq.getMethodType());
        if (RET_OK != sendMessage(m_pTcpHandle, strMsg.c_str(), strMsg.length()))
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] send audio play request fail.\n%s,",
                    getStreamId(), strMsg.c_str()));
            return RET_FAIL;
        }

        SVS_LOG((SVS_LM_WARNING,"session[%Q] send video play request.\n%s",
                         getStreamId(), strMsg.c_str()));
        return RET_OK;
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] not supported start request.",
                     getStreamId()));
    return RET_OK;
}

int32_t CMduRtpSession::handleVideoRtspSetupResp(CRtspSetupMessage &setupMessage)
{
    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        m_unVideoInterleaveNum = setupMessage.getInterleaveNum();
        SVS_LOG((SVS_LM_INFO,"session[%Q] handle video setup response, interleave[%d]",
                getStreamId(), m_unVideoInterleaveNum));
    }
    else
    {
        if ((uint32_t)-1 == setupMessage.getSourceIp())
        {
            setupMessage.setSourceIp(m_ConnVideoAddr.get_ip_address());
        }
        m_UdpPeerAddr[VIDEO_RTP_HANDLE].set(setupMessage.getServerPort(),
                                            setupMessage.getSourceIp());
        m_UdpPeerAddr[VIDEO_RTCP_HANDLE].set(setupMessage.getServerPort() + 1,
                                            setupMessage.getSourceIp());
        SVS_LOG((SVS_LM_INFO,"session[%Q] handle video setup response, server addr[%s:%d]",
                getStreamId(),
                m_UdpPeerAddr[VIDEO_RTP_HANDLE].get_host_addr(),
                m_UdpPeerAddr[VIDEO_RTP_HANDLE].get_port_number()));
    }

    m_strVideoSession = setupMessage.getSession();

    MEDIA_INFO_LIST VideoinfoList;
    SDP_MEDIA_INFO* Videoinfo = NULL;
    m_rtspSdp.getVideoInfo(VideoinfoList);
    std::string strControl    = "";
    uint32_t    ucPayloadType = INVALID_PAYLOAD_TYPE;

    if(0 <VideoinfoList.size()) {
        Videoinfo = (SDP_MEDIA_INFO*)&VideoinfoList.front();
        strControl = Videoinfo->strControl;
        ucPayloadType = Videoinfo->ucPayloadType;
    }

    if (INVALID_PAYLOAD_TYPE != ucPayloadType)
    {
        if (string::npos == strControl.find(RTSP_PROTOCOL_HEADER))
        {
            strControl = m_strRtspUrl +"/"+ strControl;
        }

        setupMessage.setMsgType(RTSP_MSG_REQ);
        setupMessage.setRtspUrl(strControl);
        setupMessage.setCSeq(m_rtspProtocol.getCseq());
        setupMessage.setSession(m_strVideoSession);
        if (TRANS_PROTOCAL_TCP == setupMessage.getTransType())
        {
            setupMessage.setInterleaveNum(m_unAudioInterleaveNum);
        }
        else
        {
            setupMessage.setClientPort(m_AudioAddr.get_port_number());
            setupMessage.setDestinationIp(m_AudioAddr.get_ip_address());
        }

        string strMsg;
        if (RET_OK != setupMessage.encodeMessage(strMsg))
        {
            SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] encode audio setup request fail,",
                    getStreamId()));
            return RET_FAIL;
        }

        m_rtspProtocol.saveSendReq(setupMessage.getCSeq(), setupMessage.getMethodType());
        if (RET_OK != sendMessage(m_pTcpHandle, strMsg.c_str(), strMsg.length()))
        {
            SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] send message fail.\n%s,",
                    getStreamId(), strMsg.c_str()));
            return RET_FAIL;
        }

        m_enSetupStatus = SETUP_STATUS_AUDIO;
    }
    else
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] no audio info in sdp, begin start media port.",
                        getStreamId()));

        if (RET_OK != startStdMediaPort())
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] start media port fail.", getStreamId()));
            return RET_FAIL;
        }

        (void) m_rtspInitSem.release();
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] handle video setup response success", getStreamId()));
    return RET_OK;
}

int32_t CMduRtpSession::handleAudioRtspSetupResp(CRtspSetupMessage &setupMessage)
{
    m_strAudioSession = setupMessage.getSession();
    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        m_unAudioInterleaveNum = setupMessage.getInterleaveNum();
        SVS_LOG((SVS_LM_INFO,"session[%Q] handle audio setup response, interleave[%d]",
                getStreamId(), m_unVideoInterleaveNum));
    }
    else
    {
        if ((uint32_t)-1 == setupMessage.getSourceIp())
        {
            setupMessage.setSourceIp(m_ConnVideoAddr.get_ip_address());
        }
        m_UdpPeerAddr[AUDIO_RTP_HANDLE].set(setupMessage.getServerPort(),
                setupMessage.getSourceIp());
        m_UdpPeerAddr[AUDIO_RTCP_HANDLE].set(setupMessage.getServerPort() + 1,
                setupMessage.getSourceIp());
        SVS_LOG((SVS_LM_INFO,"session[%Q] handle audio setup response, server addr[%s:%d]",
                getStreamId(),
                m_UdpPeerAddr[AUDIO_RTP_HANDLE].get_host_addr(),
                m_UdpPeerAddr[AUDIO_RTP_HANDLE].get_port_number()));
    }


    if (RET_OK != startStdMediaPort())
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] start media port fail.",
                getStreamId()));
        return RET_FAIL;
    }

    (void) m_rtspInitSem.release();

    SVS_LOG((SVS_LM_INFO,"session[%Q] handle audio setup success.", getStreamId()));
    return RET_OK;
}

int32_t CMduRtpSession::handleStdRtspPlayResp(CRtspMessage &rtspMessage, CMduSession &peerSession)
{
    CRtspPlayMessage *pMessage = dynamic_cast<CRtspPlayMessage *> (&rtspMessage);
    if (!pMessage)
    {
        return RET_FAIL;
    }

    if (RTSP_SUCCESS_CODE != pMessage->getStatusCode())
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] handle play response fail, status code[%u].",
                getStreamId(), pMessage->getStatusCode()));
        return RET_FAIL;
    }

    if (m_bAudioPlay)
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] handle audio play response success.",
                         getStreamId()));
        m_bAudioPlay = false;
        return RET_OK;
    }

    if (MDU_SESSION_STATUS_DISPATCHING != getStatus())
    {

        MEDIA_INFO_LIST AudioinfoList;
        SDP_MEDIA_INFO* Audioinfo = NULL;
        m_rtspSdp.getAudioInfo(AudioinfoList);
        std::string strControl = "";
        uint8_t ucPayloadType = INVALID_PAYLOAD_TYPE;

        if(0 <AudioinfoList.size()) {
            Audioinfo = (SDP_MEDIA_INFO*)&AudioinfoList.front();
            strControl = Audioinfo->strControl;
            ucPayloadType = Audioinfo->ucPayloadType;
        }

        if ((INVALID_PAYLOAD_TYPE != ucPayloadType)
                && (m_strVideoSession != m_strAudioSession))
        {

            CRtspPlayMessage request;
            request.setMsgType(RTSP_MSG_REQ);
            request.setRtspUrl(strControl);
            request.setSession(m_strAudioSession);
            request.setCSeq(m_rtspProtocol.getCseq());

            MEDIA_RANGE_S mediaRange;
            mediaRange.enRangeType = RANGE_TYPE_NPT;
            mediaRange.MediaBeginOffset = (uint32_t)OFFSET_CUR;
            mediaRange.MediaEndOffset = (uint32_t)OFFSET_END;
            request.setRange(mediaRange);

            std::string strMsg;
            if (RET_OK != request.encodeMessage(strMsg))
            {
                SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] encode play request fail,",
                        getStreamId()));
                return RET_FAIL;
            }

            m_rtspProtocol.saveSendReq(request.getCSeq(), request.getMethodType());
            if (RET_OK != sendMessage(m_pTcpHandle, strMsg.c_str(),
                    strMsg.length()))
            {
                SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] send audio play request fail.\n%s,",
                        getStreamId(), strMsg.c_str()));
                return RET_FAIL;
            }

            m_bAudioPlay = true;
        }

        setStatus(MDU_SESSION_STATUS_DISPATCHING);
    }

    std::string strMsg;
    pMessage->setStatusCode(RTSP_SUCCESS_OK);
    (void) pMessage->encodeMessage(strMsg);

    CRtspPacket rtspPack;
    if (0 != rtspPack.parse(strMsg.c_str(), strMsg.length()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] handle play response fail, rtsp packet parse fail.",
                getStreamId()));
        return RET_FAIL;
    }

    if (RET_OK != peerSession.sendVcrMessage(rtspPack))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] send vcr play response fail.",
                        peerSession.getStreamId()));
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] handle play response success.",
                    getStreamId()));
    return RET_OK;
}

int32_t CMduRtpSession::handleStdRtspPauseResp(CRtspMessage & /*rtspMessage*/,
                                           CMduSession &/*peerSession*/) const
{
    SVS_LOG((SVS_LM_INFO,"session[%Q] handle pause response success.",
                     getStreamId()));
    return RET_OK;
}

int32_t CMduRtpSession::handleStdRtspTeardownResp(CRtspMessage & /*rtspMessage*/) const
{
    SVS_LOG((SVS_LM_INFO,"session[%Q] handle teardown response success.",
                     getStreamId()));
    return RET_OK;
}

int32_t CMduRtpSession::handleStdRtspAnnounceReq(CRtspMessage &rtspMessage, CMduSession &peerSession)
{
    CRtspAnnounceMessage *pMessage = dynamic_cast<CRtspAnnounceMessage *> (&rtspMessage);
    if (!pMessage)
    {
        return RET_FAIL;
    }

    if (RET_OK != peerSession.sendSessionStopMessage(MDU_PACKET_TYPE_SESSION_EOS))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] ask peer session[%Q] send eos notify fail.",
                        getStreamId(), peerSession.getStreamId()));
    }

    CRtspTeardownMessage request;
    pMessage->setMsgType(RTSP_MSG_RSP);
    pMessage->setStatusCode(RTSP_SUCCESS_OK);

    string strMsg;
    if (RET_OK != pMessage->encodeMessage(strMsg))
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] encode announce response fail.", getStreamId()));
        return RET_FAIL;
    }

    if (RET_OK != sendMessage(m_pTcpHandle, strMsg.c_str(), strMsg.length()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp session[%Q] send teardown message fail.\n%s,",
                getStreamId(), strMsg.c_str()));
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] handle announce request success.", getStreamId()));
    return RET_OK;
}

void CMduRtpSession::sendRtcpMessage()
{
    char buf[KILO] = { 0 };
    char* pRtcpBuff = buf + RTP_INTERLEAVE_LENGTH;
    uint32_t unRtcpLen = 0;

    if (TRANS_DIRECTION_RECVONLY == m_stSessionInfo.TransDirection)
    {
        (void)m_rtcpPacket.createReceiverReport(pRtcpBuff,
                                                KILO - RTP_INTERLEAVE_LENGTH,
                                                unRtcpLen);
        SVS_LOG((SVS_LM_INFO,"session[%Q] send rtcp receive report", getStreamId()));
    }
    else
    {
        (void)m_rtcpPacket.createSenderReport(pRtcpBuff,
                                              KILO + RTP_INTERLEAVE_LENGTH,
                                              unRtcpLen);
        SVS_LOG((SVS_LM_INFO,"session[%Q] send rtcp send report", getStreamId()));
    }


    if (TRANS_PROTOCAL_TCP == getTransProtocol())
    {
        if (!m_pTcpHandle)
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] send rtcp fail, tcp handle invalid.", getStreamId()));
            return;
        }


        buf[0] = RTP_INTERLEAVE_FLAG;
        buf[1] = (char)m_unVideoInterleaveNum + 1;
        *(uint16_t*) &buf[2] = htons((uint16_t)unRtcpLen );

        (void)sendMessage(m_pTcpHandle, buf, unRtcpLen + RTP_INTERLEAVE_LENGTH);

        buf[0] = RTP_INTERLEAVE_FLAG;
        buf[1] = (char)m_unAudioInterleaveNum + 1;
        *(uint16_t*) &buf[2] = htons((uint16_t)unRtcpLen );

        (void)sendMessage(m_pTcpHandle, buf, unRtcpLen + RTP_INTERLEAVE_LENGTH);
    }
    else
    {

        if (NULL != m_pUdpHandle[VIDEO_RTCP_HANDLE])
        {
            (void)m_pUdpHandle[VIDEO_RTCP_HANDLE]->sendMessage(getStreamId(),
                                                            pRtcpBuff,
                                                            unRtcpLen,
                                                            m_UdpPeerAddr[VIDEO_RTCP_HANDLE]);
        }
        if (NULL != m_pUdpHandle[AUDIO_RTCP_HANDLE])
        {
            (void) m_pUdpHandle[AUDIO_RTCP_HANDLE]->sendMessage(getStreamId(),
                                                            pRtcpBuff,
                                                            unRtcpLen,
                                                            m_UdpPeerAddr[AUDIO_RTCP_HANDLE]);
        }
    }

    return;
}

void CMduRtpSession::resetRtpPt(CRtpPacket &rtpPack)
{

    if (NULL == m_pPeerSession)
    {
        uint64_t ullRecvId = m_stSessionInfo.RecvStreamID;
        if (0 == ullRecvId)
        {
            return;
        }


        m_pPeerSession = CMduSessionFactory::instance()->findSession(ullRecvId);
    }

    if (NULL == m_pPeerSession)
    {
        return;
    }

    char cPt = rtpPack.GetPayloadType();


    if (m_pPeerSession->getVideoPayload() != getVideoPayload())
    {
        if (cPt == m_pPeerSession->getVideoPayload())
        {
            rtpPack.SetPayloadType(getVideoPayload());
        }
    }


    return;
}

