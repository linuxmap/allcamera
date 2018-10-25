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

    // ���ݴ������Ϣ���ʼ����Ϣ�� ֻ��ʼ����Ϣͷ��������Ҫ����
    UTAPI int32_t create(char* pMsgData, uint32_t unLength);

    UTAPI int32_t checkMessage();

    UTAPI uint32_t getMsgType();

    UTAPI int32_t handleMessage();

    void dump() const;
private:
    SVS_MSG_COMMON_RESP*    m_pResp;
};

#endif /* CMDUSTOPDISTRIBUTERESP_H_ */
