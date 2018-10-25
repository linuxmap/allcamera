#ifndef __STREAM_MEDIA_PLAY_REQ_H__
#define __STREAM_MEDIA_PLAY_REQ_H__
#include "svs_vms_message.h"

class CStreamMediaPlayReq: public CStreamSvsMessage
{
public:
    CStreamMediaPlayReq();
    virtual ~CStreamMediaPlayReq();

    int32_t create(uint32_t unLength,
                uint32_t unTransNo);

    int32_t initMsgBody(uint8_t* pszDevID);

    uint32_t getMsgType();

    int32_t handleMessage();

    void dump() const;

private:
    SVS_MSG_STREAM_SESSION_PLAY_REQ*      m_pReq;
};
#endif


