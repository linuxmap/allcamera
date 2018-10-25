/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : SvsSysStat.h
  �� �� ��   : ����
  ��������   : 2011��01��23��
  ����޸�   :
  ��������   : ʵ�ֶ�ϵͳ��Ϣ�Ĳɼ�������CPU���ڴ桢������ϵͳ���̣�
               ����:��Ϊ��Ҫ�����ϸ�������Ҫ���������ݣ���ϵͳֻ��Ҫ����һ������
               �����õ���;
               ���Լ����̣߳�����3�����ڵ����ݣ�Ϊ����ṩ3�����ڵ�ƽ��ֵ��
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2011��01��23��
    �޸�����   : �����ļ�

******************************************************************************/
#ifndef _SVS_SYS_STAT_H__
#define _SVS_SYS_STAT_H__
#include "svs_ace_header.h"
#include "svs_adapter_svs_def.h"
#include <list>
using std::list;

#ifdef OS_SUSE9
#define NET_FLUX uint32_t
#else
#define NET_FLUX uint64_t
#endif


#define SVS_STAT_OK 0
#define SVS_STAT_FAILTRUE -1


// ͳ�����ڵĸ���
#define SVS_STAT_INTERVAL_NUM   3

// Ĭ�ϵ�ͳ�����ڣ���λ:s
#define SVS_STAT_DEFAULT_INTERVAL   5

// ������Ϣ����
#define SVS_STAT_NETCARD_INFO_LEN     64
#define MAX_NETCARD_NUM  16

// ·������
#define STAT_PATH_LEN 128

// ��������Ĭ�ϴ�С,��λMb
#define SVS_BANDWIDTH_DEFAULT        1000
#define ABNOMAL_VALUE 0xFFFF
#define BOND_MODE_ACTIVEBACKUP 1
#define BOND_STATE_ACTIVE 0   /* link is active */

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
    uint32_t bond_mode;
    uint32_t num_slaves;
    uint32_t miimon;
} ifbond_t;

typedef struct
{
    uint32_t slave_id; /* Used as an IN param to the BOND_SLAVE_INFO_QUERY ioctl */
    char  slave_name[IFNAMSIZ];
    char  link;
    char  state;
    uint32_t link_failure_count;
} ifslave_t;


typedef struct
{
    uint32_t cmd;
    uint32_t supported;            /* Features this interface supports */
    uint32_t advertising;          /* Features this interface advertises */
    uint16_t speed;                /* The forced speed, 10Mb, 100Mb, gigabit */
    uint8_t  duplex;                  /* Duplex, half or full */
    uint8_t  port;                    /* Which connector port */
    uint8_t  phy_address;
    uint8_t  transceiver;             /* Which tranceiver to use */
    uint8_t  autoneg;                 /* Enable or disable autonegotiation */
    uint32_t maxtxpkt;             /* Tx pkts before generating tx int32_t */
    uint32_t maxrxpkt;             /* Rx pkts before generating rx int32_t */
    uint32_t reserved[4];
} __attribute__ ((packed))          ethtool_cmd_t;
#define ETHTOOL_GSET    0x00000001 /* Get settings. */

class CSvsSysStat : public ACE_Task<ACE_MT_SYNCH>
{
public:
    static CSvsSysStat &instance()
    {
        static CSvsSysStat SystemInfoStat;
        return SystemInfoStat;
    };

    virtual ~CSvsSysStat();

    // ��ʼ��
    int32_t open(void *);

    // ͳ���߳�
    int32_t svc(void);

    // �رն���
    int32_t close(u_long);

public:// change stat objects
    // ���һ��Ҫͳ�Ƶ�����
    int32_t AddNetwordCard(const char* strIP);

    // ɾ��һ��Ҫͳ�Ƶ�����
    int32_t RemoveNetwordCard(const char* strIP);

    // ���һ��Ҫͳ�ƵĴ���
    int32_t AddDisk(const char* strDiskPath);

    // ɾ��һ��Ҫͳ�ƵĴ���
    int32_t RemoveDisk(const char* strDiskPath);

public:// get stat info
    // ��ȡcpu��Ϣ����λ�ٷֱ�
    void GetCpuInfo(uint32_t &ulUsedPer)const;

    // ��ȡ�ڴ���Ϣ����λ : KB
    void GetMemoryInfo(uint32_t& ulTotalSize, uint32_t& ulUsedSize)const;

    // ��ȡ������Ϣ,��λ : Mbps
    int32_t GetNetworkCardInfo(const char* strIP, uint32_t& ulTotalSize,
                       uint32_t& ulUsedRecvSize, uint32_t& ulUsedSendSize);

    // ��ȡ������Ϣ����λ: B
    int32_t GetDiskInfo(const char* strDiskPath, uint64_t& ullTotalSize, uint64_t& ullUsedSize);

private:// struct / macro / enum
    // ϵͳ��Ϣ
    typedef struct tagSysInfo
    {
        // cpu��Ϣ
        uint32_t    m_ulCpuUsed[SVS_STAT_INTERVAL_NUM + 1];

        // �ڴ���Ϣ
        uint32_t    m_ulMemTotal; // MB
        uint32_t    m_ulMemUsed[SVS_STAT_INTERVAL_NUM + 1];  // MB

        void reset()
        {
            memset(m_ulCpuUsed, 0, sizeof(m_ulCpuUsed));
            m_ulMemTotal = 0;
            memset(m_ulMemUsed, 0, sizeof(m_ulMemUsed));
        }
    }SysInfo;

    // ������Ϣ
    typedef struct tagNetworkCardInfo
    {
        char m_strIP[IP_STRING_LEN + 1];// ����IP
        char m_strName[SVS_STAT_NETCARD_INFO_LEN + 1];// ����mingc
        uint32_t m_ulBWTotal;// �����ܴ�����λmbps
        NET_FLUX m_ullCurrTxByteRecv;// ������ǰ�յ����ֽ�����������������ͳ��
        NET_FLUX m_ullCurrTxByteSend;// ������ǰ�յ����ֽ�����������������ͳ��

        // ������ʹ�ô���,N�����ڵ�ƽ��ֵ���ڵ�N+1��λ�ã���λkbps
        uint32_t m_ulBWUsedRecv[SVS_STAT_INTERVAL_NUM + 1];
        uint32_t m_ulBWUsedSend[SVS_STAT_INTERVAL_NUM + 1];

        // ��ʼ��
        void Reset()
        {
            memset(m_strIP, 0, sizeof(m_strIP));
            memset(m_strName, 0, sizeof(m_strName));
            m_ulBWTotal = 0;
            memset(m_ulBWUsedRecv, 0, sizeof(m_ulBWUsedRecv));
            memset(m_ulBWUsedSend, 0, sizeof(m_ulBWUsedSend));
            m_ullCurrTxByteRecv = 0;
            m_ullCurrTxByteSend = 0;
        }

    }NetworkCardInfo;

    typedef enum
    {
        DISK_LOCAL_PATH = 0, // ����·��
        DISK_MOUNT      = 1 // mount����
    }DISK_MOUNT_FLAG;
    // ������Ϣ
    typedef struct tagDiskInfo
    {
        char m_strDiskName[STAT_PATH_LEN + 1];// ����·��
        uint64_t m_ullTotalSize;// �����ܴ�С����λ:B
        uint64_t m_ullUsedSize;// �Ѿ�ʹ�õĴ�С
        int32_t m_nMountFlag;// DISK_MOUNT_FLAG

        // ��ʼ��
        void Reset()
        {
            memset(m_strDiskName, 0, sizeof(m_strDiskName));
            m_ullTotalSize = 0;
            m_ullUsedSize = 0;
            m_nMountFlag = DISK_LOCAL_PATH;
        }
    }DiskInfo;

private:
    CSvsSysStat();

    int32_t StatCpuInfo();

    int32_t StatMemoryInfo();

    int32_t StatDiskInfo();

    int32_t StatNetworkCardInfo();

    void ReadMountFile();

    int32_t CheckDiskMountState(const char * strDiskPath)const;

    void StatNetCardInfo(NetworkCardInfo* pNetworkCard)const;

    int32_t StatBondNetcardInfo(int32_t isock, NetworkCardInfo* pNetworkCard, struct ifreq* pIfr )const;

    int32_t StatEthNetcardInfo(int32_t isock, NetworkCardInfo* pNetworkCard, struct ifreq* pIfr )const;

    void StatBWInfo(NetworkCardInfo* pNetworkCard)const;
private:
    // ϵͳ��Ϣ
    SysInfo m_stSysInfo;

    // �����б�
    std::list<NetworkCardInfo> m_networkCardInfoList;

    // �����б�
    std::list<DiskInfo> m_diskInfoList;

    // ��ǰ����ͳ�Ƶ��±�ֵ
    volatile uint32_t m_ulStatIndex;

    // ���б�ʶ
    bool m_bRunFlag;

    // ϵͳ��
    ACE_Recursive_Thread_Mutex m_sysMutex;

    // ���mounts��Ϣ,ÿ�����̳���128,raid 64����ÿ��raid��32������
    char m_strMountsInfo[STAT_PATH_LEN * 32 * 64];

    // ��ȡmount�ļ��Ƿ�OK
    volatile bool m_bIsMountsOk;
};
#endif // _SVS_SYS_STAT_H__

