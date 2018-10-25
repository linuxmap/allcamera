#ifndef __MDU_MEDIA_PLAY_REQ_H__
#define __MDU_MEDIA_PLAY_REQ_H__
#include "svs_vms_message.h"

class CMduMediaPlayReq: public CMduSvsMessage
{
public:
    CMduMediaPlayReq();
    virtual ~CMduMediaPlayReq();

    int32_t create(uint32_t unLength,
                uint32_t unTransNo);

    int32_t initMsgBody(uint8_t* pszDevID);

    uint32_t getMsgType();

    int32_t handleMessage();

    void dump() const;

private:
    SVS_MSG_MDU_SESSION_PLAY_REQ*      m_pReq;
};
#endif


