#ifndef __SVS_CONTROL_MSG_LIVE_PLAY_H__
#define __SVS_CONTROL_MSG_LIVE_PLAY_H__
#include "svs_control_msg.h"


const std::string ACCESS_CONTROL_URI_LIVE_PLAY  =   "/api/media/live";
class CControlLivePlay:public CControlMessage
{
public:
    CControlLivePlay();
    virtual ~CControlLivePlay();

    virtual int32_t DealMsg();

    virtual int32_t SendMsg();
protected:
    virtual int32_t ParserMsgBody(cJSON* body);

    virtual int32_t CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body);

    virtual int32_t CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body);
private:
    SVS_ACM::REQUEST_PLAY_URL_INFO  m_rPlayUrl;
};




#endif /*__SVS_CONTROL_MSG_LIVE_PLAY_H__*/