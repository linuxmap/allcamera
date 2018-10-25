#ifndef __ScheduleCommon_h__
#define __ScheduleCommon_h__

#include <string>
#include "ace/Message_Block.h"
#include "ace/OS.h"
#include "vms/vms.h"
#include "ace/Singleton.h"
#include "svs_log_msg.h"

#define SCHEDULE_RET_OK 0



const char* const CONF_FILE = "../conf/svs_access_control.conf";
const char* const LOG_FILE  = "../log/svs_access_control.log";


const int32_t FRONT_RUN  = 0;
const int32_t DAEMON_RUN = 1;

const uint32_t SVS_USAGE_NET_RATE_ZERO = 0;
const uint32_t SVS_USAGE_PORT_RATE     = 40;
const uint32_t SVS_USAGE_NET_CARD_RATE = 60;
const uint32_t SVS_USAGE_CPU_RATE      = 70;
const uint32_t SVS_USAGE_LOAD_RATE     = 100;

const int32_t PRINT_BUFFER_SIZE     = 1024;

const int32_t SVS_THREAD_STATCK_MAX  = 2*1024 * 1024;
const int32_t SVS_MSG_QUEUE_WATER      = 64 * 1024 * 1024;

const int32_t SVS_ACM_THREAD_MAX = 3;
const int32_t SVS_GB28181_RECV_THREAD_MAX = 3;
const int32_t SVS_GB28181_SEND_THREAD_MAX = 3;
const int32_t SVS_GB28181_WORK_THREAD_MAX = 10;

//配置相关信息
#define READ_CONFIG_STRING_BUF_LEN 512

#define SVS_SERVER_NAME_LEN                         128
#define SVS_SERVER_ID_MAX_LEN                      32

const int32_t SVS_DEFALT_HTTP_STACK_LISTEN_PORT    = 2003;
const int32_t SVS_DEFALT_CONTROL_SERVER_PORT       = 80;

const int32_t SVS_DEFALT_LISTEN_PORT                = 8020;
const int32_t SVS_DEFALT_MU_LISTEN_PORT             = 8380;
const int32_t SVS_DEFALT_CONNECT_PORT               = 6640;
const int32_t SVS_DEFALT_REMOTE_DEBUG_PORT          = 2010;
const int32_t SVS_DEFALT_GB28181_STACK_LISTEN_PORT  = 5060;
const int32_t SVS_DEFALT_EHOME_STACK_LISTEN_PORT    = 7660;


// info about device
const int32_t SVS_DEV_ID_LEN = 32;

#define  MEDIA_URL_LEN              512

const int32_t SVS_MAX_EPOLL_SIZE           = 2048;

const int32_t HTTP_STATUS_CHECK_INTERVAL   = 5;
const int32_t HTTP_CONNECT_MAX_COUNT       = 100;
const int32_t HTTP_MAX_TIME_PER_THREAD      = 10;



#define _SIMPLE_QUERY_FRONTSERVER_ID
#define SINGLE_THREAD_RECCONECT
#define NOT_RECV_VTDU_POINTOR

#define NO_REOPEN



/*
enum EN_BUSINESS_SESSION_STATUS
{
    EN_BUSINESS_SESSION_STATUS_INIT        = 0,
    EN_BUSINESS_SESSION_STATUS_WAITTING_NOTIFY_DEV_RESP,
    EN_BUSINESS_SESSION_STATUS_RUNNING
};
*/


enum EN_DEV_STREAM_STATUS
{
    EN_DEV_STREAM_STATUS_INIT        = 0,
    EN_DEV_STREAM_STATUS_NOTIFYING_DEV,
    EN_DEV_STREAM_STATUS_NOTIFYING_STREAM,
    EN_DEV_STREAM_STATUS_RUNNING
};



enum EN_RTSP_URL_TYPE
{
    EN_RTSP_URL_TYPE_DEVICE         = 0,
    EN_RTSP_URL_TYPE_CLIENT,
    EN_RTSP_URL_TYPE_SHARE,
};

enum TIMER_EVENT_ID
{
    TIMER_EVENT_ID_ROUTE_MANAGER       = 1,
    TIMER_EVENT_ID_BUSINESS_MANAGER    = 0x1001, // Business Manager , deal timeout business timer.

    //Timer for device stack
    TIMER_EVENT_ID_DEVICE_STACK       = 0x2001,
    TIMER_EVENT_ID_DEVICE_REPORT      = 0x2002,
    TIMER_EVENT_ID_MAX
};

const int32_t MESSAGE_INTERVAL = 5;   // business manager timer internal.
const int32_t TIMER_INTERVAL_DEVICE_STACK   = 5;

const time_t DEVICE_STACK_QUERY_CATALOG_INTERVAL = 60;  //查询设备目录间隔

const uint32_t DEVICE_REPORT_INTERVAL = 300;//设备信息定时上报间隔
const uint32_t DEVICE_KEEP_ALIVE_TIMEOUT = 60;

enum EN_SESSION_STATUS
{
    EN_SESSION_STATUS_INIT               = 0,
    EN_SESSION_STATUS_ESTABLISHING,
    EN_SESSION_STATUS_ESTABLISHED,
};



#define SCHEDULE_IP_STRING_LEN      128
#define SCHEDULE_RTSP_PORT          554
#define SCHEDULE_HLS_PORT           80

#define TLV_MIN_LENGTH            (sizeof(uint32_t) + sizeof(uint32_t))



const int32_t FIRST_TRANS_NO = 1;
const int32_t MAX_TRANS_NO   = 0x7FFF;    // max transaction number

// internal message define

enum SVS_MSG_TYPE_AC_INTER_MSG_LIST
{
    SVS_MSG_TYPE_AC_INTER_MSG_REPORT_REQ                     = 0x00001001,
    SVS_MSG_TYPE_AC_INTER_MSG_REPORT_RESP                   = 0x00001002,
    SVS_MSG_TYPE_AC_INTER_MSG_INVITE_REQ                      = 0x00001003,
    SVS_MSG_TYPE_AC_INTER_MSG_INVITE_RESP                    = 0x00001004,
    SVS_MSG_TYPE_AC_INTER_MSG_BYE_REQ                           = 0x00001005,
    SVS_MSG_TYPE_AC_INTER_MSG_BYE_RESP                         = 0x00001006,
    SVS_MSG_TYPE_AC_INTER_MSG_KEYFRAME_RESP               = 0x00001007,
};

enum HTTP_REPORT_TYPE
{
    HTTP_REPORT_TYPE_SETUP              = 0, // report when session setup
    HTTP_REPORT_TYPE_TEARDOWN      = 1, // report when session teardown
    HTTP_REPORT_TYPE_MAX
};


// load info
const uint32_t NO_LOAD_HEALTH           = 100;    // light load percent.
const uint32_t LIGHT_LOAD_PERCENT    = 75;    // light load percent.
const uint32_t HEAVY_LOAD_PERCENT   = 90;    // light load percent.

const double CPU_PERCENT                  = 0.25;
const double MEM_PERCENT                 = 0.25;
const double IO_PERCENT                    = 0.25;
const double CACHE_PERCENT             = 0.25;


enum SVR_LOAD_TYPE
{
    SVR_LOAD_TYPE_LIGHT                     = 0, // light load
    SVR_LOAD_TYPE_MODERATE,                   // moderate load
    SVR_LOAD_TYPE_HEAVY,                          // heavy load
    SVR_LOAD_TYPE_MAX
};

#pragma pack(push, 1)

typedef struct _AC_INTER_MSG_HTTP_REPORT_REQ
{
    SVS_RECV_MSG_HDR       hdrInner;
    SVS_MSG_HEADER         MsgHeader;
    uint8_t                DeviceID[DEVICE_ID_LEN];
    HTTP_REPORT_TYPE       HttpReportType;
    PLAY_URL_TYPE          UrlType;
    PLAY_TYPE              PlayType;
    uint32_t               UrlLen;
    uint8_t                szUrl[RTSP_URL_LEN];
    uint32_t               SdpLen;
    uint8_t                szSdp[SDP_MSG_LENS];
}AC_INTER_MSG_HTTP_REPORT_REQ;


typedef struct _AC_INTER_MSG_HTTP_REPORT_RESP
{
    SVS_RECV_MSG_HDR        hdrInner;
    SVS_MSG_HEADER          MsgHeader;
    uint8_t                 DeviceID[DEVICE_ID_LEN];
    HTTP_REPORT_TYPE        HttpReportType;
    PLAY_URL_TYPE           UrlType;
    PLAY_TYPE               PlayType;
    uint32_t                RespCode;
}AC_INTER_MSG_HTTP_REPORT_RESP;



typedef struct _AC_INTER_MSG_DEV_INVITE_REQ
{
    SVS_RECV_MSG_HDR        hdrInner;
    SVS_MSG_HEADER          MsgHeader;
    uint8_t                 DeviceID[DEVICE_ID_LEN];
    PLAY_URL_TYPE           UrlType;
    PLAY_TYPE               PlayType;
    uint32_t                UrlLen;
    uint8_t                 szUrl[RTSP_URL_LEN];
    uint32_t                SdpLen;
    uint8_t                 szSdp[SDP_MSG_LENS];
}AC_INTER_MSG_DEV_INVITE_REQ;


typedef struct _AC_INTER_MSG_DEV_INVITE_RESP
{
    SVS_RECV_MSG_HDR        hdrInner;
    SVS_MSG_HEADER          MsgHeader;
    uint8_t                 DeviceID[DEVICE_ID_LEN];
    PLAY_URL_TYPE           UrlType;
    PLAY_TYPE               PlayType;
    uint32_t                SdpLen;
    uint8_t                 szSdp[SDP_MSG_LENS];
    int32_t                 SessionID;//for ehome
    uint32_t                RespCode;
}AC_INTER_MSG_DEV_INVITE_RESP;


typedef struct _AC_INTER_MSG_DEV_BYE_RESP
{
    SVS_RECV_MSG_HDR        hdrInner;
    SVS_MSG_HEADER          MsgHeader;
    uint8_t                 DeviceID[DEVICE_ID_LEN];
    PLAY_URL_TYPE           UrlType;
    PLAY_TYPE               PlayType;
    uint32_t                RespCode;
}AC_INTER_MSG_DEV_BYE_RESP;

typedef struct _KEY_FRAME_USER_DATA
{
    uint32_t                uiServerIndex;
    uint8_t                 DeviceID[DEVICE_ID_LEN];
    SVS_MSG_HEADER          stRequestHeader;  // 响应中带着
}KEY_FRAME_USER_DATA;

typedef struct _AC_INTER_MSG_MDU_KEY_FRAME_RESP
{
    SVS_RECV_MSG_HDR        hdrInner;
    SVS_MSG_HEADER          MsgHeader;
    SVS_MSG_HEADER          ReqMsgHeader;
    uint8_t                 DeviceID[DEVICE_ID_LEN];
    uint32_t                ServerIndex;
    uint32_t                RespCode;
}AC_INTER_MSG_MDU_KEY_FRAME_RESP;


#pragma pack(pop)

#endif

