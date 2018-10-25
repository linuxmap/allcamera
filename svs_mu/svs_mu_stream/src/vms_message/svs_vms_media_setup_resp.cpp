/*
 * MduMediaSetupResp.cpp
 *
 *  Created on: 2016-5-18
 *      Author:
 */

#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"

#include "svs_vms_media_setup_resp.h"
#include "svs_rtsp_service.h"
#include "svs_rtmp_service.h"
CMduMediaSetupResp::CMduMediaSetupResp()
{
    m_pSetupResp = NULL;
}

CMduMediaSetupResp::~CMduMediaSetupResp()
{
    m_pSetupResp = NULL;
}


int32_t CMduMediaSetupResp::create(char* pMsgData, uint32_t unLength)
{
    if (sizeof(SVS_MSG_MDU_SESSION_SETUP_RESP) != unLength)
    {
        SVS_LOG((SVS_LM_WARNING,"create media setup response fail, length[%u] invalid.", unLength));
        return RET_FAIL;
    }

    int32_t nRet = CMduSvsMessage::create(pMsgData, unLength);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pSetupResp = (SVS_MSG_MDU_SESSION_SETUP_RESP*)(void*)getBinaryData();

    return RET_OK;
}

int32_t CMduMediaSetupResp::checkMessage()
{

    return RET_OK;
}

uint32_t CMduMediaSetupResp::getMsgType()
{
    return SVS_MSG_TYPE_MDU_SESSION_SETUP_RESP;
}

int32_t CMduMediaSetupResp::handleMessage()
{
    if(PLAY_URL_TYPE_RTSP == m_pSetupResp->UrlType)
    {
        return CMduRtspService::instance().handleSvsMessage(*this);
    }
    else if(PLAY_URL_TYPE_RTMP == m_pSetupResp->UrlType)
    {
        return CMduRtmpService::instance().handleSvsMessage(*this);
    }

    return RET_FAIL;
}

uint32_t CMduMediaSetupResp::getLocalIndex() const
{
    if (NULL == m_pSetupResp)
    {
        return 0;
    }

    return m_pSetupResp->LocalIndex;
}

uint32_t CMduMediaSetupResp::getRespCode() const
{
    if (NULL == m_pSetupResp)
    {
        return 0;
    }

    return m_pSetupResp->RespCode;
}

PLAY_TYPE CMduMediaSetupResp::getPlayType() const
{
    if (NULL == m_pSetupResp)
    {
        return PLAY_TYPE_MAX;
    }

    return m_pSetupResp->PlayType;
}
std::string CMduMediaSetupResp::getUrl()const
{
    std::string url;
    url.append((const char*)&m_pSetupResp->szUrl[0], m_pSetupResp->UrlLen);
    return url;
}

void CMduMediaSetupResp::getSdpInfo(std::string& SdpInfo) const
{
    if (NULL == m_pSetupResp)
    {
        return ;
    }

    SdpInfo.clear();
    SdpInfo.append((const char*)&m_pSetupResp->szSdp[0],(size_t)m_pSetupResp->SdpLen);

    return;
}

uint8_t* CMduMediaSetupResp::getDeviceID() const
{
    if (NULL == m_pSetupResp)
    {
        return 0;
    }
    return &m_pSetupResp->DeviceID[0];
}


void CMduMediaSetupResp::dump() const
{
    if (NULL == m_pSetupResp)
    {
        return;
    }

    CMduSvsMessage::dump();

    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));
    SVS_LOG((SVS_LM_DEBUG,"\tLocalIndex: %u", m_pSetupResp->LocalIndex));
    SVS_LOG((SVS_LM_DEBUG,"\tServiceType: %u", m_pSetupResp->PlayType));
    SVS_LOG((SVS_LM_DEBUG,"\tRespCode: %u", m_pSetupResp->RespCode));
    SVS_LOG((SVS_LM_DEBUG,"\tUrl: %s", m_pSetupResp->szUrl));
    SVS_LOG((SVS_LM_DEBUG,"SDPInfon:"));
    SVS_LOG((SVS_LM_DEBUG,"\t\t%s", m_pSetupResp->szSdp));
    return;
}
