/*
 * StreamStdRtpSession.h
 *
 *  Created on: 2016-5-20
 *      Author:
 */

#ifndef STREAMRTMPSESSION_H_
#define STREAMRTMPSESSION_H_

#include <list>
#include "svs_adapter_session.h"
#include "svs_adapter_rtp_session.h"
#include "svs_static_preassign_buffer.h"
#include "svs_adapter_rtcp_packet.h"
#include "svs_rtmp_handle.h"




class CStreamRtmpSession: public CStreamSession
{
public:
    CStreamRtmpSession();
    virtual ~CStreamRtmpSession();
public:

    int32_t initRtmpSession(uint64_t ullPeerStreamID,
                          PLAY_TYPE      enPlayType,
                          const ACE_INET_Addr &localAddr,
                          const ACE_INET_Addr &peerAddr);

    int32_t startRtmpSession();

    void setRtmpHandle(CStreamRtmpSendHandle* pHandle);

    void setSessionId(uint64_t ullSessionId);

    void setPlayLoad(uint16_t unVedioPT,uint16_t unAudioPT );

    int32_t sendStartRequest();

public:
    int32_t initSesssion(PEER_TYPE unPeerType);

    int32_t sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount);

    int32_t sendVcrMessage(CRtspPacket &rtspPack);

    int32_t handleInnerMessage(const STREAM_INNER_MSG &innerMsg,
                            uint32_t unMsgSize,
                            CStreamSession&  peerSession);

    ACE_INET_Addr getPeerAddr()const;

    ACE_INET_Addr getMediaAddr()const;

    int32_t sendSessionStopMessage(uint32_t unStopType);

    void setSdpInfo(CMediaSdp& rtspSdp);
    void getSdpInfo(CMediaSdp& rtspSdp);

    virtual uint32_t getMediaTransType()const;

protected:
    int32_t allocMediaPort();

    int32_t startMediaPort();

    int32_t stopMediaPort();

    int32_t sendNatResponse(CStreamNatMessage &natMsg){return RET_OK;};

    bool checkMediaChannelStatus();

    int32_t setRemoteAddress(){return RET_OK;};

private:
    CStreamRtmpSendHandle* m_pHandle;
    ACE_INET_Addr       m_rtmpAddr;
    std::string         m_strRtmpSessionId;

    ACE_Thread_Mutex    m_TcpSendMutex;


    uint64_t            m_ullPeerStreamID;

    uint32_t            m_ulLastRecvTime;
    CMediaSdp           m_rtspSdp;
};

#endif /* STREAMSTDRTPSESSION_H_ */


