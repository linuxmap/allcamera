/*
 * StreamMediaSetupReq.h
 *
 *  Created on: 2016-5-18
 *      Author:
 */

#ifndef STREAMMEDIASETUPREQ_H_
#define STREAMMEDIASETUPREQ_H_

#include "svs_vms_message.h"

class CStreamMediaSetupReq: public CStreamSvsMessage
{
public:
    CStreamMediaSetupReq();
    virtual ~CStreamMediaSetupReq();

    int32_t create(uint32_t unLength,
                uint32_t unTransNo);

    int32_t initMsgBody(uint32_t unLocalIndex,const char* pszDevID,
                        PLAY_URL_TYPE UrlType,PLAY_TYPE PlayType,
                        const char* pRtspUrl,const char* pSdpInfo,
                        uint32_t MediaLinkMode,const char* pszMediaIP,uint16_t usMediaPort);

    uint32_t getMsgType();

    int32_t handleMessage();

    void dump() const;

private:
    SVS_MSG_STREAM_SESSION_SETUP_REQ   *m_pReq;
    std::string                      m_strRtspUrl;
    std::string                      m_strSdp;
};

#endif /* STREAMMEDIASETUPREQ_H_ */
