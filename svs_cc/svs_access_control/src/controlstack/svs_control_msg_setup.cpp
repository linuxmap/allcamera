#include "svs_control_msg_setup.h"
#include "svs_log_msg.h"

 CControlSetupMsg::CControlSetupMsg()
 {

 }
 CControlSetupMsg::~CControlSetupMsg()
 {

 }

int32_t CControlSetupMsg::DealMsg()
{
    return SVS_ERROR_OK;
}

int32_t CControlSetupMsg::SendMsg()
{
    return SVS_ERROR_OK;
}
int32_t CControlSetupMsg::ParserMsgBody(cJSON* body)
{
    return SVS_ERROR_OK;
}

int32_t CControlSetupMsg::CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body)
{
    return SVS_ERROR_OK;
}

int32_t CControlSetupMsg::CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body)
{
    return SVS_ERROR_OK;
}
