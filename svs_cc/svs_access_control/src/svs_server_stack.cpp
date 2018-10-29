
#include "svs_server_stack.h"
#include "svs_server_manager.h"
#include "svs_log_msg.h"
#include "svs_ac_common.h"
#include "svs_daemon_thread.h"
#include "svs_access_control.h"
#include "svs_business_manager.h"
#include "svs_utility.h"
#include "vms/vms.h"
#include "access_control_manager/svs_acm_request.h"

#include "controlstack/svs_control_stack.h"
#include "device_stack/svs_device_stack.h"

int32_t IServerStack::asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_TRACE();


    SVS_ACM::REQ_TYPE t_enType = rRequest.getType();
    SVS_LOG((SVS_LM_DEBUG, "IServerStack::asyncRequest, request type %d .", t_enType));
    switch (t_enType)
    {
        case SVS_ACM::REQ_TYPE_PLAY_URL_REQ:
        {
            SVS_ACM::REQUEST_PLAY_URL_INFO* pPlayUrlInfoRequest = NULL;
            pPlayUrlInfoRequest = (SVS_ACM::REQUEST_PLAY_URL_INFO*)&rRequest;

            SVS_LOG((SVS_LM_INFO,
                             "deal get url asyncReuest, RequestID=%d, DeviceID=%s, PlayType=%d, StreamId=%d.",
                             pPlayUrlInfoRequest->nRequestID, pPlayUrlInfoRequest->szLensID,
                             pPlayUrlInfoRequest->ePlayType, pPlayUrlInfoRequest->enStreamType));

            SVS_ACM::RESPONSE_PLAY_URL_INFO  oResponsePlayUrlInfo;
            oResponsePlayUrlInfo.nRequestID = pPlayUrlInfoRequest->nRequestID;
            oResponsePlayUrlInfo.nResponseCode = SVS_ERROR_FAIL;

            // save the message.
            CBusinessManager* pCBusinessManager = &CBusinessManager::instance();

            std::string     strUrl;
            int32_t iRet = 0;
            iRet = pCBusinessManager->get_url_req(pPlayUrlInfoRequest->szLensID,
                                                  pPlayUrlInfoRequest->ePlayType,
                                                  pPlayUrlInfoRequest->ePlayUrlType,
                                                  pPlayUrlInfoRequest->enStreamType,
                                                  pPlayUrlInfoRequest->eDevType,
                                                  strUrl);
            if (0 != iRet)
            {
                SVS_LOG((SVS_LM_ERROR,
                                 "deal get url failed, will send error response, RequestID=%d, DeviceID=%s, RespCode=%d.",
                                 pPlayUrlInfoRequest->nRequestID, pPlayUrlInfoRequest->szLensID,
                                 iRet));

                // iRet = CAccessControlStack::instance().asyncResponse(oResponsePlayUrlInfo, pUserData);
                CAccessControlStack::instance().asyncResponse(oResponsePlayUrlInfo, pUserData);
                /*
                if ( 0 != iRet)
                {
                    SVS_LOG((SVS_LM_ERROR,
                                     "send get url failed asyncResponse failed, RequestID=%d, DeviceID=%s, RespCode=%d.",
                                     pPlayUrlInfoRequest->nRequestID, pPlayUrlInfoRequest->szLensID,
                                     iRet));
             }
                else
                {
                    SVS_LOG((SVS_LM_INFO,
                                     "send get url failed asyncResponse success, RequestID=%d, DeviceID=%s, RespCode=%d.",
                                     pPlayUrlInfoRequest->nRequestID, pPlayUrlInfoRequest->szLensID,
                                     iRet));
                }
                */
            }
            else
            {
                oResponsePlayUrlInfo.nResponseCode = SVS_ERROR_OK;
                ACE_OS::strncpy(oResponsePlayUrlInfo.szUrl, strUrl.c_str(), MEDIA_URL_LEN);
                SVS_LOG((SVS_LM_INFO,
                                 "deal get url success, will send success response, RequestID=%d, DeviceID=%s, RespCode=%d, Url:%s.",
                                 pPlayUrlInfoRequest->nRequestID, pPlayUrlInfoRequest->szLensID,
                                 iRet, strUrl.c_str()));

                CAccessControlStack::instance().asyncResponse(oResponsePlayUrlInfo, pUserData);
                /*
                if ( 0 != iRet)
                {
                    SVS_LOG((SVS_LM_ERROR,
                                     "send get url success asyncResponse failed, RequestID=%d, DeviceID=%s, RespCode=%d.",
                                     pPlayUrlInfoRequest->nRequestID, pPlayUrlInfoRequest->szLensID,
                                     iRet));
             }
                else
                {
                    SVS_LOG((SVS_LM_INFO,
                                     "send get url success asyncResponse success, RequestID=%d, DeviceID=%s, RespCode=%d.",
                                     pPlayUrlInfoRequest->nRequestID, pPlayUrlInfoRequest->szLensID,
                                     iRet));
                }
                */
            }

            break;
        }
        default:
        {
            SVS_LOG((SVS_LM_ERROR, "IServerStack::asyncRequest, request type %d is invalid.", t_enType));
        }
    }

    return SVS_ERROR_OK;
}

void IServerStack::asyncResponse(SVS_ACM::RESPONSE& rResponse, void* pUserData)
{
    SVS_TRACE();

    /*
    RESP_TYPE_HTTP_REPORT            = 4,
    RESP_TYPE_SEND_INVITE2DEV        = 5,
    RESP_TYPE_SEND_BYE2DEV           = 6,
    */

    switch(rResponse.getType())
    {
        case SVS_ACM::RESP_TYPE_HTTP_REPORT:
        {
            SVS_ACM::RESPONSE_HTTP_REPORT* pReportResp = (SVS_ACM::RESPONSE_HTTP_REPORT*)&rResponse;

            // send response to server manager
            ACE_Message_Block *mbResopnse = allockMessageBlock(sizeof (AC_INTER_MSG_HTTP_REPORT_RESP));
            if (NULL == mbResopnse)
            {
                SVS_LOG((SVS_LM_ERROR, "Fail to allocate AceMessageBlock object when send live video share response."));
                return ;
            }

            SVS_LOG((SVS_LM_INFO,
                             "deal http asyncResponse, ServerID=%s, RespCode=%d.",
                             pReportResp->szServerID, pReportResp->nResponseCode));
            /*
            AC_INTER_MSG_HTTP_REPORT_RESP* pResp
                = ((AC_INTER_MSG_HTTP_REPORT_RESP*)(void*)mbResopnse->rd_ptr());

            (void)ACE_OS::memset(pResp, 0, sizeof(AC_INTER_MSG_HTTP_REPORT_RESP) );
            uint16_t usTransNum = (uint16_t)pReportResp->nRequestID;
            FillCommonHeader(
                &pResp->MsgHeader,
                SVS_MSG_TYPE_AC_INTER_MSG_REPORT_RESP,
                transactionno_respond(usTransNum),
                sizeof(AC_INTER_MSG_HTTP_REPORT_RESP));
            memcpy(pResp->DeviceID, pReportResp->szLensID, SVS_DEVICEID_LEN);

            pResp->RespCode = pReportResp->nResponseCode;

            CServerManager* pCServerManager = &CServerManager::instance();
            pCServerManager->sndMsgToSvrMgr(mbResopnse);
            */

            break;
        }
        case SVS_ACM::RESP_TYPE_SEND_INVITE2DEV:
        {
            SVS_ACM::RESPONSE_SEND_INVITE2DEV* pInviteResp = (SVS_ACM::RESPONSE_SEND_INVITE2DEV*)&rResponse;

            // send response to server manager
            ACE_Message_Block *mbResopnse = allockMessageBlock(sizeof (AC_INTER_MSG_DEV_INVITE_RESP));
            if (NULL == mbResopnse)
            {
                SVS_LOG((SVS_LM_ERROR, "Fail to allocate AceMessageBlock object when send live video share response."));
                return ;
            }

            AC_INTER_MSG_DEV_INVITE_RESP* pResp
                = ((AC_INTER_MSG_DEV_INVITE_RESP*)(void*)mbResopnse->rd_ptr());

            SVS_LOG((SVS_LM_INFO,
                             "deal invite asyncResponse, DeviceID=%s, RespCode=%d,sdp:\n%s.",
                             pInviteResp->szLensID, pInviteResp->nResponseCode,pInviteResp->szSdp));

            (void)ACE_OS::memset(pResp, 0, sizeof(AC_INTER_MSG_DEV_INVITE_RESP) );
            uint16_t usTransNum = (uint16_t)pInviteResp->nRequestID;
            FillCommonHeader(
                &pResp->MsgHeader,
                SVS_MSG_TYPE_AC_INTER_MSG_INVITE_RESP,
                transactionno_respond(usTransNum),
                sizeof(AC_INTER_MSG_DEV_INVITE_RESP));
            memcpy(pResp->DeviceID, pInviteResp->szLensID, SVS_DEVICEID_LEN);
            memcpy(pResp->szSdp, pInviteResp->szSdp, SDP_MSG_LENS);

            pResp->SdpLen = pInviteResp->SdpLen;
            pResp->SessionID = pInviteResp->lSessionID;
            pResp->RespCode = pInviteResp->nResponseCode;

            CServerManager* pCServerManager = &CServerManager::instance();
            pCServerManager->sndMsgToSvrMgr(mbResopnse);

            break;
        }
        case SVS_ACM::RESP_TYPE_SEND_KEYFRAME2DEV:
        {
            SVS_ACM::RESPONSE* pKeyFrameResp = (SVS_ACM::RESPONSE*)&rResponse;

            // send response to server manager
            ACE_Message_Block *mbResopnse = allockMessageBlock(sizeof (AC_INTER_MSG_STREAM_KEY_FRAME_RESP));
            if (NULL == mbResopnse)
            {
                SVS_LOG((SVS_LM_ERROR, "Fail to allocate AceMessageBlock object when send keyframe response."));
                return ;
            }

            if (NULL == pUserData)
            {
                SVS_LOG((SVS_LM_ERROR,
                                "The pointer is NULL, check pUserData pointer failed, when send send keyframe response."));
                return ;
            }

            const KEY_FRAME_USER_DATA* pKeyFrameUserData = (const KEY_FRAME_USER_DATA*)pUserData;

            AC_INTER_MSG_STREAM_KEY_FRAME_RESP* pResp
                = ((AC_INTER_MSG_STREAM_KEY_FRAME_RESP*)(void*)mbResopnse->rd_ptr());

            char    szDeviceID[SVS_DEVICEID_LEN+1] = {0};
            strncpy(szDeviceID, (const char*)pKeyFrameUserData->DeviceID, SVS_DEVICEID_LEN);

            SVS_LOG((SVS_LM_INFO,
                             "deal keyframe asyncResponse, DeviceID=%s, RespCode=%d.",
                             szDeviceID, pKeyFrameResp->nResponseCode));

            (void)ACE_OS::memset(pResp, 0, sizeof(AC_INTER_MSG_STREAM_KEY_FRAME_RESP) );
            uint16_t usTransNum = (uint16_t)pKeyFrameResp->nRequestID;
            FillCommonHeader(
                &pResp->MsgHeader,
                SVS_MSG_TYPE_AC_INTER_MSG_KEYFRAME_RESP,
                transactionno_respond(usTransNum),
                sizeof(AC_INTER_MSG_STREAM_KEY_FRAME_RESP));
            memcpy(pResp->DeviceID, pKeyFrameUserData->DeviceID, SVS_DEVICEID_LEN);
            memcpy(&pResp->ReqMsgHeader, &pKeyFrameUserData->stRequestHeader, SDP_MSG_LENS);

            pResp->ServerIndex = pKeyFrameUserData->uiServerIndex;
            pResp->RespCode = pKeyFrameResp->nResponseCode;

            CServerManager* pCServerManager = &CServerManager::instance();
            pCServerManager->sndMsgToSvrMgr(mbResopnse);

            delete pKeyFrameUserData;

            pKeyFrameUserData = NULL;
            break;
        }
        case SVS_ACM::RESP_TYPE_SEND_BYE2DEV:
        {
            SVS_ACM::RESPONSE_SEND_BYE2DEV* pByeResp = (SVS_ACM::RESPONSE_SEND_BYE2DEV*)&rResponse;

            // send response to server manager
            ACE_Message_Block *mbResopnse = allockMessageBlock(sizeof (AC_INTER_MSG_DEV_BYE_RESP));
            if (NULL == mbResopnse)
            {
                SVS_LOG((SVS_LM_ERROR, "Fail to allocate AceMessageBlock object when send live video share response."));
                return ;
            }

            SVS_LOG((SVS_LM_INFO,
                             "deal bye asyncResponse, DeviceID=%s, RespCode=%d.",
                             pByeResp->szLensID, pByeResp->nResponseCode));

            AC_INTER_MSG_DEV_BYE_RESP* pResp
                = ((AC_INTER_MSG_DEV_BYE_RESP*)(void*)mbResopnse->rd_ptr());

            (void)ACE_OS::memset(pResp, 0, sizeof(AC_INTER_MSG_DEV_BYE_RESP) );
            uint16_t usTransNum = (uint16_t)pByeResp->nRequestID;
            FillCommonHeader(
                &pResp->MsgHeader,
                SVS_MSG_TYPE_AC_INTER_MSG_BYE_RESP,
                transactionno_respond(usTransNum),
                sizeof(AC_INTER_MSG_DEV_BYE_RESP));
            memcpy(pResp->DeviceID, pByeResp->szLensID, SVS_DEVICEID_LEN);

            pResp->RespCode = pByeResp->nResponseCode;

            CServerManager* pCServerManager = &CServerManager::instance();
            pCServerManager->sndMsgToSvrMgr(mbResopnse);

            break;
        }
        default:
        {
            SVS_ACM::RESPONSE* pCommonResp = (SVS_ACM::RESPONSE*)&rResponse;
            SVS_LOG((SVS_LM_WARNING,
                             "deal unknown asyncResponse, RequestID=%d, RespCode=%d.",
                             pCommonResp->nRequestID, pCommonResp->nResponseCode));
        }
    }
}



