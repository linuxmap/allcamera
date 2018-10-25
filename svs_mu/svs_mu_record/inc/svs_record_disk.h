/******************************************************************************

                  版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文 件 名   : SVS_Record_Disk.h
  版 本 号   : 初稿
  生成日期   : 2009年8月21日
  最近修改   :
  功能描述   : raid组和磁盘类。Raid组之间负载均衡，raid组内的lun之间轮循写，即先
               从0,写到n,再从n到0
  函数列表   :
  修改历史   :
  1.日    期   : 2009年8月21日
    修改内容   : 修改

******************************************************************************/

#ifndef _SVS_Record_Disk_h
#define _SVS_Record_Disk_h
#include <string>
#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <sys/statvfs.h>
#include "vms/vms.h"
#include "svs_private_msg.h"
#include "svs_log_msg.h"
#include "svs_rt_record_common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct xfs_fsop_geom {
    uint32_t       blocksize;  /* filesystem (data) block size */
    uint32_t       rtextsize;  /* realtime extent size     */
    uint32_t       agblocks;   /* fsblocks in an AG        */
    uint32_t       agcount;    /* number of allocation groups  */
    uint32_t       logblocks;  /* fsblocks in the log      */
    uint32_t       sectsize;   /* (data) sector size, bytes    */
    uint32_t       inodesize;  /* inode size in bytes      */
    uint32_t       imaxpct;    /* max allowed inode space(%)   */
    uint64_t       datablocks; /* fsblocks in data subvolume   */
    uint64_t       rtblocks;   /* fsblocks in realtime subvol  */
    uint64_t       rtextents;  /* rt extents in realtime subvol*/
    uint64_t       logstart;   /* starting fsblock of the log  */
    uint8_t        uuid[16];   /* unique id of the filesystem  */
    uint32_t       sunit;      /* stripe unit, fsblocks    */
    uint32_t       swidth;     /* stripe width, fsblocks   */
    int32_t        version;    /* structure version        */
    uint32_t       flags;      /* superblock version flags */
    uint32_t       logsectsize;    /* log sector size, bytes   */
    uint32_t       rtsectsize; /* realtime sector size, bytes  */
    uint32_t       dirblocksize;   /* directory block size, bytes  */
    uint32_t       logsunit;   /* log stripe unit, bytes */
} xfs_fsop_geom_t;

#define XFS_IOC_FSGEOMETRY       _IOR ('X', 124, struct xfs_fsop_geom)
typedef struct xfs_fsop_counts {
    uint64_t   freedata;   /* free data section blocks */
    uint64_t   freertx;    /* free rt extents */
    uint64_t   freeino;    /* free inodes */
    uint64_t   allocino;   /* total allocated inodes */
} xfs_fsop_counts_t;

#define XFS_IOC_FSCOUNTS         _IOR ('X', 113, struct xfs_fsop_counts)

#define MAX_MD 5


enum DISK_CONST
{
    //磁盘分区名最大长度(含空结束符)
    DISK_NAME_SIZE = 128,

    //磁盘可使用空间下限
    DISK_LOW_LIMIT = (8 * 1024 * 1024),
};

// 磁盘类型
enum SVS_DISK_TYPE
{
    SVS_DISK_TYPE_MOUNTDISK = 0,
    SVS_DISK_TYPE_LOCALDISK = 1,
    SVS_DISK_TYPE_MAX
};

/*****************************************************************************
   磁盘错误码定义
 *****************************************************************************/
enum SVS_DISK_ERR_TYPE
{
    SVS_DISK_ERR_OK                 = 0x00, /*磁盘正常*/
    SVS_DISK_ERR_DISK_ALARM_RESUME,         /*磁盘进入告警预警范围*/
    SVS_DISK_ERR_RAID_ALARM_RESUME,         /*Raid组进入告警预警范围*/
    SVS_DISK_ERR_DISK_ALARM,                /*磁盘告警*/
    SVS_DISK_ERR_RAID_ALARM,                /*Raid组告警*/
    SVS_DISK_ERR_DISK_FULL,                 /*磁盘满*/
    SVS_DISK_ERR_RAID_FULL,                 /*Raid组满*/
    SVS_DISK_ERR_DISK_BAD,                  /*磁盘坏*/
    SVS_DISK_ERR_DISK_MOUNT_OFFLINE,         /*磁盘mount掉线*/
//    SVS_DISK_ERR_RAID_BAD,                /*Raid坏*/
    SVS_DISK_ERR_DISK_FS_INVALID,           /*磁盘文件系统不可用*/
//    SVS_DISK_ERR_RAID_FS_INVALID,         /*Raid组文件系统不可用*/
    SVS_DISK_ERR_DISK_FS_READONLY,          /*磁盘文件系统只读*/
//    SVS_DISK_ERR_RAID_FS_READONLY,        /*Raid组文件系统只读*/
    SVS_DISK_ERR_DISK_OFFLINE_CONF,         /*磁盘不再配置列表，下线*/
    SVS_DISK_ERR_RAID_NO_OUTPUT_THREAD,     /*raid组没有可用的输出线程*/
    SVS_DISK_ERR_TYPE_MAX
};

typedef struct RAID_INFO_
{
    int32_t     RaidIndex;
    int32_t     RaidLevel;
}RAID_INFO;

// 磁盘信息结构体
typedef struct DISK_INFO_
{
    char        DiskPath[DISK_NAME_SIZE];// 磁盘路径
    bool        MedaDataPartedFlag;// 元数据分离标识
    int32_t         DiskMountTypeFlag;// 磁盘是否是mount上来的磁盘
    RAID_INFO   RaidInfo;// 磁盘所属raid组属性
}DISK_INFO;

typedef vector<DISK_INFO>DISK_INFO_VECTOR;

// 磁盘门限值结构体定义
typedef struct DISK_THRESHOLD_
{
    //磁盘可用空间下限，单位KB
    uint32_t m_lowerLimit;

    // 磁盘告警阈值，单位KB
    uint32_t m_alarmLimit;

    // 磁盘告警恢复值，单位KB
    uint32_t m_alarmResumeLimit;

    DISK_THRESHOLD_& operator=(const DISK_THRESHOLD_ &T)
    {
        if(this == &T)
        {
            return *this;
        }
        m_lowerLimit = T.m_lowerLimit;
        m_alarmLimit = T.m_alarmLimit;
        m_alarmResumeLimit = T.m_alarmResumeLimit;
        return *this;
    }
}DISK_THRESHOLD;

/**
 * @class CAC_Record_Disk
 * @brief 录像磁盘分区类
 */
class CAC_Record_Disk
{
private:
    CAC_Record_Disk();
public:

    /**
     * 构建磁盘分区<strPath>, 分配索引<index>
     */
    CAC_Record_Disk(const char *strPath, size_t nIndex, const DISK_THRESHOLD &stDiskThreshold);

    virtual ~CAC_Record_Disk();

    const char *path() const
    {
        return this->str_path_;
    }

    const int32_t path_len() const
    {
        return this->path_len_;
    }

    const int32_t flag() const
    {
        return this->flags_;
    }

    const int32_t type() const
    {
        return disk_mount_flag_;
    }

    const uint64_t total_size()const
    {
        return total_size_;
    }

    const uint64_t free_size()const
    {
        return free_size_;
    }

    virtual int32_t statistics();

    int32_t check_disk();

    // 打开元数据分离的磁盘
    int32_t open_realtime_disk();

    // 打开元数据不分离的磁盘
    int32_t open_normal_disk();

    // 元数据和视频数据是否分离标识
    const bool getMedaDataPartedFlag()const
    {
        return meda_data_parted_flag_;
    }

    // 设置磁盘属性

    int32_t setDiskFlag(const int32_t newDiskFlag);
    int32_t setMedaDataPartedFlag(bool medaDataPartedFlag);

    int32_t setMountFlag(const int32_t mountFlag);
private:
    int32_t converse_disk_flag();
protected:
    // 分区的索引
    volatile const size_t index_;

    //磁盘剩余总大小，单位: KB
    volatile uint64_t total_size_;

    // 磁盘已经使用大小，单位: KB
    volatile uint64_t used_size_;

    // 磁盘已经剩余空间大小，单位: KB
    volatile uint64_t free_size_;

    //分区名称
    char str_path_[DISK_NAME_SIZE];

    //分区名称长度
    int32_t path_len_;

    //磁盘满b_full_将被设置为true, 否则为false.
    int32_t flags_;

    ACE_Thread_Mutex mutex_;

    // 磁盘类型，mount上来的磁盘或本地路径
    int32_t disk_mount_flag_;

    // 磁盘阈值
    DISK_THRESHOLD disk_threshold_;

    // 磁盘统计类
    SVS_Disk_Stat disk_stat_;

    // 元数据和视频数据分离标识，默认是未分离的
    bool meda_data_parted_flag_;
};

/**
 * @class CAC_Record_Raid
 * @brief 磁盘阵列类
 */
class CAC_Record_Raid
{
public:
    CAC_Record_Raid();
    virtual ~CAC_Record_Raid();

    /**
     * 初始化磁盘阵列
     * 磁盘空间检测线程每@a nInterval秒检查一次磁盘空间，如果磁盘分区的可用
     * 空间小于@a lowerLimit兆，那么标记该磁盘分区为不可用。
     */
    int32_t init(uint32_t lowerLimit = 7500,
             uint32_t alarmLimit = 15000,
             uint32_t alarmResumeLimit = 22500);

    // 当前正在使用的磁盘在该raid组中的索引
    const int32_t output_disk()const
    {
        return this->output_disk_;
    }

    // 获取raid组能支持的最大镜头数
    const int32_t get_capacity_camera_num()const
    {
        return capacity_camera_num_;
    }

    // 设置raid组当前正在负载的镜头数
    void set_capacity_camera_num(const int32_t cameraNum)
    {
        capacity_camera_num_ = cameraNum;
    }

    // 获取raid组负载，(当前实际负载/应该负载的值)*100，可能大于100
    const int32_t get_load()const
    {
        return raid_load_;
    }

    // 设置raid组负载，(当前实际负载/应该负载的值)*100，可能大于100
    void set_load(const int32_t raidLoad)
    {
        raid_load_ = raidLoad;
    }


    // 获取改raid组上正在录像的镜头数
    const int32_t get_camera_num()const
    {
        return camera_num_;
    }

    const int32_t get_write_thread_start_id()const
    {
        return output_thread_start_id_;
    }

    // 当前raid组上正在录像的镜头数增加1
    int32_t increase_camera_num(char * strCameraId, int32_t IoQueueIndex);


    // 当前raid组上正在录像的镜头数减少1
    int32_t decrease_camera_num(char * strCameraId, int32_t IoQueueIndex);

    // 刚刚负载上来的镜头获取所使用的输出线程ID.当前组内负载最轻的线程加上本raid组的起始线程ID,
    // 就是要分配的线程在系统的ID值
    int32_t getCurrOutputThreadIndex(uint32_t & ulThreadIndex);
    // 当前raid的错误码
    const int32_t raid_err_type()const
    {
        return raid_err_type_;
    }

    // 获取raid组总的剩余空间
    const uint32_t raid_free_size()const
    {
        return raid_free_size_;
    }

    // 获取对应的磁盘的错误码
    const int32_t disk_err_type(const int32_t diskIndex)const
    {
        return disk_[diskIndex]->flag();
    }

    // raid组的磁盘路径列表
    const char* raid_path()const
    {
        return raid_path_;
    }

    const int32_t disk_total()const
    {
        return disk_total_;
    }

    // 获取对应的磁盘的路径
    const char* disk_path(const int32_t diskIndex)const
    {
        return disk_[diskIndex]->path();
    }

    // 磁盘的元数据是否分离
    const bool get_meda_data_parted_flag(const int32_t diskIndex)const
    {
        return disk_[diskIndex]->getMedaDataPartedFlag();
    }

    /**
     * 检测磁盘空间大小，如果磁盘空间满，那么将设置磁盘不可用。
     */
    int32_t check_disk(const int32_t diskIndex);

    // 根据指定的开始和结束磁盘下标，顺序选择一个
    int32_t select_disk(int32_t nStart, int32_t nEnd);

    // 获取磁盘列表
    int32_t get_all_disk_info(DISK_INFO_VECTOR & diskInfoVector)const;

    // debug获取raid组详细信息
    int32_t debug_raid_tail_info(char* strBuff, uint32_t buffLen)const;

    int32_t debug_write_thread_load(char * strBuff, uint32_t buffLen)const;
    // 获取该磁盘上的镜头列表
    int32_t statCamreaListOnRaid(char* strBuff,uint32_t buffLen);

    // 刷新磁盘属性
    int32_t refreshDiskInfo(const DISK_INFO_VECTOR & diskInfoVec)const;

    // 尝试添加磁盘对象
    int32_t tryAddDisk(const DISK_INFO & szDiskInfo);
    //磁盘的最好状态
    int32_t GetDiskBestStatus();

private:

    //分析磁盘空间配置参数 -- 解析出磁盘分区名和磁盘分区总数
    void parse_args(const char *strSrc, vector <int32_t>& pos);

    // 静态统计信息
    int32_t statistics();

    // raid组获取的输出线程
    void allocOutputThread();

    // 释放raid组的输出线程
    void freeOutputThread();

private:

    //磁盘分区总数
    int32_t disk_total_;

    //磁盘分区数组
    CAC_Record_Disk **disk_;

    //磁盘分区数组缓冲区, assert(p_buf_ == (uint8_t *)disk_)成立
    uint8_t *p_buf_;

    //正在输出的磁盘分区索引，应小于disk_total_
    volatile int32_t output_disk_;

    // 磁盘阈值
    DISK_THRESHOLD disk_threshold_;

    // 镜头id和录像设备对象的map
    typedef ACE_Hash_Map_Manager < ACE_CString, int32_t, ACE_Null_Mutex >
        CAMERA_MAP;

    // 镜头id和录像设备对象的容器
    typedef ACE_Hash_Map_Iterator < ACE_CString, int32_t, ACE_Null_Mutex >
        CAMERA_MAP_ITER;

    typedef ACE_Hash_Map_Entry < ACE_CString, int32_t >
        CAMERA_MAP_ENTRY;

    // 镜头ID与录像对象映射表
    CAMERA_MAP camera_map_;

    ACE_Recursive_Thread_Mutex mutex_;

    // raid组剩余空间,单位MB
    uint32_t raid_free_size_;

    // 当前raid组上正在录像的镜头个数
    volatile int32_t camera_num_;

    // raid组能支持的最大镜头
    int32_t capacity_camera_num_;

    // raid组当前的负载情况，百分比
    int32_t raid_load_;

    // raid组路径
    char raid_path_[DISK_NAME_SIZE * SVS_LUN_MAX_NUM + 1];

    // raid组错误
    volatile int32_t raid_err_type_;
    // 本raid组所用的写线程的起始ID
    int32_t output_thread_start_id_;

    // 记录输出线程负载
    uint32_t outputThreadLoad_[SVS_RAID_MAX_NUM];
};

#endif //_SVS_Record_Disk_h


