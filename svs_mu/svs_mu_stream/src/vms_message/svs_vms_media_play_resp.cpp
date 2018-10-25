#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"

#include "svs_vms_media_play_resp.h"
#include "svs_rtsp_service.h"
#include "svs_rtmp_service.h"
CMduMediaPlayResp::CMduMediaPlayResp()
{
    m_pPlayResp = NULL;
}

CMduMediaPlayResp::~CMduMediaPlayResp()
{
    m_pPlayResp = NULL;
}


int32_t CMduMediaPlayResp::create(char* pMsgData, uint32_t unLength)
{
    if (sizeof(SVS_MSG_MDU_SESSION_PLAY_RESP) != unLength)
    {
        SVS_LOG((SVS_LM_WARNING,"create media play response fail, length[%u] invalid.", unLength));
        return RET_FAIL;
    }

    int32_t nRet = CMduSvsMessage::create(pMsgData, unLength);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pPlayResp = (SVS_MSG_MDU_SESSION_PLAY_RESP*)(void*)getBinaryData();

    return RET_OK;
}

int32_t CMduMediaPlayResp::checkMessage()
{

    return RET_OK;
}

uint32_t CMduMediaPlayResp::getMsgType()
{
    return SVS_MSG_TYPE_MDU_SESSION_PLAY_RESP;
}

int32_t CMduMediaPlayResp::handleMessage()
{
    if(PLAY_URL_TYPE_RTSP == m_pPlayResp->UrlType)
    {
        return CMduRtspService::instance().handleSvsMessage(*this);
    }
    else if(PLAY_URL_TYPE_RTMP == m_pPlayResp->UrlType)
    {
        return CMduRtmpService::instance().handleSvsMessage(*this);
    }

    return RET_FAIL;
}

uint32_t CMduMediaPlayResp::getLocalIndex() const
{
    if (NULL == m_pPlayResp)
    {
        return 0;
    }

    return m_pPlayResp->LocalIndex;
}

uint32_t CMduMediaPlayResp::getRespCode() const
{
    if (NULL == m_pPlayResp)
    {
        return 0;
    }

    return m_pPlayResp->RespCode;
}

uint8_t* CMduMediaPlayResp::getDeviceID() const
{
    if (NULL == m_pPlayResp)
    {
        return 0;
    }
    return &m_pPlayResp->DeviceID[0];
}


void CMduMediaPlayResp::dump() const
{
    if (NULL == m_pPlayResp)
    {
        return;
    }

    CMduSvsMessage::dump();

    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));
    SVS_LOG((SVS_LM_DEBUG,"\tLocalIndex: %u", m_pPlayResp->LocalIndex));
    SVS_LOG((SVS_LM_DEBUG,"\tUrlType: %u", m_pPlayResp->UrlType));
    SVS_LOG((SVS_LM_DEBUG,"\tRespCode: %u", m_pPlayResp->RespCode));
    return;
}
