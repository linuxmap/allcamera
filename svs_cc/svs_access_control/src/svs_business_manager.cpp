#include "svs_business_manager.h"
#include "svs_log_msg.h"
#include "svs_ac_common.h"
#include "svs_daemon_thread.h"
#include "svs_access_control.h"
#include <vms/vms.h>
#include "svs_timer.h"
#include<string>
#include<algorithm>
#include <cctype>
#include "svs_utility.h"

#include "svs_server_stack.h"
#include "svs_server_manager.h"
#include "svs_media_hot_link.h"

#include "controlstack/svs_control_stack.h"
#include "device_stack/svs_device_stack.h"
#include "access_control_manager/svs_access_control_manager.h"
using namespace std;


CBusiness::CBusiness()
    :m_llTimerID(-1),m_uiWaitTime(0)
{
    m_tLastEventTime = 0;
    SVS_LOG((SVS_LM_INFO, "CBusiness, %x", this));
    m_strDevID       = "";
    m_uiReqHandle    = 0;
    m_usReqTranNum = 0;
    m_uiLocalIndex = 0;

    m_strRtspUrl  = "";

    m_usTransno = 0;
    m_ulSessionID = -1;

}
CBusiness::~CBusiness()
{
    SVS_LOG((SVS_LM_INFO, "~CBusiness, %x", this));
}


void CBusiness::setDevID(std::string strDevID)
{
    m_strDevID = strDevID;
}


std::string CBusiness::getDevID()
{
    return m_strDevID;
}

void CBusiness::setMuSvrID(std::string& strMuSvrID)
{
    m_strMuSvrID = strMuSvrID;
}
std::string CBusiness::getMuSvrID()
{
    return m_strMuSvrID;
}



void CBusiness::setReqHandle(uint32_t uiReqHandle)
{
    m_uiReqHandle = uiReqHandle;
}
uint32_t CBusiness::getReqHandle()
{
    return m_uiReqHandle;
}
void CBusiness::setReqTranNum(uint16_t uiReqTranNum)
{
    m_usReqTranNum = uiReqTranNum;
}
uint16_t CBusiness::getReqTranNum()
{
    return m_usReqTranNum;
}

void CBusiness::setReqLocalIndex(uint32_t uiReqLocalIndex)
{
    m_uiLocalIndex = uiReqLocalIndex;
}
uint32_t CBusiness::getReqLocalIndex()
{
    return m_uiLocalIndex;
}

// ----------------------------------------------------------------------------------------------------------------------------

CRealTimeBusiness::CRealTimeBusiness()
{
    SVS_LOG((SVS_LM_INFO, "CRealTimeBusiness, %x", this));
    m_MsgCached = NULL;
    m_uiDevStreamStatus = EN_DEV_STREAM_STATUS_INIT;
}

CRealTimeBusiness::~CRealTimeBusiness()
{
    SVS_LOG((SVS_LM_INFO, "~CRealTimeBusiness, %x", this));
    /*
    std::map<uint16_t, BusinessSession*>::iterator iter;
    BusinessSession* pBusinessSession = NULL;
    for (iter = m_Transno2SessionMap.begin(); iter != m_Transno2SessionMap.end(); iter++)
    {
        //
        pBusinessSession = (BusinessSession*)iter->second;
        delete pBusinessSession;
    }

    m_Transno2SessionMap.clear();
    */

    if (NULL != m_MsgCached)
    {
        freeMessageBlock(m_MsgCached);
    }
}

void CRealTimeBusiness::setDevStatus(uint32_t uiDevStatus)
{
    m_uiDevStreamStatus = uiDevStatus;
}

uint32_t CRealTimeBusiness::getDevStatus()
{
    return m_uiDevStreamStatus;
}


bool CRealTimeBusiness::isTimeOut(time_t tCurrentTime)
{
    SVS_TRACE();

    int32_t tTimeDiff = tCurrentTime - m_tLastEventTime;

    if (0 == m_llTimerID)
    {
        return false;
    }

    if (tTimeDiff < (int32_t)m_uiWaitTime)
    {
        return false;
    }

    return true;
}

int32_t CRealTimeBusiness::on_timer(time_t tCurrentTime, int32_t eventId, int32_t timerId)
{
    SVS_TRACE();

    int32_t tTimeDiff = tCurrentTime - m_tLastEventTime;
    CStreamServer*          pCStreamServer = NULL;
    CBusinessManager*     pCBusinessManager = &CBusinessManager::instance();
    SVS_LOG((SVS_LM_DEBUG, "RealTimeBusiness timer. "));
    if (0 != m_llTimerID)
    {
        if (tTimeDiff >= (int32_t)m_uiWaitTime)
        {
            SVS_LOG((SVS_LM_WARNING,
                             "Session timeout, "
                             "DeviceID=%s, status=%d, waittime=%d.",
                             m_strDevID.c_str(), m_uiDevStreamStatus, m_uiWaitTime));
            do
            {
                // check if come from the same mu server.
                CServerManager* pCServerManager = &CServerManager::instance();
                CServer* pCServer = NULL;
                pCServer = pCServerManager->findServer(m_strMuSvrID.c_str());
                if (NULL == pCServer)
                {
                    SVS_LOG((SVS_LM_INFO, "find server by ServerID=%s failed when dealing live video share request.",
                                     m_strMuSvrID.c_str()));
                    break;
                }

                pCStreamServer = (CStreamServer*)pCServer;
                pCStreamServer->freeStreamforRealtime(m_strDevID.c_str());

                pCServerManager->ReleaseServer(pCServer);
            }while(0);

            // send tear down response message.
            if (EN_DEV_STREAM_STATUS_NOTIFYING_DEV == m_uiDevStreamStatus)
            {
                SVS_LOG((SVS_LM_WARNING,
                                 "Session setup failed, notify device invite timeout, "
                                 "DeviceID=%s, status=%d, waittime=%d.",
                                 m_strDevID.c_str(), m_uiDevStreamStatus, m_uiWaitTime));

                pCBusinessManager->send_mdu_session_setup_resp(-1, m_MsgCached);

                SVS_ACM::REQUEST_SEND_BYE2DEV oByeRequest;
                oByeRequest.nRequestID = m_usTransno;
                strncpy(oByeRequest.szLensID,m_strDevID.c_str(), SVS_DEVICEID_LEN);

                if ( 0 != IAccessControlManager::instance().asyncRequest(oByeRequest, IServerStack::asyncResponse, NULL))
                {
                    SVS_LOG((SVS_LM_WARNING,
                                     "Wait for invite response from device failed, send bye to dev failed, "
                                     "DevID=%s.",
                                     m_strDevID.c_str()));
                    freeMessageBlock(m_MsgCached);
                }
                else
                {
                    SVS_LOG((SVS_LM_INFO,
                                     "Wait for invite response from device failed, send bye to dev success, will not wait for response, "
                                     "DevID=%s.",
                                     m_strDevID.c_str()));
                }

            }
            else if (EN_DEV_STREAM_STATUS_NOTIFYING_STREAM == m_uiDevStreamStatus)
            {
                SVS_ACM::REQUEST_SEND_BYE2DEV oByeRequest;
                oByeRequest.nRequestID = m_usTransno;

                strncpy(oByeRequest.szLensID,m_strDevID.c_str(), SVS_DEVICEID_LEN);

                if ( 0 != IAccessControlManager::instance().asyncRequest(oByeRequest, IServerStack::asyncResponse, NULL))
                {
                    SVS_LOG((SVS_LM_WARNING,
                                     "Wait for play request from mu failed, send bye to dev failed, "
                                     "DevID=%s.",
                                     m_strDevID.c_str()));
                    freeMessageBlock(m_MsgCached);
                }
                else
                {
                    SVS_LOG((SVS_LM_INFO,
                                     "Wait for play request from mu failed, send bye to dev, will not wait for response, "
                                     "DevID=%s.",
                                     m_strDevID.c_str()));
                }
            }
            else if (EN_DEV_STREAM_STATUS_RUNNING < m_uiDevStreamStatus)
            {
                /// send error response message
                pCBusinessManager->send_mu_session_teardown_resp(-1, m_MsgCached);
            }

            if (NULL != m_MsgCached)
            {
                freeMessageBlock(m_MsgCached);
            }
            m_llTimerID = 0;
            m_uiWaitTime = 0;
            m_tLastEventTime = 0;

            return -1;
        }
    }

    return 0;
}

int32_t CRealTimeBusiness::mu_session_setup_realtime(ACE_Message_Block *mb, uint16_t usTransno)
{

    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "deal mu steam real time session setup request, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal mu steam real time session setup request failed, "
                        "Invalid parameter. mb is null."));
        return SVS_ERROR_CODE_FAIL;
    }

    SVS_MSG_STREAM_SESSION_SETUP_REQ* t_pReq
        = (SVS_MSG_STREAM_SESSION_SETUP_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu steam real time session setup request failed, "
                         "Invalid parameter. pReq is null."));
        return SVS_ERROR_CODE_FAIL;
    }

    int32_t                         iRespCode = SVS_ERROR_CODE_FAIL;

    string                          strMuSvrofReq;
    std::string                     t_strRtspUrl;
    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};

    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);
    t_strRtspUrl = (char*)&t_pReq->szUrl[0];

    SVS_LOG((SVS_LM_INFO,
                     "receive mu stream real time session setup request, DevID=%s,status=%d, rtspurl=%s",
                     t_szDevID, m_uiDevStreamStatus,t_strRtspUrl.c_str()));
    do
    {
        if (EN_DEV_STREAM_STATUS_INIT != m_uiDevStreamStatus)
        {
            // check if come from the same mu server.
            CServerManager* pCServerManager = &CServerManager::instance();
            CServer* pCServer = NULL;
            pCServer = pCServerManager->findServer(byte2int(mb));
            if (NULL == pCServer)
            {
                SVS_LOG((SVS_LM_WARNING,
                                 "find server by server queue id:%d failed when dealing live video share request, DeviceID=%s.",
                                 byte2int(mb), t_szDevID));
                // cann't send the response, the server does not exist.
                return SVS_ERROR_CODE_FAIL;
            }

            strMuSvrofReq = pCServer->getServerID();

            // server need to remember the stream info.
            // pCStreamServer = (CStreamServer*)pCServer;
            if ((!m_strMuSvrID.empty()) && (m_strMuSvrID != strMuSvrofReq))
            {
                // not the same server just send error response
                pCServerManager->ReleaseServer(pCServer);
                SVS_LOG((SVS_LM_WARNING,
                                 "receive mu stream real time session setup request, but from a different mu, "
                                 "CurrentMu=%s, NewMu=%s, DevID=%s, rtspurl=%s.",
                                 m_strMuSvrID.c_str(), strMuSvrofReq.c_str(), t_szDevID, t_strRtspUrl.c_str()));
                return -2;
            }
            // donn't need to alloc stream.

            pCServerManager->ReleaseServer(pCServer);

            iRespCode = 0;

            SVS_LOG((SVS_LM_INFO,
                             "receive mu stream real time session setup request, but from the same mu, "
                             "Mu=%s, regMuID=%s,DevID=%s, rtspurl=%s.",
                             m_strMuSvrID.c_str(), strMuSvrofReq.c_str(), t_szDevID, t_strRtspUrl.c_str()));
            return iRespCode;  // just send success response to mu.
        }

        m_usTransno = usTransno;

        SVS_ACM::REQUEST_SEND_INVITE2DEV oDevMediaRequest;
        oDevMediaRequest.nRequestID = m_usTransno;

        memcpy(oDevMediaRequest.szLensID, t_szDevID, SVS_DEVICEID_LEN);
        oDevMediaRequest.eCallType = t_pReq->CallType;
        oDevMediaRequest.eUrlType = t_pReq->UrlType;
        oDevMediaRequest.ePlayType = t_pReq->PlayType;
        oDevMediaRequest.eStreamtype = t_pReq->eStreamtype;
        oDevMediaRequest.UrlLen = t_pReq->UrlLen;
        memcpy(oDevMediaRequest.szUrl, t_pReq->szUrl, RTSP_URL_LEN);
        oDevMediaRequest.SdpLen = t_pReq->SdpLen;
        memcpy(oDevMediaRequest.szSdp, t_pReq->szSdp, SDP_MSG_LENS);
        strncpy(oDevMediaRequest.szMuSvrID, m_strMuSvrID.c_str(), SERVER_ID_LEN);
        oDevMediaRequest.MediaLinkMode = t_pReq->MediaLinkMode;
        memcpy(oDevMediaRequest.szMediaIP, t_pReq->szMediaIP, SVS_IP_LEN);
        oDevMediaRequest.usMediaPort   = t_pReq->usMediaPort;

        if ( 0 != IAccessControlManager::instance().asyncRequest(oDevMediaRequest, IServerStack::asyncResponse, NULL))
        {
            SVS_LOG((SVS_LM_ERROR,
                             "receive mu stream real time session setup request, send invite to device failed, "
                             "DevID=%s, rtspurl=%s.",
                              t_szDevID, t_strRtspUrl.c_str()));
            return SVS_ERROR_CODE_FAIL;  // send error response to mu
        }

        m_llTimerID         = m_usTransno;
        m_uiWaitTime        = BUSINESS_TIME_OUT;
        m_uiDevStreamStatus = EN_DEV_STREAM_STATUS_NOTIFYING_DEV;
        (void)time(&m_tLastEventTime);

        m_MsgCached = mb;   // save the request.

        SVS_LOG((SVS_LM_INFO,
                         "receive mu stream real time session setup request, send invite to device, will wait for response, "
                         "DevID=%s, rtspurl=%s.",
                          t_szDevID, t_strRtspUrl.c_str()));

         return 1;  // donn't not release the msg block.
    }    while(0);

    /// send error response message
    CBusinessManager* pCBusinessManager = &CBusinessManager::instance();

    pCBusinessManager->send_mdu_session_setup_resp(iRespCode, mb);

    return SVS_ERROR_CODE_FAIL;
}


int32_t CRealTimeBusiness::mu_session_play_realtime(ACE_Message_Block *mb)
{

    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG, "deal mu steam real time session play request, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal mu steam real time session play request failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    SVS_MSG_STREAM_SESSION_PLAY_REQ* t_pReq
        = (SVS_MSG_STREAM_SESSION_PLAY_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu steam real time session play request failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    int32_t                         iRespCode = -1;
    // CStreamServer*          pCStreamServer = NULL;
    string                          strMuSvrofReq;
    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_INFO,
                     "receive mu stream real time session play request, DevID=%s, StreamStatus=%d.",
                     t_szDevID, m_uiDevStreamStatus));
    do
    {
        if (EN_DEV_STREAM_STATUS_NOTIFYING_STREAM != m_uiDevStreamStatus)
        {
            // not the right status.
            SVS_LOG((SVS_LM_WARNING,
                             "receive mu stream real time session play request, stream status not right, DevID=%s, StreamStatus=%d.",
                             t_szDevID, m_uiDevStreamStatus));

            if (EN_DEV_STREAM_STATUS_RUNNING == m_uiDevStreamStatus)
            {
                iRespCode = 0;
                SVS_LOG((SVS_LM_INFO,
                                 "receive mu stream real time session play request, stream status is already running, "
                                 "just send success reponse, "
                                 "DevID=%s, StreamStatus=%d.",
                                 t_szDevID, m_uiDevStreamStatus));
            }
            break;
        }
        //

        {
            // check if come from the same mu server.
            CServerManager* pCServerManager = &CServerManager::instance();
            CServer* pCServer = NULL;
            pCServer = pCServerManager->findServer(byte2int(mb));
            if (NULL == pCServer)
            {
                SVS_LOG((SVS_LM_WARNING,
                                 "find server by server queue id:%d failed when dealing session play request, DeviceID=%s.",
                                 byte2int(mb), t_szDevID));
                // cann't send the response, the server does not exist.
                break;
            }

            strMuSvrofReq = pCServer->getServerID();

            pCServerManager->ReleaseServer(pCServer);

            if ((!m_strMuSvrID.empty()) && (m_strMuSvrID != strMuSvrofReq))
            {
                // not the same server just send error response
                SVS_LOG((SVS_LM_WARNING,
                                 "receive mu stream real time session play request, but from a different mu, "
                                 "CurrentMu=%s, NewMu=%s, DevID=%s.",
                                 m_strMuSvrID.c_str(), strMuSvrofReq.c_str(), t_szDevID));
                break;
            }
        }

        SVS_ACM::REQUEST_SEND_ACK2DEV oAckRequest;
        oAckRequest.nRequestID = m_usTransno;
        oAckRequest.lSessionID = t_pReq->SessionID;

        memcpy(oAckRequest.szLensID, t_szDevID, SVS_DEVICEID_LEN);

        if ( 0 != IAccessControlManager::instance().asyncRequest(oAckRequest, IServerStack::asyncResponse, NULL))
        {
            SVS_LOG((SVS_LM_ERROR,
                             "receive mu stream real time session play request, send ack to device failed, "
                             "DevID=%s.",
                              t_szDevID));

            iRespCode = -1;
            break;
        }

        iRespCode = 0;
        m_uiDevStreamStatus = EN_DEV_STREAM_STATUS_RUNNING;

        // reset the timer.
        m_llTimerID = 0;
        m_uiWaitTime = 0;

        SVS_LOG((SVS_LM_INFO,
                         "receive mu stream real time session play request, send ack to device, no need to wait for response, "
                         "stream status change to running, "
                         "DevID=%s, StreamStatus=%d.",
                          t_szDevID, m_uiDevStreamStatus));

    }    while(0);

    /// send error response message
    // CBusinessManager* pCBusinessManager = &CBusinessManager::instance();

    // pCBusinessManager->send_mdu_session_play_resp(iRespCode, mb);

    return iRespCode;
}


int32_t CRealTimeBusiness::mu_media_keyframe_req(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG, "deal mu media keyframe request, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal mu media keyframe request failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    SVS_MSG_STREAM_KEY_FRAME_REQ* t_pReq
        = (SVS_MSG_STREAM_KEY_FRAME_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu media keyframe request failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    int32_t                         iRespCode = -1;
    // CStreamServer*          pCStreamServer = NULL;
    string                          strMuSvrofReq;
    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_INFO,
                     "receive mu media keyframe request, DevID=%s, StreamStatus=%d.",
                     t_szDevID, m_uiDevStreamStatus));
    do
    {
        if (EN_DEV_STREAM_STATUS_INIT >=  m_uiDevStreamStatus)
        {
            // not the right status.
            SVS_LOG((SVS_LM_WARNING,
                             "receive mu media keyframe request, stream status not right, DevID=%s, StreamStatus=%d.",
                             t_szDevID, m_uiDevStreamStatus));
            break;
        }


        {
            // check if come from the same mu server.
            CServerManager* pCServerManager = &CServerManager::instance();
            CServer* pCServer = NULL;
            pCServer = pCServerManager->findServer(byte2int(mb));
            if (NULL == pCServer)
            {
                SVS_LOG((SVS_LM_WARNING,
                                 "find server by server queue id:%d failed when dealing mu media keyframe request, DeviceID=%s.",
                                 byte2int(mb), t_szDevID));
                break;
            }

            strMuSvrofReq = pCServer->getServerID();

            // server need to remember the stream info.
            // pCStreamServer = (CStreamServer*)pCServer;
            pCServerManager->ReleaseServer(pCServer);

            if ((!m_strMuSvrID.empty()) && (m_strMuSvrID != strMuSvrofReq))
            {
                // not the same server just send error response
                SVS_LOG((SVS_LM_WARNING,
                                 "receive mu media keyframe request, but from a different mu, "
                                 "CurrentMu=%s, NewMu=%s, DevID=%s.",
                                 m_strMuSvrID.c_str(), strMuSvrofReq.c_str(), t_szDevID));
                break;
            }
        }

        SVS_ACM::REQUEST_SEND_KEYFRAME2DEV oKeyFrameRequest;
        oKeyFrameRequest.nRequestID = m_usTransno;
        KEY_FRAME_USER_DATA* pUserData = new KEY_FRAME_USER_DATA;
        memset(pUserData, 0, sizeof(KEY_FRAME_USER_DATA));

        pUserData->uiServerIndex = byte2int(mb);
        memcpy(&pUserData->stRequestHeader, &t_pReq->MsgHeader, sizeof(SVS_MSG_HEADER));

        strncpy(oKeyFrameRequest.szLensID, t_szDevID, SVS_DEVICEID_LEN);
        strncpy((char*)pUserData->DeviceID, t_szDevID, DEVICE_ID_LEN);

        if ( 0 != IAccessControlManager::instance().asyncRequest(oKeyFrameRequest, IServerStack::asyncResponse, pUserData))
        {
            SVS_LOG((SVS_LM_ERROR,
                             "receive mu media keyframe request, send keyframe request to device failed, "
                             "DevID=%s.",
                              t_szDevID));

            iRespCode = -1;
            break;
        }

        iRespCode = 0;

        SVS_LOG((SVS_LM_INFO,
                         "receive mu media keyframe request, send keyframe request to device success, "
                         "DevID=%s, StreamStatus=%d.",
                          t_szDevID, m_uiDevStreamStatus));

    }    while(0);

    return iRespCode;
}

int32_t CRealTimeBusiness::http_report_response(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "deal http report response message, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal http report response message failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    AC_INTER_MSG_HTTP_REPORT_RESP* t_pResp
        = (AC_INTER_MSG_HTTP_REPORT_RESP*)(void*)(mb->rd_ptr());
    if (NULL == t_pResp)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal http report response message failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }


    // BusinessSession* pBusinessSession = NULL;
    // int32_t                         iRespCode = -1;
    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};

    ACE_OS::memcpy(t_szDevID,  (const char*)t_pResp->DeviceID, DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_INFO,
                     "deal http report response message, DeviceID=%s.",
                     t_szDevID));

    return 0;
}


int32_t CRealTimeBusiness::notify_dev_start_stream_response(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG, "deal notify device send stream response message, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal notify device send stream response message failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    AC_INTER_MSG_DEV_INVITE_RESP* t_pResp
        = (AC_INTER_MSG_DEV_INVITE_RESP*)(void*)(mb->rd_ptr());
    if (NULL == t_pResp)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal notify device send stream response message failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    char  t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID,  (const char*)t_pResp->DeviceID, DEVICE_ID_LEN);

    m_ulSessionID = t_pResp->SessionID;// for ehome

    SVS_LOG((SVS_LM_INFO,
                     "deal notify device send stream response message,status=%d, DeviceID=%s, RespCode:%d.",
                     m_uiDevStreamStatus, t_szDevID, t_pResp->RespCode));

    CBusinessManager* pCBusinessManager = &CBusinessManager::instance();

    if ((EN_DEV_STREAM_STATUS_NOTIFYING_DEV == m_uiDevStreamStatus)
        && (NULL != m_MsgCached))
    {
        // send the mu setup response.
        SVS_LOG((SVS_LM_INFO,
                     "deal notify device send stream response message,send setup response to stream."));
        pCBusinessManager->send_mdu_session_setup_resp(t_pResp->RespCode, m_MsgCached, mb);
    }

    m_llTimerID = 0;
    m_uiWaitTime = 0;

    if (0 == t_pResp->RespCode)
    {
        m_uiDevStreamStatus = EN_DEV_STREAM_STATUS_NOTIFYING_STREAM;

        m_llTimerID         = m_usTransno;
        m_uiWaitTime        = BUSINESS_TIME_OUT;
         (void)time(&m_tLastEventTime);

        SVS_LOG((SVS_LM_INFO,
                         "deal notify device send stream response message, stream status change to notifying stream,"
                         "DeviceID=%s, status=%d, RespCode:%d.",
                         t_szDevID, m_uiDevStreamStatus, t_pResp->RespCode));
        freeMessageBlock(m_MsgCached);
        return 0;
    }

    do
    {
        CStreamServer*          pCStreamServer = NULL;
        CServerManager*        pCServerManager = &CServerManager::instance();
        CServer*                    pCServer = NULL;
        pCServer = pCServerManager->findServer(m_strMuSvrID.c_str());
        if (NULL == pCServer)
        {
            SVS_LOG((SVS_LM_INFO, "find server by ServerID=%s failed .",
                             m_strMuSvrID.c_str()));
            break;
        }

        // server need to remember the stream info.
        pCStreamServer = (CStreamServer*)pCServer;
        // donn't need to alloc stream.
        pCStreamServer->freeStreamforRealtime(m_strDevID.c_str());

        pCServerManager->ReleaseServer(pCServer);

    }while(0);

    SVS_LOG((SVS_LM_ERROR,
                     "deal notify device send stream response message, failed to noitfy dev, "
                     "DeviceID=%s, RespCode:%d.",
                     t_szDevID, t_pResp->RespCode));

    freeMessageBlock(m_MsgCached);
    return -1;
}

int32_t CRealTimeBusiness::mu_session_teardown_req(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "deal mu session teardown request, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal mu session teardown request failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    SVS_MSG_STREAM_SESSION_TEARDOWN_REQ* t_pReq
        = (SVS_MSG_STREAM_SESSION_TEARDOWN_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu session teardown request failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    int32_t                         iRespCode = -1;
    string                          strMuSvrofReq;
    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};

    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_INFO,
                     "receive mu session teardown request, DevID=%s.",
                     t_szDevID));

    do
    {
        // check if come from the same mu server.
        CServerManager* pCServerManager = &CServerManager::instance();
        CServer* pCServer = NULL;
        pCServer = pCServerManager->findServer(byte2int(mb));
        if (NULL == pCServer)
        {
            SVS_LOG((SVS_LM_INFO, "find server by server queue id:%d failed when dealing live video share request.",
                      byte2int(mb)));
            // cann't send the response, the server does not exist.

            break;
        }

        strMuSvrofReq = pCServer->getServerID();

        if (m_strMuSvrID != strMuSvrofReq)
        {
            // just send error response
            pCServerManager->ReleaseServer(pCServer);
            SVS_LOG((SVS_LM_WARNING,
                             "receive mu session teardown request, but from a different mu, "
                             "CurrentMu=%s, NewMu=%s, DevID=%s.",
                             m_strMuSvrID.c_str(), strMuSvrofReq.c_str(), t_szDevID));
            break;
        }

        CStreamServer*          pCStreamServer = NULL;
        pCStreamServer = (CStreamServer*)pCServer;
        pCStreamServer->freeStreamforRealtime(m_strDevID.c_str());

        pCServerManager->ReleaseServer(pCServer);

        if (EN_DEV_STREAM_STATUS_NOTIFYING_STREAM <= m_uiDevStreamStatus)
        {
            SVS_ACM::REQUEST_SEND_BYE2DEV oByeRequest;
            oByeRequest.nRequestID = m_usTransno;

            oByeRequest.lSessionID = t_pReq->SessionID;

            strncpy(oByeRequest.szLensID,m_strDevID.c_str(), SVS_DEVICEID_LEN);

            if ( 0 != IAccessControlManager::instance().asyncRequest(oByeRequest, IServerStack::asyncResponse, NULL))
            {
                SVS_LOG((SVS_LM_WARNING,
                                 "receive mu session teardown request, send bye to dev failed, "
                                 "DevID=%s.",
                                 t_szDevID));
                break;
            }


            /*
            m_llTimerID = m_usTransno;
            m_uiWaitTime = BUSINESS_TIME_OUT;
            (void)time(&m_tLastEventTime);
            */

            // m_uiDevStreamStatus = EN_DEV_STREAM_STATUS_TEARDOWN_NOTIFYING_DEV;
            SVS_LOG((SVS_LM_INFO,
                             "receive mu session teardown request, send bye to dev, will not wait for response, "
                             "DevID=%s.",
                             t_szDevID));
            break;   // donn't wait for response
        }
        else
        {
            // error status
            SVS_LOG((SVS_LM_ERROR,
                             "receive mu session teardown request, device status not right, status=%d, DevID=%s.",
                             m_uiDevStreamStatus, t_szDevID));
            break;
        }
    }    while(0);

    /// send error response message
    CBusinessManager* pCBusinessManager = &CBusinessManager::instance();

    pCBusinessManager->send_mu_session_teardown_resp(iRespCode, mb);

    return -1;
}

int32_t CRealTimeBusiness::notify_dev_stop_stream_response(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG, "deal notify device stop stream response message, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal notify device stop stream response message failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    AC_INTER_MSG_DEV_BYE_RESP* t_pResp
        = (AC_INTER_MSG_DEV_BYE_RESP*)(void*)(mb->rd_ptr());
    if (NULL == t_pResp)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal notify device stop stream response message failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    char t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID,  (const char*)t_pResp->DeviceID, DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_INFO,
                     "deal notify device stop stream response message, DeviceID=%s, RespCode:%d.",
                     t_szDevID, t_pResp->RespCode));

    CBusinessManager* pCBusinessManager = &CBusinessManager::instance();


    if (0 == t_pResp->RespCode)
    {
        // m_uiDevStreamStatus = EN_DEV_STREAM_STATUS_OVER;

        do
        {
            CStreamServer*          pCStreamServer = NULL;
            CServerManager* pCServerManager = &CServerManager::instance();
            CServer* pCServer = NULL;
            pCServer = pCServerManager->findServer(m_strMuSvrID.c_str());
            if (NULL == pCServer)
            {
                SVS_LOG((SVS_LM_INFO, "find server by ServerID=%d failed .",
                          m_strMuSvrID.c_str()));
                // cann't send the response, the server does not exist.
                break;
            }

            // server need to remember the stream info.
            pCStreamServer = (CStreamServer*)pCServer;
            // donn't need to alloc stream.
            pCStreamServer->freeStreamforRealtime(m_strDevID.c_str());

            pCServerManager->ReleaseServer(pCServer);
        }while(0);
        SVS_LOG((SVS_LM_ERROR,
                         "deal notify device send stream response message, failed to noitfy dev, "
                         "DeviceID=%s, RespCode:%d.",
                         t_szDevID, t_pResp->RespCode));
    }
    else    // stop the stream failed.
    {
        m_uiDevStreamStatus = EN_DEV_STREAM_STATUS_RUNNING;
    }

    m_llTimerID = 0;
    m_uiWaitTime = 0;
    m_tLastEventTime = 0;

    /// send response message
    pCBusinessManager->send_mu_session_teardown_resp(t_pResp->RespCode, m_MsgCached);

    freeMessageBlock(m_MsgCached);

    SVS_LOG((SVS_LM_INFO, "deal mu session teardown request, send response success, DeviceID=%s, RespCode=%d.",
                    t_szDevID, t_pResp->RespCode));

    return 0;
}

void CRealTimeBusiness::server_offline()
{
    SVS_LOG((SVS_LM_INFO,
                     "mu server offline, device status=%d, ServerID=%s, DevID=%s.",
                     m_uiDevStreamStatus, m_strMuSvrID.c_str(), m_strDevID.c_str()));

    if (EN_DEV_STREAM_STATUS_RUNNING == m_uiDevStreamStatus)
    {
        SVS_ACM::REQUEST_SEND_BYE2DEV oHttpRequest;
        oHttpRequest.nRequestID = m_usTransno;

        strncpy(oHttpRequest.szLensID,m_strDevID.c_str(), SVS_DEVICEID_LEN);

        if ( 0 != IAccessControlManager::instance().asyncRequest(oHttpRequest, IServerStack::asyncResponse, NULL))
        {
            SVS_LOG((SVS_LM_WARNING,
                             "mu server offline, send bye to dev failed, "
                             "ServerID=%s, DevID=%s.",
                             m_strMuSvrID.c_str(), m_strDevID.c_str()));
            return;
     }

        // m_uiDevStreamStatus = EN_DEV_STREAM_STATUS_TEARDOWN_NOTIFYING_DEV;
        SVS_LOG((SVS_LM_INFO,
                         "mu server offline, send bye to dev, will not wait for response, "
                         "ServerID=%s, DevID=%s.",
                         m_strMuSvrID.c_str(), m_strDevID.c_str()));
        return;
    }

    // error status
    SVS_LOG((SVS_LM_INFO,
                     "mu server offline, device status not running, status=%d, ServerID=%s, DevID=%s.",
                     m_uiDevStreamStatus, m_strMuSvrID.c_str(), m_strDevID.c_str()));
}


CBusinessManager::~CBusinessManager()
{
}


CBusinessManager::CBusinessManager()
{
    m_next_transno = FIRST_TRANS_NO;
    m_iTimerID = -1;
}

int32_t CBusinessManager::initialize()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO, "initialize Business Manager."));

    const ACE_Time_Value tvMessageInterval(MESSAGE_INTERVAL);
    int32_t iTtimerID = CAC_Timer_Manager::instance()->alloc_timer(
        &CBusinessManager::instance(), CBusinessManager::business_timer_callback,
        TIMER_EVENT_ID_BUSINESS_MANAGER, tvMessageInterval,
        tvMessageInterval,
        1);
    if (iTtimerID < 0)
    {
        SVS_LOG((SVS_LM_ERROR, "Create deal timeout business timer failed. interval=%d.", MESSAGE_INTERVAL));
        return - 1;
    }

    m_iTimerID = iTtimerID;

    SVS_LOG((SVS_LM_INFO, "initialize Business Manager over, business timer:%d.", iTtimerID));

    return 0;
}

int32_t CBusinessManager::start()
{
    return 0;
}

int32_t CBusinessManager::stop()
{
    return 0;
}

int32_t CBusinessManager::release()
{
    return 0;
}

uint16_t CBusinessManager::getNextReqTransno()
{
    if (MAX_TRANS_NO == m_next_transno)
    {
        m_next_transno = FIRST_TRANS_NO;
    }

    return m_next_transno++;
}

void CBusinessManager::business_timer_callback(void *pArg, int32_t eventId, int32_t timerId, int32_t nDummy)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG,
                     "business manager timer callback, eventId=%d, timerId=%d, nDummy=%d. ",
                     eventId, timerId, nDummy));

    if (NULL == pArg)
    {
        SVS_LOG((SVS_LM_ERROR, "Invalid parameter. pArg is NULL."));
        return;
    }
    CBusinessManager* pCBusinessManager = (CBusinessManager*)pArg;

    pCBusinessManager->on_timer(eventId, timerId);
}

void CBusinessManager::on_timer(int32_t eventId, int32_t timerId)
{
    std::time_t tCurrentTime;
    (void)time(&tCurrentTime);
    CBusiness* pCBusiness = NULL;

    SVS_LOG((SVS_LM_DEBUG, "business manager timer timeout. "));

    // int32_t iRespCode = 0;

    switch (eventId)
    {
        case TIMER_EVENT_ID_BUSINESS_MANAGER:
        {
            // get current time and check if the business is timeout.
            // deal all the business here, then no lock is need in CBusiness object.

            // 1.if the business object status is
            // 2.
            ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr);

            std::pair<DEV2BUSINESS_MAP::iterator,bool> ret;

            DEV2BUSINESS_MAP::iterator iter;

            for (iter = m_mapDev2RealtimeBusiness.begin(); iter != m_mapDev2RealtimeBusiness.end();)
            {
                pCBusiness = iter->second;
                if (!pCBusiness->isTimeOut(tCurrentTime))
                {
                    iter++;
                    continue;
                }

                if (0 != pCBusiness->on_timer(tCurrentTime, eventId, timerId))
                {
                    //
                    delete pCBusiness;
                    pCBusiness = NULL;
                    m_mapDev2RealtimeBusiness.erase(iter++);
                }
                else
                {
                    iter++;
                }
            }

            break;
        }
        default:
        {
            break;
        }
    }
}

// read time video request
int32_t CBusinessManager::get_url_req(
    const string& strParaDevID, PLAY_TYPE enParaPlayType, PLAY_URL_TYPE enParaPlayUrlType,
    DEV_STREAM_TYPE enParaStreamType, SVS_DEV_TYPE enParaDevType, string& strOutParaUrl)
{
    // char szUrlBody[MEDIA_URL_LEN+1] = {0};

    // uint32_t    uiStreamPort = 0;
    uint32_t    uiRtspPort = 0;
    uint32_t    uiHlsPort = 0;
    uint32_t    uiRtmpPort = 0;

    SVS_LOG((SVS_LM_INFO,
                    "deal get url requrest,DevID=%s, PlayType:%d, PlayUrlType:%d, StreamType:%d, DevType:%d, ",
                     strParaDevID.c_str(), (uint32_t)enParaPlayType, (uint32_t)enParaPlayUrlType, enParaStreamType,
                     (uint32_t)enParaDevType));

    std::string strStreamSvrID;
    std::string strMedisServerIP;
    CSVSMediaLink  oLinkInfo;

    // TODO: now only support realtime url.

    // check if already exist, try to get exist dev streamsvr
    CServerManager* pCServerManager = &CServerManager::instance();

    CServer* pServer = NULL;
    pServer = pCServerManager->allocMuServer(enParaPlayType, strParaDevID.c_str());

    if (NULL == pServer)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "allocat  server failed when get url request."));
        return SVS_RESPCODE_OTHER_ERROR;
    }

    strStreamSvrID = pServer->getServerID();
    strMedisServerIP = pServer->getMediaServerIP();
    uiRtspPort = pServer->getMediaRtspPort();
    uiHlsPort = pServer->getMediaHlsPort();
    uiRtmpPort = pServer->getMediaRtmpPort();

    (void)pServer->decReference();

    switch(enParaPlayUrlType)
    {
        case PLAY_URL_TYPE_RTSP:
            oLinkInfo.Port(uiRtspPort);
            break;
        case PLAY_URL_TYPE_HLS:
            oLinkInfo.Port(uiHlsPort);
            break;
        case PLAY_URL_TYPE_RTMP:
            oLinkInfo.Port(uiRtmpPort);
            break;
        default:
             oLinkInfo.Port(uiRtspPort);
    };

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr, -1);

    SVS_LOG((SVS_LM_INFO,
                     "rtsp play req. "
                     "DevID=%s, StreamSvrID:%s.",
                     strParaDevID.c_str(), strStreamSvrID.c_str()));

    oLinkInfo.UrlType(enParaPlayUrlType);
    oLinkInfo.ContentID(strParaDevID);
    oLinkInfo.PlayType(enParaPlayType);
    oLinkInfo.DevType(enParaDevType);
    oLinkInfo.ServerIP(strMedisServerIP);
    oLinkInfo.StreamType(enParaStreamType);

    int32_t iRet = 0;
    iRet = CSVSMediaLinkFactory::instance().makeMediaUrl( &oLinkInfo,strOutParaUrl);

    if (0 != iRet)
    {
        SVS_LOG((SVS_LM_INFO, "deal get url requrest, make media url failed, "
                        "DevID=%s.",
                         strParaDevID.c_str()));
        return SVS_RESPCODE_OTHER_ERROR;
    }

    SVS_LOG((SVS_LM_INFO, "deal get url requrest success, "
                    "DevID=%s, Url:%s",
                     strParaDevID.c_str(), strOutParaUrl.c_str()));

    return SVS_RESPCODE_SUCCESS;
}


int32_t CBusinessManager::mu_session_setup_req(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "deal mu stream session setup request, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal mu stream  session setup request failed , "
                        "nvalid parameter. mb is null."));
        return 0;
    }

    SVS_MSG_STREAM_SESSION_SETUP_REQ* t_pReq
        = (SVS_MSG_STREAM_SESSION_SETUP_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu stream  session setup request failed, Invalid parameter. pReq is null."));
        return 0;
    }

    std::string t_strRtspUrl;

    t_strRtspUrl = (char*)&t_pReq->szUrl[0];

    SVS_LOG((SVS_LM_INFO, "receive mu stream  session setup request, PlayType=%d, UrlType=%d, rtspurl=%s",
                t_pReq->PlayType, t_pReq->UrlType, t_strRtspUrl.c_str()));

    if (PLAY_TYPE_LIVE == t_pReq->PlayType)
    {
        return mu_session_setup_realtime(mb);
    }

    SVS_LOG((SVS_LM_INFO, "deal mu stream  session setup request failed, "
       "unknown request type, PlayType=%d, UrlType=%d, rtspurl=%s",
       t_pReq->PlayType, t_pReq->UrlType, t_strRtspUrl.c_str()));

    return 0;
}


int32_t CBusinessManager::mu_session_setup_realtime(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "deal mu steam real time session setup request, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal mu steam real time session setup request failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    SVS_MSG_STREAM_SESSION_SETUP_REQ* t_pReq
        = (SVS_MSG_STREAM_SESSION_SETUP_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu steam real time session setup request failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    int32_t                iRespCode = -1;
    CBusiness*             pCBusiness = NULL;
    CRealTimeBusiness*     pCRealTimeBusiness = NULL;
    CStreamServer*         pCStreamServer = NULL;

    string                 strMuSvrofReq;
    std::string            t_strRtspUrl;
    char                   t_szDevID[DEVICE_ID_LEN+1] = {0};
    int32_t                iRet = 0;

    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);
    t_strRtspUrl = (char*)&t_pReq->szUrl[0];

    SVS_LOG((SVS_LM_DEBUG,
                     "receive mu stream real time session setup request, DevID=%s, rtspurl=%s",
                     t_szDevID, t_strRtspUrl.c_str()));

    do
    {
        std::pair<DEV2BUSINESS_MAP::iterator,bool> ret;

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr, 0);

        uint16_t t_usTransno = getNextReqTransno();

        DEV2BUSINESS_MAP::iterator iter =  m_mapDev2RealtimeBusiness.find(std::string(t_szDevID));

        if (iter != m_mapDev2RealtimeBusiness.end())
        {
            // already exist, just report to http server.

            SVS_LOG((SVS_LM_DEBUG,"deal real time session setup request,session exist, DevID=%s, rtspurl=%s",
                                 t_szDevID, t_strRtspUrl.c_str()));
            pCBusiness = iter->second;
            pCRealTimeBusiness = (CRealTimeBusiness*)pCBusiness;

            // just send the http report to the http server and wait for the response.
            return pCRealTimeBusiness->mu_session_setup_realtime(mb, t_usTransno);
        }

        SVS_LOG((SVS_LM_DEBUG,"deal real time session setup request,new session, DevID=%s, rtspurl=%s",
                                 t_szDevID, t_strRtspUrl.c_str()));

        pCRealTimeBusiness = SVS_NEW(pCRealTimeBusiness);
        if(NULL == pCRealTimeBusiness)
        {
            break;
        }
        pCBusiness = pCRealTimeBusiness;

        // store the business info.
        ret = m_mapDev2RealtimeBusiness.insert(DEV2BUSINESS_MAP::value_type(std::string(t_szDevID), pCBusiness));
        if (true != ret.second)
        {
            break;
        }

        pCRealTimeBusiness->setDevID(t_szDevID);
        pCRealTimeBusiness->setReqHandle(byte2int(mb));
        pCRealTimeBusiness->setReqTranNum(t_pReq->MsgHeader.TransactionNo);
        pCRealTimeBusiness->setReqLocalIndex(t_pReq->LocalIndex);
        pCRealTimeBusiness->setRtspUrl(t_strRtspUrl);

        CServerManager* pCServerManager = &CServerManager::instance();

        CServer* pCServer = NULL;
        pCServer = pCServerManager->findServer(byte2int(mb));
        if (NULL == pCServer)
        {
            SVS_LOG((SVS_LM_INFO, "find server by server queue id:%d failed when dealing live video share request.",
                      byte2int(mb)));
            // cann't send the response, the server does not exist.
            delete pCRealTimeBusiness;
            pCRealTimeBusiness  = NULL;
            return -1;
        }
        strMuSvrofReq = pCServer->getServerID();

        pCRealTimeBusiness->setMuSvrID(strMuSvrofReq);
        // server need to remember the stream info.
        pCStreamServer = (CStreamServer*)pCServer;
        pCStreamServer->allocStreamforRealtime(t_szDevID);

        SVS_LOG((SVS_LM_INFO, "deal mu steam session setup request,"
                        "DeviceID=%s, business pointer:%x.",
                        t_szDevID, pCBusiness));

        iRet = pCRealTimeBusiness->mu_session_setup_realtime(mb, t_usTransno);
        if (-1 == iRet)
        {
            pCStreamServer->freeStreamforRealtime(t_szDevID);
            delete pCRealTimeBusiness;
            pCRealTimeBusiness = NULL;
            m_mapDev2RealtimeBusiness.erase(t_szDevID);
            send_mdu_session_setup_resp(iRet, mb);
        }
        else if ((0 == iRet) || (-2 == iRet))   // just send the response
        {
            send_mdu_session_setup_resp(iRet, mb);
        }

        pCServerManager->ReleaseServer(pCServer);

        return iRet;
    }    while(0);

    /// send error response message
    send_mdu_session_setup_resp(iRespCode, mb);
    if(NULL != pCRealTimeBusiness)
    {
        delete pCRealTimeBusiness;
        pCRealTimeBusiness = NULL;
    }
    SVS_LOG((SVS_LM_ERROR, "deal realtime video session setup failed, send error response success, DeviceID=%s .",
                    t_szDevID));

    return -1;
}



int32_t CBusinessManager::mu_session_play_req(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG, "deal mu steam real time session play request, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal mu steam real time session play request failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    SVS_MSG_STREAM_SESSION_PLAY_REQ* t_pReq
        = (SVS_MSG_STREAM_SESSION_PLAY_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu steam real time session play request failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    int32_t                         iRespCode = -1;
    CBusiness*                  pCBusiness = NULL;
    CRealTimeBusiness*     pCRealTimeBusiness = NULL;
    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_INFO,
                     "receive mu stream real time session play request, DevID=%s.",
                     t_szDevID));

    // int32_t                         iRet = -1

    do
    {
        std::pair<DEV2BUSINESS_MAP::iterator,bool> ret;

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr, 0);

        DEV2BUSINESS_MAP::iterator iter =  m_mapDev2RealtimeBusiness.find(std::string(t_szDevID));

        if (m_mapDev2RealtimeBusiness.end() != iter)
        {
            pCBusiness = iter->second;
            pCRealTimeBusiness = (CRealTimeBusiness*)pCBusiness;

            // just send the play to device and send to mu the response.
            iRespCode = pCRealTimeBusiness->mu_session_play_realtime(mb);
            if (0 != iRespCode)
            {
                m_mapDev2RealtimeBusiness.erase(std::string(t_szDevID));
                delete pCRealTimeBusiness;
                pCRealTimeBusiness = NULL;
                SVS_LOG((SVS_LM_WARNING,
                             "deal notify device send stream response message failed, will release the business, DevID=%s",
                             t_szDevID));
            }
            else
            {
                SVS_LOG((SVS_LM_INFO,
                             "deal notify device send stream response message success, DevID=%s",
                             t_szDevID));
            }
        }
        else
        {
            SVS_LOG((SVS_LM_WARNING,
                             "receive mu stream real time session play request, cann't find the business, DevID=%s.",
                             t_szDevID));
        }

    }    while(0);

    /// send error response message
    send_mdu_session_play_resp(iRespCode, mb);

    SVS_LOG((SVS_LM_INFO,
                     "deal realtime video session play over, send response success, DeviceID=%s, RespCode=%d .",
                    t_szDevID, iRespCode));

    return 0;
}

int32_t CBusinessManager::mu_media_keyframe_req(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG, "deal mu media keyframe request, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal mu media keyframe request failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    SVS_MSG_STREAM_KEY_FRAME_REQ* t_pReq
        = (SVS_MSG_STREAM_KEY_FRAME_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu media keyframe request failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    int32_t                         iRespCode = -1;
    CBusiness*                  pCBusiness = NULL;
    CRealTimeBusiness*     pCRealTimeBusiness = NULL;
    uint32_t                        uiServerIndex = byte2int(mb);
    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_INFO,
                     "receive mu media keyframe request, DevID=%s.",
                     t_szDevID));
    do
    {
        std::pair<DEV2BUSINESS_MAP::iterator,bool> ret;

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr, 0);

        DEV2BUSINESS_MAP::iterator iter =  m_mapDev2RealtimeBusiness.find(std::string(t_szDevID));

        if (m_mapDev2RealtimeBusiness.end() != iter)
        {
            pCBusiness = iter->second;
            pCRealTimeBusiness = (CRealTimeBusiness*)pCBusiness;

            // just send the play to device and send to mu the response.
            iRespCode = pCRealTimeBusiness->mu_media_keyframe_req(mb);
            if (0 != iRespCode)
            {
                SVS_LOG((SVS_LM_WARNING,
                             "deal mu media keyframe request success failed, DevID=%s.",
                             t_szDevID));
                break;
            }
            else
            {
                SVS_LOG((SVS_LM_INFO,
                             "deal mu media keyframe request success, DevID=%s",
                             t_szDevID));
                return 0;
            }
        }
        else
        {
            SVS_LOG((SVS_LM_WARNING,
                             "receive mu media keyframe request, cann't find the business, DevID=%s.",
                             t_szDevID));
            break;
        }

    }    while(0);

    /// send error response message
    SVS_MSG_COMMON_RESP stRespMsg;
    memset(&stRespMsg,0,sizeof(SVS_MSG_COMMON_RESP));
    stRespMsg.MsgHeader.ProtocolVersion = t_pReq->MsgHeader.ProtocolVersion;
    stRespMsg.MsgHeader.TransactionNo   = transactionno_respond(t_pReq->MsgHeader.TransactionNo);
    stRespMsg.MsgHeader.PacketLength    = sizeof(SVS_MSG_COMMON_RESP);
    stRespMsg.MsgHeader.MsgType         = SVS_MSG_TYPE_COMMON_RESP;
    stRespMsg.RespCode                  = iRespCode;
    stRespMsg.RequestType       = SVS_MSG_TYPE_MEDIA_KEYFRAME_REQ;

    CServerManager* pCServerManager = &CServerManager::instance();
    iRespCode = pCServerManager->sendMessage(uiServerIndex,(char*)&stRespMsg,sizeof(SVS_MSG_COMMON_RESP));
    if(0 != iRespCode)
    {
        SVS_LOG((SVS_LM_ERROR,"send media key frame response fail, Device=%s, RespCode=%d.",
                         t_szDevID, stRespMsg.RespCode));
        return 0;
    }

    SVS_LOG((SVS_LM_INFO,
                     "send media key frame response success, Device=%s, RespCode=%d.",
                    t_szDevID, stRespMsg.RespCode));

    return 0;
}


int32_t CBusinessManager::mu_media_keyframe_resp(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_DEBUG, "deal mu media keyframe response, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal mu media keyframe response failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    AC_INTER_MSG_STREAM_KEY_FRAME_RESP* t_pReq
        = (AC_INTER_MSG_STREAM_KEY_FRAME_RESP*)(void*)(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu media keyframe response failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    int32_t                         iRespCode = -1;
    uint32_t                        uiServerIndex = t_pReq->ServerIndex;
    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_INFO,
                     "receive mu media keyframe response, DevID=%s, RespCode=%d.",
                     t_szDevID, t_pReq->RespCode));
    do
    {
        std::pair<DEV2BUSINESS_MAP::iterator,bool> ret;

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr, 0);

        DEV2BUSINESS_MAP::iterator iter =  m_mapDev2RealtimeBusiness.find(std::string(t_szDevID));

        if (m_mapDev2RealtimeBusiness.end() != iter)
        {
            // just send success response
            iRespCode = 0;
            break;
        }
        else
        {
            SVS_LOG((SVS_LM_WARNING,
                             "receive mu media keyframe response, cann't find the business, DevID=%s.",
                             t_szDevID));
            break;
        }

    }    while(0);

    /// send error response message
    SVS_MSG_COMMON_RESP stRespMsg;
    memset(&stRespMsg,0,sizeof(SVS_MSG_COMMON_RESP));
    stRespMsg.MsgHeader.ProtocolVersion = t_pReq->ReqMsgHeader.ProtocolVersion;
    stRespMsg.MsgHeader.TransactionNo   = transactionno_respond(t_pReq->ReqMsgHeader.TransactionNo);
    stRespMsg.MsgHeader.PacketLength    = sizeof(SVS_MSG_COMMON_RESP);
    stRespMsg.MsgHeader.MsgType         = SVS_MSG_TYPE_COMMON_RESP;

    if (0 == iRespCode)
    {
        stRespMsg.RespCode                  = t_pReq->RespCode;
    }
    else
    {
        stRespMsg.RespCode                  = iRespCode;
    }
    stRespMsg.RequestType       = t_pReq->ReqMsgHeader.MsgType;

    CServerManager* pCServerManager = &CServerManager::instance();
    iRespCode = pCServerManager->sendMessage(uiServerIndex,(char*)&stRespMsg,sizeof(SVS_MSG_COMMON_RESP));
    if(0 != iRespCode)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "send media key frame response fail when receive mu media keyframe response, "
                         "Device=%s, RespCode=%d.",
                         t_szDevID, stRespMsg.RespCode));
        return 0;
    }

    SVS_LOG((SVS_LM_INFO,
                     "send media key frame response success when receive mu media keyframe response, "
                     "Device=%s, RespCode=%d.",
                    t_szDevID, stRespMsg.RespCode));

    return 0;
}

int32_t CBusinessManager::mu_session_teardown_req(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "deal mu session teardown request start." ));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR, "Invalid parameter. mb is null, pointer:%x.", mb));
        return -1;
    }

    SVS_MSG_STREAM_SESSION_TEARDOWN_REQ* pReq
        = (SVS_MSG_STREAM_SESSION_TEARDOWN_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == pReq)
    {
        SVS_LOG((SVS_LM_ERROR, "Invalid parameter. pReq is null."));
        return -1;
    }

    char  t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID, pReq->DeviceID, DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_INFO, "deal mu session teardown request,DeviceID=%s.", t_szDevID));


    CBusiness* pCBusiness = NULL;
    CRealTimeBusiness*     pCRealTimeBusiness = NULL;

    int32_t    iRet   = -1;

    do
    {
        iRet   = 0;

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr, -1);

        DEV2BUSINESS_MAP::iterator iter = m_mapDev2RealtimeBusiness.find(std::string(t_szDevID));
        if(iter == m_mapDev2RealtimeBusiness.end())
        {
            iRet = -1;
            SVS_LOG((SVS_LM_WARNING,
                             "teardown the bussiness deviceid[%s] is not exist.",
                             t_szDevID));
            break;
        }

        pCBusiness = iter->second;
        pCRealTimeBusiness = (CRealTimeBusiness*)pCBusiness;
        if ((NULL == pCBusiness)
            ||(NULL == pCRealTimeBusiness))
        {
            iRet = -1;
            SVS_LOG((SVS_LM_ERROR,
                             "teardown the bussiness deviceid[%s] failed, the pointer is NULL.",
                             t_szDevID));
            break;
        }

        pCRealTimeBusiness->mu_session_teardown_req(mb);

        m_mapDev2RealtimeBusiness.erase(std::string(t_szDevID));
        delete pCRealTimeBusiness;
        pCRealTimeBusiness = NULL;
        return 0;
    }while(0);

    /// send error response message
    send_mu_session_teardown_resp(-1, mb);

    SVS_LOG((SVS_LM_WARNING, "deal mu session teardown request failed, send error response success, DeviceID=%s .",
                    t_szDevID));
    return -1;
}


int32_t CBusinessManager::http_report_response(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "deal http report response message, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal http report response message failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    AC_INTER_MSG_HTTP_REPORT_RESP* t_pResp
        = (AC_INTER_MSG_HTTP_REPORT_RESP*)(void*)(mb->rd_ptr());
    if (NULL == t_pResp)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal http report response message failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    // int32_t                         iRespCode = -1;
    CBusiness*                  pCBusiness = NULL;
    CRealTimeBusiness*     pCRealTimeBusiness = NULL;


    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    int32_t                        iRet = 0;

    ACE_OS::memcpy(t_szDevID,  (const char*)t_pResp->DeviceID, DEVICE_ID_LEN);
    SVS_LOG((SVS_LM_DEBUG,
                     "deal http report response message, DevID=%s",
                     t_szDevID));

    std::pair<DEV2BUSINESS_MAP::iterator,bool> ret;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr, -1);

    DEV2BUSINESS_MAP::iterator iter =  m_mapDev2RealtimeBusiness.find(std::string(t_szDevID));

    if (m_mapDev2RealtimeBusiness.end() != iter)
    {
        pCBusiness = iter->second;
        pCRealTimeBusiness = (CRealTimeBusiness*)pCBusiness;

        // call business deal method.
        return pCRealTimeBusiness->http_report_response(mb);
    }

    SVS_LOG((SVS_LM_WARNING,
                     "deal http report response message failed, cann't find the business, DevID=%s",
                     t_szDevID));

    return iRet;
}

int32_t CBusinessManager::notify_dev_start_stream_response(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "deal notify device send stream response message, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal notify device send stream response message failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    AC_INTER_MSG_DEV_INVITE_RESP* t_pResp
        = (AC_INTER_MSG_DEV_INVITE_RESP*)(void*)(mb->rd_ptr());
    if (NULL == t_pResp)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal notify device send stream response message failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    // int32_t                         iRespCode = -1;
    CBusiness*             pCBusiness = NULL;
    CRealTimeBusiness*     pCRealTimeBusiness = NULL;


    char                   t_szDevID[DEVICE_ID_LEN+1] = {0};
    int32_t                iRet = 0;


    ACE_OS::memcpy(t_szDevID,  (const char*)t_pResp->DeviceID, DEVICE_ID_LEN);
    SVS_LOG((SVS_LM_DEBUG,
                     "deal notify device send stream response message, DevID=%s",
                     t_szDevID));

    std::pair<DEV2BUSINESS_MAP::iterator,bool> ret;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr, -1);

    DEV2BUSINESS_MAP::iterator iter =  m_mapDev2RealtimeBusiness.find(std::string(t_szDevID));

    if (m_mapDev2RealtimeBusiness.end() != iter)
    {
        pCBusiness = iter->second;
        pCRealTimeBusiness = (CRealTimeBusiness*)pCBusiness;

        // call business deal method.
        iRet = pCRealTimeBusiness->notify_dev_start_stream_response(mb);
        if (0 != iRet)
        {
            m_mapDev2RealtimeBusiness.erase(std::string(t_szDevID));
            delete pCRealTimeBusiness;
            pCRealTimeBusiness = NULL;
            SVS_LOG((SVS_LM_WARNING,
                         "deal notify device send stream response message failed, will release the business, DevID=%s",
                         t_szDevID));
        }
        else
        {
            SVS_LOG((SVS_LM_INFO,
                         "deal notify device send stream response message success, DevID=%s",
                         t_szDevID));
        }

        return iRet;
    }

    SVS_LOG((SVS_LM_WARNING,
                     "deal notify device send stream response message failed, cann't find the business, DevID=%s",
                     t_szDevID));

    return iRet;
}



int32_t CBusinessManager::notify_dev_stop_stream_response(ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "deal notify device stop stream response message, mb:%x.", mb));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
                        "deal notify device stop stream response message failed, "
                        "Invalid parameter. mb is null."));
        return -1;
    }

    AC_INTER_MSG_DEV_BYE_RESP* t_pResp
        = (AC_INTER_MSG_DEV_BYE_RESP*)(void*)(mb->rd_ptr());
    if (NULL == t_pResp)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal notify device stop stream response message failed, "
                         "Invalid parameter. pReq is null."));
        return -1;
    }

    // int32_t                         iRespCode = -1;
    CBusiness*                  pCBusiness = NULL;
    CRealTimeBusiness*     pCRealTimeBusiness = NULL;


    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    int32_t                        iRet = 0;


    ACE_OS::memcpy(t_szDevID,  (const char*)t_pResp->DeviceID, DEVICE_ID_LEN);
    SVS_LOG((SVS_LM_DEBUG,
                     "deal notify device stop stream response message, DevID=%s",
                     t_szDevID));

    std::pair<DEV2BUSINESS_MAP::iterator,bool> ret;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr, -1);

    DEV2BUSINESS_MAP::iterator iter =  m_mapDev2RealtimeBusiness.find(std::string(t_szDevID));

    if (m_mapDev2RealtimeBusiness.end() != iter)
    {
        pCBusiness = iter->second;
        pCRealTimeBusiness = (CRealTimeBusiness*)pCBusiness;

        iRet = pCRealTimeBusiness->notify_dev_stop_stream_response(mb);

        m_mapDev2RealtimeBusiness.erase(std::string(t_szDevID));
        delete pCRealTimeBusiness;
        pCRealTimeBusiness = NULL;

        SVS_LOG((SVS_LM_WARNING,
                         "deal notify device stop stream response message failed, will release the business, DevID=%s",
                         t_szDevID));

        return iRet;
    }

    SVS_LOG((SVS_LM_WARNING,
                     "deal notify device stop stream response message failed, cann't find the business, DevID=%s",
                     t_szDevID));

    return iRet;
}


void CBusinessManager::send_mdu_session_setup_resp(int32_t iRespCode,CBusiness* pBusiness)
{
    if(NULL == pBusiness)
    {
        return;
    }

    // ready to send response
    ACE_Message_Block *mbResopnse = allockMessageBlock(sizeof (SVS_MSG_STREAM_SESSION_SETUP_RESP));
    if (NULL == mbResopnse)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to allocate AceMessageBlock object when send live video share response."));
        return ;
    }

    SVS_MSG_STREAM_SESSION_SETUP_RESP* pResp
        = ((SVS_MSG_STREAM_SESSION_SETUP_RESP*)(void*)mbResopnse->rd_ptr());

    (void)ACE_OS::memset(pResp, 0, sizeof(SVS_MSG_STREAM_SESSION_SETUP_RESP) );
    uint16_t usTransNum = pBusiness->getReqTranNum();
    FillCommonHeader(
        &pResp->MsgHeader,
        SVS_MSG_TYPE_STREAM_SESSION_SETUP_RESP,
        transactionno_respond(usTransNum),
        sizeof(SVS_MSG_STREAM_SESSION_SETUP_RESP));

    pResp->LocalIndex = pBusiness->getReqLocalIndex();
    pResp->RespCode   = iRespCode;

    std::string strURL = pBusiness->getRtspUrl();
    pResp->UrlLen      = strURL.length();
    ACE_OS::strncpy((char*)&pResp->szUrl[0], strURL.c_str(),pResp->UrlLen);
    string t_strDevID = pBusiness->getDevID();
    ACE_OS::strncpy((char*)pResp->DeviceID, t_strDevID.c_str(), DEVICE_ID_LEN);

    mbResopnse->wr_ptr(sizeof(SVS_MSG_STREAM_SESSION_SETUP_RESP));

    // putTmpSendMsg(byte2int(mb), mbResopnse);
    CServerManager* pCServerManager = &CServerManager::instance();
    // int32_t iRet = 0;
    CServer* pCServer = NULL;
    uint32_t ulServerIndex = pBusiness->getReqHandle();
    pCServer = pCServerManager->findServer(ulServerIndex);

    if (NULL == pCServer)
    {
        SVS_LOG((SVS_LM_ERROR, "find server by server queue id:%d failed when dealing live video share request.",
                  ulServerIndex));
        freeMessageBlock(mbResopnse);
        return;
    }

    pCServerManager->sendMessage(ulServerIndex,(char*)pResp, pResp->MsgHeader.PacketLength);

     SVS_LOG((SVS_LM_INFO, "send mu session setup response message, DeviceID=%s.",
                      t_strDevID.c_str()));

    freeMessageBlock(mbResopnse);
    pCServerManager->ReleaseServer(pCServer);

    return;
}


void CBusinessManager::send_mdu_session_setup_resp(
    int32_t iRespCode,ACE_Message_Block* mb, ACE_Message_Block* mbInviteResp)
{
    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR, "send mu session setup response message failed, the message block pointer is NULL."));
        return;
    }

    SVS_MSG_STREAM_SESSION_SETUP_REQ* t_pReq
        = (SVS_MSG_STREAM_SESSION_SETUP_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu steam real time session setup request failed, "
                         "Invalid parameter. pReq is null."));
        return;
    }

    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_INFO, "Try to send mu session setup response message, DeviceID=%s, RespCode=%d.",
                    t_szDevID, iRespCode));

    // ready to send response
    ACE_Message_Block *mbResopnse = allockMessageBlock(sizeof (SVS_MSG_STREAM_SESSION_SETUP_RESP));
    if (NULL == mbResopnse)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to allocate AceMessageBlock object when send live video share response."));
        return ;
    }

    SVS_MSG_STREAM_SESSION_SETUP_RESP* pResp
        = ((SVS_MSG_STREAM_SESSION_SETUP_RESP*)(void*)mbResopnse->rd_ptr());

    (void)ACE_OS::memset(pResp, 0, sizeof(SVS_MSG_STREAM_SESSION_SETUP_RESP) );
    uint16_t usTransNum = t_pReq->MsgHeader.TransactionNo;
    FillCommonHeader(
        &pResp->MsgHeader,
        SVS_MSG_TYPE_STREAM_SESSION_SETUP_RESP,
        transactionno_respond(usTransNum),
        sizeof(SVS_MSG_STREAM_SESSION_SETUP_RESP));

    pResp->LocalIndex = t_pReq->LocalIndex;
    pResp->RespCode = iRespCode;

    pResp->UrlType = t_pReq->UrlType;
    pResp->PlayType = t_pReq->PlayType;

    pResp->UrlLen = t_pReq->UrlLen;
    ACE_OS::memcpy(pResp->szUrl, t_pReq->szUrl, RTSP_URL_LEN);
    if (NULL != mbInviteResp)
    {
        AC_INTER_MSG_DEV_INVITE_RESP* t_pInviteResp
            = (AC_INTER_MSG_DEV_INVITE_RESP*)(void*)(mbInviteResp->rd_ptr());
        pResp->SdpLen = t_pInviteResp->SdpLen;
        ACE_OS::memcpy(pResp->szSdp, t_pInviteResp->szSdp, SDP_MSG_LENS);
        pResp->SessionID = t_pInviteResp->SessionID;
    }
    else
    {
        pResp->SdpLen = t_pReq->SdpLen;
        ACE_OS::memcpy(pResp->szSdp, t_pReq->szSdp, SDP_MSG_LENS);
    }

    mbResopnse->wr_ptr(sizeof(SVS_MSG_STREAM_SESSION_SETUP_RESP));

    CServerManager* pCServerManager = &CServerManager::instance();
    // int32_t iRet = 0;
    CServer* pCServer = NULL;
    uint32_t ulServerIndex = byte2int(mb);
    pCServer = pCServerManager->findServer(ulServerIndex);

    if (NULL == pCServer)
    {
        SVS_LOG((SVS_LM_ERROR, "find server by server queue id:%d failed when dealing live video share request.",
                  ulServerIndex));
        freeMessageBlock(mbResopnse);
        return;
    }

    pCServerManager->sendMessage(ulServerIndex,(char*)pResp, pResp->MsgHeader.PacketLength);
    string strMuSvrofReq = pCServer->getServerID();
    freeMessageBlock(mbResopnse);
    pCServerManager->ReleaseServer(pCServer);


     SVS_LOG((SVS_LM_INFO, "send mu session setup response message success, DeviceID=%s, MuSvrID=%s.",
                      t_szDevID, strMuSvrofReq.c_str()));

    return;
}

void CBusinessManager::send_mdu_session_play_resp(int32_t iRespCode, ACE_Message_Block* mb)
{
    if(NULL == mb)
    {
        return;
    }

    SVS_MSG_STREAM_SESSION_PLAY_REQ* t_pReq
        = (SVS_MSG_STREAM_SESSION_PLAY_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "deal mu steam real time session play request failed, "
                         "Invalid parameter. pReq is null."));
        return;
    }

    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    // ready to send response
    ACE_Message_Block *mbResopnse = allockMessageBlock(sizeof (SVS_MSG_STREAM_SESSION_PLAY_RESP));
    if (NULL == mbResopnse)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to allocate AceMessageBlock object when send mu session play response, "
                         "DeviceID=%s.",
                         t_szDevID));
        return ;
    }

    SVS_MSG_STREAM_SESSION_PLAY_RESP* pResp
        = ((SVS_MSG_STREAM_SESSION_PLAY_RESP*)(void*)mbResopnse->rd_ptr());

    (void)ACE_OS::memset(pResp, 0, sizeof(SVS_MSG_STREAM_SESSION_PLAY_RESP) );
    uint16_t usTransNum = t_pReq->MsgHeader.TransactionNo;
    FillCommonHeader(
        &pResp->MsgHeader,
        SVS_MSG_TYPE_STREAM_SESSION_PLAY_RESP,
        transactionno_respond(usTransNum),
        sizeof(SVS_MSG_STREAM_SESSION_PLAY_RESP));

    pResp->LocalIndex = t_pReq->LocalIndex;
    pResp->UrlType    = t_pReq->UrlType;
    pResp->RespCode = iRespCode;
    ACE_OS::memcpy((char*)pResp->DeviceID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    mbResopnse->wr_ptr(sizeof(SVS_MSG_STREAM_SESSION_PLAY_RESP));

    CServerManager* pCServerManager = &CServerManager::instance();
    CServer* pCServer = NULL;
    uint32_t ulServerIndex = byte2int(mb);
    pCServer = pCServerManager->findServer(ulServerIndex);

    if (NULL == pCServer)
    {
        SVS_LOG((SVS_LM_ERROR, "find server by server queue id:%d failed when send mu session play request, "
                         "DeviceID=%s.",
                         ulServerIndex, t_szDevID));
        freeMessageBlock(mbResopnse);
        return;
    }

    pCServerManager->sendMessage(ulServerIndex,(char*)pResp, pResp->MsgHeader.PacketLength);
    string strMuSvrofReq = pCServer->getServerID();
    freeMessageBlock(mbResopnse);
    pCServerManager->ReleaseServer(pCServer);


     SVS_LOG((SVS_LM_INFO, "send mu session play response message success, DeviceID=%s, MuSvrID=%s.",
                      t_szDevID, strMuSvrofReq.c_str()));

    return;
}
void CBusinessManager::send_mu_session_teardown_resp(int32_t iRespCode,ACE_Message_Block* mb)
{
    if(NULL == mb)
    {
        return;
    }

    SVS_MSG_STREAM_SESSION_TEARDOWN_REQ* t_pReq
        = (SVS_MSG_STREAM_SESSION_TEARDOWN_REQ*)(void*)GetCommonHdr(mb->rd_ptr());
    if (NULL == t_pReq)
    {
        SVS_LOG((SVS_LM_ERROR,
                         "send mu session teardown response failed, "
                         "Invalid parameter. pReq is null."));
        return;
    }

    char                            t_szDevID[DEVICE_ID_LEN+1] = {0};
    ACE_OS::memcpy(t_szDevID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    // ready to send response
    ACE_Message_Block *mbResopnse = allockMessageBlock(sizeof (SVS_MSG_STREAM_SESSION_TEARDOWN_RESP));
    if (NULL == mbResopnse)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to allocate AceMessageBlock object when send mu session teardown response."));
        return ;
    }

    // uint32_t uiVTDUIP     = 0;
    // uint16_t usVTDUPort   = 0;
    // uint32_t uiBusinessID = 0;

    SVS_MSG_STREAM_SESSION_TEARDOWN_RESP* pResp
        = ((SVS_MSG_STREAM_SESSION_TEARDOWN_RESP*)(void*)mbResopnse->rd_ptr());

    (void)ACE_OS::memset(pResp, 0, sizeof(SVS_MSG_STREAM_SESSION_TEARDOWN_RESP) );
    uint16_t usTransNum = t_pReq->MsgHeader.TransactionNo;

    FillCommonHeader(
        &pResp->MsgHeader,
        SVS_MSG_TYPE_STREAM_SESSION_TEARDOWN_RESP,
        transactionno_respond(usTransNum),
        sizeof(SVS_MSG_STREAM_SESSION_TEARDOWN_RESP));

    pResp->LocalIndex = t_pReq->LocalIndex;
    pResp->RespCode = iRespCode;
    ACE_OS::memcpy((char*)pResp->DeviceID,  (const char*)t_pReq->DeviceID, DEVICE_ID_LEN);

    mbResopnse->wr_ptr(sizeof(SVS_MSG_STREAM_SESSION_TEARDOWN_RESP));

    CServerManager* pCServerManager = &CServerManager::instance();
    // int32_t iRet = 0;
    CServer* pCServer = NULL;
    uint32_t ulServerIndex = byte2int(mb);
    pCServer = pCServerManager->findServer(ulServerIndex);

    if (NULL == pCServer)
    {
        SVS_LOG((SVS_LM_ERROR, "find server by server queue id:%d failed send mu session teardown response.",
                  ulServerIndex));
        freeMessageBlock(mbResopnse);
        return;
    }

    pCServerManager->sendMessage(ulServerIndex,(char*)pResp, pResp->MsgHeader.PacketLength);
    string strMuSvrofReq = pCServer->getServerID();
    freeMessageBlock(mbResopnse);
    pCServerManager->ReleaseServer(pCServer);

     SVS_LOG((SVS_LM_INFO, "send mu session teardown response success, DeviceID=%s, MuSvrID=%s.",
                      t_szDevID, strMuSvrofReq.c_str()));

    return;
}

void CBusinessManager::mu_server_offline(const std::string&  strServerID, const std::list<std::string>&  BindDevList)
{
    SVS_LOG((SVS_LM_INFO, "deal server offline, ServerID=%s.", strServerID.c_str()));


    CBusiness* pCBusiness = NULL;
    CRealTimeBusiness*     pCRealTimeBusiness = NULL;

    std::list<std::string>::const_iterator  ListIter;
    DEV2BUSINESS_MAP::iterator   iter;

    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr);

    for (ListIter = BindDevList.begin(); ListIter != BindDevList.end(); ListIter++)
    {
        iter = m_mapDev2RealtimeBusiness.find(*ListIter);
        if(iter == m_mapDev2RealtimeBusiness.end())
        {
            SVS_LOG((SVS_LM_ERROR,
                             "deal server offline, ServerID=%s, deviceid[%s] is not exist.",
                             strServerID.c_str(), (*ListIter).c_str()));
            continue;
        }

        pCBusiness = iter->second;
        pCRealTimeBusiness = (CRealTimeBusiness*)pCBusiness;
        if ((NULL == pCBusiness)
            ||(NULL == pCRealTimeBusiness))
        {
            SVS_LOG((SVS_LM_ERROR,
                             "deal server offline, ServerID=%s, deviceid[%s] failed, the pointer is NULL.",
                             strServerID.c_str(), (*ListIter).c_str()));
            continue;
        }

        pCRealTimeBusiness->server_offline();
        m_mapDev2RealtimeBusiness.erase(*ListIter);
        delete pCRealTimeBusiness;
        pCRealTimeBusiness = NULL;
    }

    SVS_LOG((SVS_LM_INFO,
                     "deal server offline over, ServerID=%s.",
                     strServerID.c_str()));
    return;
}
void CBusinessManager::show_business_info(const std::string& strLensID,std::string& Info)
{
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutexBusMgr);

    DEV2BUSINESS_MAP::iterator iter;
    CRealTimeBusiness* pBusiness = NULL;

    std::string strTmp;

    if(0 < strLensID.length())
    {
        iter = m_mapDev2RealtimeBusiness.find(strLensID);
        if(iter == m_mapDev2RealtimeBusiness.end())
        {
            Info = "not find the bussiness for lens";
            return;
        }
        pBusiness = (CRealTimeBusiness*)iter->second;
        strTmp = pBusiness->getDevID();
        Info += "LensID:" + strTmp + "\r\n";
        strTmp = pBusiness->getMuSvrID();
        Info += "StreamID:" + strTmp + "\r\n";
        strTmp = pBusiness->getRtspUrl();
        Info += "url:" + strTmp + "\r\n";
        return;
    }

    iter = m_mapDev2RealtimeBusiness.begin();
    for(;iter != m_mapDev2RealtimeBusiness.end();++iter)
    {
        pBusiness = (CRealTimeBusiness*)iter->second;
        strTmp = pBusiness->getDevID();
        Info += "LensID:[" + strTmp + "] ";
    }
    return;
}


