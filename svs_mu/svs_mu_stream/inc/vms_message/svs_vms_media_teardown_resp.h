/*
 * CMduStopDistributeResp.h
 *
 *  Created on: 2010-12-31
 *      Author:
 */

#ifndef CMDUSTOPDISTRIBUTERESP_H_
#define CMDUSTOPDISTRIBUTERESP_H_

#include "svs_vms_message.h"

class CMduMediaTearDownResp : public CMduSvsMessage
{
public:
    CMduMediaTearDownResp();

    virtual ~CMduMediaTearDownResp();

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
    SVS_MSG_MDU_SESSION_TEARDOWN_RESP *m_pResp;
};

#endif /* CMDUSTOPDISTRIBUTERESP_H_ */
