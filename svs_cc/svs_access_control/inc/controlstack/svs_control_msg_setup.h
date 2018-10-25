#ifndef __SVS_CONTROL_MSG_SETUP_H__
#define __SVS_CONTROL_MSG_SETUP_H__
#include "svs_control_msg.h"


const std::string ACCESS_CONTROL_URI_SET_UP  =   "/api/media/setup";
class CControlSetupMsg:public CControlMessage
{
public:
    CControlSetupMsg();
    virtual ~CControlSetupMsg();

    virtual int32_t DealMsg();

    virtual int32_t SendMsg();
protected:
    virtual int32_t ParserMsgBody(cJSON* body);

    virtual int32_t CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body);

    virtual int32_t CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body);
};




#endif /*__SVS_CONTROL_MSG_SETUP_H__*/