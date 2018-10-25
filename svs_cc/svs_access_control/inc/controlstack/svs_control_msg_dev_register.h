#ifndef __SVS_CONTROL_MSG_DEV_REGISTER_H__
#define __SVS_CONTROL_MSG_DEV_REGISTER_H__
#include "svs_control_msg.h"


const std::string ACCESS_CONTROL_URI_DEV_REG   =  "/api/dev/register";
class CControlDevRegMsg:public CControlMessage
{
public:
    CControlDevRegMsg();
    virtual ~CControlDevRegMsg();

    virtual int32_t DealMsg();

    virtual int32_t SendMsg();
protected:
    virtual int32_t ParserMsgBody(cJSON* body);

    virtual int32_t CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body);

    virtual int32_t CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body);
};


const std::string ACCESS_CONTROL_URI_DEV_REPORT   =  "/api/dev/status/notify";
class CControlDevReportMsg:public CControlMessage
{
public:
    CControlDevReportMsg();
    virtual ~CControlDevReportMsg();

    virtual int32_t DealMsg();

    virtual int32_t SendMsg();
protected:
    virtual int32_t ParserMsgBody(cJSON* body);

    virtual int32_t CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body);

    virtual int32_t CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body);
};




#endif /*__SVS_CONTROL_MSG_DEV_REGISTER_H__*/
