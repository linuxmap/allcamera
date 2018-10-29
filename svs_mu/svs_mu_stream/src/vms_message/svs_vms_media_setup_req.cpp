/*
 * StreamMediaSetupReq.cpp
 *
 *  Created on: 2016-5-18
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_vms_media_setup_req.h"
#include "svs_adapter_service_task.h"

CStreamMediaSetupReq::CStreamMediaSetupReq()
{
    m_pReq       = NULL;
    m_strRtspUrl = "";
}

CStreamMediaSetupReq::~CStreamMediaSetupReq()
{
    m_pReq       = NULL;
}

int32_t CStreamMediaSetupReq::create(uint32_t unLength,
            uint32_t unTransNo)
{
    int32_t nRet = CStreamSvsMessage::create(unLength, unTransNo);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pReq = (SVS_MSG_STREAM_SESSION_SETUP_REQ*)(void*)getBinaryData();

    return RET_OK;
}

int32_t CStreamMediaSetupReq::initMsgBody(uint32_t unLocalIndex,const char* pszDevID,
                                       PLAY_URL_TYPE UrlType,PLAY_TYPE PlayType,DEV_STREAM_TYPE eStreamtype,
                                       const char* pRtspUrl,const char* pSdpInfo,
                                       uint32_t MediaLinkMode,const char* pszMediaIP,uint16_t usMediaPort)
{
    if (NULL == m_pReq)
    {
        return RET_FAIL;
    }

    m_strRtspUrl = pRtspUrl;
    m_strSdp     = pSdpInfo;
    if (getLength() < sizeof(SVS_MSG_STREAM_SESSION_SETUP_REQ))
    {
        return RET_FAIL;
    }

    m_pReq->LocalIndex  = unLocalIndex;
    m_pReq->UrlType     = UrlType;
    m_pReq->PlayType    = PlayType;
    m_pReq->eStreamtype = eStreamtype;
    memcpy(m_pReq->DeviceID,pszDevID,DEVICE_ID_LEN);
    size_t lens = strlen(pRtspUrl);
    (void)strncpy((char*)&m_pReq->szUrl[0],pRtspUrl,lens);
    m_pReq->UrlLen =  lens;

    lens = strlen(pSdpInfo);
    (void)strncpy((char*)&m_pReq->szSdp[0], pSdpInfo,lens);
    m_pReq->SdpLen =  lens;

    m_pReq->MediaLinkMode = MediaLinkMode;
    (void)strncpy((char*)&m_pReq->szMediaIP, pszMediaIP,SVS_IP_LEN);
    m_pReq->usMediaPort = usMediaPort;

    return RET_OK;
}

uint32_t CStreamMediaSetupReq::getMsgType()
{
    return SVS_MSG_TYPE_STREAM_SESSION_SETUP_REQ;
}

int32_t CStreamMediaSetupReq::handleMessage()
{
    return CStreamServiceTask::instance()->sendMsgToSCC(this);
}

void CStreamMediaSetupReq::dump() const
{
    if (NULL == m_pReq)
    {
        return;
    }

    CStreamSvsMessage::dump();

    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));
    SVS_LOG((SVS_LM_DEBUG,"\tLocalIndex: %u", m_pReq->LocalIndex));
    SVS_LOG((SVS_LM_DEBUG,"\tRtspUrl: %s", m_strRtspUrl.c_str()));
    SVS_LOG((SVS_LM_DEBUG,"\tsdp:\n\t %s", m_strSdp.c_str()));

    return;
}
