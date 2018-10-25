/******************************************************************************

                  版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文 件 名   : SvsSysStat.h
  版 本 号   : 初稿
  生成日期   : 2011年01月23日
  最近修改   :
  功能描述   : 实现对系统信息的采集，包括CPU、内存、网卡和系统磁盘；
               单例:因为需要保存上个周期需要采样的数据，且系统只需要存在一个对象，
               所以用单例;
               有自己的线程，保存3个周期的数据，为外层提供3个周期的平均值。
  函数列表   :
  修改历史   :
  1.日    期   : 2011年01月23日
    修改内容   : 创建文件

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


// 统计周期的个数
#define SVS_STAT_INTERVAL_NUM   3

// 默认的统计周期，单位:s
#define SVS_STAT_DEFAULT_INTERVAL   5

// 网卡信息定义
#define SVS_STAT_NETCARD_INFO_LEN     64
#define MAX_NETCARD_NUM  16

// 路径长度
#define STAT_PATH_LEN 128

// 网卡带宽默认大小,单位Mb
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

    // 初始化
    int32_t open(void *);

    // 统计线程
    int32_t svc(void);

    // 关闭对象
    int32_t close(u_long);

public:// change stat objects
    // 添加一个要统计的网卡
    int32_t AddNetwordCard(const char* strIP);

    // 删除一个要统计的网卡
    int32_t RemoveNetwordCard(const char* strIP);

    // 添加一个要统计的磁盘
    int32_t AddDisk(const char* strDiskPath);

    // 删除一个要统计的磁盘
    int32_t RemoveDisk(const char* strDiskPath);

public:// get stat info
    // 获取cpu信息，单位百分比
    void GetCpuInfo(uint32_t &ulUsedPer)const;

    // 获取内存信息，单位 : KB
    void GetMemoryInfo(uint32_t& ulTotalSize, uint32_t& ulUsedSize)const;

    // 获取网卡信息,单位 : Mbps
    int32_t GetNetworkCardInfo(const char* strIP, uint32_t& ulTotalSize,
                       uint32_t& ulUsedRecvSize, uint32_t& ulUsedSendSize);

    // 获取磁盘信息，单位: B
    int32_t GetDiskInfo(const char* strDiskPath, uint64_t& ullTotalSize, uint64_t& ullUsedSize);

private:// struct / macro / enum
    // 系统信息
    typedef struct tagSysInfo
    {
        // cpu信息
        uint32_t    m_ulCpuUsed[SVS_STAT_INTERVAL_NUM + 1];

        // 内存信息
        uint32_t    m_ulMemTotal; // MB
        uint32_t    m_ulMemUsed[SVS_STAT_INTERVAL_NUM + 1];  // MB

        void reset()
        {
            memset(m_ulCpuUsed, 0, sizeof(m_ulCpuUsed));
            m_ulMemTotal = 0;
            memset(m_ulMemUsed, 0, sizeof(m_ulMemUsed));
        }
    }SysInfo;

    // 网卡信息
    typedef struct tagNetworkCardInfo
    {
        char m_strIP[IP_STRING_LEN + 1];// 网卡IP
        char m_strName[SVS_STAT_NETCARD_INFO_LEN + 1];// 网卡mingc
        uint32_t m_ulBWTotal;// 网卡总带宽，单位mbps
        NET_FLUX m_ullCurrTxByteRecv;// 网卡当前收到的字节数，用于网卡带宽统计
        NET_FLUX m_ullCurrTxByteSend;// 网卡当前收到的字节数，用于网卡带宽统计

        // 网卡已使用带宽,N个周期的平均值放在第N+1的位置，单位kbps
        uint32_t m_ulBWUsedRecv[SVS_STAT_INTERVAL_NUM + 1];
        uint32_t m_ulBWUsedSend[SVS_STAT_INTERVAL_NUM + 1];

        // 初始化
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
        DISK_LOCAL_PATH = 0, // 本地路径
        DISK_MOUNT      = 1 // mount磁盘
    }DISK_MOUNT_FLAG;
    // 磁盘信息
    typedef struct tagDiskInfo
    {
        char m_strDiskName[STAT_PATH_LEN + 1];// 磁盘路径
        uint64_t m_ullTotalSize;// 磁盘总大小，单位:B
        uint64_t m_ullUsedSize;// 已经使用的大小
        int32_t m_nMountFlag;// DISK_MOUNT_FLAG

        // 初始化
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
    // 系统信息
    SysInfo m_stSysInfo;

    // 网卡列表
    std::list<NetworkCardInfo> m_networkCardInfoList;

    // 磁盘列表
    std::list<DiskInfo> m_diskInfoList;

    // 当前正在统计的下标值
    volatile uint32_t m_ulStatIndex;

    // 运行标识
    bool m_bRunFlag;

    // 系统锁
    ACE_Recursive_Thread_Mutex m_sysMutex;

    // 存放mounts信息,每个磁盘长度128,raid 64个，每个raid组32个磁盘
    char m_strMountsInfo[STAT_PATH_LEN * 32 * 64];

    // 获取mount文件是否OK
    volatile bool m_bIsMountsOk;
};
#endif // _SVS_SYS_STAT_H__

