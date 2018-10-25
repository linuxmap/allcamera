/*
 * FileEndNotifyReq.h
 *
 *  Created on: 2016-3-22
 *      Author:
 */

#ifndef FILEENDNOTIFYREQ_H_
#define FILEENDNOTIFYREQ_H_

#include "svs_vms_message.h"

class CFileEndNotifyReq : public CMduSvsMessage
{
public:
    CFileEndNotifyReq();
    virtual ~CFileEndNotifyReq();

    int32_t create(char* pMsgData, uint32_t unLength);
    int32_t create(uint32_t unLength,
               uint32_t unTransNo);

    /// 初始化消息体
    int32_t initMsgBody(const char* pDeviceID, uint64_svs   StreamID);

    uint32_t getMsgType();

    int32_t handleMessage();

    void dump() const;
private:
    int32_t SendEOSToSession(uint64_svs   StreamID);

private:
    SVS_MSG_FILE_END_NOTE_REQ*  m_pNotifyReq;
    bool                                                m_bSCCNotify;
};

#endif /* FILEENDNOTIFYREQ_H_ */
