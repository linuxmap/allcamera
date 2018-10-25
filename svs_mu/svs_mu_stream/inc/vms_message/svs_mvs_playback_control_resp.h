/*
 *
 *  Created on: 2010-12-30
 *      Author:
 */

#ifndef CPLAYBACKCONTROLRESP_H_
#define CPLAYBACKCONTROLRESP_H_

#include "svs_vms_message.h"

class CPlaybackControlResp : public CMduSvsMessage
{
public:
    CPlaybackControlResp();
    virtual ~CPlaybackControlResp();

    // 根据消息长度初始化消息，只初始化消息头，子类需要重载
    UTAPI int32_t create(char* pMsgData, uint32_t unLength);
    // 该消息只有MDU创建，故不需要实现另外的Create方法
    UTAPI int32_t create(uint32_t unLength,
               uint32_t unTransNo);

    /// 初始化消息体
    UTAPI int32_t initMsgBody(uint64_svs     streamID,
              uint32_t   unSequence,
              uint32_t   unRtpSequence,
              uint32_t   unRtpTimestamp,
              uint32_t   unRespCode);

    UTAPI uint32_t getMsgType();

    UTAPI int32_t handleMessage();

    UTAPI void dump() const;
private:
    SVS_MSG_PLAYBACK_CONTROL_RESP*   m_pControlResp;
    bool                             m_bSCCResp;
};


#endif /* CPLAYBACKCONTROLRESP_H_ */
