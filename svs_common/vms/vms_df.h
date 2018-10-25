#ifndef _SVS_DF_H
#define _SVS_DF_H

//����Э��汾��
#ifndef SVS_PROTOCOL_VERSION
#define SVS_PROTOCOL_VERSION 0x0100
#endif

// Ŀǰֻ֧����������ǰ��
// ��������и����ǰ����Ҫ������Ҫ�޸�ǰ�˰汾����
enum enSvsDeviceVersion
{
    SVS_DEVICE_VERSION_OLD  = 0x0,       // �ϵ�ǰ��
    SVS_DEVICE_VERSION_NEW  = 0x00020000,      // �µ�ǰ��
};
//ʵʱ��Э���
#define SVS_PROTOCOL_REALTIME_STREAM_VERSION1 0x0101

//�ļ���Э���
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

#define SVS_ZONEINFO_LEN    32//�豸����������Ϣ
#define SVS_IP_LEN          64
#define SVS_PORT_LEN        5

#define SVS_STATUS_OK       1
#define SVS_STATUS_PAUSE    2
#define SVS_STATUS_DEL      0

#define SVS_NODEID_LEN       9
#define SVS_CMSINDEX_LEN    3

#define SVS_ID32_LEN              32                    //32λID����
#define SVS_ID32_STR_LEN          (SVS_ID32_LEN + 1)    //����32λID���ȵ��ַ����鳤��

#define DB_USERNAME 30

#define MOTION_BUF_SIZE 1584

#define DEVICE_NAME_LEN             128

#define SNAP_FILENAME_LEN 64 // �����ļ����Ƴ���
#define SNAPTIME_LEN  16     // ���ղ���ʱ��(YYYYMMDDHHMMSS)����

#define PTZPRESET_TIME_LEN 6

#define TOURNAME_LEN 64

#define SVS_RIGHT_LEN 64 //Ȩ���ֶγ���

#define MEDIA_URL_LEN  512  // URL����


// ¼����ٲ���ID����
#define SVS_RECORD_TRACE_POLICY_ID_LEN      20

/*
 * ����SVS��Ϣ���
 */
#define SVS_MSG_PF_MAX_SIZE    65535

/*
 *  *      ���嵱ǰ��Ϣ�汾
 *   */
#define SVS_MSG_VERSION         0x0100

/*
 *  *      ��Ϣͷ��
 *   */
#define SVS_HEADER_SIZE         8


#define CONTENT_ID_LEN              256

//////////////////////////////////////////////////////////
// Э�������ֶγ��ȶ���
//////////////////////////////////////////////////////////
enum SVS_MSG_LEN_CONSTANT
{
        STREAM_FILE_NAME_LEN  = 28, //���ļ�������
//      DEVICE_ID_LEN         = 20, //�豸ID����
//      USER_ID_LEN           = 20, //�û�ID����
//      TIME_STRING_LEN       = 14, //ʱ�䴮����,��20040628164030
        SVS_FILE_NAME_LEN    = 32, //���ݱ�����¼���ļ����ĳ���
        SVS_DEVICENUMBER_LEN =  8, //�豸��ų���
//      CENTER_SERVER_ID_LEN  = 15, //���ķ���ƽ̨������ID����
};


#ifndef SVS_ZONE_ORG_LEN
#define SVS_ZONE_CODE_LEN   3       //���ų���
#define SVS_ORG_CODE_LEN    5       //��֯�ų���
#define SVS_ZONE_ORG_LEN    ((SVS_ZONE_CODE_LEN) + (SVS_ORG_CODE_LEN))       //���š���֯�ų���
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

//2007��10��24������ ��ͨ������---
//SVSATTR�ṹ�е�ValueType:����ֵ����

#define  SVS_ATTR_VALUE_TYPE_LONG                0x10    //������
#define  SVS_ATTR_VALUE_TYPE_STRING_32        0x11    //32λ���ַ���
//SVSATTR�ṹ�е�AttrCode:��������

#define  SVS_ATTR_TYPE_PUID                                       0x3301 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_PUIP                                        0x3302 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_PU_CONTROLPORT                            0x3303 //ValueTypeΪLONG
#define  SVS_ATTR_TYPE_DOMAIN_USERNAME                        0x3304 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_DOMAIN_PASSWORD                          0x3305 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_DISK_FULL_ALARM_ENABLED                0x3306 //ValueTypeΪLONG
#define  SVS_ATTR_TYPE_DISK_FULL_HIGH_THRESHOLD                0x3307 //ValueTypeΪLONG
#define  SVS_ATTR_TYPE_DISK_FULL_LOW_THRESHOLD                0x3308 //ValueTypeΪLONG
#define  SVS_ATTR_TYPE_DISK_ERROR_ALARM_ENABLED                0x3309 //ValueTypeΪLONG
#define  SVS_ATTR_TYPE_ALARM_PRE_RECORD_TIME                    0x330A //ValueTypeΪLONG
#define  SVS_ATTR_TYPE_ALARM_RECORD_TIME                        0x330B //ValueTypeΪLONG
#define  SVS_ATTR_TYPE_PHOTO_RESOLUTION                        0x330C //ValueTypeΪLONG
#define  SVS_ATTR_TYPE_PHOTO_QUALITY                            0x330D //ValueTypeΪLONG
#define  SVS_ATTR_TYPE_SHOOTTIMES                                0x330E //ValueTypeΪLONG
#define  SVS_ATTR_TYPE_GPIN_ALARM_ENABLED                    0x330F //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_ALARM_OUTPUT_ACTIVATED_TIME            0x3310 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_GPIN_ALARM_STATUS                        0x3311 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_GPIN_ALARM_OUTPUT                        0x3312 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_GPIN_ALARM_RECORD                        0x3313 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_GPIN_ALARM_SHOOT                        0x3314 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_VIDEO_LOSE_ALARM_ENABLED                0x3315 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_VIDEO_LOSE_ALARM_OUTPUT                0x3316 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_ALARM_ENABLED        0x3317 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_ALARM_TIME             0x3318 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_X                        0x3319 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_Y                        0x331A //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_WIDTH                    0x331B //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_HEIGHT                    0x331C //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_ALARM_OUTPUT            0x331D //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_HIDE_DETECTION_ALARM_RECORD            0x331E //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_ALARM_ENABLED        0x331F //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_ALARM_TIME            0x3320 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_BLOCKS                0x3321 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_FREQUENCY            0x3322 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_SENSITIVITY            0x3323 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_ALARM_OUTPUT        0x3324 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_ALARM_RECORD        0x3325 //ValueTypeΪSTRING32
#define  SVS_ATTR_TYPE_MOTION_DETECTION_ALARM_SHOOT        0x3326 //ValueTypeΪSTRING32

//ǰ��Ԥ¼ʱ�䷽ʽ����
enum SVS_ENUM_FRONT_PRERECORD_TIME_TYPE
{
    FRONT_PRERECORD_TIME_NONE        = 0,    //��Ԥ¼;
    FRONT_PRERECORD_TIME_FIVE        = 1,    //5����;
    FRONT_PRERECORD_TIME_TEN        = 2,    //10����;
    FRONT_PRERECORD_TIME_FIFTEEN    = 3,    //15����;
    FRONT_PRERECORD_TIME_TWENTY        = 4,    //20����;
    FRONT_PRERECORD_TIME_TWENTYFIVE    = 5,    //25����;
    FRONT_PRERECORD_TIME_THIRTY        = 6,    //30����;
    FRONT_PRERECORD_TIME_MAX        = 7,    //������Ԥ¼;
};

// �û����ͳ��ȶ��� ��ݸ����/�ֻ���Ƶ���
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
    //�ͻ��˴�����:0x00000001 ~ 0x000000FF,ֻռ�õ�1���ֽ�
    ROUTE_STOP_REASON_CLIENT_MIN         = 0x00000001,
    ROUTE_STOP_REASON_CLIENT_MAX         = 0x000000FF,
    //NRU������:0x00000100 ~ 0x0000FF00,ֻռ�õ�2���ֽ�
    ROUTE_STOP_REASON_MRU_MIN            = 0x00000100,
    ROUTE_STOP_REASON_MRU_STREAM_ERROR   = 0x00000200,//MRU��鵽ý���������쳣
    ROUTE_STOP_REASON_MRU_RECVTIMEOUT    = 0x00000300,//MRU��������ʱ
    ROUTE_STOP_REASON_MRU_EXCEPTION      = 0x00000400,//MRU�����쳣
    ROUTE_STOP_REASON_MRU_CONNECT_ERROR  = 0x00000500,//MRU����MDUʧ��
    ROUTE_STOP_REASON_MRU_MAX            = 0x0000FF00,
    //MDU������:0x00010000 ~ 0x00FF0000,ֻռ�õ�3���ֽ�
    ROUTE_STOP_REASON_MDU_MIN            = 0x00010000,
    ROUTE_STOP_REASON_MDU_CLIENT_DISCON  = 0x00020000,//�ͻ��˻���MRU�Ͽ�����
    ROUTE_STOP_REASON_MDU_STREAM_ERROR   = 0x00030000,//ǰ��ý���������쳣
    ROUTE_STOP_REASON_MDU_DEV_DISCON     = 0x00040000,//ǰ�˶Ͽ�����
    ROUTE_STOP_REASON_MDU_SCHEDULE_ERROR = 0x00050000,//������������
    ROUTE_STOP_REASON_MDU_RECVTIMEOUT    = 0x00060000,//����ǰ�����ݳ�ʱ
    ROUTE_STOP_REASON_MDU_EXCEPTION       = 0X00070000,//mdu�����쳣
    ROUTE_STOP_REASON_MDU_MAX            = 0x00FF0000,
    //SCC������0x01000000 ~ 0xFF000000,ֻռ�õ�4���ֽ�
    ROUTE_STOP_REASON_SCC_MIN            = 0X01000000,
    ROUTE_STOP_REASON_SCC_DEV_OFFLINE    = 0x02000000,//�豸����
    ROUTE_STOP_REASON_SCC_FLUX_ERROR     = 0x03000000,//ý����,�����쳣
    ROUTE_STOP_REASON_SCC_MDU_ERROR      = 0x04000000,//MDU�쳣
    ROUTE_STOP_REASON_SCC_DEV_STOP       = 0x05000000,//ǰ�˽���·��
    ROUTE_STOP_REASON_SCC_PAG_ERROR      = 0x06000000,//PAG�쳣
    ROUTE_STOP_REASON_SCC_STORE_ERROR    = 0x07000000,//Store�쳣
    ROUTE_STOP_REASON_SCC_MRU_ERROR      = 0x08000000,//MRU�쳣
    ROUTE_STOP_REASON_SCC_MAX            = 0xFF000000,
    //���,����ʹ�øô�����
    ROUTE_STOP_REASON_ERROR              = 0xFFFFFFFF,
};

#endif //_SVS_DF_H

