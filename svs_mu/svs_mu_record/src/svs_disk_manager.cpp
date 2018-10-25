/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : SVS_Disk_Manager.cpp
  �� �� ��   : ����
  ��������   : 2009��8��21��
  ����޸�   :
  ��������   : ���̹�������,ʵ���˶Դ����ļ��Ĺ��������쳣������;
               ����ʵ�����ļ��Ķ�д������IO Schedulerģ���IO�ĵ���
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2009��8��21��
    �޸�����   : �����ļ�

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
    // �߳����б�ʶ
    m_bThreadFlag = false;

    // �����Դ��̼�ⶨʱ��
    m_checkDiskTimer = -1;

    // raid������
    m_raidTotal = 0;

    // ������ڣ���λ����
    m_checkInterval = 0;

    // ���̿��ÿռ����ޣ���λKB
    m_lowerLimit = 0;

    // ���̸澯��ֵ����λKB
    m_alarmLimit = 0;

    // ���̸澯�ָ�ֵ����λKB
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
            // �ͷŶ�ʱ��
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
 �� �� ��  : init
 ��������  : ��ʼ�����̹���ģ�飬������������õĴ���ȫ·�������ݽ����������raid
             ���ÿ��raid���µĴ��̡����ݴ���Ĵ��̼�����ڣ������Լ����̡�����
             ����Ĵ��̿ռ������ֵ���й������
 �������  : int32_t nInterval :���̼������
             uint32_t lowerLimit : ����ֹͣ¼�����ֵ
             uint32_t alarmLimit : ����ʣ��ռ�澯����ֵ
             uint32_t alarmResumeLimit : ����ʣ��ռ�澯�ָ�ֵ
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
*****************************************************************************/
int32_t SVS_Disk_Manager::initialize(int32_t nInterval ,
                               uint32_t lowerLimit,
                               uint32_t alarmLimit ,
                               uint32_t alarmResumeLimit)
{
    // ������̼���õĻ��������Ѿ���������4K�����
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

    //��M����ΪK
    m_lowerLimit = lowerLimit * 1024;
    m_alarmLimit = alarmLimit * 1024;
    m_alarmResumeLimit = alarmResumeLimit * 1024;

    // ���������ļ�����
    DISK_THRESHOLD diskThreshold;
    diskThreshold.m_lowerLimit = RECORD_INDEX_FILE_DISK_LIMIT;
    diskThreshold.m_alarmLimit = RECORD_INDEX_FILE_DISK_LIMIT;
    diskThreshold.m_alarmResumeLimit = RECORD_INDEX_FILE_DISK_LIMIT;
    const char* strIndexpath= CAC_RT_Record_Server::instance()->get_record_indexfile_path();
    // ���ݴ���ÿ��raid�����SVS_LUN_MAX_NUM�����̣�С�����ΪSVS_LUN_MAX_NUM-1,��SVS_LUN_MAX_NUM��ʶ����·��
    try
    {
        m_IndexFileDisk = new CAC_Record_Disk(strIndexpath, SVS_LUN_MAX_NUM, diskThreshold);
    }catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Allocate memory failed.Create index file disk object failed."));
        return SVS_ERR_NOMEM;
    }

    // ���úͼ������Ӵ��̵�����
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

    // ���������̽��м��
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

    // ��ʼ���ļ��б�
    // IO���и���
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
 �� �� ��  : open
 ��������  : �򿪶��ڼ������߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
*****************************************************************************/
int32_t SVS_Disk_Manager::open(void *argv)
{
    if (NULL == argv)
    {
        ; //only for compile warnning
    }

    // ���µ���������Ϣ����ˮλ
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

    // ������ʱ�����̶�ʱ��
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

    // ����IO�����߳�
    if (SVS_RESULT_OK != SVS_IO_Scheduler::instance().open((void*)0))
    {
        SVS_LOG((SVS_LM_ERROR, "Open IO schedule service failed."));
        return SVS_RESULT_FAILURE;
    }

    {
        ACE_Guard<ACE_Mutex> locker(m_FreeOutputThreadIndexMutex);
        // ����¼��ɹ���,������߳�ID��������б�,ֵ����ÿ��raid����õ��̵߳���ʼID
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
 �� �� ��  : svc
 ��������  : ���̼���������߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
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

        // �ͷ��ڴ�
        g_p_msg_buffer->free_mb(pMsg);

        statReporter.ReportStat();
    }
    g_p_msg_buffer->clear();
    SVS_LOG( ( SVS_LM_WARNING, "The check disk thread thread stop to run." ) );
    return 0;
}

/*****************************************************************************
 �� �� ��  : close
 ��������  : �رն���,����ǰ�����һ������
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
*****************************************************************************/
int32_t SVS_Disk_Manager::close(u_long)
{
    m_bThreadFlag = true;

    // �ͷŶ�ʱ��
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
 �� �� ��  : openFile
 ��������  : �½����ߴ�һ���ļ���ѡ��һ����������(��ͷ������)��raid�鴴��
             ¼���ļ�
 �������  : const char* filePath:¼���ļ����·������ͷ/����/¼���ļ���
 �������  : uint64_t &fileSize:��ǰ¼���ļ���С
             int32_t &vfd:���ļ����
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
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

    //���ô�������Ϊ��ǰRAID���ϵĵ�ǰ���̣������������ͬraid����ͬʱд��ͬ�Ĵ���
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
    // ��������Ŀ¼
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

    // ����¼���ļ�
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

        // �����stat�ļ�ʧ�ܣ������ϱ����̴���
        if(SVS_ERR_STATFILE != iRet)
        {
            (void)createDiskExceptMsg(minCameraRaidIndex,pFileList->DiskIndex,iRet);
        }
        return iRet;
    }

    pFileList->SysFd = fd;
    pFileList->VfdStatus = SYSFD_USABLE_STATUS;

    // ���̾�ͷ������1;
    (void)pstRecordRaid->increase_camera_num(cameraId,pFileList->IoQueueIndex);

    //memcpy(pFileList->CameraId, cameraId, SVS_DEVICEID_LEN);
    //For �豸ID���룬ʹ���µķ������ñ����ڴ����豸ID
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
 �� �� ��  : closeFile
 ��������  : �ͷ����ļ������Դ��������һ��������Ϣ��IO_Schduele,�ļ��Ĺ�����
             IO_Schedule�رա�
 �������  : int32_t vFd:���ļ����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
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

    // raid�龵ͷ����1
    int32_t raidIndex = m_fileList[vFd].RaidIndex;

    SVS_LOG((SVS_LM_INFO,
        "Will close file.The camera numberloaded on this raid is [%d]."
        "vfd[%d],system fd[%d],file name[%s].",
        m_raidList[raidIndex].get_camera_num(),
        vFd,
        m_fileList[vFd].SysFd,
        m_fileList[vFd].RecordFilePath));

    // ����һ���ر��ļ���Ϣ��IO_Schedule
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
 �� �� ��  : writeFile
 ��������  : ���ݴ�������ļ�������ҵ���Ӧ����ʵ�ļ������
 �������  : int32_t vFd:���ļ����
             ACE_Message_Block *pData:��Ҫд������
             IO_CallBack_Func pFunc:�ص�����
             void *pParam:д����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
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

    // �жϾ���Ƿ���ñ�ʶ
    if((-1 >= pVFile->SysFd) || (SYSFD_USABLE_STATUS != pVFile->VfdStatus))
    {
        // ����ļ���������ã�����Ҫд���̣���֪ͨ�ϲ�ʧ��
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

    // ����һ��д������Ϣ��IO_Schedule
    int32_t ret = SVS_IO_Scheduler::instance().add_IO(pVFile->SysFd,
                                                   pVFile->IoQueueIndex,
                                                   IO_WRITE,
                                                   pData,
                                                   pFunc,
                                                  pParam);
    return ret;
}

/*****************************************************************************
 �� �� ��  : writeFile
 ��������  : ���ݴ�������ļ��������ҵ���Ӧ����ʵ�ļ������
 �������  : int32_t vFd:���ļ����
             ACE_Message_Block *pData:��Ž�Ҫ����������
             IO_CallBack_Func pFunc:�ص�����
             void *pParam:������
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
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

    // ����һ��д������Ϣ��IO_Schedule
    int32_t ret = SVS_IO_Scheduler::instance().add_IO(m_fileList[vFd].SysFd,
                                                   m_fileList[vFd].IoQueueIndex,
                                                   IO_READ,
                                                   pData,
                                                   pFunc,
                                                   pParam);

    return ret;
}

/*****************************************************************************
 �� �� ��  : freeVfd
 ��������  : �ͷ�VFD
 �������  : int32_t fd:�ļ����
 �������  : NA
 �� �� ֵ  : NA
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
 �� �� ��  : notifyDiskExcept
 ��������  : IO Scheduler���ýӿڣ�֪ͨ���̼�����ģ���д��̻��ļ������쳣
 �������  : int32_t fd:�ļ����
             int32_t errCode:IO��������
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
*****************************************************************************/
int32_t SVS_Disk_Manager::notifyDiskExcept(int32_t fd, int32_t errCode)
{
    // ����fd��Ӧ�Ĵ��̷���
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

    // ������ļ�����쳣�����ʶ���̲�����,�ϲ�д¼��ʧ�ܺ󣬻����´��ļ�
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
 �� �� ��  : getFileFullName
 ��������  : ���ݴ�������ļ�ID��ȡ¼���ļ�ȫ·��
 �������  : int32_t vFd:���ļ����
 �������  : char *&szFileName:¼���ļ�ȫ·��
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
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
 �� �� ��  : processCheckDiskTask
 ��������  : �������������������Դ�������Լ����̺�IO�����쳣
 �������  : const ACE_Message_Block * mb:������Ϣ��
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t SVS_Disk_Manager::processCheckDiskTask(const ACE_Message_Block * mb)
{
    SVS_DISK_CHECK_TASK * pTaskMsg = (SVS_DISK_CHECK_TASK*)(void*)mb->rd_ptr();

    switch(pTaskMsg->TaskType)
    {
        // ����������Լ�������Ϣ,����������д���
        case SVS_DISK_CHECK_TASK_TIMER:
        {
            // ��ȡmount��Ϣ
            getMounts();

            // �����Լ����̿ռ�
            (void)periodCheckRaid();

            // ����ɾ������
            (void)diskFullDelRecord();
        }
        break;

        // ����Ǵ����쳣������񣬼���Ǹ�����
        case SVS_DISK_CHECK_TASK_EXCEPT:
        {
            // ��ȡmount��Ϣ
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
 �� �� ��  : timerCallback
 ��������  : ��ʱ���ص�,����������Լ����̶�ʱ�����򴴽����̼������������
 �������  : int32_t eventId:�¼�����
             int32_t timerId:��ʱ��ID
 �������  : NA
 �� �� ֵ  : NA
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

    // ������Ϣ,ֻ��Ҫ������Ϣ���ͼ��ɣ�����Ϣ���ͱ������д���
    SVS_DISK_CHECK_TASK* pMsg = (SVS_DISK_CHECK_TASK*)(void*)mb->wr_ptr();
    pMsg->TaskType = SVS_DISK_CHECK_TASK_TIMER;
    pMsg->RaidIndex = -1;
    pMsg->DiskIndex = -1;
    pMsg->ErrCode = 0;

    // ��Ϣ�����
    ACE_Time_Value tvTimeout(ACE_OS::gettimeofday());
    if(SVS_RESULT_FAILURE == SVS_Disk_Manager::instance().enqueueCheckDiskTask( mb ,&tvTimeout))
    {
        SVS_LOG((SVS_LM_ERROR, "Check disk task message enqueue failed."));
        g_p_msg_buffer->free_mb(mb);
    }

    return;
}

/*****************************************************************************
 �� �� ��  : refreshDiskList
 ��������  : ��̬ˢ�´������ã�����������ַ�������������б�ÿ�����̶����Լ�
             �����ԣ�����������raid���id������·���������Ƿ���Ԫ���ݷ��롣�Ƚ�
             ��ǰ����ʹ�õĴ��̺ͽ������Ĵ��̣�������������ʹ�õĴ��̣����ĳ
             ���̣�����Ӵ��̶��������ǰ����ʹ�õ�ĳ���̣���������û�иã�����
             ���øô��̲�����״̬
 �������  : char * strDiskPath:�����õ����еĴ����б�
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
*****************************************************************************/
int32_t SVS_Disk_Manager::refreshDiskList(const char * strDiskPath)
{
    // 1.�����ж�
    if ((NULL == strDiskPath) || ('\0' == *strDiskPath))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to init disk raid, the pointer strDiskPath is invalid. "
            "strDiskPath[0x%08x] or length is 0.",
            strDiskPath));

        return SVS_ERR_PARAM;
    }

    // 2.���¶�ȡmountֵ
    getMounts();

    // 3.���������б�����
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
    // 4.������ǰ����ʹ�õĴ��̣����ж��µ������ﻹ��û�иô��̣����û���ˣ���Ҫ��
    // �����Ĵ��̿��ñ�ʶ�óɲ�����״̬
    uint32_t uLoop = 0;
    for(uLoop = 0; uLoop < (uint32_t)m_raidTotal; uLoop++)
    {
        nRet = m_raidList[uLoop].refreshDiskInfo(diskInfoVec);
    }

    // 5.���������õĴ��̣��ڵ�ǰ����ʹ�õĴ�����Ѱ���Լ�������ҵ�����ˢ���Լ���״̬
    // ���û���ҵ��Լ��������һ���µĴ��̶��󵽸�raid����
    uint32_t vectSize = diskInfoVec.size();
    DISK_INFO * pDiskInfo = NULL;
    for(uLoop = 0; uLoop < vectSize; uLoop++)
    {
        pDiskInfo = &diskInfoVec[uLoop];

        // ���raid��index�����ˣ��������һ��raid��
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

        // ������Ӵ���
        (void)m_raidList[pDiskInfo->RaidInfo.RaidIndex].tryAddDisk(*pDiskInfo);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : parsDiskListConf
 ��������  : ���������б���Ϣ���������õĸ�ʽ:
             ����֮����','�ֿ���raid�����'|'�ֿ�����'[1]'��'[0]'�ֱ��ʾ֧��Ԫ����
             ����Ͳ�֧��Ԫ���ݷ��룬���Բ����ã�Ĭ�ϲ�֧��Ԫ���ݷ���
             RecordFilePath=/mnt/svs_md0[1],/mnt/svs_md1[1]|/mnt/svs_md2[0],/mnt/svs_md3
             ǰ������������raid��1���Ҷ���֧��Ԫ���ݷ���ģ���������������raid2
             ���Ҷ��ǲ�֧��Ԫ���ݷ���ģ����һ������ʹ��Ĭ��ֵ
 �������  : const char *strRadiList:����raid���������Ϣ
 �������  : vector <int32_t>& pos:���ÿ��raid����strRadiList��ƫ��ֵ���ִ���λ��
 �� �� ֵ  : NA
*****************************************************************************/
int32_t SVS_Disk_Manager::parseDiskListConf(const char * strDiskPath, DISK_INFO_VECTOR& diskInfoVect)const
{
    char* startPos = (char*)strDiskPath;
    char* endPos = NULL;
    int32_t raidIndex = 0;
    char diskPathTemp[DISK_NAME_SIZE] = {0,};

    bool BCtrl = true;
    // ���� ��ע��:���һ������·������û��','��ʶ
    do
    {
        // ��·��������
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
            // ����������µ�raid��
            if ('|' == *endPos)
            {
                newRaidFlag = true;
            }
        }

        // �����ʵ���ڽ��������ߵ�ǰ���ڽ����Ĵ����Ѿ�������һ������·���ˣ����������
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

        // raid����
        diskInfoTemp.RaidInfo.RaidIndex = raidIndex;

        diskInfoVect.push_back(diskInfoTemp);

    }while(BCtrl);

    return SVS_RESULT_OK;
}

int32_t SVS_Disk_Manager::parseDiskInfo(char*diskPath, DISK_INFO&diskInfo)const
{
    SVS_LOG((SVS_LM_INFO, "Begin to parse disk information. conf[%s].", diskPath));
    // ������ǰ���̵�����
    (void)memset(&diskInfo, 0, sizeof(diskInfo));

    int32_t medaDataPartedFlag = SVS_META_DATA_DISPARTED;
    char *pos = diskPath + strlen(diskPath) - 1;
    for(; diskPath != pos ; --pos)
    {
        // ����ҵ�����������
        if ('[' == *pos)
        {
            medaDataPartedFlag = *(++pos) - 48;
            break;
        }
    }

    // ������̱�ʶ��ȷ
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
    // ���̿�ʼλ��
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

    // Ϊ�����ҵ�mount��ȥ�����һ��'/',����ע����·��'/'
    unsigned diskPathLen = strlen(diskInfo.DiskPath);

    if ((1 != diskPathLen) && ('/' == diskInfo.DiskPath[diskPathLen - 1 ]))
    {
        diskInfo.DiskPath[diskPathLen - 1 ] = '\0';
    }

    // �ж�mount����
    diskInfo.DiskMountTypeFlag = SVS_DISK_TYPE_MOUNTDISK;
    int32_t nRet = checkMountOnlineStatus(diskInfo.DiskPath);

    if((SVS_RESULT_OK != nRet)
        &&(false == diskInfo.MedaDataPartedFlag))//�����Ԫ���ݷ����Ĭ����������
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

    // ����·�����Ĵ���
    for(;(!(' ' == *tempPos || '\0' == *tempPos || ',' == *tempPos || '|' == *tempPos || '[' == *tempPos)); ++tempPos)
    {
        //nothing
    }

    return tempPos;
}

/*****************************************************************************
 �� �� ��  : periodCheckRaid
 ��������  : ������еĴ��̣����ĳ��raid���ڵĵ�ǰ����ʹ�õ�lun��Ҫ�л�����һ��
             ���л������л����̸澯��֪ͨ�ϲ�ҵ��㣬�����л����̵�׼��
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t SVS_Disk_Manager::periodCheckRaid()
{
    SVS_LOG((SVS_LM_DEBUG, "Begin to check disk for period check task."));
    int32_t raidCapacityCameraNum = (int32_t)CAC_RT_Record_Server::instance()->max_lens_per_raid();
    for (int32_t i = 0; i < m_raidTotal; i++)
    {
        CAC_Record_Raid* pstRecordRaid = NULL;
        pstRecordRaid = m_raidList + i ;


        // ��¼��鵱ǰ����ʹ�õĴ����±�ֵ��ԭ���Ĵ����±��Ƿ����
        // ��¼��ǰ���̵Ĵ�������
        int32_t nOldDiskIndex,nNewDiskIndex = 0;
        int32_t nOldRaidErrCode,nNewRaidErrCode = 0;
        nOldDiskIndex = pstRecordRaid->output_disk();
        nOldRaidErrCode = pstRecordRaid->raid_err_type();

        // �������д���
        int32_t nStart,nEnd = 0;
        if((0 > nOldDiskIndex) || (nOldDiskIndex >= pstRecordRaid->disk_total()))
        {
            nOldDiskIndex = 0;
        }

        nStart = nOldDiskIndex;
        nEnd   = (pstRecordRaid->disk_total() - 1) + nOldDiskIndex;
        (void)pstRecordRaid->select_disk(nStart,nEnd);

        // ���raid���ڵ�lun�������ã�����raid�澯,����raid������
        // raid��澯��lun�Ͳ��ø澯��
        nNewRaidErrCode = pstRecordRaid->raid_err_type();
        if((SVS_DISK_ERR_RAID_FULL <= nNewRaidErrCode )
        && (nOldRaidErrCode != nNewRaidErrCode))
        {
            pstRecordRaid->set_capacity_camera_num(0);

            if (SVS_DISK_ERR_RAID_FULL > nOldRaidErrCode)// �ո��������Ĳ��ϱ��쳣
            {
                (void)notifyExcept(i, pstRecordRaid->raid_err_type());

                // ��ʱ�����ϱ�
                CAC_RT_Record_Server::instance()->add_load_report_task();
            }

            continue;
        }
        // raid�������ָ�����ָ�������ʶ
        else if((SVS_DISK_ERR_RAID_FULL <= nOldRaidErrCode)
             && (SVS_DISK_ERR_RAID_ALARM >= nNewRaidErrCode))
        {
            pstRecordRaid->set_capacity_camera_num(raidCapacityCameraNum);
        }
        else
        {
            // ����л���lun�����͸澯
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
 �� �� ��  : periodCheckRaid
 ��������  : ��������쳣֪ͨ��Ϣ����ⱨ�쳣�Ĵ����������쳣��������ȷʵ�쳣��
             ��ʶ�ô��̲����ã�����ʱ����֪ͨҵ���
 �������  : const SVS_DISK_CHECK_TASK * pTaskMsg:�����쳣������Ϣ
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
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

    // ������
    CAC_Record_Raid * pRaid = NULL;
    pRaid = &m_raidList[raidIndex];

    int32_t diskIndex = pTaskMsg->DiskIndex;
    SVS_LOG((SVS_LM_INFO,
        "Begin to handle disk exception message."
        "raid path[%s],disk path[%s]",
        pRaid->raid_path(),
        pRaid->disk_path(diskIndex)));

    (void)pRaid->check_disk(diskIndex);

//1 TODO raid�������½���ô����raid����ô����?

    // �����ǰ����ʹ�õĴ����Ѿ�����������̣������ϱ��澯
    if(pRaid->output_disk() != diskIndex)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Handle disk exception message finished.Disk has been switched."
            "raid path[%s],disk path[%s]",
            pRaid->raid_path(),
            pRaid->disk_path(diskIndex)));
        return SVS_RESULT_OK;
    }

    // ����������û���쳣��Ҳ��������֪ͨҵ���
    if(SVS_DISK_ERR_DISK_BAD > pRaid->disk_err_type(diskIndex))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Handle disk exception message finished.Disk is OK."
            "raid path[%s],disk path[%s]",
            pRaid->raid_path(),
            pRaid->disk_path(diskIndex)));
        return SVS_RESULT_OK;
    }

    // �ô������쳣����ͬʱ�ϲ�ҵ���
    (void)notifyExcept(raidIndex, pRaid->disk_err_type(diskIndex));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : notifyExcept
 ��������  : �����쳣֪ͨ,��⵽�����쳣�󣬵����쳣֪ͨ�ӿڣ���ʱ֪ͨҵ���
 �������  : const SVS_DISK_CHECK_TASK * pTaskMsg:�����쳣������Ϣ
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t SVS_Disk_Manager::notifyExcept(int32_t raidId,int32_t diskErr)
{
    std::vector<string> cameraVec;
    cameraVec.clear();

    // ��ȡ��raid���ϵ��ļ��б�.����¼���ļ�������״̬
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
 �� �� ��  : createDir
 ��������  : ����Ŀ¼�����ȳ��Դ�����ͷ��Ŀ¼��Ȼ��ʼ�����ļ�Ŀ¼
 �������  : const char* diskPath:����Ŀ¼
             const char* filePath:¼���ļ����·������ͷID/����/¼���ļ�����
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� ������
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
        // ������ͷĿ¼
        (void)snprintf(strFullDir,MAXNAMELEN,"%s/%s",diskPath,camerId);

        if(0 != ACE_OS::mkdir(strFullDir))
        {
            nErrNo = ACE_OS::last_error();
            if(EEXIST != nErrNo) // ��������Ѿ����ڴ���
            {
                SVS_LOG( (SVS_LM_ERROR,
                    "Create record directory failed.directory[%s]",
                    strFullDir));
                bCreateRet = false;
                break;
            }
        }

        // ��������Ŀ¼
        (void)snprintf(strFullDir,MAXNAMELEN,"%s/%s/%s",diskPath,camerId,strDay);
        if(0 != ACE_OS::mkdir(strFullDir))
        {
            nErrNo = ACE_OS::last_error();
            if(EEXIST != nErrNo) // ��������Ѿ����ڴ���
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
 �� �� ��  : createFile
 ��������  : ����¼���ļ�
 �������  : const char* fileFullPath:¼���ļ�����·��
 �������  : uint64_t &fileSize:��ǰ¼���ļ���С
             int32_t &vfd:���ļ����
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
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

    // ���Ԫ���ݺ���Ƶ���ݷ���
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

    // ��ǰ¼���ļ���С
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

    // �¼�¼��ʱ̫����־��ֱ����0
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
 �� �� ��  : createDiskExceptMsg
 ��������  : ���������쳣��Ϣ��������̼��������У��д��̼���̴߳���
 �������  : int32_t raidIndex:raid�������
             int32_t diskIndex:raid���ڵĴ�������
             int32_t errCode:�����쳣������
 �������  : NA
 �� �� ֵ  : NA
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

    // ������Ϣ,ֻ��Ҫ������Ϣ���ͼ��ɣ�����Ϣ���ͱ������д���
    SVS_DISK_CHECK_TASK* pMsg = (SVS_DISK_CHECK_TASK*)(void*)mb->wr_ptr();
    pMsg->TaskType = SVS_DISK_CHECK_TASK_EXCEPT;
    pMsg->RaidIndex = raidIndex;
    pMsg->DiskIndex = diskIndex;
    pMsg->ErrCode = errCode;

    // ��Ϣ�����
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
 �� �� ��  : checkUseableRaid
 ��������  : ����Ƿ��п��õĴ���
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� -1
 �޸���ʷ  : 2009-9-24  ����
*****************************************************************************/
int32_t SVS_Disk_Manager::checkUseableRaid()
{
    CAC_Record_Raid* pstRecordRaid = NULL;
    for (int32_t i = 0; i < m_raidTotal; i++)
    {
        pstRecordRaid = m_raidList + i;
        // ������̻���,������һ��
        if (SVS_DISK_ERR_RAID_FULL <= pstRecordRaid->raid_err_type())
        {
            continue;
        }

        // ��������Ѿ��ﵽ�����������raid��
        if(pstRecordRaid->get_camera_num() >= pstRecordRaid->get_capacity_camera_num())
        {
            continue;
        }

        // Raid����ã����سɹ�
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
 �� �� ��  : getAllDiskInfo
 ��������  : ��ȡ����û�л����Ĵ���·����Ϣ
 �������  : NA
 �������  : DISK_INFO_VECTOR & diskInfoVector:����·���б�
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t SVS_Disk_Manager::getAllDiskInfo(DISK_INFO_VECTOR & diskInfoVector)const
{
    diskInfoVector.clear();
    for (int32_t i = 0; i < m_raidTotal; i++)
    {
        // ������̻��ˣ����÷���
        //if (SVS_DISK_ERR_RAID_FULL <= this->m_raidList[i].raid_err_type())
        if (SVS_DISK_ERR_RAID_FULL < this->m_raidList[i].raid_err_type())
        {
            continue;
        }

        // ��ȡ��ǰ���raid�������еĴ�����Ϣ
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
        // ������̻��ˣ����÷���
        if (SVS_DISK_ERR_RAID_FULL <= this->m_raidList[i].raid_err_type())
        {
            continue;
        }

        ++usableRaidNum;
    }

    return usableRaidNum;
}
/*****************************************************************************
 �� �� ��  : time2RecodFileName
 ��������  : ���ݴ����ʱ���������Ŀ¼���ƺ�¼���ļ�����
 �������  : time_t RecordTime:¼��ʱ��
 �������  : char*& strDay:¼�����ڵ�����
             char*&strRecordFileName:¼���Ӧ��¼���ļ�����
 �� �� ֵ  : NA
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

// �����������¼��ľ�ͷ�������·���һ��vfd����
// �����������¼���һ����ͷ��ԭ��ʹ�õ�raid�鲻���ã�����һ��ȫ�µ�raid��
// �����ͷ�Ѿ���¼��������ͬraid����¼��
int32_t SVS_Disk_Manager::allocDisk(int32_t &vFd,int32_t &raidIndex)
{
    SVS_LOG((SVS_LM_DEBUG, "Allocate disk. vf[%d], raid index[%d]",vFd, raidIndex));
    SVS_DISK_VFILE * pFileList = NULL;
    int32_t isNewDisk = SVS_RESULT_OK;
    // ���ļ��б����ҵ�һ�����е��ļ��ռ�,���ϴλ�ȡ��vfdλ�ÿ�ʼ
    if(-1 >= vFd)
    {
        int32_t j = 0;
        int32_t loopStart = m_vfdCursor;
        int32_t loopEnd = m_vfdCursor + RECORD_MAX_LENS_NUM;
        for(j = loopStart; j < loopEnd; j++)
        {
            pFileList = m_fileList + (j % RECORD_MAX_LENS_NUM);

            // �ҵ�һ�����е�vfd
            if( VFD_FREE_STATUS == pFileList->VfdStatus)
            {
                break;
            }
        }

        // û�п��е��ļ��ռ�
        if(loopEnd == j)
        {
            SVS_LOG((SVS_LM_ERROR ,"Open file failed.There is not free file space."));
            return SVS_ERR_NOVFD;
        }

        // �ҵ����õ�vfd�����vfd��������vfd�α��λ��
        isNewDisk = SVS_RESULT_OK;
        vFd = (j % RECORD_MAX_LENS_NUM);
        m_vfdCursor = vFd + 1;
    }
    else // ���ǵ�һ�δ�¼���ļ���������ͬһ��raid�鳣¼��
    {
        raidIndex = m_fileList[vFd].RaidIndex;

        // ��ǰraid����ã���ɹ�����
        if(SVS_DISK_ERR_RAID_FULL > m_raidList[raidIndex].raid_err_type())
        {
            // �����ǰraid���Ѿ��л�lun
            if(m_fileList[vFd].DiskIndex != m_raidList[raidIndex].output_disk())
            {
                return  SVS_ERR_CHANGEDISK;
            }

            return SVS_RESULT_OK;
        }

        // ��ǰraid�鲻���ã���Ҫ����ѡ��һ�����õ�raid��
        isNewDisk = SVS_ERR_CHANGEDISK;
    }

    // ��Ҫ����ѡ��һ���µ�raid��
    // ���㲢����raid��ĸ���
    countRaidLoad();

    CAC_Record_Raid* pstRecordRaid = NULL;
    int32_t minCameraRaidIndex = -1;

    // ѡ��һ�����õģ����������raid�鴴���ļ�
    for (int32_t i = 0; i < m_raidTotal; i++)
    {
        pstRecordRaid = m_raidList + i ;

        // ���raid�鲻���ã�ֱ������
        if(SVS_DISK_ERR_RAID_FULL <= pstRecordRaid->raid_err_type())
        {

            continue;
        }

        // ��������Ѿ��ﵽ�����������raid��
        if(pstRecordRaid->get_camera_num() >= pstRecordRaid->get_capacity_camera_num())
        {
            continue;
        }

        // ����ǵ�һ���ҵ�һ�����õ�raid�飬��ѵ�ǰ�����raid����ΪĿǰ�ҵ��ĸ��������raid��
        if(-1 >= minCameraRaidIndex)
        {
            minCameraRaidIndex = i;
        }

        // ����ҵ�һ�����ظ����raid��,����¸��������raid����±�
        if(m_raidList[minCameraRaidIndex].get_load() > pstRecordRaid->get_load())
        {
            minCameraRaidIndex = i;
        }
    }

    // �������raid�鶼������
    if(0 > minCameraRaidIndex)
    {
        SVS_LOG((SVS_LM_ERROR ,"Open file failed.All raid are unusable."));
        return SVS_ERR_NORAID;
    }

    // ��ȡ����߳�ID
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

    // �ҵ����õ�raid��󣬳ɹ�����
    raidIndex = minCameraRaidIndex;

    return isNewDisk;
}

/*****************************************************************************
 �� �� ��  : countRaidLoad
 ��������  : ����raid��ĸ������������һ�����������raid�顣raid��ĸ��������
             ���̿ռ䡢IO��
 �������  : NA
 �������  : NA
 �� �� ֵ  : ����raid��
*****************************************************************************/
void SVS_Disk_Manager::countRaidLoad()
{
    //1 ����Ҫ��Ӷ�̬����,����ĳraid���ȹ��Ϻ󣬻ָ�
    // 1 ����,ֻ֪ͨ���̿��õ�raid��
    uint32_t diskTotalFreeSize = 0;
    int32_t diskTotalCameraNum = 0;
    CAC_Record_Raid* pstRecordRaid = NULL;
    int32_t i = 0;
    for(; i < m_raidTotal; i++)
    {
        pstRecordRaid = m_raidList + i ;

        // ���raid�鲻���ã�ֱ������
        if(SVS_DISK_ERR_RAID_FULL <= pstRecordRaid->raid_err_type())
        {
            continue;
        }

        // ��������Ѿ��ﵽ�����������raid��
        if(pstRecordRaid->get_camera_num() >= pstRecordRaid->get_capacity_camera_num())
        {
            continue;
        }

        // ͳ������raid����ܾ�ͷ����ʣ��ռ�
        diskTotalCameraNum += pstRecordRaid->get_camera_num();

        diskTotalFreeSize += pstRecordRaid->raid_free_size();

        // 2 IO


        // 3 �������������������صĲ���

    }

    // ���û��������raid��
    if(0 == diskTotalFreeSize)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Count raid load finished .All disk total free size is %d.camera number[%d]",
            diskTotalFreeSize,
            diskTotalCameraNum));
        return;
    }

    // ����ÿ��raid���Ȩ��,ʵ������¼��ľ�ͷ��Ӧ�óе��ľ�ͷ�ı�ֵ
    int32_t raidLoad = 0;
    float cameraInfo = 0;
    float capacityInfo = 0;
    for(i = 0 ; i < m_raidTotal; i++)
    {
        pstRecordRaid = m_raidList + i ;
        // ������еľ�ͷΪ0,�����ǰraid��ռ�Ϊ0
        uint32_t ulRaidFreeSize = pstRecordRaid->raid_free_size();
        if((0 == diskTotalCameraNum) || (0 == ulRaidFreeSize))
        {
            raidLoad = 0;
            continue;
        }

        // ���㸺��ֵ���Ŵ�1000������С���
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
 �� �� ��  : debugGetRaidListInfo
 ��������  : telnet��ȡraid����Ϣ
 �������  : char *strBufferInfo:�����������raid����Ϣ
             const uint32_t bufferLen:����������
 �������  : char *strBufferInfo:raid����Ϣ
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
void SVS_Disk_Manager::getMounts()
{
    m_bIsMountsOk = false;
    FILE                        *pf;
    pf = fopen("/proc/mounts", "r");
    size_t buffSize = sizeof(m_mountsInfo);

    /* ���ļ���һ�㲻��ʧ�ܣ���Ҫ���� */
    // ��������ļ�ʧ�ܣ�����Ϊ���е�mount��Ϣ���ڣ���֤���̿���
    if (NULL == pf)
    {
        SVS_LOG((SVS_LM_WARNING, "Open /proc/mounts failed."));
        return;
    }

    /* ��ȡCPUʹ��ʱ����Ϣ */
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

    // �����ȡ/proc/mountsʧ�ܣ�����Ϊmount����
    if(!m_bIsMountsOk)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Check mount online status failed.Read /proc/mounts failed."));
        return SVS_RESULT_OK;
    }

    //�鿴 /proc/mounts�Ƿ���Ҫ��ѯ�Ĵ�����Ϣ
    char tempDiskPath[DISK_NAME_SIZE];
    (void)snprintf(tempDiskPath, sizeof(tempDiskPath), " %s ", strDiskPath);

    // ���mount����
    if (NULL == strstr(m_mountsInfo, tempDiskPath))
    {
        return SVS_ERR_MOUNTOFFLINE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : debugGetRaidListInfo
 ��������  : telnet��ȡraid����Ϣ
 �������  : char *strBufferInfo:�����������raid����Ϣ
             const uint32_t bufferLen:����������
 �������  : char *strBufferInfo:raid����Ϣ
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t SVS_Disk_Manager::debugGetRaidListInfo( char *strBufferInfo,const uint32_t bufferLen )
{
    // ���δ��ʼ���ã����ܵ���
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

    // ��ȡ¼���߳��������Ϊÿ��raid����߳�����ϵͳ֧�ֵ���󲢷�����raid�����ĳ˻�
    uint32_t threadNumPerRaid = CAC_RT_Record_Server::instance()->output_thread_num_per_raid();
    uint32_t syncRaidNum = CAC_RT_Record_Server::instance()->sync_work_raid_group_num();


    // ��ӡ���̴������б�
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
        SVS_DISK_ERR_OK,                        /*��������*/
        SVS_DISK_ERR_DISK_ALARM_RESUME,         /*���̽���澯Ԥ����Χ*/
        SVS_DISK_ERR_RAID_ALARM_RESUME,         /*Raid�����澯Ԥ����Χ*/
        SVS_DISK_ERR_DISK_ALARM,                /*���̸澯*/
        SVS_DISK_ERR_RAID_ALARM,                /*Raid��澯*/
        SVS_DISK_ERR_DISK_FULL,                 /*������*/
        SVS_DISK_ERR_RAID_FULL,                 /*Raid����*/
        SVS_DISK_ERR_DISK_BAD,                  /*���̻�*/
        SVS_DISK_ERR_DISK_MOUNT_OFFLINE,         /*����mount����*/
        SVS_DISK_ERR_DISK_FS_INVALID,           /*�����ļ�ϵͳ������*/
        SVS_DISK_ERR_DISK_FS_READONLY,          /*�����ļ�ϵͳֻ��*/
        SVS_DISK_ERR_DISK_OFFLINE_CONF,         /*���̲��������б�����*/
        SVS_DISK_ERR_RAID_NO_OUTPUT_THREAD,     /*raid��û�п��õ�����߳�*/
        CAC_RT_Record_Server::instance()->max_lens(),
        threadNumPerRaid*syncRaidNum,
        threadNumPerRaid,
        freeThreadGroupNum,
        m_raidTotal);

    // ����raid���������Ϣ
    int32_t i = 0;
    for (i = 0; i < m_raidTotal ; i++)
    {
        writeLen = ACE_OS::strlen(strBufferInfo);
        //�����������꣬�˳�ѭ��
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

    // ����raid�����ϸ��Ϣ
    for (i = 0; i < m_raidTotal ; i++)
    {
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //�����������꣬�˳�ѭ��
        if( 0 >= leaveLen )
        {
            break;
        }

        (void)snprintf(&strBufferInfo[writeLen],(uint32_t)leaveLen,"\n\t      RaidIndex [%d] tail information: \n",i);
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //�����������꣬�˳�ѭ��
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
 �� �� ��  : debugGetWriteThreadInfo
 ��������  : telnet��ȡд¼���߳��ϵľ�ͷ��Ϣ
 �������  : char *strBufferInfo:�����������raid����Ϣ
             const uint32_t bufferLen:����������
 �������  : char *strBufferInfo:raid����Ϣ
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t SVS_Disk_Manager::debugGetWriteThreadInfo( char *strBufferInfo,const uint32_t bufferLen )
{
    // ���δ��ʼ���ã����ܵ���
    if(m_bThreadFlag)
    {
        return SVS_RESULT_FAILURE;
    }

    memset(strBufferInfo, 0, bufferLen);
    size_t writeLen = 0;
    int32_t leaveLen = 0;

    // ����raid�����ϸ��Ϣ
    for (int32_t i = 0; i < m_raidTotal ; i++)
    {
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //�����������꣬�˳�ѭ��
        if( 0 >= leaveLen )
        {
            break;
        }

        (void)snprintf(&strBufferInfo[writeLen],
            (uint32_t)leaveLen,
            "\n\t      RaidIndex [%d] Camera Load Info of Media Write Thread : \n",i);
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //�����������꣬�˳�ѭ��
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
    // ���δ��ʼ���ã����ܵ���
    if(m_bThreadFlag)
    {
        return SVS_RESULT_FAILURE;
    }

    memset(strBufferInfo, 0, bufferLen);
    size_t writeLen = 0;
    int32_t leaveLen = 0;

    // ����raid�����ϸ��Ϣ
    for (int32_t i = 0; i < m_raidTotal ; i++)
    {
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //�����������꣬�˳�ѭ��
        if( 0 >= leaveLen )
        {
            break;
        }

        (void)snprintf(&strBufferInfo[writeLen],(uint32_t)leaveLen,"\n\t      RaidIndex [%d] camera list: \n",i);
        writeLen = ACE_OS::strlen(strBufferInfo);
        leaveLen = (int32_t)(bufferLen-writeLen);
        //�����������꣬�˳�ѭ��
        if( 0 >= leaveLen )
        {
            break;
        }

        (void)m_raidList[i].statCamreaListOnRaid(&strBufferInfo[writeLen], (uint32_t)leaveLen);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : diskFullDelRecord
 ��������  : ����ɾ��¼����
             ���������ɾ��¼��ʽ����������Ѿ�����澯�������£���ʼ��������
             ɾ��������
             �����������ɾ��¼��ʽ����������ɾ����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
int32_t SVS_Disk_Manager::diskFullDelRecord()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_diskFullDelingFlagMutex, SVS_RESULT_FAILURE);

    CAC_Record_Raid * pstRecordRaid = NULL;
    int32_t raidBestStatus = SVS_DISK_ERR_RAID_NO_OUTPUT_THREAD;
    // �������raid���Ƿ����Ѿ�����澯�������£����������ɾ������
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

    // ���raid����������Ĵ��̣����ô���raid������ɾ������
    if (SVS_DISK_ERR_OK == raidBestStatus)
    {
        m_diskFullStartFlag = false;  // ���ý�������¼��ɾ�����񣬿����ظ�����
        return SVS_RESULT_OK;
    }

    // ���״̬��õ�raid�Ѿ�����ĸ澯���������Ƿ��Ѿ�������ɾ��¼��¼���򶼼������ɾ������
    if ((SVS_DISK_ERR_RAID_ALARM <= raidBestStatus)
        &&(SVS_DISK_ERR_RAID_FULL >= raidBestStatus))//�����쳣,�ļ�ϵͳ�쳣�Ȳ���������ɾ��
    {
        if(OVERWRITE_RECORD == CAC_RT_Record_Server::instance()->delete_record_strategy())
        {
            SVS_LOG((SVS_LM_WARNING,
                "Best raid status is below alarm. Add disk full delete task.raidBestStatus[%d],",
                raidBestStatus));

            if (SVS_RESULT_OK == send_disk_full_alarm())
            {
                m_diskFullStartFlag = true;  // ���������˱���¼��ɾ�����񣬿����ظ�����
                return SVS_RESULT_OK;
            }
        }
    }
    // ����Ѿ�����������ɾ��������û��ɾ�����澯�ָ�ֵ���ϣ������ɾ��
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

    // �����������Ȼ���̽����˴��ָ̻����䣬���ǻ�û������澯���򣬻�û��������ɾ�������ô���

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : send_disk_full_alarm
��������  : ��洢�����ʹ������澯
�������  :
�������  :
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t SVS_Disk_Manager::send_disk_full_alarm()const
{
    // ����ռ�
    ACE_Message_Block *pAlarmMb = NULL;

    pAlarmMb = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
                                    sizeof(SVS_RECORDSVR_STORE_DISKALARM_REQ_MSG));
    if (NULL == pAlarmMb)
    {
        SVS_LOG((SVS_LM_ERROR, "Allocate message block failed. Send disk full alarm fail."));

        return SVS_RESULT_FAILURE;
    }

    //���������澯��Ϣ
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

    // ɾ�����������
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

