#ifndef __CSTREAMSESSION_H__
#define __CSTREAMSESSION_H__
#include <list>
#include "svs_ace_header.h"
#include <vms/vms.h>
#include "svs_adapter_def.h"
#include "svs_adapter_inner_msg.h"
#include "svs_adapter_nat_message.h"
#include "svs_adapter_rtsp_packet.h"
#include "svs_vms_media_setup_resp.h"


typedef enum _enStreamSessionStatus
{
    STREAM_SESSION_STATUS_INIT,
    STREAM_SESSION_STATUS_WAIT_START,
    STREAM_SESSION_STATUS_WAIT_CHANNEL_REDAY,
    STREAM_SESSION_STATUS_RECVED_NAT_REQ,
    STREAM_SESSION_STATUS_DISPATCHING,
    STREAM_SESSION_STATUS_RELEASED,

    STREAM_SESSION_STATUS_ABNORMAL
} STREAM_SESSION_STATUS;

typedef struct _SESSION_INFO
{
    uint64_svs   StreamID;
    PEER_TYPE    PeerType;
    uint32_t     TransProtocol;
    uint32_t     TransDirection;
    uint64_svs   RecvStreamID;
    uint32_t     MediaTransType;
    uint32_t     SessionType;
    uint32_t     SpecifyIp;
} SESSION_INFO;


class CSessionInfo
{
private:
    CSessionInfo();
    CSessionInfo(const CSessionInfo& info);
    CSessionInfo& operator=(const CSessionInfo& info);
public:
    static CSessionInfo& instance()
    {
        static CSessionInfo info;
        return info;
    }
    UTAPI ~CSessionInfo();

    UTAPI const char* getPlayType(PLAY_TYPE Type)const;
    UTAPI const char* getPeerType(PEER_TYPE Type)const;
    UTAPI const char* getTransProtocol(uint32_t Type)const;
};

class CStreamMediaProcessor;
class CStreamSession
{
    friend class CStreamSessionFactory;
public:
    CStreamSession();

    virtual ~CStreamSession();

    virtual int32_t init(const char* pszContent,PLAY_TYPE enPlayType);

    virtual void close();

    virtual int32_t start(CStreamMediaSetupResp *pResp);

    virtual void setStatus(STREAM_SESSION_STATUS enStatus);

    virtual STREAM_SESSION_STATUS getStatus();

    virtual void setSessionId(uint64_t ullSessionId);

    virtual uint64_svs getStreamId() const;

    virtual const char* getContentID() const;

    virtual void setContentID(const char* strContentID) ;

    virtual uint32_t getVideoCodecType() const;

    virtual uint32_t getVideoCodeType() const;

    virtual PLAY_TYPE getPlayType() const;

    virtual uint32_t getTransProtocol() const;

    virtual uint32_t getMediaTransType() const;

    virtual uint8_t getVideoPayload()const;

    virtual uint8_t getAudioPayload()const;

    virtual PEER_TYPE getPeerType() const;

    virtual char getVideoInterleaveNum() const;

    virtual char getAudioInterleaveNum() const;

    virtual ACE_INET_Addr getVideoAddr() const;

    virtual ACE_INET_Addr getAudioAddr() const;

    virtual uint32_t getSessionType() const;

    virtual uint32_t getSpecifyIp() const;

    virtual uint32_t getTransDirection() const;

    virtual void getSdpInfo(std::string& SdpInfo) const;

    virtual void Dump(ACE_HANDLE handle)const;

    virtual int32_t initSesssion(PEER_TYPE unPeerType) = 0;

    virtual int32_t sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount) = 0;

    virtual int32_t handleInnerMessage(const STREAM_INNER_MSG &innerMsg,
                                   uint32_t unMsgSize,
                                   CStreamSession &peerSession) = 0;

    virtual int32_t sendVcrMessage(CRtspPacket &rtspPack) = 0;

    virtual int32_t sendSessionStopMessage(uint32_t unStopType) = 0;

    virtual int32_t dealSccVcrResp(const SVS_MSG_PLAYBACK_CONTROL_RESP* pMsg) ;

    virtual int32_t sendStartRequest() = 0;

    virtual ACE_INET_Addr getPeerAddr() const = 0;

    virtual ACE_INET_Addr getMediaAddr() const = 0;

public:
    UTAPI void checkSessionStatus();

    UTAPI uint64_t getConnStreamID();

    UTAPI int32_t handleRecvedNatRequest();

    void simulateNatReq();
protected:
    UTAPI int32_t addReference();

    UTAPI int32_t decReference();

    UTAPI int32_t handleNatMessage(CStreamNatMessage &natMsg);

    virtual int32_t allocMediaPort() = 0;

    virtual int32_t startMediaPort() = 0;

    virtual int32_t stopMediaPort() = 0;

    virtual int32_t sendNatResponse(CStreamNatMessage &natMsg) = 0;

    virtual bool checkMediaChannelStatus() = 0;

    virtual int32_t setRemoteAddress() = 0;

    void sendStartStreamRequest(CStreamSession &peerSession);

private:
    typedef std::list<CStreamNatMessage*>   STREAM_NAT_LIST;
    typedef STREAM_NAT_LIST::iterator       STREAM_NAT_LIST_ITER;

private:
    int32_t handleAudioBroadcastNatMsg(CStreamSession &peerSession, CStreamNatMessage &natMsg);
    int32_t initSessionBySdp();
protected:
    int32_t                    m_lReferenceCnt;
    ACE_Thread_Mutex           m_StatusMutex;
    STREAM_SESSION_STATUS         m_enSessionStatus;
    uint32_t                   m_ulStatusBeginTime;

    char                       m_szPeerContent[ID_LEN];
    PLAY_TYPE                  m_enPlayType;
    SESSION_INFO               m_stSessionInfo;


    std::string                m_SdpInfo;
    uint32_t                   m_ulVideoCodeType;
    uint16_t                   m_unVedioPT;
    uint32_t                   m_unAudioCodecType;
    uint16_t                   m_unAudioPT;
    uint32_t                   m_unConnSetupType;
    uint64_t                   m_ullConnStreamID;
    ACE_INET_Addr              m_ConnVideoAddr;
    ACE_INET_Addr              m_ConnAudioAddr;
    std::string                m_strRtspUrl;
    char                       m_szContentId[CONTENT_ID_LEN + 1];
    ACE_INET_Addr              m_VideoAddr;
    ACE_INET_Addr              m_AudioAddr;
    uint32_t                   m_unVideoInterleaveNum;
    uint32_t                   m_unAudioInterleaveNum;

    STREAM_NAT_LIST               m_NatMsgList;
    bool                       m_bStartPlayFlag;

};

#endif // __CSTREAMSESSION_H__


