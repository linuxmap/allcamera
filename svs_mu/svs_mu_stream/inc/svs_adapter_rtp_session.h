/*
 * MduRtpSession.h
 *
 *  Created on: 2016-2-10
 *      Author:
 */

#ifndef MDURTPSESSION_H_
#define MDURTPSESSION_H_

#include "svs_adapter_session.h"
#include "svs_adapter_network_handle.h"
#include "svs_adapter_tcp_handle.h"
#include "svs_adapter_rtsp_packet.h"
#include "svs_adapter_rtp_packet.h"
#include "svs_adapter_rtcp_packet.h"
#include "svs_rtsp_protocol.h"
#include "svs_adapter_sdp.h"
#include "svs_rtsp_setup_message.h"

/// 定义UDP句柄类型
enum _enRTP_HANDLE_TYPE
{
    VIDEO_RTP_HANDLE,
    VIDEO_RTCP_HANDLE,
    AUDIO_RTP_HANDLE,
    AUDIO_RTCP_HANDLE,

    HANDLE_TYPE_MAX
};

/// 定义播放速率，进行速率转换时用到
#define  NUM_RATE_MULT_1     ((double)1.0)      // 正常速率
#define  NUM_RATE_MULT_2     ((double)2.0)      // 2倍速率
#define  NUM_RATE_MULT_4     ((double)4.0)      // 4倍速率
#define  NUM_RATE_MULT_8     ((double)8.0)      // 8倍速率
#define  NUM_RATE_MULT_16    ((double)16.0)     // 16倍速率
#define  NUM_RATE_MULT_32    ((double)32.0)     // 32倍速率
#define  NUM_RATE_DIV_2      ((double)1.0 / (double)2.0)      // 1/2倍速
#define  NUM_RATE_DIV_4      ((double)1.0 / (double)4.0)      // 1/4倍速
#define  NUM_RATE_DIV_8      ((double)1.0 / (double)8.0)      // 1/8倍速
#define  NUM_RATE_DIV_16     ((double)1.0 / (double)16.0)     // 1/16倍速
#define  NUM_RATE_DIV_32     ((double)1.0 / (double)32.0)     // 1/32倍速

#define RATE_EQUAL_RETURN(name, expect_type, expectValue, ret_type, ret_value)  \
    if (expect_type##_##name == expectValue)   \
    {   \
        ret_value = ret_type##_##name; \
        return  RET_OK; \
    }


class CMduRtpSession : public CMduSession
{
public:
    CMduRtpSession();
    virtual ~CMduRtpSession();

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

    void getVideoInfo(SDP_MEDIA_INFO &info);

    void getAudioInfo(SDP_MEDIA_INFO &info);

    int32_t sendStartRequest();

    int32_t dealSccVcrResp(const SVS_MSG_PLAYBACK_CONTROL_RESP* pMsg);

    void setTimeStamp(uint32_t unTimeStamp);

    bool getTimeStamp(uint32_t& unTimeStamp);

protected:
    /// 发送NAT穿越响应实现
    int32_t sendNatResponse(CMduNatMessage &natMsg);

    /// 分配端口资源
    int32_t allocMediaPort();

    int32_t startMediaPort();

    int32_t stopMediaPort();

    bool checkMediaChannelStatus();

    int32_t setRemoteAddress();

    int32_t parseRtspVcrMessage(const CRtspPacket &rtspMsg, uint32_t &unOperCode,
                                uint32_t &unPlayTimeType, uint32_t &unPlayStartTime,
                                uint32_t &unPlayStopTime, uint32_t &unPlayRate) const;
private:
    int32_t allocUdpMediaPort();

    int32_t allocTcpMediaPort();

    int32_t startUdpMediaPort();

    int32_t startTcpMediaPort();

    bool checkTcpMediaChannelStatus();

    bool checkUdpMediaChannelStatus() const;

    int32_t sendTcpNatResponse(const CMduNatMessage &natMsg);

    int32_t sendUdpNatResponse(const CMduNatMessage &natMsg);

    int32_t sendUdpMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount);

    int32_t sendTcpMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount);

    int32_t handleRtspMessage(const MDU_INNER_MSG &innerMsg,
                          uint32_t unMsgSize,
                          CMduSession &peerSession);

    void sendNatRequest();

    // 构造并发送RTCP报文
    void sendRtcpMessage();

    // 发送信令消息
    int32_t sendMessage(const void* pPeerHandle, const char* pData, uint32_t unDataSize);

    // 处理RTSP VCR消息
    int32_t handleRtspVcrMessage(CRtspPacket &rtspMsg,
                             const void* pRecvHandle,
                             CMduSession &peerSession);

    // 处理RTSP响应消息
    int32_t handleRtspRespMessage(CRtspPacket &rtspMsg,
                             const void* pRecvHandle,
                             CMduSession &peerSession);

    // 处理心跳消息
    int32_t handleRtspHeartbeatMsg(const MDU_INNER_MSG &innerMsg, CRtspPacket &rtspPack);

    int32_t NumRate2PlayRate(double dNumRate, uint32_t &unPlayRate) const;

private:            // 标准RTSP PULL流程
    // 启动标准Rtsp拉流过程
    int32_t startRtspPullSession();

    // 分配标准会话的媒体端口
    int32_t allocStdMediaPort();

    // 启动标准会话的媒体端口
    int32_t startStdMediaPort();

    // 停止拉流过程
    void stopRtspPullSession();

    int32_t sendStdVcrMessage(CRtspPacket &rtspPack);

    // 处理标准Rtsp消息
    int32_t handleStdRtspMessage(const MDU_INNER_MSG &innerMsg,
                             uint32_t unMsgSize,
                             CMduSession &peerSession);

    int32_t handleStdRtspDescribeResp(CRtspMessage &rtspMessage);

    int32_t handleStdRtspSetupResp(CRtspMessage &rtspMessage, CMduSession &peerSession);

    int32_t handleVideoRtspSetupResp(CRtspSetupMessage &setupMessage);

    int32_t handleAudioRtspSetupResp(CRtspSetupMessage &setupMessage);

    int32_t handleStdRtspPlayResp(CRtspMessage &rtspMessage, CMduSession &peerSession);

    int32_t handleStdRtspPauseResp(CRtspMessage &rtspMessage, CMduSession &peerSession) const;

    int32_t handleStdRtspTeardownResp(CRtspMessage &rtspMessage) const;

    int32_t handleStdRtspAnnounceReq(CRtspMessage &rtspMessage, CMduSession &peerSession);

    // 如果RTP包中
    void resetRtpPt(CRtpPacket &rtpPack);

    // 设置媒体通道号
    void setChannelNum(char cPt, char& channelNum)const;
protected:
    typedef enum _enSETUP_STATUS
    {
        SETUP_STATUS_VIDEO,         // 正在建立视频连接
        SETUP_STATUS_AUDIO,         // 正在建立音频连接
        SETUP_STATUS_NONE
    }SETUP_STATUS;
protected:
    /// TCP会话相关句柄
    CTcpHandle*             m_pTcpHandle;
    bool                    m_bHasPeerAddr;
    ACE_INET_Addr           m_TcpPeerAddr;
    uint32_t                m_ulLastNatTime;        // 上一次NAT穿越的时间，只在MDU级联时使用

    /// UDP会话相关句柄
    CNetworkHandle*         m_pUdpHandle[HANDLE_TYPE_MAX];
    ACE_INET_Addr           m_UdpPeerAddr[HANDLE_TYPE_MAX];

    // 标准PULL会话中RTSP协议栈
    ACE_Semaphore           m_rtspInitSem;
    CTcpHandle*             m_pRtspHandle;
    CMediaSdp               m_rtspSdp;
    CRtcpPacket             m_rtcpPacket;
    CRtspProtocol           m_rtspProtocol;
    std::string             m_strVideoSession;
    std::string             m_strAudioSession;
    SETUP_STATUS            m_enSetupStatus;           // Setup计数，建立连接需要两次setup
    bool                    m_bAudioPlay;

    // Payload Type变更
    CMduSession*            m_pPeerSession;

public:
    uint32_t                m_unRangeType;
    uint32_t                m_unRangeStart;
    uint32_t                m_unRangeStop;

    uint32_t                m_timeStamp;
    bool                    bPlaybackDragFlag;


};

#endif /* MDURTPSESSION_H_ */
