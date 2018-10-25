/*
 * StreamStopDistributeReq.h
 *
 *  Created on: 2010-12-31
 *      Author:
 */

#ifndef STREAMSTOPDISTRIBUTEREQ_H_
#define STREAMSTOPDISTRIBUTEREQ_H_

#include "svs_vms_message.h"

class CStreamMediaTearDownReq : public CStreamSvsMessage
{
public:
    CStreamMediaTearDownReq();

    virtual ~CStreamMediaTearDownReq();

    UTAPI int32_t create(uint32_t unLength,
               uint32_t unTransNo);

    UTAPI uint32_t getMsgType();

    UTAPI int32_t handleMessage();

    UTAPI int32_t initMsgBody(uint32_t LocalIndex,const char* pszDevID);

    UTAPI void dump() const;

private:
    SVS_MSG_STREAM_SESSION_TEARDOWN_REQ* m_pRequst;

};

#endif /* STREAMSTOPDISTRIBUTEREQ_H_ */
