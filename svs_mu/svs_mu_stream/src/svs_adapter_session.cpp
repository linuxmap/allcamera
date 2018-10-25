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

CMduSession::CMduSession()
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

    m_enSessionStatus   = MDU_SESSION_STATUS_INIT;
    m_ulStatusBeginTime = SVS_GetSecondTime();
    m_SdpInfo           = "";

}

CMduSession::~CMduSession()
{
    SVS_LOG((SVS_LM_DEBUG,"destroy session [%Q] begin.",
                    getStreamId()));
    try
    {
        while (!m_NatMsgList.empty())
        {
            CMduNatMessage *pNatMsg = m_NatMsgList.front();
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

int32_t CMduSession::init(const char* pszContent,PLAY_TYPE enPlayType)
{

    if (MDU_SESSION_STATUS_INIT != getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,"CMduSession::init fail, session status [%d] is incorrect.",
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

    setStatus(MDU_SESSION_STATUS_WAIT_START);
    SVS_LOG((SVS_LM_INFO, "init session[%Q] success: contentid[%s] substream[%u] "
                    "service type[%s] peer type[%s].",
                    getStreamId(),
                    m_szContentId,
                    m_ulVideoCodeType,
                    CSessionInfo::instance().getPlayType(m_enPlayType),
                    CSessionInfo::instance().getPeerType(m_stSessionInfo.PeerType)));

    return RET_OK;
}

void CMduSession::close()
{
    if (MDU_SESSION_STATUS_RELEASED == getStatus())
    {
        return;
    }

    setStatus(MDU_SESSION_STATUS_RELEASED);

    (void) stopMediaPort();

    SVS_LOG((SVS_LM_INFO,"session close success. stream id[%Q].",
        getStreamId()));
    return;
}


int32_t CMduSession::start(CMduMediaSetupResp *pResp)
{
    if (MDU_SESSION_STATUS_WAIT_CHANNEL_REDAY <= getStatus())
    {
        SVS_LOG((SVS_LM_INFO, "session[%Q] has already started, status [%d].",
                         m_stSessionInfo.StreamID,
                         getStatus()));

        return RET_OK;
    }

    if (MDU_SESSION_STATUS_WAIT_START != getStatus())
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

    setStatus(MDU_SESSION_STATUS_RECVED_NAT_REQ);

    SVS_LOG((SVS_LM_INFO,"start session [%Q] success, video pt[%d] audio pt[%d], service type[%d] peer type[%d] session type[%d] .",
                    getStreamId(),
                    getVideoPayload(),
                    getAudioPayload(),
                    getPlayType(),
                    getPeerType(),
                    getSessionType()));
    return RET_OK;
}

ACE_INET_Addr CMduSession::getVideoAddr() const
{
    return m_VideoAddr;
}

ACE_INET_Addr CMduSession::getAudioAddr() const
{
    return m_AudioAddr;
}


void CMduSession::setStatus(MDU_SESSION_STATUS enStatus)
{
    ACE_Guard<ACE_Thread_Mutex>  locker(m_StatusMutex);
    if ((MDU_SESSION_STATUS_ABNORMAL == m_enSessionStatus)
        && (MDU_SESSION_STATUS_RELEASED != enStatus))
    {
        SVS_LOG((SVS_LM_DEBUG,"session[%Q] change stauts[%d] fail, status abnormal.",
                    getStreamId(), enStatus));
        return;
    }

    if (MDU_SESSION_STATUS_RELEASED == m_enSessionStatus)
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

MDU_SESSION_STATUS CMduSession::getStatus()
{
    ACE_Guard<ACE_Thread_Mutex>  locker(m_StatusMutex);
    return m_enSessionStatus;
}

void CMduSession::setSessionId(uint64_t ullSessionId)
{
    m_stSessionInfo.StreamID = ullSessionId;
    return;
}

uint64_svs CMduSession::getStreamId() const
{
    return m_stSessionInfo.StreamID;
}

const char* CMduSession::getContentID() const
{
    return (char*)m_szContentId;
}

void CMduSession::setContentID(const char* strContentID)
{
    (void)strncpy(m_szContentId, strContentID, CONTENT_ID_LEN );
}

uint32_t CMduSession::getVideoCodecType() const
{
    return m_ulVideoCodeType;
}

uint32_t CMduSession::getVideoCodeType() const
{
    return m_ulVideoCodeType;
}

uint8_t CMduSession::getVideoPayload()const
{
    return (uint8_t)m_unVedioPT;
}

uint8_t CMduSession::getAudioPayload()const
{
    return (uint8_t)m_unAudioPT;
}

PEER_TYPE CMduSession::getPeerType() const
{
    return m_stSessionInfo.PeerType;
}

PLAY_TYPE CMduSession::getPlayType() const
{
    return m_enPlayType;
}

char CMduSession::getVideoInterleaveNum() const
{
    return (char)m_unVideoInterleaveNum;
}

char CMduSession::getAudioInterleaveNum() const
{
    return (char)m_unAudioInterleaveNum;
}

uint32_t CMduSession::getTransProtocol() const
{
    return m_stSessionInfo.TransProtocol;
}


uint32_t CMduSession::getSessionType() const
{
    return m_stSessionInfo.SessionType;
}

uint32_t CMduSession::getSpecifyIp() const
{
    return m_stSessionInfo.SpecifyIp;
}
uint32_t CMduSession::getTransDirection() const
{
    return m_stSessionInfo.TransDirection;
}

void CMduSession::getSdpInfo(std::string& SdpInfo) const
{
    SdpInfo = m_SdpInfo;
    SVS_LOG((SVS_LM_DEBUG,"CMduSession SDPInfon:"
                          "\t\t%s",
                          m_SdpInfo.c_str()));
    return;
}

int32_t CMduSession::dealSccVcrResp(const SVS_MSG_PLAYBACK_CONTROL_RESP* pMsg)
{
    return RET_OK;
}

void CMduSession::checkSessionStatus()
{
    uint32_t unCostTime = SVS_GetSecondTime() - m_ulStatusBeginTime;
    if (MDU_SESSION_STATUS_DISPATCHING > getStatus())
    {
        if (MDU_STATUS_TIMEOUT_INTERVAL <= unCostTime)
        {
            SVS_LOG((SVS_LM_WARNING,"session[%Q] at status[%d] timeout, const time[%u].",
                        getStreamId(),  getStatus(), unCostTime));

            setStatus(MDU_SESSION_STATUS_ABNORMAL);
            return;
        }

        return;
    }

    if (MDU_SESSION_STATUS_ABNORMAL == getStatus())
    {
        if (MDU_STATUS_ABNORMAL_INTERVAL <= unCostTime)
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

        setStatus(MDU_SESSION_STATUS_ABNORMAL);
        return;
    }

    SVS_LOG((SVS_LM_DEBUG,"session[%Q] check status success", getStreamId()));
    return;
}
uint64_t CMduSession::getConnStreamID()
{
    return m_ullConnStreamID;
}


int32_t CMduSession::addReference()
{
    m_lReferenceCnt++;
    SVS_LOG((SVS_LM_DEBUG,"addReference session[%Q] ref[%d].", getStreamId(), m_lReferenceCnt));
    return m_lReferenceCnt;
}

int32_t CMduSession::decReference()
{
    m_lReferenceCnt--;
    SVS_LOG((SVS_LM_DEBUG,"decReference session[%Q] ref[%d].", getStreamId(), m_lReferenceCnt));
    return m_lReferenceCnt;
}

uint32_t CMduSession::getMediaTransType()const
{
    return m_stSessionInfo.MediaTransType;
}


int32_t CMduSession::handleNatMessage(CMduNatMessage &natMsg)
{
    if (MDU_SESSION_STATUS_WAIT_CHANNEL_REDAY > getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] handle nat message fail, status[%d] abnormal.",
                getStreamId(),  getStatus()));
        return RET_FAIL;
    }

    CMduSession* pPeerSession = (CMduSession*)natMsg.m_pPeerSession;
    if (NULL == pPeerSession)
    {
        return RET_FAIL;
    }

    if (MDU_SESSION_STATUS_DISPATCHING != getStatus())
    {
        if (MDU_SESSION_STATUS_RECVED_NAT_REQ > pPeerSession->getStatus())
        {
            CMduNatMessage *pNatMsg = NULL;
            try
            {
                pNatMsg = new CMduNatMessage(natMsg);
            }
            catch(...)
            {
                SVS_LOG((SVS_LM_DEBUG,"session[%Q] create waiting nat message fail.", getStreamId()));
                return RET_FAIL;
            }

            m_NatMsgList.push_back(pNatMsg);
            SVS_LOG((SVS_LM_DEBUG,"session[%Q] create waiting nat message[%p].",
                         getStreamId(), pNatMsg));
            setStatus(MDU_SESSION_STATUS_RECVED_NAT_REQ);
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
        CMduNatMessage *pNatMsg = m_NatMsgList.front();
        (void)sendNatResponse(*pNatMsg);
        SVS_LOG((SVS_LM_DEBUG,"session[%Q] delete waiting nat message[%p] when nat success.",
                getStreamId(), pNatMsg));
        delete pNatMsg;
        m_NatMsgList.pop_front();
    }

    if (MDU_SESSION_STATUS_DISPATCHING != getStatus())
    {
        setStatus(MDU_SESSION_STATUS_DISPATCHING);

        sendStartStreamRequest(*pPeerSession);
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] handle nat message success.", getStreamId()));

    return RET_OK;
}//lint !e429


void CMduSession::sendStartStreamRequest(CMduSession &peerSession)
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
        CMduRtpSession *RtpSession = NULL;
        if (PEER_TYPE_PU == getPeerType())
        {
            RtpSession = dynamic_cast<CMduRtpSession*>(this);
        }
        else
        {
            RtpSession = dynamic_cast<CMduRtpSession*>(&peerSession);
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

int32_t CMduSession::handleAudioBroadcastNatMsg(CMduSession &peerSession, CMduNatMessage &natMsg)
{
    if (RET_OK != sendNatResponse(natMsg))
    {
        SVS_LOG((SVS_LM_INFO,"session[%Q] handle nat message: send nat response fail.",
                getStreamId()));
        return RET_FAIL;
    }

    setStatus(MDU_SESSION_STATUS_DISPATCHING);

    if (PEER_TYPE_PU == m_stSessionInfo.PeerType)
    {
        (void)peerSession.handleRecvedNatRequest();
    }

    SVS_LOG((SVS_LM_INFO,"session[%Q] handle audio broadcast nat message success.", getStreamId()));
    return RET_OK;
}
int32_t CMduSession::initSessionBySdp()
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


int32_t CMduSession::handleRecvedNatRequest()
{
    if (MDU_SESSION_STATUS_DISPATCHING <= getStatus())
    {
        return RET_OK;
    }

    (void)sendStartRequest();

    if (MDU_SESSION_STATUS_RECVED_NAT_REQ != getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] handle recved nat message fail, status[%d] invalid.",
                        getStreamId(), getStatus()));
        return RET_FAIL;
    }

    while (!m_NatMsgList.empty())
    {
        CMduNatMessage *pNatMsg = m_NatMsgList.front();

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

    setStatus(MDU_SESSION_STATUS_DISPATCHING);

    SVS_LOG((SVS_LM_INFO,"session[%Q] handle recved nat message success.",
                     getStreamId()));
    return RET_OK;
}

void CMduSession::simulateNatReq()
{
    if (MDU_SESSION_STATUS_RECVED_NAT_REQ == getStatus())
    {
        setStatus(MDU_SESSION_STATUS_DISPATCHING);
        SVS_LOG((SVS_LM_DEBUG,"CMduSession:: status :MDU_SESSION_STATUS_DISPATCHING"));
    }

    return;
}

void CMduSession::Dump(ACE_HANDLE handle)const
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

