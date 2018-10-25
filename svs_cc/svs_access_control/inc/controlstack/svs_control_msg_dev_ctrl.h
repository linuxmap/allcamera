#ifndef __SVS_CONTROL_MSG_DEV_CTRL_H__
#define __SVS_CONTROL_MSG_DEV_CTRL_H__
#include "svs_control_msg.h"


//const std::string ACCESS_CONTROL_URI_LIVE_PLAY  =   "/api/media/live";
const std::string ACCESS_CONTROL_URI_DEV_CTRL  =   "/api/ptz/control";
class CControlDevCtrl:public CControlMessage
{
public:
    CControlDevCtrl();
    virtual ~CControlDevCtrl();

    virtual int32_t DealMsg();

    virtual int32_t SendMsg();
protected:
    virtual int32_t ParserMsgBody(cJSON* body);

    virtual int32_t CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body);

    virtual int32_t CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body);
private:
    SVS_ACM::REQUEST_DEV_CTRL  m_rDevCtrl;
};




#endif /*__SVS_CONTROL_MSG_DEV_CTRL_H__*/