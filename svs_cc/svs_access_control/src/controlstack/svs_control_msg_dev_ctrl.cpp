#include "svs_control_msg_dev_ctrl.h"
#include "svs_log_msg.h"
#include "svs_server_stack.h"
#include "svs_control_stack.h"
#include "svs_access_control_manager.h"

 CControlDevCtrl::CControlDevCtrl()
 {
 }
 CControlDevCtrl::~CControlDevCtrl()
 {

 }

int32_t CControlDevCtrl::DealMsg()
{
    m_enStatus = CONTROL_MGS_HTTP_STATUS_WAIT_RESP;
    m_ReqTime  = time(NULL);
    return IAccessControlManager::instance().asyncRequest(m_rDevCtrl,CAccessControlStack::asyncResponse,this);
}

int32_t CControlDevCtrl::SendMsg()
{
    SVS_LOG((SVS_LM_DEBUG,"CControlDevCtrl::SendMsg,send message:[%s]",
                                                  m_strMsg.c_str()));
    return CControlMessage::send_http_response(m_strMsg);
}

int32_t CControlDevCtrl::ParserMsgBody(cJSON* body)
{
    cJSON* cameraId = cJSON_GetObjectItem(body,"cameraId");
    if(NULL == cameraId)
    {
        return SVS_ERROR_FAIL;
    }
    std::string strCameraId = cameraId->valuestring;

    strncpy(m_rDevCtrl.szLensID,strCameraId.c_str(),SVS_DEVICEID_LEN);

    cJSON* opCode = cJSON_GetObjectItem(body,"opCode");
    if(NULL != opCode)
    {
       SVS_ACM::DEV_CTRL_TYPE enCtrlType = (SVS_ACM::DEV_CTRL_TYPE)opCode->valueint;
       m_rDevCtrl.eCtrlType  = enCtrlType;
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,"CControlLivePlay::ParserMsgBody,there is not ctrl type in message"));
        return SVS_ERROR_FAIL;
    }

    cJSON* param1 = cJSON_GetObjectItem(body,"param1");
    if(NULL != param1)
    {
       m_rDevCtrl.nCtrlParam1  = param1->valueint;
    }
    cJSON* param2 = cJSON_GetObjectItem(body,"param2");
    if(NULL != param2)
    {
       m_rDevCtrl.nCtrlParam2  = param2->valueint;
    }
    cJSON* param3 = cJSON_GetObjectItem(body,"param3");
    if(NULL != param3)
    {
       m_rDevCtrl.nCtrlParam3  = param3->valueint;
    }
    cJSON* priority = cJSON_GetObjectItem(body,"priority");
    if(NULL != priority)
    {
       m_rDevCtrl.nPriority  = priority->valueint;
    }

    return SVS_ERROR_OK;
}

int32_t CControlDevCtrl::CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body)
{
    return SVS_ERROR_OK;
}

int32_t CControlDevCtrl::CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body)
{
    return SVS_ERROR_OK;
}
