/*
 * MduRtspPushSession.cpp
 *
 *  Created on: 2016-5-16
 *      Author:
 */
#include <sstream>
#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_time.h"
#include "svs_log_msg.h"
#include <vms/vms.h>
#include "md5.h"
#include "svs_rtsp_push_session.h"
#include "svs_vms_msg_factory.h"
#include "svs_vms_media_setup_req.h"
#include "svs_vms_media_setup_resp.h"
#include "svs_vms_media_play_req.h"
#include "svs_vms_media_play_resp.h"
#include "svs_vms_media_teardown_req.h"
#include "svs_vms_media_keyframe_req.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_business_manager.h"
#include "svs_adapter_inner_msg.h"
#include "svs_adapter_business_manager.h"
#include "svs_adapter_service_task.h"

#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_media_exchange.h"

#include "svs_rtsp_options_message.h"
#include "svs_rtsp_describe_message.h"
#include "svs_rtsp_setup_message.h"
#include "svs_rtsp_play_message.h"
#include "svs_rtsp_pause_message.h"
#include "svs_rtsp_teardown_message.h"
#include "svs_rtsp_record_message.h"
#include "svs_rtsp_get_parameter_message.h"
#include "svs_rtsp_announce_message.h"
#include "svs_rtsp_service.h"
#include "svs_adapter_svs_def.h"
#include "svs_adapter_config.h"

CMduRtspPushSession::CMduRtspPushSession()
{
    m_unSessionIndex  = 0;
    m_enPlayType      = PLAY_TYPE_LIVE;
    m_bSetUp          = false;
    m_sockHandle      = ACE_INVALID_HANDLE;
    m_pRecvBuffer     = NULL;
    m_pRtpSession     = NULL;
    m_pPeerSession    = NULL;
    m_pLastRtspMsg    = NULL;

    m_unSessionStatus  = RTSP_SESSION_STATUS_INIT;
    m_ulStatusTime     = SVS_GetSecondTime();

    m_strContentID     = "";
    m_bFirstSetupFlag  = true;
    m_strPlayRange     = "";
    m_lRedoTimerId     = -1;

    m_unTransType      = TRANS_PROTOCAL_UDP;
    m_cVideoInterleaveNum = 0;
    m_cAudioInterleaveNum = 0;
}

CMduRtspPushSession::~CMduRtspPushSession()
{
    m_unSessionIndex  = 0;
    m_sockHandle      = ACE_INVALID_HANDLE;
    if(NULL != m_pRecvBuffer) {
        delete m_pRecvBuffer;
    }
    m_pRecvBuffer     = NULL;
    m_pRtpSession     = NULL;
    m_pPeerSession    = NULL;
    m_pLastRtspMsg    = NULL;

    m_bFirstSetupFlag  = true;
    m_lRedoTimerId     = -1;
}

int32_t CMduRtspPushSession::open(uint32_t unIndex, const ACE_INET_Addr &peerAddr)
{
    m_unSessionIndex  = unIndex;
    m_PeerAddr       = peerAddr;

    if (ACE_INVALID_HANDLE == m_sockHandle)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtsp push session[%u] fail, socket handle invalid",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    int32_t nRet = setSockOpt();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtsp push session[%u] fail, set socket option fail.",
                         m_unSessionIndex));
        return RET_FAIL;
    }

    if (NULL == m_pRecvBuffer)
    {
        try
        {
            m_pRecvBuffer = new Static_PreAssign_Buffer;
        }
        catch (...)
        {
            return RET_ERR_SYS_NEW;
        }
        m_pRecvBuffer->wr_ptr(0);
        m_pRecvBuffer->rd_ptr(0);
    }

    ACE_Reactor *pReactor = ACE_Reactor::instance();
    if (!pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtsp push session fail, can't find reactor instance."));
        return RET_FAIL;
    }

    nRet = pReactor->register_handler(m_sockHandle, this,
                                      ACE_Event_Handler::READ_MASK);
    /*
    nRet = pReactor->register_handler(m_sockHandle, this,
                                      ACE_Event_Handler::READ_MASK|ACE_Event_Handler::WRITE_MASK);
    */
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtsp server fail, register read mask fail[%d].",
                ACE_OS::last_error()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"open rtsp push session[%u] success, peer addr[%s:%d], handle[%d].",
                    m_unSessionIndex, m_PeerAddr.get_host_addr(),
                    m_PeerAddr.get_port_number(),
                    m_sockHandle));
    return RET_OK;
}

void CMduRtspPushSession::close()
{
    (void)ACE_OS::shutdown(m_sockHandle, SHUT_RDWR);
    SVS_LOG((SVS_LM_INFO,"close rtsp push session[%u] success.handle[%d]",
        m_unSessionIndex, m_sockHandle));
    return;
}

void CMduRtspPushSession::setStatus(uint32_t unStatus)
{
    uint32_t unOldStatus = m_unSessionStatus;
    m_unSessionStatus        = unStatus;
    m_ulStatusTime           = SVS_GetSecondTime();
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] switch status[%u] to [%u].",
            m_unSessionIndex, unOldStatus, m_unSessionStatus));
    return;
}

uint32_t CMduRtspPushSession::getStatus() const
{
    return m_unSessionStatus;
}

int32_t CMduRtspPushSession::handleSvsMessage(CMduSvsMessage &message)
{
    if(SVS_MSG_TYPE_MDU_SESSION_PLAY_RESP == message.getMsgType())
    {
        /* Key Frame request */
        sendKeyFrameReq();
        return RET_OK;
    }
    CMduMediaSetupResp *pResp = dynamic_cast<CMduMediaSetupResp*>(&message);
    if (!pResp)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle svs message fail, invalid message.",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    if (0 != pResp->getRespCode())
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle svs message fail, retcode[0x%x].",
                                m_unSessionIndex, pResp->getRespCode()));
        if (NULL != m_pLastRtspMsg)
        {
            sendCommonResp(RTSP_SERVER_INTERNAL, m_pLastRtspMsg->getCSeq());
        }
        return RET_FAIL;
    }

    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_RtspMutex);
    if (NULL == m_pLastRtspMsg)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle svs message fail, last rtsp message is null.",
                         m_unSessionIndex));
        return RET_FAIL;
    }

    if ((NULL == m_pPeerSession )||(RET_OK != m_pPeerSession->start(pResp)))
    {
        SVS_LOG((SVS_LM_ERROR,"start the peer session fail."));
        return RET_FAIL;
    }

    uint64_t ullPeerSessionId = 0;
    ullPeerSessionId = m_pPeerSession->getStreamId();

    if (RET_OK != handleRtspMessage(*m_pLastRtspMsg))
    {
        delete m_pLastRtspMsg;
        m_pLastRtspMsg = NULL;
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle last rtsp message fail, peer session id[%Q].",
                         m_unSessionIndex, ullPeerSessionId));
        return RET_FAIL;
    }

    delete m_pLastRtspMsg;
    m_pLastRtspMsg = NULL;

    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] handle svs message success, peer session id[%Q].",
                    m_unSessionIndex, ullPeerSessionId));
    return RET_OK;
}


int32_t CMduRtspPushSession::handle_input(ACE_HANDLE handle)
{
    if ((NULL == m_pRecvBuffer) || (ACE_INVALID_HANDLE == m_sockHandle))
    {
        return -1;
    }

    int32_t iRecvLen = (int32_t) m_pRecvBuffer->size() - (int32_t) m_pRecvBuffer->length();
    if (iRecvLen <= 0)
    {
        SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] recv buffer is full, size[%u] length[%u].",
                m_unSessionIndex,
                m_pRecvBuffer->size(),
                m_pRecvBuffer->length()));
        return 0;
    }

    ACE_OS::last_error(0);
    iRecvLen = ACE::recv(m_sockHandle, m_pRecvBuffer->wr_ptr(), (size_t) iRecvLen);
    if (iRecvLen <= 0)
    {
        int32_t iErrorCode = ACE_OS::last_error();
        if (!(EAGAIN == iErrorCode
                || ETIME == iErrorCode
                || EWOULDBLOCK == iErrorCode
                || ETIMEDOUT == iErrorCode
                || EINTR == iErrorCode))
        {
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] recv data fail, "
                    "close handle[%d]. errno[%d].",
                    m_unSessionIndex,
                    m_sockHandle,
                    iErrorCode));

            return -1;
        }

        SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] recv data fail, wait retry. errno[%d].",
                m_unSessionIndex,
                iErrorCode));
        return 0;
    }

    m_pRecvBuffer->wr_ptr((size_t)(m_pRecvBuffer->length() + (size_t) iRecvLen));
    m_pRecvBuffer->rd_ptr((size_t) 0);

    size_t processedSize = 0;
    size_t totalSize = m_pRecvBuffer->length();
    int32_t nSize = 0;
    do
    {
        nSize = processRecvedMessage(m_pRecvBuffer->rd_ptr() + processedSize,
                                     m_pRecvBuffer->length() - processedSize);
        if (nSize < 0)
        {
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] process recv data fail, close handle[%d]. ",
                    m_unSessionIndex,
                    m_sockHandle));
            return -1;
        }

        if (0 == nSize)
        {
            break;
        }

        processedSize += (size_t) nSize;
    }
    while (processedSize < totalSize);

    size_t dataSize = m_pRecvBuffer->length() - processedSize;
    (void) m_pRecvBuffer->copy(m_pRecvBuffer->rd_ptr() + processedSize, dataSize);
    m_pRecvBuffer->rd_ptr((size_t) 0);
    m_pRecvBuffer->wr_ptr(dataSize);

    return 0;
}

int32_t CMduRtspPushSession::handle_output (ACE_HANDLE handle)
{
    /*if (NULL != m_pRtpSession)
    {
        return m_pRtpSession->sendMessage(pData, unDataSize);
    }*/
    return RET_OK;
}


int32_t CMduRtspPushSession::handle_timeout(const ACE_Time_Value &tv, const void *arg)
{
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] retry process rtsp message.", m_unSessionIndex));

    if (!m_pLastRtspMsg)
    {
        clearRtspCachedMessage();
        return 0;
    }

    (void)handleRtspMessage(*m_pLastRtspMsg);

    return 0;
}

int32_t CMduRtspPushSession::handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask /*close_mask*/)
{
    if (ACE_INVALID_HANDLE == m_sockHandle)
    {
        return 0;
    }

    ACE_Reactor *pReactor = ACE_Reactor::instance();
    if (!pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"handle close rtsp push session[%u] fail, can't find reactor instance.",
                        m_unSessionIndex));
        return -1;
    }

    int32_t nRet = pReactor->remove_handler(m_sockHandle, ACE_Event_Handler::READ_MASK
                                                        | ACE_Event_Handler::DONT_CALL);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"handle close rtsp push session[%u] fail, remove read mask fail[%d].",
                         m_unSessionIndex, ACE_OS::last_error()));
        return -1;
    }

    (void)ACE_OS::close(m_sockHandle);
    m_sockHandle = ACE_INVALID_HANDLE;

    clearRtspCachedMessage();

    destroyMediaSession();

    setStatus(RTSP_SESSION_STATUS_TEARDOWN);
    SVS_LOG((SVS_LM_INFO,"handle close rtsp push session[%u] success.", m_unSessionIndex));

    return 0;
}

void CMduRtspPushSession::setHandle(ACE_HANDLE handle, const ACE_INET_Addr &localAddr)
{
    m_sockHandle = handle;
    m_LocalAddr  = localAddr;

    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] set handle[%d], local addr[%s:%d].",
                    m_unSessionIndex, handle,
                    m_LocalAddr.get_host_addr(), m_LocalAddr.get_port_number()));
    return;
}

ACE_HANDLE CMduRtspPushSession::get_handle() const
{
    return m_sockHandle;
}

uint32_t CMduRtspPushSession::getSessionIndex() const
{
    return m_unSessionIndex;
}

int32_t CMduRtspPushSession::check()
{
    uint32_t ulCostTime = SVS_GetSecondTime() - m_ulStatusTime;
    if ((RTSP_SESSION_STATUS_SETUP >= getStatus())
            && (ulCostTime > MDU_STATUS_TIMEOUT_INTERVAL))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] check status abnormal,"
                " close session.",
                m_unSessionIndex));
        close();
        return RET_OK;
    }

    if (m_pRtpSession)
    {
        if ((MDU_SESSION_STATUS_ABNORMAL == m_pRtpSession->getStatus())
                || (MDU_SESSION_STATUS_RELEASED == m_pRtpSession->getStatus()))
        {
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] check status abnormal,"
                    " close rtp session[%Q].",
                    m_unSessionIndex, m_pRtpSession->getStreamId()));
            close();
            return RET_OK;
        }
    }

    if ((RTSP_SESSION_STATUS_TEARDOWN == getStatus())
            && (ulCostTime > MDU_STATUS_ABNORMAL_INTERVAL))
    {
        SVS_LOG((SVS_LM_INFO,"check rtsp push session[%u] teardown, release session.",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CMduRtspPushSession::setSockOpt()
{
    if (ACE_INVALID_HANDLE == m_sockHandle)
    {
        return RET_FAIL;
    }

    if (ACE_OS::fcntl(m_sockHandle, F_SETFL, ACE_OS::fcntl(m_sockHandle, F_GETFL) | O_NONBLOCK))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] set O_NONBLOCK fail, errno[%d].",
                        m_unSessionIndex,
                        errno));
        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CMduRtspPushSession::sendMessage(const char* pData, uint32_t unDataSize)
{
    if (NULL != m_pRtpSession)
    {
        return m_pRtpSession->sendMessage(pData, unDataSize);
    }

    ACE_Time_Value timeout(1);
    int32_t nSendSize = ACE::send_n(m_sockHandle, pData, unDataSize, &timeout);
    if (unDataSize != (uint32_t)nSendSize)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] send message fail, close handle[%d].",
                        m_unSessionIndex, m_sockHandle));
        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CMduRtspPushSession::sendMediaSetupReq(CSVSMediaLink* linkInof)
{
    std::string   strSdpInfo;
    std::string   strUrl = linkInof->Url();

    m_enPlayType    = linkInof->PlayType();

    /* allocate the local session first */
    int32_t nRet = createDistribute(linkInof,strSdpInfo);
    if(RET_OK != nRet) {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] set the media sdp info.rtspUrl:\n %s",
                    m_unSessionIndex, strUrl.c_str()));
        return RET_FAIL;
    }

    uint32_t unMsgLen = sizeof(SVS_MSG_MDU_SESSION_SETUP_REQ) ;

    CMduSvsMessage *pMessage = NULL;
    nRet = CMduMsgFactory::instance()->createSvsMsg(SVS_MSG_TYPE_MDU_SESSION_SETUP_REQ,
                                                    unMsgLen, 0,pMessage);
    if ((RET_OK != nRet) || (NULL == pMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] create mdu session setup request fail.",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    CMduMediaSetupReq *pReq = dynamic_cast<CMduMediaSetupReq*>(pMessage);
    if (!pReq)
    {
        return RET_FAIL;
    }

    std::string strAddr  = m_pPeerSession->getVideoAddr().get_host_addr();
    uint16_t usPort      = m_pPeerSession->getVideoAddr().get_port_number();
    uint32_t ulTransType = m_pPeerSession->getMediaTransType();

    nRet = pReq->initMsgBody(m_unSessionIndex,linkInof->ContentID().c_str(),
                             linkInof->UrlType(),linkInof->PlayType(),
                             strUrl.c_str(),strSdpInfo.c_str(),
                             ulTransType,strAddr.c_str(),usPort);
    if (RET_OK != nRet)
    {
        CMduMsgFactory::instance()->destroySvsMsg(pMessage);
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] init mdu session setup request fail.",
                                m_unSessionIndex));
        return RET_FAIL;
    }

    if (RET_OK != pReq->handleMessage())
    {
        CMduMsgFactory::instance()->destroySvsMsg(pMessage);
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle mdu session setup request fail.",
                m_unSessionIndex));
        return RET_FAIL;
    }

    m_bSetUp = true;

    CMduMsgFactory::instance()->destroySvsMsg(pMessage);
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] send mdu session setup request success.",
            m_unSessionIndex));
    return RET_OK;
}
void CMduRtspPushSession::sendMediaPlayReq()
{
    uint32_t unMsgLen = sizeof(SVS_MSG_MDU_SESSION_PLAY_REQ);

    CMduSvsMessage *pMessage = NULL;
    int32_t nRet = CMduMsgFactory::instance()->createSvsMsg(SVS_MSG_TYPE_MDU_SESSION_PLAY_REQ,
                                                    unMsgLen, 0,pMessage);
    if ((RET_OK != nRet) || (NULL == pMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] create play request fail.",
                        m_unSessionIndex));
        return ;
    }

    CMduMediaPlayReq *pReq = dynamic_cast<CMduMediaPlayReq*>(pMessage);
    if (!pReq)
    {
        return ;
    }

    nRet = pReq->initMsgBody((uint8_t*)m_strContentID.c_str());

    if (RET_OK != nRet)
    {
        CMduMsgFactory::instance()->destroySvsMsg(pMessage);
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] init mdu play request fail.",
                                m_unSessionIndex));
        return ;
    }

    if (RET_OK != pReq->handleMessage())
    {
        CMduMsgFactory::instance()->destroySvsMsg(pMessage);
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle mdu play request fail.",
                m_unSessionIndex));
        return ;
    }

    CMduMsgFactory::instance()->destroySvsMsg(pMessage);
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] send mdu play request success.",
            m_unSessionIndex));
    return;
}

void CMduRtspPushSession::sendKeyFrameReq()
{
    uint32_t unMsgLen = sizeof(SVS_MSG_MDU_KEY_FRAME_REQ);

    CMduSvsMessage *pMessage = NULL;
    int32_t nRet = CMduMsgFactory::instance()->createSvsMsg(SVS_MSG_TYPE_MEDIA_KEYFRAME_REQ,
                                                    unMsgLen, 0,pMessage);
    if ((RET_OK != nRet) || (NULL == pMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] create mdu key frame request fail.",
                        m_unSessionIndex));
        return ;
    }

    CMduMediaKeyFrameReq *pReq = dynamic_cast<CMduMediaKeyFrameReq*>(pMessage);
    if (!pReq)
    {
        return ;
    }

    nRet = pReq->initMsgBody((uint8_t*)m_strContentID.c_str());

    if (RET_OK != nRet)
    {
        CMduMsgFactory::instance()->destroySvsMsg(pMessage);
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] init mdu key frame request fail.",
                                m_unSessionIndex));
        return ;
    }

    if (RET_OK != pReq->handleMessage())
    {
        CMduMsgFactory::instance()->destroySvsMsg(pMessage);
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle mdu key frame request fail.",
                m_unSessionIndex));
        return ;
    }

    CMduMsgFactory::instance()->destroySvsMsg(pMessage);
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] send mdu key frame request success.",
            m_unSessionIndex));
    return;
}

int32_t CMduRtspPushSession::createMediaSession()
{
    CMduSession *pSession = CMduSessionFactory::instance()->createSession(PEER_TYPE_CU, RTSP_SESSION,true);
    if (!pSession)
    {
        SVS_LOG((SVS_LM_ERROR,"rtsp push session[%u] create media session fail.", m_unSessionIndex));
        return RET_FAIL;
    }

    CMduStdRtpSession* pStdSession = dynamic_cast<CMduStdRtpSession*>(pSession);
    if (!pStdSession)
    {
        CMduSessionFactory::instance()->releaseSession(pSession);
        SVS_LOG((SVS_LM_ERROR,"rtsp push session[%u] create std media session fail.", m_unSessionIndex));
        return RET_FAIL;
    }

    if (!m_pPeerSession)
    {
        CMduSessionFactory::instance()->releaseSession(pSession);
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle rtsp play request fail, "
                "can't find peer session ContentID[%s]",
                m_unSessionIndex, m_strContentID.c_str()));
        return RET_FAIL;
    }

    /* the peer session release by the destory the media session */
    uint64_t ullPeerSessionId = 0;
    ullPeerSessionId = m_pPeerSession->getStreamId();

    pStdSession->setRtspHandle(m_sockHandle, m_PeerAddr);
    uint8_t VideoPayloadType = PT_TYPE_H264;
    uint8_t AudioPayloadType = PT_TYPE_PCMU;
    MEDIA_INFO_LIST VideoinfoList;
    MEDIA_INFO_LIST AudioinfoList;
    m_RtspSdp.getVideoInfo(VideoinfoList);
    m_RtspSdp.getAudioInfo(AudioinfoList);

    if(0 <VideoinfoList.size()) {
        VideoPayloadType = VideoinfoList.front().ucPayloadType;
    }
    if(0 <AudioinfoList.size()) {
        AudioPayloadType = AudioinfoList.front().ucPayloadType;
    }

    pStdSession->setPlayLoad(VideoPayloadType,AudioPayloadType);
    if (RET_OK != pStdSession->initStdRtpSession(ullPeerSessionId,
                                                 m_enPlayType,
                                                 m_LocalAddr,
                                                 m_PeerAddr))
    {
        CMduSessionFactory::instance()->releaseSession(pSession);
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] init rtp session fail.", m_unSessionIndex));
        return RET_FAIL;
    }

    m_pRtpSession = pStdSession;
    if(NULL != m_pRtpSession)
    {
        m_pRtpSession->setSdpInfo(m_RtspSdp);
        m_pRtpSession->setContentID(m_pPeerSession->getContentID());
    }


    CMduBusiness *pBusiness = CMduBusinessManager::instance()->createBusiness(ullPeerSessionId,
                                                                        pStdSession->getStreamId(),
                                                                        pStdSession->getPlayType());
    if (!pBusiness)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] create business fail, pu[%Q] cu[%Q].",
                        m_unSessionIndex, pSession->getStreamId(), ullPeerSessionId));

        CMduSessionFactory::instance()->releaseSession(pSession);
        m_pRtpSession = NULL;
        return RET_FAIL;
    }

    if (RET_OK != pBusiness->start())
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] start business fail, pu[%Q] cu[%Q].",
                m_unSessionIndex, pSession->getStreamId(), ullPeerSessionId));

        CMduSessionFactory::instance()->releaseSession(pSession);

        CMduBusinessManager::instance()->releaseBusiness(pBusiness);
        m_pRtpSession = NULL;
        return RET_FAIL;
    }


    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] create session success, cu[%Q] pu[%Q].",
                    m_unSessionIndex, pSession->getStreamId(), ullPeerSessionId));
    return RET_OK;
}

void CMduRtspPushSession::destroyMediaSession()
{
    SVS_LOG((SVS_LM_DEBUG,"rtsp push session destory media session."));
    uint64_t ullRtpSessionId  = 0;
    uint64_t ullPeerSessionId = 0;
    if (m_pRtpSession)
    {
        ullRtpSessionId = m_pRtpSession->getStreamId();
        CMduBusiness *pBusiness = CMduBusinessManager::instance()->findBusiness(ullRtpSessionId);
        if (!pBusiness)
        {
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] not find the buinsess.", m_unSessionIndex));
            return;
        }

        CMduBusinessManager::instance()->releaseBusiness(pBusiness);
        CMduBusinessManager::instance()->releaseBusiness(pBusiness);

        CMduSessionFactory::instance()->releaseSession(ullRtpSessionId);
        m_pRtpSession = NULL;
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] media session already released.", m_unSessionIndex));
    }

    if(NULL != m_pPeerSession)
    {
        ullPeerSessionId = m_pPeerSession->getStreamId();
        SVS_LOG((SVS_LM_DEBUG,"rtsp push session:[%Q], release PeerSession:[%Q].",
                                                          ullRtpSessionId,ullPeerSessionId));
        CMduSessionFactory::instance()->releaseSession(ullPeerSessionId);
        m_pPeerSession = NULL;
    }


    //try to find the peer session ,if the session is not exit,so call stop source
    CMduSession* pPeerSession = CMduSessionFactory::instance()->findSession(ullPeerSessionId);
    if(NULL != pPeerSession)
    {
        CMduSessionFactory::instance()->releaseSession(ullPeerSessionId);
        return;
    }

    CMduSvsMessage *pReq = NULL;
    int32_t nRet = CMduMsgFactory::instance()->createSvsMsg(SVS_MSG_TYPE_MDU_SESSION_TEARDOWN_REQ,
                                                        sizeof(SVS_MSG_MDU_SESSION_TEARDOWN_REQ),
                                                        0,
                                                        pReq);
    if ((RET_OK != nRet) || (NULL == pReq))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp session[%u] create tear down fail, create msg fail.",
                        m_unSessionIndex));
        CMduMsgFactory::instance()->destroySvsMsg(pReq);
        return;
    }

    CMduMediaTearDownReq* pTearDownRequest = (CMduMediaTearDownReq*) (void*) pReq;
    if (RET_OK != pTearDownRequest->initMsgBody(m_unSessionIndex,m_strContentID.c_str()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp session[%u] init tear down fail, create msg fail.",
                          m_unSessionIndex));
        CMduMsgFactory::instance()->destroySvsMsg(pReq);
        return;
    }

    if (RET_OK != pReq->handleMessage())
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp session[%u] handle tear down fail, create msg fail.",
                                m_unSessionIndex));
    }
    else
    {
        SVS_LOG((SVS_LM_INFO,"rtsp session[%u] handle tear down success.", m_unSessionIndex));
    }

    CMduMsgFactory::instance()->destroySvsMsg(pReq);

    SVS_LOG((SVS_LM_INFO,"rtsp session[%u] destroy media session success.", m_unSessionIndex));
    return;
}


int32_t CMduRtspPushSession::processRecvedMessage(const char* pData, uint32_t unDataSize)
{
    if ((NULL == pData) || (0 == unDataSize))
    {
        return -1;
    }

    if (RTSP_INTERLEAVE_FLAG == pData[0])
    {
        return handleRTPRTCPData(pData, unDataSize);
    }

    int32_t nMessageLen = m_RtspProtocol.IsParsable(pData, unDataSize);
    if (0 > nMessageLen)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] parse rtsp message fail.", m_unSessionIndex));
        return -1;
    }

    if (0 == nMessageLen)
    {
        return 0;
    }

    CRtspMessage *pMessage = NULL;
    int32_t nRet = m_RtspProtocol.DecodeRtspMessage(pData, (uint32_t)nMessageLen, pMessage);
    if ((RET_OK != nRet) || (!pMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] decode rtsp message fail.", m_unSessionIndex));
        return nMessageLen;
    }

    if (RET_OK != handleRtspMessage(*pMessage))
    {
        delete pMessage;
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle rtsp message fail.", m_unSessionIndex));
        return nMessageLen;
    }

    delete pMessage;
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] success to process rtsp message.", m_unSessionIndex));
    return nMessageLen;
}

int32_t CMduRtspPushSession::handleRTPRTCPData(const char* pData, uint32_t unDataSize) const
{
    if (unDataSize < RTSP_INTERLEAVE_HEADER_LEN)
    {
        return 0;
    }
    uint32_t unMediaSize = (uint32_t) ACE_NTOHS(*(uint16_t*)(void*)&pData[2]);
    if (unDataSize - RTSP_INTERLEAVE_HEADER_LEN < unMediaSize)
    {
        return 0;
    }

    if (m_pRtpSession)
    {
        if (!m_pPeerSession)
        {
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle rtcp message fail, "
                    "can't find peer session.",m_unSessionIndex));

            return (int32_t)(unMediaSize + RTSP_INTERLEAVE_HEADER_LEN);
        }
        if(TRANS_PROTOCAL_TCP == m_unTransType)
        {
            if ((m_cVideoInterleaveNum == pData[1])
            || (m_cAudioInterleaveNum == pData[1]))
            {
                handleMediaData((const char*)(pData+RTSP_INTERLEAVE_HEADER_LEN),unMediaSize);
            }
        }

        MDU_INNER_MSG innerMsg;
        fillMduInnerMsg((char*)&innerMsg,
                        m_pRtpSession->getStreamId(),
                        NULL,
                        m_PeerAddr.get_ip_address(),
                        m_PeerAddr.get_port_number(),
                        INNER_MSG_RTCP,
                        0);
        (void)m_pRtpSession->handleInnerMessage(innerMsg, sizeof(innerMsg), *m_pPeerSession);
    }

    return (int32_t)(unMediaSize + RTSP_INTERLEAVE_HEADER_LEN);
}

void CMduRtspPushSession::handleMediaData(const char* pData, uint32_t unDataSize) const
{
    uint64_t ullRtpSessionId = 0;

    if(NULL == m_pRtpSession)
    {
        SVS_LOG((SVS_LM_WARNING,"RtspPushSession,the rtp session is null."));
        return;
    }
    ullRtpSessionId = m_pRtpSession->getStreamId();

    ACE_Message_Block *pMsg = CMediaBlockBuffer::instance().allocMediaBlock();
    if (NULL == pMsg)
    {
        SVS_LOG((SVS_LM_WARNING,"RtspPushSession alloc media block fail."));
        return ;
    }



    MDU_TRANSMIT_PACKET *pPacket = (MDU_TRANSMIT_PACKET *) (void*) pMsg->base();
    pMsg->wr_ptr(sizeof(MDU_TRANSMIT_PACKET) - 1); //

    CRtpPacket rtpPacket;
    (void)rtpPacket.ParsePacket(pData ,unDataSize);

    pMsg->copy(pData, unDataSize);

    pPacket->PuStreamId = ullRtpSessionId;
    pPacket->enPacketType = MDU_PACKET_TYPE_MEDIA_DATA;

    int32_t nRet = RET_OK;
    nRet = CMduMediaExchange::instance()->addData(pMsg);


    if (RET_OK != nRet)
    {
        CMediaBlockBuffer::instance().freeMediaBlock(pMsg);

        return;
    }
    return;
}

int32_t CMduRtspPushSession::handleRtspMessage(CRtspMessage &rtspMessage)
{
    if (RTSP_MSG_REQ != rtspMessage.getMsgType())
    {
        if (RTSP_METHOD_ANNOUNCE != rtspMessage.getMethodType())
        {
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle not accepted method[%u].",
                              m_unSessionIndex, rtspMessage.getMethodType()));
            return RET_FAIL;
        }
    }

    int32_t nRet = RET_OK;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_RtspMutex);

    switch(rtspMessage.getMethodType())
    {
    case RTSP_METHOD_OPTIONS:
        nRet = handleRtspOptionsReq(rtspMessage);
        break;
    case RTSP_METHOD_DESCRIBE:
        nRet = handleRtspDescribeReq(rtspMessage);
        break;
    case RTSP_METHOD_SETUP:
        nRet = handleRtspSetupReq(rtspMessage);
        break;
    case RTSP_METHOD_PLAY:
        nRet = handleRtspPlayReq(rtspMessage);
        break;
    case RTSP_METHOD_PAUSE:
        nRet = handleRtspPauseReq(rtspMessage);
        break;
    case RTSP_METHOD_TEARDOWN:
        nRet = handleRtspTeardownReq(rtspMessage);
        break;
    case RTSP_METHOD_ANNOUNCE:
        nRet = handleRtspAnnounceReq(rtspMessage);
        break;
    case RTSP_METHOD_RECORD:
        nRet = handleRtspRecordReq(rtspMessage);
        break;
    case RTSP_METHOD_GETPARAMETER:
        nRet = handleRtspGetParameterReq(rtspMessage);
        break;
    default:
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle not accepted method[%u].",
                        m_unSessionIndex, rtspMessage.getMethodType()));
        return RET_FAIL;
    }
    return nRet;
}

int32_t CMduRtspPushSession::handleRtspOptionsReq(CRtspMessage &rtspMessage)
{
    CRtspOptionsMessage *pRequest = dynamic_cast<CRtspOptionsMessage*>(&rtspMessage);
    if (NULL == pRequest)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle options request fail.", m_unSessionIndex));
        return RET_FAIL;
    }


    pRequest->setRange(m_strPlayRange);
    pRequest->setStatusCode(RTSP_SUCCESS_OK);
    pRequest->setMsgType(RTSP_MSG_RSP);
    std::stringstream sessionIdex;
    sessionIdex << m_unSessionIndex;
    pRequest->setSession(sessionIdex.str());

    std::string strResp;
    if (RET_OK != pRequest->encodeMessage(strResp))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] encode options response fail.", m_unSessionIndex));
        return RET_FAIL;
    }

    if (RET_OK != sendMessage(strResp.c_str(), strResp.length()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] send options response fail.", m_unSessionIndex));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] send options response success.", m_unSessionIndex));

    simulateSendRtcpMsg();

    return RET_OK;
}

uint32_t CMduRtspPushSession::getRange(const string strUrl,string & strStartTime,string & strStopTime)
{
    string::size_type nStart = 0;
    string::size_type nStop  = 0;
    nStart = strUrl.find("-");
    string tmp = string(strUrl);

    for (int32_t i=0;i<3;i++)
    {
        nStart = tmp.find("-");
        tmp = tmp.substr(nStart+1);
    }

    nStop  = tmp.find("&user");

    tmp = tmp.substr(0,nStop);


    nStop  = tmp.find("-");

    strStartTime = "";
    strStopTime  = "";

    strStartTime = tmp.substr(0,nStop);
    strStopTime  = tmp.substr(nStop+1);

    std::stringstream strStart;
    std::stringstream strEnd;

    struct tm rangeTm;
    memset(&rangeTm, 0x0, sizeof(rangeTm));

    char* pRet = strptime(strStartTime.c_str(), "%Y%m%d%H%M%S", &rangeTm);
    if (NULL == pRet)
    {
        return 0;
    }

    uint32_t iStartTime = 0;
    iStartTime =  (uint32_t) mktime(&rangeTm);
    strStart<<iStartTime;
    strStart>>strStartTime;

    memset(&rangeTm, 0x0, sizeof(rangeTm));

    pRet = strptime(strStopTime.c_str(), "%Y%m%d%H%M%S", &rangeTm);
    if (NULL == pRet)
    {
        return 0;
    }

    uint32_t iStopTime = 0;
    iStopTime =  (uint32_t) mktime(&rangeTm);
    strEnd<<iStopTime;
    strEnd>>strStopTime;


    return iStopTime-iStartTime;
}


int32_t CMduRtspPushSession::handleRtspDescribeReq(const CRtspMessage &rtspMessage)
{
    CSVSMediaLink MediaLink;
    if (RTSP_SESSION_STATUS_INIT != getStatus())
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle describe req fail, status[%u] invalid.",
                m_unSessionIndex, getStatus()));
        return RET_FAIL;
    }

    int32_t nRet = CSVSMediaLinkFactory::instance().parseMediaUrl(rtspMessage.getRtspUrl(),&MediaLink);
    if((SVS_MEDIA_LINK_RESULT_SUCCESS != nRet)
        &&(SVS_MEDIA_LINK_RESULT_AUTH_FAIL != nRet))
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle describe req fail, content invalid."));
        return RET_FAIL;
    }
    if(SVS_MEDIA_LINK_RESULT_AUTH_FAIL == nRet)
    {
        if(CMduConfig::instance()->getUrlEffectiveWhile())
        {
            close();
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle describe req fail, auth invalid.",m_unSessionIndex));
            return RET_FAIL;
        }
    }

    m_strContentID = MediaLink.ContentID();

    if(NULL == m_pPeerSession)
    {
        m_pPeerSession = CMduSessionFactory::instance()->findSession(m_strContentID);
    }

    if (NULL == m_pPeerSession)
    {
        CRtspDescribeMessage *pReq = new CRtspDescribeMessage();
        if (!pReq)  //lint !e774
        {
            return RET_FAIL;
        }
        pReq->setRtspUrl(rtspMessage.getRtspUrl());
        pReq->setCSeq(rtspMessage.getCSeq());
        std::stringstream sessionIdex;
        sessionIdex << m_unSessionIndex;
        pReq->setSession(sessionIdex.str());

        if (!m_pLastRtspMsg)
        {
            delete m_pLastRtspMsg;
        }
        m_pLastRtspMsg = pReq;
        SVS_LOG((SVS_LM_INFO,"rtsp session[%u] save describe request[%p].",
                        m_unSessionIndex, m_pLastRtspMsg));

        if (RET_OK != sendMediaSetupReq(&MediaLink))
        {
            delete m_pLastRtspMsg;
            m_pLastRtspMsg = NULL;

            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle describe request fail, "
                    "send setup request fail.",
                    m_unSessionIndex));
            return RET_FAIL;
        }

        return RET_OK;
    }


    std::string strSdp;
    SDP_MEDIA_INFO  videoInfo, audioInfo;
    if (RTSP_SESSION != m_pPeerSession->getSessionType())
    {
        std::string  SdpInfo;
        CMediaSdp    peerSdp;
        MEDIA_INFO_LIST vInofList;
        MEDIA_INFO_LIST aInofList;
        m_pPeerSession->getSdpInfo(SdpInfo);

        if(RET_OK != peerSdp.decodeSdp(SdpInfo)) {
            SVS_LOG((SVS_LM_WARNING,"parser the sdp info:[%s] fail.",SdpInfo.c_str()));
            return RET_FAIL;
        }

        /* video */
        peerSdp.getVideoInfo(vInofList);
        if(1 != vInofList.size())
        {
            SVS_LOG((SVS_LM_WARNING,"the peer the sdp info:[%s] video info is wrong.",SdpInfo.c_str()));
            return RET_FAIL;
        }
        /* audio */
        peerSdp.getAudioInfo(aInofList);
        if(1 < aInofList.size())
        {
            SVS_LOG((SVS_LM_WARNING,"the peer the sdp info:[%s] audio info is more than one.",SdpInfo.c_str()));
            return RET_FAIL;
        }

        SDP_MEDIA_INFO* Videoinfo = (SDP_MEDIA_INFO*)&vInofList.front();
        SDP_MEDIA_INFO stVideoInfo;
        SDP_MEDIA_INFO stAideoInfo;

        stVideoInfo.strControl   = "";
        stVideoInfo.strFmtp      = "";
        stVideoInfo.strRtpmap    = "";
        stVideoInfo.ucPayloadType= PT_TYPE_H264;
        stVideoInfo.usPort       = 0;

        stAideoInfo.strControl   = "";
        stAideoInfo.strFmtp      = "";
        stAideoInfo.strRtpmap    = "";
        stAideoInfo.ucPayloadType= PT_TYPE_PCMU;
        stAideoInfo.usPort       = 0;

        if(Videoinfo->strRtpmap == PS_VIDEO_RTPMAP) {
            stVideoInfo.ucPayloadType = PT_TYPE_H264; /* PS -->H264 */
            m_RtspSdp.addVideoInfo(stVideoInfo);
            //stVideoInfo.ucPayloadType = PT_TYPE_H265; /* PS -->H265 */
            //m_RtspSdp.addVideoInfo(stVideoInfo);
            if(0 < aInofList.size())
            {
                stAideoInfo.ucPayloadType= PT_TYPE_PCMU;
                m_RtspSdp.addAudioInfo(stAideoInfo);
                stAideoInfo.ucPayloadType= PT_TYPE_PCMA;
                m_RtspSdp.addAudioInfo(stAideoInfo);
            }
        }
        else {
            m_RtspSdp.addVideoInfo(*Videoinfo);
            if(0 < aInofList.size())
            {
                SDP_MEDIA_INFO* Audioinfo = (SDP_MEDIA_INFO*)&aInofList.front();
                m_RtspSdp.addAudioInfo(*Audioinfo);
            }
        }
    }
    else
    {
        CMduStdRtpSession *pRtpSession = dynamic_cast<CMduStdRtpSession*>(m_pPeerSession);
        if (pRtpSession)
        {
            pRtpSession->getSdpInfo(m_RtspSdp);
        }
    }

    int32_t isplayback = 0;
    std::string strtimeRange="";
    if ( PLAY_TYPE_PLAT_RECORD == m_enPlayType)
    {
        isplayback = 1;
        if ("" == m_RtspSdp.getRange())
        {
            std::string strtmp = "";
            std::string strtmpUrl = m_RtspSdp.getUrl();
            std::string strStartTime = "";
            std::string strEndTime = "";
            strtmp += "range:npt=0-";
            uint32_t num = 0;
            num = getRange(strtmpUrl,strStartTime,strEndTime);

        if( 0 == num)
        {
            SVS_LOG((SVS_LM_WARNING,"get timeRange fail,range in url is 0."));
            return RET_FAIL;
        }

         std::stringstream stream;
         stream<<num;
         stream>>strtimeRange;
         strtmp += strtimeRange;
         SVS_LOG((SVS_LM_WARNING,"time is = [%s]",strtmp.c_str()));
         m_RtspSdp.setRange(strtmp);
        }
    }

    if (RET_OK != m_RtspSdp.encodeSdp(strSdp,isplayback,strtimeRange))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] encode sdp info fail.", m_unSessionIndex));
        return RET_FAIL;
    }


    CRtspDescribeMessage resp;
    resp.setMsgType(RTSP_MSG_RSP);
    resp.setCSeq(rtspMessage.getCSeq());
    resp.setStatusCode(RTSP_SUCCESS_OK);

    std::stringstream sessionIdex;
    sessionIdex << m_unSessionIndex;
    resp.setSession(sessionIdex.str());
    resp.setSdp(strSdp);

    std::string strResp;
    if (RET_OK != resp.encodeMessage(strResp))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] encode describe response fail.", m_unSessionIndex));
        return RET_FAIL;
    }

    if (RET_OK != sendMessage(strResp.c_str(), strResp.length()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] send describe response fail.", m_unSessionIndex));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] handle describe request success.", m_unSessionIndex));
    return RET_OK;
}

int32_t CMduRtspPushSession::handleRtspSetupReq(CRtspMessage &rtspMessage)
{
    std::string  strContentID;
    CSVSMediaLink MediaLink;

    if (RTSP_SESSION_STATUS_SETUP < getStatus())
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle setup req fail, status[%u] invalid.",
                m_unSessionIndex, getStatus()));
        return RET_FAIL;
    }

    CRtspSetupMessage *pReq = dynamic_cast<CRtspSetupMessage*>(&rtspMessage);
    if (!pReq)  //lint !e774
    {
        return RET_FAIL;
    }

    int32_t nRet = CSVSMediaLinkFactory::instance().parseMediaUrl(rtspMessage.getRtspUrl(),&MediaLink);
    if((SVS_MEDIA_LINK_RESULT_SUCCESS != nRet)
        &&(SVS_MEDIA_LINK_RESULT_AUTH_FAIL != nRet))
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle setup req fail, content invalid."));
        return RET_FAIL;
    }
    if((!m_bSetUp)&&(SVS_MEDIA_LINK_RESULT_AUTH_FAIL == nRet))
    {
        if(CMduConfig::instance()->getUrlEffectiveWhile())
        {
            close();
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle setup req fail, auth invalid.",m_unSessionIndex));
            return RET_FAIL;
        }
    }
    strContentID = MediaLink.ContentID();

    if(NULL == m_pPeerSession)
    {
        m_pPeerSession = CMduSessionFactory::instance()->findSession(m_strContentID);
    }

    if (NULL == m_pPeerSession)
    {
        CRtspSetupMessage *pSetupReq = new CRtspSetupMessage();
        if (!pSetupReq)  //lint !e774
        {
            return RET_FAIL;
        }
        pSetupReq->setRtspUrl(rtspMessage.getRtspUrl());
        pSetupReq->setCSeq(rtspMessage.getCSeq());
        //pSetupReq->setSession(rtspMessage.getSession());
        std::stringstream sessionIdex;
        sessionIdex << m_unSessionIndex;
        pSetupReq->setSession(sessionIdex.str());
        pSetupReq->setTransType(pReq->getTransType());
        pSetupReq->setInterleaveNum(pReq->getInterleaveNum());
        pSetupReq->setClientPort(pReq->getClientPort());
        pSetupReq->setDestinationIp(pReq->getDestinationIp());

        if (!m_pLastRtspMsg)
        {
            delete m_pLastRtspMsg;
        }
        m_pLastRtspMsg = pSetupReq;

        if (RET_OK != sendMediaSetupReq(&MediaLink))
        {
            delete m_pLastRtspMsg;
            m_pLastRtspMsg = NULL;

            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle describe request fail, "
                    "send setup request fail.",
                    m_unSessionIndex));
            return RET_FAIL;
        }

        SVS_LOG((SVS_LM_INFO,"rtsp session[%u] save setup request[%p], send media setup request to SCC.",
                        m_unSessionIndex, m_pLastRtspMsg));
        return RET_OK;
    }


    if (!m_pRtpSession)
    {
        if (RET_OK != createMediaSession())
        {
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle setup request fail, "
                    "create media session fail.",
                    m_unSessionIndex));
            return RET_FAIL;
        }

        SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] create media session success.",
                        m_unSessionIndex));
    }

    pReq->setDestinationIp(m_PeerAddr.get_ip_address());

    std::stringstream sessionIdex;
    sessionIdex << m_unSessionIndex;
    pReq->setSession(sessionIdex.str());
    if (RET_OK != m_pRtpSession->startStdRtpSession(*pReq))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] start media session fail.",
                                m_unSessionIndex));
        return RET_FAIL;
    }

    ACE_INET_Addr addr;
    pReq->setMsgType(RTSP_MSG_RSP);
    pReq->setStatusCode(RTSP_SUCCESS_OK);
    m_unTransType = pReq->getTransType();
    if (TRANS_PROTOCAL_UDP == pReq->getTransType())
    {
        if (m_bFirstSetupFlag)
        {
            m_cVideoInterleaveNum = pReq->getInterleaveNum();
            addr.set(m_pRtpSession->getVideoAddr());
        }
        else
        {
            m_cAudioInterleaveNum = pReq->getInterleaveNum();
            addr.set(m_pRtpSession->getAudioAddr());
        }
        pReq->setServerPort(addr.get_port_number());
        pReq->setSourceIp(addr.get_ip_address());

        m_bFirstSetupFlag = false;
    }
    else
    {
        if (m_bFirstSetupFlag)
        {
            m_cVideoInterleaveNum = pReq->getInterleaveNum();
        }
        else
        {
            m_cAudioInterleaveNum = pReq->getInterleaveNum();
        }
        m_bFirstSetupFlag = false;
    }

    std::string strResp;
    if (RET_OK != pReq->encodeMessage(strResp))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] encode setup response fail.",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    if (RET_OK != sendMessage(strResp.c_str(), strResp.length()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] send setup response fail.",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    setStatus(RTSP_SESSION_STATUS_SETUP);
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] handle setup request success.",
                            m_unSessionIndex));
    return RET_OK;
}
int32_t CMduRtspPushSession::handleRtspRecordReq(CRtspMessage &rtspMessage)
{
    if (!m_pRtpSession)
    {
        return RET_FAIL;
    }

    if (RTSP_SESSION_STATUS_SETUP > getStatus())
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle record req fail, status[%u] invalid.",
                    m_unSessionIndex, getStatus()));
        return RET_FAIL;
    }

    CRtspRecordMessage *pReq = dynamic_cast<CRtspRecordMessage*>(&rtspMessage);
    if (!pReq)
    {
        return RET_FAIL;
    }

    sendCommonResp(RTSP_SUCCESS_OK, rtspMessage.getCSeq());

    setStatus(RTSP_SESSION_STATUS_PLAY);

    clearRtspCachedMessage();

    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] handle rtsp record request success.",
                        m_unSessionIndex));
    return RET_OK;
}
int32_t CMduRtspPushSession::handleRtspGetParameterReq(CRtspMessage &rtspMessage)
{
    sendCommonResp(RTSP_SUCCESS_OK, rtspMessage.getCSeq());

    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] send get parameter response success.", m_unSessionIndex));

    simulateSendRtcpMsg();

    return RET_OK;
}


int32_t CMduRtspPushSession::handleRtspPlayReq(CRtspMessage &rtspMessage)
{
    if (!m_pRtpSession)
    {
        return RET_FAIL;
    }

    if (RTSP_SESSION_STATUS_SETUP > getStatus())
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle play req fail, status[%u] invalid.",
                m_unSessionIndex, getStatus()));
        return RET_FAIL;
    }

    CRtspPlayMessage *pReq = dynamic_cast<CRtspPlayMessage*>(&rtspMessage);
    if (!pReq)
    {
        return RET_FAIL;
    }

    if (!m_pPeerSession)
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle rtsp play request fail, "
                "can't find peer session.",m_unSessionIndex));
        return RET_FAIL;
    }

    if (MDU_SESSION_STATUS_WAIT_CHANNEL_REDAY == m_pRtpSession->getStatus())
    {
        BUSINESS_LIST businessList;
        CMduBusinessManager::instance()->findBusiness( m_pRtpSession->getStreamId(), businessList);
        for (BUSINESS_LIST_ITER iter = businessList.begin();
                iter != businessList.end(); iter++)
        {
            CMduBusiness *pBusiness = *iter;
            if (RET_OK != pBusiness->start())
            {
                CMduBusinessManager::instance()->releaseBusiness(pBusiness);
                SVS_LOG((SVS_LM_WARNING,"start distribute fail, stream[%Q] start business fail.",
                                m_pRtpSession->getStreamId()));

                return RET_FAIL;
            }

            CMduBusinessManager::instance()->releaseBusiness(pBusiness);
        }

        MDU_INNER_MSG innerMsg;
        innerMsg.ullStreamID = m_pRtpSession->getStreamId();
        innerMsg.unBodyOffset = sizeof(MDU_INNER_MSG);
        innerMsg.usMsgType = INNER_MSG_RTSP;

        (void) m_pRtpSession->handleInnerMessage(innerMsg,  sizeof(MDU_INNER_MSG), *m_pPeerSession);

        if (!pReq->hasRange())
        {
            MEDIA_RANGE_S   stRange;
            stRange.enRangeType = RANGE_TYPE_NPT;
            stRange.MediaBeginOffset = OFFSET_CUR;
            stRange.MediaEndOffset = OFFSET_END;

            pReq->setRange(stRange);
        }
        else
        {
            pReq->getRange(m_strPlayRange);
        }
    }

    if ((MDU_SESSION_STATUS_DISPATCHING != m_pPeerSession->getStatus()))
    {
        return cacheRtspMessage(rtspMessage);
    }

    if (PLAY_TYPE_LIVE == m_enPlayType)
    {
        sendCommonResp(RTSP_SUCCESS_OK, rtspMessage.getCSeq());
    }
    else
    {
        std::string strRtsp;
        (void)pReq->encodeMessage(strRtsp);

        CRtspPacket rtspPack;
        if (0 != rtspPack.parse(strRtsp.c_str(), strRtsp.length()))
        {
            sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle rtsp play request fail, "
                            "parse rtsp packet fail.",
                            m_unSessionIndex));
            return RET_FAIL;
        }

        if (RET_OK != m_pPeerSession->sendVcrMessage(rtspPack))
        {
            sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle rtsp play request fail, "
                    "peer session send vcr message fail.",
                    m_unSessionIndex));
            return RET_FAIL;
        }
    }

    setStatus(RTSP_SESSION_STATUS_PLAY);

    clearRtspCachedMessage();

    //send the play request
    if(m_bSetUp){
        sendMediaPlayReq();
    }
    else
    {
        /* Key Frame request */
        sendKeyFrameReq();
    }
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] handle rtsp play request success.",
                    m_unSessionIndex));
    return RET_OK;
}


int32_t CMduRtspPushSession::handleRtspAnnounceReq(const CRtspMessage &rtspMessage)
{
    if (RTSP_SESSION_STATUS_INIT != getStatus())
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle describe req fail, status[%u] invalid.",
                    m_unSessionIndex, getStatus()));
        return RET_FAIL;
    }
    CSVSMediaLink MediaLink;

    int32_t nRet = CSVSMediaLinkFactory::instance().parseMediaUrl(rtspMessage.getRtspUrl(),&MediaLink);
    if((SVS_MEDIA_LINK_RESULT_SUCCESS != nRet)
        &&(SVS_MEDIA_LINK_RESULT_AUTH_FAIL != nRet))
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle announce req fail, content invalid."));
        return RET_FAIL;
    }
    if(SVS_MEDIA_LINK_RESULT_AUTH_FAIL == nRet)
    {
        if(CMduConfig::instance()->getUrlEffectiveWhile())
        {
            close();
            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle announce req fail, auth invalid.",m_unSessionIndex));
            return RET_FAIL;
        }
    }

    if(NULL == m_pPeerSession)
    {
        m_pPeerSession = CMduSessionFactory::instance()->findSession(m_strContentID);
    }

    if (NULL == m_pPeerSession)
    {
        CRtspAnnounceMessage *pReq = new CRtspAnnounceMessage();
        if (!pReq)  //lint !e774
        {
            return RET_FAIL;
        }
        pReq->setRtspUrl(rtspMessage.getRtspUrl());
        pReq->setCSeq(rtspMessage.getCSeq());
        //pReq->setSession(rtspMessage.getSession());
        std::stringstream sessionIdex;
        sessionIdex << m_unSessionIndex;
        pReq->setSession(sessionIdex.str());
        std::string strContendType = rtspMessage.getContetType();
        std::string strContend = rtspMessage.getBody();
        pReq->setBody(strContendType,strContend);

        if (!m_pLastRtspMsg)
        {
            delete m_pLastRtspMsg;
        }
        m_pLastRtspMsg = pReq;
        SVS_LOG((SVS_LM_INFO,"rtsp session[%u] save Announce request[%p].",
                            m_unSessionIndex, m_pLastRtspMsg));

        if (RET_OK != sendMediaSetupReq(&MediaLink))
        {
            delete m_pLastRtspMsg;
            m_pLastRtspMsg = NULL;

            SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle Announce request fail, "
                        "send setup request fail.",
                        m_unSessionIndex));
            return RET_FAIL;
        }

        return RET_OK;
    }

    std::string strSdp = rtspMessage.getBody();
    if (RET_OK == m_RtspSdp.decodeSdp(strSdp))
    {
        m_RtspSdp.setUrl(rtspMessage.getRtspUrl());
    }

    sendCommonResp(RTSP_SUCCESS_OK, rtspMessage.getCSeq());

    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] handle describe request success.", m_unSessionIndex));
    return RET_OK;
}


int32_t CMduRtspPushSession::handleRtspPauseReq(CRtspMessage &rtspMessage)
{
    if (RTSP_SESSION_STATUS_PLAY != getStatus()
            && RTSP_SESSION_STATUS_PAUSE != getStatus())
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle pause req fail, status[%u] invalid.",
                m_unSessionIndex, getStatus()));
        return RET_FAIL;
    }

    CRtspPauseMessage *pReq = dynamic_cast<CRtspPauseMessage*>(&rtspMessage);
    if (!pReq)
    {
        return RET_FAIL;
    }

    if (!m_pPeerSession)
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle rtsp play request fail, "
                "can't find peer session.",m_unSessionIndex));
        return RET_FAIL;
    }

    std::string strRtsp;
    (void)pReq->encodeMessage(strRtsp);

    CRtspPacket rtspPack;
    if (0 != rtspPack.parse(strRtsp.c_str(), strRtsp.length()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle rtsp pause request fail, "
                        "parse rtsp packet fail.",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    if (RET_OK != m_pPeerSession->sendVcrMessage(rtspPack))
    {
        sendCommonResp(RTSP_SERVER_INTERNAL, rtspMessage.getCSeq());
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] handle rtsp pause request fail, "
                "peer session send vcr message fail.",
                m_unSessionIndex));
        return RET_FAIL;
    }

    setStatus(RTSP_SESSION_STATUS_PAUSE);
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] handle rtsp pause request success.",
                     m_unSessionIndex));
    return RET_OK;
}

// TEARDOWN
int32_t CMduRtspPushSession::handleRtspTeardownReq(CRtspMessage &rtspMessage)
{
    rtspMessage.setMsgType(RTSP_MSG_RSP);
    rtspMessage.setStatusCode(RTSP_SUCCESS_OK);

    std::string strRtsp;
    if (RET_OK != rtspMessage.encodeMessage(strRtsp))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] encode teardown response fail.", m_unSessionIndex));
    }

    if (RET_OK != sendMessage(strRtsp.c_str(), strRtsp.length()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] send teardown response fail.", m_unSessionIndex));
    }

    //close the session
    (void)handle_close(m_sockHandle, 0);
    setStatus(RTSP_SESSION_STATUS_TEARDOWN);

    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] handle rtsp teardown request success.",
                     m_unSessionIndex));
    return RET_OK;
}

void CMduRtspPushSession::sendCommonResp(uint32_t unStatusCode, uint32_t unCseq)
{
    std::string strResp;
    CRtspMessage::encodeCommonResp(unStatusCode, unCseq, m_unSessionIndex, strResp);

    if (RET_OK != sendMessage(strResp.c_str(), strResp.length()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] send common response fail.", m_unSessionIndex));
    }
    else
    {
        SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] send common response success.", m_unSessionIndex));
    }

    SVS_LOG((SVS_LM_DEBUG,"%s", strResp.c_str()));
    return;
}

int32_t CMduRtspPushSession::cacheRtspMessage(CRtspMessage &rtspMessage)
{
    if (m_pLastRtspMsg)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] cache rtsp play message fail, "
                "last message[%p] invalid.",
                m_unSessionIndex, m_pLastRtspMsg));
        return RET_FAIL;
    }

    ACE_Reactor *pReactor = reactor();
    if (!pReactor)
    {
        return RET_FAIL;
    }

    CRtspPlayMessage *pReq = dynamic_cast<CRtspPlayMessage*> (&rtspMessage);
    if (!pReq)
    {
        return RET_FAIL;
    }

    CRtspPlayMessage *pPlayMsg = new CRtspPlayMessage;
    if (!pPlayMsg)  //lint !e774
    {
        return RET_FAIL;
    }

    pPlayMsg->setMsgType(RTSP_MSG_REQ);
    pPlayMsg->setRtspUrl(rtspMessage.getRtspUrl());
    pPlayMsg->setCSeq(rtspMessage.getCSeq());
    pPlayMsg->setSession(rtspMessage.getSession());

    MEDIA_RANGE_S stRange;
    pReq->getRange(stRange);
    pPlayMsg->setRange(stRange);
    pPlayMsg->setSpeed(pReq->getSpeed());
    pPlayMsg->setSpeed(pReq->getScale());

    m_pLastRtspMsg = pPlayMsg;


    if (-1 != m_lRedoTimerId)
    {
        (void)pReactor->cancel_timer(m_lRedoTimerId);
        SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] cancel redo timer[%d].",
                m_unSessionIndex, m_lRedoTimerId));
    }

    ACE_Time_Value timeout(0, RTSP_RETRY_INTERVAL);
    m_lRedoTimerId = pReactor->schedule_timer(this, 0, timeout, timeout);
    if (-1 == m_lRedoTimerId)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] cache rtsp play message, create timer fail.",
                    m_unSessionIndex));
        delete m_pLastRtspMsg;
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] cache rtsp play message[%p], timer id[%d].",
            m_unSessionIndex, m_pLastRtspMsg, m_lRedoTimerId));
    return RET_OK;
}

void CMduRtspPushSession::clearRtspCachedMessage()
{
    if (-1 != m_lRedoTimerId)
    {
        ACE_Reactor *pReactor = reactor();
        if (pReactor)
        {
            (void)pReactor->cancel_timer(m_lRedoTimerId);
        }

        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] cancel redo timer[%d].",
                m_unSessionIndex, m_lRedoTimerId));
        m_lRedoTimerId = -1;
    }

    if (m_pLastRtspMsg)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] delete cache message[%p].",
                        m_unSessionIndex, m_pLastRtspMsg));
        delete m_pLastRtspMsg;
        m_pLastRtspMsg = NULL;
    }
    return;
}

int32_t CMduRtspPushSession::checkTransDirection(uint32_t unPeerType, uint32_t unTransDirection) const
{
    if (PEER_TYPE_PU == unPeerType)
    {
        if ((TRANS_DIRECTION_RECVONLY == unTransDirection)
          ||(TRANS_DIRECTION_SENDRECV == unTransDirection))
        {
            return RET_OK;
        }
    }
    else if (PEER_TYPE_CU == unPeerType)
    {
        if (TRANS_DIRECTION_RECVONLY == unTransDirection)
        {
            return RET_OK;
        }
    }
    else if (PEER_TYPE_RECORD == unPeerType)
    {
        if (TRANS_DIRECTION_RECVONLY == unTransDirection)
        {
            return RET_OK;
        }
    }
    else if (PEER_TYPE_STREAM == unPeerType)
    {
        if (TRANS_DIRECTION_RECVONLY == unTransDirection)
        {
            return RET_OK;
        }
    }

    return RET_FAIL;
}

int32_t CMduRtspPushSession::createDistribute(CSVSMediaLink* linkinfo,std::string& strSdpInfo)
{
    std::string strUrl;
    CMduSession *pPeerSession  = NULL;

    if(NULL == linkinfo)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] create distribute fail,get content fail."));
        return RET_FAIL;
    }
    m_strContentID = linkinfo->ContentID();

    //create the peer session first
    if(SVS_DEV_TYPE_GB28181 == linkinfo->DevType()) {
        pPeerSession = CMduSessionFactory::instance()->createSourceSession(m_strContentID, PEER_TYPE_PU, RTP_SESSION,false);
    }
    else if(SVS_DEV_TYPE_EHOME == linkinfo->DevType()) {
        pPeerSession = CMduSessionFactory::instance()->createSourceSession(m_strContentID, PEER_TYPE_PU, EHOME_SESSION,false);
    }
    if (NULL == pPeerSession)
    {
        SVS_LOG((SVS_LM_ERROR,"Create distribute fail, create peer session fail."));
        return RET_FAIL;
    }
    if (RET_OK != pPeerSession->init(m_strContentID.c_str(),linkinfo->PlayType()))
    {
        SVS_LOG((SVS_LM_ERROR,"Create distribute fail,session init fail."));
        CMduSessionFactory::instance()->releaseSession(pPeerSession);
        return RET_FAIL;
    }

    m_pPeerSession     = pPeerSession;

    /* create the sdp info by device type */
    SDP_MEDIA_INFO info;
    CMediaSdp sdpCreator;

    SVS_DEV_TYPE enDevType  = linkinfo->DevType();
    PLAY_TYPE    enPlayType = linkinfo->PlayType();

    std::string strAddr = pPeerSession->getVideoAddr().get_host_addr();

    std::string strSerID = CMduConfig::instance()->getServiceId();
    //CMduSccConnector* pConnect = CMduServiceTask::instance()->getSccConnector();
    //if (NULL != pConnect)
    //{
    //    strSerID = pConnect->getZoneID();
    //}


    sdpCreator.setConnAddr(strAddr);
    sdpCreator.setTransDirect(TRANS_DIRECTION_RECVONLY);
    //sdpCreator.setOwner(m_strContentID);
    sdpCreator.setOwner(strSerID);
    if(SVS_DEV_TYPE_VMS == enDevType) {
        /* Video */
        info.ucPayloadType = PT_TYPE_H264;
        info.usPort        = pPeerSession->getVideoAddr().get_port_number();
        info.strControl    = "";
        info.strRtpmap     = "";
        info.strFmtp       = "";
        /* H264 ES OVER RTP */
        info.ucPayloadType = PT_TYPE_H264;
        sdpCreator.addVideoInfo(info);
        /* H265 ES OVER RTP */
        info.ucPayloadType = PT_TYPE_H265;
        sdpCreator.addVideoInfo(info);

        /* Audio */
        /* PCMU OVER RTP */
        info.ucPayloadType = PT_TYPE_PCMU;
        info.usPort        = pPeerSession->getAudioAddr().get_port_number();
        info.strControl    = "";
        info.strRtpmap     = "";
        info.strFmtp       = "";
        sdpCreator.addAudioInfo(info);
        /* PCMA OVER RTP */
        info.ucPayloadType = PT_TYPE_PCMA;
        sdpCreator.addAudioInfo(info);
    }
    else if(SVS_DEV_TYPE_GB28181 == enDevType) {
        /* Video */
        info.ucPayloadType = PT_TYPE_H264;
        info.usPort        = pPeerSession->getVideoAddr().get_port_number();
        info.strControl    = "";
        info.strRtpmap     = "";
        info.strFmtp       = "";
        /* PS OVER RTP */
        info.ucPayloadType = PT_TYPE_PS;
        sdpCreator.addVideoInfo(info);
        /* H264 ES OVER RTP */
        info.ucPayloadType = PT_TYPE_H264;
        sdpCreator.addVideoInfo(info);
         /* H265 ES OVER RTP */
        //info.ucPayloadType = PT_TYPE_H265;
        //sdpCreator.addVideoInfo(info);

        /* Audio */
        /* PCMA OVER RTP */
        //info.ucPayloadType = PT_TYPE_PCMA;
        //info.usPort        = pPeerSession->getAudioAddr().get_port_number();
        //info.strControl    = "";
        //info.strRtpmap     = "";
        //info.strFmtp       = "";
        //sdpCreator.addAudioInfo(info);
        /* GB28181 only A */
        /* PCMU OVER RTP */
        //info.ucPayloadType = PT_TYPE_PCMU;
        //sdpCreator.addAudioInfo(info);

        /* GB28181 Sessoin Name
           live:Play
           playback:Playback
           DownLoad:DownLoad
         */
        if(PLAY_TYPE_LIVE == enPlayType) {
            sdpCreator.setSessionName("Play");
        }
        else if(PLAY_TYPE_FRONT_RECORD == enPlayType) {
            sdpCreator.setSessionName("Playback");
        }
        //std::string strSrrc = "0999999999";
        //sdpCreator.setSsrc(strSrrc);
        sdpCreator.setSsrcByGB28181Dev(enPlayType,m_unSessionIndex);
        //std::string strFormat = "v/2////a/1//";
        std::string strFormat = "";
        sdpCreator.setGB28181Format(strFormat);

    }

    sdpCreator.encodeSdp(strSdpInfo,0,"");

    return RET_OK;
}

void CMduRtspPushSession::simulateSendRtcpMsg()
{
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] simulate send rtcp message begin.", m_unSessionIndex));


    uint64_t ullRtpSessionId = 0;

    if(NULL == m_pRtpSession)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] simulate Send Rtcp Msg,the rtp session is null.",
                        m_unSessionIndex));
        return;
    }
    ullRtpSessionId = m_pRtpSession->getStreamId();

    ACE_Message_Block *pMsg = CMediaBlockBuffer::instance().allocMediaBlock();
    if (NULL == pMsg)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] simulate Send Rtcp Msg, alloc media blockfail.",
                        m_unSessionIndex));
        return;
    }

    fillMduInnerMsg(pMsg->base(),
                    ullRtpSessionId,
                    this,
                    m_PeerAddr.get_ip_address(),
                    m_PeerAddr.get_port_number(),
                    INNER_MSG_RTCP,
                    sizeof(MDU_INNER_MSG));
    pMsg->wr_ptr(sizeof(MDU_INNER_MSG));


    if (RET_OK != CMduServiceTask::instance()->enqueueInnerMessage(pMsg))
    {
        CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] simulate Send Rtcp Msg, enqueue inner message fail.",
                         m_unSessionIndex));
    }
    SVS_LOG((SVS_LM_INFO,"rtsp push session[%u] simulate send rtcp message end.", m_unSessionIndex));
    return;
}


