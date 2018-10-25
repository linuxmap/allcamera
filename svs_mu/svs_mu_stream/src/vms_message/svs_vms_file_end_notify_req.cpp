/*
 * MduFluxReportReq.cpp
 *
 *  Created on: 2016-3-22
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_vms_file_end_notify_req.h"
#include "svs_adapter_service_task.h"
#include "svs_vms_msg_factory.h"
#include "svs_adapter_rtp_session.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_session.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_exchange.h"


CFileEndNotifyReq::CFileEndNotifyReq()
{
    m_pNotifyReq = NULL;
    m_bSCCNotify = false;
}

CFileEndNotifyReq::~CFileEndNotifyReq()
{
    m_pNotifyReq = NULL;
    m_bSCCNotify = false;
}

int32_t CFileEndNotifyReq::create(char* pMsgData, uint32_t unLength)
{
     if (sizeof(SVS_MSG_FILE_END_NOTE_REQ) != unLength)
    {
        SVS_LOG((SVS_LM_WARNING,"create  file end notify request fail, length[%u] invalid.", unLength));
        return RET_FAIL;
    }

    // �ȵ��ø���ĳ�ʼ���ӿ�
    int32_t nRet = CMduSvsMessage::create(pMsgData, unLength);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"create file end notify request message fail, create svs message fail."));
        return nRet;
    }

    m_pNotifyReq = (SVS_MSG_FILE_END_NOTE_REQ*) (void*) getBinaryData();
    m_bSCCNotify = true;

    return RET_OK;
}
int32_t CFileEndNotifyReq::create(uint32_t unLength,
                               uint32_t unTransNo)
{
    // �ȵ��ø���ĳ�ʼ���ӿ�
    int32_t nRet = CMduSvsMessage::create(unLength, unTransNo);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pNotifyReq = (SVS_MSG_FILE_END_NOTE_REQ*)(void*)getBinaryData();
    m_bSCCNotify = false;

    // �Լ���������Ϣ����Ҫ�����
    return RET_OK;
}

/// ��ʼ����Ϣ��
int32_t CFileEndNotifyReq::initMsgBody(const char* pDeviceID, uint64_svs   StreamID)
{
    if (NULL == m_pNotifyReq)
    {
        return RET_FAIL;
    }

    if (NULL != pDeviceID)
    {
        memcpy(m_pNotifyReq->DeviceID, pDeviceID, DEVICE_ID_LEN);
    }
    else
    {
        memset(m_pNotifyReq->DeviceID, 0x0, DEVICE_ID_LEN);
    }

    m_pNotifyReq->BusinessID= StreamID;

    return RET_OK;
}

uint32_t CFileEndNotifyReq::getMsgType()
{
    return SVS_MSG_TYPE_FILE_END_NOTE_REQ;
}

int32_t CFileEndNotifyReq::handleMessage()
{
    if(false == m_bSCCNotify)
    {
        return CMduServiceTask::instance()->sendMsgToSCC(this);
    }

    uint64_svs streamId = m_pNotifyReq->BusinessID;
    int32_t nRetCode        = SVS_MSG_OK;

    // �����Ϣ
    nRetCode = checkMessage();
    if (RET_OK == nRetCode)
    {
        if (RET_OK == SendEOSToSession(streamId))
        {
            return RET_OK;
        }
    }

    CMduSvsMessage *pResp = NULL;
    int32_t nRet = CMduMsgFactory::instance()->createSvsMsg(SVS_MSG_TYPE_COMMON_RESP,
                                                sizeof(SVS_MSG_COMMON_RESP),
                                                m_pNotifyReq->MsgHeader.TransactionNo,
                                                pResp);
    if ((RET_OK != nRet) || (NULL == pResp))
    {
        SVS_LOG((SVS_LM_ERROR,"Create file end notify resp msg fail. stream id[%Q].",
                streamId));
        CMduMsgFactory::instance()->destroySvsMsg(pResp);
        return RET_FAIL;
    }


    nRet = pResp->handleMessage();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"Handle  file end notify resp msg fail. stream id[%Q].",
                streamId));
        CMduMsgFactory::instance()->destroySvsMsg(pResp);
        return RET_FAIL;
    }

    CMduMsgFactory::instance()->destroySvsMsg(pResp);
    SVS_LOG((SVS_LM_INFO,"Handle file end notify request message success. stream id[%Q].",streamId));
    return RET_OK;

}
 int32_t CFileEndNotifyReq::SendEOSToSession(uint64_svs   StreamID)
 {
    CMduSession *pSession = CMduSessionFactory::instance()->findSession(StreamID);
    if (NULL == pSession)
    {
        SVS_LOG((SVS_LM_WARNING,"can't send file end notify request message, stream[%Q] is invalid.", StreamID));
        return SVS_MSG_MDU_STREAMID_NOT_MATCH;
    }

    CMduRtpSession* pRTPSession = dynamic_cast<CMduRtpSession*>(pSession);
    if (NULL == pRTPSession)
    {
        CMduSessionFactory::instance()->releaseSession(pSession);
        SVS_LOG((SVS_LM_WARNING,"send file end notify request message fail, session[%Q] isn't svs session.", StreamID));
        return SVS_MSG_MU_INTERNAL_ERR;
    }

    int32_t iRet = pRTPSession->sendSessionStopMessage(MDU_PACKET_TYPE_SESSION_EOS);


    CMduSessionFactory::instance()->releaseSession(pSession);

    SVS_LOG((SVS_LM_INFO,"deal scc file end notify message success, session[%Q] ret[%d].", StreamID, iRet));

    return iRet;
 }

void CFileEndNotifyReq::dump() const
{
    if (NULL == m_pNotifyReq)
    {
        return;
    }

    CMduSvsMessage::dump();

    // ����Ϣ�ṹ��ӡ��Ϣ��
    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));

    char szIDBuf[PATH_LEN] = { 0 };
    memcpy(szIDBuf, m_pNotifyReq->DeviceID, DEVICE_ID_LEN);
    SVS_LOG((SVS_LM_DEBUG,"\tDeviceID: %s",szIDBuf));
    SVS_LOG((SVS_LM_DEBUG,"\tStreamID: %Q", m_pNotifyReq->BusinessID));

    return;
}
