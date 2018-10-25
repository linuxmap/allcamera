/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : SVS_Stat_Manager.h
  �� �� ��   : ����
  ��������   : 2009��8��22��
  ����޸�   :
  ��������   : ȫ�ֵ�ʵ��ͳ����,ʵ���˶�ҵ��,ϵͳ�ڲ���Դ��ͳ�Ƽ����
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2009��8��22��
    �޸�����   : �����ļ�

******************************************************************************/
#ifndef __SVS_STAT_MANAGER_H__
#define  __SVS_STAT_MANAGER_H__
#include <linux/types.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <svs_ace_header.h>
#include <list>
#include "vms/vms.h"
#include "svs_log_msg.h"
#include "svs_rt_record_common.h"

// ͳ������Ĭ��ֵ����λs
#define SVS_STAT_SYSTEM_INFO_INTERVAL   (5)// �ɼ�ϵͳ��Ϣ����,��λ��
#define SVS_STAT_DEFAULT_INTERVAL       (3*60)// ͳ�����ڣ���λ��
#define SVS_STAT_BUFF_SIZE             (100*1024)
#define SVS_STAT_LOG_DIR                "../log/svs_mu_stat"
#define SVS_STAT_LOG_FILE               "../log/svs_mu_stat/svs_mu_stat_record.log"
#define SVS_STAT_LOG_FILE_MAX_SIZE     (100*1024*1024)

#define SVS_STAT_EFFECT_INTERVAL_NUM    3   // ���������ڵ�ƽ��ֵ

// ������Ϣ����
#define SVS_STAT_NETCARD_INFO_LEN     64
#define MAX_NETCARD_NUM  16

// ��������Ĭ�ϴ�С,��λMb
#define SVS_BANDWIDTH_DEFAULT        1000
#define ABNOMAL_VALUE 0xFFFF
#define BOND_MODE_ACTIVEBACKUP 1
#define BOND_STATE_ACTIVE 0   /* link is active */


typedef struct
{
    in_addr_t     ip;
    char          name[SVS_STAT_NETCARD_INFO_LEN];
    uint32_t bandwidthtotal;    /* Mb */
    uint32_t bandwidthused;     /* Kb */
    uint32_t cur_tx_bytes;
} NetcardInfo;


typedef struct
{
    in_addr_t ip;
    char      bondName[SVS_STAT_NETCARD_INFO_LEN];
    uint32_t       mode;
    uint32_t       slaveNum;
    char      slaveName[MAX_NETCARD_NUM][SVS_STAT_NETCARD_INFO_LEN];
    char       slaveStatus[MAX_NETCARD_NUM];
} BondInfo;

typedef struct
{
    __s32 bond_mode;
    __s32 num_slaves;
    __s32 miimon;
} ifbond_t;

typedef struct
{
    __s32 slave_id; /* Used as an IN param to the BOND_SLAVE_INFO_QUERY ioctl */
    char  slave_name[IFNAMSIZ];
    char  link;
    char  state;
    __s32 link_failure_count;
} ifslave_t;


typedef struct
{
    __u32 cmd;
    __u32 supported;            /* Features this interface supports */
    __u32 advertising;          /* Features this interface advertises */
    __u16 speed;                /* The forced speed, 10Mb, 100Mb, gigabit */
    __u8  duplex;                  /* Duplex, half or full */
    __u8  port;                    /* Which connector port */
    __u8  phy_address;
    __u8  transceiver;             /* Which tranceiver to use */
    __u8  autoneg;                 /* Enable or disable autonegotiation */
    __u32 maxtxpkt;             /* Tx pkts before generating tx int32_t */
    __u32 maxrxpkt;             /* Rx pkts before generating rx int32_t */
    __u32 reserved[4];
} __attribute__ ((packed))          ethtool_cmd_t;
#define ETHTOOL_GSET        0x00000001 /* Get settings. */

class CFluxStat
{
public:
    CFluxStat()
    {
        m_tvLastStatTime.tv_sec = 0;
        m_ullLastStatTotalFlux = 0;
        m_ullRate = 0;
        m_ulStatWriteThreadNum = 0;
        m_ulStatRecvThreadNum = 0;
        m_lWriteBlockTime = 0;
        m_ulSizePerWrite = 0;
        for ( uint32_t i = 0; i < RECV_MEDIA_DATA_MAX_THREAD_NUM; i++ )
        {
            m_ullTotalFlux[i] = 0;
        }

    }

    ~CFluxStat()
    {
    }

    // ������ز���
    void SetStatPara( uint32_t statRecvThreadNum,
        uint32_t statWriteThreadNum,
        uint32_t sizePerWrite)
    {
        m_ulStatRecvThreadNum = statRecvThreadNum;
        if ( m_ulStatRecvThreadNum > RECV_MEDIA_DATA_MAX_THREAD_NUM )
        {
            m_ulStatRecvThreadNum = RECV_MEDIA_DATA_MAX_THREAD_NUM;
        }
        m_ulStatWriteThreadNum = statWriteThreadNum;

        m_ulSizePerWrite = sizePerWrite;
    }

    // ����
    uint64_t Rate() const
    {
        return m_ullRate;
    }

    // ÿ����΢��д1��
    uint32_t WriteBlockTime() const
    {
        return m_lWriteBlockTime;
    }

    // ͳ��
    void Stat();

    // ��������
    void AddFlux( uint32_t threadNo, uint32_t dataFlux )
    {
        if (( threadNo > m_ulStatRecvThreadNum ) || (RECV_MEDIA_DATA_MAX_THREAD_NUM <= threadNo))
        {
            SVS_LOG((SVS_LM_WARNING,
                "Add flux failed.The parameter is invalid.the parameter threadNo[%u], "
                "m_ulStatRecvThreadNum[%u]",
                threadNo, m_ulStatRecvThreadNum));
            return ;
        }

        m_ullTotalFlux[threadNo] += dataFlux;
    }

private:
    // ������0
    void Reset()
    {
        struct timezone tz;
        (void)gettimeofday(&m_tvLastStatTime, &tz );
        m_ullLastStatTotalFlux = 0;
        for ( uint32_t i = 0; i < m_ulStatRecvThreadNum; i++ )
        {
            m_ullTotalFlux[i] = 0;
        }
    }

private:
    volatile uint64_t m_ullTotalFlux[RECV_MEDIA_DATA_MAX_THREAD_NUM];  // ���յ������������ֽ�
    volatile uint64_t m_ullRate;       // ���� �ֽ�/��
    volatile uint32_t m_lWriteBlockTime;    // ÿ����΢��д1��
    uint64_t m_ullLastStatTotalFlux;  // �ϴ�ͳ��ʱ��������
    uint32_t m_ulStatRecvThreadNum; //ͳ�ƽ����̸߳���
    uint32_t m_ulStatWriteThreadNum;   // ͳ��д�̸߳���
    uint32_t m_ulSizePerWrite;         //ÿ��д�����ݴ�С
    struct timeval m_tvLastStatTime;        // �ϴ�ͳ��ʱ��
};


class SVS_Inform_Stat;
class SVS_Device_Stat;
class SVS_Ingress_Stat;
class SVS_Egress_Stat;
class SVS_Media_Output_Stat;
class SVS_Signal_Message_Stat;
class SVS_Media_Buffer_Stat;
class SVS_Disk_Stat;

class SVS_Stat_Manager : public ACE_Task<ACE_MT_SYNCH>
{
private:    // �ڲ����ݽṹ����
    // �����㷨Ȩ��
    enum SVS_STAT_WEIGHT
    {
        SVS_WEIGHT_DISK         = 30,
        SVS_WEIGHT_BANDWIGTH    = 20,
        SVS_WEIGHT_MEDIA_BUFFER = 30,
        SVS_WEIGHT_CPU          = 10,
        SVS_WEIGHT_MEMORY       = 10,
    };

    typedef struct tagCameraStat
    {
        char m_strCameraId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
        SVS_Inform_Stat * m_pInformStat;
        SVS_Device_Stat * m_pDeviceStat;
        SVS_Ingress_Stat * m_pIngressStat;
        SVS_Egress_Stat * m_pEgressStat;
        uint64_t m_recvTotalByteNum;
        uint64_t m_recvTotalFrameNum;
        uint64_t m_lostFrameNum;
        uint64_t m_lostPacketNum;
        uint64_t m_errFrameNum;
        uint64_t m_writeByteNum;
        uint64_t m_writeFrameNum;
        uint64_t m_currRecvByteNum;
        void reset()
        {
            (void)memset(m_strCameraId, 0, sizeof(m_strCameraId));
            m_pInformStat = NULL;
            m_pDeviceStat = NULL;
            m_pIngressStat = NULL;
            m_pEgressStat = NULL;
            m_recvTotalByteNum = 0;
            m_recvTotalFrameNum = 0;
            m_lostFrameNum = 0;
            m_lostPacketNum = 0;
            m_errFrameNum = 0;
            m_writeByteNum = 0;
            m_writeFrameNum = 0;
            m_currRecvByteNum = 0;
        }
    }CameraStat;

    typedef struct tagMediaOutputStat
    {
        uint32_t m_ThreadIndex;
        SVS_Media_Output_Stat* m_pCMediaOutputStat;
        uint64_t m_writeByteNum;
        uint64_t m_writeFrameNum;
        uint64_t m_ioScheduleTimes;
        void reset()
        {
            m_ThreadIndex = 0xFFFFFFFF;
            m_pCMediaOutputStat = NULL;
            m_writeByteNum = 0;
            m_writeFrameNum = 0;
            m_ioScheduleTimes = 0;
        }

    }MediaOutputStat;

    typedef struct tagSignalMsgStat
    {
        SVS_Signal_Message_Stat * m_pCSignalMsgStat;
        uint64_t m_totalMsgCount;
        uint64_t m_totalDisconnStoreCtrlTimes;
        uint64_t m_totalConnStoreCtrlFailTimes;
        uint64_t m_totalHearbeatTimeoutTimes;

        void reset()
        {
            m_pCSignalMsgStat = NULL;
            m_totalMsgCount = 0;
            m_totalDisconnStoreCtrlTimes = 0;
            m_totalConnStoreCtrlFailTimes = 0;
            m_totalHearbeatTimeoutTimes = 0;
        }

    }SignalMsgStat;

    typedef struct tagSystemInfoStat
    {
        // cpu��Ϣ
        uint32_t    m_usedCpu;

        // �ڴ���Ϣ
        uint32_t    m_totalMem; // MB
        uint32_t    m_usedMem;  // MB

        // ������Ϣ
        uint32_t    m_bandwidthTotal;   // mb
        uint32_t    m_bandwidthUsed;    // kb
        char        m_netcardName[MAXNAMLEN];

        // �����ܿռ�
        uint64_t    m_diskTotalSize;// KB
        uint64_t    m_diskUsableSize;// KB

        // ý�建����ˮλ
        float m_mediaBuffWaterLevel;

        void reset()
        {
            m_usedCpu = 0;
            m_totalMem = 0;
            m_usedMem = 0;
            m_bandwidthTotal = 0;
            m_bandwidthUsed = 0;
            m_diskTotalSize = 0;
            m_diskUsableSize = 0;
            m_mediaBuffWaterLevel = 0;
            (void)memset(m_netcardName, 0, sizeof(m_netcardName));
        }

        void operator+(const tagSystemInfoStat &T)
        {
            m_usedCpu += T.m_usedCpu;
            m_totalMem += T.m_totalMem;
            m_usedMem += T.m_usedMem;
            m_bandwidthTotal += T.m_bandwidthTotal;
            m_bandwidthUsed += T.m_bandwidthUsed;
            m_diskTotalSize += T.m_diskTotalSize;
            m_diskUsableSize += T.m_diskUsableSize;
            m_mediaBuffWaterLevel += T.m_mediaBuffWaterLevel;
        }

        void operator/(uint32_t ndiv)
        {
            m_usedCpu /= ndiv;
            m_totalMem /= ndiv;
            m_usedMem /= ndiv;
            m_bandwidthTotal /= ndiv;
            m_bandwidthUsed /= ndiv;
            m_diskTotalSize /= ndiv;
            m_diskUsableSize /= ndiv;
            m_mediaBuffWaterLevel /= ndiv;
        }
    }SystemInfoStat;

    // ý�建����ͳ����Ϣ
    typedef struct tagMediaBufferStat
    {
        SVS_Media_Buffer_Stat*  m_pCMediaBufferStat;
        uint64_t      m_totalAllocTimes;
        uint64_t      m_totalAllocFailTimes;
        uint64_t      m_totalAllocTmpBuffFailTimes;
        uint64_t      m_totalFreeTimes;
        uint32_t      m_bufferSize;
        uint32_t      m_bufferNumber;
        void reset()
        {
            m_pCMediaBufferStat     = NULL;
            m_totalAllocTimes       = 0;
            m_totalAllocFailTimes   = 0;
            m_totalFreeTimes        = 0;
            m_bufferSize            = 0;
            m_bufferNumber          = 0;
            m_totalAllocTmpBuffFailTimes = 0;
        }

    }MediaBufferStat;

    // ����
    typedef struct tagDiskStat
    {
        SVS_Disk_Stat* m_pDiskStat;
        void reset()
        {
            m_pDiskStat = NULL;
        }
    }DiskStat;

    // ��ͷid�;�ͷͳ�ƶ����map
    typedef ACE_Hash_Map_Manager < const char *, CameraStat *, ACE_Null_Mutex > CAMERA_STAT_MAP;

    // ��ͷid�;�ͷͳ�ƶ��������
    typedef ACE_Hash_Map_Iterator < const char *, CameraStat *, ACE_Null_Mutex > CAMERA_STAT_MAP_ITER;
    typedef ACE_Hash_Map_Entry < const char *, CameraStat * > CAMERA_STAT_MAP_ENTRY;

    typedef ACE_Vector <MediaOutputStat> MEDIA_OUTPUT_STAT_VECTOR;
    typedef ACE_Vector <MediaBufferStat> MEDIA_BUFFER_STAT_VECTOR;
    typedef ACE_Vector <DiskStat> DISK_STAT_VECTOR;

    typedef struct TagDevTakeoverInfo
    {
        char DevId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
        time_t TakeTime;
    }DevTakeoverInfo;

    typedef vector <DevTakeoverInfo> DEVTAKEOVERINFO_VECTOR;

public:
    enum enCameraObjectType
    {
        CAMERA_OBJECT_INFORM    = 0,
        CAMERA_OBJECT_DEVICE    = 1,
        CAMERA_OBJECT_INGRESS   = 2,
        CAMERA_OBJECT_EGRESS    = 3,
    };

public:
    enum enStatLogLevel
    {
        STAT_LOG_LEVEL_DEBUG = 0,
        STAT_LOG_LEVEL_INFO = 1,
        STAT_LOG_LEVEL_CLOSEED,
    };


public: // ͨ�ýӿ�
    static SVS_Stat_Manager & instance()
    {
        static SVS_Stat_Manager svs_stat_manager;
        return svs_stat_manager;
    };

    virtual ~SVS_Stat_Manager();

    // ��ʼ��
    int32_t open(void *);

    // ͳ���߳�
    int32_t svc(void);

    // �رն���
    int32_t close(u_long);

public:// �����Խӿ�
    int32_t cameraRegister(const char * strCameraId, enCameraObjectType objectType, void * objectPointer);

    int32_t cameraUnregister(const char * strCameraId, enCameraObjectType objectType);

    void refreshStatLogLeve(uint32_t newStatLogLevel);

    int32_t mediaOutputThreadRegister(uint32_t threadIndex, SVS_Media_Output_Stat * pMediaOutputStat);

    int32_t signalMsgStatRegister(SVS_Signal_Message_Stat * pSignalMsgStat);

    int32_t diskStatRegister(SVS_Disk_Stat * pDiskStat);

    int32_t mediaBufferStatRegister(SVS_Media_Buffer_Stat * pMediaBufferStat,
                                      uint32_t bufferSize,
                                      uint32_t bufferNum);
    // �����ϱ�
    int32_t loadReport(const ACE_Message_Block * mb);
    float getMediaBuffMaxLevel() const
    {
        return m_fMediaBuffMaxLevel;
    }

    // ��ȡ¼����������
    int32_t sampleRecordQuality(const char* strDeviceId, SVS_RECORD_QUALITY_REPORT& szReport);

    // ����¼����������
    int32_t resetSampleInfo(const char * strCameraId);
public:// ͬʱ֧��������״̬����telnet
    int32_t statAllCameras(int32_t nStatHandle);

    int32_t statDisks(int32_t nStatHandle);

    int32_t statMediaOutput(int32_t nStatHandle);

    int32_t statMediaBufferStat(int32_t nStatHandle);
    int32_t statQueueInfo(int32_t nStatHandle);
    int32_t statConfInfo(int32_t nStatHandle);
    int32_t statDeleteRecord(int32_t nStatHandle)const;

    void statDBSql(int32_t nStatHandle)const;

    int32_t statSignalMsg(int32_t nStatHandle);

    int32_t statSystemInfo(int32_t nStatHandle);

    int32_t statAllHandleInfo(int32_t nStatHandle);

    int32_t reportRecordStatus(const ACE_Message_Block * /*mb*/);

private:
    int32_t addCamera(const char * strCameraId,SVS_Inform_Stat* pInformStat);

    void removeCamera(const char* strCameraId);

    ACE_Message_Block* allocMessage();

    void freeMessage(ACE_Message_Block * pMsg);

    int32_t printCameraInfo(CameraStat * pCameraStat,int32_t nStatHandle);

    void getCpuInfo();

    void getMemInfo();

    void getNetcardInfo();
    int32_t getBondNetcardInfo(int32_t isock, SystemInfoStat & szBandwidthInfo, struct ifreq * pIfr)const;
    int32_t getEthNetcardInfo(int32_t isock, SystemInfoStat & szBandwidthInfo, struct ifreq * pIfr)const;

    void getBandwidthInfo();

    void getDiskInfo();

    // void getMediaBuffInfo();
    int32_t getMediaBuffInfo();

    void collectOffhandInfo();

    int32_t reopenStatLogFile();

    int32_t checkStatLogFile();

    int32_t printStatInfo(int32_t nStatHandle, const char * strStatBuff, uint32_t writeLen)const;

    // ���㸺��
    int32_t calculateLoad(uint32_t & totalCameraNum, uint32_t & currLoadCameraNum,
                          uint32_t & systemLoad);

    int32_t getRecordInputRate();
    int32_t getRecordOutputRate();
    int32_t getMediaBuffAveLevel();

    //��ȡҵ������ܿռ�������
    uint32_t GetDiskUsage();

    void getCameraRecordStatus(CameraStat * pCameraStat, RECORD_STATUS_INFO * pRecordStatusInfo, const time_t currTime);
protected:
    SVS_Stat_Manager();
private:
    //=========================public============================
    volatile bool       m_nThreadFlag;
    uint32_t        m_tvStatInterval; // ���һ�μ��ʱ��
    uint32_t       m_nStatLogLevel;
    time_t m_sysStartTime;

    // ͳ���ļ����
    int32_t m_statFileHandle;
    char m_statBuff[SVS_STAT_BUFF_SIZE];

    //=========================Cameras============================
    // ��ͷID�뾵ͷͳ�ƶ���ӳ���
    ACE_Recursive_Thread_Mutex m_MapMutex_;
    CAMERA_STAT_MAP m_cameraMap;

    // ���о�ͷ��Ϣ
    std::list<ACE_Message_Block*> m_FreeCameraStatList;

    // ���������ݽ��м���
    ACE_Recursive_Thread_Mutex m_PerformanceVarMutex_;

    //=========================Media Input============================
    // ý���������
    uint64_t m_totalRecvByte;
    uint64_t m_currRecvByte;
    uint32_t m_currRecvRateMB;  // ���յ���������

    //=========================Media Output============================
    // ý���������
    uint64_t m_totalWriteByte;
    uint64_t m_currWriteByte;
    uint64_t m_totalIoScheduleTimes;
    uint64_t m_currIoScheduleTimes;
    uint32_t m_RecordWriteRate; //ʵʱ¼�������,Mbps
    MEDIA_OUTPUT_STAT_VECTOR m_mediaOutputStatVector;
    uint32_t m_currWriteMB;   // ���͵���������

    //=========================System info==============================
    // ϵͳ��Ϣ������ͳ�� SVS_STAT_EFFECT_INTERVAL_NUM�����ڣ���ƽ��ֵ,
    // ǰSVS_STAT_EFFECT_INTERVAL_NUM��ʵ��ͳ�Ƶ����ݣ����һ����ƽ��ֵ
    SystemInfoStat m_systemInfoStat[SVS_STAT_EFFECT_INTERVAL_NUM +  1];
    uint32_t m_systemInfoIndex; // ��ǰ����ͳ�Ƶ��±�ֵ
    uint32_t m_currTxByte;       // ������ǰ�յ����ֽ�����������������ͳ��

    //=========================Signal Message==============================
    // ������Ϣ
    SignalMsgStat m_signalMsgStat;

    //=========================Media Buffer==============================
    MEDIA_BUFFER_STAT_VECTOR m_mediaBufferStatVector;
    float m_fMediaBuffMaxLevel; // ý�建���������ˮλ

    uint32_t m_fMediaBuffAveLevel;   // Cacheƽ��ˮλ
    //=========================Disk Info==============================
    DISK_STAT_VECTOR m_diskStatVector;
};

// ¼��֪ͨ����Ϣͳ��
class SVS_Inform_Stat
{
public:
    SVS_Inform_Stat();
    virtual ~SVS_Inform_Stat();
    void init(const char* strCameraId);
    void updateStrategyTime(time_t startTime, time_t endTime, int32_t strategyType, bool isEventEffective);
private:
    char m_strCameraId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
public:
    time_t m_strategyStartTime;
    time_t m_strategyEndTime;
    bool m_isEventEffective;
    int32_t m_strategyType ;
    time_t m_takeoverTime; // ��ͷ�ӹ�ʱ��
};

// ¼������ͳ����Ϣ
class SVS_Device_Stat
{
public:
    SVS_Device_Stat();
    virtual ~SVS_Device_Stat();
    void init(const char* strCameraId);

    // �����¼���Ϣ
    void updateEventInfo(time_t startTime,time_t endTime, uint32_t eventTotalNum);

    // ¼����ʼ�¼�
    void setRecordStartTime(time_t recordStartTime);

    // ����¼����Ϣ
    void updateRecordInfo(int32_t nRecordStrategyType, int32_t nRecordStatus, uint32_t nStreamId);

    // ����¼��������ʼʱ��
    void setRecordTaskStartTime(time_t ulRecordTaskStartTime);
private:
    char m_strCameraId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
public:
    time_t m_recordStartTime;
    time_t m_eventHappenTime;
    time_t m_eventOverTime;
    uint32_t m_eventTotalNum;
    int32_t m_recordStrategyType; // ¼���������
    int32_t m_recordStatus;       // ¼��״̬
    int32_t m_recordTaskStartTime; // ¼������ʼʱ��
    uint32_t m_streamId;
    int32_t m_startRecvStreamTimes; // ��ʼ��������
};


class SVS_Ingress_Stat
{
public:
    SVS_Ingress_Stat();
    virtual ~SVS_Ingress_Stat();
    void init(const char* strCameraId);
    void updateLastRecvPacketTime(time_t lastTime);
    void addRecvByteNum(uint32_t newRecvTyteNum);
    void addRecvFramNum(uint32_t newRecvFrameNum, uint8_t keyFrame = 0);
    void addLostFramNum(uint32_t newLostFramNum);
    void addErrFrameNum(uint32_t newErrFramNum);
    void addLostPacketNum(uint32_t newLostPacketNum);
    void updateFrameSize(uint32_t newFrameSize);

    // ���ñ��������ڵĲ�����Ϣ
    void resetSampleInfo();
private:
    char m_strCameraId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

public:
    time_t m_lastRecvPachetTime;
    uint32_t m_recvByteNum;
    uint32_t m_recvFrameNum;
    uint32_t m_lostFrameNum;
    uint32_t m_lostPacketNum;
    uint32_t m_errFrameNum;

    uint32_t m_sampleRecvByteNum;
    uint32_t m_sampleRecvFrameNum;
    uint32_t m_sampleRecvIFrameNum;
    uint32_t m_sampleLostFrameNum;
    uint32_t m_sampleMaxFrameSize;
    uint32_t m_sampleMinFrameSize;
};

// �������Ϣ
class SVS_Egress_Stat
{
public:
    SVS_Egress_Stat();
    virtual ~SVS_Egress_Stat();
    void init(const char* strCameraId);
    void addWriteByteNum(uint32_t newWriteByteNum);
    void addWriteFrameNum(uint32_t newWriteFrameNum);
    void addWriteIFrameNum(uint32_t newWriteIFrameNum);
    void updateIndexInfo(time_t startTime, time_t endTime);
    void updateFileName(const char* strFileName);
    void updateRecordFileSize(uint64_t newRecordFileSize);
    // ���ñ��������ڵĲ�����Ϣ
    void resetSampleInfo();
private:
    char m_strCameraId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
public:
    char m_fileName[MAXNAMELEN + 1];
    time_t m_indexStartTime;
    time_t m_indexEndTime;
    uint32_t m_writeByteNum;
    uint32_t m_writeFrameNum;

    uint32_t m_sampleRecordSize;
    uint32_t m_sampleRecordFrameNum;
    uint32_t m_sampleRecordIFrameNum;
    uint64_t m_sampleRecordFileSize;
};

// ý�����ͳ��
class SVS_Media_Output_Stat
{
public:
    SVS_Media_Output_Stat(uint32_t threadIndex );
    ~SVS_Media_Output_Stat();
    void addIoScheduleTimes(uint32_t IoScheduleTimes,
                                   uint32_t newWriteByte,
                                   uint32_t newWriteFrameNum);
    void addIoScheduleFailTimes(uint32_t newIoFailScheduleTimes);
private:
    SVS_Media_Output_Stat();
private:
    uint32_t m_threadIndex;

public:
    uint32_t m_ulIoScheduleTimes;
    uint64_t m_writeByteNum;
    uint64_t m_writeFrameNum;
    uint32_t m_IoScheduleFailTimes;
};

// ��Ϣ��Ϣͳ��
class SVS_Signal_Message_Stat
{
public:
    SVS_Signal_Message_Stat();
    ~SVS_Signal_Message_Stat();
    void addSignalMessage(uint32_t messageCount);
    void addDisconnStoreCtrlTimes(uint32_t newDisconnTimes);
    void addConnStoreCtrlFailTimes(uint32_t newConnFailTimes);
    void addHeartbeatTimeoutTimes(uint32_t newTimeoutTimes);
    void updateConnStoreCtrlTime(time_t lastConntime);
    void updateDisconnStoreCtrlTime(time_t lastDisconntime);
public:
    uint32_t m_messageCount;
    uint32_t m_disconnStoreCtrlTimes;
    uint32_t m_connStoreCtrlFailTimes;
    uint32_t m_hearbeatTimeoutTimes;
    time_t   m_lastDisconnStoreCtrlTime;
    time_t   m_lastConnStoreCtrlTime;
};

// ý�建����ʹ��ͳ��
class SVS_Media_Buffer_Stat
{
public:
    SVS_Media_Buffer_Stat();
    ~SVS_Media_Buffer_Stat();
    void init(uint32_t BufferLen, uint32_t BufferNum );
    void addAllocaTimes(uint32_t newAllocTimes);
    void addFreeTimes(uint32_t newFreeTimes);
    void addAllcoaFailTimes(uint32_t newAllocFailTimes);
    void addAlloctTmpBuffFailTimes(uint32_t newAllocFailTimes);
public:
    volatile uint32_t m_allocTimes;
    volatile uint32_t m_FreeTimes;
    volatile uint32_t m_allocFailTimes;
    volatile uint32_t m_allocTmpBuffFailTimes;
    ACE_Recursive_Thread_Mutex m_mutex;       //�����������б���
};

class SVS_Disk_Stat
{
public:
    SVS_Disk_Stat();
    ~SVS_Disk_Stat();
    void updateDiskSize(uint64_t totalSize, uint64_t usableSize);
public:
    uint64_t m_totalSize; //KB
    uint64_t m_usableSize; // KB
};

#endif /* __SVS_STAT_MANAGER_H__ */

