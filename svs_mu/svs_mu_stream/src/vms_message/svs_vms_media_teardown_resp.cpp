/*
 * CStreamStopDistributeResp.cpp
 *
 *  Created on: 2010-12-31
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_vms_media_teardown_resp.h"
#include "svs_adapter_service_task.h"
CStreamMediaTearDownResp::CStreamMediaTearDownResp()
{
    m_pResp = NULL;
}

CStreamMediaTearDownResp::~CStreamMediaTearDownResp()
{
    m_pResp = NULL;
}

int32_t CStreamMediaTearDownResp::create(char* pMsgData, uint32_t unLength)
{
    if (sizeof(SVS_MSG_STREAM_SESSION_TEARDOWN_RESP) != unLength)
    {
        SVS_LOG((SVS_LM_WARNING,"create tear downe response fail, message length[%u] invalid.",
                unLength));
        return RET_FAIL;
    }

    int32_t nRet = CStreamSvsMessage::create(pMsgData, unLength);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pResp = (SVS_MSG_STREAM_SESSION_TEARDOWN_RESP*)(void*)getBinaryData();

    return RET_OK;
}


uint32_t CStreamMediaTearDownResp::getMsgType()
{
    return SVS_MSG_TYPE_STREAM_SESSION_TEARDOWN_RESP;
}

int32_t CStreamMediaTearDownResp::checkMessage()
{
    return RET_OK;
}

int32_t CStreamMediaTearDownResp::handleMessage()
{
    //nothing to do
    return RET_OK;
}

void CStreamMediaTearDownResp::dump() const
{
    if (NULL == m_pResp)
    {
        return;
    }
    CStreamSvsMessage::dump();

    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));
    SVS_LOG((SVS_LM_DEBUG,"\tRespCode: %#x", m_pResp->RespCode));
    return;
}
