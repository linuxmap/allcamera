#ifndef __CMDUSESSION_H__
#define __CMDUSESSION_H__
#include <list>
#include "svs_ace_header.h"
#include <vms/vms.h>
#include "svs_adapter_def.h"
#include "svs_adapter_inner_msg.h"
#include "svs_adapter_nat_message.h"
#include "svs_adapter_rtsp_packet.h"
#include "svs_vms_media_setup_resp.h"


typedef enum _enMduSessionStatus
{
    MDU_SESSION_STATUS_INIT,
    MDU_SESSION_STATUS_WAIT_START,
    MDU_SESSION_STATUS_WAIT_CHANNEL_REDAY,
    MDU_SESSION_STATUS_RECVED_NAT_REQ,
    MDU_SESSION_STATUS_DISPATCHING,
    MDU_SESSION_STATUS_RELEASED,

    MDU_SESSION_STATUS_ABNORMAL
} MDU_SESSION_STATUS;

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

class CMduMediaProcessor;
class CMduSession
{
    friend class CMduSessionFactory;
public:
    CMduSession();

    virtual ~CMduSession();

    int32_t init(const char* pszContent,PLAY_TYPE enPlayType);

    void close();

    int32_t start(CMduMediaSetupResp *pResp);

    void setStatus(MDU_SESSION_STATUS enStatus);

    MDU_SESSION_STATUS getStatus();

    void setSessionId(uint64_t ullSessionId);

    uint64_svs getStreamId() const;

    const char* getContentID() const;

    void setContentID(const char* strContentID) ;

    uint32_t getVideoCodecType() const;

    uint32_t getVideoCodeType() const;

    PLAY_TYPE getPlayType() const;

    uint32_t getTransProtocol() const;

    uint32_t getMediaTransType() const;

    uint8_t getVideoPayload()const;

    uint8_t getAudioPayload()const;

    PEER_TYPE getPeerType() const;

    char getVideoInterleaveNum() const;

    char getAudioInterleaveNum() const;

    ACE_INET_Addr getVideoAddr() const;

    ACE_INET_Addr getAudioAddr() const;

    uint32_t getSessionType() const;

    uint32_t getSpecifyIp() const;

    uint32_t getTransDirection() const;

    void getSdpInfo(std::string& SdpInfo) const;

    void Dump(ACE_HANDLE handle)const;

    virtual int32_t initSesssion(PEER_TYPE unPeerType) = 0;

    virtual int32_t sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount) = 0;

    virtual int32_t handleInnerMessage(const MDU_INNER_MSG &innerMsg,
                                   uint32_t unMsgSize,
                                   CMduSession &peerSession) = 0;

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

    UTAPI int32_t handleNatMessage(CMduNatMessage &natMsg);

    virtual int32_t allocMediaPort() = 0;

    virtual int32_t startMediaPort() = 0;

    virtual int32_t stopMediaPort() = 0;

    virtual int32_t sendNatResponse(CMduNatMessage &natMsg) = 0;

    virtual bool checkMediaChannelStatus() = 0;

    virtual int32_t setRemoteAddress() = 0;

    void sendStartStreamRequest(CMduSession &peerSession);

private:
    typedef std::list<CMduNatMessage*>   MDU_NAT_LIST;
    typedef MDU_NAT_LIST::iterator       MDU_NAT_LIST_ITER;

private:
    int32_t handleAudioBroadcastNatMsg(CMduSession &peerSession, CMduNatMessage &natMsg);
    int32_t initSessionBySdp();
protected:
    int32_t                    m_lReferenceCnt;
    ACE_Thread_Mutex           m_StatusMutex;
    MDU_SESSION_STATUS         m_enSessionStatus;
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

    MDU_NAT_LIST               m_NatMsgList;
    bool                       m_bStartPlayFlag;

};

#endif // __CMDUSESSION_H__


