#ifndef __MDU_EHOME_SESSION_H_
#define __MDU_EHOME_SESSION_H_

#include "svs_adapter_session.h"
#include "svs_adapter_network_handle.h"
#include "svs_adapter_ehome_handle.h"
#include "svs_adapter_rtsp_packet.h"
#include "svs_adapter_rtp_packet.h"
#include "svs_adapter_rtcp_packet.h"
#include "svs_rtsp_protocol.h"
#include "svs_adapter_sdp.h"
#include "svs_rtsp_setup_message.h"


class CMduEhomeSession : public CMduSession
{
public:
    CMduEhomeSession();
    virtual ~CMduEhomeSession();

    int32_t initSesssion(PEER_TYPE unPeerType);
    /// 对外接口：发送媒体流
    int32_t sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount);

    int32_t sendVcrMessage(CRtspPacket &rtspPack);

    int32_t handleInnerMessage(const MDU_INNER_MSG &innerMsg,
                           uint32_t unMsgSize,
                           CMduSession &peerSession);
    // 发送EOS消息
    int32_t sendSessionStopMessage(uint32_t unStopType);

    virtual ACE_INET_Addr getPeerAddr()const;
    ACE_INET_Addr getMediaAddr()const;

    int32_t sendStartRequest();

    int32_t dealSccVcrResp(const SVS_MSG_PLAYBACK_CONTROL_RESP* pMsg);

protected:
    /// 发送NAT穿越响应实现
    int32_t sendNatResponse(CMduNatMessage &natMsg);

    /// 分配端口资源
    int32_t allocMediaPort();

    int32_t startMediaPort();

    int32_t stopMediaPort();

    bool checkMediaChannelStatus();

    int32_t setRemoteAddress();

protected:

    /// Ehome会话相关句柄
    CEhomeHandle*           m_pEhomeHandle;
    ACE_INET_Addr           m_EhomePeerAddr;
    int32_t                 m_lEhomeSessionID;

    // Payload Type变更
    CMduSession*            m_pPeerSession;


};

#endif /* MDURTPSESSION_H_ */
