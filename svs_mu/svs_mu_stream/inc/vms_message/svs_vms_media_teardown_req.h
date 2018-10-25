/*
 * MduStopDistributeReq.h
 *
 *  Created on: 2010-12-31
 *      Author:
 */

#ifndef MDUSTOPDISTRIBUTEREQ_H_
#define MDUSTOPDISTRIBUTEREQ_H_

#include "svs_vms_message.h"

class CMduMediaTearDownReq : public CMduSvsMessage
{
public:
    CMduMediaTearDownReq();

    virtual ~CMduMediaTearDownReq();

    UTAPI int32_t create(uint32_t unLength,
               uint32_t unTransNo);

    UTAPI uint32_t getMsgType();

    UTAPI int32_t handleMessage();

    UTAPI int32_t initMsgBody(uint32_t LocalIndex,const char* pszDevID);

    UTAPI void dump() const;

private:
    SVS_MSG_MDU_SESSION_TEARDOWN_REQ* m_pRequst;

};

#endif /* MDUSTOPDISTRIBUTEREQ_H_ */
