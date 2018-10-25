/*
 * CPlaybackControlResp.cpp
 *
 *  Created on: 2010-12-30
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_mvs_playback_control_resp.h"
#include "svs_adapter_service_task.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_rtp_session.h"

CPlaybackControlResp::CPlaybackControlResp()
{
    m_pControlResp = NULL;
    m_bSCCResp     = false;
}

CPlaybackControlResp::~CPlaybackControlResp()
{
    m_pControlResp = NULL;
}

int32_t CPlaybackControlResp::create(char* pMsgData, uint32_t unLength)
{
    if (sizeof(SVS_MSG_PLAYBACK_CONTROL_RESP) != unLength)
    {
        SVS_LOG((SVS_LM_WARNING,"create playback control respones fail, length[%u] invalid.", unLength));
        return RET_FAIL;
    }

    // �ȵ��ø���ĳ�ʼ���ӿ�
    int32_t nRet = CMduSvsMessage::create(pMsgData, unLength);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"create playback control respones message fail, create svs message fail."));
        return nRet;
    }

    m_pControlResp = (SVS_MSG_PLAYBACK_CONTROL_RESP*) (void*) getBinaryData();

    m_bSCCResp     = true;

    return RET_OK;
}
int32_t CPlaybackControlResp::create(uint32_t unLength,
                               uint32_t unTransNo)
{
    // �ȵ��ø���ĳ�ʼ���ӿ�
    int32_t nRet = CMduSvsMessage::create(unLength, unTransNo);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pControlResp = (SVS_MSG_PLAYBACK_CONTROL_RESP*)(void*)getBinaryData();

    // �Լ���������Ϣ����Ҫ�����
    return RET_OK;
}

uint32_t CPlaybackControlResp::getMsgType()
{
    return SVS_MSG_TYPE_PLAYBACK_CONTROL_RESP;
}

int32_t CPlaybackControlResp::initMsgBody(uint64_svs     streamID,
                                    uint32_t   unSequence,
                                    uint32_t   unRtpSequence,
                                    uint32_t   unRtpTimestamp,
                                    uint32_t   unRespCode)
{
    if (NULL == m_pControlResp)
    {
        return RET_FAIL;
    }

    m_pControlResp->BusinessID           = streamID;
    m_pControlResp->Sequence             = unSequence;
    m_pControlResp->RtpSequence          = unRtpSequence;
    m_pControlResp->RtpTimestamp         = unRtpTimestamp;
    m_pControlResp->RespCode             = unRespCode;

    return RET_OK;
}

int32_t CPlaybackControlResp::handleMessage()
{
    if(NULL == m_pControlResp)
    {
        return RET_FAIL;
    }

    if(!m_bSCCResp)
    {
        return CMduServiceTask::instance()->sendMsgToSCC(this);
    }

    int32_t iRet = CMduSvsMessage::checkMessage();
    if (RET_OK != iRet)
    {
        SVS_LOG((SVS_LM_WARNING,"Check PlaybackControl Respones message fail, message header is incorrect. iRet[0x%x].",
                iRet));
        return iRet;
    }

    CMduSession *pSession = CMduSessionFactory::instance()->findSession(m_pControlResp->BusinessID);
    if (NULL == pSession)
    {
        SVS_LOG((SVS_LM_WARNING,"can't send playback control Respones message, stream[%Q] is invalid.", m_pControlResp->BusinessID));
        return SVS_MSG_MDU_STREAMID_NOT_MATCH;
    }

   CMduRtpSession* pRtpSession = dynamic_cast<CMduRtpSession*>(pSession);
    if (NULL == pRtpSession)
    {
        CMduSessionFactory::instance()->releaseSession(pSession);
        SVS_LOG((SVS_LM_WARNING,"send playback control Respones message fail, session[%Q] isn't svs session.", m_pControlResp->BusinessID));
        return SVS_MSG_MU_INTERNAL_ERR;
    }

    iRet = pRtpSession->dealSccVcrResp(m_pControlResp);

    // �ͷŻỰ�����ü���
    CMduSessionFactory::instance()->releaseSession(pSession);

    SVS_LOG((SVS_LM_INFO,"deal scc playback control message success, session[%Q] ret[%d].", m_pControlResp->BusinessID, iRet));

    return iRet;
}

void CPlaybackControlResp::dump() const
{
    if (NULL == m_pControlResp)
    {
        return;
    }

    // �ȴ�ӡ��Ϣͷ
    CMduSvsMessage::dump();

    // ����Ϣ�ṹ��ӡ��Ϣ��
    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));

    SVS_LOG((SVS_LM_DEBUG,"\t\tStreamID: %Q", m_pControlResp->BusinessID));
    SVS_LOG((SVS_LM_DEBUG,"\t\tSequence: %u", m_pControlResp->Sequence));
    SVS_LOG((SVS_LM_DEBUG,"\t\tRespCode: %u", m_pControlResp->RespCode));
    SVS_LOG((SVS_LM_DEBUG,"\t\tRtpSequence: %u", m_pControlResp->RtpSequence));
    SVS_LOG((SVS_LM_DEBUG,"\t\tRtpTimestamp: %u", m_pControlResp->RtpTimestamp));

    return;
}

