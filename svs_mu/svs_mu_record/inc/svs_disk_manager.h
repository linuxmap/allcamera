/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : SVS_Disk_Manager.h
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
#ifndef __SVS_DISK_MANAGER_H__
#define  __SVS_DISK_MANAGER_H__
#include <svs_ace_header.h>
#include "svs_record_disk.h"

// ���̹����߳�����
enum SVS_DISK_THREAD
{
    SVS_DISK_CHECK_THREAD = 0,
    SVS_DISK_MAX_THREAD
};

// ���̼����������
enum SVS_DISK_CHECK_TASK_TYPE
{
    SVS_DISK_CHECK_TASK_TIMER  = 0,
    SVS_DISK_CHECK_TASK_EXCEPT = 1,
    SVS_DISK_CHECK_TASK_MAX
};

// ���̼��������Ϣ������Ƕ�ʱ��������������в���̣������д��̽��пռ���
typedef struct _SVS_DISK_CHECK_TASK
{
    int32_t TaskType; // ��ʱ��������񣬻��ߴ����쳣
    int32_t RaidIndex;
    int32_t DiskIndex;
    int32_t ErrCode;
}SVS_DISK_CHECK_TASK,*PSVS_DISK_CHECK_TASK;


enum VFD_STATUS
{
    VFD_FREE_STATUS = 1, // VFD����״̬����δ��ռ�ã����Ի�ȡ��
    SYSFD_USABLE_STATUS = 2, // ϵͳ�������
    SYSFD_SWITCH_FILE_STATUS = 3, // ϵͳ��������л��ļ�״̬
    SYSFD_UNUSABLE_STATUS = 4, // ϵͳ���������
};

typedef struct _SVS_DISK_VFILE
{
    int32_t VFd; // ��ʼ��֮��Ͳ��ٱ仯
    int32_t IoQueueIndex;// ��ʼ��֮��Ͳ��ٱ仯
    int32_t SysFd;
    int32_t RaidIndex;
    int32_t DiskIndex;
    char CameraId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
    char RecordFilePath[MAXNAMELEN + 1];
    VFD_STATUS VfdStatus;
}SVS_DISK_VFILE,*PSVS_DISK_VFILE;

/**
 * @class SVS_Disk_Manager
 * @brief ¼����̹�����
 */
class SVS_Disk_Manager : public ACE_Task<ACE_MT_SYNCH>
{
public:  // ���½ӿ�Ϊͨ�ýӿ�

    // �������������õķ�ʽ���ؾͲ���Ҫ���е�ַ��Ч���ж�
    static SVS_Disk_Manager & instance()
    {
        static SVS_Disk_Manager svs_disk_manager;
        return svs_disk_manager;
    };

    /*
     * ��ʼ����������
     * ���̿ռ����߳�ÿ@a nInterval����һ�δ��̿ռ䣬������̷����Ŀ���
     * �ռ�С��@a lowerLimit�ף���ô��Ǹô��̷���Ϊ�����á�
     */
    virtual int32_t initialize(int32_t nInterval,
                             uint32_t lowerLimit,
                             uint32_t alarmLimit,
                             uint32_t alarmResumeLimit);

    // �����߳�
    virtual int32_t open(void *);

    // �رն���,����ǰ�����һ������
    virtual int32_t close(u_long );

    // ���̼���߳�
    virtual int32_t svc(void );

public: // ���½ӿ�ʵ�ֶԴ����쳣�ļ��ʹ���
    // ����IO Scheduler�ϱ��Ĵ��̻��ļ������쳣
    UTAPI int32_t notifyDiskExcept(int32_t fd, int32_t errCode);

    // ����ʱ����������
    UTAPI int32_t checkDiskTimerCallback();

public:        // ���½ӿ�ʵ�ֶԴ����ļ��Ĺ���

    // �½����ߴ�һ���ļ�,��Ҫ�������·��,�����ڲ�������ļ����V_FD
    UTAPI int32_t openFile(char * cameraId,time_t recordTime,
                         char* fileExName,uint64_t &fileSize,
                         int32_t& vfd,bool & medaDataPartedFlag);

    // �ر�һ���Ѿ��򿪵��ļ�
    UTAPI int32_t closeFile(int32_t vfd);

    // ��ȡ�ļ�·��
    UTAPI int32_t getFileFullName(int32_t vfd, char *szFileName);

    // ��ָ���ļ�д������
    UTAPI int32_t writeFile(int32_t vfd,
                    ACE_Message_Block *pData,
                    IO_CallBack_Func pFunc = NULL,
                    void *pParam = NULL);

    // ��ָ���ļ���ȡ����
    UTAPI int32_t readFile(int32_t vfd,
                ACE_Message_Block *pData = NULL,
                u_long = 0,
                IO_CallBack_Func pFunc = NULL,
                void *pParam = NULL)const;

    // �ͷ�vfd
    UTAPI void freeVfd(int32_t vfd);

public:        // �����ӿ�

    // ����Ƿ��п��ô��̣��ɹ�����0��ʧ�ܷ���-1
    int32_t checkUseableRaid();

    // ����·���Ƿ����
    bool isRecordIndexpathuseable()const;

    UTAPI int32_t getAllDiskInfo(DISK_INFO_VECTOR & diskInfoVector)const;

    UTAPI const int32_t raidTotalNum()const
    {
        return m_raidTotal;
    }

    UTAPI int32_t getUsableRaidNum()const;

    // ��ȡraid����Ϣ
    UTAPI int32_t debugGetRaidListInfo(char * strBufferInfo, const uint32_t bufferLen);

    // telnet��ȡд¼���߳��ϵľ�ͷ��Ϣ
    UTAPI int32_t debugGetWriteThreadInfo(char * strBufferInfo, const uint32_t bufferLen);
    // ��ȡraid����Ϣ
    UTAPI int32_t statCamreaListOnRaid(char * strBufferInfo, const uint32_t bufferLen);

    // ��ѯmount�Ĵ����Ƿ�����
    UTAPI int32_t checkMountOnlineStatus(const char * strDiskPath)const;

    // ˢ�´�������
    UTAPI int32_t refreshDiskList(const char * strDiskPath);

    UTAPI int32_t allocOutputThreadIndex(uint32_t & ulThreadIndex);

    UTAPI void freeOutputThreadIndex(uint32_t ulThreadIndex);

    UTAPI int32_t getFreeOutputThreadGroupNum();

    UTAPI const char* getDiskCheckBuff()const
    {
        return m_strDiskCheckBuff;
    }
private:       // �ڲ��ӿ�
    SVS_Disk_Manager();
    virtual ~SVS_Disk_Manager();

    // ������̼������
    UTAPI int32_t processCheckDiskTask(const ACE_Message_Block * mb);

    // ������������Ϣ�����
    UTAPI int32_t enqueueCheckDiskTask( ACE_Message_Block *newItem, ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = putq(newItem, timeout);
        return enqueueResult;
    }

    // ��ʱ���ص�
    static void timerCallback( void * /*pArg*/, int32_t /*eventId*/, int32_t /*timerId*/, int32_t /*nDummy*/ );

    // ����raid�����ã�raid��֮����'|'����
    UTAPI int32_t parseDiskListConf(const char * strDiskPath, DISK_INFO_VECTOR & diskInfoVect)const;

    // ����raid�����
    UTAPI int32_t createRaid(const char *pRaidList, vector <int32_t>& pos);

    // �����Լ�����
    UTAPI int32_t periodCheckRaid();

    // ��������쳣�������
    UTAPI int32_t exceptCheckDisk(const SVS_DISK_CHECK_TASK * pTaskMsg);

    // �����쳣֪ͨ��֪ͨ���ϲ�ҵ���
    UTAPI int32_t notifyExcept(int32_t raidId,int32_t diskErr);

    // ����Ŀ¼
    UTAPI int32_t createDir(const char * diskPath, char * camerId, char * strDay)const;

    // �������
    UTAPI int32_t allocDisk(int32_t & vFd, int32_t & raidIndex);

    // ʱ��ת����¼���ļ���
    UTAPI void time2RecodFileName(const time_t RecordTime,
                                        char* strRecordFileName,
                                        const uint32_t bufLen,
                                        char* strfileExName)const;

    // ����¼���ļ�
    UTAPI int32_t createFile(const char* fileFullPath,bool medaDataPartedFlag,
                            uint64_t &fileSize,int32_t &fd)const;

    // ���������쳣�����Ϣ
    UTAPI int32_t createDiskExceptMsg(int32_t raidIndex,int32_t diskIndex,int32_t errCode);

    // ��ȡmounts��Ϣ
    UTAPI void getMounts();

    // ���㲢����raid��ĸ���
    UTAPI void countRaidLoad();

    // ��������ɾ��
    UTAPI int32_t diskFullDelRecord();

    UTAPI int32_t send_disk_full_alarm()const;

    // ��������������Ϣ
    UTAPI int32_t parseDiskInfo(char * diskPath, DISK_INFO & diskInfo)const;

    // �ƶ�ָ��
    char* move_pos(const char * pos)const;

private:       // �����ļ�������ر���

    ACE_Recursive_Thread_Mutex mutex_;

    SVS_DISK_VFILE m_fileList[RECORD_MAX_LENS_NUM];
    int32_t m_vfdCursor;

    // ��������߳�list
    std::list<uint32_t>   m_freeOutputThreadIndexList;
    ACE_Mutex                  m_FreeOutputThreadIndexMutex;

private:       // ���̼����ر���
    // �߳����б�ʶ
    volatile bool m_bThreadFlag;

    // �����Դ��̼�ⶨʱ��
    int32_t m_checkDiskTimer;

    // raid������
    int32_t m_raidTotal;

    // ������ڣ���λ����
    int32_t m_checkInterval;

    // ���̿��ÿռ����ޣ���λKB
    uint32_t m_lowerLimit;

    // ���̸澯��ֵ����λKB
    uint32_t m_alarmLimit;

    // ���̸澯�ָ�ֵ����λKB
    uint32_t m_alarmResumeLimit;

    // raid��
    CAC_Record_Raid m_raidList[SVS_RAID_MAX_NUM];

    // �������̶���
    CAC_Record_Disk* m_IndexFileDisk;

    // ���mounts��Ϣ
    char m_mountsInfo[DISK_NAME_SIZE * SVS_LUN_MAX_NUM * SVS_RAID_MAX_NUM];

    volatile bool m_bIsMountsOk;

    // ÿ������ɾ��������ʶ
    volatile bool m_diskFullStartFlag;

    ACE_Recursive_Thread_Mutex m_diskFullDelingFlagMutex;

    // ���̼���û�����
    char *m_strDiskCheckBuff;

};
#endif /* __SVS_DISK_MANAGER_H__ */

