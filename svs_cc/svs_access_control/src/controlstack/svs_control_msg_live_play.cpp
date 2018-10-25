#include "svs_control_msg_live_play.h"
#include "svs_log_msg.h"
#include "svs_server_stack.h"
#include "svs_control_stack.h"
#include "svs_access_control_manager.h"

 CControlLivePlay::CControlLivePlay()
 {
 }
 CControlLivePlay::~CControlLivePlay()
 {

 }

int32_t CControlLivePlay::DealMsg()
{

    /* find the device */
    std::string strLensID = m_rPlayUrl.szLensID;
    //SVS_ACM::DEVICE_INFO stDeviceInfo;
    //int32_t nResult = IAccessControlManager::instance().getDeviceInfo( strDevID,stDeviceInfo);
    SVS_ACM::LENS_INFO stLensInfo;

    m_enStatus = CONTROL_MGS_HTTP_STATUS_WAIT_RESP;
    m_ReqTime  = time(NULL);
	
    int32_t nResult = IAccessControlManager::instance().getLensInfo( strLensID,stLensInfo);
    if(SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_WARNING,"CControlLivePlay::DealMsg,get the Lens:[%s] fail",
                                                  strLensID.c_str()));
        SVS_ACM::RESPONSE_PLAY_URL_INFO  oResponsePlayUrlInfo;
        oResponsePlayUrlInfo.nRequestID = m_rPlayUrl.nRequestID;//pPlayUrlInfoRequest->nRequestID;
        oResponsePlayUrlInfo.nResponseCode = SVS_RESPCODE_DEVICE_OFFLINE;
        CAccessControlStack::instance().asyncResponse(oResponsePlayUrlInfo, this);
	
        return SVS_ERROR_FAIL;
    }
    m_rPlayUrl.eDevType = stLensInfo.eLensType;
    //m_enStatus = CONTROL_MGS_HTTP_STATUS_WAIT_RESP;
    //m_ReqTime  = time(NULL);
    return IServerStack::instance().asyncRequest(m_rPlayUrl,CAccessControlStack::asyncResponse,this);
}

int32_t CControlLivePlay::SendMsg()
{
    SVS_LOG((SVS_LM_DEBUG,"CControlLivePlay::SendMsg,send message:[%s]",
                                                  m_strMsg.c_str()));
    return CControlMessage::send_http_response(m_strMsg);
}

int32_t CControlLivePlay::ParserMsgBody(cJSON* body)
{
    //m_rPlayUrl.nRequestID = (int32_t)this;
    m_rPlayUrl.nRequestID = 0;
    m_rPlayUrl.ePlayType  = PLAY_TYPE_LIVE;
    cJSON* cameraId = cJSON_GetObjectItem(body,"cameraId");
    if(NULL == cameraId)
    {
        return SVS_ERROR_FAIL;
    }
    std::string strCameraId = cameraId->valuestring;

    strncpy(m_rPlayUrl.szLensID,strCameraId.c_str(),SVS_DEVICEID_LEN);

    cJSON* streamType = cJSON_GetObjectItem(body,"streamType");
    if(NULL != streamType)
    {
       m_rPlayUrl.ulStreamId  = streamType->valueint;
    }

    cJSON* urlType = cJSON_GetObjectItem(body,"urlType");
    if(NULL != urlType)
    {
       m_rPlayUrl.ulStreamId  = urlType->valueint;
    }

    return SVS_ERROR_OK;
}

int32_t CControlLivePlay::CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body)
{
    return SVS_ERROR_OK;
}

int32_t CControlLivePlay::CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body)
{
    SVS_ACM::RESPONSE_PLAY_URL_INFO& m_respUrl = (SVS_ACM::RESPONSE_PLAY_URL_INFO&)rResponse;
    if(0 == strlen(m_respUrl.szUrl) || SVS_ERROR_OK != m_respUrl.nResponseCode) {
        return SVS_ERROR_OK;
    }
    cJSON* url = cJSON_CreateString(m_respUrl.szUrl);
    if(NULL == url)
    {
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(body,"url",url);
    m_nRequestID = rResponse.nRequestID;
    return SVS_ERROR_OK;
}
