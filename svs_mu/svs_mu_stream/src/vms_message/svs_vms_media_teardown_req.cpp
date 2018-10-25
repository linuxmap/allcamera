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

CStreamMediaTearDownReq::CStreamMediaTearDownReq()
{
    m_pRequst = NULL;
}

CStreamMediaTearDownReq::~CStreamMediaTearDownReq()
{
    m_pRequst = NULL;
}


int32_t CStreamMediaTearDownReq::create(uint32_t unLength,
                                    uint32_t unTransNo)
{

    int32_t nRet = CStreamSvsMessage::create(unLength, unTransNo);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pRequst = (SVS_MSG_STREAM_SESSION_TEARDOWN_REQ*)(void*)getBinaryData();

    return RET_OK;
}


uint32_t CStreamMediaTearDownReq::getMsgType()
{
    return SVS_MSG_TYPE_STREAM_SESSION_TEARDOWN_REQ;
}

int32_t CStreamMediaTearDownReq::handleMessage()
{
    if (NULL == m_pRequst)
    {
        SVS_LOG((SVS_LM_ERROR,"CStreamMediaTearDownReq::handleMessage fail, msg is null."));
        return RET_FAIL;
    }

    return CStreamServiceTask::instance()->sendMsgToSCC(this);
}

int32_t CStreamMediaTearDownReq::initMsgBody(uint32_t LocalIndex,const char* pszDevID)
{

    if (NULL == m_pRequst)
    {
        SVS_LOG((SVS_LM_ERROR,"CStreamMediaTearDownReq::initMsgBody fail, msg is null."));
        return RET_FAIL;
    }
    m_pRequst->LocalIndex = LocalIndex;
    memcpy(m_pRequst->DeviceID,pszDevID,DEVICE_ID_LEN);
    return RET_OK;
}



void CStreamMediaTearDownReq::dump() const
{
    if (NULL == m_pRequst)
    {
        return;
    }

    CStreamSvsMessage::dump();

    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));
    SVS_LOG((SVS_LM_DEBUG,"\tLocalIndex: %d", m_pRequst->LocalIndex));
    return;
}

