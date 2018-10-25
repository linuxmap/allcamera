/*
 * svs_es_rtp_encap.h
 *
 *  Created on: 2015-1-11
 *      Author: hexin
 */

#ifndef __TS_SEGMENT_ENCAP_H_
#define __TS_SEGMENT_ENCAP_H_
#include "svs_ace_header.h"
#include "svs_adapter_def.h"
#include "svs_adapter_mpeg_ts_enc.h"


#define STARTCODE_LEN  4

// 处理经过PS封装后的RTP包回调接口
class CTsSegEncapHandle
{
public:
    CTsSegEncapHandle() {}
    virtual ~CTsSegEncapHandle() {}

    virtual void handleTsSegPacket(const char* pRtpData, uint32_t unDataSize,uint32_t ulDuration,uint32_t ulTsTag, void* pUserData) = 0;
};

class CEs2TSEncap:public CMpegTsEncHandle
{
public:
    CEs2TSEncap();
    virtual ~CEs2TSEncap();
    int32_t Init(CTsSegEncapHandle* pTsSegHandle,void* pUserData);

    virtual void handleTsStream(const char* pTSData, uint32_t unDataSize,
                                uint32_t ulDuration, void* pUserData);

    virtual void handleVideoEsStream(const char* pEsData, uint32_t unDataSize,
                                uint32_t unTimestamp,bool bkeyFrame);

    virtual void handleAudioEsStream(const char* pEsData, uint32_t unDataSize,
                                uint32_t unTimestamp);

private:
    //判断是否需切片
    void checkSegInfo();

    uint32_t CalcSendDuration();

private:
    CMpegTsEnc                  m_MpegTsEnc;
    CTsSegEncapHandle*          m_pTsSegHandle;
    void*                       m_pUserData;
    uint32_t                    m_ulSegDuration;
    uint32_t                    m_ulSendDuration;
    uint32_t                    m_ulSegStatus;
    time_t                      m_ulLastCheck;
    bool                        m_bOpen;
};

#endif /* __TS_SEGMENT_ENCAP_H_ */

