/*
 * CPsRtpEcap类
 * 本类实现将一个H264帧封装成PS帧并进行RTP切片
 *  Created on: 2012-2-17
 *      Author:
 */

#ifndef __PSRTPECAP_H__
#define __PSRTPECAP_H__

#include "svs_ace_header.h"
#include "svs_adapter_mpeg_ps_enc.h"
#include "svs_adapter_def.h"



// 处理经过PS封装后的RTP包回调接口
class CPsRtpEncapHandle
{
public:
    CPsRtpEncapHandle() {}
    virtual ~CPsRtpEncapHandle() {}

    virtual void handlePsRtpPacket(const char* pRtpData, uint32_t unDataSize, void* pUserData) = 0;
};


// TODO SSRC问题

class CPsRtpEncap : public CMpegPsEncHandle
{
public:
    CPsRtpEncap();

    virtual ~CPsRtpEncap();

    int32_t init(CPsRtpEncapHandle* pHandle, void *pUserData);

    int32_t insertVideoFrame(const char* pData, uint32_t unDataSize, uint32_t unTimestamp);

    int32_t insertAudioFrame(const char* pData, uint32_t unDataSize, uint32_t unTimestamp);

    // CMpegPsEncHandle回调接口实现
    void handlePsStream(const char* pPSData, uint32_t unDataSize,
                        uint32_t unTimestamp, void* pUserData, bool endFlag);

    void release();
private:
    CPsRtpEncapHandle*      m_pRtpHandle;
    void*                   m_pUserData;

    CMpegPsEnc              m_MpegPsEncap;

    // RTP封装实现
    char                    m_RtpBuffer[MDU_MSG_BLOCK_SIZE];
    uint16_t                m_usRtpSeq;
};

#endif /* __PSRTPECAP_H__ */
