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

    // 根据传入的消息块初始化消息， 只初始化消息头，子类需要重载
    // 该消息由SCC发起，只需要实现些创建方法
    UTAPI int32_t create(char* pMsgData, uint32_t unLength);

    UTAPI uint32_t getMsgType();

    /// 父类中只对消息头进行检查，具体消息体检查还需要各子类实现
    UTAPI int32_t checkMessage();

    UTAPI int32_t handleMessage();

    UTAPI int32_t initMsgBody(uint64_svs streamID, uint32_t unRespCode);

    UTAPI void dump() const;
private:
    SVS_MSG_MDU_SESSION_TEARDOWN_RESP *m_pResp;
};

#endif /* CMDUSTOPDISTRIBUTERESP_H_ */
