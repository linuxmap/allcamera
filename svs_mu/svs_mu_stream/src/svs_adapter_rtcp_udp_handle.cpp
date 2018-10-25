/*
 * RtcpUdpHandle.cpp
 *
 *  Created on: 2016-1-4
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_rtcp_udp_handle.h"
#include "svs_adapter_inner_msg.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_session.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_exchange.h"
#include "svs_adapter_service_task.h"

#include "svs_adapter_rtsp_packet.h"
CRtcpUdpHandle::CRtcpUdpHandle()
{
}

CRtcpUdpHandle::~CRtcpUdpHandle()
{
}

int32_t CRtcpUdpHandle::handleRecvedData(ACE_Message_Block *pMsg, ACE_INET_Addr &remoteAddr)
{
    if (NULL == pMsg)
    {
        return RET_FAIL;
    }

    if (RET_OK == handleDummyMessage(pMsg, remoteAddr))
    {
        return RET_OK;
    }

    uint16_t usMsgType     = INNER_MSG_RTCP;
    MDU_TRANSMIT_PACKET *pPacket = (MDU_TRANSMIT_PACKET *)(void*)pMsg->base();
    uint32_t unRtspLen       = 0;

    if (RET_OK == CRtspPacket::checkRtsp((char*)pPacket->cData,
                    (pMsg->length() - sizeof(MDU_TRANSMIT_PACKET)) + 1,
                    unRtspLen))
    {
         usMsgType = INNER_MSG_RTSP;
    }

    fillMduInnerMsg(pMsg->base(),
            m_ullStreamID,
            (void*) this,
            remoteAddr.get_ip_address(),
            remoteAddr.get_port_number(),
            usMsgType,
            sizeof(MDU_TRANSMIT_PACKET) - 1);

    return CMduServiceTask::instance()->enqueueInnerMessage(pMsg);
}

int32_t CRtcpUdpHandle::handleDummyMessage(ACE_Message_Block *pMsg, const ACE_INET_Addr &remoteAddr)
{
    if (NULL == pMsg)
    {
        return RET_FAIL;
    }

    uint32_t ulSize = (pMsg->length() - sizeof(MDU_TRANSMIT_PACKET)) + 1;

    MDU_TRANSMIT_PACKET *pPacket = (MDU_TRANSMIT_PACKET *)(void*) pMsg->base();

    uint32_t ulDummy = *(uint32_t*)(void*)&pPacket->cData;

    if ((DUMMYSIZE != ulSize)
        ||(RTP_RTCP_DUMMY != ulDummy))
    {
        return RET_FAIL;
    }

    fillMduInnerMsg(pMsg->base(),
            m_ullStreamID,
            (void*) this,
            remoteAddr.get_ip_address(),
            remoteAddr.get_port_number(),
            INNER_MSG_RTCPDUMMY,
            sizeof(MDU_TRANSMIT_PACKET) - 1);

    return CMduServiceTask::instance()->enqueueInnerMessage(pMsg);
}
