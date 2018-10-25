#ifndef _SVS_DF_H
#define _SVS_DF_H

//信令协议版本号
#ifndef SVS_PROTOCOL_VERSION
#define SVS_PROTOCOL_VERSION 0x0100
#endif

// 目前只支持新老两种前端
// 后续如果有更多的前端需要兼容需要修改前端版本定义
enum enSvsDeviceVersion
{
    SVS_DEVICE_VERSION_OLD  = 0x0,       // 老的前端
    SVS_DEVICE_VERSION_NEW  = 0x00020000,      // 新的前端
};
//实时流协议号
#define SVS_PROTOCOL_REALTIME_STREAM_VERSION1 0x0101

//文件流协议号
#define SVS_PROTOCOL_FILE_STREAM_VERSION1  0x0100

#define INVALIDVALUE                    -1

#define DESCRIPTION_LEN         256

#define TIME_STRING_LEN         15

#define USERID_LEN                              20
#define ROLEID_LEN                20
#define SESSIONID_LEN                       20
#define DEVUSERID_LEN                           20
#define DEVICEID_LEN                         20

#define OPERATORID_LEN              20

#define DOMAINID_LEN                20
#define GROUPID_LEN                               20
#define SVSID_LEN                20
#define LENSDEVID_LEN                  20
#define SVS_NAME_LEN                       30
#define SVS_DEVICEID_LEN            20

#define SVS_ZONEINFO_LEN    32//设备所属区域信息
#define SVS_IP_LEN          64
#define SVS_PORT_LEN        5

#define SVS_STATUS_OK       1
#define SVS_STATUS_PAUSE    2
#define SVS_STATUS_DEL      0

#define SVS_NODEID_LEN       9
#define SVS_CMSINDEX_LEN    3

#define SVS_ID32_LEN              32                    //32位ID长度
#define SVS_ID32_STR_LEN          (SVS_ID32_LEN + 1)    //保存32位ID长度的字符数组长度

#define DB_USERNAME 30

#define MOTION_BUF_SIZE 1584

#define DEVICE_NAME_LEN             128

#define SNAP_FILENAME_LEN 64 // 快照文件名称长度
#define SNAPTIME_LEN  16     // 快照产生时间(YYYYMMDDHHMMSS)长度

#define PTZPRESET_TIME_LEN 6

#define TOURNAME_LEN 64

#define SVS_RIGHT_LEN 64 //权限字段长度

#define MEDIA_URL_LEN  512  // URL长度


// 录像跟踪策略ID长度
#define SVS_RECORD_TRACE_POLICY_ID_LEN      20

/*
 * 单个SVS消息最大长
 */
#define SVS_MSG_PF_MAX_SIZE    65535

/*
 *  *      定义当前消息版本
 *   */
#define SVS_MSG_VERSION         0x0100

/*
 *  *      消息头长
 *   */
#define SVS_HEADER_SIZE         8


#define CONTENT_ID_LEN              256

//////////////////////////////////////////////////////////
// 协议数据字段长度定义
//////////////////////////////////////////////////////////
enum SVS_MSG_LEN_CONSTANT
{
        STREAM_FILE_NAME_LEN  = 28, //流文件名长度
//      DEVICE_ID_LEN         = 20, //设备ID长度
//      USER_ID_LEN           = 20, //用户ID长度
//      TIME_STRING_LEN       = 14, //时间串长度,如20040628164030
        SVS_FILE_NAME_LEN    = 32, //数据报文中录象文件名的长度
        SVS_DEVICENUMBER_LEN =  8, //设备序号长度
//      CENTER_SERVER_ID_LEN  = 15, //中心服务平台服务器ID长度
};


#ifndef SVS_ZONE_ORG_LEN
#define SVS_ZONE_CODE_LEN   3       //区号长度
#define SVS_ORG_CODE_LEN    5       //组织号长度
#define SVS_ZONE_ORG_LEN    ((SVS_ZONE_CODE_LEN) + (SVS_ORG_CODE_LEN))       //区号、组织号长度
#endif

/************************************************************************/
/*                            GEIP Error Code                           */
/************************************************************************/
#define SVS_ERROR_CODE_SUCCESS                     0x00    // Success
#define SVS_ERROR_CODE_FAIL                        0x01    // Failure
#define SVS_ERROR_CODE_UNKNOWN                     0xFF    // Unknown


/************************************************************************/
/*                     Transport Protocol Code                          */
/************************************************************************/

#define SVS_TRANSPORT_PROTOCOL_NONE                                     0x00
#define SVS_TRANSPORT_PROTOCOL_RTP                                      0x01
#define SVS_TRANSPORT_PROTOCOL_UDP                                      0x02
#define SVS_TRANSPORT_PROTOCOL_TCP                                      0x03
#define SVS_TRANSPORT_PROTOCOL_MULTICAST                                0x04

//2007年10月24日新增 网通测试用---
//SVSATTR结构中的ValueType:属性值类型

#define  SVS_ATTR_VALUE_TYPE_LONG                0x10    //长整型
#define  SVS_ATTR_VALUE_TYPE_STRING_32        0x11    //32位的字符串
//SVSATTR结构中的AttrCode:属性类型

#define  SVS_ATTR_TYPE_PUID                                       0x3301 //ValueType为STRING32
#define  SVS_ATTR_TYPE_PUIP                                        0x3302 //ValueType为STRING32
#define  SVS_ATTR_TYPE_PU_CONTROLPORT                            0x3303 //ValueType为LONG
#define  SVS_ATTR_TYPE_DOMAIN_USERNAME                        0x3304 //ValueType为STRING32
#define  SVS_ATTR_TYPE_DOMAIN_PASSWORD                          0x3305 //ValueType为STRING32
#define  SVS_ATTR_TYPE_DISK_FULL_ALARM_ENABLED                0x3306 //ValueType为LONG
#define  SVS_ATTR_TYPE_DISK_FULL_HIGH_THRESHOLD                0x3307 //ValueType为LONG
#define  SVS_ATTR_TYPE_DISK_FULL_LOW_THRESHOLD                0x3308 //ValueType为LONG
#define  SVS_ATTR_TYPE_DISK_ERROR_ALARM_ENABLED                0x3309 //ValueType为LONG
#define  SVS_ATTR_TYPE_ALARM_PRE_RECORD_TIME                    0x330A //ValueType为LONG
#define  SVS_ATTR_TYPE_ALARM_RECORD_TIME                        0x330B //ValueType为LONG
#define  SVS_ATTR_TYPE_PHOTO_RESOLUTION                        0x330C //ValueType为LONG
#define  SVS_ATTR_TYPE_PHOTO_QUALITY                            0x330D //ValueType为LONG
#define  SVS_ATTR_TYPE_SHOOTTIMES                                0x330E //ValueType为LONG
#define  SVS_ATTR_TYPE_GPIN_ALARM_ENABLED                    0x330F //ValueType为STRING32
#define  SVS_ATTR_TYPE_ALARM_OUTPUT_ACTIVATED_TIME            0x3310 //ValueType为STRING32
#define  SVS_ATTR_TYPE_GPIN_ALARM_STATUS                        0x3311 //ValueType为STRING32
#define  SVS_ATTR_TYPE_GPIN_ALARM_OUTPUT                        0x3312 //ValueType为STRING32
#define  SVS_ATTR_TYPE_GPIN_ALARM_RECORD                        0x3313 //ValueType为STRING32
#define  SVS_ATTR_TYPE_GPIN_ALARM_SHOOT                        0x3314 //ValueType为STRING32
#define  SVS_ATTR_TYPE_VIDEO_LOSE_ALARM_ENABLED                0x3315 //ValueType为STRING32
#define  SVS_ATTR_TYPE_VIDEO_LOSE_ALARM_OUTPUT                0x3316 //ValueType为STRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_ALARM_ENABLED        0x3317 //ValueType为STRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_ALARM_TIME             0x3318 //ValueType为STRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_X                        0x3319 //ValueType为STRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_Y                        0x331A //ValueType为STRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_WIDTH                    0x331B //ValueType为STRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_HEIGHT                    0x331C //ValueType为STRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_ALARM_OUTPUT            0x331D //ValueType为STRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_ALARM_RECORD            0x331E //ValueType为STRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_ALARM_ENABLED        0x331F //ValueType为STRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_ALARM_TIME            0x3320 //ValueType为STRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_BLOCKS                0x3321 //ValueType为STRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_FREQUENCY            0x3322 //ValueType为STRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_SENSITIVITY            0x3323 //ValueType为STRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_ALARM_OUTPUT        0x3324 //ValueType为STRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_ALARM_RECORD        0x3325 //ValueType为STRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_ALARM_SHOOT        0x3326 //ValueType为STRING32

//前端预录时间方式定义
enum SVS_ENUM_FRONT_PRERECORD_TIME_TYPE
{
    FRONT_PRERECORD_TIME_NONE        = 0,    //不预录;
    FRONT_PRERECORD_TIME_FIVE        = 1,    //5秒钟;
    FRONT_PRERECORD_TIME_TEN        = 2,    //10秒钟;
    FRONT_PRERECORD_TIME_FIFTEEN    = 3,    //15秒钟;
    FRONT_PRERECORD_TIME_TWENTY        = 4,    //20秒钟;
    FRONT_PRERECORD_TIME_TWENTYFIVE    = 5,    //25秒钟;
    FRONT_PRERECORD_TIME_THIRTY        = 6,    //30秒钟;
    FRONT_PRERECORD_TIME_MAX        = 7,    //尽可能预录;
};

// 用户类型长度定义 东莞需求/手机视频监控
#define USER_TYPE_LEN                    3

#define CMSID_LEN_MAX   32


#define PU_VESION_LEN                  16
#define UPGRADE_TASKID_LEN             20
#define UPGRADE_VERSIONID_LEN          20
#define UPGRADE_DATE_LEN                14
#define UPGRADE_STATUS_LEN              3
#define UPGRADE_VENDORID_LEN           10
#define UPGRADE_VERSION_LEN            64
#define UPGRADE_MD5CHECKCODE_LEN       16
#define UPGRADE_PLATFORMTYPE_LEN       64
#define UPGRADE_MODE_LEN                64
#define CHALLENGECODE_LEN               64

enum ROUTE_STOP_REASON
{
    ROUTE_STOP_REASON_OK                 = 0x00000000,
    //客户端错误码:0x00000001 ~ 0x000000FF,只占用低1个字节
    ROUTE_STOP_REASON_CLIENT_MIN         = 0x00000001,
    ROUTE_STOP_REASON_CLIENT_MAX         = 0x000000FF,
    //NRU错误码:0x00000100 ~ 0x0000FF00,只占用第2个字节
    ROUTE_STOP_REASON_MRU_MIN            = 0x00000100,
    ROUTE_STOP_REASON_MRU_STREAM_ERROR   = 0x00000200,//MRU检查到媒体流数据异常
    ROUTE_STOP_REASON_MRU_RECVTIMEOUT    = 0x00000300,//MRU接收流超时
    ROUTE_STOP_REASON_MRU_EXCEPTION      = 0x00000400,//MRU自身异常
    ROUTE_STOP_REASON_MRU_CONNECT_ERROR  = 0x00000500,//MRU连接MDU失败
    ROUTE_STOP_REASON_MRU_MAX            = 0x0000FF00,
    //MDU错误码:0x00010000 ~ 0x00FF0000,只占用第3个字节
    ROUTE_STOP_REASON_MDU_MIN            = 0x00010000,
    ROUTE_STOP_REASON_MDU_CLIENT_DISCON  = 0x00020000,//客户端或者MRU断开连接
    ROUTE_STOP_REASON_MDU_STREAM_ERROR   = 0x00030000,//前端媒体流数据异常
    ROUTE_STOP_REASON_MDU_DEV_DISCON     = 0x00040000,//前端断开连接
    ROUTE_STOP_REASON_MDU_SCHEDULE_ERROR = 0x00050000,//调度重新启动
    ROUTE_STOP_REASON_MDU_RECVTIMEOUT    = 0x00060000,//接收前端数据超时
    ROUTE_STOP_REASON_MDU_EXCEPTION       = 0X00070000,//mdu自身异常
    ROUTE_STOP_REASON_MDU_MAX            = 0x00FF0000,
    //SCC错误码0x01000000 ~ 0xFF000000,只占用第4个字节
    ROUTE_STOP_REASON_SCC_MIN            = 0X01000000,
    ROUTE_STOP_REASON_SCC_DEV_OFFLINE    = 0x02000000,//设备下线
    ROUTE_STOP_REASON_SCC_FLUX_ERROR     = 0x03000000,//媒体流,流量异常
    ROUTE_STOP_REASON_SCC_MDU_ERROR      = 0x04000000,//MDU异常
    ROUTE_STOP_REASON_SCC_DEV_STOP       = 0x05000000,//前端结束路由
    ROUTE_STOP_REASON_SCC_PAG_ERROR      = 0x06000000,//PAG异常
    ROUTE_STOP_REASON_SCC_STORE_ERROR    = 0x07000000,//Store异常
    ROUTE_STOP_REASON_SCC_MRU_ERROR      = 0x08000000,//MRU异常
    ROUTE_STOP_REASON_SCC_MAX            = 0xFF000000,
    //最大,不能使用该错误码
    ROUTE_STOP_REASON_ERROR              = 0xFFFFFFFF,
};

#endif //_SVS_DF_H

