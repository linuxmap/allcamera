#ifndef __STREAMCONFIG_H__
#define __STREAMCONFIG_H__

#include <list>
#include "svs_ace_header.h"
#include "svs_adapter_util_config.h"
#include <vms/vms.h>
#include "svs_log_msg.h"

#define STREAM_MAX_BUF_LEN               1024          /// �����ļ���һ�����õ���󳤶�
#define STREAM_IP_ADDR_LEN               24            /// IP��ַ�ַ���󳤶�
#define STREAM_SERVICE_NET_TYPE_DEFAULT  3             /// ��������Ĭ��֧�̶ֹ����ƶ�
#define MAX_SERVICE_PORT_NUM          8             /// STREAM���֧��8��ҵ��˿�
#define MAX_MEDIA_PORT_RANGE_NUM      8             /// STREAM���ý�������֧��8���˿ڶ�
#define MAX_MEDIA_IP_NUM              4             /// STREAM���֧��4��ý��������
#define MAX_CONFIG_FILE_NAME_LEN      1024

#define DEFAULT_SCC_CONNECT_PORT      8020
#define DEFAULT_RECORD_MEDIA_PORT     8070
#define DEFAULT_RTSP_SERVER_PORT      554
#define DEFAULT_HLS_SERVER_PORT       80
#define DEFAULT_SVS_PORT_SWITCH       0
#define DEFAULT_TCP_RECV_MEDIA_PORT   6001
#define DEFAULT_TCP_SEND_MEDIA_PORT   6002
#define DEFAULT_STREAM_DEBUG_PORT        9010

// ��������ʱʱ������λ:��
enum _enSTREAM_RTSP_ANTIHOTLINKING_TIME_LEN
{
    STREAM_RTSP_ANTIHOTLINKING_TIME_LEN_DEFAULT = 30,
    STREAM_RTSP_ANTIHOTLINKING_TIME_LEN_MAX     = 300,
    STREAM_RTSP_ANTIHOTLINKING_TIME_LEN_MIN     = 30
};

/// STREAMת������
enum _enSTREAM_SERVICE_CAPACITY
{
    STREAM_SERVICE_CAPACITY_DEFAULT  = 100,            /// Ĭ��֧��100·�ַ�
    STREAM_SERVICE_CAPACITY_MAX      = 2000            /// ���֧��2000·�ַ�
};

/// STREAMͳ���ϱ�ʱ��
enum _enSTREAM_REPORT_INTERVAL
{
    STREAM_REPORT_INTERVAL_DEFAULT = 10,
    STREAM_REPORT_INTERVAL_MAX     = 60,
    STREAM_REPORT_INTERVAL_MIN     = 5
};

/// STREAM�Ʒ�ʱ��
enum _enSTREAM_ACCOUNT_INTERVAL
{
    STREAM_ACCOUNT_INTERVAL_DEFAULT = 300,
    STREAM_ACCOUNT_INTERVAL_MAX     = 300,
    STREAM_ACCOUNT_INTERVAL_MIN     = 30
};

enum _enSTREAM_SOURCE_LIMIT
{
    STREAM_SOURCE_LIMIT_DEFAULT    = 80,
    STREAM_SOURCE_LIMIT_MAX        = 100,
    STREAM_SOURCE_LIMIT_MIN        = 0
};



typedef struct _PortRange
{
    uint16_t usStartPort;
    uint16_t usEndPort;
} PortRange;


typedef struct _MediaPortConfig
{
    uint16_t       usPortRangeNum;
    PortRange      stPortRange[MAX_MEDIA_PORT_RANGE_NUM];
} MediaPortConfig;


typedef std::list<uint32_t>   STREAM_IP_LIST;

class CStreamConfig : public ACE_Event_Handler
{
public:
    CStreamConfig();
    virtual ~CStreamConfig();
    static CStreamConfig *instance()
    {
        static CStreamConfig streamConfig;
        return &streamConfig;
    }

    const char* getDecodeTag() const;

    int32_t init(const char * pCfgFile);

    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);

    int32_t startReloadTimer();

    void stopReloadTimer();

    const char* getServiceId() const;

    const char* getServiceName() const;

    const char* GetRegisterPasswd()const;

    uint32_t getServiceNetType() const;

    uint32_t getServiceCapacity() const;

    uint32_t getServiceIp() const;

    void getServicePortList(uint16_t *&pusServicePortList,
                           uint32_t &unArrayItemNum);

    uint32_t getInternalMediaIp() const;

    void getExternalMediaIpList(STREAM_IP_LIST &ipList) const;

    uint32_t getInternalReportIp() const;

    void getExternalReportIpList(STREAM_IP_LIST &ipList) const;

    void getUdpMediaPortConfig(MediaPortConfig* &pUdpConfig);

    uint16_t getEnableEhome() const;

    uint16_t getEhomeTransType() const;

    void getEhomeMediaPortConfig(MediaPortConfig* &pEhomeConfig);

    uint16_t getTcpMediaPort()const;

    uint16_t getRtspServerPort() const;

    uint16_t getRtmpServerPort() const;

    uint16_t getHLSServerPort() const;

    uint16_t getRecordMediaPort() const;

    uint32_t getAccountPeriod() const;

    uint32_t getReportPeriod() const;

    uint32_t getSccIp() const;

    uint16_t getSccPort() const;

    uint32_t  getSccHeartbeatInterval() const;

    uint16_t getDebugPort() const;

    uint32_t getDebugCheckThreadInterval() const;

    uint32_t getDebugRestartServerFlag() const;

    uint32_t getDebugDumpServerFlag() const;

    uint32_t getUrlEffectiveWhile()const;

    uint32_t getDistributeLimitPerChannel()const;

    uint32_t getCpuLimit()const;
    uint32_t getMemoryLimit()const;
    uint32_t getStorageLimit()const;

    int32_t set(const char* sectionName, const char* itemName, const char* keyValue)const;

private:
    int32_t readStreamConfigure(CSVS_Config &sysConfig);
    int32_t readMediaConfigure(CSVS_Config &sysConfig);
    int32_t readSccConfigure();
    int32_t readDebugConfigure(CSVS_Config &sysConfig);

    int32_t readItemServiceId(CSVS_Config &sysConfig);
    int32_t readItemServiceName(CSVS_Config &sysConfig);
    int32_t readItemServiceNetType(CSVS_Config &sysConfig);
    int32_t readItemServicePortList(CSVS_Config &sysConfig);

    int32_t readItemMediaPortConfig(CSVS_Config &sysConfig);
    int32_t readItemEhomeMediaPortConfig(CSVS_Config &sysConfig);


    int32_t readIpValue
    (
        const char*   section,
        const char*   key,
        uint32_t& value
    )const;

    int32_t readIpList
    (
        const char*   section,
        const char*   key,
        STREAM_IP_LIST&  ipList
    )const;

    // OMC
    int32_t readIntValue
    (
        const char*   section,
        const char*   key,
        uint32_t& value,
        uint32_t  maxValue,
        uint32_t  minValue,
        uint32_t  defaultValue
    )const;
private:
    // about check
    int32_t checkPortRange(uint16_t usPortRangeNum, const PortRange *pstPortRange) const;
    int32_t checkDeviceID(const char *pszDevID) const;
    int32_t parsePortList(const char* pPortbuf, MediaPortConfig &config) const;
    void checkMediaIp(const std::string &strIp, STREAM_IP_LIST& ipList)const;

    static CStreamConfig* m_pStreamConfig;

    char           m_szStreamID[SERVER_ID_LEN + 1];              // STREAM ID
    char           m_szStreamName[SERVER_NAME_LEN + 1];          // STREAM Name
    uint32_t       m_unServiceNetType;                        // ServiceNetType
    uint32_t       m_unServiceCapacity;                        // ServiceCapacity
    uint32_t       m_unServiceIp;                              // ServiceIp
    uint16_t       m_usServicePortList[MAX_SERVICE_PORT_NUM];
    uint32_t       m_unServicePortListNum;
    uint32_t       m_unAccountPeriod;
    uint32_t       m_unReportPeriod;


    uint32_t       m_unSccIp;
    uint16_t       m_usSccPort;
    uint32_t       m_unSccHeartbeat;


    uint32_t       m_unInternalMediaIp;
    STREAM_IP_LIST    m_ExternalMediaIpList;
    uint32_t       m_unInternalReportIp;
    STREAM_IP_LIST    m_ExternalReportIpList;
    uint16_t       m_usRecordMediaPort;
    uint16_t       m_usRtspServerPort;
    uint16_t       m_usRtmpServerPort;
    uint16_t       m_usHlsServerPort;
    uint16_t       m_usTcpMediaPort;
    MediaPortConfig m_stUdpMediaPort;
    uint16_t        m_usEnableEhome;
    uint16_t        m_usEhomeTransType;
    MediaPortConfig m_stEhomeMediaPort;

    uint16_t       m_usDebugPort;
    uint32_t       m_unCheckThreadInterval;
    uint32_t       m_unRestartServerFlag;
    uint32_t       m_unDumpServerFlag;

    char           m_FileName[MAX_CONFIG_FILE_NAME_LEN];
    time_t         m_LastModifyTime;

    std::string    m_strDecodeTag;

    uint32_t       m_ulUrlEffectiveWhile;

    char           m_strRegisterPasswd[AUTH_PASSWD_LEN + 1];

    uint32_t       m_DistributeLimitPerChannel;

};

#endif /* __STREAMCONFIG_H__ */

