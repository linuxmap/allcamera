//=============================CAC_Record_Disk BEGIN=================================
#include <sys/statvfs.h>
#include "svs_rt_record_common.h"
#include "svs_stat_manager.h"
#include "svs_record_disk.h"
#include "svs_real_record_server.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"

CAC_Record_Disk::CAC_Record_Disk(const char *strPath, size_t nIndex,
                                        const DISK_THRESHOLD &stDiskThreshold)
    : index_(nIndex), disk_threshold_(stDiskThreshold)
{
    total_size_ = 0;
    used_size_ = 0;
    free_size_ = 0;

    (void)ACE_OS::strcpy(str_path_, strPath);
    path_len_ = (int32_t)strlen(str_path_);

    this->flags_ = SVS_DISK_ERR_OK;
    meda_data_parted_flag_ = false;
}

CAC_Record_Disk::~CAC_Record_Disk()
{
}

/*****************************************************************************
 函 数 名  : statistics
 功能描述  : 检查磁盘空间和文件系统读写属性
 输入参数  : uint32_t lowerLimit:停止录像时的磁盘剩余空间阈值
             uint32_t alarmLimit:告警时的磁盘剩余空间阈值
             uint32_t alarmResumeLimit:告警恢复时的磁盘剩余空间阈值
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t CAC_Record_Disk::statistics()
{
    // 如果磁盘不在配置列表，则不用检测
    if(SVS_DISK_ERR_DISK_OFFLINE_CONF == flags_)
    {
        return SVS_RESULT_OK;
    }

    // 如果是mount上来的磁盘，查看mount是否掉线了
    if ( (SVS_DISK_TYPE_MOUNTDISK == disk_mount_flag_)
      && (SVS_RESULT_OK != SVS_Disk_Manager::instance().checkMountOnlineStatus(str_path_)))
    {
        SVS_LOG((SVS_LM_WARNING, "Mount disk is offline.path[%s]",str_path_));
        total_size_ = 0;
        free_size_ = 0;
        used_size_ = 0;
        flags_ = SVS_DISK_ERR_DISK_MOUNT_OFFLINE;
        disk_stat_.updateDiskSize((uint64_t)0, (uint64_t)0);
        return SVS_ERR_MOUNTOFFLINE;
    }

    int32_t iRet = SVS_RESULT_OK;
    if(meda_data_parted_flag_)
    {
        iRet = open_realtime_disk();
    }
    else
    {
        iRet = open_normal_disk();
    }

    if(SVS_RESULT_OK != iRet)
    {
        return iRet;
    }

    (void)converse_disk_flag();

    // 可用的磁盘空间,注意:如果被翻转下面的逻辑会赋值为0
    uint64_t usableSize = this->free_size_ - disk_threshold_.m_lowerLimit;

    if(SVS_DISK_ERR_DISK_FULL == this->flags_)
    {
        usableSize = 0;
    }

    // 如果是索引目录，则不用加入磁盘空间统计
    if(index_ == SVS_LUN_MAX_NUM)
    {
        disk_stat_.updateDiskSize((uint64_t)0,(uint64_t)0);
    }
    else
    {
        disk_stat_.updateDiskSize(this->total_size_, usableSize);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : check_disk
 功能描述  : 全面检查磁盘,由浅入深检查磁盘，直到检查出磁盘异常或磁盘真的没有异常
 输入参数  : uint32_t lowerLimit:停止录像时的磁盘剩余空间阈值
             uint32_t alarmLimit:告警时的磁盘剩余空间阈值
             uint32_t alarmResumeLimit:告警恢复时的磁盘剩余空间阈值
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t CAC_Record_Disk::check_disk()
{
    // 1.先简单检测,磁盘空间和文件系统只读
    (void)statistics();

    // 如果已经检测出磁盘依然异常，则不用再深入检测了
    if(SVS_DISK_ERR_DISK_BAD <= flags_)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Disk is unusable.disk path[%s], error type[%d].",
            str_path_,
            flags_));

        return SVS_RESULT_OK;
    }

    //1 TODO 其它中间检测方法，待续中......

    // 最后尝试创建一个文件，并写一些数据，看磁盘是否正常
    char filePath[MAXNAMELEN + 1] = {0,};
    (void)snprintf(filePath, sizeof(filePath),"%s/diskTest",str_path_);

    // 打开模式
    int32_t flags = O_CREAT | O_WRONLY | O_APPEND | O_LARGEFILE;
    if (1 <= CAC_RT_Record_Server::instance()->get_use_direct_io_flag())
    {
        flags |= O_DIRECT;

        SVS_LOG((SVS_LM_INFO, "Create file use O_DIRECT flag.Disk check."));
    }

    int32_t fd = ACE_OS::open(filePath,flags,ACE_DEFAULT_FILE_PERMS);
    if(-1 >= fd)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Check disk finished.Disk is Bad.Open file failed."
            "disk path[%s]",
            str_path_));

        flags_ = SVS_DISK_ERR_DISK_BAD;
        disk_stat_.updateDiskSize((uint64_t)0,(uint64_t)0);

        return SVS_ERR_DISKFAULT;
    }

    // 如果元数据和视频数据分离
    if (meda_data_parted_flag_)
    {
        struct fsxattr attrs;
        if(0 != ioctl(fd,XFS_IOC_FSGETXATTR,&attrs))
        {
            SVS_LOG((SVS_LM_ERROR,
            "Check disk finished.Disk is Bad.ioctl failed."
            "disk path[%s],file path[%s].",
            str_path_,
            filePath));

            flags_ = SVS_DISK_ERR_DISK_BAD;

            // 释放资源，关闭文件，并删除临时文件
            (void)ACE_OS::close((ACE_HANDLE)fd);
            (void)ACE_OS::unlink(filePath);

            disk_stat_.updateDiskSize((uint64_t)0,(uint64_t)0);

            return SVS_ERR_DISKFAULT;
        }
        attrs.fsx_xflags |= XFS_XFLAG_REALTIME;
        if(0 != ioctl(fd,XFS_IOC_FSSETXATTR,&attrs))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Check disk finished.Disk is Bad.ioctl failed."
                "disk path[%s],file path[%s].",
                str_path_,
                filePath));

            flags_ = SVS_DISK_ERR_DISK_BAD;

            // 释放资源，关闭文件，并删除临时文件
            (void)ACE_OS::close((ACE_HANDLE)fd);
            (void)ACE_OS::unlink(filePath);

            disk_stat_.updateDiskSize((uint64_t)0,(uint64_t)0);

            return SVS_ERR_DISKFAULT;
        }
    }

    // 打开成功后，尝试写数据
    uint32_t dataLen = CAC_RT_Record_Server::instance()->write_media_data_stripe_width();
    char * buff = (char*)SVS_Disk_Manager::instance().getDiskCheckBuff();
    if(NULL == buff)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Check disk failed as get the buffer for check disk failed."
            "disk path[%s].",
            str_path_));

        return SVS_RESULT_OK;
    }

    // 非4K对齐,进行4K对齐处理
    uint32_t alignPos = (uint32_t)(uint64_t)buff;
    if( 0 != (alignPos % SVS_RECORD_FILE_ALIGN_LEN) )
    {
        alignPos = (alignPos + SVS_RECORD_FILE_ALIGN_LEN) & 0xfffff000;
        buff = (char*)(uint64_t)alignPos;
    }

    int32_t writeLen = 0;
    writeLen = ACE_OS::write((ACE_HANDLE)fd, buff, dataLen);
    if((int32_t)dataLen != writeLen)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Check disk finished.Disk is Bad.Write file failed."
            "disk path[%s],file path[%s],data length[%d],real write length[%d]",
            str_path_,
            filePath,
            dataLen,
            writeLen));

        flags_ = SVS_DISK_ERR_DISK_BAD;

        // 释放资源，关闭文件，并删除临时文件
        (void)ACE_OS::close((ACE_HANDLE)fd);
        (void)ACE_OS::unlink(filePath);
        disk_stat_.updateDiskSize((uint64_t)0,(uint64_t)0);

        return SVS_ERR_DISKFAULT;
    }

    // 打开和写文件都成功了，表示磁盘正常
    // 释放资源，关闭文件，并删除临时文件
    (void)ACE_OS::close((ACE_HANDLE)fd);
    (void)ACE_OS::unlink(filePath);

    SVS_LOG((SVS_LM_INFO, "Check disk finished.Disk is OK.disk path[%s]",str_path_));

    return SVS_RESULT_OK;
}

// 磁盘状态转换
int32_t CAC_Record_Disk::converse_disk_flag()
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, -1);
    if (this->free_size_ <= disk_threshold_.m_lowerLimit)
    {
        this->flags_ = SVS_DISK_ERR_DISK_FULL;
        SVS_LOG((SVS_LM_WARNING,
            "Disk is full. disk path[%s], flag[0x%02x].",
            this->str_path_,
            this->flags_));
    }
    else if (this->free_size_ <= disk_threshold_.m_alarmLimit)
    {
        this->flags_ = SVS_DISK_ERR_DISK_ALARM;
        SVS_LOG((SVS_LM_INFO,
            "Disk is alarm. disk path[%s], flag[0x%02x].",
            this->str_path_,
            this->flags_));
    }
    else if (this->free_size_ <= disk_threshold_.m_alarmResumeLimit)
    {
        this->flags_ = SVS_DISK_ERR_DISK_ALARM_RESUME;
        SVS_LOG((SVS_LM_INFO,
            "Disk is alarm resume. disk path[%s], flag[0x%02x].",
            this->str_path_,
            this->flags_));
    }
    else
    {
        this->flags_ = SVS_DISK_ERR_OK;
        SVS_LOG((SVS_LM_DEBUG,
            "Disk is OK. disk path[%s], flag[0x%02x].",
            this->str_path_,
            this->flags_));
    }

    return SVS_RESULT_OK;
}

// 设置磁盘状态
int32_t CAC_Record_Disk::setDiskFlag(const int32_t newDiskFlag)
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, -1);
    if(flags_ != newDiskFlag)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Disk flag has been changed.int32_t type: old flag[%d] to new flag[%d]",
            flags_,
            newDiskFlag));

        flags_ = newDiskFlag;
    }

    return SVS_RESULT_OK;
}

// 设置磁盘的类型是否支持元数据分离
int32_t CAC_Record_Disk::setMedaDataPartedFlag(bool medaDataPartedFlag)
{
    SVS_LOG((SVS_LM_DEBUG,
        "old realtime flag[%d], new realtime flag[%d].",
        meda_data_parted_flag_,
        (int32_t)medaDataPartedFlag));

    ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, -1);
    // 如果磁盘元数据分离标识变化了，则重新赋值
    if(meda_data_parted_flag_ && (!medaDataPartedFlag))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Disk meda data parted flag has been changed."
            "media data parted ===> media data unparted."));

        meda_data_parted_flag_ = medaDataPartedFlag;
    }
    else if((!meda_data_parted_flag_) && medaDataPartedFlag)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Disk meda data parted flag has been changed."
            "media data unparted ===> media data parted."));

        meda_data_parted_flag_ = medaDataPartedFlag;
    }

    return SVS_RESULT_OK;
}

// 设置磁盘的mount状态
int32_t CAC_Record_Disk::setMountFlag(const int32_t mountFlag)
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, -1);
    if(disk_mount_flag_ != mountFlag)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Disk mount flag has been changed.type: old flag[%d] to new flag[%d]",
            disk_mount_flag_,
            mountFlag));

        disk_mount_flag_ = mountFlag;
    }

    return SVS_RESULT_OK;
}

int32_t CAC_Record_Disk::open_realtime_disk()
{
    int32_t iRet = SVS_RESULT_FAILURE;
    int32_t fd = open (this->str_path_, O_RDONLY,0755);
    if (-1 == fd)
    {
        const int32_t nErrCode = ACE_OS::last_error();

        SVS_LOG((SVS_LM_CRITICAL,
            "Fail to read disk information.disk[%s], errno[%d].",
            this->str_path_,
            nErrCode));

        ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, -1);
        // 目录错误
        if((ENOENT == nErrCode) || (ENOTDIR == nErrCode))
        {
            this->flags_ = SVS_DISK_ERR_DISK_BAD;
            iRet = SVS_ERR_DISKFAULT;
        }
        else if(EIO == nErrCode)// 文件系统错误
        {
            this->flags_ = SVS_DISK_ERR_DISK_FS_INVALID;
            iRet = SVS_ERR_FILESYSTEM;
        }
        else
        {
            this->flags_ = SVS_DISK_ERR_DISK_BAD;
            iRet = SVS_ERR_IOERR;
        }

        disk_stat_.updateDiskSize((uint64_t)0,(uint64_t)0);
        ::close(fd);
        return iRet;
    }

    struct xfs_fsop_geom geom;
    struct xfs_fsop_counts counts;
    if(0 != ioctl(fd, XFS_IOC_FSGEOMETRY, &geom))
    {
        ::close(fd);
        SVS_LOG((SVS_LM_CRITICAL,
            "Ioctl disk information failed.disk[%s].",
            this->str_path_));
        ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, -1);
        this->flags_ = SVS_DISK_ERR_DISK_BAD;
        return SVS_ERR_IOERR;
    }

    if (0 != ioctl(fd, XFS_IOC_FSCOUNTS, &counts))
    {
        ::close(fd);
        SVS_LOG((SVS_LM_CRITICAL,
            "Ioctl disk information failed.disk[%s].",
            this->str_path_));
        ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, -1);
        this->flags_ = SVS_DISK_ERR_DISK_BAD;
        return SVS_ERR_IOERR;
    }

    this->total_size_ = (uint64_t)geom.rtextents * geom.rtextsize * geom.blocksize / 1024;
    this->free_size_ = (uint64_t)counts.freertx * geom.rtextsize * geom.blocksize / 1024;
    this->used_size_ = this->total_size_ - this->free_size_;
    ::close(fd);

    return SVS_RESULT_OK;
}

int32_t CAC_Record_Disk::open_normal_disk()
{
    int32_t iRet = SVS_RESULT_FAILURE;
    struct statvfs hdstat;
    if (-1 == statvfs(this->str_path_, &hdstat))
    {
        const int32_t nErrCode = ACE_OS::last_error();

        SVS_LOG((SVS_LM_CRITICAL,
            "Fail to read disk information.disk[%s], errno[%d].",
            this->str_path_,
            nErrCode));

        ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, -1);
        // 目录错误
        if((ENOENT == nErrCode) || (ENOTDIR == nErrCode))
        {
            this->flags_ = SVS_DISK_ERR_DISK_BAD;
            iRet = SVS_ERR_DISKFAULT;
        }
        else if(EIO == nErrCode)// 文件系统错误
        {
            this->flags_ = SVS_DISK_ERR_DISK_FS_INVALID;
            iRet = SVS_ERR_FILESYSTEM;
        }
        else
        {
            this->flags_ = SVS_DISK_ERR_DISK_BAD;
            iRet = SVS_ERR_IOERR;
        }

        disk_stat_.updateDiskSize((uint64_t)0,(uint64_t)0);
        return iRet;
    }

     // 文件系统只读
    if(ST_RDONLY == hdstat.f_flag)
    {
        SVS_LOG((SVS_LM_ERROR, "Disk's filesystem is realonly.disk[%s]", this->str_path_));
        ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, -1);
        this->flags_ = SVS_DISK_ERR_DISK_FS_READONLY;
        disk_stat_.updateDiskSize((uint64_t)0,(uint64_t)0);
        return SVS_ERR_FSREADONLY;
    }

    this->total_size_ = (uint64_t)hdstat.f_bsize * hdstat.f_blocks / 1024;
    this->free_size_ = (uint64_t)hdstat.f_bsize * hdstat.f_bavail / 1024;
    this->used_size_ = this->total_size_ - this->free_size_;

    return SVS_RESULT_OK;
}

//========================CAC_Record_Disk END==================================

//========================CAC_Record_Raid BEGIN================================
CAC_Record_Raid::CAC_Record_Raid()
{
    this->disk_total_ = 0;
    this->disk_  = NULL;
    this->p_buf_ = NULL;
    this->output_disk_ = -1;

    raid_err_type_ = SVS_DISK_ERR_OK;
    camera_num_ = 0;
    capacity_camera_num_ = 0;
    raid_free_size_ = 0;

    output_thread_start_id_ = -1;

    memset(&disk_threshold_, 0, sizeof(disk_threshold_));
    memset(outputThreadLoad_,  0, sizeof(outputThreadLoad_));
}

CAC_Record_Raid::~CAC_Record_Raid()
{
    if (NULL != disk_)
    {
        delete[] disk_;
        disk_ = NULL;
    }

    if(NULL != p_buf_)
    {
        delete [] p_buf_;
        p_buf_ = NULL;
    }
}

/*****************************************************************************
 函 数 名  : init
 功能描述  : 初始化Raid。创建所配置的该raid组的所有磁盘，如果所配置的磁盘中有非法
             的磁盘则初始化失败
 输入参数  : char * strRaidPath: raid组路径配置
             uint32_t lowerLimit:停止录像时的磁盘剩余空间阈值
             uint32_t alarmLimit:告警时的磁盘剩余空间阈值
             uint32_t alarmResumeLimit:告警恢复时的磁盘剩余空间阈值
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t CAC_Record_Raid::init(uint32_t lowerLimit,
                          uint32_t alarmLimit,
                          uint32_t alarmResumeLimit)
{
    // 单位都是KB，不用转换
    this->disk_threshold_.m_lowerLimit = lowerLimit;
    this->disk_threshold_.m_alarmLimit = alarmLimit;
    this->disk_threshold_.m_alarmResumeLimit = alarmResumeLimit;

    raid_load_ = 0;
    disk_total_ = 0;
    output_disk_ = -1;

    // 标识磁盘不可用
    raid_err_type_ = SVS_DISK_ERR_RAID_FULL;

    //创建磁盘分区对象
    try
    {
        this->disk_ = new CAC_Record_Disk*[SVS_LUN_MAX_NUM];
    }
    catch(...)
    {
        return SVS_ERR_NOMEM;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : statistics
 功能描述  : 统计磁盘信所有磁盘信息，对于存在异常的录像磁盘，进行更严格的检查
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t CAC_Record_Raid::statistics()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    raid_free_size_ = 0;

    for(int32_t i = 0; i < disk_total_; i++)
    {
        // 已经异常的磁盘，检查更全面
        if(SVS_DISK_ERR_DISK_BAD <= disk_[i]->flag())
        {
            (void)disk_[i]->check_disk();
        }
        else
        {
            (void)disk_[i]->statistics();
        }

        // 计算raid组的磁盘剩余，包括已经满的磁盘
        if(SVS_DISK_ERR_DISK_FULL >= disk_[i]->flag())
        {
            // KB化成MB
            raid_free_size_ += (uint32_t)(disk_[i]->free_size() / 1024);
        }
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : select_disk
 功能描述  : 轮循选择一个状态最好的磁盘。根据输入的起始磁盘索引号和结束磁盘索引号
             顺序选择一个状态最好的磁盘，输入的磁盘索引可以是:总盘数为3个，输入的
             参数磁盘索引是1，3，那么磁盘选择顺序是第1,2,0块磁盘
 输入参数  : int32_t nStart:磁盘起始索引号
             int32_t nEnd:磁盘结束索引号,允许大于总磁盘数
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t CAC_Record_Raid::select_disk(int32_t nStart, int32_t nEnd)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    SVS_LOG((SVS_LM_DEBUG,
        "Start select disk.nStart[%d]~nEnd[%d].total disk[%d],output disk index[%d]",
        nStart,
        nEnd,
        disk_total_,
        output_disk_));

    if(0 >= disk_total_)
    {
        return SVS_RESULT_OK;
    }

    (void)statistics();

    bool diskUseableFlag = false;
    int32_t m = 0;
    int32_t diskIndex = 0;

    // 如果当前正在使用的磁盘可用，则不用选其它盘
    if ((0 <= output_disk_) && (SVS_DISK_ERR_OK == this->disk_[output_disk_]->flag()))
    {
        raid_err_type_ = SVS_DISK_ERR_OK;

        // 获取输出线程
        allocOutputThread();

        return SVS_RESULT_OK;
    }

    // 如果当前raid组是第一次选择磁盘，则置第一个磁盘为所选择的磁盘
    if(0 > output_disk_)
    {
        output_disk_ = 0;
    }

    // 选择第1个空闲磁盘分区
    m = nStart;
    for (; m <= nEnd; m++)
    {
        diskIndex = m % disk_total_;
        if (SVS_DISK_ERR_OK == disk_[diskIndex]->flag())
        {
            diskUseableFlag = true;
            output_disk_ = diskIndex;
            raid_err_type_ = SVS_DISK_ERR_OK;

            break;
        }
    }

    // 2.二轮选择小于告警恢复阈值的磁盘
    if (!diskUseableFlag)
    {
        //选择第1个空闲磁盘分区
        m = nStart;
        for (; m <= nEnd; m++)
        {
            diskIndex = m % disk_total_;
            if (SVS_DISK_ERR_DISK_ALARM_RESUME == disk_[diskIndex]->flag())
            {
                SVS_LOG((SVS_LM_INFO, "Select a alarm resume"));
                diskUseableFlag = true;
                output_disk_ = diskIndex;
                raid_err_type_ = SVS_DISK_ERR_RAID_ALARM_RESUME;

                break;
            }
        }
    }

    // 3.三轮选择正在发生告警的磁盘
    if (!diskUseableFlag)
    {
        //选择第1个空闲磁盘分区
        m = nStart;
        for (; m <= nEnd; m++)
        {
            diskIndex = m % disk_total_;
            if (SVS_DISK_ERR_DISK_ALARM == disk_[diskIndex]->flag())
            {
                diskUseableFlag = true;
                output_disk_ = diskIndex;
                raid_err_type_ = SVS_DISK_ERR_RAID_ALARM;

                break;
            }
        }
    }

    // 4.没得可用的磁盘，返回失败
    if (!diskUseableFlag || (m > nEnd))
    {
        raid_err_type_ = SVS_DISK_ERR_RAID_FULL;

        SVS_LOG((SVS_LM_ERROR,
            "All disks in this raid are full. raid path[%s],total disk num[%d].",
            raid_path_,
            disk_total_));
        // 释放线程id
        freeOutputThread();
        return SVS_ERR_DISKFAULT;
    }

    // 获取输出线程
    allocOutputThread();
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : check_disk
 功能描述  : 全面检查磁盘，该函数在磁盘被检测到异常活当前磁盘属于异常的时候，
             才调用该检查方法
 输入参数  : const int32_t diskIndex: 磁盘在该raid组中的索引
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t CAC_Record_Raid::check_disk(const int32_t diskIndex)
{
    if((0 > diskIndex) || (disk_total_ <= diskIndex))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Check disk failed.The parameter is invalid."
            "raid path[%s],diskIndex[%d],total disk[%d].",
            raid_path_,
            diskIndex,
            disk_total_));
        return SVS_ERR_PARAM;
    }


    return disk_[diskIndex]->check_disk();
}

/*****************************************************************************
 函 数 名  : get_all_disk_info
 功能描述  : 获取所有没有坏掉的磁盘路径信息
 输入参数  : NA
 输出参数  : DISK_INFO_VECTOR & diskInfoVector:磁盘路径列表
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t CAC_Record_Raid::get_all_disk_info(DISK_INFO_VECTOR & diskInfoVector)const
{
    diskInfoVector.clear();
    DISK_INFO diskInfo;
    for (int32_t i = 0; i < disk_total_; i++)
    {
        // 如果磁盘坏了，则不用返回
        if (SVS_DISK_ERR_DISK_BAD <= this->disk_[i]->flag())
        {
            continue;
        }

        memset(diskInfo.DiskPath, 0, sizeof(diskInfo.DiskPath));
        memcpy(diskInfo.DiskPath, this->disk_[i]->path(), (uint32_t)this->disk_[i]->path_len());

        diskInfoVector.push_back(diskInfo);
    }

    return SVS_RESULT_OK;
}

int32_t CAC_Record_Raid::debug_raid_tail_info(char * strBuff, uint32_t buffLen)const
{
    size_t writeLen = 0;
    uint32_t leaveLen = 0;
    CAC_Record_Disk * pstDisk = NULL;
    double unitG2K = RTRECORD_UNIT_K * RTRECORD_UNIT_K;
    double totalSize ,freeSize = 0;

    (void)ACE_OS::snprintf(   strBuff,
        buffLen,
        "\tDiskId  DiskErr MountFlag RealTimeFalg TotalSize(GB)  FreeSize(GB)   Used(%)    Path \n");

    for (int32_t i = 0; i < disk_total_; i++)
    {
        writeLen = ACE_OS::strlen(strBuff);
        //缓冲区已用完，退出循环
        leaveLen = buffLen-writeLen;
        if( 1 >= leaveLen )
        {
            break;
        }
        pstDisk = disk_[i];
        totalSize = pstDisk->total_size()/unitG2K;
        freeSize = pstDisk->free_size()/unitG2K;

        int32_t metaDataPartedType = SVS_META_DATA_DISPARTED;
        if(pstDisk->getMedaDataPartedFlag())
        {
            metaDataPartedType = SVS_META_DATA_PARTED;
        }
        (void)ACE_OS::snprintf(   &strBuff[writeLen],
            leaveLen,
            "\t [%d]     [%d]       [%d]        [%d]      [%12.3f] [%12.3f] [%5.2f]PER [%s]\n",
            i,
            pstDisk->flag(),
            pstDisk->type(),
            metaDataPartedType,
            totalSize,
            freeSize,
            (0 == totalSize)? 0: ((totalSize - freeSize) * 100) / totalSize,
            pstDisk->path());
    }

    return SVS_RESULT_OK;
}
// telnet获取写录像线程上的镜头信息
int32_t CAC_Record_Raid::debug_write_thread_load(char * strBuff, uint32_t buffLen)const
{
    size_t writeLen = 0;
    uint32_t leaveLen = 0;

    (void)ACE_OS::snprintf(   strBuff,
        buffLen,
        "\tWriteThreadId  CameraLoadNum \n");

    uint32_t threadNumPerRaidGroup = CAC_RT_Record_Server::instance()->output_thread_num_per_raid();
    for (uint32_t i = 0; i < threadNumPerRaidGroup; i++)
    {
        writeLen = ACE_OS::strlen(strBuff);
        //缓冲区已用完，退出循环
        leaveLen = buffLen-writeLen;
        if( 1 >= leaveLen )
        {
            break;
        }
        (void)ACE_OS::snprintf(   &strBuff[writeLen],
            leaveLen,
            "\t [%u]            [%u] \n",
            i,
            outputThreadLoad_[i]);
    }

    return SVS_RESULT_OK;
}


int32_t CAC_Record_Raid::increase_camera_num(char * strCameraId, int32_t IoQueueIndex)
{
    int32_t threadNumPerRaidGroup = (int32_t)CAC_RT_Record_Server::instance()->output_thread_num_per_raid();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    // 增加线程负载值
    int32_t threadIndex = IoQueueIndex - output_thread_start_id_;

    // 判断线程ID的有效性
    if((0 <= threadIndex) && (threadIndex < threadNumPerRaidGroup))
    {
        // 增加该线程负载值
        ++outputThreadLoad_[threadIndex];
    }
    if(0 != camera_map_.bind(ACE_CString(strCameraId), 0))
    {
        SVS_LOG((SVS_LM_ERROR, "bind camera to raid failed .cameraId[%s]", strCameraId));
        return SVS_RESULT_FAILURE;
    }

    camera_num_++;
    SVS_LOG((SVS_LM_INFO,
            "bind camera to raid OK, camera number[%d], cameraid[%s]",
            camera_num_,
            strCameraId));

    return SVS_RESULT_OK;
}

int32_t CAC_Record_Raid::decrease_camera_num(char * strCameraId, int32_t IoQueueIndex)
{
    int32_t threadNumPerRaidGroup = (int32_t)CAC_RT_Record_Server::instance()->output_thread_num_per_raid();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    // 增加线程负载值
    int32_t threadIndex = IoQueueIndex - output_thread_start_id_;

    // 判断线程ID的有效性。
    if((0 <= threadIndex) && (threadIndex < threadNumPerRaidGroup))
    {
        // 减小该线程负载值
        --outputThreadLoad_[threadIndex];
    }
    if(0 != camera_map_.unbind(ACE_CString(strCameraId)))
    {
        SVS_LOG((SVS_LM_ERROR, "unbind camera from raid failed .cameraId[%s]", strCameraId));
        return SVS_RESULT_FAILURE;
    }

    camera_num_--;
    SVS_LOG((SVS_LM_INFO,
            "unbind camera from raid OK, camera number[%d], cameraid[%s]",
            camera_num_,
            strCameraId));

    return SVS_RESULT_OK;
}
// 刚刚负载上来的镜头获取所使用的输出线程ID.当前组内负载最轻的线程加上本raid组的起始线程ID,
// 就是要分配的线程在系统的ID值
int32_t CAC_Record_Raid::getCurrOutputThreadIndex(uint32_t &ulThreadIndex)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    if (0 > output_thread_start_id_)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Get raid current ouput thread index failed."
            "output_thread_start_id[%d]",
            output_thread_start_id_));
        return SVS_RESULT_FAILURE;
    }

    uint32_t threadNumPerRaidGroup = CAC_RT_Record_Server::instance()->output_thread_num_per_raid();
    uint32_t minLoadThreadIndex = 0;
    for(uint32_t i = 0; i < threadNumPerRaidGroup; i++)
    {
        // 如果有一个负载更轻的线程，则选用该线程
        if(outputThreadLoad_[i] < outputThreadLoad_[minLoadThreadIndex])
        {
            minLoadThreadIndex = i;
        }
    }

    ulThreadIndex = (uint32_t)output_thread_start_id_ + minLoadThreadIndex;

    return SVS_RESULT_OK;
}

// raid组获取的输出线程
void CAC_Record_Raid::allocOutputThread()
{
    // 已经有可用线程，则不处理
    if(0 <= output_thread_start_id_)
    {
        return;
    }

    uint32_t ulThreadIndex = 0;
    int32_t nRet =  SVS_Disk_Manager::instance().allocOutputThreadIndex(ulThreadIndex);
    if(SVS_RESULT_OK != nRet)
    {
        raid_err_type_ = SVS_DISK_ERR_RAID_NO_OUTPUT_THREAD;
        return;
    }

    memset(outputThreadLoad_, 0, sizeof(outputThreadLoad_));
    output_thread_start_id_ = (int32_t)ulThreadIndex;
}

// 释放raid组的输出线程
void CAC_Record_Raid::freeOutputThread()
{
    if (0 <= output_thread_start_id_)
    {
        SVS_Disk_Manager::instance().freeOutputThreadIndex((uint32_t)output_thread_start_id_);
        output_thread_start_id_ = -1;
        memset(outputThreadLoad_, 0, sizeof(outputThreadLoad_));
    }

    return;
}

int32_t CAC_Record_Raid::statCamreaListOnRaid(char * strBuff, uint32_t buffLen)
{
    size_t writeLen = 0;
    uint32_t leaveLen = 0;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    CAMERA_MAP_ITER iter(camera_map_);
    CAMERA_MAP_ENTRY *pEntry = NULL;
    uint32_t camraNum = camera_map_.current_size();
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        writeLen = ACE_OS::strlen(strBuff);
        //缓冲区已用完，退出循环
        leaveLen = buffLen-writeLen;
        if( 20 >= leaveLen )
        {
            break;
        }
        (void)ACE_OS::snprintf(   &strBuff[writeLen],
            leaveLen,
            "\t camrea[%s]\n",
            pEntry->ext_id_.c_str());
    }

    writeLen = ACE_OS::strlen(strBuff);
    //缓冲区已用完，退出循环
    leaveLen = buffLen-writeLen;
    if( 20 >= leaveLen )
    {
        return SVS_RESULT_OK;
    }

    (void)ACE_OS::snprintf(   &strBuff[writeLen],
            leaveLen,
            "\t number [%u]",
            camraNum);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : refreshDiskInfo
 功能描述  : 刷新磁盘属性。
             如果发现传入的vector某磁盘的路径和raid组中的某个磁盘路径相同，则认为
             是同一个磁盘，更新改磁盘的mount属性和元数据分离属性
 输入参数  : diskInfoVec:所配置的所有的磁盘列表
 输出参数  : NA
 返 回 值  : 成功返回 0,失败返回 -1
*****************************************************************************/
int32_t CAC_Record_Raid::refreshDiskInfo(const DISK_INFO_VECTOR &diskInfoVec)const
{
    bool bFindRet = false;
    uint32_t diskNum = diskInfoVec.size();

    for(int32_t i = 0; i < disk_total_; i++)
    {
        bFindRet = false;
        uint32_t vectLoop = 0;
        for(; vectLoop < diskNum; vectLoop++)
        {
            const DISK_INFO* pDiskInfo = &diskInfoVec[vectLoop];
            // 如果找到磁盘，设置磁盘新的属性
            if(0 == strcmp(pDiskInfo->DiskPath, disk_[i]->path()))
            {
                (void)disk_[i]->setMedaDataPartedFlag(pDiskInfo->MedaDataPartedFlag);
                bFindRet = true;

                // 跳出本次vector遍历
                break;
            }
        }

        // 如果在vector里找不到当前正在使用的这个磁盘，说明该磁盘下线，设置为不可用状态
        if(!bFindRet)
        {
            (void)disk_[i]->setDiskFlag(SVS_DISK_ERR_DISK_OFFLINE_CONF);
        }
        // 如果找到了磁盘，并且原来的磁盘状态是下线状态，则修改磁盘为上线，不可用状态(不可用就OK)
        else if(SVS_DISK_ERR_DISK_OFFLINE_CONF == disk_[i]->flag())
        {
            (void)disk_[i]->setDiskFlag(SVS_DISK_ERR_DISK_BAD);
        }
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : tryAddDisk
 功能描述  : 尝试添加磁盘。如果当前raid组中有该磁盘，则不用添加，退出函数，
             否则添加一个新的磁盘。
 输入参数  : const DISK_INFO & szDiskInfo:磁盘信息
 输出参数  : NA
 返 回 值  : 成功返回 0,失败返回 -1
*****************************************************************************/
int32_t CAC_Record_Raid::tryAddDisk(const DISK_INFO & szDiskInfo)
{
    for(int32_t i = 0; i < disk_total_; i++)
    {
        // 如果找到磁盘，则不用添加磁盘
        if(0 == strcmp(szDiskInfo.DiskPath, disk_[i]->path()))
        {
            return SVS_RESULT_OK;
        }
    }

    // 到这里则说明没有找到磁盘，添加一个新的磁盘对象
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    // 如果raid允许的磁盘路径个数已经达到最大，则添加失败
    if(SVS_LUN_MAX_NUM <= disk_total_)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Add disk failed. disk max number in a raid[%d], current disk number[%d].",
            SVS_LUN_MAX_NUM,disk_total_));

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Begin add disk.raid path[%s], new disk path[%s].",
        raid_path_, szDiskInfo.DiskPath));
    // 创建磁盘对象
    try
    {
        this->disk_[disk_total_] = new CAC_Record_Disk(szDiskInfo.DiskPath,
                                                        (size_t)disk_total_,
                                                        disk_threshold_);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Add disk failed. Create disk object failed.raid path[%s], new disk path[%s].",
            raid_path_, szDiskInfo.DiskPath));

        return SVS_ERR_NOMEM;
    }

    // 设置和检测新添加磁盘的属性
    (void)disk_[disk_total_]->setMedaDataPartedFlag(szDiskInfo.MedaDataPartedFlag);
    (void)disk_[disk_total_]->setMountFlag(szDiskInfo.DiskMountTypeFlag);
    (void)disk_[disk_total_]->statistics();

    uint32_t raidPathLen = strlen(raid_path_);
    (void)snprintf(&raid_path_[raidPathLen],
                   sizeof(raid_path_) - raidPathLen,"%s," ,
                   szDiskInfo.DiskPath);

    ++disk_total_;

    SVS_LOG((SVS_LM_WARNING, "success to add disk.raid path[%s], new diskPath[%s].",
        raid_path_, szDiskInfo.DiskPath));

    return SVS_RESULT_OK;
}
//磁盘的最好状态
int32_t CAC_Record_Raid::GetDiskBestStatus()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    CAC_Record_Disk * pstDisk = NULL;
    int32_t nBestDiskStatus = SVS_DISK_ERR_TYPE_MAX;
    for (int32_t i = 0; i < disk_total_; i++)
    {
        pstDisk = disk_[i];
        if(nBestDiskStatus > pstDisk->flag())
        {
            nBestDiskStatus = pstDisk->flag();
        }
    }
    return nBestDiskStatus;
}
//================================CAC_Record_Raid END===================================


