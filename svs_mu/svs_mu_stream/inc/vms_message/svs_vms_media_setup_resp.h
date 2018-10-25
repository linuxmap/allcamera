/*
 * MduMediaSetupResp.h
 *
 *  Created on: 2016-5-18
 *      Author:
 */

#ifndef MDUMEDIASETUPRESP_H_
#define MDUMEDIASETUPRESP_H_

#include "svs_vms_message.h"

class CMduMediaSetupResp: public CMduSvsMessage
{
public:
    CMduMediaSetupResp();
    virtual ~CMduMediaSetupResp();

    int32_t create(char* pMsgData, uint32_t unLength);

    int32_t checkMessage();

    uint32_t getMsgType();

    int32_t handleMessage();

    void dump() const;

    uint32_t getLocalIndex() const;

    uint32_t getRespCode() const;

    PLAY_TYPE getPlayType() const;

    std::string getUrl()const;

    UTAPI void getSdpInfo(std::string& SdpInfo) const;

    UTAPI uint32_t getConnSetupType() const;

    UTAPI uint8_t* getDeviceID() const;

    UTAPI uint32_t getConnVideoIp() const;

    UTAPI uint16_t getConnVideoPort() const;

    UTAPI uint32_t getConnAudioIp() const;

    UTAPI uint16_t getConnAudioPort() const;

private:
    SVS_MSG_MDU_SESSION_SETUP_RESP  *m_pSetupResp;
};

#endif /* MDUMEDIASETUPRESP_H_ */
