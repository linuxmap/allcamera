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

    // 根据消息长度初始化消息，只初始化消息头，子类需要重载
    // 该消息只有STREAM创建，故不需要实现另外的Create方法
    int32_t create(uint32_t unLength,
               uint32_t unTransNo);

    /// 初始化消息体
    int32_t initMsgBody();

    uint32_t getMsgType();

    int32_t handleMessage();

    void dump() const;
private:
    SVS_MSG_STREAM_LOAD_INFO_REQ* m_pLoadInfo;
};

#endif /* STREAMLOADINFOREQ_H_ */
