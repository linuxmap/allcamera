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
    ES_TYPE_VIDEO,
    ES_TYPE_AUDIO,

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

#define H264_START_CODE 4
#define H265_START_CODE 4


#define PS_HEAD_BUF_SIZE_MAX   32

#pragma pack(push, 1)
union littel_endian_size
{
    unsigned short int  length;
    unsigned char       byte[2];
};
struct pack_start_code
{
    unsigned char start_code[3];
    unsigned char stream_id[1];
};
struct program_stream_pack_header
{
    pack_start_code PackStart;// 4

    uint8_t         system_clock_reference_base21:2;
    uint8_t         marker_bit:1;
    uint8_t         system_clock_reference_base1:3;
    uint8_t         fix_bit:2;    //'01'

    uint8_t         system_clock_reference_base22;

    uint8_t         system_clock_reference_base31:2;
    uint8_t         marker_bit1:1;
    uint8_t         system_clock_reference_base23:5;

    uint8_t         system_clock_reference_base32;

    uint8_t         system_clock_reference_extension1:2;
    uint8_t         marker_bit2:1;
    uint8_t         system_clock_reference_base33:5; //system_clock_reference_base 33bit

    uint8_t         marker_bit3:1;
    uint8_t         system_clock_reference_extension2:7; //system_clock_reference_extension 9bit

    uint8_t         program_mux_rate1;

    uint8_t         program_mux_rate2;

    uint8_t         marker_bit5:1;
    uint8_t         marker_bit4:1;
    uint8_t         program_mux_rate3:6;

    uint8_t         pack_stuffing_length:3;
    uint8_t         reserved : 5;
};
struct program_system_pack_header
{
    pack_start_code     PackStart;// 4
    littel_endian_size  PackLength;//2

    uint32_t            marker_bit1:1;//1
    uint32_t            rate_bound:22;
    uint32_t            marker_bit2:1;
    uint32_t            audio_bound:6;
    uint32_t            fixed_flag:1;
    uint32_t            CSPS_flag:1;

    uint16_t system_audio_lock_flag:1;
    uint16_t system_video_lock_flag:1;
    uint16_t            marker_bit3:1;
    uint16_t            video_bound:5;
    uint16_t packet_rate_restriction_flag:1;
    uint16_t            reserved_bits:7;

    uint8_t             streamid;
    uint8_t             reserved[5];
};//18

struct program_elementary_stream_info
{
    uint8_t             stream_type;
    uint8_t             elementary_stream_id;
    uint8_t             elementary_stream_info_length[2];
};
struct program_elementary_stream_map_info
{
    uint8_t             elementary_stream_map_length[2];
};


struct program_stream_map
{
    pack_start_code    PackStart;
    littel_endian_size PackLength;//we mast do exchange

    uint8_t            program_stream_map_version:5;
    uint8_t            reserved1:2;
    uint8_t            current_next_indicator:1;

    uint8_t            marker_bit:1;
    uint8_t            reserved2:7;

    uint8_t            program_stream_info_length[2];

};
struct program_private_head
{
    pack_start_code PackStart;
    littel_endian_size PackLength;//we mast do exchange
};
struct program_stream_e
{
    pack_start_code     PackStart;
    littel_endian_size  PackLength;//we mast do exchange
    char                PackInfo1[2];
    unsigned char       stuffing_length;
};


#define RTP_PLAYLOAD_LEN   1400
#define RTP_PACK_BUF_SIZE   1500

#define AU_HEADER_SIZE 4
#define AAC_EXTRA      7
#define AU_HALF_SIZE   2

struct es_frame_info
{
    uint8_t            payload;
    uint32_t           length;
    uint8_t            keyframe;
    uint8_t            streamId;
    uint32_t           timestamp;
    uint32_t           ssrc;
    uint32_t           bVideo;
};

#pragma pack(pop)





#endif /* MPEGDEF_H_ */
