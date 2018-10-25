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
    /// ����ӿڣ�����ý����
    int32_t sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount);

    int32_t sendVcrMessage(CRtspPacket &rtspPack);

    int32_t handleInnerMessage(const MDU_INNER_MSG &innerMsg,
                           uint32_t unMsgSize,
                           CMduSession &peerSession);
    // ����EOS��Ϣ
    int32_t sendSessionStopMessage(uint32_t unStopType);

    virtual ACE_INET_Addr getPeerAddr()const;
    ACE_INET_Addr getMediaAddr()const;

    int32_t sendStartRequest();

    int32_t dealSccVcrResp(const SVS_MSG_PLAYBACK_CONTROL_RESP* pMsg);

protected:
    /// ����NAT��Խ��Ӧʵ��
    int32_t sendNatResponse(CMduNatMessage &natMsg);

    /// ����˿���Դ
    int32_t allocMediaPort();

    int32_t startMediaPort();

    int32_t stopMediaPort();

    bool checkMediaChannelStatus();

    int32_t setRemoteAddress();

protected:

    /// Ehome�Ự��ؾ��
    CEhomeHandle*           m_pEhomeHandle;
    ACE_INET_Addr           m_EhomePeerAddr;
    int32_t                 m_lEhomeSessionID;

    // Payload Type���
    CMduSession*            m_pPeerSession;


};

#endif /* MDURTPSESSION_H_ */
