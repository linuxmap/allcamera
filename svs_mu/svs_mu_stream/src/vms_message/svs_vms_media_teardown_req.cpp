/*
 * svs_vms_media_teardown_req.cpp
 *
 *  Created on: 2010-12-31
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_vms_media_teardown_req.h"
#include "svs_adapter_service_task.h"
#include "svs_vms_msg_factory.h"
#include "svs_adapter_session_factory.h"
#include "svs_vms_media_teardown_resp.h"
#include "svs_adapter_business_manager.h"

CMduMediaTearDownReq::CMduMediaTearDownReq()
{
    m_pRequst = NULL;
}

CMduMediaTearDownReq::~CMduMediaTearDownReq()
{
    m_pRequst = NULL;
}


int32_t CMduMediaTearDownReq::create(uint32_t unLength,
                                    uint32_t unTransNo)
{

    int32_t nRet = CMduSvsMessage::create(unLength, unTransNo);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pRequst = (SVS_MSG_MDU_SESSION_TEARDOWN_REQ*)(void*)getBinaryData();

    return RET_OK;
}


uint32_t CMduMediaTearDownReq::getMsgType()
{
    return SVS_MSG_TYPE_MDU_SESSION_TEARDOWN_REQ;
}

int32_t CMduMediaTearDownReq::handleMessage()
{
    if (NULL == m_pRequst)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaTearDownReq::handleMessage fail, msg is null."));
        return RET_FAIL;
    }

    return CMduServiceTask::instance()->sendMsgToSCC(this);
}

int32_t CMduMediaTearDownReq::initMsgBody(uint32_t LocalIndex,const char* pszDevID)
{

    if (NULL == m_pRequst)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMediaTearDownReq::initMsgBody fail, msg is null."));
        return RET_FAIL;
    }
    m_pRequst->LocalIndex = LocalIndex;
    memcpy(m_pRequst->DeviceID,pszDevID,DEVICE_ID_LEN);
    return RET_OK;
}



void CMduMediaTearDownReq::dump() const
{
    if (NULL == m_pRequst)
    {
        return;
    }

    CMduSvsMessage::dump();

    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));
    SVS_LOG((SVS_LM_DEBUG,"\tLocalIndex: %d", m_pRequst->LocalIndex));
    return;
}

