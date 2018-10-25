/*
 * Rtp2PsProcessor.h
 *
 *  Created on: 2012-2-24
 *      Author:
 */

#ifndef RTP2PSPROCESSOR_H_
#define RTP2PSPROCESSOR_H_

#include "svs_adapter_session.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_rtp_frame_organizer.h"
#include "svs_adapter_ps_rtp_encap.h"
class CRtp2PsProcessor: public CDirectProcessor, public CPsRtpEncapHandle, public IRtpFrameHandler
{
public:
    CRtp2PsProcessor();

    virtual ~CRtp2PsProcessor();

    int32_t Init(uint64_t ullSendSessionID);

    // 发送接口
    void Send (ACE_Message_Block* pMb);

    // 处理RTP帧回调接口
    void handlePsRtpPacket(const char* pRtpData, uint32_t unDataSize, void* pUserData);

    // 处理PS封装后的RTP包回调接口
    void handleRtpFrame(RTP_FRAME_LIST &rtpFrameList);

private:
#define  RTP_FRAME_CACHE_SIZE     (1024 * 1024)

    uint32_t getWritePos()const;

    // 检查帧是否完整
    int32_t checkVideoRtpFrame(RTP_FRAME_LIST &rtpFrameList);

    int32_t checkAudioRtpFrame(RTP_FRAME_LIST &rtpFrameList);

    void HandleVideoRtpFrame(RTP_FRAME_LIST &rtpFrameList);

    void HandleAudioRtpFrame(RTP_FRAME_LIST &rtpFrameList);
    uint8_t GetRecvVideoPT();
private:
    CRtpFrameOrganizer      m_VideoRtpFrameOrganizer;
    CRtpFrameOrganizer      m_AudioRtpFrameOrganizer;
    CPsRtpEncap             m_PsRtpEncap;
    uint8_t           m_ucRecvVideoPT;

    // H264帧缓冲
    char*                   m_pRtpFrameCache;
    char*                   m_pWritePos;
};

#endif /* RTP2PSPROCESSOR_H_ */
