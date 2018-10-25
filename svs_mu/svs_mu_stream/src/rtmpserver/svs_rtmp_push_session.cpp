/*
 * StreamRtspPushSession.cpp
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
#include "svs_rtmp_push_session.h"
#include "svs_vms_msg_factory.h"
#include "svs_vms_media_setup_req.h"
#include "svs_vms_media_setup_resp.h"
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

#include "svs_rtmp_service.h"
#include "svs_adapter_svs_def.h"

#include "svs_rtmp_amf.h"
#include "svs_rtmp_utils.h"
#include "svs_rtmp.h"
#include <vector>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

CStreamRtmpPushSession::CStreamRtmpPushSession()
{
    m_unSessionIndex  = 0;
    m_enPlayType      = PLAY_TYPE_LIVE;
    m_sockHandle      = ACE_INVALID_HANDLE;
    m_pRecvBuffer     = NULL;
    m_pRtmpSession    = NULL;
    m_pPeerSession    = NULL;

    m_enSessionStatus  = RTMP_SESSION_STATUS_INIT;
    m_ulStatusTime     = SVS_GetSecondTime();

    m_strContentID     = "";
    m_strUri           = "";
    m_lRedoTimerId     = -1;

    m_bPlaying         = false;
    m_bReady           = false;
    m_chunk_len        = DEFAULT_CHUNK_LEN;
    for (int i = 0; i < RTMP_MESSAGE_SIZE; ++i) {
        m_Messages[i].timestamp = 0;
        m_Messages[i].len = 0;
    }

    memset(&m_Serversig, 0, sizeof(Handshake));
    m_Serversig.flags[0] = 0x03;
    for (int i = 0; i < RANDOM_LEN; ++i) {
        m_Serversig.random[i] = rand();
    }
}

CStreamRtmpPushSession::~CStreamRtmpPushSession()
{
    m_unSessionIndex  = 0;
    m_sockHandle      = ACE_INVALID_HANDLE;
    if(NULL != m_pRecvBuffer) {
        delete m_pRecvBuffer;
    }
    m_pRecvBuffer     = NULL;
    m_pRtmpSession     = NULL;
    m_pPeerSession    = NULL;
    m_enHSStatus      = RTMP_HANDSHAKE_STATUS_SERVER_CHALLENGE;

    m_lRedoTimerId     = -1;
}

int32_t CStreamRtmpPushSession::open(uint32_t unIndex, const ACE_INET_Addr &peerAddr)
{
    m_unSessionIndex  = unIndex;
    m_PeerAddr       = peerAddr;

    m_RtpFrameOrganizer.init(this,MAX_RTP_FRAME_CACHE_NUM,true);

    if (ACE_INVALID_HANDLE == m_sockHandle)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtmp push session[%u] fail, socket handle invalid",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    int32_t nRet = setSockOpt();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtmp push session[%u] fail, set socket option fail.",
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
        SVS_LOG((SVS_LM_WARNING,"open rtmp push session fail, can't find reactor instance."));
        return RET_FAIL;
    }

    nRet = pReactor->register_handler(m_sockHandle, this,
                                      ACE_Event_Handler::READ_MASK);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtmp server fail, register read mask fail[%d].",
                ACE_OS::last_error()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"open rtmp push session[%u] success, peer addr[%s:%d], handle[%d].",
                    m_unSessionIndex, m_PeerAddr.get_host_addr(),
                    m_PeerAddr.get_port_number(),
                    m_sockHandle));
    return RET_OK;
}

void CStreamRtmpPushSession::close()
{
    (void)ACE_OS::shutdown(m_sockHandle, SHUT_RDWR);
    SVS_LOG((SVS_LM_INFO,"close rtmp push session[%u] success.handle[%d]",
        m_unSessionIndex, m_sockHandle));
    return;
}

void CStreamRtmpPushSession::setStatus(RTMP_SESSION_STATUS unStatus)
{
    RTMP_SESSION_STATUS unOldStatus = m_enSessionStatus;
    m_enSessionStatus        = unStatus;
    m_ulStatusTime           = SVS_GetSecondTime();
    SVS_LOG((SVS_LM_INFO,"rtmp push session[%u] switch status[%u] to [%u].",
            m_unSessionIndex, unOldStatus, m_enSessionStatus));
    return;
}

RTMP_SESSION_STATUS CStreamRtmpPushSession::getStatus() const
{
    return m_enSessionStatus;
}
ACE_INET_Addr CStreamRtmpPushSession::getPeerAddr()const
{
    return m_PeerAddr;
}

ACE_INET_Addr CStreamRtmpPushSession::getLocalAddr()const
{
    return m_LocalAddr;
}

int32_t CStreamRtmpPushSession::sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount)
{
    if(RTMP_SESSION_STATUS_PLAY != m_enSessionStatus) {
        return RET_OK;
    }

    for(uint32_t i = 0;i < MsgCount;i++)
    {
        m_RtpFrameOrganizer.insertRtpPacket(pMbArray[i]);
    }

    return RET_OK;
}
void CStreamRtmpPushSession::handleRtpFrame(RTP_FRAME_LIST &rtpFrameList)
{
    if(0 == rtpFrameList.size())
    {
        return;
    }

    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_RtmpMutex);

    ACE_Message_Block* pMb = rtpFrameList.front();
    CRtpPacket rtpPacket;
    if (RET_OK != rtpPacket.ParsePacket(pMb->rd_ptr(), pMb->length()))
    {
        SVS_LOG((SVS_LM_ERROR, "fail to insert rtp packet, parse rtp packet fail."));
        return ;
    }

    if(PT_TYPE_H264 == rtpPacket.GetPayloadType())
    {
        sendH264FramebyRtmp(rtpFrameList);
    }
    else
    {
        sendCommonFramebyRtmp(rtpFrameList);
    }
    return;
}


int32_t CStreamRtmpPushSession::handleSvsMessage(CStreamSvsMessage &message)
{
    CStreamMediaSetupResp *pResp = dynamic_cast<CStreamMediaSetupResp*>(&message);
    if (!pResp)
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] handle svs message fail, invalid message.",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    if (0 != pResp->getRespCode())
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] handle svs message fail, retcode[0x%x].",
                                m_unSessionIndex, pResp->getRespCode()));
        close();
        return RET_FAIL;
    }

    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_RtmpMutex);

    if ((NULL == m_pPeerSession )||(RET_OK != m_pPeerSession->start(pResp)))
    {
        SVS_LOG((SVS_LM_ERROR,"start the peer session fail."));
        return RET_FAIL;
    }

    handleStartPlayback();
    uint64_t ullPeerSessionId = m_pPeerSession->getStreamId();

    SVS_LOG((SVS_LM_INFO,"rtmp push session[%u] handle svs message success, peer session id[%Q].",
                    m_unSessionIndex, ullPeerSessionId));
    return RET_OK;
}


int32_t CStreamRtmpPushSession::handle_input(ACE_HANDLE handle)
{
    if ((NULL == m_pRecvBuffer) || (ACE_INVALID_HANDLE == m_sockHandle))
    {
        return -1;
    }

    int32_t iRecvLen = (int32_t) m_pRecvBuffer->size() - (int32_t) m_pRecvBuffer->length();
    if (iRecvLen <= 0)
    {
        SVS_LOG((SVS_LM_INFO,"rtmp push session[%u] recv buffer is full, size[%u] length[%u].",
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
            SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] recv data fail, "
                    "close handle[%d]. errno[%d].",
                    m_unSessionIndex,
                    m_sockHandle,
                    iErrorCode));

            return -1;
        }

        SVS_LOG((SVS_LM_INFO,"rtmp push session[%u] recv data fail, wait retry. errno[%d].",
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
            SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] process recv data fail, close handle[%d]. ",
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

int32_t CStreamRtmpPushSession::handle_timeout(const ACE_Time_Value &tv, const void *arg)
{
    SVS_LOG((SVS_LM_INFO,"rtmp push session[%u] retry process rtmp message.", m_unSessionIndex));

    handleStartPlayback();

    return 0;
}

int32_t CStreamRtmpPushSession::handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask /*close_mask*/)
{
    if (ACE_INVALID_HANDLE == m_sockHandle)
    {
        return 0;
    }

    ACE_Reactor *pReactor = ACE_Reactor::instance();
    if (!pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"handle close rtmp push session[%u] fail, can't find reactor instance.",
                        m_unSessionIndex));
        return -1;
    }

    int32_t nRet = pReactor->remove_handler(m_sockHandle, ACE_Event_Handler::READ_MASK
                                                        | ACE_Event_Handler::DONT_CALL);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"handle close rtmp push session[%u] fail, remove read mask fail[%d].",
                         m_unSessionIndex, ACE_OS::last_error()));
        return -1;
    }

    (void)ACE_OS::close(m_sockHandle);
    m_sockHandle = ACE_INVALID_HANDLE;

    destroyMediaSession();

    setStatus(RTMP_SESSION_STATUS_TEARDOWN);
    SVS_LOG((SVS_LM_INFO,"handle close rtmp push session[%u] success.", m_unSessionIndex));

    return 0;
}

void CStreamRtmpPushSession::setHandle(ACE_HANDLE handle, const ACE_INET_Addr &localAddr)
{
    m_sockHandle = handle;
    m_LocalAddr  = localAddr;

    SVS_LOG((SVS_LM_INFO,"rtmp push session[%u] set handle[%d], local addr[%s:%d].",
                    m_unSessionIndex, handle,
                    m_LocalAddr.get_host_addr(), m_LocalAddr.get_port_number()));
    return;
}

ACE_HANDLE CStreamRtmpPushSession::get_handle() const
{
    return m_sockHandle;
}

uint32_t CStreamRtmpPushSession::getSessionIndex() const
{
    return m_unSessionIndex;
}

int32_t CStreamRtmpPushSession::check()
{
    uint32_t ulCostTime = SVS_GetSecondTime() - m_ulStatusTime;
    if ((RTMP_SESSION_STATUS_PLAY >= getStatus())
            && (ulCostTime > STREAM_STATUS_TIMEOUT_INTERVAL))
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] check status abnormal,"
                " close session.",
                m_unSessionIndex));
        close();
        return RET_OK;
    }

    if (m_pRtmpSession)
    {
        if ((STREAM_SESSION_STATUS_ABNORMAL == m_pRtmpSession->getStatus())
                || (STREAM_SESSION_STATUS_RELEASED == m_pRtmpSession->getStatus()))
        {
            SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] check status abnormal,"
                    " close rtp session[%Q].",
                    m_unSessionIndex, m_pRtmpSession->getStreamId()));
            close();
            return RET_OK;
        }
    }

    if ((RTMP_SESSION_STATUS_TEARDOWN == getStatus())
            && (ulCostTime > STREAM_STATUS_ABNORMAL_INTERVAL))
    {
        SVS_LOG((SVS_LM_INFO,"check rtmp push session[%u] teardown, release session.",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CStreamRtmpPushSession::setSockOpt()
{
    if (ACE_INVALID_HANDLE == m_sockHandle)
    {
        return RET_FAIL;
    }

    if (ACE_OS::fcntl(m_sockHandle, F_SETFL, ACE_OS::fcntl(m_sockHandle, F_GETFL) | O_NONBLOCK))
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] set O_NONBLOCK fail, errno[%d].",
                        m_unSessionIndex,
                        errno));
        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CStreamRtmpPushSession::sendMessage(const char* pData, uint32_t unDataSize)
{

    ACE_Time_Value timeout(1);
    int32_t nSendSize = ACE::send_n(m_sockHandle, pData, unDataSize, &timeout);
    if (unDataSize != (uint32_t)nSendSize)
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] send message fail, close handle[%d].",
                        m_unSessionIndex, m_sockHandle));
        return RET_FAIL;
    }

    return RET_OK;
}
int32_t CStreamRtmpPushSession::sendRtmpData(uint8_t type, uint32_t endpoint,const std::string &buf,
                                    unsigned long timestamp,int channel_num)
{
    if (endpoint == STREAM_ID) {
        /*
         * For some unknown reason, stream-related msgs must be sent
         * on a specific channel.
         */
        channel_num = CHAN_STREAM;
    }

    RTMP_Header header;
    header.flags = (channel_num & 0x3f) | (0 << 6);
    header.msg_type = type;
    set_be24(header.timestamp, timestamp);
    set_be24(header.msg_len, buf.size());
    set_le32(header.endpoint, endpoint);

    m_send_queue.append((char *) &header, sizeof header);

    size_t pos = 0;
    while (pos < buf.size()) {
        if (pos) {
            uint8_t flags = (channel_num & 0x3f) | (3 << 6);
            m_send_queue += char(flags);

        }

        size_t chunk = buf.size() - pos;
        if (chunk > m_chunk_len)
            chunk = m_chunk_len;
        m_send_queue.append(buf, pos, chunk);


        pos += chunk;
    }

    sendMessage(m_send_queue.c_str(),m_send_queue.length());
    return RET_OK;
}


int32_t CStreamRtmpPushSession::sendMediaSetupReq(CSVSMediaLink* linkInof)
{
    std::string   strSdpInfo;
    std::string   strUrl = linkInof->Url();

    m_enPlayType    = linkInof->PlayType();

    /* allocate the local session first */
    int32_t nRet = createDistribute(linkInof,strSdpInfo);
    if(RET_OK != nRet) {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] set the media sdp info.rtspUrl:\n %s",
                    m_unSessionIndex, nRet,strUrl.c_str()));
        return RET_FAIL;
    }

    uint32_t unMsgLen = sizeof(SVS_MSG_STREAM_SESSION_SETUP_REQ) ;

    CStreamSvsMessage *pMessage = NULL;
    nRet = CStreamMsgFactory::instance()->createSvsMsg(SVS_MSG_TYPE_STREAM_SESSION_SETUP_REQ,
                                                    unMsgLen, 0,pMessage);
    if ((RET_OK != nRet) || (NULL == pMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] create stream session setup request fail.",
                        m_unSessionIndex));
        return RET_FAIL;
    }

    CStreamMediaSetupReq *pReq = dynamic_cast<CStreamMediaSetupReq*>(pMessage);
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
        CStreamMsgFactory::instance()->destroySvsMsg(pMessage);
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] init stream session setup request fail.",
                                m_unSessionIndex));
        return RET_FAIL;
    }

    if (RET_OK != pReq->handleMessage())
    {
        CStreamMsgFactory::instance()->destroySvsMsg(pMessage);
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] handle stream session setup request fail.",
                m_unSessionIndex));
        return RET_FAIL;
    }

    CStreamMsgFactory::instance()->destroySvsMsg(pMessage);
    SVS_LOG((SVS_LM_INFO,"rtmp push session[%u] send stream session setup request success.",
            m_unSessionIndex));
    return RET_OK;
}

void CStreamRtmpPushSession::sendKeyFrameReq()
{
    uint32_t unMsgLen = sizeof(SVS_MSG_STREAM_KEY_FRAME_REQ);

    CStreamSvsMessage *pMessage = NULL;
    int32_t nRet = CStreamMsgFactory::instance()->createSvsMsg(SVS_MSG_TYPE_MEDIA_KEYFRAME_REQ,
                                                    unMsgLen, 0,pMessage);
    if ((RET_OK != nRet) || (NULL == pMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] create stream key frame request fail.",
                        m_unSessionIndex));
        return ;
    }

    CStreamMediaKeyFrameReq *pReq = dynamic_cast<CStreamMediaKeyFrameReq*>(pMessage);
    if (!pReq)
    {
        return ;
    }

    nRet = pReq->initMsgBody((uint8_t*)m_strContentID.c_str());

    if (RET_OK != nRet)
    {
        CStreamMsgFactory::instance()->destroySvsMsg(pMessage);
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] init stream key frame request fail.",
                                m_unSessionIndex));
        return ;
    }

    if (RET_OK != pReq->handleMessage())
    {
        CStreamMsgFactory::instance()->destroySvsMsg(pMessage);
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] handle stream key frame request fail.",
                m_unSessionIndex));
        return ;
    }

    CStreamMsgFactory::instance()->destroySvsMsg(pMessage);
    SVS_LOG((SVS_LM_INFO,"rtmp push session[%u] send stream key frame request success.",
            m_unSessionIndex));
    return;
}

int32_t CStreamRtmpPushSession::createMediaSession()
{
    CStreamSession *pSession = CStreamSessionFactory::instance()->createSession(PEER_TYPE_CU, RTSP_SESSION,true);
    if (!pSession)
    {
        SVS_LOG((SVS_LM_ERROR,"rtmp push session[%u] create media session fail.", m_unSessionIndex));
        return RET_FAIL;
    }

    CStreamRtmpSession* pRtmpSession = dynamic_cast<CStreamRtmpSession*>(pSession);
    if (!pRtmpSession)
    {
        CStreamSessionFactory::instance()->releaseSession(pSession);
        SVS_LOG((SVS_LM_ERROR,"rtmp push session[%u] create std media session fail.", m_unSessionIndex));
        return RET_FAIL;
    }

    if (!m_pPeerSession)
    {
        CStreamSessionFactory::instance()->releaseSession(pSession);
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] handle rtmp play request fail, "
                "can't find peer session ContentID[%s]",
                m_unSessionIndex, m_strContentID.c_str()));
        return RET_FAIL;
    }

    uint64_t ullPeerSessionId = m_pPeerSession->getStreamId();

    pRtmpSession->setRtmpHandle(this);
    uint8_t VideoPayloadType = PT_TYPE_H264;
    uint8_t AudioPayloadType = PT_TYPE_PCMU;
    MEDIA_INFO_LIST VideoinfoList;
    MEDIA_INFO_LIST AudioinfoList;
    m_RtmpSdp.getVideoInfo(VideoinfoList);
    m_RtmpSdp.getAudioInfo(AudioinfoList);

    if(0 <VideoinfoList.size()) {
        VideoPayloadType = VideoinfoList.front().ucPayloadType;
    }
    if(0 <AudioinfoList.size()) {
        AudioPayloadType = AudioinfoList.front().ucPayloadType;
    }

    pRtmpSession->setPlayLoad(VideoPayloadType,AudioPayloadType);
    if (RET_OK != pRtmpSession->initRtmpSession(ullPeerSessionId,
                                                 m_enPlayType,
                                                 m_LocalAddr,
                                                 m_PeerAddr))
    {
        CStreamSessionFactory::instance()->releaseSession(pSession);
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] init rtmp session fail.", m_unSessionIndex));
        return RET_FAIL;
    }

    m_pRtmpSession = pRtmpSession;
    if(NULL != m_pRtmpSession)
    {
        m_pRtmpSession->setSdpInfo(m_RtmpSdp);
        m_pRtmpSession->setContentID(m_pPeerSession->getContentID());
    }


    CStreamBusiness *pBusiness = CStreamBusinessManager::instance()->createBusiness(ullPeerSessionId,
                                                                        pRtmpSession->getStreamId(),
                                                                        pRtmpSession->getPlayType());
    if (!pBusiness)
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] create business fail, pu[%Q] cu[%Q].",
                        m_unSessionIndex, pSession->getStreamId(), ullPeerSessionId));

        CStreamSessionFactory::instance()->releaseSession(pSession);
        m_pRtmpSession = NULL;
        return RET_FAIL;
    }

    if (RET_OK != pBusiness->start())
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] start business fail, pu[%Q] cu[%Q].",
                m_unSessionIndex, pSession->getStreamId(), ullPeerSessionId));

        CStreamSessionFactory::instance()->releaseSession(pSession);

        CStreamBusinessManager::instance()->releaseBusiness(pBusiness);
        m_pRtmpSession = NULL;
        return RET_FAIL;
    }


    SVS_LOG((SVS_LM_INFO,"rtmp push session[%u] create session success, cu[%Q] pu[%Q].",
                    m_unSessionIndex, pSession->getStreamId(), ullPeerSessionId));
    return RET_OK;
}

void CStreamRtmpPushSession::destroyMediaSession()
{
    uint64_t ullPeerSessionId = 0;
    uint64_t ullRtmpSessionId = 0;
    SVS_LOG((SVS_LM_DEBUG,"rtmp push session destory media session."));
    if (m_pRtmpSession)
    {
        ullRtmpSessionId = m_pRtmpSession->getStreamId();
        CStreamBusiness *pBusiness = CStreamBusinessManager::instance()->findBusiness(ullRtmpSessionId);
        if (!pBusiness)
        {
            SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] not find the buinsess.", m_unSessionIndex));
            return;
        }

        CStreamBusinessManager::instance()->releaseBusiness(pBusiness);
        CStreamBusinessManager::instance()->releaseBusiness(pBusiness);

        CStreamSessionFactory::instance()->releaseSession(ullRtmpSessionId);
        m_pRtmpSession = NULL;
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] media session already released.", m_unSessionIndex));
    }

    if(NULL != m_pPeerSession)
    {
        ullPeerSessionId = m_pPeerSession->getStreamId();
        /* the peer session is create by self,so release twice*/
        SVS_LOG((SVS_LM_DEBUG,"rtmp push session:[%Q], release PeerSession:[%Q].",
                                                          ullRtmpSessionId,ullPeerSessionId));
        CStreamSessionFactory::instance()->releaseSession(m_pPeerSession);
        m_pPeerSession = NULL;
    }


    //try to find the peer session ,if the session is not exit,so call stop source
    CStreamSession* pPeerSession = CStreamSessionFactory::instance()->findSession(ullPeerSessionId);
    if(NULL != pPeerSession)
    {
        CStreamSessionFactory::instance()->releaseSession(ullPeerSessionId);
        return;
    }

    CStreamSvsMessage *pReq = NULL;
    int32_t nRet = CStreamMsgFactory::instance()->createSvsMsg(SVS_MSG_TYPE_STREAM_SESSION_TEARDOWN_REQ,
                                                        sizeof(SVS_MSG_STREAM_SESSION_TEARDOWN_REQ),
                                                        0,
                                                        pReq);
    if ((RET_OK != nRet) || (NULL == pReq))
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] create tear down fail, create msg fail.",
                        m_unSessionIndex));
        CStreamMsgFactory::instance()->destroySvsMsg(pReq);
        return;
    }

    CStreamMediaTearDownReq* pTearDownRequest = (CStreamMediaTearDownReq*) (void*) pReq;
    if (RET_OK != pTearDownRequest->initMsgBody(m_unSessionIndex,m_strContentID.c_str()))
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] init tear down fail, create msg fail.",
                          m_unSessionIndex));
        CStreamMsgFactory::instance()->destroySvsMsg(pReq);
        return;
    }

    if (RET_OK != pReq->handleMessage())
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle tear down fail, create msg fail.",
                                m_unSessionIndex));
    }
    else
    {
        SVS_LOG((SVS_LM_INFO,"rtmp session[%u] handle tear down success.", m_unSessionIndex));
    }

    CStreamMsgFactory::instance()->destroySvsMsg(pReq);

    SVS_LOG((SVS_LM_INFO,"rtmp session[%u] destroy media session success.", m_unSessionIndex));
    return;
}


int32_t CStreamRtmpPushSession::processRecvedMessage(const char* pData, uint32_t unDataSize)
{
    if ((NULL == pData) || (0 == unDataSize))
    {
        return -1;
    }
    uint32_t nMessageLen = unDataSize;
    char*   pMessgae    = (char*)pData;
    int32_t nDealLen    = 0;

    if(RTMP_SESSION_STATUS_INIT == m_enSessionStatus) {
        return handleHandShake(pData,unDataSize);
    }

    /* parser the RTMP Message */
    while (0 < nMessageLen) {
        uint8_t flags = pData[0];

        static const size_t HEADER_LENGTH[] = {12, 8, 4, 1};

        size_t header_len = HEADER_LENGTH[flags >> 6];

        if (nMessageLen < header_len) {
            /* need more data */
            break;
        }

        RTMP_Header header;
        memcpy(&header, pData, header_len);

        RTMP_Message *msg = &m_Messages[flags & 0x3f];

        if (header_len >= 8) {
            msg->len = load_be24(header.msg_len);
            if (msg->len < RTMP_CHUNK_SIZE) {
                SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle invalid msg length:[%d].",
                                m_unSessionIndex,msg->len));
                return -1;
            }
            msg->type = header.msg_type;
        }
        if (header_len >= 12) {
            msg->endpoint = load_le32(header.endpoint);
        }

        if (msg->len == 0) {
            SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle message without a header.",
                                m_unSessionIndex));
            return -1;
        }
        size_t chunk = msg->len - nMessageLen;
        if (chunk > m_chunk_len)
            chunk = m_chunk_len;

        if (nMessageLen < header_len + chunk) {
            /* need more data */
            break;
        }

        if (header_len >= 4) {
            unsigned long ts = load_be24(header.timestamp);
            if (ts == 0xffffff) {
                SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle ext timestamp not supported.",
                                m_unSessionIndex));
            }
            if (header_len < 12) {
                ts += msg->timestamp;
            }
            msg->timestamp = ts;
        }

        msg->buf = &pData[header_len];
        size_t     Lens = header_len + chunk;
        nDealLen    += Lens;
        pMessgae    += Lens;
        nMessageLen -= Lens;

        if (Lens == msg->len) {
            handleMessage(msg);
            msg->buf = "";
        }
    }
    SVS_LOG((SVS_LM_INFO,"rtmp push session[%u] success to process rtmp message.", m_unSessionIndex));
    return nDealLen;
}

int32_t CStreamRtmpPushSession::handleHandShake(const char* pData, uint32_t unDataSize)
{
    uint32_t nMessageLen = unDataSize;
    char*   pMessgae    = (char*)pData;
    int32_t nDealLen    = 0;

    do {
        if(RTMP_HANDSHAKE_STATUS_SERVER_CHALLENGE == m_enHSStatus) {
            // recv C0
            if (pData[0] != HANDSHAKE_PLAINTEXT) {
                return -1;
            }

            // send S0
            if (RET_OK != sendMessage(pData, 1)) {
                return -1;
            }
            nDealLen    += 1;
            pMessgae    += 1;
            nMessageLen -= 1;

            //  send S1
            if (RET_OK != sendMessage((const char*)&m_Serversig, sizeof(Handshake))) {
                return -1;
            }
            m_enHSStatus = RTMP_HANDSHAKE_STATUS_SERVER_RESPONSE;
        }
        if(RTMP_HANDSHAKE_STATUS_SERVER_RESPONSE == m_enHSStatus) {

            // recv C1
            if (nMessageLen < sizeof(Handshake)) {
                break ;
            }
            // send S2
            if (RET_OK != sendMessage(pMessgae, sizeof(Handshake))) {
                return -1;
            }
            nDealLen    += sizeof(Handshake);
            pMessgae    += sizeof(Handshake);
            nMessageLen -= sizeof(Handshake);
            m_enHSStatus = RTMP_HANDSHAKE_STATUS_SERVER_DONE;
        }
        if(RTMP_HANDSHAKE_STATUS_SERVER_DONE == m_enHSStatus) {
            // recv C2
            if (nMessageLen < sizeof(Handshake)) {
                break;
            }
            nDealLen    += sizeof(Handshake);
            pMessgae    += sizeof(Handshake);
            nMessageLen -= sizeof(Handshake);
            m_enSessionStatus = RTMP_SESSION_STATUS_HANDSHAKED;
        }
    }while(true);
    return nDealLen;
}

void CStreamRtmpPushSession::handleMessage(RTMP_Message *msg)
{
    size_t pos = 0;

    switch (msg->type) {
        case MSG_BYTES_READ:
            if (pos + 4 > msg->len) {
                SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle,Not enough data",
                                m_unSessionIndex));
                return;
            }
            break;

        case MSG_SET_CHUNK:
            if (pos + 4 > msg->buf.size()) {
                SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle,Not enough data",
                                m_unSessionIndex));
                return;
            }
            m_chunk_len = load_be32(&msg->buf[pos]);
            break;

        case MSG_INVOKE: {
                Decoder dec;
                dec.version = 0;
                dec.buf = msg->buf;
                dec.pos = 0;
                handleInvoke(msg, &dec);
            }
            break;

        case MSG_INVOKE3: {
                Decoder dec;
                dec.version = 0;
                dec.buf = msg->buf;
                dec.pos = 1;
                handleInvoke(msg, &dec);
            }
            break;

        case MSG_NOTIFY: {
                Decoder dec;
                dec.version = 0;
                dec.buf = msg->buf;
                dec.pos = 0;
                std::string type = amf_load_string(&dec);
                debug("notify %s\n", type.c_str());
                if (msg->endpoint == STREAM_ID) {
                    if (type == "@setDataFrame") {
                        handleSetdataframe(&dec);
                    }
                }
            }
            break;

        case MSG_AUDIO:
            SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle,not a publisher",m_unSessionIndex));
            break;

        case MSG_VIDEO:
            SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle,not a publisher",m_unSessionIndex));
            break;

        case MSG_FLASH_VIDEO:
            SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle,streaming FLV not supported",m_unSessionIndex));
            break;

        default:
            SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle,unhandled message: %02x\n",m_unSessionIndex, msg->type));
            break;
    }
}

void CStreamRtmpPushSession::handleInvoke(const RTMP_Message *msg, Decoder *dec)
{
    std::string method = amf_load_string(dec);
    double txid = amf_load_number(dec);

    SVS_LOG((SVS_LM_DEBUG,"invoked %s\n", method.c_str()));

    if (msg->endpoint == CONTROL_ID) {
        if (method == "connect") {
            handleConnect(txid, dec);
        } else if (method == "FCPublish") {
            handleFcpublish(txid, dec);
        } else if (method == "createStream") {
            handleCreateStream(txid, dec);
        }

    } else if (msg->endpoint == STREAM_ID) {
        if (method == "publish") {
            handlePublish(txid, dec);
        } else if (method == "play") {
            handlePlay(txid, dec);
        } else if (method == "play2") {
            handlePlay2(txid, dec);
        } else if (method == "pause") {
            handlePause(txid, dec);
        }
    }
}

void CStreamRtmpPushSession::handleSetdataframe(Decoder *dec)
{
    SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle,not a publisher.",m_unSessionIndex));
    return;
    /*
    std::string type = amf_load_string(dec);
    if (type != "onMetaData") {
        SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle,can only set metadata.",m_unSessionIndex));
        return;
    }
    amf_object_t metadata;
    metadata = amf_load_ecma(dec);

    Encoder notify;
    amf_write(&notify, std::string("onMetaData"));
    amf_write_ecma(&notify, metadata);

    FOR_EACH(std::vector<Client *>, i, clients) {
        Client *client = *i;
        if (client != NULL && client->playing) {
            rtmp_send(client, MSG_NOTIFY, STREAM_ID, notify.buf);
        }
    }
    */
}

void CStreamRtmpPushSession::handleConnect(double txid, Decoder *dec)
{
    amf_object_t params = amf_load_object(dec);
    std::string app = get(params, std::string("app")).as_string();
    std::string ver = "(unknown)";
    AMFValue flashver = get(params, std::string("flashVer"));
    if (flashver.type() == AMF_STRING) {
        ver = flashver.as_string();
    }

    /* TODO: check the app name
    if (app != APP_NAME) {
        throw std::runtime_error("Unsupported application: " + app);
    }
    */

    SVS_LOG((SVS_LM_DEBUG,"connect: %s (version %s).", app.c_str(), ver.c_str()));

    /* TODO :send setup message to access control */

    amf_object_t version;
    version.insert(std::make_pair("fmsVer", std::string("FMS/4,5,1,484")));
    version.insert(std::make_pair("capabilities", 255.0));
    version.insert(std::make_pair("mode", 1.0));

    amf_object_t status;
    status.insert(std::make_pair("level", std::string("status")));
    status.insert(std::make_pair("code", std::string("NetConnection.Connect.Success")));
    status.insert(std::make_pair("description", std::string("Connection succeeded.")));
    /* report support for AMF3 */
    status.insert(std::make_pair("objectEncoding", 3.0));

    sendRtmpReply(txid, version, status);

    m_enSessionStatus = RTMP_SESSION_STATUS_CONNECTED;
}

void CStreamRtmpPushSession::handleFcpublish(double txid, Decoder *dec)
{
    SVS_LOG((SVS_LM_WARNING,"rtmp session[%u] handle,not a publisher.",m_unSessionIndex));
    return;
    /*
    publisher = client;
    printf("publisher connected.\n");

    amf_load(dec);

    std::string path = amf_load_string(dec);
    debug("fcpublish %s\n", path.c_str());

    amf_object_t status;
    status.insert(std::make_pair("code", std::string("NetStream.Publish.Start")));
    status.insert(std::make_pair("description", path));

    Encoder invoke;
    amf_write(&invoke, std::string("onFCPublish"));
    amf_write(&invoke, 0.0);
    amf_write_null(&invoke);
    amf_write(&invoke, status);
    rtmp_send(client, MSG_INVOKE, CONTROL_ID, invoke.buf);

    send_reply(client, txid);
    */
}

void CStreamRtmpPushSession::handleCreateStream(double txid, Decoder *dec)
{
    /* todo: start the business */
    sendRtmpReply(txid, AMFValue(), double(STREAM_ID));
}

void CStreamRtmpPushSession::handlePublish(double txid, Decoder *dec)
{
    amf_load(dec); /* NULL */

    std::string path = amf_load_string(dec);
    SVS_LOG((SVS_LM_DEBUG,"rtmp session[%u] handle,publish %s.", m_unSessionIndex,path.c_str()));
    m_strUri = path;

    amf_object_t status;
    status.insert(std::make_pair("level", std::string("status")));
    status.insert(std::make_pair("code", std::string("NetStream.Publish.Start")));
    status.insert(std::make_pair("description", std::string("Stream is now published.")));
    status.insert(std::make_pair("details", path));

    Encoder invoke;
    amf_write(&invoke, std::string("onStatus"));
    amf_write(&invoke, 0.0);
    amf_write_null(&invoke);
    amf_write(&invoke, status);
    sendRtmpData(MSG_INVOKE, STREAM_ID, invoke.buf);

    sendRtmpReply(txid);
}

void CStreamRtmpPushSession::handleStartPlayback()
{
    CStreamSession *pPeerSession = NULL;
    CSVSMediaLink MediaLink;
    if (RTMP_SESSION_STATUS_CONNECTED != getStatus())
    {
        close();
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] handle describe req fail, status[%u] invalid.",
                m_unSessionIndex, getStatus()));
        return ;
    }

    int32_t nRet = CSVSMediaLinkFactory::instance().parseMediaUrl(m_strUri,&MediaLink);
    if((SVS_MEDIA_LINK_RESULT_SUCCESS != nRet)
        &&(SVS_MEDIA_LINK_RESULT_AUTH_FAIL != nRet))
    {
        close();
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] handle start play fail, content invalid.",m_unSessionIndex));
        return ;
    }

    if(SVS_MEDIA_LINK_RESULT_AUTH_FAIL == nRet)
    {
        if(CStreamConfig::instance()->getUrlEffectiveWhile())
        {
            close();
            SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] handle start play fail, auth invalid.",m_unSessionIndex));
            return ;
        }
    }

    m_strContentID = MediaLink.ContentID();
    MediaLink.UrlType(PLAY_URL_TYPE_RTMP);

    pPeerSession = CStreamSessionFactory::instance()->findSession(m_strContentID);
    if (NULL == pPeerSession)
    {
        if (RET_OK != sendMediaSetupReq(&MediaLink))
        {
            close();
            SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] handle describe request fail, "
                    "send setup request fail.",
                    m_unSessionIndex));
            return;
        }
        return;
    }

    if (RET_OK != createMediaSession())
    {
        close();
        SVS_LOG((SVS_LM_WARNING,"rtmp push session[%u] handle play request fail, "
                "create media session fail.",
                m_unSessionIndex));
        return;
    }

    if (RET_OK != m_pRtmpSession->startRtmpSession())
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] start media session fail.",
                                m_unSessionIndex));
        return ;
    }


    amf_object_t status;
    status.insert(std::make_pair("level", std::string("status")));
    status.insert(std::make_pair("code", std::string("NetStream.Play.Reset")));
    status.insert(std::make_pair("description", std::string("Resetting and playing stream.")));

    Encoder invoke;
    amf_write(&invoke, std::string("onStatus"));
    amf_write(&invoke, 0.0);
    amf_write_null(&invoke);
    amf_write(&invoke, status);
    sendRtmpData(MSG_INVOKE, STREAM_ID, invoke.buf);

    status.clear();
    status.insert(std::make_pair("level", std::string("status")));
    status.insert(std::make_pair("code", std::string("NetStream.Play.Start")));
    status.insert(std::make_pair("description", std::string("Started playing.")));

    invoke.buf.clear();
    amf_write(&invoke, std::string("onStatus"));
    amf_write(&invoke, 0.0);
    amf_write_null(&invoke);
    amf_write(&invoke, status);
    sendRtmpData(MSG_INVOKE, STREAM_ID, invoke.buf);

    invoke.buf.clear();
    amf_write(&invoke, std::string("|RtmpSampleAccess"));
    amf_write(&invoke, true);
    amf_write(&invoke, true);
    sendRtmpData(MSG_NOTIFY, STREAM_ID, invoke.buf);

    m_bPlaying = true;
    m_bReady   = false;

    // TODO: the onMetaData

    /*if (publisher != NULL) {
        Encoder notify;
        amf_write(&notify, std::string("onMetaData"));
        amf_write_ecma(&notify, metadata);
        sendRtmpData(MSG_NOTIFY, STREAM_ID, notify.buf);
    }*/
    m_enSessionStatus = RTMP_SESSION_STATUS_PLAY;
}

void CStreamRtmpPushSession::handlePlay(double txid, Decoder *dec)
{
    amf_load(dec); /* NULL */

    std::string path = amf_load_string(dec);

    SVS_LOG((SVS_LM_DEBUG,"play %s", path.c_str()));
    m_strUri += "/" + path;

    handleStartPlayback();

    sendRtmpReply(txid);
}

void CStreamRtmpPushSession::handlePlay2(double txid, Decoder *dec)
{
    amf_load(dec); /* NULL */

    amf_object_t params = amf_load_object(dec);
    std::string path = get(params, std::string("streamName")).as_string();

    SVS_LOG((SVS_LM_DEBUG,"play %s", path.c_str()));
    m_strUri += "/" + path;

    handleStartPlayback();

    sendRtmpReply(txid);
}

void CStreamRtmpPushSession::handlePause(double txid, Decoder *dec)
{
    amf_load(dec); /* NULL */

    bool paused = amf_load_boolean(dec);

    if (paused) {
        SVS_LOG((SVS_LM_DEBUG,"pausing"));

        amf_object_t status;
        status.insert(std::make_pair("level", std::string("status")));
        status.insert(std::make_pair("code", std::string("NetStream.Pause.Notify")));
        status.insert(std::make_pair("description", std::string("Pausing.")));

        Encoder invoke;
        amf_write(&invoke, std::string("onStatus"));
        amf_write(&invoke, 0.0);
        amf_write_null(&invoke);
        amf_write(&invoke, status);
        sendRtmpData(MSG_INVOKE, STREAM_ID, invoke.buf);
        m_bPlaying = false;
    } else {
        handleStartPlayback();
    }

    sendRtmpReply(txid);
}


int32_t CStreamRtmpPushSession::checkTransDirection(uint32_t unPeerType, uint32_t unTransDirection) const
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

int32_t CStreamRtmpPushSession::createDistribute(CSVSMediaLink* linkinfo,std::string& strSdpInfo)
{
    std::string strUrl;
    CStreamSession *pPeerSession  = NULL;

    if(NULL == linkinfo)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp push session[%u] create distribute fail,get content fail."));
        return RET_FAIL;
    }
    m_strContentID = linkinfo->ContentID();

    //create the peer session first
    pPeerSession = CStreamSessionFactory::instance()->createSourceSession(m_strContentID, PEER_TYPE_PU, RTP_SESSION,false);
    if (NULL == pPeerSession)
    {
        SVS_LOG((SVS_LM_ERROR,"Create distribute fail, create peer session fail."));
        return RET_FAIL;
    }
    if (RET_OK != pPeerSession->init(m_strContentID.c_str(),linkinfo->PlayType()))
    {
        SVS_LOG((SVS_LM_ERROR,"Create distribute fail,session init fail."));
        CStreamSessionFactory::instance()->releaseSession(pPeerSession);
        return RET_FAIL;
    }

    m_pPeerSession     = pPeerSession;

    /* create the sdp info by device type */
    SDP_MEDIA_INFO info;
    CMediaSdp sdpCreator;

    SVS_DEV_TYPE enDevType  = linkinfo->DevType();
    PLAY_TYPE    enPlayType = linkinfo->PlayType();

    std::string strAddr = pPeerSession->getVideoAddr().get_host_addr();

    std::string strSerID = CStreamConfig::instance()->getServiceId();
    //CStreamSccConnector* pConnect = CStreamServiceTask::instance()->getSccConnector();
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

    }

    sdpCreator.encodeSdp(strSdpInfo,0,"");

    return RET_OK;

    return RET_OK;
}

void CStreamRtmpPushSession::sendRtmpReply(double txid, const AMFValue &reply,const AMFValue &status)
{
    if (txid <= 0.0)
        return;
    Encoder invoke;
    amf_write(&invoke, std::string("_result"));
    amf_write(&invoke, txid);
    amf_write(&invoke, reply);
    amf_write(&invoke, status);
    sendRtmpData(MSG_INVOKE, CONTROL_ID, invoke.buf, 0, CHAN_RESULT);
}
void CStreamRtmpPushSession::simulateSendRtcpMsg()
{
    return;
}

void CStreamRtmpPushSession::sendH264FramebyRtmp(RTP_FRAME_LIST &rtpFrameList)
{
    return;
}
void CStreamRtmpPushSession::sendCommonFramebyRtmp(RTP_FRAME_LIST &rtpFrameList)
{
    return;
}



