#ifndef __SVS_MEDIA_H__
#define __SVS_MEDIA_H__
#include <vms/vms.h>

#pragma pack(push, 1)

enum FRAME_TYPE
{
    FRAME_TYPE_VIDEO                    = 0x01,
    FRAME_TYPE_AUDIO                    = 0x02,
    FRAME_TYPE_NAT_REQ                  = 0x03,
    FRAME_TYPE_NAT_RESP                 = 0x04,
    FRAME_TYPE_PLAYBACK_CONTROL_REQ     = 0x05,
    FRAME_TYPE_PLAYBACK_CONTROL_RESP    = 0x06,
    FRAME_TYPE_EOS                      = 0x07,
    FRAME_TYPE_BOS                      = 0x08
};


enum KEY_FRAME_FLAG
{
    KEY_FRAME_FLAG_COMMON       = 0x00,
    KEY_FRAME_FLAG_KEY          = 0x01,

};


enum PLAY_RATE
{
    PLAY_RATE_MULT_1       = 0x10001,
    PLAY_RATE_MULT_2       = 0x10002,
    PLAY_RATE_MULT_4       = 0x10004,
    PLAY_RATE_MULT_8       = 0x10008,
    PLAY_RATE_MULT_16      = 0x10010,
    PLAY_RATE_MULT_32      = 0x10020,
    PLAY_RATE_DIV_2        = 0x20002,
    PLAY_RATE_DIV_4        = 0x20004,
    PLAY_RATE_DIV_8        = 0x20008,
    PLAY_RATE_DIV_16       = 0x20010,
    PLAY_RATE_DIV_32       = 0x20020,
    PLAY_RATE_SINGLE_FRAME = 0x30001
};

enum PLAY_TIME_TYPE
{
    ABSOLUTE_TIME,
    RELATIVE_TIME

};

enum OPER_CODE
{

    OPER_CODE_SPEED      = 0x01,
    OPER_CODE_BACK       = 0x02,
    OPER_CODE_DRAG       = 0x03,
    OPER_CODE_PAUSE      = 0x04,
    OPER_CODE_CONTINUE   = 0x05,
    OPER_CODE_STOP       = 0x06,
    OPER_CODE_QUERY_TIME = 0x07
};


typedef struct _COMMAND_V1_VALUE
{
    uint32_t PlayTimeType;
    uint32_t PlayStartTime;
    uint32_t PlayStopTime;
    uint32_t PlayRate;
}COMMAND_v1_VALUE;

typedef struct _PLAYBACK_V1_COMMAND
{
    uint32_t            OperCode;
    uint32_t            OperSeq;
    COMMAND_v1_VALUE    Value;
}PLAYBACK_V1_COMMAND;

#pragma pack(pop)

#endif /*__SVS_MEDIA_H__*/


