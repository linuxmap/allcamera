/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : SvsSysStat.cpp
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
#include "svs_ace_header.h"
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include "svs_log_msg.h"
#include "svs_adapter_sys_stat.h"

CSvsSysStat::CSvsSysStat()
{
    memset(&m_stSysInfo, 0 , sizeof(m_stSysInfo));

    // �����б�
    m_networkCardInfoList.clear();

    // �����б�
    m_diskInfoList.clear();

    // ��ǰ����ͳ�Ƶ��±�ֵ
    m_ulStatIndex = 0;

    m_bRunFlag = false;

    // ���mounts��Ϣ,ÿ�����̳���128,raid 64����ÿ��raid��32������
    memset(m_strMountsInfo, 0, sizeof(m_strMountsInfo));

    // ��ȡmount�ļ��Ƿ�OK
    m_bIsMountsOk = false;
}

CSvsSysStat::~CSvsSysStat()
{
    memset(&m_stSysInfo, 0 , sizeof(m_stSysInfo));

    try
    {
        // �����б�
        m_networkCardInfoList.clear();

        // �����б�
        m_diskInfoList.clear();
    }catch(...)
    {

    }

    // ��ǰ����ͳ�Ƶ��±�ֵ
    m_ulStatIndex = 0;

    m_bRunFlag = false;
}

/*****************************************************************************
 �� �� ��  : open
 ��������  : ��ϵͳͳ��ģ�鹦�ܣ������̣߳���ʼ������ͳ��ϵͳ��Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�: 0 ʧ��: -1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSvsSysStat::open(void *)
{
    if(m_bRunFlag)
    {
        SVS_LOG((SVS_LM_WARNING,"Open the moudle of stating system failed as it already is running."));

        return SVS_STAT_FAILTRUE;
    }

    // ����ý�崦���߳�
    m_bRunFlag = true;
    size_t stackSize = DEFAULT_THREAD_STACK_SIZE;
    int32_t nRet = activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED,
                        1,
                        0,
                        ACE_DEFAULT_THREAD_PRIORITY,
                        -1,
                        0,
                        0,
                        0,
                        &stackSize,
                        0 );
    if (0 != nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"Activate system stat thread failed."));

        return SVS_STAT_FAILTRUE;
    }

    SVS_LOG((SVS_LM_INFO,"Open the stat moudle for system success."));

    return SVS_STAT_OK;
}

/*****************************************************************************
 �� �� ��  : svc
 ��������  : �����̣߳���ʼ������ͳ��ϵͳ��Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�: 0 ʧ��: -1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSvsSysStat::svc(void)
{
    ACE_thread_t threadID = ACE_OS::thr_self();
    while (m_bRunFlag)
    {
        // ÿ��1��ͳ������ͳ��һ��
        for (uint32_t i = 0; i < SVS_STAT_DEFAULT_INTERVAL; i++)
        {
            if (!m_bRunFlag)
            {
                SVS_LOG((SVS_LM_WARNING,"The stat thread exit.thread id [%d].",threadID));

                return SVS_STAT_OK;
            }

            // �������ͳ��ϵͳ��Ϣ������
            if(0 == (i) % SVS_STAT_DEFAULT_INTERVAL)
            {
                // ͳ��CPU��Ϣ
                (void)StatCpuInfo();

                // ͳ���ڴ���Ϣ
                (void)StatMemoryInfo();

                // ͳ�ƴ�����Ϣ
                (void)StatDiskInfo();

                // ͳ��������Ϣ
                (void)StatNetworkCardInfo();

                // ��ͳ���±�ֵ����һλ
                ++m_ulStatIndex;

                m_ulStatIndex %= SVS_STAT_INTERVAL_NUM;
            }

            // Ϊ��ֹsleepʱ����������ܼ�ʱ��Ӧ�˳���Ϣ������ֻsleep 1��
            (void)::sleep(1);
        }
    }

    SVS_LOG((SVS_LM_WARNING,"The system stat thread exit.thread id [%d].",threadID));

    return SVS_STAT_OK;
}


int32_t CSvsSysStat::close(u_long)
{
    m_bRunFlag = false;
    (void)this->wait();

    SVS_LOG((SVS_LM_WARNING, "close the moudle of stating system."));

    return SVS_STAT_OK;
}

// ���һ��Ҫͳ�Ƶ�����
int32_t CSvsSysStat::AddNetwordCard(const char* strIP)
{
    if(NULL == strIP)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Add a netword card to moudle stat failed as the paramter is invalid."
            "strIP[%p]",
            strIP));

        return SVS_STAT_FAILTRUE;
    }

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_sysMutex, SVS_STAT_FAILTRUE);
    std::list<NetworkCardInfo>::iterator iter = m_networkCardInfoList.begin();

    bool bFindFlag = false;
    // �������еĴ��̣��ж��Ƿ��Ѿ�����ͳ�ƶ���
    for(; iter != m_networkCardInfoList.end(); iter++)
    {
        // �ҵ��˶�Ӧ������
        if(0 == strncmp((*iter).m_strIP, strIP, strlen(strIP)))
        {
            bFindFlag = true;
            break;
        }
    }

    // Ҫ��ӵ������Ѿ���ͳ�ƶ�������
    if(bFindFlag)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Add a netword card to stat moudle OK.It already has been added to the list."
            "strIP[%s]",
            strIP));

        return SVS_STAT_OK;
    }

    NetworkCardInfo stNetCardInfo;
    stNetCardInfo.Reset();
    strncpy(stNetCardInfo.m_strIP, strIP, sizeof(stNetCardInfo.m_strIP));
    m_networkCardInfoList.push_back(stNetCardInfo);
    SVS_LOG((SVS_LM_INFO, "Add a netword card to stat moudle OK.strIP[%s]", strIP));

    return SVS_STAT_OK;
}

// ɾ��һ��Ҫͳ�Ƶ�����
int32_t CSvsSysStat::RemoveNetwordCard(const char* strIP)
{
    if(NULL == strIP)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Remove a netword card from stat moudle failed as the paramter is invalid."
            "strIP[%p]",
            strIP));

        return SVS_STAT_FAILTRUE;
    }



    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_sysMutex, SVS_STAT_FAILTRUE);
    std::list<NetworkCardInfo>::iterator iter = m_networkCardInfoList.begin();

    // �������еĴ��̣��ж��Ƿ��Ѿ�����ͳ�ƶ���
    for(; iter != m_networkCardInfoList.end(); iter++)
    {
        // �ҵ��˶�Ӧ������,ɾ���ڵ�
        if(0 == strncmp((*iter).m_strIP, strIP, strlen(strIP)))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Remove a netword card from stat moudle OK .strIP[%s]",
                strIP));
            m_networkCardInfoList.erase(iter);

            return SVS_STAT_OK;
        }
    }

    SVS_LOG((SVS_LM_WARNING,
        "Remove a netword card from stat moudle failed as can't find the IP mark."
        "strIP[%s]",
        strIP));

    return SVS_STAT_FAILTRUE;
}


// ���һ��Ҫͳ�ƵĴ���
int32_t CSvsSysStat::AddDisk(const char* strDiskPath)
{
    if(NULL == strDiskPath)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Add a disk to moudle stat failed as the paramter is invalid."
            "strDiskPath[%p]",
            strDiskPath));

        return SVS_STAT_FAILTRUE;
    }


    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_sysMutex, SVS_STAT_FAILTRUE);
    std::list<DiskInfo>::iterator iter = m_diskInfoList.begin();

    bool bFindFlag = false;
    // �������еĴ��̣��ж��Ƿ��Ѿ�����ͳ�ƶ���
    for(; iter != m_diskInfoList.end(); iter++)
    {
        // �ҵ��˶�Ӧ������
        if(0 == strncmp((*iter).m_strDiskName, strDiskPath, strlen(strDiskPath)))
        {
            bFindFlag = true;
            break;
        }
    }

    // Ҫ��ӵ������Ѿ���ͳ�ƶ�������
    if(bFindFlag)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Add a disk to stat moudle OK.It already has been added to the list."
            "strDiskPath[%s]",
            strDiskPath));

        return SVS_STAT_OK;
    }

    DiskInfo stDiskInfo;
    stDiskInfo.Reset();
    strncpy(stDiskInfo.m_strDiskName, strDiskPath, sizeof(stDiskInfo.m_strDiskName));

    // ��ȡ���̵�mount��Ϣ
    ReadMountFile();
    stDiskInfo.m_nMountFlag = DISK_LOCAL_PATH;
    if(SVS_STAT_OK == CheckDiskMountState(strDiskPath))
    {
        stDiskInfo.m_nMountFlag = DISK_MOUNT;
    }

    m_diskInfoList.push_back(stDiskInfo);

    SVS_LOG((SVS_LM_INFO,
        "Add a disk to stat moudle OK.strDiskPath[%s], mountFlag[%d]",
        strDiskPath,
        stDiskInfo.m_nMountFlag));

    return SVS_STAT_OK;
}


// ɾ��һ��Ҫͳ�ƵĴ���
int32_t CSvsSysStat::RemoveDisk(const char* strDiskPath)
{
    if(NULL == strDiskPath)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Remove a disk from stat moudle failed as the paramter is invalid."
            "strDiskPath[%p]",
            strDiskPath));

        return SVS_STAT_FAILTRUE;
    }


    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_sysMutex, SVS_STAT_FAILTRUE);
    std::list<DiskInfo>::iterator iter = m_diskInfoList.begin();

    // �������еĴ��̣��ж��Ƿ��Ѿ�����ͳ�ƶ���
    for(; iter != m_diskInfoList.end(); iter++)
    {
        // �ҵ��˶�Ӧ�Ĵ���,ɾ���ڵ�
        if(0 == strncmp((*iter).m_strDiskName, strDiskPath, strlen(strDiskPath)))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Remove a disk from stat moudle OK .strDiskPath[%s]",
                strDiskPath));

            m_diskInfoList.erase(iter);

            return SVS_STAT_OK;
        }
    }

    SVS_LOG((SVS_LM_WARNING,
        "Remove a disk from stat moudle failed as can't find the disk mark."
        "strDiskPath[%s]",
        strDiskPath));

    return SVS_STAT_FAILTRUE;
}


// ��ȡcpu��Ϣ����λ�ٷֱ�
void CSvsSysStat::GetCpuInfo(uint32_t &ulUsedPer)const
{
    ulUsedPer = m_stSysInfo.m_ulCpuUsed[SVS_STAT_INTERVAL_NUM];
    return;
}

// ��ȡ�ڴ���Ϣ����λ : KB
void CSvsSysStat::GetMemoryInfo(uint32_t& ulTotalSize, uint32_t& ulUsedSize)const
{
    ulTotalSize = m_stSysInfo.m_ulMemTotal;
    ulUsedSize = m_stSysInfo.m_ulMemUsed[SVS_STAT_INTERVAL_NUM];

    return;
}

// ��ȡ������Ϣ,��λ : Mbps
int32_t CSvsSysStat::GetNetworkCardInfo(const char* strIP, uint32_t& ulTotalSize,
                       uint32_t& ulUsedRecvSize, uint32_t& ulUsedSendSize)
{
    if(NULL == strIP)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Get netword card info from stat moudle failed as the paramter is invalid."
            "strIP[%p]",
            strIP));

        return SVS_STAT_FAILTRUE;
    }

    ulTotalSize = 0;
    ulUsedRecvSize = 0;
    ulUsedSendSize = 0;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_sysMutex, SVS_STAT_FAILTRUE);
    std::list<NetworkCardInfo>::iterator iter = m_networkCardInfoList.begin();

    // �������еĴ��̣����������б�
    for(; iter != m_networkCardInfoList.end(); iter++)
    {
        // �ҵ��˶�Ӧ������,���������Ϣ
        if(0 == strncmp((*iter).m_strIP, strIP, strlen(strIP)))
        {
            NetworkCardInfo* pNetworkCardInfo = &*iter;
            ulTotalSize = pNetworkCardInfo->m_ulBWTotal;
            ulUsedRecvSize = pNetworkCardInfo->m_ulBWUsedRecv[SVS_STAT_INTERVAL_NUM] / 1024;
            ulUsedSendSize = pNetworkCardInfo->m_ulBWUsedSend[SVS_STAT_INTERVAL_NUM] / 1024;
            return SVS_STAT_OK;
        }
    }

    SVS_LOG((SVS_LM_WARNING,
        "Get netword card info from stat moudle failed as can't find the IP mark."
        "strIP[%s]",
        strIP));

    return SVS_STAT_FAILTRUE;
}


// ��ȡ������Ϣ����λ: B
int32_t CSvsSysStat::GetDiskInfo(const char* strDiskPath, uint64_t& ullTotalSize, uint64_t& ullUsedSize)
{
    if(NULL == strDiskPath)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Get disk info from stat moudle failed as the paramter is invalid."
            "strDiskPath[%p]",
            strDiskPath));

        return SVS_STAT_FAILTRUE;
    }


    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_sysMutex, SVS_STAT_FAILTRUE);
    std::list<DiskInfo>::iterator iter = m_diskInfoList.begin();

    // �������еĴ���,�ҵ���Ӧ�Ĵ���
    for(; iter != m_diskInfoList.end(); iter++)
    {
        // �ҵ��˶�Ӧ�Ĵ���,ɾ���ڵ�
        if(0 == strncmp((*iter).m_strDiskName, strDiskPath, strlen(strDiskPath)))
        {
            DiskInfo *pDiskInfo = &*iter;
            ullTotalSize = pDiskInfo->m_ullTotalSize;
            ullUsedSize = pDiskInfo->m_ullUsedSize;

            return SVS_STAT_OK;
        }
    }

    SVS_LOG((SVS_LM_WARNING,
        "Get disk info from stat moudle failed as can't find the disk mark."
        "strDiskPath[%s]",
        strDiskPath));

    return SVS_STAT_FAILTRUE;
}


/*****************************************************************************
 �� �� ��  : StatCpuInfp
 ��������  : ͳ��ϵͳ��cpuռ����Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�: 0 ʧ��: -1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSvsSysStat::StatCpuInfo()
{

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_sysMutex, SVS_STAT_FAILTRUE);
    FILE                        *pf;
    static uint64_t lastUserTime = 0;
    static uint64_t lastNiceTime = 0;
    static uint64_t lastSysTime = 0;
    static uint64_t lastIdleTime = 0;
    uint64_t userTime = 0;
    uint64_t niceTime = 0;
    uint64_t systTime = 0;
    uint64_t idleTime = 0;
    uint64_t totalIdleTime = 0;
    uint64_t totalUsedTime = 0;
    char strStatInfo[MAXNAMELEN + 1] = {'\0', };

    pf = fopen("/proc/stat", "r");

    if (NULL == pf)
    {
        /* ���ļ���һ�㲻��ʧ�ܣ���Ҫ���� */
        return SVS_STAT_FAILTRUE;
    }

    /* ��ȡCPUʹ��ʱ����Ϣ */
    if (0 == fread(strStatInfo, sizeof(char), MAXNAMELEN, pf))
    {
        fclose(pf);
        return SVS_STAT_FAILTRUE;
    }
    fclose(pf);
    char *pszTmp = strstr(strStatInfo, "cpu ");
    if (NULL == pszTmp)
    {
        return SVS_STAT_FAILTRUE;
    }

    // cpu�İ���u�ַ������޷��ų�ͻ�����Ǵ���
    sscanf(pszTmp, "cpu %Lu %Lu %Lu %Lu\n", &userTime, &niceTime, &systTime, &idleTime);//lint !e561 !e566

    /* �״����в����� */
    if (0 == lastUserTime)
    {
        lastUserTime = userTime;
        lastNiceTime = niceTime;
        lastSysTime = systTime;
        lastIdleTime = idleTime;
        return SVS_STAT_OK;
    }

    /* ����CPUռ���� */
    totalIdleTime = (uint64_t) (idleTime - lastIdleTime);
    totalUsedTime = (uint64_t) ((userTime - lastUserTime)
                             + (niceTime - lastNiceTime)
                             + (systTime - lastSysTime)
                             + (idleTime - lastIdleTime));

    // �����������̫�����òɼ�
    if (1000 > totalUsedTime)
    {
        return SVS_STAT_OK;
    }

    // �����ϸ����ڵ�cpuֵ��Ϊ���ֵ���������
    m_stSysInfo.m_ulCpuUsed[m_ulStatIndex]= (uint32_t)(((totalUsedTime - totalIdleTime)
                                             * 100) / totalUsedTime);

    lastUserTime = userTime;
    lastNiceTime = niceTime;
    lastSysTime = systTime;
    lastIdleTime = idleTime;

    // cpu��ƽ��ֵ
    uint32_t * pCpuAverge  = &m_stSysInfo.m_ulCpuUsed[SVS_STAT_INTERVAL_NUM];
    *pCpuAverge = 0;
    for(uint32_t i = 0; i < SVS_STAT_INTERVAL_NUM; i++)
    {
        *pCpuAverge += m_stSysInfo.m_ulCpuUsed[i];
    }

    *pCpuAverge = *pCpuAverge / SVS_STAT_INTERVAL_NUM;

    return SVS_STAT_OK;
}

/*****************************************************************************
 �� �� ��  : StatMemoryInfo
 ��������  : ͳ��ϵͳ���ڴ�ռ����Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�: 0 ʧ��: -1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSvsSysStat::StatMemoryInfo()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_sysMutex, SVS_STAT_FAILTRUE);
    FILE    *fp = NULL;
    char cBuff[MAXNAMELEN+1] = {0, };
    int32_t imemTotal = 0;
    int32_t imemfree = 0;
    int32_t ibuffer = 0;
    int32_t icache = 0;

    /* ��ȡ�ڴ���Ϣ */
    fp = fopen("/proc/meminfo", "r");
    if (NULL == fp)
    {
        return SVS_STAT_FAILTRUE;
    }
    while (fgets(cBuff, sizeof(cBuff), fp))
    {
        if (!strncmp(cBuff, "MemTotal:", strlen("MemTotal:")))
        {
            sscanf(cBuff, "MemTotal: %d kB", &imemTotal);
        }
        m_stSysInfo.m_ulMemTotal = (uint32_t)imemTotal;

        if (!strncmp(cBuff, "MemFree:", strlen("MemFree:")))
        {
            sscanf(cBuff, "MemFree: %d kB", &imemfree);
        }

        if (!strncmp(cBuff, "Buffers:", strlen("Buffers:")))
        {
            sscanf(cBuff, "Buffers: %d kB", &ibuffer);
        }

        if (!strncmp(cBuff, "Cached:", strlen("Cached:")))
        {
            sscanf(cBuff, "Cached: %d kB", &icache);
            break;
        }
    }

    // �ر��ڴ������ļ�
    (void)fclose(fp);

    // �����Ѿ�ʹ���ڴ�
    m_stSysInfo.m_ulMemUsed[m_ulStatIndex] =
        m_stSysInfo.m_ulMemTotal - (uint32_t)(imemfree + ibuffer + icache);

    // �����ڴ�ʹ�õ�ƽ��ֵ
    uint32_t* ulMemAvg = &m_stSysInfo.m_ulMemUsed[SVS_STAT_INTERVAL_NUM];
    *ulMemAvg = 0;
    for(uint32_t i = 0; i < SVS_STAT_INTERVAL_NUM; i++)
    {
        *ulMemAvg += m_stSysInfo.m_ulMemUsed[i];
    }

    *ulMemAvg = *ulMemAvg / SVS_STAT_INTERVAL_NUM;

    return SVS_STAT_OK;
}

int32_t CSvsSysStat::StatDiskInfo()
{
    ReadMountFile();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_sysMutex, SVS_STAT_FAILTRUE);
    std::list<DiskInfo>::iterator iter = m_diskInfoList.begin();
    DiskInfo* pDiskInfo = NULL;

    // �������еĴ��̣�ͳ�ƴ�����Ϣ
    for(; iter != m_diskInfoList.end(); iter++)
    {
        pDiskInfo = &*iter;

        // �������ע��ʱʱmount�ģ����������mount�ļ����Ҳ����ˣ�˵������unmount��
        if((DISK_MOUNT == pDiskInfo->m_nMountFlag)
            && (SVS_STAT_OK != CheckDiskMountState(pDiskInfo->m_strDiskName)))
        {
            SVS_LOG((SVS_LM_WARNING,
                "The Disk's mount flag is offline.diskPath[%s]",
                pDiskInfo->m_strDiskName));

            pDiskInfo->m_ullTotalSize = 0;
            pDiskInfo->m_ullUsedSize = 0;

            continue;
        }

        struct statvfs hdstat;
        if (-1 == statvfs(pDiskInfo->m_strDiskName, &hdstat))
        {
            SVS_LOG((SVS_LM_ERROR, "Fail to read disk information.disk[%s]", pDiskInfo->m_strDiskName));

            pDiskInfo->m_ullTotalSize = 0;
            pDiskInfo->m_ullUsedSize = 0;

            continue;
        }

         // �ļ�ϵͳֻ��
        if(ST_RDONLY == hdstat.f_flag)
        {
            SVS_LOG((SVS_LM_ERROR, "Disk's filesystem is realonly.disk[%s]", pDiskInfo->m_strDiskName));

            pDiskInfo->m_ullTotalSize = 0;
            pDiskInfo->m_ullUsedSize = 0;

            continue;
        }

        pDiskInfo->m_ullTotalSize = (uint64_t)hdstat.f_bsize * hdstat.f_blocks;
        uint64_t ullFreeSize = (uint64_t)hdstat.f_bsize * hdstat.f_bavail;
        pDiskInfo->m_ullUsedSize = pDiskInfo->m_ullTotalSize - ullFreeSize;
    }

    return SVS_STAT_OK;
}

int32_t CSvsSysStat::StatNetworkCardInfo()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_sysMutex, SVS_STAT_FAILTRUE);
    std::list<NetworkCardInfo>::iterator iter = m_networkCardInfoList.begin();
    NetworkCardInfo* pNetCardInfo = NULL;

    // �������еĴ��̣�ͳ�ƴ�����Ϣ
    for(; iter != m_networkCardInfoList.end(); iter++)
    {
        pNetCardInfo = &*iter;
        StatNetCardInfo(pNetCardInfo);

        // ��������ƽ��ֵ
        uint32_t* pBWAvgRecv = &pNetCardInfo->m_ulBWUsedRecv[SVS_STAT_INTERVAL_NUM];
        uint32_t* pBWAvgSend = &pNetCardInfo->m_ulBWUsedSend[SVS_STAT_INTERVAL_NUM];

        // ����ܴ���Ϊ0,˵�������쳣
        if(0 == pNetCardInfo->m_ulBWTotal)
        {
            *pBWAvgRecv = 0;
            *pBWAvgSend = 0;

            continue;
        }

        *pBWAvgRecv = 0;
        *pBWAvgSend = 0;
        for(int32_t i = 0; i < SVS_STAT_INTERVAL_NUM; i++)
        {
            *pBWAvgRecv += pNetCardInfo->m_ulBWUsedRecv[i];
            *pBWAvgSend += pNetCardInfo->m_ulBWUsedSend[i];
        }

        *pBWAvgRecv = *pBWAvgRecv / SVS_STAT_INTERVAL_NUM;
        *pBWAvgSend = *pBWAvgSend / SVS_STAT_INTERVAL_NUM;

        SVS_LOG((SVS_LM_DEBUG,
            "Netcard stat info:NetCard[%s:%s], totalBW[%u]Mbps,Recv[%u]kbps,Send[%u]kbps",
            pNetCardInfo->m_strName,
            pNetCardInfo->m_strIP,
            pNetCardInfo->m_ulBWTotal,
            pNetCardInfo->m_ulBWUsedRecv[SVS_STAT_INTERVAL_NUM],
            pNetCardInfo->m_ulBWUsedSend[SVS_STAT_INTERVAL_NUM]));
        ACE_OS::last_error(0);
    }

    return SVS_STAT_OK;
}

/*****************************************************************************
 �� �� ��  : debugGetRaidListInfo
 ��������  : telnet��ȡraid����Ϣ
 �������  : char *strBufferInfo:�����������raid����Ϣ
             const uint32_t bufferLen:����������
 �������  : char *strBufferInfo:raid����Ϣ
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ��� ������
*****************************************************************************/
void CSvsSysStat::ReadMountFile()
{
    m_bIsMountsOk = false;
    FILE                        *pf;
    pf = fopen("/proc/mounts", "r");
    size_t buffSize = sizeof(m_strMountsInfo);

    /* ���ļ���һ�㲻��ʧ�ܣ���Ҫ���� */
    // ��������ļ�ʧ�ܣ�����Ϊ���е�mount��Ϣ���ڣ���֤���̿���
    if (NULL == pf)
    {
        SVS_LOG((SVS_LM_WARNING,"Open /proc/mounts failed."));
        return;
    }

    /* ��ȡCPUʹ��ʱ����Ϣ */
    memset(m_strMountsInfo, 0, buffSize);
    if (0 == fread(m_strMountsInfo, sizeof(char), buffSize, pf))
    {
        (void)fclose(pf);
        return;
    }

    m_bIsMountsOk = true;
    (void)fclose(pf);

    return;
}

int32_t CSvsSysStat::CheckDiskMountState(const char * strDiskPath)const
{
    if(NULL == strDiskPath)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Check mount online status failed.The parameter is invalid."
            " strDiskPath[%p]",
            strDiskPath));
        return SVS_STAT_FAILTRUE;
    }

    // �����ȡ/proc/mountsʧ�ܣ�����Ϊmount����
    if(!m_bIsMountsOk)
    {
        SVS_LOG((SVS_LM_WARNING, "Check mount online status failed.Read /proc/mounts failed."));
        return SVS_STAT_FAILTRUE;
    }

    //�鿴 /proc/mounts�Ƿ���Ҫ��ѯ�Ĵ�����Ϣ
    char tempDiskPath[STAT_PATH_LEN + 1];
    (void)snprintf(tempDiskPath, sizeof(tempDiskPath), " %s ", strDiskPath);

    // ���mount����
    if (NULL == strstr(m_strMountsInfo, tempDiskPath))
    {
        return SVS_STAT_FAILTRUE;
    }

    return SVS_STAT_OK;
}

void CSvsSysStat::StatNetCardInfo(NetworkCardInfo* pNetworkCard)const
{
    in_addr_t serviceIp = inet_addr(pNetworkCard->m_strIP);

    static int32_t isock = -1;
    struct ifconf stIfc;
    struct ifreq         *pIfr = NULL;
    struct ifreq stArrayIfr[MAX_NETCARD_NUM]; // ���16������
    struct sockaddr_in   *pAddr = NULL;
    ethtool_cmd_t stEcmd;          // = { ETHTOOL_GSET };

    if (-1 == isock)
    {
        isock = socket(AF_INET, SOCK_DGRAM, 0);
        if(0 > isock)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Get netcard info failed. Create socket failed."
                "IP[%s], Name[%s].",
                pNetworkCard->m_strIP,
                pNetworkCard->m_strName));

            return;
        }
    }

    /* ��ʼ�� */
    memset(&stIfc, 0, sizeof(stIfc));
    memset(&stEcmd, 0, sizeof(ethtool_cmd_t));
    memset(stArrayIfr, 0, sizeof(stArrayIfr));

    stEcmd.cmd = ETHTOOL_GSET;
    stIfc.ifc_len = sizeof(struct ifreq) * MAX_NETCARD_NUM;
    stIfc.ifc_buf = (char *) stArrayIfr;

    /* ��ȡ���л��������Ϣ */
    if (ioctl(isock, SIOCGIFCONF, &stIfc) < 0)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get all active netcard failed.ioctl <SIOCGIFCONF> failed."
            "IP[%s], Name[%s].",
            pNetworkCard->m_strIP,
            pNetworkCard->m_strName));
        return;
    }

    pIfr = stIfc.ifc_req;

    int32_t nRet = SVS_STAT_FAILTRUE;
    for (uint32_t i = 0; i < (uint32_t) stIfc.ifc_len; i += sizeof(struct ifreq))
    {
        /* ��ȡ���л��������ip��ַ������������eth1 */
        if (SVS_STAT_OK != ioctl(isock, SIOCGIFADDR, pIfr))
        {
            continue;
        }

        pAddr = (struct sockaddr_in *) (void*)&pIfr->ifr_addr;

        if ((0 != strncmp(pIfr->ifr_name, "eth", strlen("eth"))) // �������eth����
         && (0 != strncmp(pIfr->ifr_name, "bond", strlen("bond"))))// Ҳ����bond������������
        {
            pIfr++;
            continue;
        }

        // ���IP����ҵ������IP��������
        if (serviceIp != pAddr->sin_addr.s_addr)
        {
            pIfr++;
            continue;
        }

        // ������������
        memset(pNetworkCard->m_strName, 0, sizeof(pNetworkCard->m_strName));
        strncpy(pNetworkCard->m_strName, pIfr->ifr_name, strlen(pIfr->ifr_name));

        // ��ȡ��������״̬��ʶ
        if (SVS_STAT_OK != ioctl(isock, SIOCGIFFLAGS, pIfr))
        {
            SVS_LOG((SVS_LM_WARNING,
                "get netcard%u 's information failed."
                "IP[%s], Name[%s].",
                i,
                pNetworkCard->m_strIP,
                pNetworkCard->m_strName));

                nRet = SVS_STAT_FAILTRUE;

                break;
        }

        // �����������up����running״̬
        if (!(pIfr->ifr_flags & IFF_UP) || !(pIfr->ifr_flags & IFF_RUNNING))
        {
            SVS_LOG((SVS_LM_WARNING,
                "netcard is down or not running, please check it."
                "IP[%s], Name[%s], network name[%s].",
                pNetworkCard->m_strIP,
                pNetworkCard->m_strName,
                pIfr->ifr_name));

            pIfr++;
            continue;
        }

        // ҵ������ֻ��һ��,eth��������bond����
        if (0 == strncmp(pIfr->ifr_name, "bond", strlen("bond")))// �����bond����
        {
            nRet = StatBondNetcardInfo(isock, pNetworkCard,pIfr);
            break;
        }
        else // �����eth����
        {
            nRet = StatEthNetcardInfo(isock, pNetworkCard,pIfr);
            break;
        }
    }

    if(SVS_STAT_OK != nRet)
    {
        SVS_LOG((SVS_LM_INFO,"Get the network card information failed.IP[%s]",pNetworkCard->m_strIP));
        pNetworkCard->m_ulBWTotal = 0;
        memset(pNetworkCard->m_ulBWUsedRecv, 0, sizeof(pNetworkCard->m_ulBWUsedRecv));
        memset(pNetworkCard->m_ulBWUsedSend, 0, sizeof(pNetworkCard->m_ulBWUsedSend));
        pNetworkCard->m_ullCurrTxByteRecv = 0;
        pNetworkCard->m_ullCurrTxByteSend = 0;
        return;
    }

    StatBWInfo(pNetworkCard);
    return ;
}

int32_t CSvsSysStat::StatBondNetcardInfo(int32_t isock, NetworkCardInfo* pNetworkCard, struct ifreq* pIfr )const
{
    uint32_t ulTotalBW = 0;
    ifbond_t stBondStat;
    memset(&stBondStat, 0, sizeof(stBondStat));
    /* ��ip��Ӧ��������bonding����ȡ��bonding����Ϣ */
    pIfr->ifr_data = (char *) &stBondStat;
    /* ��ȡbonding��״̬ */
    if (SVS_STAT_OK != ioctl(isock, SIOCBONDINFOQUERY, pIfr))
    {
        SVS_LOG((SVS_LM_WARNING,"ioctl <SIOCBONDINFOQUERY> on %s failed.", pIfr->ifr_name));

        return SVS_STAT_FAILTRUE;
    }

    uint32_t i = 0;
    ifslave_t slave_info;
    BondInfo stBondInfo;
    memset(&slave_info, 0, sizeof(slave_info));
    memset(&stBondInfo, 0, sizeof(stBondInfo));

    pIfr->ifr_data = (char *) &slave_info;
    for (i= 0; i < (uint32_t)stBondStat.num_slaves; i++)
    {
        slave_info.slave_id = i;
        if (SVS_STAT_OK != ioctl(isock, SIOCBONDSLAVEINFOQUERY, pIfr))
        {
            SVS_LOG((SVS_LM_WARNING,"ioctl <SIOCBONDSLAVEINFOQUERY> on %s failed.", pIfr->ifr_name));
            continue;
        }

        // ���������ģʽ,���ȡ�����õ�����������Ϣ
        if (BOND_MODE_ACTIVEBACKUP == stBondStat.bond_mode)
        {
            // �ҵ��������󣬿�������ѭ����
            if(BOND_STATE_ACTIVE == slave_info.state)
            {
                /* ��ȡbonding��Ӧ��slave��Ϣ */
                strncpy(stBondInfo.slaveName[0], slave_info.slave_name, strlen(slave_info.slave_name));
                stBondInfo.slaveNum = 1;
                break;
            }
        }
        else // ����Ǹ��ؾ���ģʽ
        {

            strncpy(stBondInfo.slaveName[stBondInfo.slaveNum],
                        slave_info.slave_name,
                        strlen(slave_info.slave_name));
            ++stBondInfo.slaveNum;
        }
    }

    // ��ȡbond�ܴ�����Ϣ
    ethtool_cmd_t       ecmd;
    ecmd.cmd = ETHTOOL_GSET;
    for(i = 0; i < stBondInfo.slaveNum; i++)
    {
        strcpy(pIfr->ifr_name,stBondInfo.slaveName[i]);
        pIfr->ifr_data = (char *) &ecmd;

        if (SVS_STAT_OK != ioctl(isock, SIOCETHTOOL, pIfr))
        {
            SVS_LOG((SVS_LM_WARNING,"ioctl <SIOCETHTOOL> on %s failed.", pIfr->ifr_name));
        }
        else if (ABNOMAL_VALUE == ecmd.speed)   /* all the bit is 1 */
        {
            SVS_LOG((SVS_LM_WARNING,"The network is down.network name[%s].", pIfr->ifr_name));
        }
        else
        {
            ulTotalBW += ecmd.speed;
        }
    }

    if(0 == ulTotalBW)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get bond bandwidth is failed. IP[%s], Name[%s].",
            pNetworkCard->m_strIP,
            pNetworkCard->m_strName));

        return SVS_STAT_FAILTRUE;
    }

    pNetworkCard->m_ulBWTotal = ulTotalBW;

    return SVS_STAT_OK;
}


int32_t CSvsSysStat::StatEthNetcardInfo(int32_t isock, NetworkCardInfo* pNetworkCard, struct ifreq* pIfr )const
{
    ethtool_cmd_t stEcmd;// = { ETHTOOL_GSET };
    memset(&stEcmd, 0, sizeof(ethtool_cmd_t));
    stEcmd.cmd = ETHTOOL_GSET;
    // ��ȡ�����ܴ�����Ϣ
    pIfr->ifr_data = (char *) &stEcmd;
    if (SVS_STAT_OK != ioctl(isock, SIOCETHTOOL, pIfr))
    {
        SVS_LOG((SVS_LM_INFO, "ioctl <SIOCETHTOOL> on %s failed.", pIfr->ifr_name));
        return SVS_STAT_FAILTRUE;
    }
    else if (ABNOMAL_VALUE == stEcmd.speed)    /* all the bit is 1������down�� */
    {
        SVS_LOG((SVS_LM_WARNING, "The network is down.network name[%s]",pIfr->ifr_name));
        return SVS_STAT_FAILTRUE;
    }

    // ���������ܴ�����Ϣ
    pNetworkCard->m_ulBWTotal = stEcmd.speed;

    return SVS_STAT_OK;
}


void CSvsSysStat::StatBWInfo(NetworkCardInfo* pNetworkCard)const
{
    /* ���»�ȡ����������ô��� */
    FILE          *fp = NULL;
    fp = fopen("/proc/net/dev", "r");
    if (NULL == fp)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat bandwidth failed as open /proc/net/dev failed.IP[%s], name[%s]",
            pNetworkCard->m_strIP,
            pNetworkCard->m_strName));

        fp = NULL;
        return ;
    }

    char cline[1024] = {0, };
    char    *pszcp1 = NULL;
    char    *pszcp2 = NULL;
    NET_FLUX ullrecvBytes = 0;
    NET_FLUX ullsendBytes = 0;
    uint32_t ultmp = 0;
    while (fgets(cline, sizeof(cline), fp))
    {
        pszcp1 = cline;

        while (isspace(*pszcp1))
        {
            pszcp1++;
        }

        pszcp2 = strchr(pszcp1, ':');

        if (NULL == pszcp2)
        {
            continue;
        }
        if((0 != strncmp(pszcp1, "eth", strlen("eth")))
        && (0 != strncmp(pszcp1, "bond", strlen("bond"))))
        {
            continue;            /* if ':' no found read the next cline or the
                                   header line or 'lo' 'sit' card */
        }

        *pszcp2 = '\0';
        pszcp2++;
        // �������Ҫ�ҵ�eth������������
        if (0 != strcmp(pszcp1, pNetworkCard->m_strName))
        {
            continue;
        }
//#ifdef OS_SUSE9
        sscanf(pszcp2,
               "%lu %lu%lu%lu%lu%lu%lu%lu  %lu    %lu%lu%lu%lu%lu%lu%lu",
               &ullrecvBytes,
               &ultmp, &ultmp, &ultmp, &ultmp, &ultmp, &ultmp, &ultmp,
               &ullsendBytes,
               &ultmp, &ultmp, &ultmp, &ultmp, &ultmp, &ultmp, &ultmp);
//#else
//        sscanf(pszcp2,
//               "%Q %lu%lu%lu%lu%lu%lu%lu  %Q    %lu%lu%lu%lu%lu%lu%lu",
//               &ullrecvBytes,
//               &ultmp, &ultmp, &ultmp, &ultmp, &ultmp, &ultmp, &ultmp,
//               &ullsendBytes,
//               &ultmp, &ultmp, &ultmp, &ultmp, &ultmp, &ultmp, &ultmp);
//#endif

        // ���������Ľ�������
        if (0 == pNetworkCard->m_ullCurrTxByteRecv)
        {
            pNetworkCard->m_ullCurrTxByteRecv = ullrecvBytes - 1; //first is 1kb/s
        }

        //change unit to kb/s
        uint32_t* pBWUsed = &pNetworkCard->m_ulBWUsedRecv[m_ulStatIndex];
        *pBWUsed = (uint32_t)(((ullrecvBytes - pNetworkCard->m_ullCurrTxByteRecv) * 8)
            / (1024 * SVS_STAT_DEFAULT_INTERVAL));
        if (*pBWUsed > (pNetworkCard->m_ulBWTotal* 1024))
        {
            *pBWUsed = 1;
        }
        pNetworkCard->m_ullCurrTxByteRecv = ullrecvBytes;

        // ���������ķ�������
        if (0 == pNetworkCard->m_ullCurrTxByteSend)
        {
            pNetworkCard->m_ullCurrTxByteSend = ullsendBytes - 1; //first is 1kb/s
        }

        //change unit to kb/s
        pBWUsed = &pNetworkCard->m_ulBWUsedSend[m_ulStatIndex];
        *pBWUsed = (uint32_t)(((ullsendBytes - pNetworkCard->m_ullCurrTxByteSend) * 8)
            / (1024 * SVS_STAT_DEFAULT_INTERVAL));
        if (*pBWUsed > (pNetworkCard->m_ulBWTotal* 1024))
        {
            *pBWUsed = 1;
        }
        pNetworkCard->m_ullCurrTxByteSend = ullsendBytes;

        break;
    }
    ACE_OS::last_error(0);

    (void)fclose(fp);

    return;
}




