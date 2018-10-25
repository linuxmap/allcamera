/*
 * CMduCommonResp.h
 *
 *  Created on: 2010-12-31
 *      Author:
 */

#ifndef CMDUCOMMONRESP_H_
#define CMDUCOMMONRESP_H_

#include "svs_vms_message.h"

class CMduCommonResp : public CMduSvsMessage
{
public:
    CMduCommonResp();

    virtual ~CMduCommonResp();

    // 根据传入的消息块初始化消息， 只初始化消息头，子类需要重载
    UTAPI int32_t create(char* pMsgData, uint32_t unLength);

    UTAPI int32_t checkMessage();

    UTAPI uint32_t getMsgType();

    UTAPI int32_t handleMessage();

    void dump() const;
private:
    SVS_MSG_COMMON_RESP*    m_pResp;
};

#endif /* CMDUSTOPDISTRIBUTERESP_H_ */
