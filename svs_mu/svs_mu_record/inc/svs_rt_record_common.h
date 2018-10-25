#ifndef _SVS_RtRecord_Common_h_
#define _SVS_RtRecord_Common_h_
#include <list>
#include <vector>
#include <string>
#include <map>
#include "vms/vms.h"
#include "svs_sub_system.h"
#include "svs_ace_header.h"
#include "svs_utility.h"

#ifdef UNITTEST
#define UTAPI virtual
#else
#define UTAPI
#endif
#ifndef O_LARGEFILE
#define  O_LARGEFILE    0100000
#endif
#ifndef O_DIRECT
#define  O_DIRECT 040000 /* direct disk access hint */
#endif

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS   64
#endif


// rt�ϱ�¼��ý��ͨ��״̬��ʱ���ڣ���λ:��
#define REPORT_RECORD_STATUS_INTERVAL        3

// ¼��ý��ͨ��״̬�ȶ�ʱ����3���ϱ����ڣ���λs
#define RECORD_MEDIA_STATUS_READY_TIME    (3 * REPORT_RECORD_STATUS_INTERVAL)

// ÿ��ý��ͨ��״̬�ϱ���Ϣ�а�����ͷ����
#define REPORT_RECORD_STATUS_MAX_DEV_NUM 1000

#define SVS_PRINT_MSG_NUM   5000

// ¼��״̬�������
#define SVS_CHECK_RECORD_STATUS_INTERVAL  1

//ÿ��ɾ��������ɾ�����¼��ʱ������λ:��
#define SVS_DEL_SUB_TASK_MAX_TIME               (5*60*60)

//¼��ɾ�������������ʱ�䣬��λ:��
#define SVS_DEL_TASK_START_INTERVAL_TIME        (60*60)

//¼��ɾ������������ʱʱ�䣬��λ:��
#define SVS_DEL_TASK_START_DELAY_TIME           (1*60)

//¼��ɾ�����������ؼ��ʱ�䣬��λ:΢��
#define SVS_DEL_FLOW_CONTROL_INTERVAL_TIME      (1500*1000)

// ɾ������Ŀ¼������ʱʱ�䣬��λ:��
#define SVS_DEL_DATE_DIR_DELAY_TIME            (60*60)

// ɾ������Ŀ¼��ʱ���ڣ���λ:��
#define SVS_DEL_DATE_DIR_INTERVAL_TIME         (24*60*60)

// ���Ԥ¼ʱ������λ:��
#define SVS_PRORECORD_MAX_TIME                 (5 * 60)

// ���´���״̬�ļ�����������ʱʱ�䣬��λ:��
#define SVS_UPDATE_SERVER_STATUS_DELAY_TIME            (30)

// ���´���״̬�ļ����ݶ�ʱ���ڣ���λ:��
#define SVS_UPDATE_SERVER_STATUS_INTERVAL_TIME         (30)

//¼���¼��ֱ�����󳤶�
#define SVS_RECORD_EVENT_TABLE_NAME_MAX_LEN     32

//¼���¼��ֱ�������
#define SVS_RECORD_EVNET_TABLE_MAX_NUM          1000

// һ���Զ�ʱ����ʱʱ������λ����
#define SVS_TIMER_INTERVAL_TIMEOUT              (60*60)

//¼���¼���ʧ�󣬴���ȴ�ʱ��,��Сֵ����λ������
#define SVS_EVENT_RECORD_LOST_MIN_TIMEOUT       30

//¼���¼���ʧ�󣬴���ȴ�ʱ��,���ֵ����λ������
#define SVS_EVENT_RECORD_LOST_MAX_TIMEOUT       120

// ¼���¼���ʧ�󣬶�ʱ����Ƿ�ﵽ���ʱ����ѭ����ʱ�����ڣ���λ:��
#define SVS_EVENT_RECORD_LOST_INTERVAL          10

//¼���¼���ʧ�󣬴���ȴ�ʱ��,Ĭ��ֵ����λ������
#define SVS_EVENT_RECORD_LOST_DEFAULT_TIMEOUT   30

// ǰ��¼�񱸷ݣ����α��ݿ�ʼʱ������ϴα��ݽ���ʱ�����С��������С�ڸü�������ݣ���λ:��
#define SVS_BACKUP_MIN_DISTANCE_TIME            120

// ǰ��¼�񱸷ݼ�¼����ʱ�������������ʱ����ڸ�ʱ�������ٱ�����¼����λ:��
#define SVS_BACKUP_FALG_HOLD_TIME_LEN           1200
// �����ϱ����ڣ� Ĭ��ֵ����Сֵ�����ֵ����λ��
#define SVS_LOAD_REPORT_DEFAULT_INTERVAL    (20)
#define SVS_LOAD_REPORT_MIN_INTERVAL        (10)
#define SVS_LOAD_REPORT_MAX_INTERVAL        (5*60)


//��RtRecordSvr��ͬһ����ͷ¼��.
#define _SVS_MULTI_RT_RECORD_VS_ONE_LENS_

//������ݻ�������󳤶�
#define RECV_DATA_BUFF_MAX_LEN  (2*1024*1024)

//����������ȴ�ʱ�䣬��λ��΢��
#define RECV_DATA_MAX_TIMEOUT   (100 * 1000)

//SOCKET���������ֵ
#define SVS_SOCKET_BUFF_MAX     (256*1024)

//������Ϣ����ˮλ
#define SVS_SEND_MSG_QUEUE_WM   (20*1024*1024)

// IO������Ϣˮλ
#define SVS_IO_TASK_QUEUE_WM    (2*1024*1024)

//һ���Է�����Ϣ����
#define SVS_ALLOC_MSG_BUFF_COUNT    200

//һ�����ͷ���Ϣ����
#define SVS_FREE_MSG_BUFF_COUNT     200

//�������¼�ʧ�ܺ��ش���ʱ�䣬��λ����
#define SVS_PROC_MONITOR_EVENT_INTERVAL_TIME    10

//ȡ����¼��������Ϣ��ʱʱ�䣬��λ����
#define SVS_GET_PROC_TASK_TIMEOUT           2

//����������ݿ���������ȴ�ʱ�䣬��λ����
#define SVS_MAX_DB_OPERATION_TIMEOUT        5

//����������ݿ���������ȴ�ʱ�䣬��λ����
#define SVS_BAKUP_MAX_DB_OPERATION_TIMEOUT        3

//ÿ������ݿ��������ύһ��
#define SVS_MAX_OPERATION_PER_COMMIT        100

//¼��澯��Ϣ����г�ʱʱ��,��λ:΢��
#define SVS_ENQUEUE_ALARM_EVENT_TIMEOUT   100

//Ĭ�ϵ�ǰ���豸��������
#define SVS_DEFAULT_ENCODE_TYPE             44

//�˳���Ϣ����
#define SVS_EXIT_MSG_LEN                    512

//��ʱ�����?Ӧ���������ʱ�䣬��λ��΢��
#define SVS_REACTOR_PROC_TIME               (500*1000)

#define SVS_FRAME_TIMETICK_INTERVAL_MAX    1000
#define SVS_FRAME_DEFAULT_TIMETICK          40

#define SVS_RESULT_FAILURE         -1

//��Ϣ��������,��λcaps
#define RTRECORDTOKENNUM                               2000
//���ƶ�ʱ������,10ms
#define RTRECORDTIMERSCALE                             10
//��ʱȥˢ�����Ƶ�ʱ����,��λ10ms
#define RTRECORDPROMSGRATE                             1
//���¼������
#define RTRECORDMAXWRITERATE                          400

enum SVS_ERR_CODE
{
    // ͨ�ô���,ϵͳ��
    SVS_RESULT_OK                   = 0x0000,
    SVS_ERR_NOMEM                   = 0x0001,
    SVS_ERR_QUEUE                   = 0x0002,
    SVS_ERR_PARAM                   = 0x0003,
    SVS_ERR_NOVFD                   = 0x0004,
    SVS_ERR_NOMSG                   = 0x0005,
    SVS_ERR_WAIT_RESP               = 0x0006,
    SVS_ERR_OTERERR                 = 0x0007,

    // �洢�����
    SVS_ERR_NOSPACE                 = 0x1001,
    SVS_ERR_NODISK                  = 0x1002,
    SVS_ERR_NORAID                  = 0x1003,
    SVS_ERR_THRESHHOLD              = 0x1004,
    SVS_ERR_DISKFAULT               = 0x1005,
    SVS_ERR_DISKPERF                = 0x1006,
    SVS_ERR_FILESYSTEM              = 0x1007,
    SVS_ERR_WRITEOVERTIME           = 0x1008,
    SVS_ERR_IOERR                   = 0x1009,
    SVS_ERR_FSREADONLY              = 0x100A,
    SVS_ERR_NOFILE                  = 0x100B,
    SVS_ERR_INVALIDFD               = 0x100C,
    SVS_ERR_CHANGEDISK              = 0x100D,
    SVS_ERR_MOUNTOFFLINE            = 0x100E,
    SVS_ERR_STATFILE                = 0x100F,
    SVS_ERR_FD_SWITCH_FILE          = 0x1010,

    // ��Ϣ�����
    SVS_ERR_MSGTYPE                 = 0x2001,
    SVS_ERR_MSGLEN                  = 0x2002,
    SVS_ERR_MSGPARAM                = 0x2003,

    // ¼�����������
    SVS_ERR_RTOVERTIME              = 0x3001,/*�������*/
    SVS_ERR_DEVOFFLINE              = 0x3002,/*��ͷ������*/
    SVS_ERR_NOTASK                  = 0x3003,/*��ͷ��¼������*/
    SVS_ERR_NOROUTER                = 0x3004,/*��ͷ���벻��·��*/
    SVS_ERR_DATAOVERTIME            = 0x3005,/*��ͷý����ݽ��ճ�ʱ*/
    SVS_ERR_CONNOVERTIME            = 0x3006,/*��ͷ����VTDU��ʱ*/
    SVS_ERR_OVER_LOAD               = 0x3007,/*�������ͷ����*/

    // ��ݽ��������
    SVS_ERR_RECV_FAIL               = 0x4001,    // ͨ�ý��մ���
    SVS_ERR_RECV_OVERTIME           = 0x4002,   // ���ճ�ʱ
    SVS_ERR_RECV_ERRDATA            = 0x4003,   // ���յ��������

    SVS_ERR_BUTT
};

// ¼���ļ���չ��
#define SVS_RT_FILE_EXNAME          "vod"
#define SVS_BACKUP_FILE_EXNAME      "vod.front"

// ý�������ջ������С
#define SVS_SOCKET_RECV_BUFF_MAX            (32*1024)

//���仺����ʧ�ܺ�,�ȴ�ʱ��,��λ:΢��
#define SVS_ALLOC_BUFFER_SLEEP_TIME        (10*1000)

#define PRINT_ALLOC_BUFFER_FAILURE_TIMES    50000

#define SVS_EPOLL_MAX_HANDLE_NUM            20000

//�߳�ջ���ֵ
#define SVS_THREAD_STATCK_MAX               (128*1024)

//����Ƿ��еȴ�¼��ͷ��ʱ��,��λ:��
#define SVS_CHECK_WAIT_RECORD_LEN_TIME      (10*60)

#define SVS_RETRY_STOP_RECORD_TOTAL_TIMES   5
//����ֹͣ¼�����,ÿ�ô���ͣһ��¼��,ÿ��sleep SVS_CHECK_STOP_RECORD_INTERVAL_TIME
#define SVS_RETRY_STOP_RECORD_TIMES         60

//��ݿ����ʧ���������Ӵ���
#define SVS_DB_FAUILT_RETRY_TIMES           1

//��ݿ����ʧ���������ӵȴ�ʱ��(��)
#define SVS_DB_FAULT_RETRY_WAIT_TIME        1

//��ӡд�ļ�������,����
#define PRINT_WRITE_FILE_NUM                1000

//��ӡ������״̬���,����
#define PRINT_RECV_BUFFER_STATUS_NUM        50000

//����ý���������߳���
#define SVS_RECV_MEDIA_THREAD_MAX_NUM       32

//¼���ļ�¼��ʱ��,��λ:��
#define SVS_RECORDFILE_SLICE_TIME           30

//��Ƶ֡��󳤶�
#define SVS_RT_MEDIA_FRAME_MAX_LEN          (64*1024)

//����ý������������С����  128K+��Ƶ֡��󳤶�
#define SVS_RECV_RT_MEDIA_DATA_BUFFER_MIN_LEN (128 * 1024)

//����ý������������󳤶�  10M+��Ƶ֡��󳤶�
#define SVS_RECV_RT_MEDIA_DATA_BUFFER_MAX_LEN (10 * 1024 * 1024)

//����ý����������������  1.7G
#define SVS_RECV_RT_MEDIA_DATA_POOL_MAX_LEN (1700*1024*1024)

//����ý�建������С����
#define SVS_RECV_RT_MEDIA_DATA_BUFFER_MIN_NUM 1

//����ý�建������С����(��ͷ��2��)
#define SVS_MAX_LENS_NUM_2                  2

#define SVS_TEMP_BUFFER_MAX_NUM             800

//----------��Ϣ����ˮλ------------start
//������Ϣ����ˮλ
#define SVS_CMD_MSG_QUEUE_WM                (50*1024*1024)

//�ص���Ϣ����ˮλ
#define TIMER_CALL_BACK_QUEUE_WM            (50*1024*1024)

// �ϲ�¼���ļ���Ϣˮλ
#define UNITE_FILE_QUEUE_WM                 (50*1024*1024)

// ����ǰ��¼��������Ϣˮλ
#define BAKUP_FRONT_RECORD_TASK_QUEUE_WM    (20*1024*1024)

//�澯�¼�����ˮλ
#define SVS_ALARM_EVENT_QUEUE_WM            (50*1024*1024)

//����¼���ļ�������Ϣ����ˮλ��С
#define RECORD_FILE_INDEX_QUEUE_WM          (50*1024*1024)

//����¼����Զ���ˮλ
#define SVS_RENEW_STRATEGY_QUEUE_WM         (20*1024*1024)

//����¼�����ˮλ
#define SVS_MONITOR_EVENT_QUEUE_WM          (10*1024*1024)

// ���Կ���̨��Ϣ�Ķ���ˮλ
#define SVS_CONSOLE_MESSAGE_QUEUE_WM        (10*1024*1024)

// ¼����ٶ�ʱ������ˮλ
#define SVS_CONSOLE_TIMER_CALLBACK_QUEUE_WM (10*1024*1024)


/* RtRecordSvr����ɾ������,������ʱɾ������,������ɾ������,Ԥ¼�ļ�ɾ������,
 * ������� del_task_queue_, ����ˮλΪ DEL_RECORD_TASK_QUEUE_WM
 * Ȼ��Ըö����е�������д���, ����ɾ����Ϣ,������� del_record_queue_, ����ˮλΪ DEL_RECORD_QUEUE_WM
 * Ȼ�����ض�ʱ�����ʱ���ȡ�ö����е���Ϣ, �����Ϣ����,
 * ��ʱɾ����Ϣ�ʹ�����ɾ����Ϣ������� del_record_file_queue_, ����ˮλΪ DEL_RECORD_FILE_QUEUE_WM
 * Ԥ¼�ļ�ɾ����Ϣ������� del_prerecord_file_queue_, ����ˮλΪ DEL_RECORD_PRERECORD_QUEUE_WM
 */
//ɾ���������ˮλ
#define DEL_RECORD_TASK_QUEUE_WM                  (10*1024*1024)

//ɾ����Ϣ����ˮλ
#define DEL_RECORD_QUEUE_WM                     (10*1024*1024)

//ɾ���ļ���Ϣ����ˮλ
#define DEL_RECORD_FILE_QUEUE_WM               (10*1024*1024)

//ɾ��Ԥ¼��Ϣ����ˮλ
#define DEL_RECORD_PRERECORD_QUEUE_WM         (10*1024*1024)

// ɾ������Ŀ¼��Ϣˮλ
#define DEL_DATE_DIR_WM     (10*1024*1024)

// ���ļ��汾ɾ������ʼ����ʱ��
#define NEW_DEL_START_TIME    "20090101000000"

// ɾ����������г�ʱʱ�䣬��λ:����
#define DEL_RECORD_TASK_ENQUEUE_TIMEOUT   100

//----------��Ϣ����ˮλ------------end
// ��־�ַ��ȣ�����20080901
#define RT_DATE_STR_LEN                      8

// ����ý����ݻ������С 8M
#define UNITE_MEDIA_BUFF_LEN                 (8*1024*1024)

//��ʼ����¼��ʱ,�м���ʱ��,��λ:΢��
#define EXEC_STRATEGY_SLEEP_TIME            (10*1000)

//ÿ����*��¼��,����һ��ʱ��
#define EXEC_STRATEGY_SLEEP_DEVICE_NUM      20

//ǰ���豸ֹͣ״̬
#define SVS_FRONT_DEVICE_STATE_STOP         0x20

//����ֹͣ��Ϣ����������߳�������
#define SEND_EXIT_MSG_TIMES                 3

// ֧��Raid��������
#define SVS_RAID_MAX_NUM                     64

// ֧��LUN������
#define SVS_LUN_MAX_NUM                      32


//¼������б�Ĭ�ϴ�С,��λ: 20
#define STRATEGY_LIST_DEFAULT_SIZE          20

//�������ļ��ִ��������С
#define READ_CONFIG_STRING_BUF_LEN          512

//��Ϣ���������ֵ
#define MSG_TRANSACTION_MAX_NUM             0x7fff

//�豸�������ͻ������
#define DEVICE_ENCODE_TYPE_BUF_LEN          1024

//ʵʱ����ݰ��С���ֵ
#define RT_FRAME_MAX_SIZE                   (1024*2)

//����ý������ݰ���������
#define SVS_RECV_MEDIA_DATA_BUF_MAX_NUM     10240


//SQL��仺�����С
#define SVS_SQL_BUFF_LEN                    1024

//��ݿ����¼���¼�ID��󳤶�
#define DB_RECORD_EVETN_ID_LEN              24

//������־��Ϣ��󳤶�
#define NET_LOG_DESC_MAX_LEN                512

//ʱ���ִ���󳤶�
#define SVS_STR_TIME_MAX_LEN                64

//IP��ַ�ִ���󳤶�
#define SVS_STR_IP_ADDR_MAX_LEN             128

//����UDP��Ƶ�����󳤶�
#define SVS_RECV_UDP_VIDEO_PACKET_MAX_LEN   2048

//Start -- ����ͳ�Ƽ���
//ÿд¼���ļ�������־���
#define LOG_WRITE_FILE_TIMES                1000

//ÿ�յ���Ƶ�������־���
#define LOG_RECV_PACKET_NUMS                5000

//End -- ����ͳ�Ƽ���

//Ĭ��Զ�̵��Զ˿�
#define SVS_DEFALT_REMOTE_DEBUG_PORT        40404

//--------ʱ��������--------start----

// Sleep״̬���豸����Ƿ�����¼����(s)
#define SVS_RETRY_SLEEP_LENS_TIME          60

//¼����������г�ʱʱ��,��λ:΢��
#define SVS_ENQUEUE_RECORD_INDEX_TIMEOUT   100

//��ӡ���հ�״̬���,��
#define PRINT_RECV_PACKET_STATUS_INTERVAL_TIME (10*60)

//�����˳��ȴ�ʱ��,��λ:��
#define SERVER_EXIT_WAIT_TIME               5

//ֹͣ��Ϣ����г�ʱʱ��,��λ:��
#define SVS_ENQUEUE_TIMEOUT                 1

//����¼����������ʱ��,��λ:��
#define START_RECORD_CHECK_INTERVAL_TIME    10

//���͹ܵ���ݳ�ʱʱ��,��λ:��
#define SEND_PIPE_DATA_TIMEOUT              2

//������Ƶ��ݣգģа�ʱʱ�䣬��λ����
#define RECV_UDP_DATA_TIMEOUT               1

//������Ϣ��ʱʱ��,��λ:��
#define SVS_SEND_MSG_TIMEOUT                10

//����·�����Լ��(��)
#define REQUEST_ROUTER_INTERVAL_TIME        30

// ����vtdu�ӳ�ʱ��
#define REQUEST_ROUTER_DELAY_TIME           5

//VTDU����ý����ݰ�(����֡)��ʱ��Сʱ��(��)
#define VTDU_SEND_DATA_MIN_TIMEOUT           5

//д¼���ļ���ʱʱ��(��)
#define WRITE_RECORD_FILE_TIMEOUT           60

#define TIME_UNIT_MINUTE                    60

//�������,���ʱ����(��)
#define CHECK_DISK_INTERVAL_TIME            120

//�ֶ�¼�����ʱ��(��)
#define MANUAL_RECORD_OVERTIME_TIME           (6*60*60)

//ֹͣ¼��ʱ�����״̬ʱ��������λ��΢��
#define SVS_CHECK_STOP_RECORD_INTERVAL_TIME (1000*1000)


//--------ʱ��������--------end----

//Start----���ܹ��----
//Ĭ�����¼��ͷ��
#define DEFAULT_MAX_LENS_NUM                50

//Ĭ�ϼ���������(��)
#define CHECK_DISK_INTERVAL_DEFAULT         5
#define CHECK_DISK_INTERVAL_MAX             30

//Ĭ�ϴ��̿��пռ��СK
#define DEFAULT_DISK_LOWER_LIMIT            (4*1024)

// ����Ŀ¼���̿ռ���С��ֵ,��λ:KB
#define RECORD_INDEX_FILE_DISK_LIMIT        (100*1024)

//Start----���ܹ��----
//��������߳�������
#define RECV_MEDIA_DATA_MAX_THREAD_NUM      32
//��������߳�Ĭ�ϸ���
#define RECV_MEDIA_DATA_DEFAULT_THREAD_NUM  1

//д¼���ļ�Ĭ���߳���
#define WRITE_MEDIA_DATA_DEFAULT_THREAD_NUM 4
#define WRITE_MEDIA_DATA_MIN_THREAD_NUM         2
#define WRITE_MEDIA_DATA_MAX_THREAD_NUM         32

// ϵͳ��֧�ֵĲ���������raid����
#define SYNC_WORK_RAID_GROUP_MIN_NUM         1
//#define SYNC_WORK_RAID_GROUP_MAX_NUM    SVS_RAID_MAX_NUM
#define SYNC_WORK_RAID_GROUP_DEFAULT_NUM    2

//���¼��ͷ��
#define RECORD_MAX_LENS_NUM                 2000//1000
//End----���ܹ��----


#define ALARM_ID_CONNECT_STORESVR           606024001



enum ALARM_ATATUS_EN
{
    ALARM_STATUS_FAULTING = -1,
    ALARM_STATUS_RESUMED = 0
};

enum ALAEM_INDEX_EN
{
    ALARM_CONNECT_STORESVR_INDEX = 0,
    ALARM_ACCESS_DATABASE_ERROR_INDEX,
    ALARM_INDEX_NUM
};
// End----�澯ID----

// ���õ�λ����
#define RTRECORD_UNIT_K                     1024.0
#define RTRECORD_UNIT_BYTE                  8.0

// �ٷ���
#define RTRECORD_UNIT_PERCENT               100

// Start-----------------¼��ɾ��--------------
// ¼�񱣴�����
#define RECORD_DEFAULT_STORE_DAYS     3

// ��С¼�񱣴�����
#define RECORD_MIN_STORE_DAYS          1

// ���¼�񱣴�����
#define RECORD_MAX_STORE_DAYS          90

// ¼���ļ�ʱ��Ƭ����λ:��
#define BIG_RECORDFILE_TIME_SLICE         3600


// ˢ�������ļ�����,��λ:��
#define REFRESH_CONFIG_INTERVAL             60

//¼���¼���ΪRECORD_EVENT_CONTROL_TYPE_MAX(=5)��, ÿ��¼���¼�������
//��ͬ�澯Դ����. ͬһ�澯Դ���Բ�����¼���¼�.
//enum RECORD_EVENT_CONTROL_TYPE
//{
//    RECORD_EVENT_CONTROL_TYPE_MANUAL = 0,   //�ֶ�¼��
//    RECORD_EVENT_CONTROL_TYPE_ALARM,    //�澯�¼�
//    RECORD_EVENT_CONTROL_TYPE_VIDEO,    //��Ƶ��ʧ/�ָ�
//    RECORD_EVENT_CONTROL_TYPE_MOTION,   //�˶�������
//    RECORD_EVENT_CONTROL_TYPE_DEVICE,   //�豸������
//    RECORD_EVENT_CONTROL_TYPE_MASK,     //ͼ���ڱ�
//    RECORD_EVENT_CONTROL_TYPE_DISK,     //���̸澯
//    RECORD_EVENT_CONTROL_TYPE_MAX,
//};

//¼���¼���ΪRECORD_EVENT_CONTROL_TYPE_MAX(=5)��, ÿ��¼���¼�������
//��ͬ�澯Դ����. ͬһ�澯Դ���Բ�����¼���¼�.
typedef map <string, int32_t> EVENTMAP;


enum RECORD_INDEX_SET_TYPE
{
    RECORD_INDEX_TYPE_UPDATE = 0,
    RECORD_INDEX_TYPE_INSERT = 1,
    RECORD_INDEX_TYPE_OVERWRITE = 2,
};


// ¼��ɾ����
enum DEL_FLAG_TYPE
{
    DEL_FLAG_SELECT = 0,                /* ɾ����:ɾ��ͷ��ѡ¼��*/
    DEL_FLAG_ALL,                       /* ɾ����:ɾ��ͷȫ��¼�� */
    DEL_FLAG_LAST_DISK_FULL_TASK     /* ɾ����:���һ������ɾ����������ʶ */
};


// ɾ������
enum DEL_RECORD_TYPE
{
    DEL_RECORD_TYPE_NORMAL = 0,
    DEL_RECORD_TYPE_DISK_FULL,
    DEL_RECORD_TYPE_PRERECORD,
    DEL_RECORD_TYPE_DATE_DIR,
    DEL_RECORD_TYPE_MAX
};


enum RT_RECORD_TIMETICK
{
    RT_RECORD_TIMETICK_LOCAL      = 0,
    RT_RECORD_TIMETICK_PU         = 1
};



#define SVS_DEL_MAX_INQUEUE_NUM  10    // ɾ�����������������д���

#define SECONDS_PER_DAY      (86400)                /* һ�������   */
#define SECONDS_PER_HOUR     (3600)                 /* һСʱ������ */

#define ADJUST_FORWARD(X, Y)     ((X) - (X) % (Y))         /* ���� X ��ǰ���� Y �ı���*/
#define ADJUST_BACKWARD(X, Y)    ((X) + (Y) - (X) % (Y))   /* ���� X ������ Y �ı���*/

typedef struct _SVS_DELETE_LENS_INFO
{
    uint8_t    LensID[LOCALIZE_DEVICE_ID_MAX_LEN + 1];    /* ��ͷID */
    time_t           MinTime;                    /* �����¼��ʱ�� */
    time_t           MaxTime;                    /* �����¼��ʱ�� */
    uint32_t     StoreDays;                  /* ¼�񱣴�����   */
    uint8_t    DeleteMode;                 /* ɾ��ģʽ 1-ȫ��ɾ��0-����ɾ��*/
}SVS_DELETE_LENS_INFO, *PSVS_DELETE_LENS_INFO;


typedef struct _SVS_DEL_RECORD
{
    uint8_t    MsgType;                    /* 0 - ��Ķ�ʱɾ��
                                                     1 - ����ɾ����ɵ�¼���ļ�
                                                     2 - ɾ��Ԥ¼�ļ� */
    time_t          DeleteTime;                 /* ɾ��ʱ�� */
    uint8_t   LensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];  /* ɾ��Ԥ¼�ļ�ʱ��Rt֪ͨ*/

}SVS_DEL_RECORD, *PSVS_DEL_RECORD;

typedef struct _SVS_DEL_RECORD_FILE
{
    uint8_t  MsgType;                       /* ��Ϣ���� 0 - ��Ķ�ʱɾ��
                                                               1 - ����ɾ����ɵ�¼���ļ�
                                                               2 - ɾ��Ԥ¼�ļ�*/
    uint8_t  LensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];     /* ��ͷID       */
    time_t         StartTime;
    time_t         EndTime;
    uint8_t  DeleteMode;                    /* ɾ��ģʽ 1-ȫ��ɾ��0-����ɾ��*/
}SVS_DEL_RECORD_FILE,*PSVS_DEL_RECORD_FILE;

//��ͷ����
typedef struct _SVS_LENS_PARAMETER
{
    uint32_t  RecordStoreDays;            // ¼�񱣴�����
    uint32_t BeforeSeconds;              // Ԥ¼ʱ��
    uint32_t AfterSeconds;               // ��¼ʱ��
}SVS_LENS_PARAMETER,*PSVS_LENS_PARAMETER;


typedef struct _SVS_LOAD_REPORT_TASK
{
    uint32_t LoadReportType;
}SVS_LOAD_REPORT_TASK;

typedef struct _SVS_REPORT_RECORD_STATUS_TASK
{
    uint32_t ReportType;
}SVS_REPORT_RECORD_STATUS_TASK;

#define CONF_FILE  "../conf/svs_mu_record.conf"
#define LOG_FILE  "../log/svs_mu_record.log"

#define ROUND_UP(x)         ((x+SVS_RECORD_FILE_ALIGN_LEN)&0xfffff000)


//����ѡ��---

//------д�����Ż�����Ľṹ-------------

#pragma pack(push, 1)
enum REPACK_FRAME_CONST
{
    SEGMENT_MAX_SIZE = 1400,
    SVS_STREAM_DATAGRAM_MAX = 1418,
};


#define SVS_INDEXFILE_HEADER_STR "AllCam INDEX FILE V1.0"

#define SVS_RECORD_FILE_ALIGN_LEN           4096

typedef struct _SVS_EXTEND_FRAME_HEADER
{
    uint32_t   nFrameSize;             //֡����,����չ���֡ͷ���
    uint32_t   nAdjustFrameSize;       //У��֡����,����չ���֡ͷ���,4K���벹�����(����еĻ�)
    uint32_t   nPacketCount;           //��֡С�����
    uint8_t   ReserveData[4];         //�����ֽ�
}SVS_EXTEND_FRAME_HEADER,*PSVS_EXTEND_FRAME_HEADER;


typedef struct _SVS_Media_Index
{
    uint32_t     LastIndexFrameID;           // ��һ����������֡ID
    time_t            LastIndexRecvTime;          // ��һ�ε�����Ľ���ʱ��

    uint32_t     NewIndexFrameID;           // �µ�һ�ε��������ʼID
    time_t            NewIndexRecvTime;          // �µ�һ�ε��������ʼʱ��
    uint32_t     NewIndexMediaPos;          // �µ�һ�ε�ý������ڻ������е�ƫ��
}SVS_Media_Index;

typedef std::list<SVS_Media_Index>            INDEX_LIST;
typedef std::list<SVS_Media_Index>::iterator  INDEX_LIST_ITER;


//������ݻ������ṹ��
typedef struct _SVS_Recv_Media_Header
{
    uint32_t       HandlerIndex;            //д¼����
    time_t              FirstFrameRecvTime;     //��һ֡����ʱ��

    time_t              LastFrameRecvTime;      //���һ֡����ʱ��
    time_t              LastPacketRecvTime;     //���һ�����ʱ��
    uint32_t       MediaDataStartPos;      //ý����ݿ�ʼƫ��λ��

    uint32_t       RecvedFrameCount;       //�ѽ���֡����
    uint32_t       LastFrameStartPos;      //���һ����ȫ֡�ڻ������ƫ��λ��
    INDEX_LIST          *IndexList;              // �û����������б�

    INDEX_LIST          *IndexBreakList;
    uint32_t       CurFrameStartPos;       //��ǰ֡��ʼ�ڻ������ƫ��λ��
    uint32_t       CurRecvFrameID;         //��ǰ����֡ID
    uint32_t       CurRecvedDataLen;       //��ǰ֡������ݳ���
    uint32_t       CurRecvedPacketCount;   //��ǰ֡�ѽ��հ����
    bool                IsTmpfsMemory;            // �Ƿ�Ϊtmpfs�ڴ��
    int32_t                 TmpfsFd;                   // tmpfs �ļ����
    uint32_t       TmpfsDatalen;             // tmpfs ��ݴ�С
    uint32_t       TmpfsPos;                 // tmpfs �ļ�λ��
}SVS_Recv_Media_Header;

typedef struct _SVS_NEW_ALL_HEADER
{
    SVS_EXTEND_FRAME_HEADER     ExtendFrameHeader;  //��չ֡ͷ
    SVS_MEDIA_FRAGMENT_HEADER   PacketHeader;       //��ͷ
    SVS_MEDIA_STREAM_HEADER     StreamHeader;       //��ͷ
    SVS_MEDIA_FRAME_HEADER      FrameHeaer;         //֡ͷ
}SVS_NEW_ALL_HEADER;

#pragma pack(pop)


#pragma pack(push, 1)

//��ʱ���ص�������Ϣ
typedef struct _SVS_TIMER_CALL_BACK_MSG
{
    enum MSG_TYPE_ENUM
    {
        RT_RECORD_DEVICE_MANAGER_TYPE = 0,
        RECORD_INFORM_MANAGER_TYPE,
        REOCRD_SERVER,                    // ϵͳ
        RECORDBAK_Dev_Manager_TYPE,        // ǰ��¼�񱸷�
        BACKUP_RECORD_STATUS_TYPE,      // ǰ��¼�񱸷�״̬���
        REALTIME_RECORD_STATUS_TYPE,       // ʵʱ¼��״̬���
        RECORD_TRACE_TYPE,
        UNACKED_INDEX_TIME_CHECK,       // ���ڼ��δ��Ӧ������
    };

    int32_t            EventId;        //�¼�ID
    int32_t            TimerId;        //��ʱ��ID
    int32_t             Dummy;
    void*           ProcObject;     //�������ָ��
    uint8_t   MsgType;        //��Ϣ����
}SVS_TIMER_CALL_BACK_MSG,*PSVS_TIMER_CALL_BACK_MSG;

typedef struct _SVS_TLV_STRUCT
{
    uint16_t Type;        //����
    uint16_t Length;      //����
}SVS_TLV_STRUCT;

#pragma pack(pop)

//ʱ��ת����yyyy-mm-dd hh:mi:ss��ʽ�ִ�
char *time2string(char *pDestBuf, int32_t nbuflen, time_t srcTime);

// ¼�����¼�����
enum RT_MONITOR_EVENT_TYPE
{
    RT_MONITOR_EVENT_START_RECORD = 0,
    RT_MONITOR_EVENT_STOP_RECORD,
    RT_MONITOR_EVENT_DISK_ALARM,
    RT_MONITOR_EVENT_MAX
};
// ������Сͳ������
#define SVS_FLUX_STAT_MIN_INTERVAL  5
// ������������,�ֽ�
#define SVS_FLUX_MIN_VALUE (1024*1024*25)
// ý�建����ˮλ���ֵ
#define SVS_MEDIA_BUFF_MAX_LEVEL    0.6
// ý�建����ˮλ�澯ʱ��д�̵���С����
#define SVS_MEDIA_BUFFER_ALARM_FLUX_MIN_VALUE (1024*1024*50)

#pragma pack(push, 1)
typedef struct _SVS_EXCEPTION_ALARM
{
    uint16_t              ExceptEvent;   // �쳣�¼�����
    int32_t                         ExceptCode;    // �쳣�룬�ο��������Ͷ���SVS_ERR_CODE
    int32_t                         RaidID;        // �����ϱ��쳣ʱ��Я��raid_id,������Ϊ-1
    std::vector<std::string>    *DevIDVec;    // �쳣Ӱ����豸�б�ָ��
}SVS_EXCEPTION_ALARM;

#pragma pack(pop)


struct Alarm_Event_Msg
{
    uint8_t   eventType;
    uint32_t    controlCode;
    char            lensID[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
    time_t          alarmEventTime;
};

#endif // SVS_RtRecord_Common.h



