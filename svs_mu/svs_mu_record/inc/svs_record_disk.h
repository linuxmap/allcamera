/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : SVS_Record_Disk.h
  �� �� ��   : ����
  ��������   : 2009��8��21��
  ����޸�   :
  ��������   : raid��ʹ����ࡣRaid��֮�为�ؾ��⣬raid���ڵ�lun֮����ѭд������
               ��0,д��n,�ٴ�n��0
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2009��8��21��
    �޸�����   : �޸�

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
    //���̷�������󳤶�(���ս�����)
    DISK_NAME_SIZE = 128,

    //���̿�ʹ�ÿռ�����
    DISK_LOW_LIMIT = (8 * 1024 * 1024),
};

// ��������
enum SVS_DISK_TYPE
{
    SVS_DISK_TYPE_MOUNTDISK = 0,
    SVS_DISK_TYPE_LOCALDISK = 1,
    SVS_DISK_TYPE_MAX
};

/*****************************************************************************
   ���̴����붨��
 *****************************************************************************/
enum SVS_DISK_ERR_TYPE
{
    SVS_DISK_ERR_OK                 = 0x00, /*��������*/
    SVS_DISK_ERR_DISK_ALARM_RESUME,         /*���̽���澯Ԥ����Χ*/
    SVS_DISK_ERR_RAID_ALARM_RESUME,         /*Raid�����澯Ԥ����Χ*/
    SVS_DISK_ERR_DISK_ALARM,                /*���̸澯*/
    SVS_DISK_ERR_RAID_ALARM,                /*Raid��澯*/
    SVS_DISK_ERR_DISK_FULL,                 /*������*/
    SVS_DISK_ERR_RAID_FULL,                 /*Raid����*/
    SVS_DISK_ERR_DISK_BAD,                  /*���̻�*/
    SVS_DISK_ERR_DISK_MOUNT_OFFLINE,         /*����mount����*/
//    SVS_DISK_ERR_RAID_BAD,                /*Raid��*/
    SVS_DISK_ERR_DISK_FS_INVALID,           /*�����ļ�ϵͳ������*/
//    SVS_DISK_ERR_RAID_FS_INVALID,         /*Raid���ļ�ϵͳ������*/
    SVS_DISK_ERR_DISK_FS_READONLY,          /*�����ļ�ϵͳֻ��*/
//    SVS_DISK_ERR_RAID_FS_READONLY,        /*Raid���ļ�ϵͳֻ��*/
    SVS_DISK_ERR_DISK_OFFLINE_CONF,         /*���̲��������б�����*/
    SVS_DISK_ERR_RAID_NO_OUTPUT_THREAD,     /*raid��û�п��õ�����߳�*/
    SVS_DISK_ERR_TYPE_MAX
};

typedef struct RAID_INFO_
{
    int32_t     RaidIndex;
    int32_t     RaidLevel;
}RAID_INFO;

// ������Ϣ�ṹ��
typedef struct DISK_INFO_
{
    char        DiskPath[DISK_NAME_SIZE];// ����·��
    bool        MedaDataPartedFlag;// Ԫ���ݷ����ʶ
    int32_t         DiskMountTypeFlag;// �����Ƿ���mount�����Ĵ���
    RAID_INFO   RaidInfo;// ��������raid������
}DISK_INFO;

typedef vector<DISK_INFO>DISK_INFO_VECTOR;

// ��������ֵ�ṹ�嶨��
typedef struct DISK_THRESHOLD_
{
    //���̿��ÿռ����ޣ���λKB
    uint32_t m_lowerLimit;

    // ���̸澯��ֵ����λKB
    uint32_t m_alarmLimit;

    // ���̸澯�ָ�ֵ����λKB
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
 * @brief ¼����̷�����
 */
class CAC_Record_Disk
{
private:
    CAC_Record_Disk();
public:

    /**
     * �������̷���<strPath>, ��������<index>
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

    // ��Ԫ���ݷ���Ĵ���
    int32_t open_realtime_disk();

    // ��Ԫ���ݲ�����Ĵ���
    int32_t open_normal_disk();

    // Ԫ���ݺ���Ƶ�����Ƿ�����ʶ
    const bool getMedaDataPartedFlag()const
    {
        return meda_data_parted_flag_;
    }

    // ���ô�������

    int32_t setDiskFlag(const int32_t newDiskFlag);
    int32_t setMedaDataPartedFlag(bool medaDataPartedFlag);

    int32_t setMountFlag(const int32_t mountFlag);
private:
    int32_t converse_disk_flag();
protected:
    // ����������
    volatile const size_t index_;

    //����ʣ���ܴ�С����λ: KB
    volatile uint64_t total_size_;

    // �����Ѿ�ʹ�ô�С����λ: KB
    volatile uint64_t used_size_;

    // �����Ѿ�ʣ��ռ��С����λ: KB
    volatile uint64_t free_size_;

    //��������
    char str_path_[DISK_NAME_SIZE];

    //�������Ƴ���
    int32_t path_len_;

    //������b_full_��������Ϊtrue, ����Ϊfalse.
    int32_t flags_;

    ACE_Thread_Mutex mutex_;

    // �������ͣ�mount�����Ĵ��̻򱾵�·��
    int32_t disk_mount_flag_;

    // ������ֵ
    DISK_THRESHOLD disk_threshold_;

    // ����ͳ����
    SVS_Disk_Stat disk_stat_;

    // Ԫ���ݺ���Ƶ���ݷ����ʶ��Ĭ����δ�����
    bool meda_data_parted_flag_;
};

/**
 * @class CAC_Record_Raid
 * @brief ����������
 */
class CAC_Record_Raid
{
public:
    CAC_Record_Raid();
    virtual ~CAC_Record_Raid();

    /**
     * ��ʼ����������
     * ���̿ռ����߳�ÿ@a nInterval����һ�δ��̿ռ䣬������̷����Ŀ���
     * �ռ�С��@a lowerLimit�ף���ô��Ǹô��̷���Ϊ�����á�
     */
    int32_t init(uint32_t lowerLimit = 7500,
             uint32_t alarmLimit = 15000,
             uint32_t alarmResumeLimit = 22500);

    // ��ǰ����ʹ�õĴ����ڸ�raid���е�����
    const int32_t output_disk()const
    {
        return this->output_disk_;
    }

    // ��ȡraid����֧�ֵ����ͷ��
    const int32_t get_capacity_camera_num()const
    {
        return capacity_camera_num_;
    }

    // ����raid�鵱ǰ���ڸ��صľ�ͷ��
    void set_capacity_camera_num(const int32_t cameraNum)
    {
        capacity_camera_num_ = cameraNum;
    }

    // ��ȡraid�鸺�أ�(��ǰʵ�ʸ���/Ӧ�ø��ص�ֵ)*100�����ܴ���100
    const int32_t get_load()const
    {
        return raid_load_;
    }

    // ����raid�鸺�أ�(��ǰʵ�ʸ���/Ӧ�ø��ص�ֵ)*100�����ܴ���100
    void set_load(const int32_t raidLoad)
    {
        raid_load_ = raidLoad;
    }


    // ��ȡ��raid��������¼��ľ�ͷ��
    const int32_t get_camera_num()const
    {
        return camera_num_;
    }

    const int32_t get_write_thread_start_id()const
    {
        return output_thread_start_id_;
    }

    // ��ǰraid��������¼��ľ�ͷ������1
    int32_t increase_camera_num(char * strCameraId, int32_t IoQueueIndex);


    // ��ǰraid��������¼��ľ�ͷ������1
    int32_t decrease_camera_num(char * strCameraId, int32_t IoQueueIndex);

    // �ոո��������ľ�ͷ��ȡ��ʹ�õ�����߳�ID.��ǰ���ڸ���������̼߳��ϱ�raid�����ʼ�߳�ID,
    // ����Ҫ������߳���ϵͳ��IDֵ
    int32_t getCurrOutputThreadIndex(uint32_t & ulThreadIndex);
    // ��ǰraid�Ĵ�����
    const int32_t raid_err_type()const
    {
        return raid_err_type_;
    }

    // ��ȡraid���ܵ�ʣ��ռ�
    const uint32_t raid_free_size()const
    {
        return raid_free_size_;
    }

    // ��ȡ��Ӧ�Ĵ��̵Ĵ�����
    const int32_t disk_err_type(const int32_t diskIndex)const
    {
        return disk_[diskIndex]->flag();
    }

    // raid��Ĵ���·���б�
    const char* raid_path()const
    {
        return raid_path_;
    }

    const int32_t disk_total()const
    {
        return disk_total_;
    }

    // ��ȡ��Ӧ�Ĵ��̵�·��
    const char* disk_path(const int32_t diskIndex)const
    {
        return disk_[diskIndex]->path();
    }

    // ���̵�Ԫ�����Ƿ����
    const bool get_meda_data_parted_flag(const int32_t diskIndex)const
    {
        return disk_[diskIndex]->getMedaDataPartedFlag();
    }

    /**
     * �����̿ռ��С��������̿ռ�������ô�����ô��̲����á�
     */
    int32_t check_disk(const int32_t diskIndex);

    // ����ָ���Ŀ�ʼ�ͽ��������±꣬˳��ѡ��һ��
    int32_t select_disk(int32_t nStart, int32_t nEnd);

    // ��ȡ�����б�
    int32_t get_all_disk_info(DISK_INFO_VECTOR & diskInfoVector)const;

    // debug��ȡraid����ϸ��Ϣ
    int32_t debug_raid_tail_info(char* strBuff, uint32_t buffLen)const;

    int32_t debug_write_thread_load(char * strBuff, uint32_t buffLen)const;
    // ��ȡ�ô����ϵľ�ͷ�б�
    int32_t statCamreaListOnRaid(char* strBuff,uint32_t buffLen);

    // ˢ�´�������
    int32_t refreshDiskInfo(const DISK_INFO_VECTOR & diskInfoVec)const;

    // ������Ӵ��̶���
    int32_t tryAddDisk(const DISK_INFO & szDiskInfo);
    //���̵����״̬
    int32_t GetDiskBestStatus();

private:

    //�������̿ռ����ò��� -- ���������̷������ʹ��̷�������
    void parse_args(const char *strSrc, vector <int32_t>& pos);

    // ��̬ͳ����Ϣ
    int32_t statistics();

    // raid���ȡ������߳�
    void allocOutputThread();

    // �ͷ�raid�������߳�
    void freeOutputThread();

private:

    //���̷�������
    int32_t disk_total_;

    //���̷�������
    CAC_Record_Disk **disk_;

    //���̷������黺����, assert(p_buf_ == (uint8_t *)disk_)����
    uint8_t *p_buf_;

    //��������Ĵ��̷���������ӦС��disk_total_
    volatile int32_t output_disk_;

    // ������ֵ
    DISK_THRESHOLD disk_threshold_;

    // ��ͷid��¼���豸�����map
    typedef ACE_Hash_Map_Manager < ACE_CString, int32_t, ACE_Null_Mutex >
        CAMERA_MAP;

    // ��ͷid��¼���豸���������
    typedef ACE_Hash_Map_Iterator < ACE_CString, int32_t, ACE_Null_Mutex >
        CAMERA_MAP_ITER;

    typedef ACE_Hash_Map_Entry < ACE_CString, int32_t >
        CAMERA_MAP_ENTRY;

    // ��ͷID��¼�����ӳ���
    CAMERA_MAP camera_map_;

    ACE_Recursive_Thread_Mutex mutex_;

    // raid��ʣ��ռ�,��λMB
    uint32_t raid_free_size_;

    // ��ǰraid��������¼��ľ�ͷ����
    volatile int32_t camera_num_;

    // raid����֧�ֵ����ͷ
    int32_t capacity_camera_num_;

    // raid�鵱ǰ�ĸ���������ٷֱ�
    int32_t raid_load_;

    // raid��·��
    char raid_path_[DISK_NAME_SIZE * SVS_LUN_MAX_NUM + 1];

    // raid�����
    volatile int32_t raid_err_type_;
    // ��raid�����õ�д�̵߳���ʼID
    int32_t output_thread_start_id_;

    // ��¼����̸߳���
    uint32_t outputThreadLoad_[SVS_RAID_MAX_NUM];
};

#endif //_SVS_Record_Disk_h


