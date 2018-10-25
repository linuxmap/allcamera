#include "svs_adapter_time.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include <vms/vms.h>

#include "svs_adapter_session.h"
#include "svs_adapter_session_factory.h"
#include "svs_log_msg.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_exchange.h"
#include "svs_adapter_service_task.h"
#include "svs_vms_msg_factory.h"
#include "svs_adapter_business_manager.h"
#include "svs_adapter_rtp_session.h"
#include "svs_adapter_service_task.h"
#include <svs_media_nat_mgr.h>


CSessionInfo::CSessionInfo()
{
}

CSessionInfo::~CSessionInfo()
{
}

const char* CSessionInfo::getPlayType(PLAY_TYPE Type)const
{
    const char* szServiceType[] =
    {
        "realtime video",
        "platform playback",
        "front playback",
        ""
    };

    if (Type >= sizeof(szServiceType) / sizeof(szServiceType[0]))
    {
        return szServiceType[PLAY_TYPE_MAX];
    }

    return szServiceType[Type];
}

const char* CSessionInfo::getPeerType(PEER_TYPE Type)const
{
    const char* szPeerType[] =
    {
        "unkonw",
        "PU",
        "CU",
        "RECORD",
        "STREAM",
        ""
    };

    if (Type >= sizeof(szPeerType) / sizeof(szPeerType[0]))
    {
        return szPeerType[PEER_TYPE_MAX];
    }

    return szPeerType[Type];
}

const char* CSessionInfo::getTransProtocol(uint32_t Type)const
{
    const char* szTransProtocol[] =
    {
        "",
        "TCP",
        "UDP"
    };

    if (Type >= sizeof(szTransProtocol) / sizeof(szTransProtocol[0]))
    {
        return szTransProtocol[0];
    }

    return szTransProtocol[Type];
}

CStreamSession::CStreamSession()
{
    m_lReferenceCnt     = 1;
    memset(m_szContentId, 0x0, sizeof(m_szContentId));

    m_ulVideoCodeType  = 0;
    m_unVedioPT         = 0;
    m_unAudioCodecType  = 0;
    m_unAudioPT         = 0;

    m_unVideoInterleaveNum = 0;
    m_unAudioInterleaveNum = 2;

    m_unConnSetupType   = CONN_SETUP_TYPE_PASSIVE;
    m_ullConnStreamID   = 0;
    m_strRtspUrl        = "";

    m_NatMsgList.clear();
    m_bStartPlayFlag    = false;

    m_enSessionStatus   = STREAM_SESSION_STATUS_INIT;
    m_ulStatusBeginTime = SVS_GetSecondTime();
    m_SdpInfo           = "";

}

CStreamSession::~CStreamSession()
{
    SVS_LOG((SVS_LM_DEBUG,"destroy session [%Q] begin.",
                    getStreamId()));
    try
    {
        while (!m_NatMsgList.empty())
        {
            CStreamNatMessage *pNatMsg = m_NatMsgList.front();
            delete pNatMsg;
            m_NatMsgList.pop_front();
        }
    }
    catch(...)
    {

    }
    SVS_LOG((SVS_LM_DEBUG,"destroy session [%Q] end.",
                    getStreamId()));
}

int32_t CStreamSession::init(const char* pszContent,PLAY_TYPE enPlayType)
{

    if (STREAM_SESSION_STATUS_INIT != getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,"CStreamSession::init fail, session status [%d] is incorrect.",
            getStatus()));
        return RET_ERR_STATUS_ABNORMAL;
    }

    strncpy(m_szContentId, pszContent, CONTENT_ID_LEN);

    m_enPlayType = enPlayType;

    if (RET_OK != allocMediaPort())
    {
        SVS_LOG((SVS_LM_WARNING,"alloc rtp media port fail, session[%Q].",
                getStreamId()));
        return RET_FAIL;
    }

    setStatus(STREAM_SESSION_STATUS_WAIT_START);
    SVS_LOG((SVS_LM_INFO, "init session[%Q] success: contentid[%s] substream[%u] "
                    "service type[%s] peer type[%s].",
                    getStreamId(),
                    m_szContentId,
                    m_ulVideoCodeType,
                    CSessionInfo::instance().getPlayType(m_enPlayType),
                    CSessionInfo::instance().getPeerType(m_stSessionInfo.PeerType)));

    return RET_OK;
}

void CStreamSession::close()
{
    if (STREAM_SESSION_STATUS_RELEASED == getStatus())
    {
        return;
    }

    setStatus(STREAM_SESSION_STATUS_RELEASED);

    (void) stopMediaPort();

    SVS_LOG((SVS_LM_INFO,"session close success. stream id[%Q].",
        getStreamId()));
    return;
}


int32_t CStreamSession::start(CStreamMediaSetupResp *pResp)
{
    if (STREAM_SESSION_STATUS_WAIT_CHANNEL_REDAY <= getStatus())
    {
        SVS_LOG((SVS_LM_INFO, "session[%Q] has already started, status [%d].",
                         m_stSessionInfo.StreamID,
                         getStatus()));

        return RET_OK;
    }

    if (STREAM_SESSION_STATUS_WAIT_START != getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,
            "start session[%Q] fail, status [%d] is incorrect. stream id.",
            m_stSessionInfo.StreamID,
            getStatus()));
        return RET_ERR_STATUS_ABNORMAL;
    }

    pResp->getSdpInfo(m_SdpInfo);

    if (RET_OK != initSessionBySdp())
    {
        SVS_LOG((SVS_LM_INFO,"start session [%Q] fail, init the session by sdp fail.",
                getStreamId()));
        return RET_FAIL;
    }

    if (RET_OK != setRemoteAddress())
    {
        SVS_LOG((SVS_LM_INFO,"start session [%Q] fail, set remote address fail.",
                getStreamId()));
        return RET_FAIL;
    }

    if (RET_OK != startMediaPort())
    {
        SVS_LOG((SVS_LM_WARNING,"start session[%Q] fail, start port fail.",
            m_stSessionInfo.StreamID));
        return RET_FAIL;
    }

    setStatus(STREAM_SESSION_STATUS_RECVED_NAT_REQ);

    SVS_LOG((SVS_LM_INFO,"start session [%Q] success, video pt[%d] audio pt[%d], service type[%d] peer type[%d] session type[%d] .",
                    getStreamId(),
                    getVideoPayload(),
                    getAudioPayload(),
                    getPlayType(),
                    getPeerType(),
                    getSessionType()));
    return RET_OK;
}

ACE_INET_Addr CStreamSession::getVideoAddr() const
{
    return m_VideoAddr;
}

ACE_INET_Addr CStreamSession::getAudioAddr() const
{
    return m_AudioAddr;
}


void CStreamSession::setStatus(STREAM_SESSION_STATUS enStatus)
{
    ACE_Guard<ACE_Thread_Mutex>  locker(m_StatusMutex);
    if ((STREAM_SESSION_STATUS_ABNORMAL == m_enSessionStatus)
        && (STREAM_SESSION_STATUS_RELEASED != enStatus))
    {
        SVS_LOG((SVS_LM_DEBUG,"session[%Q] change stauts[%d] fail, status abnormal.",
                    getStreamId(), enStatus));
        return;
    }

    if (STREAM_SESSION_STATUS_RELEASED == m_enSessionStatus)
    {
        SVS_LOG((SVS_LM_DEBUG,"session[%Q] change stauts[%d] fail, session released.",
                getStreamId(), enStatus));
        return;
    }

    SVS_LOG((SVS_LM_DEBUG,"session[%Q] change stauts: old status[%d] new status[%d].",
            getStreamId(), m_enSessionStatus, enStatus));

    m_enSessionStatus   = enStatus;
    m_ulStatusBeginTime = SVS_GetSecondTime();
    return;
}

STREAM_SESSION_STATUS CStreamSession::getStatus()
{
    ACE_Guard<ACE_Thread_Mutex>  locker(m_StatusMutex);
    return m_enSessionStatus;
}

void CStreamSession::setSessionId(uint64_t ullSessionId)
{
    m_stSessionInfo.StreamID = ullSessionId;
    return;
}

uint64_svs CStreamSession::getStreamId() const
{
    return m_stSessionInfo.StreamID;
}

const char* CStreamSession::getContentID() const
{
    return (char*)m_szContentId;
}

void CStreamSession::setContentID(const char* strContentID)
{
    (void)strncpy(m_szContentId, strContentID, CONTENT_ID_LEN );
}

uint32_t CStreamSession::getVideoCodecType() const
{
    return m_ulVideoCodeType;
}

uint32_t CStreamSession::getVideoCodeType() const
{
    return m_ulVideoCodeType;
}

uint8_t CStreamSession::getVideoPayload()const
{
    return (uint8_t)m_unVedioPT;
}

uint8_t CStreamSession::getAudioPayload()const
{
    return (uint8_t)m_unAudioPT;
}

PEER_TYPE CStreamSession::getPeerType() const
{
    return m_stSessionInfo.PeerType;
}

PLAY_TYPE CStreamSession::getPlayType() const
{
    return m_enPlayType;
}

char CStreamSession::getVideoInterleaveNum() const
{
    return (char)m_unVideoInterleaveNum;
}

char CStreamSession::getAudioInterleaveNum() const
{
    return (char)m_unAudioInterleaveNum;
}

uint32_t CStreamSession::getTransProtocol() const
{
    return m_stSessionInfo.TransProtocol;
}


uint32_t CStreamSession::getSessionType() const
{
    return m_stSessionInfo.SessionType;
}

uint32_t CStreamSession::getSpecifyIp() const
{
    return m_stSessionInfo.SpecifyIp;
}
uint32_t CStreamSession::getTransDirection() const
{
    return m_stSessionInfo.TransDirection;
}

void CStreamSession::getSdpInfo(std::string& SdpInfo) const
{
    SdpInfo = m_SdpInfo;
    SVS_LOG((SVS_LM_DEBUG,"CStreamSession SDPInfon:"
                          "\t\t%s",
                          m_SdpInfo.c_str()));
    return;
}

int32_t CStreamSession::dealSccVcrResp(const SVS_MSG_PLAYBACK_CONTROL_RESP* pMsg)
{
    return RET_OK;
}

void CStreamSession::checkSessionStatus()
{
    uint32_t unCostTime = SVS_GetSecondTime() - m_ulStatusBeginTime;
    if (STREAM_SESSION_STATUS_DISPATCHING > getStatus())
    {
        if (STREAM_STATUS_TIMEOUT_INTERVAL <= unCostTime)
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] at status[%d] timeout, const time[%u].",
                        getStreamId(),  getStatus(), unCostTime));

            setStatus(STREAM_SESSION_STATUS_ABNORMAL);
            return;
        }

        return;
    }

    if (STREAM_SESSION_STATUS_ABNORMAL == getStatus())
    {
        if (STREAM_STATUS_ABNORMAL_INTERVAL <= unCostTime)
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] at abnormal status [%u]s, release session.",
                    getStreamId(), unCostTime));

            //TODO: release the session
            return;
        }

        return;
    }

    if (!checkMediaChannelStatus())
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] check media channel status fail",
                getStreamId()));

        setStatus(STREAM_SESSION_STATUS_ABNORMAL);
        return;
    }

    SVS_LOG((SVS_LM_DEBUG,"session[%Q] check status success", getStreamId()));
    return;
}
uint64_t CStreamSession::getConnStreamID()
{
    return m_ullConnStreamID;
}


int32_t CStreamSession::addReference()
{
    m_lReferenceCnt++;
    SVS_LOG((SVS_LM_DEBUG,"addReference session[%Q] ref[%d].", getStreamId(), m_lReferenceCnt));
    return m_lReferenceCnt;
}

int32_t CStreamSession::decReference()
{
    m_lReferenceCnt--;
    SVS_LOG((SVS_LM_DEBUG,"decReference session[%Q] ref[%d].", getStreamId(), m_lReferenceCnt));
    return m_lReferenceCnt;
}

uint32_t CStreamSession::getMediaTransType()const
{
    return m_stSessionInfo.MediaTransType;
}


int32_t CStreamSession::handleNatMessage(CStreamNatMessage &natMsg)
{
    if (STREAM_SESSION_STATUS_WAIT_CHANNEL_REDAY > getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] handle nat message fail, status[%d] abnormal.",
                getStreamId(),  getStatus()));
        return RET_FAIL;
    }

    CStreamSession* pPeerSession = (CStreamSession*)natMsg.m_pPeerSession;
    if (NULL == pPeerSession)
    {
        return RET_FAIL;
    }

    if (STREAM_SESSION_STATUS_DISPATCHING != getStatus())
    {
        if (STREAM_SESSION_STATUS_RECVED_NAT_REQ > pPeerSession->getStatus())
        {
            CStreamNatMessage *pNatMsg = NULL;
            try
            {
                pNatMsg = new CStreamNatMessage(natMsg);
            }
            catch(...)
            {
                SVS_LOG((SVS_LM_DEBUG,"session[%Q] create waiting nat message fail.", getStreamId()));
                return RET_FAIL;
            }

            m_NatMsgList.push_back(pNatMsg);
            SVS_LOG((SVS_LM_DEBUG,"session[%Q] create waiting nat message[%p].",
                         getStreamId(), pNatMsg));
            setStatus(STREAM_SESSION_STATUS_RECVED_NAT_REQ);
            SVS_LOG((SVS_LM_INFO,"session[%Q] attached session[%Q] has not NAT success.",
                    getStreamId(),
                    pPeerSession->getStreamId()));
            return RET_OK;  //lint !e429
        }

        (void)pPeerSession->handleRecvedNatRequest();
    }

    if (RET_OK != sendNatResponse(natMsg))
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] handle nat message: send nat response fail.",
                getStreamId()));
        return RET_FAIL;
    }

    while (!m_NatMsgList.empty())
    {
        CStreamNatMessage *pNatMsg = m_NatMsgList.front();
        (void)sendNatResponse(*pNatMsg);
        SVS_LOG((SVS_LM_DEBUG,"session[%Q] delete waiting nat message[%p] when nat success.",
                getStreamId(), pNatMsg));
        delete pNatMsg;
        m_NatMsgList.pop_front();
    }

    if (STREAM_SESSION_STATUS_DISPATCHING != getStatus())
    {
        setStatus(STREAM_SESSION_STATUS_DISPATCHING);

        sendStartStreamRequest(*pPeerSession);
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] handle nat message success.", getStreamId()));

    return RET_OK;
}//lint !e429


void CStreamSession::sendStartStreamRequest(CStreamSession &peerSession)
{
    CRtspPacket rtspPack;
    rtspPack.setCseq(0);
    rtspPack.setMethodIndex(RtspPlayMethod);

    if (PLAY_TYPE_PLAT_RECORD == getPlayType())
    {
        /* TODO */
        /*
        if (NULL == pSession)
        {
            return;
        }

        std::string strRtspUrl = "rtsp://vds.AllCam.com/plat.sdp";
        rtspPack.setRtspUrl(strRtspUrl);
        rtspPack.setRangeTime(RELATIVE_TIME, 0, 0);
        (void)pSession->sendVcrMessage(rtspPack);
        */
        return;
    }

    if (PLAY_TYPE_FRONT_RECORD  == getPlayType())
    {
        CStreamRtpSession *RtpSession = NULL;
        if (PEER_TYPE_PU == getPeerType())
        {
            RtpSession = dynamic_cast<CStreamRtpSession*>(this);
        }
        else
        {
            RtpSession = dynamic_cast<CStreamRtpSession*>(&peerSession);
        }

        if (NULL == RtpSession)
        {
            return;
        }

        rtspPack.setRangeTime(RtpSession->m_unRangeType,
                              RtpSession->m_unRangeStart,
                              RtpSession->m_unRangeStop);

        (void)RtpSession->sendVcrMessage(rtspPack);
    }

    return;
}

int32_t CStreamSession::handleAudioBroadcastNatMsg(CStreamSession &peerSession, CStreamNatMessage &natMsg)
{
    if (RET_OK != sendNatResponse(natMsg))
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] handle nat message: send nat response fail.",
                getStreamId()));
        return RET_FAIL;
    }

    setStatus(STREAM_SESSION_STATUS_DISPATCHING);

    if (PEER_TYPE_PU == m_stSessionInfo.PeerType)
    {
        (void)peerSession.handleRecvedNatRequest();
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] handle audio broadcast nat message success.", getStreamId()));
    return RET_OK;
}
int32_t CStreamSession::initSessionBySdp()
{
    if(0 == m_SdpInfo.length())
    {
        return RET_OK;
    }
    CMediaSdp objsdp;
    int32_t nRet = objsdp.decodeSdp(m_SdpInfo);
    if(RET_OK != nRet) {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] init session by sdp fail, status[%d],decode sdp:[%s] fail.",
                        getStreamId(), getStatus(),m_SdpInfo.c_str()));
        return RET_FAIL;
    }

    TRANS_DIRECTION enTranDirect = objsdp.getTransDirect();

    if(TRANS_DIRECTION_MAX != enTranDirect) {
        m_stSessionInfo.TransDirection = enTranDirect;
    }

    MEDIA_INFO_LIST vInfoList;

    objsdp.getVideoInfo(vInfoList);

    if(0 == vInfoList.size()) {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] init session by sdp fail, status[%d],video info is empty,sdp:[%s] fail.",
                        getStreamId(), getStatus(),m_SdpInfo.c_str()));
        return RET_FAIL;
    }

    if(1 < vInfoList.size()) {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] init session by sdp fail, status[%d],video info more than one,sdp:[%s] fail.",
                        getStreamId(), getStatus(),m_SdpInfo.c_str()));
        return RET_FAIL;
    }

    SDP_MEDIA_INFO* Videoinfo = (SDP_MEDIA_INFO*)&vInfoList.front();

    if(Videoinfo->strRtpmap == PS_VIDEO_RTPMAP) {
        m_stSessionInfo.MediaTransType = MEDIA_TRANS_TYPE_PS;
    }
    else if(Videoinfo->strRtpmap == H264_VIDEO_RTPMAP) {
        m_stSessionInfo.MediaTransType = MEDIA_TRANS_TYPE_RTP;
    }
    else if(Videoinfo->strRtpmap == H265_VIDEO_RTPMAP) {
        m_stSessionInfo.MediaTransType = MEDIA_TRANS_TYPE_RTP;
    }
    else {
        m_stSessionInfo.MediaTransType = MEDIA_TRANS_TYPE_MAX;
    }

    return RET_OK;
}


int32_t CStreamSession::handleRecvedNatRequest()
{
    if (STREAM_SESSION_STATUS_DISPATCHING <= getStatus())
    {
        return RET_OK;
    }

    (void)sendStartRequest();

    if (STREAM_SESSION_STATUS_RECVED_NAT_REQ != getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] handle recved nat message fail, status[%d] invalid.",
                        getStreamId(), getStatus()));
        return RET_FAIL;
    }

    while (!m_NatMsgList.empty())
    {
        CStreamNatMessage *pNatMsg = m_NatMsgList.front();

        if (RET_OK != sendNatResponse(*pNatMsg))
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] handle recved nat message, send nat response fail.",
                    getStreamId()));
            return RET_FAIL;
        }

        SVS_LOG((SVS_LM_DEBUG,"session[%Q] delete waiting nat message[%p] when nat success.",
                        getStreamId(), pNatMsg));
        delete pNatMsg;
        m_NatMsgList.pop_front();
    }

    setStatus(STREAM_SESSION_STATUS_DISPATCHING);

    SVS_LOG((SVS_LM_INFO,"session[%Q] handle recved nat message success.",
                     getStreamId()));
    return RET_OK;
}

void CStreamSession::simulateNatReq()
{
    if (STREAM_SESSION_STATUS_RECVED_NAT_REQ == getStatus())
    {
        setStatus(STREAM_SESSION_STATUS_DISPATCHING);
        SVS_LOG((SVS_LM_DEBUG,"CStreamSession:: status :STREAM_SESSION_STATUS_DISPATCHING"));
    }

    return;
}

void CStreamSession::Dump(ACE_HANDLE handle)const
{
    char buf[KILO + 1] = {0};

    snprintf(buf,
             KILO,
             "session [%lld] ContentID[%s] service type[%s] peer type[%s] trans protocol[%s] Ref:[%d]",
             getStreamId(),
             getContentID(),
             CSessionInfo::instance().getPlayType(m_enPlayType),
             CSessionInfo::instance().getPeerType(m_stSessionInfo.PeerType),
             CSessionInfo::instance().getTransProtocol(m_stSessionInfo.TransProtocol),
             m_lReferenceCnt);
    (void)ACE::send(handle, buf, strlen(buf));
    return;
}

