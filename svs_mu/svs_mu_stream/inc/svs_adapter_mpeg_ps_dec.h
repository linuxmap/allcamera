/*
 * MpegPsDec.h
 *
 *  Created on: 2012-12-18
 */

#ifndef MPEGPSDEC_H_
#define MPEGPSDEC_H_

#include "svs_adapter_mpeg_def.h"
#include "svs_adapter_types.h"
#include "svs_adapter_time.h"

class CMpegPsDecHandle
{
public:
    CMpegPsDecHandle() {}

    virtual ~CMpegPsDecHandle() {}

    virtual void handleVideoEsStream(const char* pEsData, uint32_t unDataSize,
                                uint32_t unTimestamp,uint8_t ucStreamType, void* pUserData) = 0;

    virtual void handleAudioEsStream(const char* pEsData, uint32_t unDataSize,
                                uint32_t unTimestamp,uint8_t ucStreamType, void* pUserData) = 0;
};

class CMpegPsDec
{
public:
    CMpegPsDec();

    virtual ~CMpegPsDec();

    int32_t init(CMpegPsDecHandle* pHandle, void* pUserData);

    int32_t writePsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp);

    void release();
private:
    int32_t StripPSHeader();

    int32_t StripSysHeader();

    int32_t StripPsmPacket();

    int32_t StripPESHeader();

    uint32_t getStartCode()const;
private:
    CMpegPsDecHandle*       m_pEsStreamHandle;
    void*                   m_pUserData;
    ES_TYPE_E               m_enCurType;
    char*                   m_pCurMediaData;
    uint32_t                m_ulCurMediaLen;
};

#endif /* MPEGPSDEC_H_ */
