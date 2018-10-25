#include "svs_vms_media_keyframe_req.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_service_task.h"


CStreamMediaKeyFrameReq::CStreamMediaKeyFrameReq()
{
    m_pReq = NULL;
}
CStreamMediaKeyFrameReq::~CStreamMediaKeyFrameReq()
{
}

int32_t CStreamMediaKeyFrameReq::create(uint32_t unLength,
            uint32_t unTransNo)
{
    int32_t nRet = CStreamSvsMessage::create(unLength, unTransNo);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pReq = (SVS_MSG_STREAM_KEY_FRAME_REQ*)(void*)getBinaryData();
    return RET_OK;
}

int32_t CStreamMediaKeyFrameReq::initMsgBody(uint8_t* pszDevID)
{
    memcpy(m_pReq->DeviceID,pszDevID,DEVICE_ID_LEN);
    return RET_OK;
}

uint32_t CStreamMediaKeyFrameReq::getMsgType()
{
    return SVS_MSG_TYPE_MEDIA_KEYFRAME_REQ;
}

int32_t CStreamMediaKeyFrameReq::handleMessage()
{
   return CStreamServiceTask::instance()->sendMsgToSCC(this);
}

void CStreamMediaKeyFrameReq::dump() const
{
    if (NULL == m_pReq)
    {
        return;
    }

    CStreamSvsMessage::dump();
    char szDevID[DEVICE_ID_LEN+1] = { 0 };
    memcpy(szDevID,m_pReq->DeviceID,DEVICE_ID_LEN);

    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));
    SVS_LOG((SVS_LM_DEBUG,"\tDeviceID: %s", szDevID));

}


