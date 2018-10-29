#ifndef _SVS_MSG_H_
#define _SVS_MSG_H_
#include <vms/vms_mt.h>
#include <vms/vms_adt.h>
#include <vms/vms_df.h>
#include <vms/vms_cm.h>

#pragma pack(push, 1)




typedef uint64_t uint64_svs;


#define SVS_MSG_PROTOCOL_VERSION   0x0100
#define ID_LEN                      32
#define SERVER_ID_LEN               32
#define USER_ID_LEN                 32
#define DEVICE_ID_LEN               32
#define SAFETY_INFO_LEN             16
#define PATH_LEN                    128
#define POP_ID_LEN                  8
#define SERVER_NAME_LEN             128
#define AUDIO_PROFILE_LEN           448
#define ALARM_INFO_LEN              256
#define COMPRESSION_VIDEO_LNE       20
#define FLOW_ID_LNE                 64
#define CODEC_LEN                   64

#define DEVICE_SN_LEN               128
#define DEVICE_LOGIN_NAME_LEN       32
#define DEVICE_LOGIN_DOMAIN_LEN     128
#define DEVICE_PASSWORD             128
#define DEVICE_NAME_LEN             128
#define RESOLUTION_LEN              64
#define DEV_VERSION_LEN             16
#define PLAT_VERSION_LEN            32
#define DEV_FAC_LEN                 128
#define PLAT_FAC_LEN                32
#define UTC_TIME_LEN                32
#define TIME_ZONE_LEN               16
#define DEVICE_TYPE_LEN             32
#define SERIAL_NUMBER_LEN           128
#define SOFT_VERSION_LEN            256
#define RTSP_URL_LEN                512
#define PTZ_PARAM_LEN               128
#define SERVICE_TYPE_LEN            128
#define UPLOAD_URL_LEN              256
#define SESSION_ID_LEN              32
#define SNAP_TIME_LEN               16
#define IMAGE_LOCATION_LEN          16
#define SIM_CARD_LEN                32
#define SENSOR_DATA_PARAM_LEN       128
#define ALARM_TYPE_LEN              64
#define ALARM_ID_LEN                64
#define ALARM_DESCRIPTION_LEN       256
#define SDP_MSG_LENS                2048


#define REALM_LEN                    128
#define NONCE_LEN                    32
#define URI_LEN                      128
#define CNONCE_LEN                   128
#define NC_LEN                       128
#define DEV_OPAQUE_LEN               128
#define RESPONSE_LEN                 128
#define ALGORITHM_LEN                32
#define QOP_LEN                      128


#define AUTH_PASSWD_LEN            255


#define MediaAnalyseAlarmInfoLength 256
#define MaxAlarmTypeLength          64
#define SVS_TIME_STRING_LEN         15

#define ENCRYPT_KEY_LEN             32

#define SVS_VOD_TYPE_FRONT    "front:"
#define SVS_VOD_TYPE_PLAT     "plat:"


enum SVS_TLV_TAG
{
    SVS_TLV_TAG_SDP                     = 0x9001,
    SVS_TLV_TAG_RTSPURL                 = 0x9002,
    SVS_TLV_TAG_IP                      = 0x9003,
    SVS_TLV_TAG_SESSIONID_R1            = 0x9004,
    SVS_TLV_TAG_ENCODER_TYPE            = 0x9005,
    SVS_TLV_TAG_RTMPURL                 = 0x9006
};


enum SVS_MSG_RETURN_CODE
{
    SVS_MSG_OK                              = 0,
    SVS_MSG_MU_INTERNAL_ERR                 = 0x2100,
    SVS_MSG_VERSION_ERR                     = 0x2101,
    SVS_MSG_TYPE_ERR                        = 0x2102,
    SVS_MSG_LENGTH_ERR                      = 0x2103,
    SVS_MSG_MU_ID_ERR                       = 0x2104,
    SVS_MSG_FIELD_ERR                       = 0x2105,

    SVS_MSG_MU_RESIGER_MU_ID_EXIST          = 0x2201,
    SVS_MSG_MU_RESIGER_POP_ID_ERR           = 0x2202,
    SVS_MSG_MU_RESIGER_SERVER_TYPE_ERR      = 0x2203,
    SVS_MSG_MU_RESIGER_SERVICE_TYPE_ERR     = 0x2204,
    SVS_MSG_MU_RESIGER_SCC_NOT_ALREADY_ERR  = 0x2205,
    SVS_MSG_MU_RESIGER_SCC_BUSY_ERR         = 0x2206,
    SVS_MSG_MU_RESIGER_DB_ERR               = 0x2207,
    SVS_MSG_MU_RESIGER_EXCEED_MAX_CNT_ERR   = 0x2208,
    SVS_MSG_MU_RESIGER_PARA_ERR             = 0x2209,
    SVS_MSG_SERVER_RESIGER_SERVER_NOT_EXIST = 0x220a,
    SVS_MSG_SERVER_RESIGER_STREAM_NO_IP_LIST   = 0x220b,
    SVS_MSG_SERVER_RESIGER_AUTH_FAIL        = 0x220c,

    SVS_MSG_RECORD_TASK_EXIST_ERR           = 0x2300,
    SVS_MSG_RECORD_TASK_TYPE_ERR            = 0x2301,
    SVS_MSG_RECORD_TASK_STREAMID_ERR        = 0x2302,
    SVS_MSG_RECORD_TASK_MDUADDR_ERR         = 0x2303,
    SVS_MSG_RECORD_TASK_MRUOVERLOAD_ERR     = 0x2304,
    SVS_MSG_RECORD_TASK_PURECORDTIME_ERR    = 0x2305,

    SVS_MSG_RECORD_OVER_RECORDTYPE_ERR      = 0x2310,
    SVS_MSG_RECORD_OVER_NOTEXIST_ERR        = 0x2311,
    SVS_MSG_RECORD_OVER_STREAMID_ERR        = 0x2312,
    SVS_MSG_RECORD_OVER_VIDEOSUBSTREAMID_ERR= 0x2313,

    SVS_MSG_RECORD_DEL_INDEX_FILE_INVALID   = 0x2320,
    SVS_MSG_RECORD_DEL_MEDIA_FILE_INXISTENT = 0x2321,
    SVS_MSG_RECORD_DEL_TIME_SPAN_INVALID    = 0x2322,

    SVS_MSG_RECORD_INDEX_REPORT_PARAMETER_INVALID  = 0x2330,
    SVS_MSG_RECORD_INDEX_REPORT_SCC_DB_ERROR       = 0x2331,


    SVS_MSG_STREAM_TIMESPAN_INVALID          = 0x2400,
    SVS_MSG_STREAM_VIDEOTYPE_INVALID         = 0x2401,
    SVS_MSG_STREAM_BUSINESSTYPE_INVALID      = 0x2402,
    SVS_MSG_STREAM_STREAMID_INVALID          = 0x2403,
    SVS_MSG_STREAM_CREATE_RESOURCE_FAIL      = 0x2404,
    SVS_MSG_STREAM_MEDIA_FILE_ABNORMAL       = 0x2405,
    SVS_MSG_STREAM_INDEX_FILE_ABNORMAL       = 0x2406,
    SVS_MSG_STREAM_STREAM_CONN_ABNORMAL         = 0x2407,
    SVS_MSG_STREAM_PLAYBACK_PARAM_INVALID    = 0x2408,

    SVS_MSG_STREAM_DOWNLOAD_REACH_MAX        = 0x2410,
    SVS_MSG_STREAM_PLAYBACK_REACH_MAX        = 0x2411,
    SVS_MSG_STREAM_STATE_ABNORMAL            = 0x2412,
    SVS_MSG_STREAM_BACKUP_REACH_MAX          = 0x2413,

    SVS_MSG_STREAM_STREAMID_NOT_MATCH           = 0x2501,
    SVS_MSG_STREAM_DEVICEID_NOT_MATCH           = 0x2502,
    SVS_MSG_STREAM_SUBSTREAMID_NOT_MATCH        = 0x2503,
    SVS_MSG_STREAM_STREAMID_EXIST               = 0x2504,
    SVS_MSG_STREAM_RECV_STREAMID_ERROR          = 0x2505,
    SVS_MSG_STREAM_RESOURCE_USEDUP              = 0x2506,
    SVS_MSG_STREAM_MAX_COPY_STREAM              = 0x2507,

    SVS_MSG_RETURN_CODE_MAX
};

enum PLAY_URL_TYPE
{
    PLAY_URL_TYPE_DEFAULT,
    PLAY_URL_TYPE_RTSP,
    PLAY_URL_TYPE_HLS,
    PLAY_URL_TYPE_RTMP,
    PLAY_URL_TYPE_MAX
};

enum PLAY_TYPE
{
    PLAY_TYPE_LIVE,
    PLAY_TYPE_PLAT_RECORD,
    PLAY_TYPE_FRONT_RECORD,
    PLAY_TYPE_MAX
};

enum DEV_STREAM_TYPE
{
    DEV_STREAM_TYPE_MAIN,
    DEV_STREAM_TYPE_SUB,
    DEV_STREAM_MAX
};

enum SERVER_TYPE
{
    SERVER_TYPE_ACCESS_CONTROL         = 0x00,
    SERVER_TYPE_MU_STREAM              = 0x01,
    SERVER_TYPE_MU_RECORD              = 0x02,
    SERVER_TYPE_MAX
};


enum RESET_FLAGS
{
    RESET_FLAGS_RUN         = 0x00,
    RESET_FLAGS_RESET       = 0x01
};

enum MEDIA_TRANS_TYPE
{
    MEDIA_TRANS_TYPE_TCP           = 0x0,
    MEDIA_TRANS_TYPE_UDP           = 0x1,
    MEDIA_TRANS_TYPE_SVS           = 0x2,
    MEDIA_TRANS_TYPE_RTP           = 0x3,
    MEDIA_TRANS_TYPE_PS            = 0x4,
    MEDIA_TRANS_TYPE_HLS           = 0x5,
    MEDIA_TRANS_TYPE_RTMP          = 0x6,
    MEDIA_TRANS_TYPE_MAX
};

enum SESSION_TYPE
{
    RTP_SESSION,
    RTSP_SESSION,
    RTMP_SESSION,
    HLS_SESSION,
    EHOME_SESSION,
    SESSION_TYPE_MAX
};


enum PEER_TYPE
{
    PEER_TYPE_PU                 = 0x01,     // PU
    PEER_TYPE_CU                 = 0x02,     // CU
    PEER_TYPE_RECORD             = 0x03,     // RECORD
    PEER_TYPE_STREAM             = 0x04,     // STREAM
    PEER_TYPE_MAX
};


enum TRANS_DIRECTION
{
    TRANS_DIRECTION_RECVONLY     = 0x1,
    TRANS_DIRECTION_SENDONLY     = 0x2,
    TRANS_DIRECTION_SENDRECV     = 0x3,
    TRANS_DIRECTION_MAX
};

enum CONN_SETUP_TYPE
{
    CONN_SETUP_TYPE_ACTIVE       = 0x1,
    CONN_SETUP_TYPE_PASSIVE      = 0x2,
    CONN_SETUP_TYPE_MAX
};


enum INTERLEAVE_TYPE
{
    INTERLEAVE_TYPE_NONE         = 0x0,
    INTERLEAVE_TYPE_RTP_OVER_TCP = 0x1,
    INTERLEAVE_TYPE_MAX
};


enum SERVICE_NET_TYPE
{
    SERVICE_NET_TYPE_FIX    = 0x01,
    SERVICE_NET_TYPE_MOTION = 0x02,
    SERVICE_NET_TYPE_BOTH   = 0x03
};


enum REPORT_MEDIA_TASK_FLAG
{
    REPORT_MEDIA_TASK_REPORT   = 0x01,
    REPORT_MEDIA_TASK_UNREPORT = 0x02
};


enum PORT_REUSED_FLAG
{
    PORT_REUSED         = 0x01,
    PORT_UNREUSED       = 0x02,
    PORT_REUSED_FLAG_MAX
};




enum TRANS_PROTOCOL
{
    TRANS_PROTOCAL_TCP   = 0x01,
    TRANS_PROTOCAL_UDP   = 0x02,
    TRANS_PROTOCAL_MAX
};


enum NET_TYPE
{
    NET_TYPE_V1_MOTION     = 0x01,
    NET_TYPE_V1_FIX        = 0x02,
    NET_TYPE_V1_MAX
};


enum SUPPORT_QOS_FLAG
{
    SUPPORT_QOS_FLAG_TRUE    = 0x01,
    SUPPORT_QOS_FLAG_FALSE   = 0x02,
    SUPPORT_QOS_FLAG_MAX
};


enum RECORD_TYPE
{
    RECORD_TYPE_RT           = 0x01,
    RECORD_TYPE_FRONT_BACKUP = 0x02
};


enum ROUTEWAY_STATUS
{
    ROUTEWAY_STATUS_READY  = 0x00,
    ROUTEWAY_STATUS_ERROR  = 0x01
};


enum OVER_CAUSE
{
    OVER_CAUSE_SUCCESSED   = 0x00,
    OVER_CAUSE_EXCEPTION   = 0x01
};



enum RECORD_TASK_TYPE_V1
{
    RECORD_TASK_V1_REAL_VIDEO       = 0x01,
    RECORD_TASK_V1_PU_BACKUP_VIDEO  = 0x02,
    RECORD_TASK_V1_XONE_VIDEO       = 0x03,
    RECORD_TASK_V1_PRERECORD        = 0x04,
    RECORD_TASK_V1_BACKUP_RECORD    = 0x05
};

enum REPORT_TYPE
{
    REPORT_TYPE_INDEX_START      = 0x01,
    REPORT_TYPE_INDEX_EDN        = 0x02,
    REPORT_TYPE_RECORDING_SPACE  = 0x03
};


enum DELETE_TYPE
{
    DELETE_TYPE_OVERDUE          = 0x01,
    DELETE_TYPE_PRERECORD        = 0x02,
    DELETE_TYPE_OVERRECORD       = 0x03
};


enum ALARM_TYPE
{
    ALARM_TYPE_DISKSPACE_LACK    = 0x01,
    ALARM_TYPE_RECORD            = 0x02,
    ALARM_TYPE_PLAYBACK          = 0x03,
    ALARM_TYPE_DOWNLOAD          = 0x04,
    ALARM_TYPR_MEDIA_DISTRIBUTE  = 0x05,
    ALARM_TYPR_MEDIA_ANALYSE     = 0X06
};


enum ALARM_CODE
{
    ALARM_DISK_FULL                   = 0x00010001,
    ALARM_DISK_OFFLINE                = 0x00010002,
    ALARM_DISK_OTHER                  = 0x00010099,

    ALARM_RECORD_STREAM_WRITE_ERROR   = 0x00020001,
    ALARM_RECORD_STREAM_RECEIVE_ERROR = 0x00020002,
    ALARM_RECORD_STREAM_NAT_FAIL         = 0x00020003,
    ALARM_RECORD_MRU_INSIDE_ERROR     = 0x00020099,

    ALARM_PLAYBACK_VIDEO_READ_ERROR   = 0x00030001,
    ALARM_PLAYBACK_VIDEO_SEND_ERROR   = 0x00030002,
    ALARM_PLAYBACK_STREAM_NAT_FAIL       = 0x00030003,
    ALARM_PLAYBACK_PAUSE_TIMEOUT      = 0x00030004,
    ALARM_PLAYBACK_MEDIA_HEARTBEAT_TIMEOUT = 0x00030005,
    ALARM_PLAYBACK_MRU_INSIDE_ERROR   = 0x00030099,



    ALARM_STREAM_SESSION_CLOSED          = 0x00040000,
    ALARM_STREAM_SESSION_TIMEOUT         = 0x00040001,
    ALARM_STREAM_STREAM_TIMEOUT          = 0x00040002,
    ALARM_STREAM_SESSION_ABNORMAL        = 0x00040003,
    ALARM_MAU_MEDIA_ANALYSE_FAIL      = 0x00060001,
    ALARM_MAU_MEDIA_DECODE_FAIL       = 0x00060002,
    ALARM_MAU_MEDIA_TIMEOUT           = 0x00060003,
    ALARM_MAU_MEDIA_SESSION_ABNORMAL  = 0x00060004,

};


enum TRANS_TYPE
{
    TRANS_TYPE_TRANS_MDU,
    TRANS_TYPE_DIRECT_CU
};


enum BUSINESS_TYPE_V1
{
    BUSINESS_TYPE_V1_SCC_PLAYBACK        = 0x01,
    BUSINESS_TYPE_V1_SCC_DOWNLOAD        = 0x02,
    BUSINESS_TYPE_V1_PU_PLAGBACK         = 0x03,
    BUSINESS_TYPE_V1_PU_DOWNLOAD         = 0x04,
    BUSINESS_TYPE_V1_PU_SNAP_DOWNLOAD    = 0x05,
    BUSINESS_TYPE_V1_PU_SNAP_PLAYBACK    = 0x06,
    BUSINESS_TYPE_V1_BACKUP_PLAYBACK     = 0x07,
    BUSINESS_TYPE_V1_BACKUP_DOWNLOAD     = 0x08,
    BUSINESS_TYPE_V1_SCC_BACKUP          = 0x09
};


enum ENCRYPT_FALG
{
    ENCRYPT_DISABLE = 0,
    ENCRYPT_ENABLE  = 1
};


enum ENCRYPT_MODE
{
    ENCRYPT_KEY_FRAME = 1,
    ENCRYPT_ALL_FRAME = 2
};


enum ENCRYPT_ALGORITHM
{
    ENCRYPT_ALGORITHM_AES128 = 1,
    ENCRYPT_ALGORITHM_AES192 = 2,
    ENCRYPT_ALGORITHM_AES256 = 3
};


enum FILE_ENCRYPT_FALG
{
    FILE_ENCRYPT_ENABLE  = 1,
    FILE_ENCRYPT_DISABLE = 2
};


enum BACKUP_FINISH_TYPE
{
    BACKUP_FINISH_TYPE_SUCCESS = 0x01,
    BACKUP_FINISH_TYPE_FAIL    = 0x02,
    BACKUP_FINISH_TYPE_MAX
};

enum RECORD_SESSION_TYPE
{
    RECORD_SESSION_TYPE_MAIN    = 0,
    RECORD_SESSION_TYPE_ALARM   = 1,
    RECORD_SESSION_TYPE_MAX,
};

enum SETUP_TYPE
{
    SETUP_TYPE_DISTRIBUTE  = 0,//only distibute,not call the dev
    SETUP_TYPE_CALL_DEV    = 1,//call the dev
    SETUP_TYPE_MAX
};


typedef struct _SVS_TLV_V1
{
        uint32_t    Tag;
        uint32_t    Length;
        void*       Value;
}SVS_TLV_V1 ,*PSVS_TLV_V1;


typedef struct _TIME_SPAN
{
    uint32_t StartTime;
    uint32_t EndTime;
}TIME_SPAN;


enum STREAM_IP_TYPE
{
    STREAM_IP_TYPE_INTERNAL            = 0,
    STREAM_IP_TYPE_EXTERNAL            = 1,
    STREAM_IP_TYPE_INTERNAL_REPORT     = 2,
    STREAM_IP_TYPE_EXTERNAL_REPORT     = 3,
    STREAM_IP_TYPE_MAX
};


typedef struct _SVS_STREAM_IP
{
        uint32_t    uiIpType;
        uint32_t    uiIndex;
        char        szIp[SVS_IP_LEN];
}SVS_STREAM_IP ,*PSVS_STREAM_IP;


typedef struct _SVS_MSG_COMMON_RESP
{
    SVS_MSG_HEADER  MsgHeader;
    uint32_t        RequestType;
    uint32_t        RespCode;
}SVS_MSG_COMMON_RESP;


typedef struct _SVS_MSG_HEARTBEAT
{
    SVS_MSG_HEADER MsgHeader;
}SVS_MSG_HEARTBEAT;

#define REGISTER_CHECKSUM_LENGTH    16
typedef struct _SVS_MSG_SERVER_REGIST_REQ
{
    SVS_MSG_HEADER MsgHeader;
    uint32_t       ServerType;
    uint32_t       ResetFlags;
    char           Name[SERVER_NAME_LEN];
    char           BindServerID[SERVER_ID_LEN];
    uint32_t       ServiceNetType;
    uint32_t       LastRunTime;
    char           serviceIP[SVS_IP_LEN];
    uint32_t       RtspservicePort;
    uint32_t       HlsservicePort;
    uint32_t       RtmpservicePort;
    uint8_t        Md5CheckSum[REGISTER_CHECKSUM_LENGTH];
}SVS_MSG_SERVER_REGIST_REQ;


typedef struct _SVS_MSG_SERVER_REGISTER_RESP
{
    SVS_MSG_HEADER MsgHeader;
    uint32_t       ReportMediaTaskFlag;
    uint32_t       RespCode;
    char           ServerID[SERVER_ID_LEN];
}SVS_MSG_SERVER_REGISTER_RESP;


typedef struct _SVS_BUSINESS_REPORT_INFO
{
    uint8_t         DeviceID[DEVICE_ID_LEN];
    uint32_t        ServiceType;
    uint8_t         szUrl[RTSP_URL_LEN];
    TIME_SPAN       OperateTimeSpan;
    uint64_svs      FluxSize;
}SVS_SBUSINESS_REPORT_INFO;

typedef struct _SVS_MSG_MU_BUSINESS_REPORT_REQ
{
    SVS_MSG_HEADER               MsgHeader;
    uint32_t                     BusinessTotalNum;
    SVS_SBUSINESS_REPORT_INFO    BusinessInfo[1];
}SVS_MSG_MU_BUSINESS_REPORT_REQ;

typedef struct _ADDR_INFO
{
    uint32_t IP;
    uint32_t Port;
}ADDR_INFO;




typedef struct _SVS_MSG_STREAM_SESSION_SETUP_REQ
{
    SVS_MSG_HEADER  MsgHeader;
    uint32_t        LocalIndex;
    uint8_t         DeviceID[DEVICE_ID_LEN];
    SETUP_TYPE      CallType;
    PLAY_URL_TYPE   UrlType;
    PLAY_TYPE       PlayType;
    DEV_STREAM_TYPE eStreamtype;//0- 主码流，1- 子码流, 2- 第三码流
    uint32_t        UrlLen;
    uint8_t         szUrl[RTSP_URL_LEN];
    uint32_t        SdpLen;
    uint8_t         szSdp[SDP_MSG_LENS];
    uint32_t        MediaLinkMode;//FOR VMS and EHOME 连接模式：0- TCP方式，1- UDP方式，2- HRUDP方式（可靠传输）
    uint8_t         szMediaIP[SVS_IP_LEN];//FOR VMS and EHOME
    uint16_t        usMediaPort;//FOR VMS and EHOME
}SVS_MSG_STREAM_SESSION_SETUP_REQ;


typedef struct _SVS_MSG_STREAM_SESSION_SETUP_RESP
{
    SVS_MSG_HEADER            MsgHeader;
    uint32_t                  LocalIndex;
    SETUP_TYPE                CallType;
    uint32_t                  RespCode;
    uint8_t                   DeviceID[DEVICE_ID_LEN];
    PLAY_URL_TYPE             UrlType;
    PLAY_TYPE                 PlayType;
    uint32_t                  UrlLen;
    uint8_t                   szUrl[RTSP_URL_LEN];
    uint32_t                  SdpLen;
    uint8_t                   szSdp[SDP_MSG_LENS];
    int32_t                   SessionID;// for ehome
}SVS_MSG_STREAM_SESSION_SETUP_RESP;

typedef struct _SVS_MSG_STREAM_SESSION_PLAY_REQ
{
    SVS_MSG_HEADER MsgHeader;
    uint32_t       LocalIndex;
    uint8_t        DeviceID[DEVICE_ID_LEN];
    PLAY_URL_TYPE  UrlType;
    int32_t        SessionID;// for ehome
}SVS_MSG_STREAM_SESSION_PLAY_REQ;


typedef struct _SVS_MSG_STREAM_SESSION_PLAY_RESP
{
    SVS_MSG_HEADER            MsgHeader;
    uint32_t                  LocalIndex;
    uint32_t                  RespCode;
    uint8_t                   DeviceID[DEVICE_ID_LEN];
    PLAY_URL_TYPE             UrlType;
}SVS_MSG_STREAM_SESSION_PLAY_RESP;

typedef struct _SVS_MSG_STREAM_KEY_FRAME_REQ
{
    SVS_MSG_HEADER MsgHeader;
    uint8_t        DeviceID[DEVICE_ID_LEN];
}SVS_MSG_STREAM_KEY_FRAME_REQ;

typedef struct _SVS_MSG_STREAM_SESSION_TEARDOWN_REQ
{
    SVS_MSG_HEADER MsgHeader;
    uint32_t       LocalIndex;
    uint8_t        DeviceID[DEVICE_ID_LEN];
    int32_t        SessionID;// for ehome
}SVS_MSG_STREAM_SESSION_TEARDOWN_REQ;


typedef struct _SVS_MSG_STREAM_SESSION_TEARDOWN_RESP
{
    SVS_MSG_HEADER            MsgHeader;
    uint32_t                  LocalIndex;
    uint8_t                   DeviceID[DEVICE_ID_LEN];
    uint32_t                  RespCode;
}SVS_MSG_STREAM_SESSION_TEARDOWN_RESP;


typedef struct _NETWORK_CARD_INFO
{
    uint32_t IngressTotalBW;
    uint32_t IngressUsedBW;
    uint32_t EgressTotalBW;
    uint32_t EgressUsedBW;
}NETWORK_CARD_INFO;

typedef struct _SVS_MSG_STREAM_LOAD_INFO_REQ
{
    SVS_MSG_HEADER    MsgHeader;
    uint32_t          TransmitNumber;
    uint32_t          TotalMemorySize;
    uint32_t          UsedMemorySize;
    uint32_t          UsedCpuLoad;
    uint32_t          IowaitCpuLoad;
    uint32_t          CacheTotalNum;
    uint32_t          CacheUsedNum;
    uint32_t          NetworkCardNum;
    NETWORK_CARD_INFO NetworkCardInfo[1];
}SVS_MSG_STREAM_LOAD_INFO_REQ;


typedef struct _SVS_MSG_PLAYBACK_CONTROL_REQ
{
    SVS_MSG_HEADER   MsgHeader;
    uint64_svs       BusinessID;
    uint32_t         Sequence;
    uint32_t         OperCode;
    uint32_t         TimeType;
    uint32_t         StartTime;
    uint32_t         StopTime;
    uint32_t         PlayRate;
}SVS_MSG_PLAYBACK_CONTROL_REQ;


typedef struct _SVS_MSG_PLAYBACK_CONTROL_RESP
{
    SVS_MSG_HEADER    MsgHeader;
    uint64_svs        BusinessID;
    uint32_t          Sequence;
    uint32_t          RespCode;
    uint32_t          RtpSequence;
    uint32_t          RtpTimestamp;
}SVS_MSG_PLAYBACK_CONTROL_RESP;


typedef struct _SVS_MSG_FILE_END_NOTE_REQ
{
    SVS_MSG_HEADER   MsgHeader;
    char             DeviceID[DEVICE_ID_LEN];
    uint64_svs       BusinessID;
}SVS_MSG_FILE_END_NOTE_REQ;


#pragma pack(pop)

#endif

