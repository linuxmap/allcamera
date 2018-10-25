/*
 * PlaybackControlReq.h
 *
 *  Created on: 2010-12-30
 *      Author:
 */

#ifndef CPLAYBACKCONTROLREQ_H_
#define CPLAYBACKCONTROLREQ_H_

#include "svs_vms_message.h"


class CPlaybackControlReq : public CMduSvsMessage
{
public:
    CPlaybackControlReq();
    virtual ~CPlaybackControlReq();

    UTAPI int32_t create(char* pMsgData, uint32_t unLength);


    UTAPI int32_t create(uint32_t unLength,
                   uint32_t unTransNo);

    /// 初始化消息体
    UTAPI int32_t initMsgBody(uint64_svs BusinessID,
                              uint32_t   Sequence,
                              uint32_t   OperCode,
                              uint32_t   TimeType,
                              uint32_t   StartTime,
                              uint32_t   StopTime,
                              uint32_t   PlayRate);

    UTAPI int32_t checkMessage();

    UTAPI uint32_t getMsgType();

    UTAPI int32_t handleMessage();

    UTAPI void dump() const;
private:

    int32_t sendVcrMsg(uint64_svs streamId)const;


private:
    SVS_MSG_PLAYBACK_CONTROL_REQ*  m_pControlMsg;
    bool                           m_bActiveReq;
};

#endif /* CPLAYBACKCONTROLREQ_H_ */
