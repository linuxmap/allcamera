/*
 * MduLoadInfoReq.h
 *
 *  Created on: 2016-3-22
 *      Author:
 */

#ifndef MDULOADINFOREQ_H_
#define MDULOADINFOREQ_H_

#include "svs_vms_message.h"

class CMduLoadInfoReq : public CMduSvsMessage
{
public:
    CMduLoadInfoReq();

    virtual ~CMduLoadInfoReq();

    // ������Ϣ���ȳ�ʼ����Ϣ��ֻ��ʼ����Ϣͷ��������Ҫ����
    // ����Ϣֻ��MDU�������ʲ���Ҫʵ�������Create����
    int32_t create(uint32_t unLength,
               uint32_t unTransNo);

    /// ��ʼ����Ϣ��
    int32_t initMsgBody();

    uint32_t getMsgType();

    int32_t handleMessage();

    void dump() const;
private:
    SVS_MSG_MDU_LOAD_INFO_REQ* m_pLoadInfo;
};

#endif /* MDULOADINFOREQ_H_ */
