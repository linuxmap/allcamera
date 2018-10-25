/*
 * EsRtpEncap.h
 *
 *  Created on: 2013-1-11
 */

#ifndef ESRTPENCAP_H_
#define ESRTPENCAP_H_
#include "svs_ace_header.h"
#include "svs_adapter_def.h"
#include "svs_adapter_mpeg_ps_dec.h"


#define STARTCODE_LEN  4

// 处理经过PS封装后的RTP包回调接口
class CEsRtpEncapHandle
{
public:
    CEsRtpEncapHandle() {}
    virtual ~CEsRtpEncapHandle() {}

    virtual void handleEsRtpPacket(const char* pRtpData, uint32_t unDataSize, void* pUserData) = 0;
};

class CEsRtpEncap:public CMpegPsDecHandle
{
public:
    CEsRtpEncap();
    virtual ~CEsRtpEncap();
    int32_t Init(CEsRtpEncapHandle* pEsRtpHandle,void* pUserData);

    int32_t insertPSFrame(const char* pData, uint32_t unDataSize, uint32_t unTimestamp);

    void SetVideoPayloadType(uint8_t ucVideoPT)
    {
        m_ucVideoPT = ucVideoPT;
    };

    virtual void handleVideoEsStream(const char* pEsData, uint32_t unDataSize,
                                uint32_t unTimestamp,uint8_t ucStreamType, void* pUserData);

    virtual void handleVideoMulNaluStream(const char* pEsData, uint32_t unDataSize,
                                uint32_t unTimestamp,uint8_t ucStreamType, void* pUserData);

    virtual void handleAudioEsStream(const char* pEsData, uint32_t unDataSize,
                                uint32_t unTimestamp,uint8_t ucStreamType, void* pUserData);


private:
    char* h264_find_startcode_internal(char *p, char *end);

    char* h264_find_startcode(char *p, char *end);

    void EnCapRtpData(const char* pData, uint32_t unDataSize, uint32_t unTimestamp,uint8_t ucStreamType);



private:
    CMpegPsDec                  m_MpegPsDec;
    CEsRtpEncapHandle*          m_pEsRtpHandle;
    void*                       m_pUserData;
    // RTP封装实现
    char                        m_RtpBuffer[STREAM_MSG_BLOCK_SIZE];
    uint16_t                    m_usVideoRtpSeq;
    uint16_t                    m_usAudioRtpSeq;

    uint8_t               m_ucVideoPT;

};

#endif /* ESRTPENCAP_H_ */
