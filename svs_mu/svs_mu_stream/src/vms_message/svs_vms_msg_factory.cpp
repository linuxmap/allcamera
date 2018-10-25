/*
 * MduMsgFactory.cpp
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



CMduMsgFactory* CMduMsgFactory::g_mduMsgFactoy = NULL;

CMduMsgFactory::CMduMsgFactory()
{
    m_unTransNo = 0;
}

CMduMsgFactory::~CMduMsgFactory()
{
}


uint32_t CMduMsgFactory::getReqTransactionNo()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_TransNoMutex);
    m_unTransNo++;
    if (m_unTransNo & MDU_RESP_TRANS_NO_MASK)
    {
        m_unTransNo |= ~MDU_RESP_TRANS_NO_MASK;
    }

    return m_unTransNo;
}


uint32_t CMduMsgFactory::getRespTransactionNo(uint32_t unReqTransNo)const
{
    return (unReqTransNo | MDU_RESP_TRANS_NO_MASK);
}

int32_t CMduMsgFactory::createSvsMsg(const char* pMsg,
                              uint32_t unMsgLength,
                              CMduSvsMessage* &pMessage)const
{
    if ((NULL == pMsg)
        || (0 == unMsgLength)
        || (unMsgLength <= MDU_SVS_MSAG_HEADER_SIZE))
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


int32_t CMduMsgFactory::createSvsMsg(uint32_t unMsgType,
                              uint32_t unMsgLength,
                              uint32_t unTransNo,
                              CMduSvsMessage* &pMessage)const
{
    if ((0 == unMsgLength)
        || (unMsgLength <= MDU_SVS_MSAG_HEADER_SIZE))
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMsgFactory::createSvsMsg fail, msg len [%d] is invalid, msg type[0x%x].",
            unMsgLength,
            unMsgType));
        return RET_ERR_PARAM;
    }

    pMessage = createSvsMsgByType(unMsgType);
    if (NULL == pMessage)
    {
        SVS_LOG((SVS_LM_ERROR,"CMduMsgFactory::createSvsMsg fail, msg type[0x%x].",
            unMsgType));
        return RET_FAIL;
    }

    return pMessage->create(unMsgLength,unTransNo);
}


void CMduMsgFactory::destroySvsMsg(CMduSvsMessage* &pMessage)const
{
    if (NULL != pMessage)
    {
        delete pMessage;
        pMessage = NULL;
    }

    return;
}


CMduSvsMessage* CMduMsgFactory::createSvsMsgByType(uint32_t unMsgType)const
{
    CMduSvsMessage *pMessage = NULL;

    pMessage = createReqMsg(unMsgType);
    if (NULL == pMessage)
    {
        pMessage = createRespMsg(unMsgType);
    }

    return pMessage;
}

CMduSvsMessage* CMduMsgFactory::createReqMsg(uint32_t unMsgType)const
{
    CMduSvsMessage *pMessage = NULL;

    try
    {
        switch(unMsgType)
        {
        case SVS_MSG_TYPE_MDU_SESSION_TEARDOWN_REQ:
            pMessage = new CMduMediaTearDownReq();
            break;

        case SVS_MSG_TYPE_MDU_LOAD_INFO_REQ:
            pMessage = new CMduLoadInfoReq();
            break;

        case SVS_MSG_TYPE_MDU_SESSION_SETUP_REQ:
            pMessage = new CMduMediaSetupReq();
            break;

        case SVS_MSG_TYPE_MDU_SESSION_PLAY_REQ:
            pMessage = new CMduMediaPlayReq();
            break;

        case SVS_MSG_TYPE_PLAYBACK_CONTROL_REQ:
            pMessage = new CPlaybackControlReq();
            break;

        case SVS_MSG_TYPE_FILE_END_NOTE_REQ:
            pMessage = new CFileEndNotifyReq();
            break;
        case SVS_MSG_TYPE_MEDIA_KEYFRAME_REQ:
            pMessage = new CMduMediaKeyFrameReq();
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
CMduSvsMessage* CMduMsgFactory::createRespMsg(uint32_t unMsgType)const
{
    CMduSvsMessage *pMessage = NULL;

    try
    {
        switch(unMsgType)
        {
        case SVS_MSG_TYPE_COMMON_RESP:
        case SVS_MSG_TYPE_MDU_LOAD_INFO_RESP:
        case SVS_MSG_TYPE_MU_SESSION_REPORT_RESP:
            pMessage = new CMduCommonResp();
            break;

        case SVS_MSG_TYPE_MDU_SESSION_TEARDOWN_RESP:
            pMessage = new CMduMediaTearDownResp();
            break;

        case SVS_MSG_TYPE_MDU_SESSION_SETUP_RESP:
            pMessage = new CMduMediaSetupResp();
            break;

        case SVS_MSG_TYPE_MDU_SESSION_PLAY_RESP:
            pMessage = new CMduMediaPlayResp();
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
