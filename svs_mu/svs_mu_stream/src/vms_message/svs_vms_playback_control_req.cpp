/*
 * StreamDistributeReq.cpp
 *
 *  Created on: 2010-12-30
 *      Author:
 */

#include "svs_adapter_svs_retcode.h"
#include "svs_vms_playback_control_req.h"
#include "svs_adapter_session_factory.h"
#include "svs_mvs_playback_control_resp.h"
#include "svs_vms_msg_factory.h"
#include "svs_adapter_service_task.h"

CPlaybackControlReq::CPlaybackControlReq()
{
    m_pControlMsg = NULL;
    m_bActiveReq  = false;
}

CPlaybackControlReq::~CPlaybackControlReq()
{
    m_pControlMsg = NULL;
}


int32_t CPlaybackControlReq::create(char* pMsgData, uint32_t unLength)
{
    if (sizeof(SVS_MSG_PLAYBACK_CONTROL_REQ) != unLength)
    {
        SVS_LOG((SVS_LM_WARNING,"create playback control request fail, length[%u] invalid.", unLength));
        return RET_FAIL;
    }

    int32_t nRet = CStreamSvsMessage::create(pMsgData, unLength);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"create playback control request message fail, create svs message fail."));
        return nRet;
    }

    m_pControlMsg = (SVS_MSG_PLAYBACK_CONTROL_REQ*) (void*) getBinaryData();

    return RET_OK;
}


int32_t CPlaybackControlReq::create(uint32_t unLength,
                            uint32_t unTransNo)
{
    int32_t nRet = CStreamSvsMessage::create(unLength, unTransNo);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pControlMsg = (SVS_MSG_PLAYBACK_CONTROL_REQ*)(void*)getBinaryData();

    return RET_OK;
}

int32_t CPlaybackControlReq::initMsgBody(uint64_svs BusinessID,
                                        uint32_t   Sequence, uint32_t   OperCode,
                                        uint32_t   TimeType,uint32_t   StartTime,
                                        uint32_t   StopTime,uint32_t   PlayRate)
{
    if(NULL == m_pControlMsg)
    {
        return RET_FAIL;
    }

    m_pControlMsg->BusinessID = BusinessID;
    m_pControlMsg->Sequence = Sequence;
    m_pControlMsg->OperCode = OperCode;
    m_pControlMsg->TimeType = TimeType;
    m_pControlMsg->StartTime = StartTime;
    m_pControlMsg->StopTime = StopTime;
    m_pControlMsg->PlayRate = PlayRate;

    m_bActiveReq = true;

    return RET_OK;
}

int32_t CPlaybackControlReq::checkMessage()
{
    int32_t iRet = CStreamSvsMessage::checkMessage();
    if (RET_OK != iRet)
    {
        SVS_LOG((SVS_LM_WARNING,"Check distribute request message fail, message header is incorrect. iRet[0x%x].",
                iRet));
        return iRet;
    }

    return RET_OK;
}

uint32_t CPlaybackControlReq::getMsgType()
{
    return SVS_MSG_TYPE_PLAYBACK_CONTROL_REQ;
}

int32_t CPlaybackControlReq::handleMessage()
{
    if (NULL == m_pControlMsg)
    {
        SVS_LOG((SVS_LM_ERROR,"Handle playback control request message fail, message is null."));
        return RET_FAIL;
    }

    if(m_bActiveReq)
    {
        return CStreamServiceTask::instance()->sendMsgToSCC(this);
    }

    uint64_svs streamId = m_pControlMsg->BusinessID;
    int32_t nRetCode        = SVS_MSG_OK;

    nRetCode = checkMessage();
    if (RET_OK == nRetCode)
    {
        if (RET_OK == sendVcrMsg(streamId))
        {
            return RET_OK;
        }
    }


    CStreamSvsMessage *pResp = NULL;
    int32_t nRet = CStreamMsgFactory::instance()->createSvsMsg(SVS_MSG_TYPE_PLAYBACK_CONTROL_RESP,
                                                sizeof(SVS_MSG_PLAYBACK_CONTROL_RESP),
                                                m_pControlMsg->MsgHeader.TransactionNo,
                                                pResp);
    if ((RET_OK != nRet) || (NULL == pResp))
    {
        SVS_LOG((SVS_LM_ERROR,"Create playback control resp msg fail. stream id[%Q].",
                streamId));
        CStreamMsgFactory::instance()->destroySvsMsg(pResp);
        return RET_FAIL;
    }

    CPlaybackControlResp *pControlResp = (CPlaybackControlResp*)(void*) pResp;
    nRet = pControlResp->initMsgBody(streamId,
                                     m_pControlMsg->Sequence,
                                     0,
                                     0,
                                     (uint32_t) nRetCode);

    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"Init playback control resp msg fail. stream id[%Q].",
                streamId));
        CStreamMsgFactory::instance()->destroySvsMsg(pResp);
        return RET_FAIL;
    }

    nRet = pControlResp->handleMessage();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"Handle playback control resp msg fail. stream id[%Q].",
                streamId));
        CStreamMsgFactory::instance()->destroySvsMsg(pResp);
        return RET_FAIL;
    }

    CStreamMsgFactory::instance()->destroySvsMsg(pResp);
    SVS_LOG((SVS_LM_INFO,"Handle playback control request message success. stream id[%Q].",
            streamId));
    return RET_OK;
}


int32_t CPlaybackControlReq::sendVcrMsg(uint64_svs streamId)const
{
    CStreamSession *pSession = CStreamSessionFactory::instance()->findSession(streamId);
    if (NULL == pSession)
    {
        SVS_LOG((SVS_LM_WARNING,"can't send playback control request message, stream[%Q] is invalid.", streamId));
        return SVS_MSG_STREAM_STREAMID_NOT_MATCH;
    }
    int32_t iRet = RET_OK;
    /*
    CStreamSvsFrameSession* pSvsSession = dynamic_cast<CStreamSvsFrameSession*>(pSession);
    if (NULL == pSvsSession)
    {
        CStreamSessionFactory::instance()->releaseSession(pSession);
        SVS_LOG((SVS_LM_WARNING,"send playback control request message fail, session[%Q] isn't svs session.", streamId));
        return SVS_MSG_MU_INTERNAL_ERR;
    }

    iRet = RET_OK;//pSvsSession->dealSccVcrMsg(m_pControlMsg);
    */
    CStreamSessionFactory::instance()->releaseSession(pSession);

    SVS_LOG((SVS_LM_INFO,"deal scc playback control message success, session[%Q] ret[%d].", streamId, iRet));

    return iRet;
}

void CPlaybackControlReq::dump() const
{
    if (NULL == m_pControlMsg)
    {
        return;
    }

    CStreamSvsMessage::dump();

    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));

    SVS_LOG((SVS_LM_DEBUG,"\t\tBusinessID: %Q", m_pControlMsg->BusinessID));
    SVS_LOG((SVS_LM_DEBUG,"\t\tSequence: %u", m_pControlMsg->Sequence));
    SVS_LOG((SVS_LM_DEBUG,"\t\tOperCode: %u", m_pControlMsg->OperCode));
    SVS_LOG((SVS_LM_DEBUG,"\t\tTimeType: %u", m_pControlMsg->TimeType));
    SVS_LOG((SVS_LM_DEBUG,"\t\tStartTime: %u", m_pControlMsg->StartTime));
    SVS_LOG((SVS_LM_DEBUG,"\t\tStopTime: %u", m_pControlMsg->StopTime));
    SVS_LOG((SVS_LM_DEBUG,"\t\tPlayRate: 0x%x", m_pControlMsg->PlayRate));

    return;
}


