/*
 * StreamMediaSetupResp.cpp
 *
 *  Created on: 2016-5-18
 *      Author:
 */

#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"

#include "svs_vms_media_setup_resp.h"
#include "svs_rtsp_service.h"
#include "svs_rtmp_service.h"
CStreamMediaSetupResp::CStreamMediaSetupResp()
{
    m_pSetupResp = NULL;
}

CStreamMediaSetupResp::~CStreamMediaSetupResp()
{
    m_pSetupResp = NULL;
}


int32_t CStreamMediaSetupResp::create(char* pMsgData, uint32_t unLength)
{
    if (sizeof(SVS_MSG_STREAM_SESSION_SETUP_RESP) != unLength)
    {
        SVS_LOG((SVS_LM_WARNING,"create media setup response fail, length[%u] invalid.", unLength));
        return RET_FAIL;
    }

    int32_t nRet = CStreamSvsMessage::create(pMsgData, unLength);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pSetupResp = (SVS_MSG_STREAM_SESSION_SETUP_RESP*)(void*)getBinaryData();

    return RET_OK;
}

int32_t CStreamMediaSetupResp::checkMessage()
{

    return RET_OK;
}

uint32_t CStreamMediaSetupResp::getMsgType()
{
    return SVS_MSG_TYPE_STREAM_SESSION_SETUP_RESP;
}

int32_t CStreamMediaSetupResp::handleMessage()
{
    if(PLAY_URL_TYPE_RTSP == m_pSetupResp->UrlType)
    {
        return CStreamRtspService::instance().handleSvsMessage(*this);
    }
    else if(PLAY_URL_TYPE_RTMP == m_pSetupResp->UrlType)
    {
        return CStreamRtmpService::instance().handleSvsMessage(*this);
    }

    return RET_FAIL;
}

uint32_t CStreamMediaSetupResp::getLocalIndex() const
{
    if (NULL == m_pSetupResp)
    {
        return 0;
    }

    return m_pSetupResp->LocalIndex;
}

uint32_t CStreamMediaSetupResp::getRespCode() const
{
    if (NULL == m_pSetupResp)
    {
        return 0;
    }

    return m_pSetupResp->RespCode;
}

PLAY_TYPE CStreamMediaSetupResp::getPlayType() const
{
    if (NULL == m_pSetupResp)
    {
        return PLAY_TYPE_MAX;
    }

    return m_pSetupResp->PlayType;
}
std::string CStreamMediaSetupResp::getUrl()const
{
    std::string url;
    url.append((const char*)&m_pSetupResp->szUrl[0], m_pSetupResp->UrlLen);
    return url;
}

void CStreamMediaSetupResp::getSdpInfo(std::string& SdpInfo) const
{
    if (NULL == m_pSetupResp)
    {
        return ;
    }

    SdpInfo.clear();
    SdpInfo.append((const char*)&m_pSetupResp->szSdp[0],(size_t)m_pSetupResp->SdpLen);

    return;
}

uint8_t* CStreamMediaSetupResp::getDeviceID() const
{
    if (NULL == m_pSetupResp)
    {
        return 0;
    }
    return &m_pSetupResp->DeviceID[0];
}

int32_t CStreamMediaSetupResp::getSessionID() const
{
    if (NULL == m_pSetupResp)
    {
        return 0;
    }
    return m_pSetupResp->SessionID;
}



void CStreamMediaSetupResp::dump() const
{
    if (NULL == m_pSetupResp)
    {
        return;
    }

    CStreamSvsMessage::dump();

    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));
    SVS_LOG((SVS_LM_DEBUG,"\tLocalIndex: %u", m_pSetupResp->LocalIndex));
    SVS_LOG((SVS_LM_DEBUG,"\tServiceType: %u", m_pSetupResp->PlayType));
    SVS_LOG((SVS_LM_DEBUG,"\tRespCode: %u", m_pSetupResp->RespCode));
    SVS_LOG((SVS_LM_DEBUG,"\tSessionID: %u", m_pSetupResp->SessionID));
    SVS_LOG((SVS_LM_DEBUG,"\tUrl: %s", m_pSetupResp->szUrl));
    SVS_LOG((SVS_LM_DEBUG,"SDPInfon:"));
    SVS_LOG((SVS_LM_DEBUG,"\t\t%s", m_pSetupResp->szSdp));
    return;
}
