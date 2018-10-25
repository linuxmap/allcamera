/*
 * CStreamCommonResp.h
 *
 *  Created on: 2010-12-31
 *      Author:
 */

#ifndef CSTREAMCOMMONRESP_H_
#define CSTREAMCOMMONRESP_H_

#include "svs_vms_message.h"

class CStreamCommonResp : public CStreamSvsMessage
{
public:
    CStreamCommonResp();

    virtual ~CStreamCommonResp();

    // ���ݴ������Ϣ���ʼ����Ϣ�� ֻ��ʼ����Ϣͷ��������Ҫ����
    UTAPI int32_t create(char* pMsgData, uint32_t unLength);

    UTAPI int32_t checkMessage();

    UTAPI uint32_t getMsgType();

    UTAPI int32_t handleMessage();

    void dump() const;
private:
    SVS_MSG_COMMON_RESP*    m_pResp;
};

#endif /* CSTREAMSTOPDISTRIBUTERESP_H_ */
