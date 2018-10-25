/******************************************************************************

                  版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文 件 名   : SVS_Disk_Manager.cpp
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
#include "svs_timer.h"
#include "svs_stat_manager.h"
#include "svs_rt_record_common.h"
#include "svs_daemon_thread.h"
#include "svs_real_record_server.h"
#include "svs_record_disk.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"

// extern int32_t g_alarmStatus[ALARM_INDEX_NUM];

SVS_Disk_Manager::SVS_Disk_Manager()
{
    // 线程运行标识
    m_bThreadFlag = false;

    // 周期性磁盘检测定时器
    m_checkDiskTimer = -1;

    // raid组总数
    m_raidTotal = 0;

    // 检查周期，单位：秒
    m_checkInterval = 0;

    // 磁盘可用空间下限，单位KB
    m_lowerLimit = 0;

    // 磁盘告警阈值，单位KB
    m_alarmLimit = 0;

    // 磁盘告警恢复值，单位KB
    m_alarmResumeLimit = 0;

    (void)memset(m_mountsInfo, 0, sizeof(m_mountsInfo));
    (void)memset(m_fileList, 0, sizeof(m_fileList));

    m_bIsMountsOk = false;

    m_diskFullStartFlag = false;
    m_vfdCursor = 0;
    m_freeOutputThreadIndexList.clear();

    m_IndexFileDisk = NULL;

    m_strDiskCheckBuff = NULL;
}

SVS_Disk_Manager::~SVS_Disk_Manager()
{
    try
    {
        if (!m_bThreadFlag)
        {
            m_bThreadFlag = true;
            // 释放定时器
            if(0 <= m_checkDiskTimer)
            {
                (void)CAC_Timer_Manager::instance()->free_timer(m_checkDiskTimer);
                m_checkDiskTimer = -1;
            }
        }

        if(NULL != m_IndexFileDisk)
        {
            SVS_DELETE(m_IndexFileDisk);
        }

        if(NULL != m_strDiskCheckBuff)
        {
            SVS_DELETE(m_strDiskCheckBuff, SVS_DELETE_MULTI);
        }

    }
    catch(...)
    {

    }
}

/*****************************************************************************
 函 数 名  : init
 功能描述  : 初始化磁盘管理模块，解析传入的配置的磁盘全路径，根据解析结果创建raid
             组和每个raid组下的磁盘。根据传入的磁盘检测周期，周期性检测磁盘。根据
             传入的磁盘空间管理阈值进行管理磁盘
 输入参数  : int32_t nInterval :磁盘检测周期
             uint32_t lowerLimit : 磁盘停止录像的阈值
             uint32_t alarmLimit : 磁盘剩余空间告警的阈值
             uint32_t alarmResumeLimit : 磁盘剩余空间告警恢复值
 输出参数  : NA
 返 回 值  : 成功返回 0,失败返回 -1
*****************************************************************************/
int32_t SVS_Disk_Manager::initialize(int32_t nInterval ,
                               uint32_t lowerLimit,
                               uint32_t alarmLimit ,
                               uint32_t alarmResumeLimit)
{
    // 申请磁盘检测用的缓存区，已经是增加了4K对齐的
    uint32_t ulBuffLen = CAC_RT_Record_Server::instance()->recv_media_buffer_size();
    m_strDiskCheckBuff = SVS_NEW(m_strDiskCheckBuff, ulBuffLen);
    if(NULL == m_strDiskCheckBuff)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Initialize disk manager failed.Allocate buffer for disk check failed."));

        return SVS_RESULT_FAILURE;
    }



    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, -1);
    m_bThreadFlag = true;
    m_checkInterval = nInterval;
    m_raidTotal = 0;

    //从M换算为K
    m_lowerLimit = lowerLimit * 1024;
    m_alarmLimit = alarmLimit * 1024;
    m_alarmResumeLimit = alarmResumeLimit * 1024;

    // 创建索引文件对象
    DISK_THRESHOLD diskThreshold;
    diskThreshold.m_lowerLimit = RECORD_INDEX_FILE_DISK_LIMIT;
    diskThreshold.m_alarmLimit = RECORD_INDEX_FILE_DISK_LIMIT;
    diskThreshold.m_alarmResumeLimit = RECORD_INDEX_FILE_DISK_LIMIT;
    const char* strIndexpath= CAC_RT_Record_Server::instance()->get_record_indexfile_path();
    // 数据磁盘每个raid组最大SVS_LUN_MAX_NUM个磁盘，小标最大为SVS_LUN_MAX_NUM-1,用SVS_LUN_MAX_NUM标识索引路径
    try
    {
        m_IndexFileDisk = new CAC_Record_Disk(strIndexpath, SVS_LUN_MAX_NUM, diskThreshold);
    }catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Allocate memory failed.Create index file disk object failed."));
        return SVS_ERR_NOMEM;
    }

    // 设置和检测新添加磁盘的属性
    (void)m_IndexFileDisk->setMedaDataPartedFlag(false);

    getMounts();
    if(SVS_RESULT_OK == checkMountOnlineStatus(strIndexpath))
    {
        (void)m_IndexFileDisk->setMountFlag(SVS_DISK_TYPE_MOUNTDISK);
    }
    else
    {
        (void)m_IndexFileDisk->setMountFlag(SVS_DISK_TYPE_LOCALDISK);
    }

    // 对索引磁盘进行检测
    (void)m_IndexFileDisk->statistics();
    if(SVS_DISK_ERR_DISK_FULL <= m_IndexFileDisk->flag())
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Initialize disk manager failed as record indexpath is unusable."
            "record indexfile path[%s], Indexpath disk limit is [%u]MB.",
            strIndexpath,
            RECORD_INDEX_FILE_DISK_LIMIT / 1024));

        return SVS_RESULT_FAILURE;
    }

    // 初始化文件列表
    // IO队列个数
//    int32_t ioQueueMaxNum = (int32_t)CAC_RT_Record_Server::instance()->output_thread_num_sys();
    SVS_DISK_VFILE * pFileList = NULL;
    for(int32_t i = 0; i < RECORD_MAX_LENS_NUM; i++)
    {
        pFileList = m_fileList + i;
        pFileList->VFd = i;
//        pFileList->IoQueueIndex = i % ioQueueMaxNum;
        pFileList->IoQueueIndex = -1;
        pFileList->SysFd = -1;
        pFileList->RaidIndex = -1;
        pFileList->DiskIndex = -1;
        pFileList->RecordFilePath[0] = '\0';
        pFileList->CameraId[0] = '\0';
        pFileList->VfdStatus = VFD_FREE_STATUS;
    }
    m_bThreadFlag = false;
    m_vfdCursor = 0;

    m_freeOutputThreadIndexList.clear();

    SVS_LOG((SVS_LM_INFO,
        "Initialize disk manager success.raid group num[%d].",
        m_raidTotal));
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : open
 功能描述  : 打开定期检测磁盘线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回 0,失败返回 -1
*****************************************************************************/
int32_t SVS_Disk_Manager::open(void *argv)
{
    if (NULL == argv)
    {
        ; //only for compile warnning
    }

    // 重新调整发送消息队列水位
    (void)msg_queue()->close();
    if (0 != msg_queue()->open(SVS_SEND_MSG_QUEUE_WM, SVS_SEND_MSG_QUEUE_WM))
    {
        SVS_LOG( (SVS_LM_ERROR, "Fail to open reactor register msg queue." ) );
        return SVS_ERR_QUEUE;
    }

    size_t stack_size = 128 * 1024;
    int32_t nRet = activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED,
                        1,
                        0,
                        ACE_DEFAULT_THREAD_PRIORITY,
                        -1,
                        0,
                        0,
                        0,
                        &stack_size,
                        0 );
    if (0 != nRet)
    {
        SVS_LOG((SVS_LM_ERROR, "Active check disk thread failed."));
    }

    // 创建定时检测磁盘定时器
    ACE_Time_Value tv((int32_t)CAC_RT_Record_Server::instance()->get_check_disk_interval(), 0);
    m_checkDiskTimer = CAC_Timer_Manager::instance()->alloc_timer(
                    &SVS_Disk_Manager::instance(),
                    &SVS_Disk_Manager::timerCallback,
                    1,
                    tv,
                    tv,
                    1);

    if ( 0 > m_checkDiskTimer )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Allocate check disk interval timer failed."));
        return SVS_RESULT_FAILURE;
    }

    // 启动IO调度线程
    if (SVS_RESULT_OK != SVS_IO_Scheduler::instance().open((void*)0))
    {
        SVS_LOG((SVS_LM_ERROR, "Open IO schedule service failed."));
        return SVS_RESULT_FAILURE;
    }

    {
        ACE_Guard<ACE_Mutex> locker(m_FreeOutputThreadIndexMutex);
        // 启动录像成功后,把输出线程ID加入空闲列表,值保存每个raid组可用的线程的起始ID
        uint32_t syncWorkRaidGroupNum = CAC_RT_Record_Server::instance()->sync_work_raid_group_num();
        for(uint32_t i = 0; i < syncWorkRaidGroupNum; i++)
        {
            uint32_t threadIndex = i * CAC_RT_Record_Server::instance()->output_thread_num_per_raid();
            m_freeOutputThreadIndexList.push_back(threadIndex);
        }
    }

    return nRet;
}

/*****************************************************************************
 函 数 名  : svc
 功能描述  : 磁盘检测任务处理线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回 0,失败返回 -1
*****************************************************************************/
int32_t SVS_Disk_Manager::svc(void)
{
    SVS_LOG( ( SVS_LM_WARNING, "The check disk thread start to running..." ) );
    g_p_msg_buffer->init();

    ACE_hthread_t currThreadID = (ACE_hthread_t)ACE_OS::thr_self();
    SVS_LOG((SVS_LM_WARNING, "Current thread ID[%u,%d]", currThreadID, currThreadID ));

    CThread_Stat_Reporter statReporter("CheckDiskThread");
    ACE_Time_Value tvDelay(1);
    ACE_Message_Block *pMsg = NULL;
    while (!m_bThreadFlag)
    {
        ACE_Time_Value tv = ACE_OS::gettimeofday() + tvDelay;
        if (-1 == getq(pMsg ,&tv))
        {
            statReporter.ReportStat(0);
            continue;
        }

        (void)processCheckDiskTask(pMsg);

        // 释放内存
        g_p_msg_buffer->free_mb(pMsg);

        statReporter.ReportStat();
    }
    g_p_msg_buffer->clear();
    SVS_LOG( ( SVS_LM_WARNING, "The check disk thread thread stop to run." ) );
    return 0;
}

/*****************************************************************************
 函 数 名  : close
 功能描述  : 关闭对象,析构前的最后一步操作
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回 0,失败返回 -1
*****************************************************************************/
int32_t SVS_Disk_Manager::close(u_long)
{
    m_bThreadFlag = true;

    // 释放定时器
    if(0 <= m_checkDiskTimer)
    {
        (void)CAC_Timer_Manager::instance()->free_timer(m_checkDiskTimer);
        m_checkDiskTimer = -1;
    }

    (void)msg_queue()->close();
    (void)wait();


    SVS_LOG((SVS_LM_INFO, "Close the check disk thread." ));
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : openFile
 功能描述  : 新建或者打开一个文件。选择一个负载最轻(镜头数最少)的raid组创建
             录像文件
 输入参数  : const char* filePath:录像文件相对路径，镜头/日期/录像文件名
 输出参数  : uint64_t &fileSize:当前录像文件大小
             int32_t &vfd:虚文件句柄
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::openFile(char * cameraId, time_t recordTime,
                                 char* fileExName, uint64_t & fileSize, int32_t & vfd,
                                 bool & medaDataPartedFlag)
{
    if((NULL == cameraId) || (RECORD_MAX_LENS_NUM <= vfd) || (NULL == fileExName))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Open file failed.the parameter is invalid.cameraId[0x%08x],fileExName[0x%08x],vfd[%d].",
            cameraId,
            fileExName,
            vfd));
        return SVS_ERR_PARAM;
    }

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, -1);
    int32_t minCameraRaidIndex = 0;

    int32_t isNewDisk = allocDisk(vfd, minCameraRaidIndex);
    if((SVS_RESULT_OK != isNewDisk) && (SVS_ERR_CHANGEDISK != isNewDisk))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Allocate disk or vfd resource failed. cameraId[%s],fileExName[%s],iRet[%d].",
            cameraId,
            fileExName,
            isNewDisk));
        return isNewDisk;
    }
    SVS_DISK_VFILE * pFileList = &m_fileList[vfd];
    CAC_Record_Raid* pstRecordRaid = &m_raidList[minCameraRaidIndex];
//    pFileList->SysFd = -1;
    pFileList->RaidIndex = minCameraRaidIndex;

    //设置磁盘索引为当前RAID组上的当前磁盘，不会出现在相同raid组上同时写不同的磁盘
    pFileList->DiskIndex = pstRecordRaid->output_disk();

    SVS_LOG((SVS_LM_INFO,
        "Allocate disk successful.cameraId[%s], fileExName[%s],isNewDisk[0x%04x],vfd[%d],raidIndex[%d],diskIndex[%d]",
        cameraId,
        fileExName,
        isNewDisk,
        vfd,
        pFileList->RaidIndex,
        pFileList->DiskIndex));

    char strRecordFileName[MAXNAMELEN + 1] = {0,};
    char strDay[RT_DATE_STR_LEN + 1] = {0,};
    time2RecodFileName(recordTime,strRecordFileName,sizeof(strRecordFileName),fileExName);
    memcpy(strDay, strRecordFileName,RT_DATE_STR_LEN);

    char diskPath[DISK_NAME_SIZE] = {0,};
    (void)snprintf(diskPath, sizeof(diskPath),pstRecordRaid->disk_path(pFileList->DiskIndex));
    // 创建磁盘目录
    int32_t iRet = createDir(diskPath , cameraId,strDay);
    if(SVS_RESULT_OK != iRet)
    {
        SVS_LOG((SVS_LM_ERROR ,
            "Open file failed.Create directory failed.file full path[%s].fileExName[%s],ret[%d]",
            pFileList->RecordFilePath,
            fileExName,
            iRet));
        (void)createDiskExceptMsg(minCameraRaidIndex,pFileList->DiskIndex,iRet);
        return iRet;
    }

    // 创建录像文件
    (void)snprintf(pFileList->RecordFilePath,
                   sizeof(pFileList->RecordFilePath),
                   "%s/%s/%s/%s",
                   diskPath,
                   cameraId,
                   strDay,
                   strRecordFileName);
    medaDataPartedFlag = pstRecordRaid->get_meda_data_parted_flag(pFileList->DiskIndex);
    int32_t fd = -1;
    iRet = createFile(pFileList->RecordFilePath,medaDataPartedFlag,fileSize,fd);
    if((SVS_RESULT_OK != iRet) || (-1 >= fd))
    {
        SVS_LOG((SVS_LM_ERROR ,
        "Open file failed.Create file fail.file full path[%s],fd[%d],ret[%d].",
        pFileList->RecordFilePath,
        fd,
        iRet));

        // 如果是stat文件失败，则不用上报磁盘错误
        if(SVS_ERR_STATFILE != iRet)
        {
            (void)createDiskExceptMsg(minCameraRaidIndex,pFileList->DiskIndex,iRet);
        }
        return iRet;
    }

    pFileList->SysFd = fd;
    pFileList->VfdStatus = SYSFD_USABLE_STATUS;

    // 磁盘镜头数增加1;
    (void)pstRecordRaid->increase_camera_num(cameraId,pFileList->IoQueueIndex);

    //memcpy(pFileList->CameraId, cameraId, SVS_DEVICEID_LEN);
    //For 设备ID剥离，使用新的方法设置本地内存中设备ID
    SetLocalizeDeviceId((uint8_t *)(pFileList->CameraId),
                        sizeof(pFileList->CameraId),
                        cameraId);

    //pFileList->CameraId[SVS_DEVICEID_LEN]= '\0';
    SVS_LOG((SVS_LM_INFO,
        "Open file success, cameraId[%s], "
        "The camera number loaded on this raid is [%d], "
        "raid index[%d], vfd[%d], fd[%d], file full path[%s].",
        cameraId,
        pstRecordRaid->get_camera_num(),
        minCameraRaidIndex,
        pFileList->VFd,
        pFileList->SysFd,
        pFileList->RecordFilePath));

    return isNewDisk;
}

/*****************************************************************************
 函 数 名  : closeFile
 功能描述  : 释放虚文件句柄资源，并发送一个管理消息给IO_Schduele,文件的管理由
             IO_Schedule关闭。
 输入参数  : int32_t vFd:虚文件句柄
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::closeFile(int32_t vFd)
{
    if((0 > vFd) || (vFd >= RECORD_MAX_LENS_NUM))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Close file failed.The parameter is invalid.vfd[%d]",
            vFd));
        return SVS_ERR_PARAM;
    }

    if(-1 >= m_fileList[vFd].SysFd)
    {
        SVS_LOG((SVS_LM_DEBUG,
            "Close file failed.the system fd is -1.vfd[%d],",
            vFd));
        return SVS_ERR_NOFILE;
    }

    // raid组镜头数减1
    int32_t raidIndex = m_fileList[vFd].RaidIndex;

    SVS_LOG((SVS_LM_INFO,
        "Will close file.The camera numberloaded on this raid is [%d]."
        "vfd[%d],system fd[%d],file name[%s].",
        m_raidList[raidIndex].get_camera_num(),
        vFd,
        m_fileList[vFd].SysFd,
        m_fileList[vFd].RecordFilePath));

    // 创建一个关闭文件消息给IO_Schedule
    int32_t ret = SVS_IO_Scheduler::instance().add_IO(m_fileList[vFd].SysFd,
                                                   m_fileList[vFd].IoQueueIndex,
                                                   IO_CLOSE,
                                                   NULL,
                                                   NULL,
                                                   NULL);
    (void)m_raidList[raidIndex].decrease_camera_num(m_fileList[vFd].CameraId, m_fileList[vFd].IoQueueIndex);
    m_fileList[vFd].SysFd = -1;
    m_fileList[vFd].VfdStatus = SYSFD_UNUSABLE_STATUS;

    return ret;
}

/*****************************************************************************
 函 数 名  : writeFile
 功能描述  : 根据传入的虚文件句柄，找到对应的真实文件句柄，
 输入参数  : int32_t vFd:虚文件句柄
             ACE_Message_Block *pData:将要写的数据
             IO_CallBack_Func pFunc:回调函数
             void *pParam:写参数
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::writeFile(int32_t vFd, ACE_Message_Block *pData,
                                 IO_CallBack_Func pFunc, void *pParam)
{
    if((0 > vFd) || (vFd >= RECORD_MAX_LENS_NUM))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Write file failed.The parameter is invalid."
            "vfd[%d],pFunc[0x%08x],pParam[0x%08x]",
            vFd,
            pFunc,
            pParam));

        return SVS_ERR_PARAM;
    }
    SVS_DISK_VFILE * pVFile = &m_fileList[vFd];

    // 判断句柄是否可用标识
    if((-1 >= pVFile->SysFd) || (SYSFD_USABLE_STATUS != pVFile->VfdStatus))
    {
        // 如果文件句柄不可用，且需要写磁盘，则通知上层失败
        if (NULL != pData)
        {
            if( SYSFD_SWITCH_FILE_STATUS == pVFile->VfdStatus )
            {
                SVS_LOG((SVS_LM_INFO,
                    "Write file failed. fd is switch status. vfd[%d],"
                    "the system fd is [%d] the system fd status[%d],filePath[%s]",
                    vFd,
                    pVFile->SysFd,
                    pVFile->VfdStatus,
                    pVFile->RecordFilePath));

                return SVS_ERR_FD_SWITCH_FILE;
            }
            else
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Write file failed.vfd[%d],the system fd is [%d],"
                    " the system fd status[%d],filePath[%s]",
                    vFd,
                    pVFile->SysFd,
                    pVFile->VfdStatus,
                    pVFile->RecordFilePath));

                return SVS_ERR_INVALIDFD;
            }

         }
    }

    // 创建一个写数据消息给IO_Schedule
    int32_t ret = SVS_IO_Scheduler::instance().add_IO(pVFile->SysFd,
                                                   pVFile->IoQueueIndex,
                                                   IO_WRITE,
                                                   pData,
                                                   pFunc,
                                                  pParam);
    return ret;
}

/*****************************************************************************
 函 数 名  : writeFile
 功能描述  : 根据传入的虚文件绝病，找到对应的真实文件句柄，
 输入参数  : int32_t vFd:虚文件句柄
             ACE_Message_Block *pData:存放将要读出的数据
             IO_CallBack_Func pFunc:回调函数
             void *pParam:读参数
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::readFile(int32_t vFd, ACE_Message_Block *pData,
                                u_long /*ulReadSize*/,
                                IO_CallBack_Func pFunc, void *pParam)const
{
    if((0 > vFd)
    || (vFd >= RECORD_MAX_LENS_NUM))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Raad file failed.The parameter is invalid."
            "vfd[%d],pFunc[0x%08x],pParam[0x%08x]",
            vFd,
            pFunc,
            pParam));
        return SVS_ERR_PARAM;
    }

    if(-1 >= m_fileList[vFd].SysFd)
    {
        SVS_LOG((SVS_LM_DEBUG,
            "Raad file failed.the system fd is -1.vfd[%d],",
            vFd));
        return SVS_ERR_NOFILE;
    }

    // 创建一个写数据消息给IO_Schedule
    int32_t ret = SVS_IO_Scheduler::instance().add_IO(m_fileList[vFd].SysFd,
                                                   m_fileList[vFd].IoQueueIndex,
                                                   IO_READ,
                                                   pData,
                                                   pFunc,
                                                   pParam);

    return ret;
}

/*****************************************************************************
 函 数 名  : freeVfd
 功能描述  : 释放VFD
 输入参数  : int32_t fd:文件句柄
 输出参数  : NA
 返 回 值  : NA
*****************************************************************************/
void SVS_Disk_Manager::freeVfd(int32_t vfd)
{
    if((0 > vfd) || (vfd >= RECORD_MAX_LENS_NUM))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Free vfd failed.The parameter is invalid.vfd[%d].",
            vfd));

        return ;
    }

    m_fileList[vfd].VfdStatus = VFD_FREE_STATUS;

    return;
}

/*****************************************************************************
 函 数 名  : notifyDiskExcept
 功能描述  : IO Scheduler调用接口，通知磁盘检测管理模块有磁盘或文件操作异常
 输入参数  : int32_t fd:文件句柄
             int32_t errCode:IO错误类型
 输出参数  : NA
 返 回 值  : 成功返回 0,失败返回 -1
*****************************************************************************/
int32_t SVS_Disk_Manager::notifyDiskExcept(int32_t fd, int32_t errCode)
{
    // 查找fd对应的磁盘分区
    SVS_DISK_VFILE * pFileList = NULL;
    bool findRet = false;
    for(int32_t i = 0; i < RECORD_MAX_LENS_NUM; i++)
    {
        pFileList = m_fileList + i;

        if(fd == pFileList->SysFd)
        {
            findRet = true;
            break;
        }
    }

    if(!findRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Notify disk except message failed.Can't find the fd."
            "fd[%d],error code[%d]",
            fd,
            errCode));
        return SVS_ERR_INVALIDFD;
    }

    // 如果是文件句柄异常，则标识磁盘不可用,上层写录像失败后，会重新打开文件
    if(SVS_ERR_INVALIDFD == errCode)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Disk is exception.File fd is invalid.file name[%s]",
            pFileList->RecordFilePath));
        pFileList->VfdStatus = SYSFD_UNUSABLE_STATUS;
    }

    SVS_LOG((SVS_LM_WARNING,
        "Disk is exception.raid index[%d], "
        "disk index[%d],fd[%d] file name[%s],error code[%d]",
        pFileList->RaidIndex,
        pFileList->DiskIndex,
        fd,
        pFileList->RecordFilePath,
        errCode));

    (void)createDiskExceptMsg(pFileList->RaidIndex, pFileList->DiskIndex, errCode);
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : getFileFullName
 功能描述  : 根据传入的虚文件ID获取录像文件全路径
 输入参数  : int32_t vFd:虚文件句柄
 输出参数  : char *&szFileName:录像文件全路径
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::getFileFullName(int32_t vFd, char *szFileName)
{
    if((0 > vFd) || (vFd >= RECORD_MAX_LENS_NUM) || (NULL == szFileName))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Get file full name failed.The parameter is invalid.",
            "vFd[%d], szFileName[0x%08x]",
            vFd,
            szFileName));
        return SVS_ERR_PARAM;
    }

    SVS_DISK_VFILE * pVfdFile = &m_fileList[vFd];
    memcpy(szFileName, pVfdFile->RecordFilePath, strlen(pVfdFile->RecordFilePath));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : processCheckDiskTask
 功能描述  : 处理检测磁盘任务，任务来源与周期性检测磁盘和IO操作异常
 输入参数  : const ACE_Message_Block * mb:任务消息块
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::processCheckDiskTask(const ACE_Message_Block * mb)
{
    SVS_DISK_CHECK_TASK * pTaskMsg = (SVS_DISK_CHECK_TASK*)(void*)mb->rd_ptr();

    switch(pTaskMsg->TaskType)
    {
        // 如果是周期性检测磁盘消息,遍历检查所有磁盘
        case SVS_DISK_CHECK_TASK_TIMER:
        {
            // 获取mount信息
            getMounts();

            // 周期性检测磁盘空间
            (void)periodCheckRaid();

            // 盘满删除处理
            (void)diskFullDelRecord();
        }
        break;

        // 如果是磁盘异常检测任务，检测那个磁盘
        case SVS_DISK_CHECK_TASK_EXCEPT:
        {
            // 获取mount信息
            getMounts();
            (void)exceptCheckDisk(pTaskMsg);
        }
        break;

        default:
            SVS_LOG((SVS_LM_ERROR,
                "Invalid disk check message.message type[%d]",
                pTaskMsg->TaskType));

        return SVS_ERR_MSGTYPE;

    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : timerCallback
 功能描述  : 定时器回调,如果是周期性检测磁盘定时任务，则创建磁盘检测任务放入队列
 输入参数  : int32_t eventId:事件类型
             int32_t timerId:定时器ID
 输出参数  : NA
 返 回 值  : NA
*****************************************************************************/
void SVS_Disk_Manager::timerCallback( void * /*pArg*/, int32_t /*eventId*/, int32_t /*timerId*/, int32_t /*nDummy*/ )
{
    uint32_t msgLen = sizeof(SVS_DISK_CHECK_TASK);
    ACE_Message_Block *mb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK, msgLen );
    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create check disk task failed.Fail to allocate mb. "
            "type[%d], size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK,
            msgLen ));

        return ;
    }

    // 构造消息,只需要构造消息类型即可，改消息类型遍历所有磁盘
    SVS_DISK_CHECK_TASK* pMsg = (SVS_DISK_CHECK_TASK*)(void*)mb->wr_ptr();
    pMsg->TaskType = SVS_DISK_CHECK_TASK_TIMER;
    pMsg->RaidIndex = -1;
    pMsg->DiskIndex = -1;
    pMsg->ErrCode = 0;

    // 消息入队列
    ACE_Time_Value tvTimeout(ACE_OS::gettimeofday());
    if(SVS_RESULT_FAILURE == SVS_Disk_Manager::instance().enqueueCheckDiskTask( mb ,&tvTimeout))
    {
        SVS_LOG((SVS_LM_ERROR, "Check disk task message enqueue failed."));
        g_p_msg_buffer->free_mb(mb);
    }

    return;
}

/*****************************************************************************
 函 数 名  : refreshDiskList
 功能描述  : 动态刷新磁盘配置，解析传入的字符串，构造磁盘列表，每个磁盘都有自己
             的属性，包括所属的raid组的id，磁盘路径，磁盘是否是元数据分离。比较
             当前正在使用的磁盘和解析出的磁盘，如果相对于正在使用的磁盘，多出某
             磁盘，则添加磁盘对象如果当前正在使用的某磁盘，新配置中没有该，磁盘
             则置该磁盘不可用状态
 输入参数  : char * strDiskPath:所配置的所有的磁盘列表
 输出参数  : NA
 返 回 值  : 成功返回 0,失败返回 -1
*****************************************************************************/
int32_t SVS_Disk_Manager::refreshDiskList(const char * strDiskPath)
{
    // 1.参数判断
    if ((NULL == strDiskPath) || ('\0' == *strDiskPath))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to init disk raid, the pointer strDiskPath is invalid. "
            "strDiskPath[0x%08x] or length is 0.",
            strDiskPath));

        return SVS_ERR_PARAM;
    }

    // 2.重新读取mount值
    getMounts();

    // 3.解析磁盘列表配置
    DISK_INFO_VECTOR diskInfoVec;
    int32_t nRet = parseDiskListConf(strDiskPath,diskInfoVec);
    if(SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Refresh disk list failed. parse disk list configuration failed."
            "nRet[%d]",
            nRet));

        return nRet;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Success to parse disk list configuration. disk num[%u].",
        diskInfoVec.size()));

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    // 4.遍历当前正在使用的磁盘，看判断新的配置里还有没有该磁盘，如果没有了，需要把
    // 这样的磁盘可用标识置成不可用状态
    uint32_t uLoop = 0;
    for(uLoop = 0; uLoop < (uint32_t)m_raidTotal; uLoop++)
    {
        nRet = m_raidList[uLoop].refreshDiskInfo(diskInfoVec);
    }

    // 5.遍历新配置的磁盘，在当前正在使用的磁盘中寻找自己，如果找到，则刷新自己的状态
    // 如果没有找到自己，则添加一个新的磁盘对象到该raid组中
    uint32_t vectSize = diskInfoVec.size();
    DISK_INFO * pDiskInfo = NULL;
    for(uLoop = 0; uLoop < vectSize; uLoop++)
    {
        pDiskInfo = &diskInfoVec[uLoop];

        // 如果raid组index增加了，则新添加一个raid组
        if(pDiskInfo->RaidInfo.RaidIndex >= m_raidTotal)
        {
            nRet = m_raidList[pDiskInfo->RaidInfo.RaidIndex].init(m_lowerLimit,
                                                                      m_alarmLimit,
                                                                      m_alarmResumeLimit);

            if(SVS_RESULT_OK != nRet)
            {
                SVS_LOG((SVS_LM_CRITICAL,"Init raid group failed. nRet[%d]", nRet));
                return nRet;
            }

            m_raidTotal++;
        }

        // 尝试添加磁盘
        (void)m_raidList[pDiskInfo->RaidInfo.RaidIndex].tryAddDisk(*pDiskInfo);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : parsDiskListConf
 功能描述  : 解析磁盘列表信息，磁盘配置的格式:
             磁盘之间用','分开，raid组间用'|'分开，用'[1]'或'[0]'分别表示支持元数据
             分离和不支持元数据分离，可以不配置，默认不支持元数据分离
             RecordFilePath=/mnt/svs_md0[1],/mnt/svs_md1[1]|/mnt/svs_md2[0],/mnt/svs_md3
             前两个磁盘属于raid组1，且都是支持元数据分离的，后两个磁盘属于raid2
             ，且都是不支持元数据分离的，最后一个磁盘使用默认值
 输入参数  : const char *strRadiList:所有raid组的配置信息
 输出参数  : vector <int32_t>& pos:存放每隔raid组在strRadiList的偏移值，字串的位置
 返 回 值  : NA
*****************************************************************************/
int32_t SVS_Disk_Manager::parseDiskListConf(const char * strDiskPath, DISK_INFO_VECTOR& diskInfoVect)const
{
    char* startPos = (char*)strDiskPath;
    char* endPos = NULL;
    int32_t raidIndex = 0;
    char diskPathTemp[DISK_NAME_SIZE] = {0,};

    bool BCtrl = true;
    // 解析 ，注意:最后一个磁盘路径允许没有','标识
    do
    {
        // 找路径结束符
        bool newRaidFlag = false;
        endPos = startPos;
        if(startPos != strDiskPath)
        {
            endPos = startPos + 1;
            if ('|' == *startPos)
            {
                newRaidFlag = true;
            }
        }

        for(; ',' != *endPos && '\0' != *endPos && '|' != *endPos; endPos++)
        {
            // 如果到达了新的raid组
            if ('|' == *endPos)
            {
                newRaidFlag = true;
            }
        }

        // 如果其实等于结束，或者当前正在解析的磁盘已经不再是一个磁盘路径了，则解析结束
        if((endPos <= startPos) || (NULL == strchr(startPos, '/')))
        {
            break;
        }

        if (newRaidFlag)
        {
            ++raidIndex;
        }

        (void)memset(diskPathTemp, 0, sizeof(diskPathTemp));
        (void)strncpy(diskPathTemp, startPos, (uint32_t)(endPos - startPos));
        startPos = endPos;
        int32_t nRet = SVS_RESULT_FAILURE;
        DISK_INFO diskInfoTemp;
        nRet = parseDiskInfo(diskPathTemp, diskInfoTemp);
        if(SVS_RESULT_OK != nRet)
        {
            continue;
//            return SVS_RESULT_FAILURE;
        }

        // raid属性
        diskInfoTemp.RaidInfo.RaidIndex = raidIndex;

        diskInfoVect.push_back(diskInfoTemp);

    }while(BCtrl);

    return SVS_RESULT_OK;
}

int32_t SVS_Disk_Manager::parseDiskInfo(char*diskPath, DISK_INFO&diskInfo)const
{
    SVS_LOG((SVS_LM_INFO, "Begin to parse disk information. conf[%s].", diskPath));
    // 解析当前磁盘的属性
    (void)memset(&diskInfo, 0, sizeof(diskInfo));

    int32_t medaDataPartedFlag = SVS_META_DATA_DISPARTED;
    char *pos = diskPath + strlen(diskPath) - 1;
    for(; diskPath != pos ; --pos)
    {
        // 如果找到磁盘属性项
        if ('[' == *pos)
        {
            medaDataPartedFlag = *(++pos) - 48;
            break;
        }
    }

    // 如果磁盘标识正确
    if(SVS_META_DATA_DISPARTED == medaDataPartedFlag)
    {
        diskInfo.MedaDataPartedFlag = false;
    }
    else if(SVS_META_DATA_PARTED == medaDataPartedFlag)
    {
        diskInfo.MedaDataPartedFlag = true;
    }
    else
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Parse disk list configuration failed. the meda data parted flag is wrong."
            "Shoud [0] or [1], but really[%d], disk[%s].",
            medaDataPartedFlag,
            diskPath));

        return SVS_RESULT_FAILURE;
    }

    pos = NULL;
    char* startPos = diskPath;
    char* endPos = diskPath + (strlen(diskPath) - 1);
    // 磁盘开始位置
    for(; endPos >= startPos ; ++startPos)
    {
        if ('/' == *startPos)
        {
            pos = startPos;
            break;
        }
    }

    if(NULL == pos)
    {
        SVS_LOG((SVS_LM_WARNING, "[%s] do not include a disk path.", diskPath));
        return SVS_RESULT_FAILURE;
    }

    pos = move_pos(pos);

    strncpy(diskInfo.DiskPath, startPos, (uint32_t)(pos - startPos));

    // 为了能找到mount，去掉最后一个'/',但是注意规避路径'/'
    unsigned diskPathLen = strlen(diskInfo.DiskPath);

    if ((1 != diskPathLen) && ('/' == diskInfo.DiskPath[diskPathLen - 1 ]))
    {
        diskInfo.DiskPath[diskPathLen - 1 ] = '\0';
    }

    // 判断mount属性
    diskInfo.DiskMountTypeFlag = SVS_DISK_TYPE_MOUNTDISK;
    int32_t nRet = checkMountOnlineStatus(diskInfo.DiskPath);

    if((SVS_RESULT_OK != nRet)
        &&(false == diskInfo.MedaDataPartedFlag))//如果是元数据分离的默认是网络盘
    {
        diskInfo.DiskMountTypeFlag = SVS_DISK_TYPE_LOCALDISK;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Parse disk.diskPath[%s], realtime flag[%d], mount flag[%d],",
        diskInfo.DiskPath,
        diskInfo.MedaDataPartedFlag,
        diskInfo.DiskMountTypeFlag));
    return SVS_RESULT_OK;
}

char* SVS_Disk_Manager::move_pos(const char* pos)const
{
    char* tempPos = (char*)pos;
    if(NULL == tempPos)
    {
        //nothing
        return tempPos;
    }

    // 磁盘路径最后的处理
    for(;(!(' ' == *tempPos || '\0' == *tempPos || ',' == *tempPos || '|' == *tempPos || '[' == *tempPos)); ++tempPos)
    {
        //nothing
    }

    return tempPos;
}

/*****************************************************************************
 函 数 名  : periodCheckRaid
 功能描述  : 检测所有的磁盘，如果某个raid组内的当前可以使用的lun需要切换到下一个
             则切换创建切换磁盘告警，通知上层业务层，做好切换磁盘的准备
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::periodCheckRaid()
{
    SVS_LOG((SVS_LM_DEBUG, "Begin to check disk for period check task."));
    int32_t raidCapacityCameraNum = (int32_t)CAC_RT_Record_Server::instance()->max_lens_per_raid();
    for (int32_t i = 0; i < m_raidTotal; i++)
    {
        CAC_Record_Raid* pstRecordRaid = NULL;
        pstRecordRaid = m_raidList + i ;


        // 记录检查当前正在使用的磁盘下标值和原来的磁盘下标是否相等
        // 记录当前磁盘的错误类型
        int32_t nOldDiskIndex,nNewDiskIndex = 0;
        int32_t nOldRaidErrCode,nNewRaidErrCode = 0;
        nOldDiskIndex = pstRecordRaid->output_disk();
        nOldRaidErrCode = pstRecordRaid->raid_err_type();

        // 遍历所有磁盘
        int32_t nStart,nEnd = 0;
        if((0 > nOldDiskIndex) || (nOldDiskIndex >= pstRecordRaid->disk_total()))
        {
            nOldDiskIndex = 0;
        }

        nStart = nOldDiskIndex;
        nEnd   = (pstRecordRaid->disk_total() - 1) + nOldDiskIndex;
        (void)pstRecordRaid->select_disk(nStart,nEnd);

        // 如果raid组内的lun都不可用，则发送raid告警,设置raid组能力
        // raid组告警后，lun就不用告警了
        nNewRaidErrCode = pstRecordRaid->raid_err_type();
        if((SVS_DISK_ERR_RAID_FULL <= nNewRaidErrCode )
        && (nOldRaidErrCode != nNewRaidErrCode))
        {
            pstRecordRaid->set_capacity_camera_num(0);

            if (SVS_DISK_ERR_RAID_FULL > nOldRaidErrCode)// 刚刚是正常的才上报异常
            {
                (void)notifyExcept(i, pstRecordRaid->raid_err_type());

                // 即时负载上报
                CAC_RT_Record_Server::instance()->add_load_report_task();
            }

            continue;
        }
        // raid组能力恢复，则恢复能力标识
        else if((SVS_DISK_ERR_RAID_FULL <= nOldRaidErrCode)
             && (SVS_DISK_ERR_RAID_ALARM >= nNewRaidErrCode))
        {
            pstRecordRaid->set_capacity_camera_num(raidCapacityCameraNum);
        }
        else
        {
            // 如果切换的lun，则发送告警
            nNewDiskIndex = pstRecordRaid->output_disk();
            if((nOldDiskIndex != nNewDiskIndex)
            && ((nOldDiskIndex >= 0) && (nOldDiskIndex < pstRecordRaid->disk_total())))
            {
                (void)notifyExcept(i, pstRecordRaid->disk_err_type(nOldDiskIndex));
                continue;
            }
        }
    }

    if(NULL == m_IndexFileDisk)
    {
        SVS_LOG((SVS_LM_WARNING, "Check record indexpath failed.The object is NULL"));

        return SVS_RESULT_OK;
    }

    (void)m_IndexFileDisk->statistics();

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : periodCheckRaid
 功能描述  : 处理磁盘异常通知消息，检测报异常的磁盘真正的异常情况，如果确实异常，
             标识该磁盘不可用，并及时向上通知业务层
 输入参数  : const SVS_DISK_CHECK_TASK * pTaskMsg:磁盘异常任务消息
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::exceptCheckDisk(const SVS_DISK_CHECK_TASK * pTaskMsg)
{
    if(NULL ==  pTaskMsg)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to check disk's exception.The parameter is null."));
        return SVS_ERR_PARAM;
    }
    int32_t raidIndex = pTaskMsg->RaidIndex;

    if(raidIndex >= m_raidTotal)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to check disk's exception.The parameter is invalid."
            "Total raid number[%d], raid index in message [%d]",
            m_raidTotal,
            raidIndex));
        return SVS_ERR_PARAM;
    }

    // 检测磁盘
    CAC_Record_Raid * pRaid = NULL;
    pRaid = &m_raidList[raidIndex];

    int32_t diskIndex = pTaskMsg->DiskIndex;
    SVS_LOG((SVS_LM_INFO,
        "Begin to handle disk exception message."
        "raid path[%s],disk path[%s]",
        pRaid->raid_path(),
        pRaid->disk_path(diskIndex)));

    (void)pRaid->check_disk(diskIndex);

//1 TODO raid组性能下降怎么处理，raid组怎么调度?

    // 如果当前正在使用的磁盘已经不是这个磁盘，则不用上报告警
    if(pRaid->output_disk() != diskIndex)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Handle disk exception message finished.Disk has been switched."
            "raid path[%s],disk path[%s]",
            pRaid->raid_path(),
            pRaid->disk_path(diskIndex)));
        return SVS_RESULT_OK;
    }

    // 经过检查磁盘没有异常，也不用向上通知业务层
    if(SVS_DISK_ERR_DISK_BAD > pRaid->disk_err_type(diskIndex))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Handle disk exception message finished.Disk is OK."
            "raid path[%s],disk path[%s]",
            pRaid->raid_path(),
            pRaid->disk_path(diskIndex)));
        return SVS_RESULT_OK;
    }

    // 该磁盘有异常，则同时上层业务层
    (void)notifyExcept(raidIndex, pRaid->disk_err_type(diskIndex));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : notifyExcept
 功能描述  : 磁盘异常通知,检测到磁盘异常后，调用异常通知接口，及时通知业务层
 输入参数  : const SVS_DISK_CHECK_TASK * pTaskMsg:磁盘异常任务消息
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::notifyExcept(int32_t raidId,int32_t diskErr)
{
    std::vector<string> cameraVec;
    cameraVec.clear();

    // 获取该raid组上的文件列表.并置录像文件不可用状态
    SVS_DISK_VFILE * pFileList = NULL;
    for(int32_t i = 0; i < RECORD_MAX_LENS_NUM; i++)
    {
        pFileList = m_fileList + i;
        if ((raidId == pFileList->RaidIndex) && (-1 < pFileList->SysFd))
        {
            cameraVec.push_back(pFileList->CameraId);

            if ( diskErr < SVS_DISK_ERR_DISK_BAD )
            {
                pFileList->VfdStatus = SYSFD_SWITCH_FILE_STATUS;
            }
            else
            {
                pFileList->VfdStatus = SYSFD_UNUSABLE_STATUS;
            }
        }
    }

    int32_t iRet = CAC_RT_Record_Server::instance()->notify_exception(cameraVec, raidId,diskErr);
    if (SVS_RESULT_OK != iRet)
    {
        SVS_LOG((SVS_LM_ERROR ,
            "Notify disk exception failed."
            "raid path[%s],disk error code[%d],fd number[%d].",
            m_raidList[raidId].raid_path(),
            diskErr,
            cameraVec.size()));
        return iRet;
    }

    SVS_LOG((SVS_LM_INFO,
        "Notify disk exception successfully."
        "raid path[%s],disk error code[%d],fd number[%d].",
        m_raidList[raidId].raid_path(),
        diskErr,
        cameraVec.size()));
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : createDir
 功能描述  : 创建目录，首先尝试创建镜头级目录，然后创始创建文件目录
 输入参数  : const char* diskPath:磁盘目录
             const char* filePath:录像文件相对路径，镜头ID/日期/录像文件名称
 输出参数  : NA
 返 回 值  : 成功返回 0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::createDir(const char* diskPath,char* camerId,char * strDay)const
{
    if((NULL == diskPath) || (NULL == camerId) || (NULL == strDay))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create directory failed.The parameter is invalid."
            "diskPath[0x%08x],camerId[0x%08x],strDay[0x%08x]",
            diskPath,
            camerId,
            strDay));
        return SVS_ERR_PARAM;
    }

    char strFullDir[MAXNAMELEN] = {0,};
    int32_t nErrNo = 0;
    bool bCreateRet = true;
    do
    {
        // 创建镜头目录
        (void)snprintf(strFullDir,MAXNAMELEN,"%s/%s",diskPath,camerId);

        if(0 != ACE_OS::mkdir(strFullDir))
        {
            nErrNo = ACE_OS::last_error();
            if(EEXIST != nErrNo) // 如果不是已经存在错误
            {
                SVS_LOG( (SVS_LM_ERROR,
                    "Create record directory failed.directory[%s]",
                    strFullDir));
                bCreateRet = false;
                break;
            }
        }

        // 创建日期目录
        (void)snprintf(strFullDir,MAXNAMELEN,"%s/%s/%s",diskPath,camerId,strDay);
        if(0 != ACE_OS::mkdir(strFullDir))
        {
            nErrNo = ACE_OS::last_error();
            if(EEXIST != nErrNo) // 如果不是已经存在错误
            {
                SVS_LOG( (SVS_LM_ERROR,
                    "Create record directory failed.directory[%s]",
                    strFullDir));
                bCreateRet = false;
                break;
            }
        }
    }while(0);

    if(!bCreateRet)
    {
        switch(nErrNo)
        {
            case EIO:
            {
                return SVS_ERR_IOERR;
            }
            case ENOSPC:
            {
                return SVS_ERR_NOSPACE;
            }
            case EROFS:
            {
                return SVS_ERR_FSREADONLY;
            }
            default:
            break;
        }

        return SVS_ERR_DISKFAULT;
    }

    ACE_OS::last_error(0);
    SVS_LOG( (SVS_LM_DEBUG,
        "Create record directory succeeded.directory[%s].",
        strFullDir));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : createFile
 功能描述  : 创建录像文件
 输入参数  : const char* fileFullPath:录像文件绝对路径
 输出参数  : uint64_t &fileSize:当前录像文件大小
             int32_t &vfd:虚文件句柄
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::createFile(const char* fileFullPath,bool medaDataPartedFlag,
                                   uint64_t &fileSize,int32_t &fd)const
{
    if(NULL == fileFullPath)
    {
        SVS_LOG((SVS_LM_ERROR, "Create file failed. The parameter is NULL."));
        return SVS_ERR_PARAM;
    }

    int32_t flags = O_CREAT | O_WRONLY | O_APPEND | O_LARGEFILE;
    if (1 <= CAC_RT_Record_Server::instance()->get_use_direct_io_flag())
    {
        flags |= O_DIRECT;
        SVS_LOG((SVS_LM_INFO, "Create file use O_DIRECT flag.file path[%s].",fileFullPath));
    }

    fd = ACE_OS::open(fileFullPath,flags,ACE_DEFAULT_FILE_PERMS);
    if(-1 >= fd)
    {
        const int32_t nErrNo = ACE_OS::last_error();
        switch(nErrNo)
        {
            case EIO:
            {
                return SVS_ERR_IOERR;
            }
            case ENOSPC:
            {
                return SVS_ERR_NOSPACE;
            }
            case EROFS:
            {
                return SVS_ERR_FSREADONLY;
            }
            default:
            break;
        }
        return SVS_ERR_DISKFAULT;
    }

    // 如果元数据和视频数据分离
    if(medaDataPartedFlag)
    {
        struct fsxattr attrs;
        if (0 != ioctl(fd,XFS_IOC_FSGETXATTR,&attrs))
        {
            (void)ACE_OS::close((ACE_HANDLE)fd);
            fd = -1;
            SVS_LOG((SVS_LM_ERROR,
                "Failed to open record file.ioctl failed ."
                "record file name[%s].",
                fileFullPath));
            return SVS_ERR_IOERR;
        }
        attrs.fsx_xflags |= XFS_XFLAG_REALTIME;

        if (0 != ioctl(fd,XFS_IOC_FSSETXATTR,&attrs))
        {
            (void)ACE_OS::close((ACE_HANDLE)fd);
            fd = -1;
            SVS_LOG((SVS_LM_ERROR,
                "Failed to open record file.ioctl failed ."
                "record file name[%s].",
                fileFullPath));
            return SVS_ERR_IOERR;
        }

    }

    // 当前录像文件大小
    struct stat64 st;
    if (0 != ::stat64(fileFullPath, &st))
    {
        (void)ACE_OS::close((ACE_HANDLE)fd);
        fd = -1;
        SVS_LOG((SVS_LM_ERROR,
            "Failed to open record file.Stat record file size failed ."
            "record file name[%s].",
            fileFullPath));
        return SVS_ERR_STATFILE;
    }

    fileSize = (uint64_t)st.st_size;//lint !e67 base type

    // 事件录像时太多日志，直接置0
    ACE_OS::last_error(0);
    SVS_LOG((SVS_LM_INFO,
        "Open record file succeeded."
        "record file[handle:%d,name:%s],fileSize[%Q]",
        fd,
        fileFullPath,
        fileSize));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : createDiskExceptMsg
 功能描述  : 创建磁盘异常消息，放入磁盘检测任务队列，有磁盘检测线程处理
 输入参数  : int32_t raidIndex:raid组的索引
             int32_t diskIndex:raid组内的磁盘索引
             int32_t errCode:磁盘异常错误码
 输出参数  : NA
 返 回 值  : NA
*****************************************************************************/
int32_t SVS_Disk_Manager::createDiskExceptMsg(int32_t raidIndex,int32_t diskIndex,int32_t errCode)
{
    uint32_t msgLen = sizeof(SVS_DISK_CHECK_TASK);
    ACE_Message_Block *mb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK, msgLen );
    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create check disk task failed.Fail to allocate mb. type[%d], size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK,
            msgLen ));

        return SVS_ERR_NOMSG;
    }

    // 构造消息,只需要构造消息类型即可，改消息类型遍历所有磁盘
    SVS_DISK_CHECK_TASK* pMsg = (SVS_DISK_CHECK_TASK*)(void*)mb->wr_ptr();
    pMsg->TaskType = SVS_DISK_CHECK_TASK_EXCEPT;
    pMsg->RaidIndex = raidIndex;
    pMsg->DiskIndex = diskIndex;
    pMsg->ErrCode = errCode;

    // 消息入队列
    ACE_Time_Value tvTimeout(ACE_OS::gettimeofday());
    if(SVS_RESULT_FAILURE == enqueueCheckDiskTask( mb ,&tvTimeout))
    {
        SVS_LOG((SVS_LM_ERROR, "Check disk task message enqueue failed."));
        g_p_msg_buffer->free_mb(mb);
        return SVS_ERR_QUEUE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : checkUseableRaid
 功能描述  : 检查是否有可用的磁盘
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 -1
 修改历史  : 2009-9-24  新增
*****************************************************************************/
int32_t SVS_Disk_Manager::checkUseableRaid()
{
    CAC_Record_Raid* pstRecordRaid = NULL;
    for (int32_t i = 0; i < m_raidTotal; i++)
    {
        pstRecordRaid = m_raidList + i;
        // 如果磁盘坏了,查找下一个
        if (SVS_DISK_ERR_RAID_FULL <= pstRecordRaid->raid_err_type())
        {
            continue;
        }

        // 如果负载已经达到最大，则跳过该raid组
        if(pstRecordRaid->get_camera_num() >= pstRecordRaid->get_capacity_camera_num())
        {
            continue;
        }

        // Raid组可用，返回成功
        SVS_LOG((SVS_LM_INFO, "check useable disk success, raid[%s] free size[%u] camera[%d]",
                   pstRecordRaid->raid_path(),
                   pstRecordRaid->raid_free_size(),
                   pstRecordRaid->get_camera_num()));
        return SVS_RESULT_OK;
    }

    SVS_LOG((SVS_LM_INFO, "check useable disk, all raid is unuseable."));

    return SVS_RESULT_FAILURE;
}

/*****************************************************************************
 函 数 名  : getAllDiskInfo
 功能描述  : 获取所有没有坏掉的磁盘路径信息
 输入参数  : NA
 输出参数  : DISK_INFO_VECTOR & diskInfoVector:磁盘路径列表
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::getAllDiskInfo(DISK_INFO_VECTOR & diskInfoVector)const
{
    diskInfoVector.clear();
    for (int32_t i = 0; i < m_raidTotal; i++)
    {
        // 如果磁盘坏了，则不用返回
        //if (SVS_DISK_ERR_RAID_FULL <= this->m_raidList[i].raid_err_type())
        if (SVS_DISK_ERR_RAID_FULL < this->m_raidList[i].raid_err_type())
        {
            continue;
        }

        // 获取当前这个raid组内所有的磁盘信息
        DISK_INFO_VECTOR tempDiskVect;
        uint32_t tempSize = 0;
        tempDiskVect.clear();
        (void)this->m_raidList[i].get_all_disk_info(tempDiskVect);
        tempSize = tempDiskVect.size();
        for(uint32_t j = 0; j < tempSize; j++ )
        {
            diskInfoVector.push_back(tempDiskVect[j]);
        }
    }

    return SVS_RESULT_OK;
}

int32_t SVS_Disk_Manager::getUsableRaidNum()const
{
    int32_t usableRaidNum = 0;
    for (int32_t i = 0; i < m_raidTotal; i++)
    {
        // 如果磁盘坏了，则不用返回
        if (SVS_DISK_ERR_RAID_FULL <= this->m_raidList[i].raid_err_type())
        {
            continue;
        }

        ++usableRaidNum;
    }

    return usableRaidNum;
}
/*****************************************************************************
 函 数 名  : time2RecodFileName
 功能描述  : 根据传入的时间计算日期目录名称和录像文件名称
 输入参数  : time_t RecordTime:录像时间
 输出参数  : char*& strDay:录像所在的日期
             char*&strRecordFileName:录像对应的录像文件名称
 返 回 值  : NA
*****************************************************************************/
void SVS_Disk_Manager::time2RecodFileName(const time_t RecordTime,
                                                char* strRecordFileName,
                                                const uint32_t bufLen,
                                                char* strfileExName)const
{
    struct tm tmv;
    (void)ACE_OS::localtime_r( &RecordTime, &tmv );
    (void)ACE_OS::snprintf(strRecordFileName, bufLen, "%04d%02d%02d%02d.%s",
                     tmv.tm_year + 1900,
                     tmv.tm_mon + 1,
                     tmv.tm_mday,
                     tmv.tm_hour,
                     strfileExName);
     return;
}

// 如果是新上线录像的镜头，则重新分配一个vfd对象
// 如果是新上线录像的一个镜头或原来使用的raid组不可用，则找一个全新的raid；
// 如果镜头已经在录像，则在相同raid组上录像
int32_t SVS_Disk_Manager::allocDisk(int32_t &vFd,int32_t &raidIndex)
{
    SVS_LOG((SVS_LM_DEBUG, "Allocate disk. vf[%d], raid index[%d]",vFd, raidIndex));
    SVS_DISK_VFILE * pFileList = NULL;
    int32_t isNewDisk = SVS_RESULT_OK;
    // 在文件列表中找到一个空闲的文件空间,从上次获取的vfd位置开始
    if(-1 >= vFd)
    {
        int32_t j = 0;
        int32_t loopStart = m_vfdCursor;
        int32_t loopEnd = m_vfdCursor + RECORD_MAX_LENS_NUM;
        for(j = loopStart; j < loopEnd; j++)
        {
            pFileList = m_fileList + (j % RECORD_MAX_LENS_NUM);

            // 找到一个空闲的vfd
            if( VFD_FREE_STATUS == pFileList->VfdStatus)
            {
                break;
            }
        }

        // 没有空闲的文件空间
        if(loopEnd == j)
        {
            SVS_LOG((SVS_LM_ERROR ,"Open file failed.There is not free file space."));
            return SVS_ERR_NOVFD;
        }

        // 找到可用的vfd，获得vfd，并更新vfd游标的位置
        isNewDisk = SVS_RESULT_OK;
        vFd = (j % RECORD_MAX_LENS_NUM);
        m_vfdCursor = vFd + 1;
    }
    else // 不是第一次打开录像文件，则尝试在同一个raid组常录像
    {
        raidIndex = m_fileList[vFd].RaidIndex;

        // 当前raid组可用，则成功返回
        if(SVS_DISK_ERR_RAID_FULL > m_raidList[raidIndex].raid_err_type())
        {
            // 如果当前raid组已经切换lun
            if(m_fileList[vFd].DiskIndex != m_raidList[raidIndex].output_disk())
            {
                return  SVS_ERR_CHANGEDISK;
            }

            return SVS_RESULT_OK;
        }

        // 当前raid组不可用，需要重新选择一个可用的raid组
        isNewDisk = SVS_ERR_CHANGEDISK;
    }

    // 需要重新选择一个新的raid组
    // 计算并设置raid组的负载
    countRaidLoad();

    CAC_Record_Raid* pstRecordRaid = NULL;
    int32_t minCameraRaidIndex = -1;

    // 选择一个可用的，负载最轻的raid组创建文件
    for (int32_t i = 0; i < m_raidTotal; i++)
    {
        pstRecordRaid = m_raidList + i ;

        // 如果raid组不可用，直接跳过
        if(SVS_DISK_ERR_RAID_FULL <= pstRecordRaid->raid_err_type())
        {

            continue;
        }

        // 如果负载已经达到最大，则跳过该raid组
        if(pstRecordRaid->get_camera_num() >= pstRecordRaid->get_capacity_camera_num())
        {
            continue;
        }

        // 如果是第一次找到一个可用的raid组，则把当前的这个raid组最为目前找到的负载最轻的raid组
        if(-1 >= minCameraRaidIndex)
        {
            minCameraRaidIndex = i;
        }

        // 如果找到一个负载更轻的raid组,则更新负载最轻的raid组的下标
        if(m_raidList[minCameraRaidIndex].get_load() > pstRecordRaid->get_load())
        {
            minCameraRaidIndex = i;
        }
    }

    // 如果所有raid组都不可用
    if(0 > minCameraRaidIndex)
    {
        SVS_LOG((SVS_LM_ERROR ,"Open file failed.All raid are unusable."));
        return SVS_ERR_NORAID;
    }

    // 获取输出线程ID
    uint32_t ulThreadIndex = 0;
    int32_t nRet = m_raidList[minCameraRaidIndex].getCurrOutputThreadIndex(ulThreadIndex);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_ERROR ,
            "Open file failed.get output thread index failed.raid id[%d].",
            minCameraRaidIndex ));
        return SVS_ERR_NORAID;
    }
    m_fileList[vFd].IoQueueIndex = (int32_t)ulThreadIndex;

    // 找到可用的raid组后，成功返回
    raidIndex = minCameraRaidIndex;

    return isNewDisk;
}

/*****************************************************************************
 函 数 名  : countRaidLoad
 功能描述  : 计算raid组的负载情况，查找一个负载最轻的raid组。raid组的负载情况由
             磁盘空间、IO等
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 返回raid组
*****************************************************************************/
void SVS_Disk_Manager::countRaidLoad()
{
    //1 还需要添加动态负载,例如某raid组先故障后，恢复
    // 1 磁盘,只通知磁盘可用的raid组
    uint32_t diskTotalFreeSize = 0;
    int32_t diskTotalCameraNum = 0;
    CAC_Record_Raid* pstRecordRaid = NULL;
    int32_t i = 0;
    for(; i < m_raidTotal; i++)
    {
        pstRecordRaid = m_raidList + i ;

        // 如果raid组不可用，直接跳过
        if(SVS_DISK_ERR_RAID_FULL <= pstRecordRaid->raid_err_type())
        {
            continue;
        }

        // 如果负载已经达到最大，则跳过该raid组
        if(pstRecordRaid->get_camera_num() >= pstRecordRaid->get_capacity_camera_num())
        {
            continue;
        }

        // 统计所有raid组的总镜头数和剩余空间
        diskTotalCameraNum += pstRecordRaid->get_camera_num();

        diskTotalFreeSize += pstRecordRaid->raid_free_size();

        // 2 IO


        // 3 －－－－其它参数负载的参数

    }

    // 如果没有正常的raid组
    if(0 == diskTotalFreeSize)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Count raid load finished .All disk total free size is %d.camera number[%d]",
            diskTotalFreeSize,
            diskTotalCameraNum));
        return;
    }

    // 设置每个raid组的权限,实际正在录像的镜头和应该承担的镜头的比值
    int32_t raidLoad = 0;
    float cameraInfo = 0;
    float capacityInfo = 0;
    for(i = 0 ; i < m_raidTotal; i++)
    {
        pstRecordRaid = m_raidList + i ;
        // 如果所有的镜头为0,如果当前raid组空间为0
        uint32_t ulRaidFreeSize = pstRecordRaid->raid_free_size();
        if((0 == diskTotalCameraNum) || (0 == ulRaidFreeSize))
        {
            raidLoad = 0;
            continue;
        }

        // 计算负载值，放大1000倍，减小误差
        cameraInfo = (float)pstRecordRaid->get_camera_num() / diskTotalCameraNum;
        capacityInfo = (float)ulRaidFreeSize / diskTotalFreeSize;
        raidLoad = 1;
        if(0 != capacityInfo)
        {
            raidLoad = (int32_t)(cameraInfo / capacityInfo);
        }

        SVS_LOG((SVS_LM_DEBUG,
            "Set raid load .Raid index [%d],count load result[%d]"
            "raid camera num[%d], raid total camera num[%d],"
            "raid free size[%d]MB,raid total free size[%d]MB.",
            i, raidLoad,
            pstRecordRaid->get_camera_num(),diskTotalCameraNum,
            ulRaidFreeSize,diskTotalFreeSize));

        pstRecordRaid->set_load(raidLoad);
    }


    return;
}

/*****************************************************************************
 函 数 名  : debugGetRaidListInfo
 功能描述  : telnet获取raid组信息
 输入参数  : char *strBufferInfo:缓存区，存放raid组信息
             const uint32_t bufferLen:缓存区长度
 输出参数  : char *strBufferInfo:raid组信息
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
void SVS_Disk_Manager::getMounts()
{
    m_bIsMountsOk = false;
    FILE                        *pf;
    pf = fopen("/proc/mounts", "r");
    size_t buffSize = sizeof(m_mountsInfo);

    /* 虚文件打开一般不会失败，但要保护 */
    // 如果打开虚文件失败，则认为所有的mount信息都在，保证磁盘可用
    if (NULL == pf)
    {
        SVS_LOG((SVS_LM_WARNING, "Open /proc/mounts failed."));
        return;
    }

    /* 获取CPU使用时间信息 */
    memset(m_mountsInfo, 0, buffSize);
    if (0 == fread(m_mountsInfo, sizeof(char), buffSize, pf))
    {
        (void)fclose(pf);
        return;
    }

    m_bIsMountsOk = true;
    (void)fclose(pf);

    ACE_OS::last_error(0);
    return;
}

int32_t SVS_Disk_Manager::checkMountOnlineStatus(const char * strDiskPath)const
{
    if(NULL == strDiskPath)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Check mount online status failed.The parameter is invalid."
            " strDiskPath[0x%08x]",
            strDiskPath));
        return SVS_ERR_PARAM;
    }

    // 如果读取/proc/mounts失败，则认为mount可用
    if(!m_bIsMountsOk)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Check mount online status failed.Read /proc/mounts failed."));
        return SVS_RESULT_OK;
    }

    //查看 /proc/mounts是否有要查询的磁盘信息
    char tempDiskPath[DISK_NAME_SIZE];
    (void)snprintf(tempDiskPath, sizeof(tempDiskPath), " %s ", strDiskPath);

    // 如果mount掉线
    if (NULL == strstr(m_mountsInfo, tempDiskPath))
    {
        return SVS_ERR_MOUNTOFFLINE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : debugGetRaidListInfo
 功能描述  : telnet获取raid组信息
 输入参数  : char *strBufferInfo:缓存区，存放raid组信息
             const uint32_t bufferLen:缓存区长度
 输出参数  : char *strBufferInfo:raid组信息
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::debugGetRaidListInfo( char *strBufferInfo,const uint32_t bufferLen )
{
    // 如果未初始化好，则不能调试
    if(m_bThreadFlag)
    {
        return SVS_RESULT_FAILURE;
    }

    memset(strBufferInfo, 0, bufferLen);
    CAC_Record_Raid *pstRecordRaid = NULL;
    int32_t diskIndex = 0;
    size_t writeLen = 0;
    int32_t leaveLen = 0;

    int32_t freeThreadGroupNum = getFreeOutputThreadGroupNum();

    // 获取录像线程输出数，为每个raid组的线程数和系统支持的最大并发工作raid组数的乘积
    uint32_t threadNumPerRaid = CAC_RT_Record_Server::instance()->output_thread_num_per_raid();
    uint32_t syncRaidNum = CAC_RT_Record_Server::instance()->sync_work_raid_group_num();


    // 打印磁盘错误码列表
    (void)ACE_OS::snprintf(   strBufferInfo,
        bufferLen,
        "\t    ErrorCodeList    \n"
        "\t [%d]-->OK \n"
        "\t [%d]-->DISK_ALARM_RESUME \n"
        "\t [%d]-->RAID_ALARM_RESUME \n"
        "\t [%d]-->DISK_ALARM \n"
        "\t [%d]-->RAID_ALARM \n"
        "\t [%d]-->DISK_FULL \n"
        "\t [%d]-->RAID_FULL \n"
        "\t [%d]-->DISK_BAD \n"
        "\t [%d]-->DISK_MOUNT_OFFLINE \n"
        "\t [%d]-->DISK_FS_INVALID \n"
        "\t [%d]-->DISK_FS_READONLY \n"
        "\t [%d]-->DISK_OFFLINE_CONF \n"
        "\t [%d]-->RAID_NO_OUTPUT_THREAD \n"
        "\n\t   The Attribute of Disk Explain \n"
        "\t Disk Mount(0)/Local(1) \n"
        "\t Disk Realtime(1)/NotRealtime(0) \n"
        "\n\t SystemMaxCameraNum      [%u] \n"
        "\t SystemOutputThreadNum     [%u] \n"
        "\t ThreadNumPerRaidGroup     [%u] \n"
        "\t FreeThreadGroupNum        [%u] \n"
        "\n\t             RaidTotalNumber     [%d] \n\n"
        "\tRaidId MaxCameraNum CurCameraNum WriteThreadStartId WriteThreadNum RaidErr DiskErr CurrentDiskInfo\n",
        SVS_DISK_ERR_OK,                        /*磁盘正常*/
        SVS_DISK_ERR_DISK_ALARM_RESUME,         /*磁盘进入告警预警范围*/
        SVS_DISK_ERR_RAID_ALARM_RESUME,         /*Raid组进入告警预警范围*/
        SVS_DISK_ERR_DISK_ALARM,                /*磁盘告警*/
        SVS_DISK_ERR_RAID_ALARM,                /*Raid组告警*/
        SVS_DISK_ERR_DISK_FULL,                 /*磁盘满*/
        SVS_DISK_ERR_RAID_FULL,                 /*Raid组满*/
        SVS_DISK_ERR_DISK_BAD,                  /*磁盘坏*/
        SVS_DISK_ERR_DISK_MOUNT_OFFLINE,         /*磁盘mount掉线*/
        SVS_DISK_ERR_DISK_FS_INVALID,           /*磁盘文件系统不可用*/
        SVS_DISK_ERR_DISK_FS_READONLY,          /*磁盘文件系统只读*/
        SVS_DISK_ERR_DISK_OFFLINE_CONF,         /*磁盘不再配置列表，下线*/
        SVS_DISK_ERR_RAID_NO_OUTPUT_THREAD,     /*raid组没有可用的输出线程*/
        CAC_RT_Record_Server::instance()->max_lens(),
        threadNumPerRaid*syncRaidNum,
        threadNumPerRaid,
        freeThreadGroupNum,
        m_raidTotal);

    // 所有raid组的总览信息
    int32_t i = 0;
    for (i = 0; i < m_raidTotal ; i++)
    {
        writeLen = ACE_OS::strlen(strBufferInfo);
        //缓冲区已用完，退出循环
        leaveLen = (int32_t)(bufferLen-writeLen);
        if( 0 == leaveLen )
        {
            break;
        }

        pstRecordRaid = &m_raidList[i];
        diskIndex = pstRecordRaid->output_disk();
        diskIndex = (diskIndex > 0) ? diskIndex:0 ;
        (void)ACE_OS::snprintf(   &strBufferInfo[writeLen],
            (uint32_t)leaveLen,
            "\t [%d]      [%d]        [%d]             [%d]               [%u]        [%d]     [%d]   [%d:%s]\n",
            i,
            pstRecordRaid->get_capacity_camera_num(),
            pstRecordRaid->get_camera_num(),
            pstRecordRaid->get_write_thread_start_id(),
            CAC_RT_Record_Server::instance()->output_thread_num_per_raid(),
            pstRecordRaid->raid_err_type(),
            pstRecordRaid->disk_err_type(diskIndex),
            diskIndex,
            pstRecordRaid->disk_path(diskIndex));
    }

    // 各个raid组的详细信息
    for (i = 0; i < m_raidTotal ; i++)
    {
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //缓冲区已用完，退出循环
        if( 0 >= leaveLen )
        {
            break;
        }

        (void)snprintf(&strBufferInfo[writeLen],(uint32_t)leaveLen,"\n\t      RaidIndex [%d] tail information: \n",i);
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //缓冲区已用完，退出循环
        if( 0 >= leaveLen )
        {
            break;
        }

        (void)m_raidList[i].debug_raid_tail_info(&strBufferInfo[writeLen], (uint32_t)leaveLen);
    }

    double totalSize = m_IndexFileDisk->total_size() / RTRECORD_UNIT_K;
    double freeSize = m_IndexFileDisk->free_size() / RTRECORD_UNIT_K;
    double limitSize = RECORD_INDEX_FILE_DISK_LIMIT / RTRECORD_UNIT_K;
    writeLen = ACE_OS::strlen(strBufferInfo);
    leaveLen = (int32_t)(bufferLen-writeLen);
    (void)ACE_OS::snprintf(   &strBufferInfo[writeLen],
            (uint32_t)leaveLen,
            "\n\tIndexFile[%s] MountFlag[%d] ErrFlag[%d] TotalSize[%.3f]MB FreeSize[%.3f]MB LimitSize[%.3f]MB\n",
            m_IndexFileDisk->path(),
            m_IndexFileDisk->type(),
            m_IndexFileDisk->flag(),
            totalSize,
            freeSize,
            limitSize);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : debugGetWriteThreadInfo
 功能描述  : telnet获取写录像线程上的镜头信息
 输入参数  : char *strBufferInfo:缓存区，存放raid组信息
             const uint32_t bufferLen:缓存区长度
 输出参数  : char *strBufferInfo:raid组信息
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::debugGetWriteThreadInfo( char *strBufferInfo,const uint32_t bufferLen )
{
    // 如果未初始化好，则不能调试
    if(m_bThreadFlag)
    {
        return SVS_RESULT_FAILURE;
    }

    memset(strBufferInfo, 0, bufferLen);
    size_t writeLen = 0;
    int32_t leaveLen = 0;

    // 各个raid组的详细信息
    for (int32_t i = 0; i < m_raidTotal ; i++)
    {
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //缓冲区已用完，退出循环
        if( 0 >= leaveLen )
        {
            break;
        }

        (void)snprintf(&strBufferInfo[writeLen],
            (uint32_t)leaveLen,
            "\n\t      RaidIndex [%d] Camera Load Info of Media Write Thread : \n",i);
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //缓冲区已用完，退出循环
        if( 0 >= leaveLen )
        {
            break;
        }

        (void)m_raidList[i].debug_write_thread_load(&strBufferInfo[writeLen], (uint32_t)leaveLen);
    }
    return SVS_RESULT_OK;
}

int32_t SVS_Disk_Manager::statCamreaListOnRaid( char *strBufferInfo,const uint32_t bufferLen )
{
    // 如果未初始化好，则不能调试
    if(m_bThreadFlag)
    {
        return SVS_RESULT_FAILURE;
    }

    memset(strBufferInfo, 0, bufferLen);
    size_t writeLen = 0;
    int32_t leaveLen = 0;

    // 各个raid组的详细信息
    for (int32_t i = 0; i < m_raidTotal ; i++)
    {
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //缓冲区已用完，退出循环
        if( 0 >= leaveLen )
        {
            break;
        }

        (void)snprintf(&strBufferInfo[writeLen],(uint32_t)leaveLen,"\n\t      RaidIndex [%d] camera list: \n",i);
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //缓冲区已用完，退出循环
        if( 0 >= leaveLen )
        {
            break;
        }

        (void)m_raidList[i].statCamreaListOnRaid(&strBufferInfo[writeLen], (uint32_t)leaveLen);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : diskFullDelRecord
 功能描述  : 盘满删除录像处理。
             如果是盘满删除录像方式，如果磁盘已经进入告警区域以下，则开始启动盘满
             删除操作；
             如果不是盘满删除录像方式，则不用盘满删除；
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
int32_t SVS_Disk_Manager::diskFullDelRecord()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_diskFullDelingFlagMutex, SVS_RESULT_FAILURE);

    CAC_Record_Raid * pstRecordRaid = NULL;
    int32_t raidBestStatus = SVS_DISK_ERR_RAID_NO_OUTPUT_THREAD;
    // 如果所有raid组是否有已经进入告警区域以下，则添加盘满删除任务
    for(int32_t i = 0; i < m_raidTotal; i++)
    {
        pstRecordRaid = m_raidList + i;
        if(SVS_DISK_ERR_RAID_NO_OUTPUT_THREAD == pstRecordRaid->raid_err_type())
        {
            if(pstRecordRaid->GetDiskBestStatus() < raidBestStatus)
            {
                raidBestStatus = pstRecordRaid->GetDiskBestStatus();
            }
        }
        else if(pstRecordRaid->raid_err_type() < raidBestStatus)
        {
            raidBestStatus = pstRecordRaid->raid_err_type();
        }
    }

    // 如果raid组存在正常的磁盘，则不用创建raid组盘满删除任务
    if (SVS_DISK_ERR_OK == raidBestStatus)
    {
        m_diskFullStartFlag = false;  // 设置结束本轮录像删除任务，可以重复设置
        return SVS_RESULT_OK;
    }

    // 如果状态最好的raid已经进入的告警区域，无论是否已经启动了删除录像录像，则都继续添加删除任务
    if ((SVS_DISK_ERR_RAID_ALARM <= raidBestStatus)
        &&(SVS_DISK_ERR_RAID_FULL >= raidBestStatus))//磁盘异常,文件系统异常等不启动盘满删除
    {
        if(OVERWRITE_RECORD == CAC_RT_Record_Server::instance()->delete_record_strategy())
        {
            SVS_LOG((SVS_LM_WARNING,
                "Best raid status is below alarm. Add disk full delete task.raidBestStatus[%d],",
                raidBestStatus));

            if (SVS_RESULT_OK == send_disk_full_alarm())
            {
                m_diskFullStartFlag = true;  // 设置启动了本轮录像删除任务，可以重复设置
                return SVS_RESULT_OK;
            }
        }
    }
    // 如果已经启动了盘满删除，但是没有删除到告警恢复值以上，则继续删除
    else if((m_diskFullStartFlag) && (SVS_DISK_ERR_RAID_ALARM_RESUME <= raidBestStatus))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Alread begin to disk full delete, "
            "but best raid status is below alarm resume, "
            "Add disk full delete task.raidBestStatus[%d].",
            raidBestStatus));

        if (SVS_RESULT_OK == send_disk_full_alarm())
        {
            return SVS_RESULT_OK;
        }
    }

    // 其它情况，虽然磁盘进入了磁盘恢复区间，但是还没进入过告警区域，还没启动盘满删除，则不用处理

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : send_disk_full_alarm
功能描述  : 向存储管理发送磁盘满告警
输入参数  :
输出参数  :
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Disk_Manager::send_disk_full_alarm()const
{
    // 申请空间
    ACE_Message_Block *pAlarmMb = NULL;

    pAlarmMb = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
                                    sizeof(SVS_RECORDSVR_STORE_DISKALARM_REQ_MSG));
    if (NULL == pAlarmMb)
    {
        SVS_LOG((SVS_LM_ERROR, "Allocate message block failed. Send disk full alarm fail."));

        return SVS_RESULT_FAILURE;
    }

    //构造盘满告警消息
    SVS_RECORDSVR_STORE_DISKALARM_REQ_MSG *pAlarmMsg =
        (SVS_RECORDSVR_STORE_DISKALARM_REQ_MSG *)(void *)pAlarmMb->rd_ptr();
    memset((void *)pAlarmMsg, 0x0, sizeof(SVS_RECORDSVR_STORE_DISKALARM_REQ_MSG));
    FillCommonHeader(pAlarmMsg->Header,
                SVS_MSG_TYPE_RECORDSVR_STORE_DISKALARM_REQ,
                CAC_RT_Record_Server::instance()->transaction_no(),
                sizeof(SVS_RECORDSVR_STORE_DISKALARM_REQ_MSG));
    pAlarmMsg->AlarmCode = SVS_ALARM_CODE_RECORD_STORE_FULL;
    SetNLSDeviceId((uint8_t *)pAlarmMsg->SvrID,
                sizeof(pAlarmMsg->SvrID),
                (const char *)CAC_RT_Record_Server::instance()->str_id());
    pAlarmMb->wr_ptr(sizeof(SVS_RECORDSVR_STORE_DISKALARM_REQ_MSG));

    // 删除任务入队列
    ACE_Time_Value tvTimeout(ACE_OS::gettimeofday());
    tvTimeout.set(tvTimeout.sec(), tvTimeout.usec() + DEL_RECORD_TASK_ENQUEUE_TIMEOUT);
    int32_t iEnQueueRet = CAC_RT_Record_Server::instance()->send_msg(pAlarmMb);

    if (SVS_RESULT_FAILURE == iEnQueueRet)
    {
        SVS_LOG((SVS_LM_ERROR, "Disk full alarm enqueue failed."));

        g_p_msg_buffer->free_mb(pAlarmMb);

        return SVS_ERR_QUEUE;//lint !e429 already release
    }

    SVS_LOG((SVS_LM_INFO,"Disk full alarm send succeeded."));

    return SVS_RESULT_OK;//lint !e429 already enqueue
}//lint !e429 already enqueue


int32_t SVS_Disk_Manager::allocOutputThreadIndex(uint32_t &ulThreadIndex)
{
    ACE_Guard<ACE_Mutex> locker(m_FreeOutputThreadIndexMutex);
    if (m_freeOutputThreadIndexList.empty())
    {
//        SVS_LOG((SVS_LM_WARNING, "Get output thread failed. the list is empty."));

        return SVS_RESULT_FAILURE;
    }

    ulThreadIndex = m_freeOutputThreadIndexList.front();
    m_freeOutputThreadIndexList.pop_front();

    return SVS_RESULT_OK;
}

void SVS_Disk_Manager::freeOutputThreadIndex(uint32_t ulThreadIndex)
{
    ACE_Guard<ACE_Mutex> locker(m_FreeOutputThreadIndexMutex);
    m_freeOutputThreadIndexList.push_back(ulThreadIndex);

    return ;
}

int32_t SVS_Disk_Manager::getFreeOutputThreadGroupNum()
{
    ACE_Guard<ACE_Mutex> locker(m_FreeOutputThreadIndexMutex);

    return (int32_t)m_freeOutputThreadIndexList.size();
}

bool SVS_Disk_Manager::isRecordIndexpathuseable()const
{
    if(NULL == m_IndexFileDisk)
    {
        SVS_LOG((SVS_LM_WARNING, "Record indexpath is unusable as the object is NULL."));

        return false;
    }

    if(SVS_DISK_ERR_DISK_FULL <= m_IndexFileDisk->flag())
    {
        SVS_LOG((SVS_LM_WARNING,
            "Record indexpath is unusable.RecordIndexPath[%s],flag[%d].",
            m_IndexFileDisk->path(),
            m_IndexFileDisk->flag()));

        return false;
    }

    return true;
}

