#ifndef __CMPEGPS_H__
#define __CMPEGPS_H__

#include "svs_adapter_time.h"
#include "svs_adapter_mpeg_def.h"


class CMpegPsEncHandle
{
public:
    CMpegPsEncHandle() {}

    virtual ~CMpegPsEncHandle() {}

    virtual void handlePsStream(const char* pPSData, uint32_t unDataSize,
                                uint32_t unTimestamp, void* pUserData, bool endFlag) = 0;
};


class CMpegPsEnc
{
public:
    CMpegPsEnc();

    virtual ~CMpegPsEnc();

    int32_t init(CMpegPsEncHandle* pHandle, void* pUserData);

    int32_t writeVideoEsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp);

    int32_t writeAudioEsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp);

    void release();
//private:
    int32_t writeESPacket(const char* pInData, uint32_t unDataSize, ES_TYPE_E enEsType);

    int32_t writePSHeader();

    int32_t writeSysHeader();

    int32_t writePsmPacket();

    int32_t writePESHeader(uint32_t unDataSize, uint8_t unStreamId,
                       uint64_t ullPts, uint64_t ullDts);

    void calcEsBitrate(uint32_t unDataSize);

    uint32_t getWritePos()const;

private:
    CMpegPsEncHandle*   m_pPsStreamHandle;
    void*               m_pUserData;

    uint8_t*            m_pPsBuffer;
    uint32_t            m_unBufferSize;
    uint8_t*            m_pWritePos;

    uint64_t            m_ullScr;
    uint64_t            m_ullVideoPts;
    uint64_t            m_ullAudioPts;
    uint32_t            m_unMuxRate;

    uint32_t            m_unVideoTimestamp;
    uint32_t            m_unAudioTimestamp;
    uint32_t            m_unEsStreamSize;
    uint32_t            m_LastCalcTime;
    bool                m_isFirstPES;
};

#endif  /* __CMPEGPS_H__ */
