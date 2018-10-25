#ifndef _MDU_MEDIA_PLAY_RESP_H_
#define _MDU_MEDIA_PLAY_RESP_H_

#include "svs_vms_message.h"

class CMduMediaPlayResp: public CMduSvsMessage
{
public:
    CMduMediaPlayResp();
    virtual ~CMduMediaPlayResp();

    int32_t create(char* pMsgData, uint32_t unLength);

    int32_t checkMessage();

    uint32_t getMsgType();

    int32_t handleMessage();

    void dump() const;

    uint32_t getLocalIndex() const;

    uint32_t getRespCode() const;

    uint8_t* getDeviceID() const;

private:
    SVS_MSG_MDU_SESSION_PLAY_RESP  *m_pPlayResp;
};

#endif /* _MDU_MEDIA_PLAY_RESP_H_ */
