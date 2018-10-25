#ifndef __CMpegTs_H__
#define __CMpegTs_H__

#include "svs_adapter_time.h"
#include "svs_adapter_mpeg_def.h"
/** The output bit rate in kbit/s */
//#define OUTPUT_BIT_RATE 96000
#define OUTPUT_BIT_RATE 64000
/** The number of output channels */
#define OUTPUT_CHANNELS 1

#define AAC_SAMPLE_RATE 44100

#define SVS_TIME_BASE   1000

#define MAX_EXTRADATA_SIZE ((INT_MAX - 10) / 2)


class CMpegTsEncHandle
{
public:
    CMpegTsEncHandle() {}

    virtual ~CMpegTsEncHandle() {}

    virtual void handleTsStream(const char* pTSData, uint32_t unDataSize,
                                uint32_t ulDuration, void* pUserData) = 0;
};


class CMpegTsEnc
{
public:
    CMpegTsEnc();

    virtual ~CMpegTsEnc();

    int32_t init(CMpegTsEncHandle* pHandle, void* pUserData);

    void release();

    int32_t open();

    void close();

    int32_t writeVideoEsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp,bool bKeyFrame);

    int32_t writeAudioEsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp);

private:
    static int write_buffer(void *opaque, uint8_t *buf, int buf_size);

    int write_ts_data(uint8_t *buf, int buf_size);
private:
    void Reset();
private:
    CMpegTsEncHandle*   m_pPsStreamHandle;
    void*               m_pUserData;

    unsigned char*      m_pPsBuffer;
    uint32_t            m_unBufferSize;
    uint32_t            m_ulTimeTick;
    uint32_t            m_ulLastTimeTick;
};

#endif  /* __CMpegTs_H__ */
