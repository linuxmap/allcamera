#ifndef _SVS_CM_H
#define _SVS_CM_H

#include <time.h>

#include <vms/vms_adt.h>
#include <vms/vms_df.h>

#pragma pack(push, 1)

typedef struct
_SVS_HEADER
{
        uint16_t          ProtocolVersion;
        uint32_t          MsgType;
        uint16_t          TransactionNo;
        uint32_t          PacketLength;
}
SVS_HEADER,*PSVS_HEADER, SVS_MSG_HEADER, *PSVS_MSG_HEADER;

#define SVS_MSG_HEADER_SIZE           sizeof(SVS_HEADER)

typedef struct
_SVS_MSG
{
        SVS_HEADER        Header;
}
SVS_MSG,*PSVS_MSG;

typedef struct
_SVS_FRONTDEV_ID
{
        char             ID[DEVICEID_LEN];
}
SVS_FRONTDEV_ID,*PSVS_FRONTDEV_ID;

typedef struct
_SVS_CONFIG
{
        uint16_t         ConfigID;
        uint8_t          ValueType;
        void *           Value;
}
SVS_CONFIG, *PSVS_CONFIG;

typedef struct
_SVS_ERROR_MSG
{
        SVS_HEADER        Header;
        uint8_t           Id[DEVICEID_LEN];
        uint16_t          RequestMsgType;
        uint32_t          ErrorCode;
}
SVS_ERROR_MSG,*PSVS_ERROR_MSG;


typedef struct _SVS_ERROR_RESP_MSG
{
        SVS_HEADER        Header;
        uint8_t           SessionID[SESSIONID_LEN];
        uint16_t          RequestType;
        int32_t           ErrorCode;
}
SVS_ERROR_RESP_MSG;

#define SVS_ERR_RESP_MSG_LEN            sizeof(SVS_ERROR_MSG)

typedef struct
_TIMESPAN
{
        uint8_t           StartTime[TIME_STRING_LEN];
        uint8_t           EndTime[TIME_STRING_LEN];
}
TIMESPAN,*PTIMESPAN;


typedef struct _SVSATTR
{
        uint16_t          AttrCode;
        char              ValueType;
        void*             Value;
}
SVSATTR,*PSVSATTR,SVS_MSG_ATTR;

typedef struct
_SVS_HEADER_SESSIONID
{
        SVS_HEADER        Header;
        uint8_t           SessionID[SESSIONID_LEN];
}
SVS_HEADER_SESSIONID,*PSVS_HEADER_SESSIONID;

typedef struct
_SVS_HEADER_SEGMENT
{
        SVS_HEADER        Header;
        uint8_t           SessionID[SESSIONID_LEN];
        uint8_t           UserID[USERID_LEN];
}
SVS_HEADER_SEGMENT,*PSVS_HEADER_SEGMENT;


struct SVS_TIME_T_SPAN
{
        time_t         StartTime;
        time_t         EndTime;
        uint8_t        FilePath[32];
};


typedef struct _SVS_FRONTDEV_ID_STRUCT
{
        char           Zone[SVS_ZONE_CODE_LEN];
        char           OrgCode[SVS_ORG_CODE_LEN];
        char           CodeType;
        char           FrontDevType;
        char           FrontSubDevType[2];
        char           FrontDevNum[8];
}
SVS_FRONTDEV_ID_STRUCT, *PSVS_FRONTDEV_ID_STRUCT;


typedef struct _SVS_CENTER_SERVER_ID
{
        char         Zone[3];
        char         OrgCode[5];
        char         CodeType;
        char         DeviceType[2];
        char         DeviceNo[3];
        char         Reserve[6];
}
SVS_CENTER_SERVER_ID, *PSVS_CENTER_SERVER_ID;


typedef struct _SVS_SESSION_ID
{
        char         ID[20];
}
SVS_SESSION_ID, *PSVS_SESSION_ID;

typedef struct
_GE_MSG
{
        uint16_t      MSGID;
        uint8_t       ValueType;
        void*         Value ;

}GE_MSG,*PGE_MSG;

typedef struct
_SVS_CLIENT_MSG_MSG
{
        SVS_HEADER    Header;
        uint8_t       DeviceID[DEVICEID_LEN];
        GE_MSG        Message;
}
SVS_CLIENT_MSG_MSG,*PSVS_CLIENT_MSG_MSG;

typedef struct _CRUISE_POINT_
{
        uint8_t      presetNum;
        uint8_t      speed;
        uint8_t      dwellTime;
        uint8_t      res;
}CRUISE_POINT, *PCRUISE_POINT;


typedef struct _PTZ_CRUISE_TRACK_
{
    char            cruiseName[32];
    char            trackNum;
    char            enabled;
    uint16_t        pointCount;
    CRUISE_POINT    point[16];
}PTZ_CRUISE_TRACK, *PPTZ_CRUISE_TRACK;


enum SVS_LOG_TYPE
{
    LOG_TYPE_ALARM            = 1,
    LOG_TYPE_EXCEPTION        = 2,
    LOG_TYPE_OPT              = 3,
    LOG_TYPE_ALL              = 0xFF,
};
typedef struct _FRONT_LOG_INFO
{
    char        LogType;
    char        StartTime[TIME_STRING_LEN];
    char        NetUser[DB_USERNAME];
    uint16_t    VideoID;
    uint16_t    DiskNumber;
    uint16_t    AlarmInPort;
    uint16_t    AlarmOutPort;
}FRONT_LOG_INFO,*PFRONT_LOG_INFO;


typedef struct _DEVICE_ID_LIST
{
    uint32_t           RecordTime; 
    SVS_FRONTDEV_ID    LensID;
} DEVICE_ID_LIST,*PDEVICE_ID_LIST;

typedef struct _FRONT_ALARM_POLICY
 {
    uint8_t            AlarmCode;
    uint8_t            RecordEvent;
    uint32_t           LensIDCount;
    DEVICE_ID_LIST     LensIDList[1];
}FRONT_ALARM_POLICY,*PFRONT_ALARM_POLICY;

typedef struct _SVS_TLV_ATTR
{
    uint16_t unType;
    uint16_t unLen;
    char     szValue[1];
}SVS_TLV_ATTR, *PSVS_TLV_ATTR;

typedef struct _SVS_VMS_MEDIA_ADDR
{
    uint32_t      ServerIp;
    uint16_t      ServerPort;
}SVS_VMS_MEDIA_ADDR,*PSVS_VMS_MEDIA_ADDR;

typedef struct _SVS_RTP_MEDIA_ADDR
{
    uint32_t      VideoIp;
    uint16_t      VideoRtpPort;
    uint16_t      VideoRtcpPort;
    uint32_t      AudioIP;
    uint16_t      AudioRtpPort;
    uint16_t      AudioRtcpPort;
}SVS_RTP_MEDIA_ADDR,*PSVS_RTP_MEDIA_ADDR;

typedef union _SVS_MEDIA_ADDR
{
    SVS_VMS_MEDIA_ADDR  VmsAddr;  //VMS协议地址
    SVS_RTP_MEDIA_ADDR  RtpAddr;  //RTP协议地址
}SVS_MEDIA_ADDR;

#pragma pack(pop)

#endif //_SVS_CM_H

