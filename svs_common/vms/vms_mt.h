#ifndef _SVS_MT_H
#define _SVS_MT_H

#define SVS_MSG_TYPE_ERROR                0x00000001

#define HEARTBEAT_SEND_INTERVAL           60
#define HEARTBEAT_CHECK_INTERVAL          180


enum SVS_MSG_TYPE_LIST
{

    //======================================v1.0.0 begin==========================================
    SVS_MSG_TYPE_COMMON_RESP                                    = 0x00000001,
    SVS_MSG_TYPE_HEARTBEAT                                      = 0x00000002,
    SVS_MSG_TYPE_SERVER_REGIST_REQ                              = 0x00000003,
    SVS_MSG_TYPE_SERVER_REGISTER_RESP                           = 0x00000004,
    SVS_MSG_TYPE_MU_SESSION_REPORT_REQ                          = 0x00000007,
    SVS_MSG_TYPE_MU_SESSION_REPORT_RESP                         = 0x00000008,
    SVS_MSG_TYPE_MDU_LOAD_INFO_REQ                              = 0x00000009,
    SVS_MSG_TYPE_MDU_LOAD_INFO_RESP                             = 0x0000000a,
    SVS_MSG_TYPE_MDU_SESSION_SETUP_REQ                          = 0x0000000b,
    SVS_MSG_TYPE_MDU_SESSION_SETUP_RESP                         = 0x0000000c,
    SVS_MSG_TYPE_MDU_SESSION_PLAY_REQ                           = 0x0000000d,
    SVS_MSG_TYPE_MDU_SESSION_PLAY_RESP                          = 0x0000000e,
    SVS_MSG_TYPE_MDU_SESSION_TEARDOWN_REQ                       = 0x0000000f,
    SVS_MSG_TYPE_MDU_SESSION_TEARDOWN_RESP                      = 0x00000010,
    SVS_MSG_TYPE_PLAYBACK_CONTROL_REQ                           = 0x00000011,
    SVS_MSG_TYPE_PLAYBACK_CONTROL_RESP                          = 0x00000012,
    SVS_MSG_TYPE_FILE_END_NOTE_REQ                              = 0x00000013,
    SVS_MSG_TYPE_FILE_END_NOTE_RESP                             = 0x00000014,
    SVS_MSG_TYPE_MEDIA_KEYFRAME_REQ                             = 0x00000015,

    SVS_MSG_TYPE_LAST                                           = 0xFFFFFFFD
};

#endif //_SVS_MT_H

