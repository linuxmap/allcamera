#ifndef __STREAM_MEDIA_KEY_FRAME_H__
#define __STREAM_MEDIA_KEY_FRAME_H__
#include "svs_vms_message.h"

class CStreamMediaKeyFrameReq: public CStreamSvsMessage
{
public:
    CStreamMediaKeyFrameReq();
    virtual ~CStreamMediaKeyFrameReq();

    int32_t create(uint32_t unLength,
                uint32_t unTransNo);

    int32_t initMsgBody(uint8_t* pszDevID);

    uint32_t getMsgType();

    int32_t handleMessage();

    void dump() const;

private:
    SVS_MSG_STREAM_KEY_FRAME_REQ*      m_pReq;
};
#endif


