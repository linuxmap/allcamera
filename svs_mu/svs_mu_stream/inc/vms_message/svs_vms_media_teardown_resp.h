/*
 * CStreamStopDistributeResp.h
 *
 *  Created on: 2010-12-31
 *      Author:
 */

#ifndef CSTREAMSTOPDISTRIBUTERESP_H_
#define CSTREAMSTOPDISTRIBUTERESP_H_

#include "svs_vms_message.h"

class CStreamMediaTearDownResp : public CStreamSvsMessage
{
public:
    CStreamMediaTearDownResp();

    virtual ~CStreamMediaTearDownResp();

    // ���ݴ������Ϣ���ʼ����Ϣ�� ֻ��ʼ����Ϣͷ��������Ҫ����
    // ����Ϣ��SCC����ֻ��Ҫʵ��Щ��������
    UTAPI int32_t create(char* pMsgData, uint32_t unLength);

    UTAPI uint32_t getMsgType();

    /// ������ֻ����Ϣͷ���м�飬������Ϣ���黹��Ҫ������ʵ��
    UTAPI int32_t checkMessage();

    UTAPI int32_t handleMessage();

    UTAPI int32_t initMsgBody(uint64_svs streamID, uint32_t unRespCode);

    UTAPI void dump() const;
private:
    SVS_MSG_STREAM_SESSION_TEARDOWN_RESP *m_pResp;
};

#endif /* CSTREAMSTOPDISTRIBUTERESP_H_ */
