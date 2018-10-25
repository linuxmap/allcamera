/*
 * MpegDef.h
 *
 *  Created on: 2012-12-18
 */

#ifndef MPEGDEF_H_
#define MPEGDEF_H_

#include "svs_adapter_types.h"

//#define MPEG_BUFFER_SIZE (2*1024*1024)


#define HLS_SEND_DURATION_MIN       2
#define HLS_SEND_DURATION_MAX       10
#define HLS_SEND_DURATION_INTER     5

enum TS_SEG_TAG
{
    TS_SEG_TAG_START   = 0x0,
    TS_SEG_TAG_MIDDLE  = 0x1,
    TS_SEG_TAG_END     = 0x2
};

#define TIME_TICK_SECOND 1000



typedef enum
{
    PS_H264_NALU_TYPE_UNDEFINED    =0,
    PS_H264_NALU_TYPE_IDR          =5,
    PS_H264_NALU_TYPE_SEI          =6,
    PS_H264_NALU_TYPE_SPS          =7,
    PS_H264_NALU_TYPE_PPS          =8,
    PS_H264_NALU_TYPE_AUD          =9,
    PS_H264_NALU_TYPE_END
}PS_H264_NALU_TYPE;//lint !e751

typedef struct
{
    //byte 0
    unsigned char TYPE:5;
    unsigned char NRI:2;
    unsigned char F:1;
} PS_NALU_HEADER; /**//* 1 BYTES */

typedef struct
{
    //byte 0
    unsigned char TYPE:5;
    unsigned char NRI:2;
    unsigned char F:1;
} PS_FU_INDICATOR; /**//* 1 BYTES */

typedef struct
{
    //byte 0
    unsigned char TYPE:5;
    unsigned char R:1;
    unsigned char E:1;
    unsigned char S:1;
} PS_FU_HEADER; /**//* 1 BYTES */

typedef enum _enES_TYPE_E
{
    ES_TYPE_VIDEO,       // ��������ES
    ES_TYPE_AUDIO,       // ��������ES

    ES_TYPE_MAX
}ES_TYPE_E;

#define MAX_ENCAP_RTP_SIZE    1418

#define PS_RTP_PAYLOAD_TYPE   96


#define VIDEO_STREAM_ID     0xe0
#define AUDIO_STREAM_ID     0xc0

#define VIDEO_STREAM_TYPE_MPEG4  0x10
#define VIDEO_STREAM_TYPE_H264   0x1b
#define VIDEO_STREAM_TYPE_H265   0x24
#define VIDEO_STREAM_TYPE_SVAC   0x80


#define AUDIO_STREAM_TYPE_G711   0x90
#define AUDIO_STREAM_TYPE_G722   0x92
#define AUDIO_STREAM_TYPE_G723   0x93
#define AUDIO_STREAM_TYPE_SVAC   0x98
#define AUDIO_STREAM_TYPE_G729   0x99


#define DEFAULT_PS_CACHE    (1024 * 1024)
#define DEFAULT_TS_CACHE    (1024 * 1024)
#define INVALID_TIMESTAMP   0x8000000000000000LL
#define MAX_PS_HEADER_LEN   128

#define GET_BITS(x, s, e) ( (x >> s) & ~( ~0 << (((e) - (s)) + 1) ) )


#define PS_STARTCODE_LEN    4
#define PS_HEAD_LEN         14
#define SYS_HEAD_LEN_MIN    6
#define PSM_HEAD_LEN_MIN    6

//StreamType
#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_AUDIO_AAC_LATM  0x11
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_VIDEO_H264      0x1b
#define STREAM_TYPE_VIDEO_HEVC      0x24
#define STREAM_TYPE_VIDEO_CAVS      0x42
#define STREAM_TYPE_VIDEO_VC1       0xea
#define STREAM_TYPE_VIDEO_DIRAC     0xd1

#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x82
#define STREAM_TYPE_AUDIO_TRUEHD    0x83


#define AUDIO_ID 0xc0
#define VIDEO_ID 0xe0
#define AC3_ID   0x80
#define DTS_ID   0x8a
#define LPCM_ID  0xa0
#define SUB_ID   0x20


#define PS_STARTCODE_INVAL          ((int32_t)0xFFFFFFFF)

#define PS_STARTCODE                ((int32_t)0x000001)

#define PACK_START_CODE             ((int32_t)0x000001ba)
#define SYSTEM_HEADER_START_CODE    ((int32_t)0x000001bb)
#define SEQUENCE_END_CODE           ((int32_t)0x000001b7)
#define PACKET_START_CODE_MASK      ((int32_t)0xffffff00)
#define PACKET_START_CODE_PREFIX    ((int32_t)0x00000100)
#define ISO_11172_END_CODE          ((int32_t)0x000001b9)

/* mpeg2 */
#define PROGRAM_STREAM_MAP          ((int32_t)0x000001bc)
#define PRIVATE_STREAM_1            ((int32_t)0x000001bd)
#define PADDING_STREAM              ((int32_t)0x000001be)
#define PRIVATE_STREAM_2            ((int32_t)0x000001bf)

#define H264_PES_STREAM_CODE        ((int32_t)(PACKET_START_CODE_PREFIX|VIDEO_STREAM_ID))
#define G711_PES_STREAM_CODE        ((int32_t)(PACKET_START_CODE_PREFIX|AUDIO_STREAM_ID))

#define TS_FEC_PACKET_SIZE 204
#define TS_DVHS_PACKET_SIZE 192
#define TS_PACKET_SIZE 188
#define TS_MAX_PACKET_SIZE 204

#define NB_PID_MAX 8192
#define MAX_SECTION_SIZE 4096

/* pids */
#define PAT_PID                 0x0000
#define SDT_PID                 0x0011

/* table ids */
#define PAT_TID   0x00
#define PMT_TID   0x02
#define M4OD_TID  0x05
#define SDT_TID   0x42

#define MPEG_BUFFER_SIZE (64*TS_PACKET_SIZE)




#endif /* MPEGDEF_H_ */
