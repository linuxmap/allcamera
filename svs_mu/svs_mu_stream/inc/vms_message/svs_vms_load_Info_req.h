/*
 * StreamLoadInfoReq.h
 *
 *  Created on: 2016-3-22
 *      Author:
 */

#ifndef STREAMLOADINFOREQ_H_
#define STREAMLOADINFOREQ_H_

#include "svs_vms_message.h"

class CStreamLoadInfoReq : public CStreamSvsMessage
{
public:
    CStreamLoadInfoReq();

    virtual ~CStreamLoadInfoReq();

    // ������Ϣ���ȳ�ʼ����Ϣ��ֻ��ʼ����Ϣͷ��������Ҫ����
    // ����Ϣֻ��STREAM�������ʲ���Ҫʵ�������Create����
    int32_t create(uint32_t unLength,
               uint32_t unTransNo);

    /// ��ʼ����Ϣ��
    int32_t initMsgBody();

    uint32_t getMsgType();

    int32_t handleMessage();

    void dump() const;
private:
    SVS_MSG_STREAM_LOAD_INFO_REQ* m_pLoadInfo;
};

#endif /* STREAMLOADINFOREQ_H_ */
