/*
 * MduRtspPushSession.h
 *
 *  Created on: 2016-5-16
 *      Author:
 */

#ifndef MDURTMPPUSHSESSION_H_
#define MDURTMPPUSHSESSION_H_

#include "svs_ace_header.h"
#include "svs_adapter_session.h"
#include "svs_static_preassign_buffer.h"
#include "svs_adapter_rtmp_session.h"
#include "svs_media_hot_link.h"
#include "svs_rtmp_handle.h"
#include "svs_rtmp_amf.h"
#include "svs_rtmp.h"
#include "svs_adapter_rtp_frame_organizer.h"
#include <list>

#define RTMP_MESSAGE_SIZE 64

#define RTMP_CHUNK_SIZE   4096


struct RTMP_Message {
    uint8_t        type;
    size_t         len;
    unsigned long  timestamp;
    uint32_t       endpoint;
    std::string    buf;
};


enum RTMP_SESSION_STATUS
{
    RTMP_SESSION_STATUS_INIT       = 0,
    RTMP_SESSION_STATUS_HANDSHAKED = 1,
    RTMP_SESSION_STATUS_CONNECTED  = 2,
    RTMP_SESSION_STATUS_PLAY       = 3,
    RTMP_SESSION_STATUS_PUBLISHED  = 4,
    RTMP_SESSION_STATUS_TEARDOWN   = 5
};

enum RTMP_HANDSHAKE_STATUS
{

    RTMP_HANDSHAKE_STATUS_SERVER_CHALLENGE  = 1,
    RTMP_HANDSHAKE_STATUS_SERVER_RESPONSE   = 2,
    RTMP_HANDSHAKE_STATUS_SERVER_DONE       = 3,


    RTMP_HANDSHAKE_STATUS_CLIENT_CHALLENGE  = 4,
    RTMP_HANDSHAKE_STATUS_CLIENT_RESPONSE   = 5,
    RTMP_HANDSHAKE_STATUS_CLIENT_DONE       = 6,
};



#define RTMP_RETRY_INTERVAL     (200 * 1000)

typedef std::list<ACE_Message_Block*>  RTMP_SEND_LIST;
typedef RTMP_SEND_LIST::iterator       RTMP_SEND_LIST_ITER;


class CMduRtmpPushSession: public ACE_Event_Handler,CMduRtmpSendHandle,IRtpFrameHandler
{
public:
    CMduRtmpPushSession();
    virtual ~CMduRtmpPushSession();

public:
    int32_t open(uint32_t unIndex, const ACE_INET_Addr &peerAddr);

    void close();

    int32_t handleSvsMessage(CMduSvsMessage &message);

    int32_t handle_input(ACE_HANDLE handle);

    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask);

    int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);

    void setHandle(ACE_HANDLE handle, const ACE_INET_Addr &localAddr);

    ACE_HANDLE get_handle() const;

    uint32_t getSessionIndex() const;

    int32_t check();

    void setStatus(RTMP_SESSION_STATUS unStatus);

    RTMP_SESSION_STATUS getStatus() const;
public:
    virtual ACE_INET_Addr getPeerAddr()const;

    virtual ACE_INET_Addr getLocalAddr()const;

    virtual int32_t sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount);

    virtual void handleRtpFrame(RTP_FRAME_LIST &rtpFrameList);

private:
    int32_t setSockOpt();

    int32_t createStreamSession();

    int32_t sendMessage(const char* pData, uint32_t unDataSize);

    int32_t sendMediaSetupReq(CSVSMediaLink* linkInof);

    void    sendKeyFrameReq();

    int32_t createMediaSession();

    void    destroyMediaSession();

private:
    int32_t processRecvedMessage(const char* pData, uint32_t unDataSize);

    int32_t handleHandShake(const char* pData, uint32_t unDataSize);

    void    handleMessage(RTMP_Message *msg);

    void    handleInvoke(const RTMP_Message *msg, Decoder *dec);

    void    handleSetdataframe(Decoder *dec);

    void    handleConnect(double txid, Decoder *dec);

    void    handleFcpublish(double txid, Decoder *dec);

    void    handleCreateStream(double txid, Decoder *dec);

    void    handlePublish(double txid, Decoder *dec);

    void    handleStartPlayback();

    void    handlePlay(double txid, Decoder *dec);

    void    handlePlay2(double txid, Decoder *dec);

    void    handlePause(double txid, Decoder *dec);

    void    sendRtmpReply(double txid, const AMFValue &reply = AMFValue(),
                           const AMFValue &status = AMFValue());

    int32_t sendRtmpData(uint8_t type, uint32_t endpoint,const std::string &buf,
                      unsigned long timestamp = 0,int channel_num = CHAN_CONTROL);

    int32_t checkTransDirection(uint32_t unPeerType, uint32_t unTransDirection) const;

    int32_t createDistribute(CSVSMediaLink* linkinfo,std::string& strSdpInfo);

    void    simulateSendRtcpMsg();
private:
    void    sendH264FramebyRtmp(RTP_FRAME_LIST &rtpFrameList);
    void    sendCommonFramebyRtmp(RTP_FRAME_LIST &rtpFrameList);

private:
    ACE_Recursive_Thread_Mutex   m_RtmpMutex;
    uint32_t                     m_unSessionIndex;
    PLAY_TYPE                    m_enPlayType;

    ACE_HANDLE                   m_sockHandle;
    ACE_INET_Addr                m_PeerAddr;
    ACE_INET_Addr                m_LocalAddr;

    ACE_Thread_Mutex             m_StatusMutex;
    RTMP_SESSION_STATUS          m_enSessionStatus;
    RTMP_HANDSHAKE_STATUS        m_enHSStatus;//handshake status
    uint32_t                     m_ulStatusTime;

    Static_PreAssign_Buffer*     m_pRecvBuffer;

    CMduRtmpSession*             m_pRtmpSession;
    CMduSession*                 m_pPeerSession;

    std::string                  m_strContentID;
    std::string                  m_strUri;
    int32_t                      m_lRedoTimerId;
    CMediaSdp                    m_RtmpSdp;
private:
    Handshake                    m_Serversig;
    bool                         m_bPlaying; /* Wants to receive the stream */
    bool                         m_bReady;   /* Wants to receive and seen a keyframe */
    RTMP_Message                 m_Messages[RTMP_MESSAGE_SIZE];
    std::string                  m_buf;
    std::string                  m_send_queue;
    RTMP_SEND_LIST               m_rtmp_send_queue;
    size_t                       m_chunk_len;
    CRtpFrameOrganizer           m_RtpFrameOrganizer;
};

#endif /* MDURTSPPUSHSESSION_H_ */
