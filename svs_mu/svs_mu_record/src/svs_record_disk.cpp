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
 �� �� ��  : statistics
 ��������  : �����̿ռ���ļ�ϵͳ��д����
 �������  : uint32_t lowerLimit:ֹͣ¼��ʱ�Ĵ���ʣ��ռ���ֵ
             uint32_t alarmLimit:�澯ʱ�Ĵ���ʣ��ռ���ֵ
             uint32_t alarmResumeLimit:�澯�ָ�ʱ�Ĵ���ʣ��ռ���ֵ
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t CAC_Record_Disk::statistics()
{
    // ������̲��������б����ü��
    if(SVS_DISK_ERR_DISK_OFFLINE_CONF == flags_)
    {
        return SVS_RESULT_OK;
    }

    // �����mount�����Ĵ��̣��鿴mount�Ƿ������
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

    // ���õĴ��̿ռ�,ע��:�������ת������߼��ḳֵΪ0
    uint64_t usableSize = this->free_size_ - disk_threshold_.m_lowerLimit;

    if(SVS_DISK_ERR_DISK_FULL == this->flags_)
    {
        usableSize = 0;
    }

    // ���������Ŀ¼�����ü�����̿ռ�ͳ��
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
 �� �� ��  : check_disk
 ��������  : ȫ�������,��ǳ��������̣�ֱ�����������쳣��������û���쳣
 �������  : uint32_t lowerLimit:ֹͣ¼��ʱ�Ĵ���ʣ��ռ���ֵ
             uint32_t alarmLimit:�澯ʱ�Ĵ���ʣ��ռ���ֵ
             uint32_t alarmResumeLimit:�澯�ָ�ʱ�Ĵ���ʣ��ռ���ֵ
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t CAC_Record_Disk::check_disk()
{
    // 1.�ȼ򵥼��,���̿ռ���ļ�ϵͳֻ��
    (void)statistics();

    // ����Ѿ�����������Ȼ�쳣����������������
    if(SVS_DISK_ERR_DISK_BAD <= flags_)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Disk is unusable.disk path[%s], error type[%d].",
            str_path_,
            flags_));

        return SVS_RESULT_OK;
    }

    //1 TODO �����м��ⷽ����������......

    // ����Դ���һ���ļ�����дһЩ���ݣ��������Ƿ�����
    char filePath[MAXNAMELEN + 1] = {0,};
    (void)snprintf(filePath, sizeof(filePath),"%s/diskTest",str_path_);

    // ��ģʽ
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

    // ���Ԫ���ݺ���Ƶ���ݷ���
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

            // �ͷ���Դ���ر��ļ�����ɾ����ʱ�ļ�
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

            // �ͷ���Դ���ر��ļ�����ɾ����ʱ�ļ�
            (void)ACE_OS::close((ACE_HANDLE)fd);
            (void)ACE_OS::unlink(filePath);

            disk_stat_.updateDiskSize((uint64_t)0,(uint64_t)0);

            return SVS_ERR_DISKFAULT;
        }
    }

    // �򿪳ɹ��󣬳���д����
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

    // ��4K����,����4K���봦��
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

        // �ͷ���Դ���ر��ļ�����ɾ����ʱ�ļ�
        (void)ACE_OS::close((ACE_HANDLE)fd);
        (void)ACE_OS::unlink(filePath);
        disk_stat_.updateDiskSize((uint64_t)0,(uint64_t)0);

        return SVS_ERR_DISKFAULT;
    }

    // �򿪺�д�ļ����ɹ��ˣ���ʾ��������
    // �ͷ���Դ���ر��ļ�����ɾ����ʱ�ļ�
    (void)ACE_OS::close((ACE_HANDLE)fd);
    (void)ACE_OS::unlink(filePath);

    SVS_LOG((SVS_LM_INFO, "Check disk finished.Disk is OK.disk path[%s]",str_path_));

    return SVS_RESULT_OK;
}

// ����״̬ת��
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

// ���ô���״̬
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

// ���ô��̵������Ƿ�֧��Ԫ���ݷ���
int32_t CAC_Record_Disk::setMedaDataPartedFlag(bool medaDataPartedFlag)
{
    SVS_LOG((SVS_LM_DEBUG,
        "old realtime flag[%d], new realtime flag[%d].",
        meda_data_parted_flag_,
        (int32_t)medaDataPartedFlag));

    ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, -1);
    // �������Ԫ���ݷ����ʶ�仯�ˣ������¸�ֵ
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

// ���ô��̵�mount״̬
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
        // Ŀ¼����
        if((ENOENT == nErrCode) || (ENOTDIR == nErrCode))
        {
            this->flags_ = SVS_DISK_ERR_DISK_BAD;
            iRet = SVS_ERR_DISKFAULT;
        }
        else if(EIO == nErrCode)// �ļ�ϵͳ����
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
        // Ŀ¼����
        if((ENOENT == nErrCode) || (ENOTDIR == nErrCode))
        {
            this->flags_ = SVS_DISK_ERR_DISK_BAD;
            iRet = SVS_ERR_DISKFAULT;
        }
        else if(EIO == nErrCode)// �ļ�ϵͳ����
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

     // �ļ�ϵͳֻ��
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
 �� �� ��  : init
 ��������  : ��ʼ��Raid�����������õĸ�raid������д��̣���������õĴ������зǷ�
             �Ĵ������ʼ��ʧ��
 �������  : char * strRaidPath: raid��·������
             uint32_t lowerLimit:ֹͣ¼��ʱ�Ĵ���ʣ��ռ���ֵ
             uint32_t alarmLimit:�澯ʱ�Ĵ���ʣ��ռ���ֵ
             uint32_t alarmResumeLimit:�澯�ָ�ʱ�Ĵ���ʣ��ռ���ֵ
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t CAC_Record_Raid::init(uint32_t lowerLimit,
                          uint32_t alarmLimit,
                          uint32_t alarmResumeLimit)
{
    // ��λ����KB������ת��
    this->disk_threshold_.m_lowerLimit = lowerLimit;
    this->disk_threshold_.m_alarmLimit = alarmLimit;
    this->disk_threshold_.m_alarmResumeLimit = alarmResumeLimit;

    raid_load_ = 0;
    disk_total_ = 0;
    output_disk_ = -1;

    // ��ʶ���̲�����
    raid_err_type_ = SVS_DISK_ERR_RAID_FULL;

    //�������̷�������
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
 �� �� ��  : statistics
 ��������  : ͳ�ƴ��������д�����Ϣ�����ڴ����쳣��¼����̣����и��ϸ�ļ��
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t CAC_Record_Raid::statistics()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    raid_free_size_ = 0;

    for(int32_t i = 0; i < disk_total_; i++)
    {
        // �Ѿ��쳣�Ĵ��̣�����ȫ��
        if(SVS_DISK_ERR_DISK_BAD <= disk_[i]->flag())
        {
            (void)disk_[i]->check_disk();
        }
        else
        {
            (void)disk_[i]->statistics();
        }

        // ����raid��Ĵ���ʣ�࣬�����Ѿ����Ĵ���
        if(SVS_DISK_ERR_DISK_FULL >= disk_[i]->flag())
        {
            // KB����MB
            raid_free_size_ += (uint32_t)(disk_[i]->free_size() / 1024);
        }
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : select_disk
 ��������  : ��ѭѡ��һ��״̬��õĴ��̡������������ʼ���������źͽ�������������
             ˳��ѡ��һ��״̬��õĴ��̣�����Ĵ�������������:������Ϊ3���������
             ��������������1��3����ô����ѡ��˳���ǵ�1,2,0�����
 �������  : int32_t nStart:������ʼ������
             int32_t nEnd:���̽���������,��������ܴ�����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
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

    // �����ǰ����ʹ�õĴ��̿��ã�����ѡ������
    if ((0 <= output_disk_) && (SVS_DISK_ERR_OK == this->disk_[output_disk_]->flag()))
    {
        raid_err_type_ = SVS_DISK_ERR_OK;

        // ��ȡ����߳�
        allocOutputThread();

        return SVS_RESULT_OK;
    }

    // �����ǰraid���ǵ�һ��ѡ����̣����õ�һ������Ϊ��ѡ��Ĵ���
    if(0 > output_disk_)
    {
        output_disk_ = 0;
    }

    // ѡ���1�����д��̷���
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

    // 2.����ѡ��С�ڸ澯�ָ���ֵ�Ĵ���
    if (!diskUseableFlag)
    {
        //ѡ���1�����д��̷���
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

    // 3.����ѡ�����ڷ����澯�Ĵ���
    if (!diskUseableFlag)
    {
        //ѡ���1�����д��̷���
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

    // 4.û�ÿ��õĴ��̣�����ʧ��
    if (!diskUseableFlag || (m > nEnd))
    {
        raid_err_type_ = SVS_DISK_ERR_RAID_FULL;

        SVS_LOG((SVS_LM_ERROR,
            "All disks in this raid are full. raid path[%s],total disk num[%d].",
            raid_path_,
            disk_total_));
        // �ͷ��߳�id
        freeOutputThread();
        return SVS_ERR_DISKFAULT;
    }

    // ��ȡ����߳�
    allocOutputThread();
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : check_disk
 ��������  : ȫ������̣��ú����ڴ��̱���⵽�쳣�ǰ���������쳣��ʱ��
             �ŵ��øü�鷽��
 �������  : const int32_t diskIndex: �����ڸ�raid���е�����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
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
 �� �� ��  : get_all_disk_info
 ��������  : ��ȡ����û�л����Ĵ���·����Ϣ
 �������  : NA
 �������  : DISK_INFO_VECTOR & diskInfoVector:����·���б�
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t CAC_Record_Raid::get_all_disk_info(DISK_INFO_VECTOR & diskInfoVector)const
{
    diskInfoVector.clear();
    DISK_INFO diskInfo;
    for (int32_t i = 0; i < disk_total_; i++)
    {
        // ������̻��ˣ����÷���
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
        //�����������꣬�˳�ѭ��
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
// telnet��ȡд¼���߳��ϵľ�ͷ��Ϣ
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
        //�����������꣬�˳�ѭ��
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
    // �����̸߳���ֵ
    int32_t threadIndex = IoQueueIndex - output_thread_start_id_;

    // �ж��߳�ID����Ч��
    if((0 <= threadIndex) && (threadIndex < threadNumPerRaidGroup))
    {
        // ���Ӹ��̸߳���ֵ
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
    // �����̸߳���ֵ
    int32_t threadIndex = IoQueueIndex - output_thread_start_id_;

    // �ж��߳�ID����Ч�ԡ�
    if((0 <= threadIndex) && (threadIndex < threadNumPerRaidGroup))
    {
        // ��С���̸߳���ֵ
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
// �ոո��������ľ�ͷ��ȡ��ʹ�õ�����߳�ID.��ǰ���ڸ���������̼߳��ϱ�raid�����ʼ�߳�ID,
// ����Ҫ������߳���ϵͳ��IDֵ
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
        // �����һ�����ظ�����̣߳���ѡ�ø��߳�
        if(outputThreadLoad_[i] < outputThreadLoad_[minLoadThreadIndex])
        {
            minLoadThreadIndex = i;
        }
    }

    ulThreadIndex = (uint32_t)output_thread_start_id_ + minLoadThreadIndex;

    return SVS_RESULT_OK;
}

// raid���ȡ������߳�
void CAC_Record_Raid::allocOutputThread()
{
    // �Ѿ��п����̣߳��򲻴���
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

// �ͷ�raid�������߳�
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
        //�����������꣬�˳�ѭ��
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
    //�����������꣬�˳�ѭ��
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
 �� �� ��  : refreshDiskInfo
 ��������  : ˢ�´������ԡ�
             ������ִ����vectorĳ���̵�·����raid���е�ĳ������·����ͬ������Ϊ
             ��ͬһ�����̣����¸Ĵ��̵�mount���Ժ�Ԫ���ݷ�������
 �������  : diskInfoVec:�����õ����еĴ����б�
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
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
            // ����ҵ����̣����ô����µ�����
            if(0 == strcmp(pDiskInfo->DiskPath, disk_[i]->path()))
            {
                (void)disk_[i]->setMedaDataPartedFlag(pDiskInfo->MedaDataPartedFlag);
                bFindRet = true;

                // ��������vector����
                break;
            }
        }

        // �����vector���Ҳ�����ǰ����ʹ�õ�������̣�˵���ô������ߣ�����Ϊ������״̬
        if(!bFindRet)
        {
            (void)disk_[i]->setDiskFlag(SVS_DISK_ERR_DISK_OFFLINE_CONF);
        }
        // ����ҵ��˴��̣�����ԭ���Ĵ���״̬������״̬�����޸Ĵ���Ϊ���ߣ�������״̬(�����þ�OK)
        else if(SVS_DISK_ERR_DISK_OFFLINE_CONF == disk_[i]->flag())
        {
            (void)disk_[i]->setDiskFlag(SVS_DISK_ERR_DISK_BAD);
        }
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : tryAddDisk
 ��������  : ������Ӵ��̡������ǰraid�����иô��̣�������ӣ��˳�������
             �������һ���µĴ��̡�
 �������  : const DISK_INFO & szDiskInfo:������Ϣ
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
*****************************************************************************/
int32_t CAC_Record_Raid::tryAddDisk(const DISK_INFO & szDiskInfo)
{
    for(int32_t i = 0; i < disk_total_; i++)
    {
        // ����ҵ����̣�������Ӵ���
        if(0 == strcmp(szDiskInfo.DiskPath, disk_[i]->path()))
        {
            return SVS_RESULT_OK;
        }
    }

    // ��������˵��û���ҵ����̣����һ���µĴ��̶���
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    // ���raid����Ĵ���·�������Ѿ��ﵽ��������ʧ��
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
    // �������̶���
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

    // ���úͼ������Ӵ��̵�����
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
//���̵����״̬
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


