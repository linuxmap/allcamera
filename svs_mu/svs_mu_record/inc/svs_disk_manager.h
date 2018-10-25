/******************************************************************************

                  版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文 件 名   : SVS_Disk_Manager.h
  版 本 号   : 初稿
  生成日期   : 2009年8月21日
  最近修改   :
  功能描述   : 磁盘管理单例类,实现了对磁盘文件的管理及磁盘异常检测机制;
               对真实磁盘文件的读写依赖于IO Scheduler模块对IO的调度
  函数列表   :
  修改历史   :
  1.日    期   : 2009年8月21日
    修改内容   : 创建文件

******************************************************************************/
#ifndef __SVS_DISK_MANAGER_H__
#define  __SVS_DISK_MANAGER_H__
#include <svs_ace_header.h>
#include "svs_record_disk.h"

// 磁盘管理线程类型
enum SVS_DISK_THREAD
{
    SVS_DISK_CHECK_THREAD = 0,
    SVS_DISK_MAX_THREAD
};

// 磁盘检测任务类型
enum SVS_DISK_CHECK_TASK_TYPE
{
    SVS_DISK_CHECK_TASK_TIMER  = 0,
    SVS_DISK_CHECK_TASK_EXCEPT = 1,
    SVS_DISK_CHECK_TASK_MAX
};

// 磁盘检测任务消息，如果是定时器任务，则遍历所有测磁盘，对所有磁盘进行空间检测
typedef struct _SVS_DISK_CHECK_TASK
{
    int32_t TaskType; // 定时器检测任务，或者磁盘异常
    int32_t RaidIndex;
    int32_t DiskIndex;
    int32_t ErrCode;
}SVS_DISK_CHECK_TASK,*PSVS_DISK_CHECK_TASK;


enum VFD_STATUS
{
    VFD_FREE_STATUS = 1, // VFD空闲状态，还未被占用，可以获取到
    SYSFD_USABLE_STATUS = 2, // 系统句柄可用
    SYSFD_SWITCH_FILE_STATUS = 3, // 系统句柄正常切换文件状态
    SYSFD_UNUSABLE_STATUS = 4, // 系统句柄不可用
};

typedef struct _SVS_DISK_VFILE
{
    int32_t VFd; // 初始化之后就不再变化
    int32_t IoQueueIndex;// 初始化之后就不再变化
    int32_t SysFd;
    int32_t RaidIndex;
    int32_t DiskIndex;
    char CameraId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
    char RecordFilePath[MAXNAMELEN + 1];
    VFD_STATUS VfdStatus;
}SVS_DISK_VFILE,*PSVS_DISK_VFILE;

/**
 * @class SVS_Disk_Manager
 * @brief 录像磁盘管理类
 */
class SVS_Disk_Manager : public ACE_Task<ACE_MT_SYNCH>
{
public:  // 以下接口为通用接口

    // 单例对象，以引用的方式返回就不需要进行地址有效性判断
    static SVS_Disk_Manager & instance()
    {
        static SVS_Disk_Manager svs_disk_manager;
        return svs_disk_manager;
    };

    /*
     * 初始化磁盘阵列
     * 磁盘空间检测线程每@a nInterval秒检查一次磁盘空间，如果磁盘分区的可用
     * 空间小于@a lowerLimit兆，那么标记该磁盘分区为不可用。
     */
    virtual int32_t initialize(int32_t nInterval,
                             uint32_t lowerLimit,
                             uint32_t alarmLimit,
                             uint32_t alarmResumeLimit);

    // 创建线程
    virtual int32_t open(void *);

    // 关闭对象,析构前的最后一步操作
    virtual int32_t close(u_long );

    // 磁盘检测线程
    virtual int32_t svc(void );

public: // 以下接口实现对磁盘异常的检测和处理
    // 处理IO Scheduler上报的磁盘或文件操作异常
    UTAPI int32_t notifyDiskExcept(int32_t fd, int32_t errCode);

    // 处理定时检测磁盘任务
    UTAPI int32_t checkDiskTimerCallback();

public:        // 以下接口实现对磁盘文件的管理

    // 新建或者打开一个文件,需要传入相对路径,返回内部分配的文件句柄V_FD
    UTAPI int32_t openFile(char * cameraId,time_t recordTime,
                         char* fileExName,uint64_t &fileSize,
                         int32_t& vfd,bool & medaDataPartedFlag);

    // 关闭一个已经打开的文件
    UTAPI int32_t closeFile(int32_t vfd);

    // 获取文件路径
    UTAPI int32_t getFileFullName(int32_t vfd, char *szFileName);

    // 向指定文件写入数据
    UTAPI int32_t writeFile(int32_t vfd,
                    ACE_Message_Block *pData,
                    IO_CallBack_Func pFunc = NULL,
                    void *pParam = NULL);

    // 从指定文件读取数据
    UTAPI int32_t readFile(int32_t vfd,
                ACE_Message_Block *pData = NULL,
                u_long = 0,
                IO_CallBack_Func pFunc = NULL,
                void *pParam = NULL)const;

    // 释放vfd
    UTAPI void freeVfd(int32_t vfd);

public:        // 其它接口

    // 检查是否有可用磁盘，成功返回0，失败返回-1
    int32_t checkUseableRaid();

    // 索引路径是否可用
    bool isRecordIndexpathuseable()const;

    UTAPI int32_t getAllDiskInfo(DISK_INFO_VECTOR & diskInfoVector)const;

    UTAPI const int32_t raidTotalNum()const
    {
        return m_raidTotal;
    }

    UTAPI int32_t getUsableRaidNum()const;

    // 获取raid组信息
    UTAPI int32_t debugGetRaidListInfo(char * strBufferInfo, const uint32_t bufferLen);

    // telnet获取写录像线程上的镜头信息
    UTAPI int32_t debugGetWriteThreadInfo(char * strBufferInfo, const uint32_t bufferLen);
    // 获取raid组信息
    UTAPI int32_t statCamreaListOnRaid(char * strBufferInfo, const uint32_t bufferLen);

    // 查询mount的磁盘是否在线
    UTAPI int32_t checkMountOnlineStatus(const char * strDiskPath)const;

    // 刷新磁盘配置
    UTAPI int32_t refreshDiskList(const char * strDiskPath);

    UTAPI int32_t allocOutputThreadIndex(uint32_t & ulThreadIndex);

    UTAPI void freeOutputThreadIndex(uint32_t ulThreadIndex);

    UTAPI int32_t getFreeOutputThreadGroupNum();

    UTAPI const char* getDiskCheckBuff()const
    {
        return m_strDiskCheckBuff;
    }
private:       // 内部接口
    SVS_Disk_Manager();
    virtual ~SVS_Disk_Manager();

    // 处理磁盘检测任务
    UTAPI int32_t processCheckDiskTask(const ACE_Message_Block * mb);

    // 检测磁盘任务消息入队列
    UTAPI int32_t enqueueCheckDiskTask( ACE_Message_Block *newItem, ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = putq(newItem, timeout);
        return enqueueResult;
    }

    // 定时器回调
    static void timerCallback( void * /*pArg*/, int32_t /*eventId*/, int32_t /*timerId*/, int32_t /*nDummy*/ );

    // 解析raid组配置，raid组之间用'|'隔开
    UTAPI int32_t parseDiskListConf(const char * strDiskPath, DISK_INFO_VECTOR & diskInfoVect)const;

    // 创建raid组对象
    UTAPI int32_t createRaid(const char *pRaidList, vector <int32_t>& pos);

    // 周期性检测磁盘
    UTAPI int32_t periodCheckRaid();

    // 处理磁盘异常检测任务
    UTAPI int32_t exceptCheckDisk(const SVS_DISK_CHECK_TASK * pTaskMsg);

    // 磁盘异常通知，通知给上层业务层
    UTAPI int32_t notifyExcept(int32_t raidId,int32_t diskErr);

    // 创建目录
    UTAPI int32_t createDir(const char * diskPath, char * camerId, char * strDay)const;

    // 申请磁盘
    UTAPI int32_t allocDisk(int32_t & vFd, int32_t & raidIndex);

    // 时间转换成录像文件名
    UTAPI void time2RecodFileName(const time_t RecordTime,
                                        char* strRecordFileName,
                                        const uint32_t bufLen,
                                        char* strfileExName)const;

    // 创建录像文件
    UTAPI int32_t createFile(const char* fileFullPath,bool medaDataPartedFlag,
                            uint64_t &fileSize,int32_t &fd)const;

    // 创建磁盘异常检测消息
    UTAPI int32_t createDiskExceptMsg(int32_t raidIndex,int32_t diskIndex,int32_t errCode);

    // 获取mounts信息
    UTAPI void getMounts();

    // 计算并设置raid组的负载
    UTAPI void countRaidLoad();

    // 处理盘满删除
    UTAPI int32_t diskFullDelRecord();

    UTAPI int32_t send_disk_full_alarm()const;

    // 解析磁盘配置信息
    UTAPI int32_t parseDiskInfo(char * diskPath, DISK_INFO & diskInfo)const;

    // 移动指针
    char* move_pos(const char * pos)const;

private:       // 磁盘文件管理相关变量

    ACE_Recursive_Thread_Mutex mutex_;

    SVS_DISK_VFILE m_fileList[RECORD_MAX_LENS_NUM];
    int32_t m_vfdCursor;

    // 空闲输出线程list
    std::list<uint32_t>   m_freeOutputThreadIndexList;
    ACE_Mutex                  m_FreeOutputThreadIndexMutex;

private:       // 磁盘检测相关变量
    // 线程运行标识
    volatile bool m_bThreadFlag;

    // 周期性磁盘检测定时器
    int32_t m_checkDiskTimer;

    // raid组总数
    int32_t m_raidTotal;

    // 检查周期，单位：秒
    int32_t m_checkInterval;

    // 磁盘可用空间下限，单位KB
    uint32_t m_lowerLimit;

    // 磁盘告警阈值，单位KB
    uint32_t m_alarmLimit;

    // 磁盘告警恢复值，单位KB
    uint32_t m_alarmResumeLimit;

    // raid组
    CAC_Record_Raid m_raidList[SVS_RAID_MAX_NUM];

    // 索引磁盘对象
    CAC_Record_Disk* m_IndexFileDisk;

    // 存放mounts信息
    char m_mountsInfo[DISK_NAME_SIZE * SVS_LUN_MAX_NUM * SVS_RAID_MAX_NUM];

    volatile bool m_bIsMountsOk;

    // 每轮盘满删除启动标识
    volatile bool m_diskFullStartFlag;

    ACE_Recursive_Thread_Mutex m_diskFullDelingFlagMutex;

    // 磁盘检测用缓存区
    char *m_strDiskCheckBuff;

};
#endif /* __SVS_DISK_MANAGER_H__ */

