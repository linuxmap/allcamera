/*
 * StreamMsgFactory.cpp
 *
 *  Created on: 2010-12-30
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_vms_msg_factory.h"
#include "svs_log_msg.h"
#include "svs_vms_common_resp.h"
#include "svs_vms_load_Info_req.h"
#include "svs_vms_media_setup_req.h"
#include "svs_vms_media_setup_resp.h"
#include "svs_vms_media_teardown_req.h"
#include "svs_vms_media_teardown_resp.h"
#include "svs_vms_file_end_notify_req.h"
#include "svs_mvs_playback_control_resp.h"
#include "svs_vms_playback_control_req.h"
#include "svs_vms_media_keyframe_req.h"
#include "svs_vms_media_play_req.h"
#include "svs_vms_media_play_resp.h"



CStreamMsgFactory* CStreamMsgFactory::g_streamMsgFactoy = NULL;

CStreamMsgFactory::CStreamMsgFactory()
{
    m_unTransNo = 0;
}

CStreamMsgFactory::~CStreamMsgFactory()
{
}


uint32_t CStreamMsgFactory::getReqTransactionNo()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_TransNoMutex);
    m_unTransNo++;
    if (m_unTransNo & STREAM_RESP_TRANS_NO_MASK)
    {
        m_unTransNo |= ~STREAM_RESP_TRANS_NO_MASK;
    }

    return m_unTransNo;
}


uint32_t CStreamMsgFactory::getRespTransactionNo(uint32_t unReqTransNo)const
{
    return (unReqTransNo | STREAM_RESP_TRANS_NO_MASK);
}

int32_t CStreamMsgFactory::createSvsMsg(const char* pMsg,
                              uint32_t unMsgLength,
                              CStreamSvsMessage* &pMessage)const
{
    if ((NULL == pMsg)
        || (0 == unMsgLength)
        || (unMsgLength <= STREAM_SVS_MSAG_HEADER_SIZE))
    {
        SVS_LOG((SVS_LM_WARNING,"create svs message fail, msg len[%d]",
            unMsgLength));
        return RET_ERR_PARAM;
    }

    SVS_MSG_HEADER *pHeader = (SVS_MSG_HEADER*)(void*)pMsg;

    pMessage = createSvsMsgByType(pHeader->MsgType);
    if (NULL == pMessage)
    {
        SVS_LOG((SVS_LM_WARNING,"create svs message fail, message is null. msg type[0x%x]",
            pHeader->MsgType));
        return RET_FAIL;
    }

    return pMessage->create((char*)pMsg, unMsgLength);
}


int32_t CStreamMsgFactory::createSvsMsg(uint32_t unMsgType,
                              uint32_t unMsgLength,
                              uint32_t unTransNo,
                              CStreamSvsMessage* &pMessage)const
{
    if ((0 == unMsgLength)
        || (unMsgLength <= STREAM_SVS_MSAG_HEADER_SIZE))
    {
        SVS_LOG((SVS_LM_ERROR,"CStreamMsgFactory::createSvsMsg fail, msg len [%d] is invalid, msg type[0x%x].",
            unMsgLength,
            unMsgType));
        return RET_ERR_PARAM;
    }

    pMessage = createSvsMsgByType(unMsgType);
    if (NULL == pMessage)
    {
        SVS_LOG((SVS_LM_ERROR,"CStreamMsgFactory::createSvsMsg fail, msg type[0x%x].",
            unMsgType));
        return RET_FAIL;
    }

    return pMessage->create(unMsgLength,unTransNo);
}


void CStreamMsgFactory::destroySvsMsg(CStreamSvsMessage* &pMessage)const
{
    if (NULL != pMessage)
    {
        delete pMessage;
        pMessage = NULL;
    }

    return;
}


CStreamSvsMessage* CStreamMsgFactory::createSvsMsgByType(uint32_t unMsgType)const
{
    CStreamSvsMessage *pMessage = NULL;

    pMessage = createReqMsg(unMsgType);
    if (NULL == pMessage)
    {
        pMessage = createRespMsg(unMsgType);
    }

    return pMessage;
}

CStreamSvsMessage* CStreamMsgFactory::createReqMsg(uint32_t unMsgType)const
{
    CStreamSvsMessage *pMessage = NULL;

    try
    {
        switch(unMsgType)
        {
        case SVS_MSG_TYPE_STREAM_SESSION_TEARDOWN_REQ:
            pMessage = new CStreamMediaTearDownReq();
            break;

        case SVS_MSG_TYPE_STREAM_LOAD_INFO_REQ:
            pMessage = new CStreamLoadInfoReq();
            break;

        case SVS_MSG_TYPE_STREAM_SESSION_SETUP_REQ:
            pMessage = new CStreamMediaSetupReq();
            break;

        case SVS_MSG_TYPE_STREAM_SESSION_PLAY_REQ:
            pMessage = new CStreamMediaPlayReq();
            break;

        case SVS_MSG_TYPE_PLAYBACK_CONTROL_REQ:
            pMessage = new CPlaybackControlReq();
            break;

        case SVS_MSG_TYPE_FILE_END_NOTE_REQ:
            pMessage = new CFileEndNotifyReq();
            break;
        case SVS_MSG_TYPE_MEDIA_KEYFRAME_REQ:
            pMessage = new CStreamMediaKeyFrameReq();
            break;
        default:
            break;
        }
    }
    catch(...)
    {
    }

    return pMessage;
}
CStreamSvsMessage* CStreamMsgFactory::createRespMsg(uint32_t unMsgType)const
{
    CStreamSvsMessage *pMessage = NULL;

    try
    {
        switch(unMsgType)
        {
        case SVS_MSG_TYPE_COMMON_RESP:
        case SVS_MSG_TYPE_STREAM_LOAD_INFO_RESP:
        case SVS_MSG_TYPE_MU_SESSION_REPORT_RESP:
            pMessage = new CStreamCommonResp();
            break;

        case SVS_MSG_TYPE_STREAM_SESSION_TEARDOWN_RESP:
            pMessage = new CStreamMediaTearDownResp();
            break;

        case SVS_MSG_TYPE_STREAM_SESSION_SETUP_RESP:
            pMessage = new CStreamMediaSetupResp();
            break;

        case SVS_MSG_TYPE_STREAM_SESSION_PLAY_RESP:
            pMessage = new CStreamMediaPlayResp();
            break;
        case SVS_MSG_TYPE_PLAYBACK_CONTROL_RESP:
            pMessage = new CPlaybackControlResp();
            break;
        default:
            break;
        }
    }
    catch(...){}

    return pMessage;
}
