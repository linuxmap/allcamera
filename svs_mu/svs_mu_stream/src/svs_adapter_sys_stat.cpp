/******************************************************************************

                  版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文 件 名   : SvsSysStat.cpp
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
#include "svs_ace_header.h"
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include "svs_log_msg.h"
#include "svs_adapter_sys_stat.h"

CSvsSysStat::CSvsSysStat()
{
    memset(&m_stSysInfo, 0 , sizeof(m_stSysInfo));

    // 网卡列表
    m_networkCardInfoList.clear();

    // 磁盘列表
    m_diskInfoList.clear();

    // 当前正在统计的下标值
    m_ulStatIndex = 0;

    m_bRunFlag = false;

    // 存放mounts信息,每个磁盘长度128,raid 64个，每个raid组32个磁盘
    memset(m_strMountsInfo, 0, sizeof(m_strMountsInfo));

    // 获取mount文件是否OK
    m_bIsMountsOk = false;
}

CSvsSysStat::~CSvsSysStat()
{
    memset(&m_stSysInfo, 0 , sizeof(m_stSysInfo));

    try
    {
        // 网卡列表
        m_networkCardInfoList.clear();

        // 磁盘列表
        m_diskInfoList.clear();
    }catch(...)
    {

    }

    // 当前正在统计的下标值
    m_ulStatIndex = 0;

    m_bRunFlag = false;
}

/*****************************************************************************
 函 数 名  : open
 功能描述  : 打开系统统计模块功能，启动线程，开始周期性统计系统信息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功: 0 失败: -1
 修改历史  :
*****************************************************************************/
int32_t CSvsSysStat::open(void *)
{
    if(m_bRunFlag)
    {
        SVS_LOG((SVS_LM_WARNING,"Open the moudle of stating system failed as it already is running."));

        return SVS_STAT_FAILTRUE;
    }

    // 启动媒体处理线程
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
 函 数 名  : svc
 功能描述  : 启动线程，开始周期性统计系统信息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功: 0 失败: -1
 修改历史  :
*****************************************************************************/
int32_t CSvsSysStat::svc(void)
{
    ACE_thread_t threadID = ACE_OS::thr_self();
    while (m_bRunFlag)
    {
        // 每隔1个统计周期统计一次
        for (uint32_t i = 0; i < SVS_STAT_DEFAULT_INTERVAL; i++)
        {
            if (!m_bRunFlag)
            {
                SVS_LOG((SVS_LM_WARNING,"The stat thread exit.thread id [%d].",threadID));

                return SVS_STAT_OK;
            }

            // 如果到了统计系统信息的周期
            if(0 == (i) % SVS_STAT_DEFAULT_INTERVAL)
            {
                // 统计CPU信息
                (void)StatCpuInfo();

                // 统计内存信息
                (void)StatMemoryInfo();

                // 统计磁盘信息
                (void)StatDiskInfo();

                // 统计网卡信息
                (void)StatNetworkCardInfo();

                // 把统计下标值后移一位
                ++m_ulStatIndex;

                m_ulStatIndex %= SVS_STAT_INTERVAL_NUM;
            }

            // 为防止sleep时间过长，不能及时响应退出消息，这里只sleep 1秒
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

// 添加一个要统计的网卡
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
    // 遍历所有的磁盘，判断是否已经加入统计队列
    for(; iter != m_networkCardInfoList.end(); iter++)
    {
        // 找到了对应的网卡
        if(0 == strncmp((*iter).m_strIP, strIP, strlen(strIP)))
        {
            bFindFlag = true;
            break;
        }
    }

    // 要添加的网卡已经在统计队列中了
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

// 删除一个要统计的网卡
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

    // 遍历所有的磁盘，判断是否已经加入统计队列
    for(; iter != m_networkCardInfoList.end(); iter++)
    {
        // 找到了对应的网卡,删除节点
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


// 添加一个要统计的磁盘
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
    // 遍历所有的磁盘，判断是否已经加入统计队列
    for(; iter != m_diskInfoList.end(); iter++)
    {
        // 找到了对应的网卡
        if(0 == strncmp((*iter).m_strDiskName, strDiskPath, strlen(strDiskPath)))
        {
            bFindFlag = true;
            break;
        }
    }

    // 要添加的网卡已经在统计队列中了
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

    // 获取磁盘的mount信息
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


// 删除一个要统计的磁盘
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

    // 遍历所有的磁盘，判断是否已经加入统计队列
    for(; iter != m_diskInfoList.end(); iter++)
    {
        // 找到了对应的磁盘,删除节点
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


// 获取cpu信息，单位百分比
void CSvsSysStat::GetCpuInfo(uint32_t &ulUsedPer)const
{
    ulUsedPer = m_stSysInfo.m_ulCpuUsed[SVS_STAT_INTERVAL_NUM];
    return;
}

// 获取内存信息，单位 : KB
void CSvsSysStat::GetMemoryInfo(uint32_t& ulTotalSize, uint32_t& ulUsedSize)const
{
    ulTotalSize = m_stSysInfo.m_ulMemTotal;
    ulUsedSize = m_stSysInfo.m_ulMemUsed[SVS_STAT_INTERVAL_NUM];

    return;
}

// 获取网卡信息,单位 : Mbps
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

    // 遍历所有的磁盘，遍历网卡列表
    for(; iter != m_networkCardInfoList.end(); iter++)
    {
        // 找到了对应的网卡,获得网卡信息
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


// 获取磁盘信息，单位: B
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

    // 遍历所有的磁盘,找到对应的磁盘
    for(; iter != m_diskInfoList.end(); iter++)
    {
        // 找到了对应的磁盘,删除节点
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
 函 数 名  : StatCpuInfp
 功能描述  : 统计系统的cpu占用信息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功: 0 失败: -1
 修改历史  :
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
        /* 虚文件打开一般不会失败，但要保护 */
        return SVS_STAT_FAILTRUE;
    }

    /* 获取CPU使用时间信息 */
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

    // cpu的包含u字符，和无符号冲突，不是错误
    sscanf(pszTmp, "cpu %Lu %Lu %Lu %Lu\n", &userTime, &niceTime, &systTime, &idleTime);//lint !e561 !e566

    /* 首次运行不计算 */
    if (0 == lastUserTime)
    {
        lastUserTime = userTime;
        lastNiceTime = niceTime;
        lastSysTime = systTime;
        lastIdleTime = idleTime;
        return SVS_STAT_OK;
    }

    /* 计算CPU占用率 */
    totalIdleTime = (uint64_t) (idleTime - lastIdleTime);
    totalUsedTime = (uint64_t) ((userTime - lastUserTime)
                             + (niceTime - lastNiceTime)
                             + (systTime - lastSysTime)
                             + (idleTime - lastIdleTime));

    // 如果采样周期太多则不用采集
    if (1000 > totalUsedTime)
    {
        return SVS_STAT_OK;
    }

    // 保存上个周期的cpu值，为容灾调度做依据
    m_stSysInfo.m_ulCpuUsed[m_ulStatIndex]= (uint32_t)(((totalUsedTime - totalIdleTime)
                                             * 100) / totalUsedTime);

    lastUserTime = userTime;
    lastNiceTime = niceTime;
    lastSysTime = systTime;
    lastIdleTime = idleTime;

    // cpu的平均值
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
 函 数 名  : StatMemoryInfo
 功能描述  : 统计系统的内存占用信息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功: 0 失败: -1
 修改历史  :
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

    /* 获取内存信息 */
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

    // 关闭内存虚拟文件
    (void)fclose(fp);

    // 计算已经使用内存
    m_stSysInfo.m_ulMemUsed[m_ulStatIndex] =
        m_stSysInfo.m_ulMemTotal - (uint32_t)(imemfree + ibuffer + icache);

    // 计算内存使用的平均值
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

    // 遍历所有的磁盘，统计磁盘信息
    for(; iter != m_diskInfoList.end(); iter++)
    {
        pDiskInfo = &*iter;

        // 如果磁盘注册时时mount的，如果现在在mount文件中找不到了，说明磁盘unmount了
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

         // 文件系统只读
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

    // 遍历所有的磁盘，统计磁盘信息
    for(; iter != m_networkCardInfoList.end(); iter++)
    {
        pNetCardInfo = &*iter;
        StatNetCardInfo(pNetCardInfo);

        // 计算网卡平均值
        uint32_t* pBWAvgRecv = &pNetCardInfo->m_ulBWUsedRecv[SVS_STAT_INTERVAL_NUM];
        uint32_t* pBWAvgSend = &pNetCardInfo->m_ulBWUsedSend[SVS_STAT_INTERVAL_NUM];

        // 如果总带宽为0,说明网卡异常
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
 函 数 名  : debugGetRaidListInfo
 功能描述  : telnet获取raid组信息
 输入参数  : char *strBufferInfo:缓存区，存放raid组信息
             const uint32_t bufferLen:缓存区长度
 输出参数  : char *strBufferInfo:raid组信息
 返 回 值  : 成功返回0,失败返回 错误码
*****************************************************************************/
void CSvsSysStat::ReadMountFile()
{
    m_bIsMountsOk = false;
    FILE                        *pf;
    pf = fopen("/proc/mounts", "r");
    size_t buffSize = sizeof(m_strMountsInfo);

    /* 虚文件打开一般不会失败，但要保护 */
    // 如果打开虚文件失败，则认为所有的mount信息都在，保证磁盘可用
    if (NULL == pf)
    {
        SVS_LOG((SVS_LM_WARNING,"Open /proc/mounts failed."));
        return;
    }

    /* 获取CPU使用时间信息 */
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

    // 如果读取/proc/mounts失败，则认为mount可用
    if(!m_bIsMountsOk)
    {
        SVS_LOG((SVS_LM_WARNING, "Check mount online status failed.Read /proc/mounts failed."));
        return SVS_STAT_FAILTRUE;
    }

    //查看 /proc/mounts是否有要查询的磁盘信息
    char tempDiskPath[STAT_PATH_LEN + 1];
    (void)snprintf(tempDiskPath, sizeof(tempDiskPath), " %s ", strDiskPath);

    // 如果mount掉线
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
    struct ifreq stArrayIfr[MAX_NETCARD_NUM]; // 最大16块网卡
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

    /* 初始化 */
    memset(&stIfc, 0, sizeof(stIfc));
    memset(&stEcmd, 0, sizeof(ethtool_cmd_t));
    memset(stArrayIfr, 0, sizeof(stArrayIfr));

    stEcmd.cmd = ETHTOOL_GSET;
    stIfc.ifc_len = sizeof(struct ifreq) * MAX_NETCARD_NUM;
    stIfc.ifc_buf = (char *) stArrayIfr;

    /* 获取所有活动的网卡信息 */
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
        /* 获取所有活动的网卡的ip地址及网卡名，如eth1 */
        if (SVS_STAT_OK != ioctl(isock, SIOCGIFADDR, pIfr))
        {
            continue;
        }

        pAddr = (struct sockaddr_in *) (void*)&pIfr->ifr_addr;

        if ((0 != strncmp(pIfr->ifr_name, "eth", strlen("eth"))) // 如果不是eth网卡
         && (0 != strncmp(pIfr->ifr_name, "bond", strlen("bond"))))// 也不是bond网卡，则跳过
        {
            pIfr++;
            continue;
        }

        // 如果IP不是业务网卡IP，则跳过
        if (serviceIp != pAddr->sin_addr.s_addr)
        {
            pIfr++;
            continue;
        }

        // 保存网卡名称
        memset(pNetworkCard->m_strName, 0, sizeof(pNetworkCard->m_strName));
        strncpy(pNetworkCard->m_strName, pIfr->ifr_name, strlen(pIfr->ifr_name));

        // 获取网卡健康状态标识
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

        // 如果网卡不在up或不在running状态
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

        // 业务网卡只有一个,eth网卡或者bond网卡
        if (0 == strncmp(pIfr->ifr_name, "bond", strlen("bond")))// 如果是bond网卡
        {
            nRet = StatBondNetcardInfo(isock, pNetworkCard,pIfr);
            break;
        }
        else // 如果是eth网卡
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
    /* 该ip对应的网卡是bonding，获取该bonding的信息 */
    pIfr->ifr_data = (char *) &stBondStat;
    /* 获取bonding的状态 */
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

        // 如果是主备模式,则获取正在用的主网卡的信息
        if (BOND_MODE_ACTIVEBACKUP == stBondStat.bond_mode)
        {
            // 找到主网卡后，可以跳出循环了
            if(BOND_STATE_ACTIVE == slave_info.state)
            {
                /* 获取bonding对应的slave信息 */
                strncpy(stBondInfo.slaveName[0], slave_info.slave_name, strlen(slave_info.slave_name));
                stBondInfo.slaveNum = 1;
                break;
            }
        }
        else // 如果是负载均衡模式
        {

            strncpy(stBondInfo.slaveName[stBondInfo.slaveNum],
                        slave_info.slave_name,
                        strlen(slave_info.slave_name));
            ++stBondInfo.slaveNum;
        }
    }

    // 获取bond总带宽信息
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
    // 获取网卡总带宽信息
    pIfr->ifr_data = (char *) &stEcmd;
    if (SVS_STAT_OK != ioctl(isock, SIOCETHTOOL, pIfr))
    {
        SVS_LOG((SVS_LM_INFO, "ioctl <SIOCETHTOOL> on %s failed.", pIfr->ifr_name));
        return SVS_STAT_FAILTRUE;
    }
    else if (ABNOMAL_VALUE == stEcmd.speed)    /* all the bit is 1，网卡down掉 */
    {
        SVS_LOG((SVS_LM_WARNING, "The network is down.network name[%s]",pIfr->ifr_name));
        return SVS_STAT_FAILTRUE;
    }

    // 保存网卡总带宽信息
    pNetworkCard->m_ulBWTotal = stEcmd.speed;

    return SVS_STAT_OK;
}


void CSvsSysStat::StatBWInfo(NetworkCardInfo* pNetworkCard)const
{
    /* 以下获取活动网卡的已用带宽 */
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
        // 如果不是要找的eth网卡，则跳过
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

        // 计算网卡的接收流量
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

        // 计算网卡的发送流量
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




