#include "svs_vms_media_play_req.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_service_task.h"


CMduMediaPlayReq::CMduMediaPlayReq()
{
    m_pReq = NULL;
}
CMduMediaPlayReq::~CMduMediaPlayReq()
{
}

int32_t CMduMediaPlayReq::create(uint32_t unLength,
            uint32_t unTransNo)
{
    int32_t nRet = CMduSvsMessage::create(unLength, unTransNo);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pReq = (SVS_MSG_MDU_SESSION_PLAY_REQ*)(void*)getBinaryData();
    return RET_OK;
}

int32_t CMduMediaPlayReq::initMsgBody(uint8_t* pszDevID)
{
    memcpy(m_pReq->DeviceID,pszDevID,DEVICE_ID_LEN);
    return RET_OK;
}

uint32_t CMduMediaPlayReq::getMsgType()
{
    return SVS_MSG_TYPE_MDU_SESSION_PLAY_REQ;
}

int32_t CMduMediaPlayReq::handleMessage()
{
   return CMduServiceTask::instance()->sendMsgToSCC(this);
}

void CMduMediaPlayReq::dump() const
{
    if (NULL == m_pReq)
    {
        return;
    }

    CMduSvsMessage::dump();
    char szDevID[DEVICE_ID_LEN+1] = { 0 };
    memcpy(szDevID,m_pReq->DeviceID,DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));
    SVS_LOG((SVS_LM_DEBUG,"\tDeviceID: %s", szDevID));

}


