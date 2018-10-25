#ifndef __PACKER_H__
#define __PACKER_H__
#include "svs_adapter_types.h"
#include "svs_adapter_rtp_head.h"

#define PCMU                    0
#define PCMA                    8
#define PCM_HZ                  8000.0
#define PT_ACC                  97
#define PT_H264                 96
#define H264_HZ                 90000.0
#define RTP_FUA                 28
#define PACK_SENDBUF_LEN        1500
#define RTP_STD_LEN             12
#define RTP_HW_MEDIA_EXT_LEN    28
#define RTP_HDR_LEN             12
#define RTP_SSRC                78541

#define RTP_PLAYLOAD_LEN   1400
#define RTP_PACK_BUF_SIZE   1500

#define AU_HEADER_SIZE 4
#define AAC_EXTRA      7
#define AU_HALF_SIZE   2



/* NAL FU unit types */
typedef enum
{
    H264_NALU_TYPE_UNDEFINED    =0,
    H264_NALU_TYPE_IDR          =5,
    H264_NALU_TYPE_SEI          =6,
    H264_NALU_TYPE_SPS          =7,
    H264_NALU_TYPE_PPS          =8,
    H264_NALU_TYPE_STAP_A       =24,
    H264_NALU_TYPE_STAP_B       =25,
    H264_NALU_TYPE_MTAP16       =26,
    H264_NALU_TYPE_MTAP24       =27,
    H264_NALU_TYPE_FU_A         =28,
    H264_NALU_TYPE_FU_B         =29,
    H264_NALU_TYPE_END
}H264_NALU_TYPE;//lint !e751

/* NAL unit types */
enum {
    H264_NAL_UNDEFINED       = 0,
    H264_NAL_SLICE           = 1,
    H264_NAL_DPA             = 2,
    H264_NAL_DPB             = 3,
    H264_NAL_DPC             = 4,
    H264_NAL_IDR_SLICE       = 5,
    H264_NAL_SEI             = 6,
    H264_NAL_SPS             = 7,
    H264_NAL_PPS             = 8,
    H264_NAL_AUD             = 9,
    H264_NAL_END_SEQUENCE    = 10,
    H264_NAL_END_STREAM      = 11,
    H264_NAL_FILLER_DATA     = 12,
    H264_NAL_SPS_EXT         = 13,
    H264_NAL_AUXILIARY_SLICE = 19,
    H264_NAL_END
};

/**
 * Table 7-3: NAL unit type codes
 */
enum HEVCNALUnitType {
    HEVC_NAL_TRAIL_N    = 0,
    HEVC_NAL_TRAIL_R    = 1,
    HEVC_NAL_TSA_N      = 2,
    HEVC_NAL_TSA_R      = 3,
    HEVC_NAL_STSA_N     = 4,
    HEVC_NAL_STSA_R     = 5,
    HEVC_NAL_RADL_N     = 6,
    HEVC_NAL_RADL_R     = 7,
    HEVC_NAL_RASL_N     = 8,
    HEVC_NAL_RASL_R     = 9,
    HEVC_NAL_VCL_N10    = 10,
    HEVC_NAL_VCL_R11    = 11,
    HEVC_NAL_VCL_N12    = 12,
    HEVC_NAL_VCL_R13    = 13,
    HEVC_NAL_VCL_N14    = 14,
    HEVC_NAL_VCL_R15    = 15,
    HEVC_NAL_BLA_W_LP   = 16,
    HEVC_NAL_BLA_W_RADL = 17,
    HEVC_NAL_BLA_N_LP   = 18,
    HEVC_NAL_IDR_W_RADL = 19,
    HEVC_NAL_IDR_N_LP   = 20,
    HEVC_NAL_CRA_NUT    = 21,
    HEVC_NAL_IRAP_VCL22 = 22,
    HEVC_NAL_IRAP_VCL23 = 23,
    HEVC_NAL_RSV_VCL24  = 24,
    HEVC_NAL_RSV_VCL25  = 25,
    HEVC_NAL_RSV_VCL26  = 26,
    HEVC_NAL_RSV_VCL27  = 27,
    HEVC_NAL_RSV_VCL28  = 28,
    HEVC_NAL_RSV_VCL29  = 29,
    HEVC_NAL_RSV_VCL30  = 30,
    HEVC_NAL_RSV_VCL31  = 31,
    HEVC_NAL_VPS        = 32,
    HEVC_NAL_SPS        = 33,
    HEVC_NAL_PPS        = 34,
    HEVC_NAL_AUD        = 35,
    HEVC_NAL_EOS_NUT    = 36,
    HEVC_NAL_EOB_NUT    = 37,
    HEVC_NAL_FD_NUT     = 38,
    HEVC_NAL_SEI_PREFIX = 39,
    HEVC_NAL_SEI_SUFFIX = 40,
    HEVC_NAL_END
};

typedef enum
{
    MPEG4_FRAME_TYPE_I          = 0,
    MPEG4_FRAME_TYPE_P          = 1,
    MPEG4_FRAME_TYPE_B          = 0,
    MPEG4_FRAME_TYPE_END
}MPEG4_FRAME_TYPE;

typedef struct
{
    //byte 0
    uint8_t TYPE:5;
    uint8_t NRI:2;
    uint8_t F:1;
} H264_NALU_HEADER; /**//* 1 BYTES */

typedef struct
{
    //byte 0
    uint8_t LATERID0:1;
    uint8_t TYPE:6;
    uint8_t F:1;
    //byte 1
    uint8_t TID:3;
    uint8_t LATERID1:5;
} H265_NALU_HEADER; /**//* 2 BYTES */


typedef struct
{
    //byte 0
    uint8_t TYPE:5;
    uint8_t NRI:2;
    uint8_t F:1;
} FU_INDICATOR; /**//* 1 BYTES */

typedef struct
{
    //byte 0
    uint8_t TYPE:5;
    uint8_t R:1;
    uint8_t E:1;
    uint8_t S:1;
} FU_HEADER; /**//* 1 BYTES */

class CRtpHandle
{
public:
    CRtpHandle() {}

    virtual ~CRtpHandle() {}

    virtual void handleRtpPacket(const char* pPSData, uint32_t unDataSize) = 0;
};
class CPackager
{
public:
    CPackager();
    virtual ~CPackager();

    virtual int32_t Init (CRtpHandle* pCallBackHandle,uint32_t ulVideoPt,uint32_t ulAudioPt,uint32_t ulVideoClockFreq,uint32_t ulAudioClockFreq);

    virtual int32_t Release();

    virtual int32_t SendRawVideoData  ( const char* pBuf,uint32_t ulBufLen,uint32_t ulTimeStamp ) = 0;

    virtual int32_t SendRawAudioData  ( const char* pBuf,uint32_t ulBufLen,uint32_t ulTimeStamp ) = 0;


protected:
    CRtpHandle* m_pCallBackHandle;
protected:
    char       m_pSendVideoBuf[RTP_PACK_BUF_SIZE];
    char       m_pSendAudioBuf[RTP_PACK_BUF_SIZE];
    uint16_t   m_usCacheAudiFrameCount;
    uint16_t   m_VideoRTPSeq;
    uint16_t   m_AudioRTPSeq;
    uint32_t   m_ulVideoTimeStamp;
    uint32_t   m_ulAudioTimeStamp;
    uint32_t   m_ulVideoPt;
    uint32_t   m_ulAudioPt;
    uint32_t   m_ulVideoClockFreq;
    uint32_t   m_ulAudioClockFreq;
};

class CH264Packager:public CPackager
{
public:
    CH264Packager();
    virtual ~CH264Packager();
public:
    int32_t SendRawVideoData( const char* pBuf,uint32_t ulBufLen ,uint32_t ulTimeStamp);

    int32_t SendRawAudioData( const char* pBuf,uint32_t ulBufLen,uint32_t ulTimeStamp);
private:
    bool FindStartCode( const char* pBuf, uint32_t ulBufLen, uint32_t& ulStartcodeLen );
    bool FindNaluEnd( const char* pBuf,  uint32_t ulBufLen, uint32_t& ulNalLen);
    int32_t SendNaluVideoData(const char* pBuf,uint32_t ulBufLen ,uint32_t ulTimeStamp);

};

#endif

