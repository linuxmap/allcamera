#include <sys/ioctl.h>
#include <linux/sockios.h>
#include "svs_rt_record_common.h"
#include "svs_stat_manager.h"
#include "svs_record_disk.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"
#include "svs_real_record_server.h"
#include "svs_bakup_front_record_lens.h"
#include "svs_media_processor.h"

extern CFluxStat g_FluxStat;
void CFluxStat::Stat()
{
    struct timezone tz;
    // ��1��ͳ�ƣ�������ͳ��
    if ( 0 == m_tvLastStatTime.tv_sec )
    {
        (void)gettimeofday( &m_tvLastStatTime, &tz);
        return ;
    }

    // ��ȡͳ�Ƽ��
    struct timeval currTime;
    (void)gettimeofday( &currTime, &tz );
    double diffTime =  (double)(currTime.tv_sec - m_tvLastStatTime.tv_sec) * 1000000;
    diffTime  += (currTime.tv_usec - m_tvLastStatTime.tv_usec );
    diffTime /= 1000000;

    // δ�ﵽ��Сͳ�����ڲ��һ�����ˮλδ�澯��������ͳ��
    bool bMediaBuffAlarmFlag = false;
    float fMediaBuffMaxLevel = SVS_Stat_Manager::instance().getMediaBuffMaxLevel();
    if ( fMediaBuffMaxLevel >= SVS_MEDIA_BUFF_MAX_LEVEL )
    {
        bMediaBuffAlarmFlag = true;
    }

    bool bUpdateLastStatInfoFlag = true;    // �Ƿ�������ͳ����Ϣ
    if ( diffTime <  SVS_FLUX_STAT_MIN_INTERVAL )
    {
        if ( !bMediaBuffAlarmFlag )
        {
            return;
        }

        // ��δ��ͳ��ʱ�䣬��ˮλ���ʱ��������ͳ����Ϣ
        bUpdateLastStatInfoFlag = false;
    }

    uint64_t tmpTotalFlux = 0;
    for ( uint32_t i = 0; i < m_ulStatRecvThreadNum; i++ )
    {
        tmpTotalFlux += m_ullTotalFlux[i];
    }

    uint64_t diffDataFlux = tmpTotalFlux - m_ullLastStatTotalFlux;
    m_ullRate = (uint64_t)(diffDataFlux / diffTime);

    double writeBlockNum = m_ullRate;
    // ������������,��ֹ����ͻȻ����ʱ�������������޷�д�����
    if ( writeBlockNum < SVS_FLUX_MIN_VALUE )
    {
        writeBlockNum = SVS_FLUX_MIN_VALUE ;
    }
    else
    {
        // �ϸ�15%
        writeBlockNum *= 1.15;
    }

    // ���ˮλ��ߣ��Ӵ�������
    if ( bMediaBuffAlarmFlag )
    {
        // ��������£��ϸ�25%+����SVS_MEDIA_BUFF_MAX_LEVEL�İٷֱ�
        writeBlockNum *= (1.25 + fMediaBuffMaxLevel - SVS_MEDIA_BUFF_MAX_LEVEL);
        // ��ֹд���ٶȹ�С
        if ( writeBlockNum < SVS_MEDIA_BUFFER_ALARM_FLUX_MIN_VALUE )
        {
            writeBlockNum = SVS_MEDIA_BUFFER_ALARM_FLUX_MIN_VALUE;
        }
    }
    if((0 != m_ulStatWriteThreadNum) && (0 != m_ulSizePerWrite))
    {
        writeBlockNum = writeBlockNum / ((double)m_ulStatWriteThreadNum * (double)m_ulSizePerWrite);
    }

    // ÿ����΢��д1����ݿ�
    m_lWriteBlockTime = (uint32_t) (1000000 / writeBlockNum);

    // ����ͳ����Ϣ
    if ( bUpdateLastStatInfoFlag )
    {
        m_ullLastStatTotalFlux = tmpTotalFlux;
        m_tvLastStatTime = currTime;

        if ( tmpTotalFlux > 0xffffffff )
        {
            Reset();
        }
    }
}

//==========================SVS_Stat_Manager BEGIN==============================
SVS_Stat_Manager::SVS_Stat_Manager()
{
    m_tvStatInterval           = SVS_STAT_DEFAULT_INTERVAL;
    m_nThreadFlag              = false;
    (void)memset(m_statBuff, 0, sizeof(m_statBuff));
    m_sysStartTime = 0;
    m_totalRecvByte = 0;
    m_currRecvByte = 0;
    m_statFileHandle = -1;
    m_nStatLogLevel = STAT_LOG_LEVEL_INFO;
    m_mediaOutputStatVector.clear();

    // д��Ϣ
    m_totalWriteByte = 0;
    m_currWriteByte = 0;
    m_totalIoScheduleTimes = 0;
    m_currIoScheduleTimes = 0;

    //m_RecordWriteRate = 0; //ʵʱ¼�������,Mbps

    // ϵͳ��Ϣ
    m_systemInfoIndex = 0;
    m_currTxByte = 0;
    for(uint32_t i = 0; i < (SVS_STAT_EFFECT_INTERVAL_NUM +  1); i++)
    {
        m_systemInfoStat[i].reset();
    }

    // ������Ϣ
    m_signalMsgStat.reset();

    // ý�建������Ϣ
    m_mediaBufferStatVector.clear();

    // ����
    m_diskStatVector.clear();

    m_fMediaBuffMaxLevel = 0.0f; // ý�建��������ˮλ

    m_currWriteMB = 0;
    m_currRecvRateMB = 0;
    m_fMediaBuffAveLevel = 0;
}

SVS_Stat_Manager::~SVS_Stat_Manager()
{
    try
    {
        m_tvStatInterval           = SVS_STAT_DEFAULT_INTERVAL;
        m_nThreadFlag              = false;
        (void)memset(m_statBuff, 0, sizeof(m_statBuff));
        m_sysStartTime = 0;
        m_totalRecvByte = 0;
        m_currRecvByte = 0;
        m_statFileHandle = -1;
        m_nStatLogLevel = STAT_LOG_LEVEL_INFO;
        m_mediaOutputStatVector.clear();

        // д��Ϣ
        m_totalWriteByte = 0;
        m_currWriteByte = 0;
        m_totalIoScheduleTimes = 0;
        m_currIoScheduleTimes = 0;

        //m_RecordWriteRate  = 0; //ʵʱ¼�������,Mbps

        // ϵͳ��Ϣ
        m_systemInfoIndex = 0;

        for(uint32_t i = 0; i < (SVS_STAT_EFFECT_INTERVAL_NUM +  1); i++)
        {
            m_systemInfoStat[i].reset();
        }

        // ������Ϣ
        m_signalMsgStat.reset();

        // ý�建������Ϣ
        m_mediaBufferStatVector.clear();

        // ����
        m_diskStatVector.clear();
    }
    catch(...)
    {

    }
}

// ��ʼ��
int32_t SVS_Stat_Manager::open(void * args)
{
    if (NULL == args)
    {
       ; // only for compile warning
    }

    (void)ACE_OS::mkdir(SVS_STAT_LOG_DIR);

    // ����һ��deamon�̣߳�ÿ��һ��ʱ��������ע���������߳��Ƿ�����
    size_t stack_size = SVS_THREAD_STATCK_MAX;
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
        SVS_LOG((SVS_LM_ERROR, "[stat thread]open deamon thread fail."));
    }

    return nRet;
}

// ͳ���߳�
int32_t SVS_Stat_Manager::svc(void)
{
    ACE_thread_t threadID = ACE_OS::thr_self();
    SVS_LOG((SVS_LM_WARNING, "[stat thread]thread[%u] run...", threadID));
    m_sysStartTime = time(NULL);
    char strTime[SVS_STR_TIME_MAX_LEN] = {0,};
    (void)checkStatLogFile();
    ACE_Time_Value delay(1);
    while (!m_nThreadFlag)
    {
        // ÿ��1��ͳ������ͳ��һ��
        for (uint32_t i = 0; i < m_tvStatInterval; i++)
        {
            // Ϊ��ֹsleepʱ�����ܼ�ʱ��Ӧ�˳���Ϣ������ֻsleep 1��
            (void)ACE_OS::sleep(delay);

            if (m_nThreadFlag)
            {
                SVS_LOG((SVS_LM_WARNING, "The stat thread exit.thread id [%d].",threadID));

                return SVS_RESULT_OK;
            }

            // bufferˮλÿ��ͳ��һ��
            (void)getMediaBuffInfo();

            g_FluxStat.Stat();

            // �����ͳ��ϵͳ��Ϣ������
            if(0 == (i) % SVS_STAT_SYSTEM_INFO_INTERVAL)
            {
                collectOffhandInfo();
            }

        }

        if(STAT_LOG_LEVEL_CLOSEED <= m_nStatLogLevel)
        {
            continue;
        }

        // �ж��Ƿ���Ҫ�л�ͳ����־�ļ�
        if(SVS_RESULT_OK != checkStatLogFile())
        {
            continue;
        }

        // ��ӡͷ��Ϣ
        (void)time2string(strTime, sizeof(strTime), time(NULL));
        (void)snprintf(m_statBuff, sizeof(m_statBuff),"\n\tPeriod Stat %s :\n", strTime);
        (void)printStatInfo(m_statFileHandle, m_statBuff, strlen(m_statBuff));

        // ������ͳ�ƾ�ͷ��Ϣ
        int32_t statResult = statAllCameras(m_statFileHandle);
        if(SVS_ERR_INVALIDFD == statResult)
        {
            (void)reopenStatLogFile();
        }

        // ������ͳ�ƴ�����Ϣ
        statResult = statDisks(m_statFileHandle);
        if(SVS_ERR_INVALIDFD == statResult)
        {
            (void)reopenStatLogFile();
        }

        // ������ͳ��IO�����Ϣ
        statResult = statMediaOutput(m_statFileHandle);
        if(SVS_ERR_INVALIDFD == statResult)
        {
            (void)reopenStatLogFile();
        }

        // ���ͳ����Ϣ
        (void)statQueueInfo(m_statFileHandle);

        // ������ͳ��media������ʹ����Ϣ
        statResult = statMediaBufferStat(m_statFileHandle);
        if(SVS_ERR_INVALIDFD == statResult)
        {
            (void)reopenStatLogFile();
        }

        // ���������ɾ����Ϣ
        statResult = statDeleteRecord(m_statFileHandle);
        if(SVS_ERR_INVALIDFD == statResult)
        {
            (void)reopenStatLogFile();
        }

        // ������ͳ����ݿ������Ϣ
        statDBSql(m_statFileHandle);

        // ������ͳ��������Ϣ
        statResult = statSignalMsg(m_statFileHandle);
        if(SVS_ERR_INVALIDFD == statResult)
        {
            (void)reopenStatLogFile();
        }

        // ������ͳ��ϵͳ������Ϣ
        statResult = statSystemInfo(m_statFileHandle);
        if(SVS_ERR_INVALIDFD == statResult)
        {
            (void)reopenStatLogFile();
        }

        // ���ͳ����Ϣ
    }

    SVS_LOG((SVS_LM_WARNING, "The stat thread exit.thread id [%d].",threadID));

    return SVS_RESULT_OK;
}


// �رն���
int32_t SVS_Stat_Manager::close(u_long)
{
    m_nThreadFlag = true;

    m_mediaOutputStatVector.clear();
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_MapMutex_, -1);
    CAMERA_STAT_MAP_ITER iter(m_cameraMap);
    CAMERA_STAT_MAP_ENTRY *pEntry = NULL;
    CameraStat * pCameraStat = NULL;
    ACE_Message_Block* mb = NULL;
    // �ͷ����еĶ���
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pCameraStat = pEntry->int_id_;
        mb = (ACE_Message_Block*)(void*)pCameraStat;
        freeMessage(mb);
    }
    (void)m_cameraMap.unbind_all();

    while(!m_FreeCameraStatList.empty())
    {
        mb = m_FreeCameraStatList.front();
        m_FreeCameraStatList.pop_front();
        delete mb;
    }

    return SVS_RESULT_OK;
}
// ͳ�����о�ͷ����Ϣ
int32_t SVS_Stat_Manager::statAllCameras(int32_t nStatHandle)
{
    if(0 > nStatHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat all cameras information failed. nStatHandle[%d]",
            nStatHandle));

        return SVS_ERR_INVALIDFD;
    }

    // ��ȡhandle ����Ϣ
    int32_t nRet = statAllHandleInfo(nStatHandle);
    if(SVS_RESULT_OK != nRet)
    {
        return nRet;
    }

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_MapMutex_, -1);
    CAMERA_STAT_MAP_ITER iter(m_cameraMap);
    CAMERA_STAT_MAP_ENTRY *pEntry = NULL;
    CameraStat * pCameraStat = NULL;
    m_currRecvByte = 0;
    SVS_LOG((SVS_LM_INFO,
        "Begin to stat all cameras info. camera number[%u]",
        m_cameraMap.current_size()));
    // �������о�ͷ��ͳ����Ϣ
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pCameraStat = pEntry->int_id_;
        int32_t printRet = printCameraInfo(pCameraStat,nStatHandle);
        if(SVS_RESULT_OK != printRet)
        {
            SVS_LOG((SVS_LM_WARNING, "Print all cameras stat information failed."));
            return printRet;
        }

        m_currRecvByte += pCameraStat->m_currRecvByteNum;
    }
    int32_t timeLen = time(NULL) - m_sysStartTime;
    m_totalRecvByte += m_currRecvByte;
    float totalRecvGB = (float)m_totalRecvByte/(1024*1024*1024);
    float totalRecvRateMB = ((float)m_totalRecvByte*8 /(1024*1024)) /timeLen;
    float crrRecvRateMB = ((float)m_currRecvByte*8/(1024*1024)) / m_tvStatInterval;
    (void)snprintf(m_statBuff, sizeof(m_statBuff),
        "\nSystem.Camera number[%u], Total receive size[%.3f]GB,"
        " Total receive rate[%.3f]Mbps, Current receive rate[%.3f]Mbps.\n",
        m_cameraMap.current_size(),
        totalRecvGB,
        totalRecvRateMB,
        crrRecvRateMB);

    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard1, m_PerformanceVarMutex_, -1);
        // ������������
        m_currRecvRateMB = (uint32_t)crrRecvRateMB;
    }

    return printStatInfo(nStatHandle, m_statBuff, strlen(m_statBuff));
}

// ������ͳ�ƴ�����Ϣ
int32_t SVS_Stat_Manager::statDisks(int32_t nStatHandle)
{
    if(0 > nStatHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat disks information failed. nStatHandle[%d]",
            nStatHandle));

        return SVS_ERR_INVALIDFD;
    }

    (void)SVS_Disk_Manager::instance().debugGetRaidListInfo(m_statBuff, sizeof(m_statBuff));

    return printStatInfo(nStatHandle, m_statBuff, strlen(m_statBuff));
}

// ������ͳ��IO�����Ϣ
int32_t SVS_Stat_Manager::statMediaOutput(int32_t nStatHandle)
{
    // �����ж�
    if(0 > nStatHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat media output thread information failed. nStatHandle[%d]",
            nStatHandle));

        return SVS_ERR_INVALIDFD;
    }

    uint32_t writeLen = 0;
    uint32_t leftLen = 0;
    const uint32_t buffSize = sizeof(m_statBuff);
    MediaOutputStat *pszMediaOutputStat;
    uint32_t threadNum = m_mediaOutputStatVector.size();

    // ��ӡͷ��Ϣ
    leftLen = buffSize;
    (void)snprintf(&m_statBuff[writeLen],leftLen,
        "\n[Stat Media Output thread information,thread number[%u]]:\n",
        threadNum);
    m_currWriteByte = 0;
    m_currIoScheduleTimes = 0;
    float totalWriteGB = 0;
    float currWriteMB = 0;
    float currWriteRateMb = 0;
    //float fAllThreadWriteMB = 0;
    // ��ӡÿ������̵߳���Ϣ
    for(uint32_t i = 0; i < threadNum; i++)
    {
        pszMediaOutputStat = &m_mediaOutputStatVector[i];

        if(NULL == pszMediaOutputStat->m_pCMediaOutputStat)
        {
            continue;
        }
        SVS_Media_Output_Stat * pCMediaOutputStat = pszMediaOutputStat->m_pCMediaOutputStat ;
        writeLen = strlen(m_statBuff);
        leftLen = buffSize - writeLen;
        if(1 >= leftLen)
        {
            SVS_LOG((SVS_LM_WARNING, "Stat media output thread OK. No buffer space."));
            break;
        }
        pszMediaOutputStat->m_writeByteNum += pCMediaOutputStat->m_writeByteNum;
        m_currWriteByte += pCMediaOutputStat->m_writeByteNum;

        //fAllThreadWriteMB +=(float)(pCMediaOutputStat->m_writeByteNum)/(1024*1024);

        pszMediaOutputStat->m_writeFrameNum += pCMediaOutputStat->m_writeFrameNum;
        pszMediaOutputStat->m_ioScheduleTimes += pCMediaOutputStat->m_ulIoScheduleTimes;
        m_currIoScheduleTimes += pCMediaOutputStat->m_ulIoScheduleTimes;

        totalWriteGB = (float)pszMediaOutputStat->m_writeByteNum / (1024*1024*1024);
        currWriteMB = (float)pCMediaOutputStat->m_writeByteNum / (1024*1024);
        currWriteRateMb = (float)pCMediaOutputStat->m_writeByteNum*8 / (1024*1024*(float)m_tvStatInterval);
        float totalWriteMF = (float)pszMediaOutputStat->m_writeFrameNum / (1024*1024);
        float currWriteMF = (float)pCMediaOutputStat->m_writeByteNum / (1024*1024);
        (void)snprintf(&m_statBuff[writeLen], leftLen,
               "Thread Index[%u]:\n"
               "Total write byte[%.3f]GB, Current write byte[%.3f]MB,Current write rate[%.3f]Mbps\n"
               "Total write frame[%.3f]MB,Current write frame[%.3f]MB\n"
               "Total IO schedul times[%Lu], Current IO schedul times[%u],"
               "current IO schedule fail times[%u]\n\n",
               pszMediaOutputStat->m_ThreadIndex,
               totalWriteGB, currWriteMB, currWriteRateMb,
               totalWriteMF, currWriteMF,
               pszMediaOutputStat->m_ioScheduleTimes,
               pCMediaOutputStat->m_ulIoScheduleTimes,
               pCMediaOutputStat->m_IoScheduleFailTimes);

        // ������ͳ�Ƶ���Ϣ���
        pCMediaOutputStat->m_writeByteNum = 0;
        pCMediaOutputStat->m_writeFrameNum = 0;
        pCMediaOutputStat->m_ulIoScheduleTimes = 0;
        pCMediaOutputStat->m_IoScheduleFailTimes = 0;
    }
    writeLen = strlen(m_statBuff);
    leftLen = buffSize - writeLen;

    m_totalWriteByte += m_currWriteByte;
    m_totalIoScheduleTimes += m_currIoScheduleTimes;
    totalWriteGB = (float)m_totalWriteByte/(1024*1024*1024);
    currWriteMB = (float)m_currWriteByte/(1024*1024);
    currWriteRateMb = (float)m_currWriteByte*8/(1024*1024* (float)m_tvStatInterval);

    //m_RecordWriteRate = (uint32_t)(fAllThreadWriteMB*8.0/(float)m_tvStatInterval) ;

    (void)snprintf(&m_statBuff[writeLen], leftLen,
               "***System media output stat information:\n"
               "Total write byte[%.3f]GB, Current write byte[%.3f]MB,Current write rate[%.3f]Mbps\n"
               "Total IO schedul times[%Lu], Current IO schedul times[%Lu].\n",
               totalWriteGB, currWriteMB, currWriteRateMb,
               m_totalIoScheduleTimes,m_currIoScheduleTimes);

    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_PerformanceVarMutex_, -1);
        // �����������
        m_currWriteMB = (uint32_t)currWriteRateMb;
    }

    return printStatInfo(nStatHandle, m_statBuff, strlen(m_statBuff));
}

// ͳ��ý�建����ʹ����Ϣ
int32_t SVS_Stat_Manager::statMediaBufferStat(int32_t nStatHandle)
{
    // �����ж�
    if(0 > nStatHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat media buffer stat information failed. nStatHandle[%d]",
            nStatHandle));

        return SVS_ERR_INVALIDFD;
    }

    uint32_t writeLen = 0;
    uint32_t leftLen = 0;
    const uint32_t buffSize = sizeof(m_statBuff);
    MediaBufferStat szMediaBufferStat;
    uint32_t threadNum = m_mediaBufferStatVector.size();

    // ��ӡͷ��Ϣ
    leftLen = buffSize;
    (void)snprintf(&m_statBuff[writeLen],leftLen,
        "\n[Stat Media Buffer Stat information,Receive thread number[%u] ]:\n",
        threadNum);

    uint32_t allocTimes = 0;
    uint32_t allocFaileTime = 0;
    uint32_t allocTmpBuffFailTimes = 0;
    uint32_t freeTime = 0;
    // ��ӡÿ�������̵߳���Ϣ
    for(uint32_t i = 0; i < threadNum; i++)
    {
        if(NULL == m_mediaBufferStatVector[i].m_pCMediaBufferStat)
        {
            continue;
        }

        memcpy(&szMediaBufferStat, &m_mediaBufferStatVector[i], sizeof(szMediaBufferStat));

        SVS_Media_Buffer_Stat* pCMediaBufferStat = szMediaBufferStat.m_pCMediaBufferStat;
        writeLen = strlen(m_statBuff);
        leftLen = buffSize - writeLen;
        if(1 >= leftLen)
        {
            SVS_LOG((SVS_LM_WARNING, "Stat media buffer OK. No buffer space."));
            break;
        }

        // ��ȡ������ͳ�Ƶ���Ϣ,����ʱ���
        allocTimes = pCMediaBufferStat->m_allocTimes;
        allocFaileTime = pCMediaBufferStat->m_allocFailTimes;
        allocTmpBuffFailTimes = pCMediaBufferStat->m_allocTmpBuffFailTimes;
        freeTime = pCMediaBufferStat->m_FreeTimes;

        szMediaBufferStat.m_totalAllocTimes += allocTimes;
        szMediaBufferStat.m_totalAllocFailTimes += allocFaileTime;
        szMediaBufferStat.m_totalFreeTimes += freeTime;
        szMediaBufferStat.m_totalAllocTmpBuffFailTimes += allocTmpBuffFailTimes;

        // ����,��������С���ͷţ���ֵ����ģ�һ���Ƕ��߳�ͬ���������⣬ͳ����ݣ���������
        if(szMediaBufferStat.m_totalAllocTimes < szMediaBufferStat.m_totalFreeTimes)
        {
            szMediaBufferStat.m_totalAllocTimes = szMediaBufferStat.m_totalFreeTimes;
        }

        // �������Ѿ�ʹ�õ�ˮλ
        float waterLevel = (float)(szMediaBufferStat.m_totalAllocTimes - szMediaBufferStat.m_totalFreeTimes)
                           /(szMediaBufferStat.m_bufferNumber);
        waterLevel = RTRECORD_UNIT_PERCENT * waterLevel;

        (void)snprintf(&m_statBuff[writeLen], leftLen,
               "\tReceive Thread Index[%u]:\n"
               "Media buffer size                                       [%u]KB\n"
               "Media buffer number                                     [%u]\n"
               "Total allocate media buffer times                       [%Lu]\n"
               "Current period allocate media buffer times              [%u]\n"
               "Total free media buffer times                           [%Lu]\n"
               "Current period free media buffer times                  [%u]\n"
               "Total allocate media buffer fail times                  [%Lu]\n"
               "Current period allocate media buffer fail times         [%u]\n"
               "Total allocate temp fs media buffer fail times          [%Lu]\n"
               "Current period allocate temp fs media buffer fail times [%u]\n"
               "Media buffer used water level                           [%.2f]Percent\n",
               i,
               (szMediaBufferStat.m_bufferSize / 1024),szMediaBufferStat.m_bufferNumber,
               szMediaBufferStat.m_totalAllocTimes, allocTimes,
               szMediaBufferStat.m_totalFreeTimes, freeTime,
               szMediaBufferStat.m_totalAllocFailTimes, allocFaileTime,
               szMediaBufferStat.m_totalAllocTmpBuffFailTimes, allocTmpBuffFailTimes,
               waterLevel);
    }

    return printStatInfo(nStatHandle, m_statBuff, strlen(m_statBuff));
}

// ��ȡ��Ҫ������Ϣ
int32_t SVS_Stat_Manager::statQueueInfo(int32_t nStatHandle)
{
    // �����ж�
    if(0 > nStatHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat queue info stat information failed. nStatHandle[%d]",
            nStatHandle));

        return SVS_ERR_INVALIDFD;
    }

    uint32_t buffSize = sizeof(m_statBuff);
    uint32_t writeLen = strlen(m_statBuff);
    uint32_t leftLen = buffSize - writeLen;

    // ��ӡͷ��Ϣ
    (void)snprintf(&m_statBuff[writeLen],leftLen,
        "\n[Stat Key Queue information.\n");

    // ͳ��media process��ý��ˮλ
    writeLen = strlen(m_statBuff);
    leftLen = buffSize - writeLen;
    SVS_Media_Processor::instance().statMediaQueueInfo( &m_statBuff[writeLen],leftLen);

    // ͳ���������ˮλ
    writeLen = strlen(m_statBuff);
    leftLen = buffSize - writeLen;
    SVS_IO_Scheduler::instance().statMediaQueueInfo( &m_statBuff[writeLen],leftLen);

    return printStatInfo(nStatHandle, m_statBuff, strlen(m_statBuff));
}

// ��ȡ������Ϣ
int32_t SVS_Stat_Manager::statConfInfo(int32_t nStatHandle)
{
    // �����ж�
    if(0 > nStatHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat configuration information failed. nStatHandle[%d]",
            nStatHandle));

        return SVS_ERR_INVALIDFD;
    }

    CAC_RT_Record_Server* pRtServer = CAC_RT_Record_Server::instance();
    if(NULL == pRtServer)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat configuration information failed. pRtServer[0x%08x]",
            pRtServer));

        return SVS_RESULT_FAILURE;
    }

    uint32_t writeFlag = pRtServer->get_write_media_flowctrl_flag();

    // ��ӡͷ��Ϣ
    (void)snprintf(m_statBuff,sizeof(m_statBuff),
            "\n[Stat Key Conf information.\n"
            "\t RtRecordSvrId               [%s]\n"
            "\t RtRecordSvrServerIp         [%s]\n"
            "\t RtRecordSvrLanAddress       [%s:%u]\n"
            "\t StoreSvrAddress             [%s:%u]\n"
            "\t LoadReportInterval          [%lu]second\n"
            "\t EventRecordLostTimeout      [%lu]second\n"
            "\t CheckDiskInterval           [%lu]second\n"
            "\t RecvMediaBufferSize         [%lu]KB.\n"
            "\t RecvMediaBufferNum          [%lu]\n"
            "\t RecordIndexPath             [%s]\n"
            "\t WriteMediaFlowControlSwitch [%s]\n",
            pRtServer->str_id(),
            pRtServer->str_local_ip(),
            pRtServer->lan_ip(),pRtServer->lan_port(),
            pRtServer->recordctrl_ip(),pRtServer->recordctrl_port(),
            pRtServer->get_load_report_interval(),
            pRtServer->event_record_lost_timeout(),
            pRtServer->get_check_disk_interval(),
            pRtServer->recv_media_buffer_size() / 1024,
            pRtServer->recv_media_buffer_num(),
            pRtServer->get_record_indexfile_path(),
            (0 == writeFlag)?"ON":"OFF");

    return printStatInfo(nStatHandle, m_statBuff, strlen(m_statBuff));
}


// ��ȡ��ͷ��Ϣ
int32_t SVS_Stat_Manager::statAllHandleInfo(int32_t nStatHandle)
{
    if(0 > nStatHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat all handle information failed.The parameter is invalid.nStatHandle[%d]",
            nStatHandle));

        return SVS_RESULT_FAILURE;
    }

    uint32_t startHandle = 0;
    bool bFinish = false;

    do
    {
        uint32_t buffSize = sizeof(m_statBuff);
        memset(m_statBuff, 0 , buffSize);

        int32_t nRet = SVS_Media_Processor::instance().statAllHandleInfo(startHandle,m_statBuff, buffSize);

        if(SVS_RESULT_OK != nRet)
        {
            bFinish = true;
            break;
        }

        nRet = printStatInfo(nStatHandle, m_statBuff, strlen(m_statBuff));

        if(SVS_RESULT_OK != nRet)
        {
            bFinish = true;
        }

    }while(!bFinish);

    return SVS_RESULT_OK;
}

// ���������ɾ����Ϣ
int32_t SVS_Stat_Manager::statDeleteRecord(int32_t /*nStatHandle*/)const
{
    return SVS_RESULT_OK;
}

// ������ͳ����ݿ������Ϣ
void SVS_Stat_Manager::statDBSql(int32_t /*nStatHandle*/)const
{
    return;
}

// ������ͳ��������Ϣ
int32_t SVS_Stat_Manager::statSignalMsg(int32_t nStatHandle)
{
    if(0 > nStatHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat signal message information failed. nStatHandle[%d]",
            nStatHandle));

        return SVS_ERR_INVALIDFD;
    }

    if(NULL == m_signalMsgStat.m_pCSignalMsgStat)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat signal message information failed.SignalMsgStat[0x%08x]",
            m_signalMsgStat.m_pCSignalMsgStat));

        return SVS_RESULT_OK;
    }

    SVS_Signal_Message_Stat* pCSignalMsgStat = m_signalMsgStat.m_pCSignalMsgStat;
    char strTempTime[SVS_STR_TIME_MAX_LEN] = {0,};
    (void)time2string(strTempTime, sizeof(strTempTime), pCSignalMsgStat->m_lastConnStoreCtrlTime);
    string strLastConnTime = strTempTime;
    (void)time2string(strTempTime, sizeof(strTempTime), pCSignalMsgStat->m_lastDisconnStoreCtrlTime);
    string strLastDisconnTime = strTempTime;

    m_signalMsgStat.m_totalDisconnStoreCtrlTimes += pCSignalMsgStat->m_disconnStoreCtrlTimes;
    m_signalMsgStat.m_totalConnStoreCtrlFailTimes += pCSignalMsgStat->m_connStoreCtrlFailTimes;
    m_signalMsgStat.m_totalHearbeatTimeoutTimes += pCSignalMsgStat->m_hearbeatTimeoutTimes;
    m_signalMsgStat.m_totalMsgCount += pCSignalMsgStat->m_messageCount;

    (void)snprintf(m_statBuff, sizeof(m_statBuff),
            "\n[Stat Signal Message Info]:\n"
            "Last connect with storeCtrl time[%s], Last disconnect from storeCtrl time[%s];\n"
            "Total disconnect from strorCtrl times[%Lu], Currnet disconnect from strorCtrl times[%u];\n"
            "Total connect with strorCtrl fail times[%Lu], Currnet connect with strorCtrl fail times[%u];\n"
            "Total heartbeat timeout times[%Lu], Currnet heartbeat timeout times[%u];\n"
            "Total signal message count[%Lu],Current signal message count[%u].\n",
            strLastConnTime.c_str(),strLastDisconnTime.c_str(),
            m_signalMsgStat.m_totalDisconnStoreCtrlTimes,pCSignalMsgStat->m_disconnStoreCtrlTimes,
            m_signalMsgStat.m_totalConnStoreCtrlFailTimes,pCSignalMsgStat->m_connStoreCtrlFailTimes,
            m_signalMsgStat.m_totalHearbeatTimeoutTimes,pCSignalMsgStat->m_hearbeatTimeoutTimes,
            m_signalMsgStat.m_totalMsgCount, pCSignalMsgStat->m_messageCount);

    pCSignalMsgStat->m_connStoreCtrlFailTimes = 0;
    pCSignalMsgStat->m_connStoreCtrlFailTimes = 0;
    pCSignalMsgStat->m_hearbeatTimeoutTimes = 0;
    pCSignalMsgStat->m_messageCount = 0;

    return printStatInfo(nStatHandle, m_statBuff, strlen(m_statBuff));

}

// ������ͳ��ϵͳ������Ϣ
int32_t SVS_Stat_Manager::statSystemInfo(int32_t nStatHandle)
{
    if(0 > nStatHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat system information failed. nStatHandle[%d]",
            nStatHandle));

        return SVS_ERR_INVALIDFD;
    }

    float memUseRate = 0;
    SystemInfoStat *pSystemInfo = &m_systemInfoStat[SVS_STAT_EFFECT_INTERVAL_NUM];
    if(pSystemInfo->m_totalMem > 0)
    {
        memUseRate = (float)((float)pSystemInfo->m_usedMem * RTRECORD_UNIT_PERCENT / pSystemInfo->m_totalMem);
    }

    float bandwidthUseRate = 0;
    if(pSystemInfo->m_bandwidthTotal > 0)
    {
        bandwidthUseRate = ((float)pSystemInfo->m_bandwidthUsed * RTRECORD_UNIT_PERCENT ) / ((float)pSystemInfo->m_bandwidthTotal * 1000);
    }

    float diskTotalGB = (float)pSystemInfo->m_diskTotalSize / (1024*1024);
    float diskUsableGB = (float)pSystemInfo->m_diskUsableSize / (1024*1024);
    float diskUsableRate = 0;
    if(diskTotalGB > 0)
    {
        diskUsableRate = (diskUsableGB * RTRECORD_UNIT_PERCENT ) /  diskTotalGB;
    }

    (void)snprintf(m_statBuff, sizeof(m_statBuff),
                  "\n[Stat system information]: \n"
                  "CPU:Used Cpu[%u]Percent\n"
                  "Memory:Total[%u]MB, Used[%u]MB, Free[%u]MB, UsedRate[%.2f]Percent.\n"
                  "Network Bandwidth:Total[%u]Mb, Used[%u]kb, UsedRate[%.2f]Percent.\n"
                  "Disks:Total[%.2f]GB, Usable[%.2f]GB, UsableRate[%.2f]Percent.\n",
                  pSystemInfo->m_usedCpu,
                  (pSystemInfo->m_totalMem / 1024),
                  (pSystemInfo->m_usedMem / 1024),
                  ((pSystemInfo->m_totalMem - pSystemInfo->m_usedMem) / 1024),
                  memUseRate,
                  pSystemInfo->m_bandwidthTotal,
                  pSystemInfo->m_bandwidthUsed,
                  bandwidthUseRate,
                  diskTotalGB, diskUsableGB, diskUsableRate);


    return printStatInfo(nStatHandle, m_statBuff, strlen(m_statBuff));

}

// ��ͷ��ͳ��ģ��ע��
int32_t SVS_Stat_Manager::cameraRegister( const char* strCameraId,
                                          enCameraObjectType objectType,
                                          void* objectPointer)
{
    // �����ж�
    if((NULL == strCameraId) || (NULL == objectPointer))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Camera register to stat manager failed."
            "strCameraId[0x%08x],objectType[%d],objectPointer[0x%08x]",
            strCameraId,
            objectType,
            objectPointer));

        return SVS_ERR_PARAM;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Camera begin to register to stat manager."
        "CameraId[%s],objectType[%d],objectPointer[0x%08x]",
        strCameraId,
        objectType,
        objectPointer));

    // ע����Ҫ��ȡ��
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_MapMutex_, -1);
    if (CAMERA_OBJECT_INFORM == objectType)
    {
        return addCamera(strCameraId,(SVS_Inform_Stat*)objectPointer);
    }

    CameraStat * pCameraStat = NULL;
    // ������������ҵ���ͷͳ�ƶ���
    if (0 != m_cameraMap.find(strCameraId, pCameraStat))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Camera register to stat manager failed.Can not find camera stat."
            "CameraId[%s],objectType[%d],objectPointer[0x%08x]",
            strCameraId,
            objectType,
            objectPointer));

        return SVS_RESULT_FAILURE;
    }

    switch(objectType)
    {
        case CAMERA_OBJECT_DEVICE:
        {
            pCameraStat->m_pDeviceStat = (SVS_Device_Stat*)objectPointer;
        }
        break;

        case CAMERA_OBJECT_INGRESS:
        {
            pCameraStat->m_pIngressStat = (SVS_Ingress_Stat*)objectPointer;
        }
        break;

        case CAMERA_OBJECT_EGRESS:
        {
            pCameraStat->m_pEgressStat = (SVS_Egress_Stat*)objectPointer;
        }
        break;
        case CAMERA_OBJECT_INFORM:
        break;
        default:
            SVS_LOG((SVS_LM_ERROR,
                "Camera register to stat manager failed."
                " cameraId[%s],objectType[%d],objectPointer[0x%08x]",
                strCameraId,
                objectType,
                objectPointer));
            return SVS_ERR_PARAM;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Camera register to stat manager successful."
        "CameraId[%s],objectType[%d],objectPointer[0x%08x]",
        strCameraId,
        objectType,
        objectPointer));
    return SVS_RESULT_OK;
}

// ��ͷ��ͳ��ģ��ȥע��
int32_t SVS_Stat_Manager::cameraUnregister( const char* strCameraId,enCameraObjectType objectType)
{
    // �����ж�
    if(NULL == strCameraId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Camera unregister from stat manager failed."
            "strCameraId[0x%08x],objectType[%d]",
            strCameraId,
            objectType));

        return SVS_ERR_PARAM;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Camera begin to unregister from stat manager."
        "CameraId[%s],objectType[%d]",
        strCameraId,
        objectType));

    // ע����Ҫ��ȡ��
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_MapMutex_, -1);
    if (CAMERA_OBJECT_INFORM == objectType)
    {
        removeCamera(strCameraId);
        return SVS_RESULT_OK;
    }

    CameraStat * pCameraStat = NULL;
    // ������������ҵ���ͷͳ�ƶ���
    if (0 != m_cameraMap.find(strCameraId, pCameraStat))
    {
        return SVS_RESULT_FAILURE;
    }

    switch(objectType)
    {
        case CAMERA_OBJECT_DEVICE:
        {
            pCameraStat->m_pDeviceStat = NULL;
        }
        break;

        case CAMERA_OBJECT_INGRESS:
        {
            pCameraStat->m_pIngressStat = NULL;
        }
        break;

        case CAMERA_OBJECT_EGRESS:
        {
            pCameraStat->m_pEgressStat = NULL;
        }
        break;
        case CAMERA_OBJECT_INFORM:
        break;
        default:
            SVS_LOG((SVS_LM_ERROR,
                "Camera unregister from stat manager failed."
                " cameraId[%s],objectType[%d].",
                strCameraId,
                objectType));

            return SVS_ERR_PARAM;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Camera register to stat manager successful."
        "CameraId[%s],objectType[%d]",
        strCameraId,
        objectType));

    return SVS_RESULT_OK;
}

int32_t SVS_Stat_Manager::addCamera(const char * strCameraId,SVS_Inform_Stat* pInformStat)
{
    // ����Ѿ����ڸþ�ͷ�򷵻سɹ�,�������ע����Ϣ
    CameraStat * pCameraStat = NULL;
    if (0 == m_cameraMap.find(strCameraId, pCameraStat))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera register to stat manager ok. The object exit already.cameraId[%s]",
            strCameraId));

        return SVS_RESULT_OK;
    }

    // �����ڴ�
    ACE_Message_Block* mb = NULL;
    mb = allocMessage();
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera register to stat manager ok. No memory.cameraId[%s]",
            strCameraId));

        return SVS_ERR_NOMEM;
    }

    // ע��
    pCameraStat = (CameraStat*)(void*)mb->base();
    pCameraStat->reset();

    //memcpy(pCameraStat->m_strCameraId, strCameraId, SVS_DEVICEID_LEN);
    SetLocalizeDeviceId((uint8_t *)pCameraStat->m_strCameraId,
                    sizeof(pCameraStat->m_strCameraId),
                    strCameraId);

    pCameraStat->m_pInformStat = pInformStat;
    if(0 != m_cameraMap.bind(strCameraId, pCameraStat))
    {
        SVS_LOG((SVS_LM_INFO,
            "Camera regiest to stat manager succssful.Bind to map failed."
            "cameraId[%s],pInformStat[0x%08x]",
            strCameraId,
            pInformStat));

        freeMessage(mb);

        return SVS_RESULT_FAILURE;
    }
    ACE_OS::last_error(0);
    SVS_LOG((SVS_LM_INFO,
        "Camera regiest to stat manager succssful."
        "cameraId[%s],pInformStat[0x%08x]",
        strCameraId,
        pInformStat));

    char strTime[SVS_STR_TIME_MAX_LEN] = {0,};
    (void)time2string(strTime, sizeof(strTime), time(NULL));
    (void)snprintf(m_statBuff, sizeof(m_statBuff),
                   "[%s] Camera[%s] register to stat manager.\n",
                   strTime,
                   strCameraId);

    return printStatInfo(m_statFileHandle, m_statBuff, strlen(m_statBuff));

}

void SVS_Stat_Manager::removeCamera(const char* strCameraId)
{
    if (0 != m_cameraMap.unbind(strCameraId))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera remove form map failed.cameraId[%s].",
            strCameraId));
        return;
    }

    SVS_LOG((SVS_LM_INFO,
        "Camera remove from map succussful.cameraId[%s]",
        strCameraId));

    char strTime[SVS_STR_TIME_MAX_LEN] = {0,};
    (void)time2string(strTime, sizeof(strTime), time(NULL));
    (void)snprintf(m_statBuff, sizeof(m_statBuff),
                   "[%s] Camera[%s] unregister from stat manager.\n",
                   strTime,
                   strCameraId);

    (void)printStatInfo(m_statFileHandle, m_statBuff, strlen(m_statBuff));

    return;
}
ACE_Message_Block* SVS_Stat_Manager::allocMessage()
{
    ACE_Message_Block* pMsg = NULL;
    if (m_FreeCameraStatList.empty())
    {
        try
        {
            pMsg = new ACE_Message_Block(sizeof(CameraStat));
        }
        catch(...)
        {
            return NULL;
        }
    }
    else
    {
        pMsg = m_FreeCameraStatList.front();
        m_FreeCameraStatList.pop_front();
    }

    //����ACE_Message_Block��release�ӿ��ͷſռ䣬����lint�澯
    return pMsg;
}//lint !e429

void SVS_Stat_Manager::freeMessage(ACE_Message_Block *pMsg)
{
    if (NULL == pMsg)
    {
        return;
    }

    pMsg->reset();

    m_FreeCameraStatList.push_back(pMsg);

    return;
}

int32_t SVS_Stat_Manager::mediaOutputThreadRegister(uint32_t threadIndex, SVS_Media_Output_Stat* pMediaOutputStat)
{
    if(NULL == pMediaOutputStat)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Media output thread register to stat manager failed.The parameter is invalid."
            "media output thread index[%d],pMediaOutputStat[0x%08x].",
            threadIndex,
            pMediaOutputStat));
        return SVS_ERR_PARAM;
    }

    MediaOutputStat szMediaOutputStat;
    szMediaOutputStat.reset();
    szMediaOutputStat.m_ThreadIndex = threadIndex;
    szMediaOutputStat.m_pCMediaOutputStat = pMediaOutputStat;

    m_mediaOutputStatVector.push_back(szMediaOutputStat);

    return SVS_RESULT_OK;
}

// ��Ϣ��Ϣͳ��ע��ӿ�
int32_t SVS_Stat_Manager::signalMsgStatRegister(SVS_Signal_Message_Stat* pSignalMsgStat)
{
    if(NULL == pSignalMsgStat)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Signal message stat register to stat manager failed.pSignalMsgStat[0x%08x]",
            pSignalMsgStat));

        return SVS_ERR_PARAM;
    }

    m_signalMsgStat.m_pCSignalMsgStat = pSignalMsgStat;

    return SVS_RESULT_OK;
}

// ��Ϣ��Ϣͳ��ע��ӿ�
int32_t SVS_Stat_Manager::diskStatRegister(SVS_Disk_Stat* pDiskStat)
{
    if(NULL == pDiskStat)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Disk register to stat manager failed.pDiskStat[0x%08x]",
            pDiskStat));

        return SVS_ERR_PARAM;
    }

    // ����vector
    DiskStat szDiskStat;
    szDiskStat.reset();
    szDiskStat.m_pDiskStat = pDiskStat;

    m_diskStatVector.push_back(szDiskStat);

    return SVS_RESULT_OK;
}

int32_t SVS_Stat_Manager::printCameraInfo(CameraStat * pCameraStat,int32_t nStatHandle)
{
    uint32_t writeBuffLen = 0;
    uint32_t leftBuffLen = 0;
    uint32_t unBuffSize = sizeof(m_statBuff);
    char strTime[SVS_STR_TIME_MAX_LEN] = {0,};
    string strStartTime;
    string strEndTime;
    int32_t timeLen = 0;
    if(NULL != pCameraStat->m_pInformStat)
    {
        writeBuffLen = strlen(m_statBuff);
        leftBuffLen = unBuffSize - writeBuffLen;
        (void)time2string(strTime, SVS_STR_TIME_MAX_LEN, pCameraStat->m_pInformStat->m_strategyStartTime);
        strStartTime = strTime;
        (void)time2string(strTime, SVS_STR_TIME_MAX_LEN, pCameraStat->m_pInformStat->m_strategyEndTime);
        strEndTime = strTime;
        string strTemp = "NO";
        if(pCameraStat->m_pInformStat->m_isEventEffective)
        {
            strTemp = "YES";
        }

        (void)snprintf(m_statBuff, leftBuffLen,
            "CameraId[%s]:\nInform:strategy time seg[%s~%s], record type[%d],isEventEffective[%s].",
            pCameraStat->m_strCameraId,
            strStartTime.c_str(),
            strEndTime.c_str(),
            pCameraStat->m_pInformStat->m_strategyType,
            strTemp.c_str());
    }
    if (NULL != pCameraStat->m_pDeviceStat)
    {
        writeBuffLen = strlen(m_statBuff);
        leftBuffLen = unBuffSize - writeBuffLen;
        (void)time2string(strTime, SVS_STR_TIME_MAX_LEN, pCameraStat->m_pDeviceStat->m_eventHappenTime);
        strStartTime = strTime;
        (void)time2string(strTime, SVS_STR_TIME_MAX_LEN, pCameraStat->m_pDeviceStat->m_eventOverTime);
        strEndTime = strTime;
        (void)time2string(strTime, SVS_STR_TIME_MAX_LEN, pCameraStat->m_pDeviceStat->m_recordStartTime);
        string startRecordTime = strTime;
        (void)snprintf(&m_statBuff[writeBuffLen], leftBuffLen,
            "Device:start record time[%s],event happen and over time seg[%s~%s], event total number[%u].",
            startRecordTime.c_str(),
            strStartTime.c_str(),
            strEndTime.c_str(),
            pCameraStat->m_pDeviceStat->m_eventTotalNum);
        timeLen = time(NULL) - pCameraStat->m_pDeviceStat->m_recordStartTime;
    }

    if(NULL != pCameraStat->m_pIngressStat)
    {
        SVS_Ingress_Stat * pIngressStat = pCameraStat->m_pIngressStat;
        writeBuffLen = strlen(m_statBuff);
        leftBuffLen = unBuffSize - writeBuffLen;
        (void)time2string(strTime, SVS_STR_TIME_MAX_LEN, pIngressStat->m_lastRecvPachetTime);
        strStartTime = strTime;
        (void)snprintf(&m_statBuff[writeBuffLen], leftBuffLen,
            "Ingress:Recv last packet time[%s],receive byte number[%u],receive frame number[%u]"
            "lost frame number[%u], lost packet number[%u], error frame number[%u], "
            "receive frame rate[%u],receive rate[%u]Kbps.",
            strStartTime.c_str(),
            pIngressStat->m_recvByteNum,
            pIngressStat->m_recvFrameNum,
            pIngressStat->m_lostFrameNum,
            pIngressStat->m_lostPacketNum,
            pIngressStat->m_errFrameNum,
            pIngressStat->m_recvFrameNum/m_tvStatInterval,
            (pIngressStat->m_recvByteNum * 8) / (m_tvStatInterval*1024));

        pCameraStat->m_recvTotalByteNum += pIngressStat->m_recvByteNum;
        pCameraStat->m_currRecvByteNum = pIngressStat->m_recvByteNum;
        pIngressStat->m_recvByteNum = 0;
        pCameraStat->m_recvTotalFrameNum += pIngressStat->m_recvFrameNum;
        pIngressStat->m_recvFrameNum = 0;
        pCameraStat->m_lostFrameNum += pIngressStat->m_lostFrameNum;
        pIngressStat->m_lostFrameNum = 0;
        pCameraStat->m_lostPacketNum += pIngressStat->m_lostPacketNum;
        pIngressStat->m_lostPacketNum = 0;
        pCameraStat->m_errFrameNum += pIngressStat->m_errFrameNum;
        pIngressStat->m_errFrameNum = 0;
    }

    if(NULL != pCameraStat->m_pEgressStat)
    {
        SVS_Egress_Stat * pEgressStat = pCameraStat->m_pEgressStat;
        writeBuffLen = strlen(m_statBuff);
        leftBuffLen = unBuffSize - writeBuffLen;
        (void)time2string(strTime, SVS_STR_TIME_MAX_LEN, pEgressStat->m_indexStartTime);
        strStartTime = strTime;
        (void)time2string(strTime, SVS_STR_TIME_MAX_LEN, pEgressStat->m_indexEndTime);
        strEndTime = strTime;
        (void)snprintf(&m_statBuff[writeBuffLen], leftBuffLen,
            "Egress:index time seg[%s~%s],file name[%s],write byte number[%u].",
            strStartTime.c_str(),
            strEndTime.c_str(),
            pEgressStat->m_fileName,
            pEgressStat->m_writeByteNum);
        pCameraStat->m_writeByteNum += pEgressStat->m_writeByteNum;
        pEgressStat->m_writeByteNum = 0;
        pCameraStat->m_writeFrameNum += pEgressStat->m_writeFrameNum;
        pEgressStat->m_writeFrameNum = 0;
    }

    float TotalRecvRate = 0;
    if(0 != timeLen)
    {
        TotalRecvRate = (float)pCameraStat->m_recvTotalByteNum*8 /((float)timeLen * 1024);
    }
    float currRecvRate = ((float)pCameraStat->m_currRecvByteNum*8) /((float)m_tvStatInterval* 1024);
    float totalRecvMB = (float)pCameraStat->m_recvTotalByteNum/(1024*1024);
    float lostFrameRate = (float)pCameraStat->m_lostFrameNum/pCameraStat->m_recvTotalFrameNum;

    if (STAT_LOG_LEVEL_DEBUG == m_nStatLogLevel)
    {
        writeBuffLen = strlen(m_statBuff);
        leftBuffLen = unBuffSize - writeBuffLen;
        (void)snprintf(&m_statBuff[writeBuffLen], leftBuffLen,
                "Camera total:Total receive size[%.3f]MB,Total receive rate[%.3f]Kbps."
                "Current receive rate[%.3f]Kbps.Total lost frame rate[%.3f].\n",
                totalRecvMB,
                TotalRecvRate,
                currRecvRate,
                lostFrameRate);
    }
    else
    {
        leftBuffLen = strlen(m_statBuff);
        (void)snprintf(m_statBuff, leftBuffLen,
                "Camera[%s]:Total receive size[%.3f]MB,Total receive rate[%.3f]Kbps."
                "Current receive rate[%.3f]Kbps.Total lost frame rate[%.3f].\n",
                pCameraStat->m_strCameraId,
                totalRecvMB,
                TotalRecvRate,
                currRecvRate,
                lostFrameRate);
    }

    return printStatInfo(nStatHandle, m_statBuff, strlen(m_statBuff));
}

void SVS_Stat_Manager::collectOffhandInfo()
{
    // ����ϵͳ��Ϣ�����±�
    ++m_systemInfoIndex;
    m_systemInfoIndex %= SVS_STAT_EFFECT_INTERVAL_NUM;

    // ��ȡcpu��Ϣ
    getCpuInfo();

    // ��ȡ�ڴ���Ϣ
    getMemInfo();

    // ��ȡ����Ϣ
    getNetcardInfo();

    // ��ȡ������Ϣ
    getDiskInfo();

    // �Ѿ�ÿ�붼����ˮλ�ˣ����ﲻ��Ҫ�ٴμ�����
//    // ��ȡý�建������Ϣ
//    (void)getMediaBuffInfo();

    // ����ƽ��ֵ
    SystemInfoStat szSystemInfo;
    szSystemInfo.reset();
    for(uint32_t i = 0; i < SVS_STAT_EFFECT_INTERVAL_NUM; i++)
    {
         // szSystemInfo + m_systemInfoStat[i];
         szSystemInfo.operator+( m_systemInfoStat[i] );
    }
    // szSystemInfo / SVS_STAT_EFFECT_INTERVAL_NUM;
    szSystemInfo.operator/( SVS_STAT_EFFECT_INTERVAL_NUM  );

    memcpy(&m_systemInfoStat[SVS_STAT_EFFECT_INTERVAL_NUM], &szSystemInfo, sizeof(szSystemInfo));

    // ���ĳ��ֵͻȻ���꣬��ʱ�ϱ�����
    float bandwidthRate = RTRECORD_UNIT_PERCENT;
    SystemInfoStat *pSystemInfo = &m_systemInfoStat[m_systemInfoIndex];
    if(0 < pSystemInfo->m_bandwidthTotal)
    {
        bandwidthRate = ((float)pSystemInfo->m_bandwidthUsed * RTRECORD_UNIT_PERCENT) / ((float)pSystemInfo->m_bandwidthTotal * 1024);
    }

    // ����ʱ��������
    int32_t startTimeLen = time(NULL) - m_sysStartTime;
    if ((70 <= pSystemInfo->m_usedCpu)
     || (90 <= bandwidthRate)
     || (85 <= pSystemInfo->m_mediaBuffWaterLevel) )
    {
        SVS_LOG((SVS_LM_WARNING,
            "System load changed. Create load report."
            "currnet load:cpu[%u]percent, networt bandwitdth rate[%u]percent, media buffer water level[%u].",
            pSystemInfo->m_usedCpu,
            (uint32_t)bandwidthRate,
            (uint32_t)(pSystemInfo->m_mediaBuffWaterLevel)));

        CAC_RT_Record_Server::instance()->add_load_report_task();
    }
    else if ( (5 * SVS_STAT_SYSTEM_INFO_INTERVAL >= startTimeLen) )
    {
        SVS_LOG((SVS_LM_INFO,
            "Create load report."
            "currnet load:cpu[%u]percent, networt bandwitdth rate[%u]percent, media buffer water level[%u].",
            pSystemInfo->m_usedCpu,
            (uint32_t)bandwidthRate,
            (uint32_t)(pSystemInfo->m_mediaBuffWaterLevel)));

        CAC_RT_Record_Server::instance()->add_load_report_task();
    }

    return;
}

/*******************************************************************************
  ������          : CAAPMsgHandle::getCpuInfo
  ��������        : ��ȡ�豸��ǰCUP��ʹ���ʡ�
  �������        :
  �������        :
  ����ֵ          : int32_t
  ���ú���        :
  ��������        : alarmThread
  �޸���ʷ        :
  1 ����          : 2007-4-9
    ����          : q60020228
    �޸ķ�ʽ      : ���
*******************************************************************************/
void SVS_Stat_Manager::getCpuInfo()
{
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
        return;
    }

    /* ��ȡCPUʹ��ʱ����Ϣ */
    if (0 == fread(strStatInfo, sizeof(char), MAXNAMELEN, pf))
    {
        fclose(pf);
        return;
    }
    fclose(pf);
    char *pszTmp = strstr(strStatInfo, "cpu ");
    if (NULL == pszTmp)
    {
        return;
    }

    // cpu�İ�u�ַ���޷�ų�ͻ�����Ǵ���
    sscanf(pszTmp, "cpu %Lu %Lu %Lu %Lu\n", &userTime, &niceTime, &systTime, &idleTime);//lint !e561 !e566

    /* �״����в����� */
    if (0 == lastUserTime)
    {
        lastUserTime = userTime;
        lastNiceTime = niceTime;
        lastSysTime = systTime;
        lastIdleTime = idleTime;
        return;
    }

    /* ����CPUռ���� */
    totalIdleTime = (uint64_t) (idleTime - lastIdleTime);
    totalUsedTime = (uint64_t) ((userTime - lastUserTime)
                             + (niceTime - lastNiceTime)
                             + (systTime - lastSysTime)
                             + (idleTime - lastIdleTime));

    // ����������̫�����òɼ�
    if (1000 > totalUsedTime)
    {
        return;
    }

    SystemInfoStat *pSystemInfo = &m_systemInfoStat[m_systemInfoIndex];

    // �����ϸ����ڵ�cpuֵ��Ϊ���ֵ���������
    pSystemInfo->m_usedCpu = (uint32_t)(((totalUsedTime - totalIdleTime) * RTRECORD_UNIT_PERCENT)
                / totalUsedTime);

    lastUserTime = userTime;
    lastNiceTime = niceTime;
    lastSysTime = systTime;
    lastIdleTime = idleTime;

    return;
}

void SVS_Stat_Manager::getMemInfo()
{
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
        return ;
    }
    SystemInfoStat *pSystemInfo = &m_systemInfoStat[m_systemInfoIndex];
    while (fgets(cBuff, sizeof(cBuff), fp))
    {
        if (!strncmp(cBuff, "MemTotal:", strlen("MemTotal:")))
        {
            sscanf(cBuff, "MemTotal: %d kB", &imemTotal);
        }

        pSystemInfo->m_totalMem = (uint32_t)imemTotal;
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

    // �����Ѿ�ʹ���ڴ�
    pSystemInfo->m_usedMem = pSystemInfo->m_totalMem - (uint32_t)(imemfree + ibuffer + icache);

    // �ر��ڴ������ļ�
    (void)fclose(fp);

    return ;
}

// ��ȡ����Ϣ
void SVS_Stat_Manager::getNetcardInfo()
{
    static int32_t isock = -1;
    in_addr_t serviceIp = CAC_RT_Record_Server::instance()->get_local_ip(0);
    struct ifconf stIfc;
    struct ifreq         *pIfr = NULL;
    struct ifreq stArrayIfr[MAX_NETCARD_NUM]; // ���16����
    struct sockaddr_in   *pAddr = NULL;
    ethtool_cmd_t stEcmd;          // = { ETHTOOL_GSET };
    NetcardInfo szNetcardInfoList[MAX_NETCARD_NUM];

    if (-1 == isock)
    {
        isock = socket(AF_INET, SOCK_DGRAM, 0);
        if(0 > isock)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Get netcard info failed. Create socket failed."));
            return;
        }
    }

    /* ��ʼ�� */
    memset(&stIfc, 0, sizeof(stIfc));
    memset(&stEcmd, 0, sizeof(ethtool_cmd_t));
    memset(stArrayIfr, 0, sizeof(stArrayIfr));
    memset(szNetcardInfoList, 0, sizeof(szNetcardInfoList));

    stEcmd.cmd = ETHTOOL_GSET;
    stIfc.ifc_len = sizeof(struct ifreq) * MAX_NETCARD_NUM;
    stIfc.ifc_buf = (char *) stArrayIfr;

    /* ��ȡ���л������Ϣ */
    if (ioctl(isock, SIOCGIFCONF, &stIfc) < 0)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get all active netcard failed.ioctl <SIOCGIFCONF> failed."));
        return;
    }

    pIfr = stIfc.ifc_req;

    SystemInfoStat szBandwidthInfo;
    szBandwidthInfo.reset();
    int32_t nRet = SVS_RESULT_FAILURE;
    SystemInfoStat *pSystemInfo = &m_systemInfoStat[m_systemInfoIndex];
    for (uint32_t i = 0; i < (uint32_t) stIfc.ifc_len; i += sizeof(struct ifreq))
    {
        /* ��ȡ���л�����ip��ַ��������eth1 */
        if (SVS_RESULT_OK != ioctl(isock, SIOCGIFADDR, pIfr))
        {
            continue;
        }

        pAddr = (struct sockaddr_in *) (void*)&pIfr->ifr_addr;

        /*if ((0 == strncmp(pIfr->ifr_name, "eth", strlen("eth"))) // �����eth��
         && (0 != strncmp(pIfr->ifr_name, "bond", strlen("bond"))))// Ҳ����bond�������
        {
            pIfr++;
            continue;
        }*/

        // ���IP����ҵ����IP�������
        if (serviceIp != pAddr->sin_addr.s_addr)
        {
            pIfr++;
            continue;
        }

        // ���������
        strncpy(pSystemInfo->m_netcardName, pIfr->ifr_name, sizeof(pSystemInfo->m_netcardName));

        // ��ȡ����״̬��ʶ
        if (SVS_RESULT_OK != ioctl(isock, SIOCGIFFLAGS, pIfr))
        {
            SVS_LOG((SVS_LM_WARNING,"get netcard%u 's information failed\n", i));
            nRet = SVS_RESULT_FAILURE;
            break;
        }

        // �������up����running״̬
        if (!(pIfr->ifr_flags & IFF_UP) || !(pIfr->ifr_flags & IFF_RUNNING))
        {
            SVS_LOG((SVS_LM_WARNING,
                "netcard is down or not running, please check it.network name[%s]",
                pIfr->ifr_name));

            //��������ó���ͬ��IPʱ��ȡ����Ϊ0
            //nRet = SVS_RESULT_FAILURE;
            //break;
            pIfr++;
            continue;
        }

        // ҵ����ֻ��һ��,eth�����bond��
        if (0 == strncmp(pIfr->ifr_name, "bond", strlen("bond")))// �����bond��
        {
            nRet = getBondNetcardInfo(isock, szBandwidthInfo,pIfr);
            break;
        }
        else // �����eth��
        {
            nRet = getEthNetcardInfo(isock, szBandwidthInfo,pIfr);
            break;
        }
    }

    if(SVS_RESULT_OK != nRet)
    {
        // ��ʼ������Ϣ
        pSystemInfo->m_bandwidthTotal = 0;
        pSystemInfo->m_bandwidthUsed = 0;
        return;
    }

    pSystemInfo->m_bandwidthTotal = szBandwidthInfo.m_bandwidthTotal;
    pSystemInfo->m_bandwidthUsed = 0;

    getBandwidthInfo();
    return ;
}

// ��ȡ���Ѿ�ʹ�ô��
int32_t SVS_Stat_Manager::getBondNetcardInfo(int32_t isock, SystemInfoStat &szBandwidthInfo, struct ifreq* pIfr )const
{
    if(NULL == pIfr)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get bond netcard information failed.The parameter is invalid."
            "pIfr[ox%08x]", pIfr));
        return SVS_RESULT_FAILURE;
    }

    szBandwidthInfo.m_bandwidthTotal = 0;

    ifbond_t stBondStat;
    memset(&stBondStat, 0, sizeof(stBondStat));

    /* ��ip��Ӧ������bonding����ȡ��bonding����Ϣ */
    pIfr->ifr_data = (char *) &stBondStat;
    /* ��ȡbonding��״̬ */
    if (SVS_RESULT_OK != ioctl(isock, SIOCBONDINFOQUERY, pIfr))
    {
        SVS_LOG((SVS_LM_WARNING, "ioctl <SIOCBONDINFOQUERY> on %s failed.",
            pIfr->ifr_name));

        return SVS_RESULT_FAILURE;
    }

    uint32_t i = 0;
    ifslave_t slave_info;
    BondInfo stBondInfo;
    memset(&slave_info, 0, sizeof(slave_info));
    memset(&stBondInfo, 0, sizeof(stBondInfo));

    /* ��bonding��Ӧ������Ŀ stBondStat.num_slaves*/
//    stBondInfo.mode = stBondStat.bond_mode;

    /* ��bonding��Ӧ�İ�ģʽ stBondStat.bond_mode */
//    stBondInfo.slaveNum = stBondStat.num_slaves;

    pIfr->ifr_data = (char *) &slave_info;
    for (i= 0; i < (uint32_t)stBondStat.num_slaves; i++)
    {
        slave_info.slave_id = (int32_t)i;
        if (SVS_RESULT_OK != ioctl(isock, SIOCBONDSLAVEINFOQUERY, pIfr))
        {
            SVS_LOG((SVS_LM_WARNING, "ioctl <SIOCBONDSLAVEINFOQUERY> on [%s][%d] failed.",
                pIfr->ifr_name, i));
            continue;
        }

        // ���������ģʽ,���ȡ�����õ��������Ϣ
        if (BOND_MODE_ACTIVEBACKUP == stBondStat.bond_mode)
        {
            // �ҵ�����󣬿������ѭ����
            if(BOND_STATE_ACTIVE == slave_info.state)
            {
                /* ��ȡbonding��Ӧ��slave��Ϣ */
                strncpy(stBondInfo.slaveName[0], slave_info.slave_name, strlen(slave_info.slave_name));
                stBondInfo.slaveNum = 1;
                break;
            }
        }
        else // ����Ǹ��ؾ��ģʽ
        {

            strncpy(stBondInfo.slaveName[stBondInfo.slaveNum],
                        slave_info.slave_name,
                        strlen(slave_info.slave_name));
            ++stBondInfo.slaveNum;
        }
    }

    // ��ȡbond�ܴ����Ϣ
    ethtool_cmd_t       ecmd;
    ecmd.cmd = ETHTOOL_GSET;
    for(i = 0; i < stBondInfo.slaveNum; i++)
    {
        strcpy(pIfr->ifr_name,stBondInfo.slaveName[i]);
        pIfr->ifr_data = (char *) &ecmd;

        if (SVS_RESULT_OK != ioctl(isock, SIOCETHTOOL, pIfr))
        {
            SVS_LOG((SVS_LM_WARNING, "ioctl <SIOCETHTOOL> on %s failed.", pIfr->ifr_name));

            szBandwidthInfo.m_bandwidthTotal += CAC_RT_Record_Server::instance()->test_bandwidth_total();
        }
        else if (ABNOMAL_VALUE == ecmd.speed)   /* all the bit is 1 */
        {
            SVS_LOG((SVS_LM_WARNING,"The network is down.network name[%s]",pIfr->ifr_name));
        }
        else
        {
            szBandwidthInfo.m_bandwidthTotal += ecmd.speed;
        }
    }

    if(0 == szBandwidthInfo.m_bandwidthTotal)
    {
        SVS_LOG((SVS_LM_WARNING, "Get bond bandwidth is failed."));
        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

int32_t SVS_Stat_Manager::getEthNetcardInfo(int32_t isock, SystemInfoStat &szBandwidthInfo, struct ifreq* pIfr )const
{
    if(NULL == pIfr)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get bond netcard information failed.The parameter is invalid."
            "pIfr[ox%08x]", pIfr));
        return SVS_RESULT_FAILURE;
    }

    szBandwidthInfo.m_bandwidthTotal = 0;

    ethtool_cmd_t stEcmd;          // = { ETHTOOL_GSET };
    memset(&stEcmd, 0, sizeof(ethtool_cmd_t));
    stEcmd.cmd = ETHTOOL_GSET;
    pIfr->ifr_data = (char *) &stEcmd;
    if (SVS_RESULT_OK != ioctl(isock, SIOCETHTOOL, pIfr))
    {
        SVS_LOG((SVS_LM_WARNING, "ioctl <SIOCETHTOOL> on %s failed.", pIfr->ifr_name));
        szBandwidthInfo.m_bandwidthTotal = CAC_RT_Record_Server::instance()->test_bandwidth_total();
        if(0 == szBandwidthInfo.m_bandwidthTotal)
        {
            return SVS_RESULT_FAILURE;
        }

        return SVS_RESULT_OK;
    }
    else if (ABNOMAL_VALUE == stEcmd.speed)    /* all the bit is 1����down�� */
    {
        SVS_LOG((SVS_LM_WARNING,"The network is down.network name[%s]",pIfr->ifr_name));
        return SVS_RESULT_FAILURE;
    }

    // �������ܴ����Ϣ
    szBandwidthInfo.m_bandwidthTotal = stEcmd.speed;

    return SVS_RESULT_OK;
}
void SVS_Stat_Manager::getBandwidthInfo()
{
    /* ���»�ȡ�������ô�� */
    FILE          *fp = NULL;
    fp = fopen("/proc/net/dev", "r");
    if (NULL == fp)
    {
        SVS_LOG((SVS_LM_WARNING,"open /proc/net/dev failed."));
        fp = NULL;
        return ;
    }

    char cline[1024] = {0, };
    char    *pszcp1 = NULL;
    char    *pszcp2 = NULL;
    uint32_t ulrecvBytes = 0;
    uint32_t ulsendBytes = 0;
    uint32_t ultmp = 0;
    SystemInfoStat *pSystemInfo = &m_systemInfoStat[m_systemInfoIndex];
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

        // �����Ҫ�ҵ�eth�������
        if (0 != strcmp(pszcp1, pSystemInfo->m_netcardName))
        {
            continue;
        }

        sscanf(pszcp2,
               "%lu %lu%lu%lu%lu%lu%lu%lu  %lu    %lu%lu%lu%lu%lu%lu%lu",
               &ulrecvBytes,
               &ultmp,
               &ultmp,
               &ultmp,
               &ultmp,
               &ultmp,
               &ultmp,
               &ultmp,
               &ulsendBytes,
               &ultmp,
               &ultmp,
               &ultmp,
               &ultmp,
               &ultmp,
               &ultmp,
               &ultmp);

        if (0 == m_currTxByte)
        {
            m_currTxByte = ulrecvBytes - 1; //first is 1kb/s
        }

        //change unit to kb/s
        pSystemInfo->m_bandwidthUsed = (uint32_t)(((ulrecvBytes - m_currTxByte) * 8)
                                        / (1024 * SVS_STAT_SYSTEM_INFO_INTERVAL));
        if (pSystemInfo->m_bandwidthUsed > (pSystemInfo->m_bandwidthTotal* 1024))
        {
            pSystemInfo->m_bandwidthUsed = 1;
        }

        m_currTxByte = ulrecvBytes;
    }

    (void)fclose(fp);

    return;
}

// ����ͳ����Ϣ,
void SVS_Stat_Manager::getDiskInfo()
{
    SystemInfoStat *pSystemInfo = &m_systemInfoStat[m_systemInfoIndex];

    pSystemInfo->m_diskTotalSize = 0;
    pSystemInfo->m_diskUsableSize = 0;
    DiskStat *pszDiskStat = NULL;
    uint32_t diskNum = m_diskStatVector.size();
    for(uint32_t i = 0; i < diskNum; i++)
    {
        pszDiskStat = &m_diskStatVector[i];
        if(NULL == pszDiskStat->m_pDiskStat)
        {
            continue;
        }

        pSystemInfo->m_diskTotalSize += pszDiskStat->m_pDiskStat->m_totalSize;
        pSystemInfo->m_diskUsableSize += pszDiskStat->m_pDiskStat->m_usableSize;
    }
}

uint32_t SVS_Stat_Manager::GetDiskUsage()
{
    uint64_t DiskTotalSize = 0;
    uint64_t DiskUsableSize = 0;
    uint32_t DiskUsage = 0;
    DiskStat *pszDiskStat = NULL;
    uint32_t diskNum = m_diskStatVector.size();
    for(uint32_t i = 0; i < diskNum; i++)
    {
        pszDiskStat = &m_diskStatVector[i];
        if(NULL == pszDiskStat->m_pDiskStat)
        {
            continue;
        }

        DiskTotalSize += pszDiskStat->m_pDiskStat->m_totalSize;
        DiskUsableSize += pszDiskStat->m_pDiskStat->m_usableSize;
    }

    if(0 == DiskTotalSize)
    {
        DiskUsage = 0;
    }
    else
    {
        DiskUsage = (uint32_t)(RTRECORD_UNIT_PERCENT*(DiskTotalSize - DiskUsableSize)/DiskTotalSize);
    }

    return DiskUsage;
}

int32_t SVS_Stat_Manager::getRecordInputRate()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_PerformanceVarMutex_, -1);

    // ��ȡ��������
    return (int32_t)m_currRecvRateMB;
}

int32_t SVS_Stat_Manager::getRecordOutputRate()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_PerformanceVarMutex_, -1);

    // ��ȡ�������
    return (int32_t)m_currWriteMB;
}

int32_t SVS_Stat_Manager::getMediaBuffAveLevel()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_PerformanceVarMutex_, -1);

    // ��ȡ�������
    return (int32_t)m_fMediaBuffAveLevel;
}


// void SVS_Stat_Manager::getMediaBuffInfo()
int32_t SVS_Stat_Manager::getMediaBuffInfo()
{
    MediaBufferStat *pszMediaBufferStat;
    uint32_t threadNum = m_mediaBufferStatVector.size();
    SystemInfoStat *pSystemInfo = &m_systemInfoStat[m_systemInfoIndex];

    // ��ӡÿ�������̵߳���Ϣ
    float fMediaBuffMaxLevel = 0.0f;
    uint32_t allocTimes = 0;
    uint32_t allocFaileTime = 0;
    uint32_t allocTmpBuffFailTimes = 0;
    uint32_t freeTime = 0;
    float   totalWaterLevel = 0;

    float minBufferLevel = 0;
    for(uint32_t i = 0; i < threadNum; i++)
    {
        pszMediaBufferStat = &m_mediaBufferStatVector[i];

        if(NULL == pszMediaBufferStat->m_pCMediaBufferStat)
        {
            continue;
        }

        // ˮλ���⣬����
        SVS_Media_Buffer_Stat* pCMediaBufferStat = pszMediaBufferStat->m_pCMediaBufferStat ;
        ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, pCMediaBufferStat->m_mutex, -1 );

        // ��ȡ������ͳ�Ƶ���Ϣ,����ʱ���
        allocTimes = pCMediaBufferStat->m_allocTimes;
        pCMediaBufferStat->m_allocTimes = 0;
        allocFaileTime = pCMediaBufferStat->m_allocFailTimes;
        pCMediaBufferStat->m_allocFailTimes = 0;
        allocTmpBuffFailTimes = pCMediaBufferStat->m_allocTmpBuffFailTimes;
        pCMediaBufferStat->m_allocTmpBuffFailTimes = 0;
        freeTime = pCMediaBufferStat->m_FreeTimes;
        pCMediaBufferStat->m_FreeTimes = 0;

        pszMediaBufferStat->m_totalAllocTimes += allocTimes;
        pszMediaBufferStat->m_totalAllocFailTimes += allocFaileTime;
        pszMediaBufferStat->m_totalFreeTimes += freeTime;
        pszMediaBufferStat->m_totalAllocTmpBuffFailTimes += allocTmpBuffFailTimes;

        // ����,��������С���ͷţ���ֵ����ģ�һ���Ƕ��߳�ͬ���������⣬ͳ����ݣ���������
        if(pszMediaBufferStat->m_totalAllocTimes < pszMediaBufferStat->m_totalFreeTimes)
        {
            pszMediaBufferStat->m_totalAllocTimes = pszMediaBufferStat->m_totalFreeTimes;
        }

        // �������Ѿ�ʹ�õ�ˮλ
        float waterLevel = (float)(pszMediaBufferStat->m_totalAllocTimes - pszMediaBufferStat->m_totalFreeTimes)
                           /(pszMediaBufferStat->m_bufferNumber);
        if ( waterLevel > fMediaBuffMaxLevel )
        {
            fMediaBuffMaxLevel = waterLevel; // ý�建��������ˮλ
        }
        waterLevel = RTRECORD_UNIT_PERCENT * waterLevel;

        // ϵͳ�İ���С���Ǹ������߳���
        if((minBufferLevel > waterLevel)
        || (0 == minBufferLevel ))
        {
            minBufferLevel = waterLevel;
        }
        totalWaterLevel += waterLevel;
    }

    pSystemInfo->m_mediaBuffWaterLevel = minBufferLevel;
    m_fMediaBuffMaxLevel = fMediaBuffMaxLevel;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_PerformanceVarMutex_, -1);
        m_fMediaBuffAveLevel = (uint32_t)(totalWaterLevel/threadNum);
    }

    // return ;
    return 0;
}
int32_t SVS_Stat_Manager::mediaBufferStatRegister(SVS_Media_Buffer_Stat* pMediaBufferStat,
                                                    uint32_t bufferSize, uint32_t bufferNum)
{
    if(NULL == pMediaBufferStat)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Media receive thread register to stat manager failed.The parameter is invalid."
            "media receive thread index[%d],pMediaBufferStat[0x%08x].",
            m_mediaBufferStatVector.size(),
            pMediaBufferStat));
        return SVS_ERR_PARAM;
    }

    MediaBufferStat szMediaBufferStat;
    szMediaBufferStat.reset();
    szMediaBufferStat.m_bufferSize = bufferSize;
    szMediaBufferStat.m_bufferNumber = bufferNum;
    szMediaBufferStat.m_pCMediaBufferStat = pMediaBufferStat;

    m_mediaBufferStatVector.push_back(szMediaBufferStat);

    return SVS_RESULT_OK;
}
int32_t SVS_Stat_Manager::printStatInfo(int32_t nStatHandle, const char* strStatBuff, uint32_t writeLen)const
{
    if((0 > nStatHandle) || (NULL == strStatBuff) || (0 == writeLen))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Print stat failed.nStatHandle[%d],strStatBuff[0x%08x],writeLen[%u]",
            nStatHandle,
            strStatBuff,
            writeLen));
        return SVS_ERR_PARAM;
    }

    ssize_t writeRet = ::write(nStatHandle, strStatBuff, writeLen);

    if(writeRet != (ssize_t)writeLen)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Write stat information failed. The stat handle[%d],want to write[%u], real write[%d].",
            nStatHandle,
            writeLen,
            writeRet));

        return SVS_ERR_INVALIDFD;
    }

    return SVS_RESULT_OK;
}

int32_t SVS_Stat_Manager::checkStatLogFile()
{
    if(0 > m_statFileHandle)
    {
        return reopenStatLogFile();
    }

    struct stat st;
    if (0 != ::fstat(m_statFileHandle, &st))
    {
        SVS_LOG((SVS_LM_ERROR, "Fstat stat log file failed."));
        m_statFileHandle = -1;

        return SVS_ERR_INVALIDFD;
    }

    // �ж�ͳ����־�ļ��Ƿ���Ҫ�л�
    if(SVS_STAT_LOG_FILE_MAX_SIZE <= st.st_size)
    {
        SVS_LOG((SVS_LM_INFO, "Change stat log file."));
        (void)::close(m_statFileHandle);
        char strTime[SVS_STR_TIME_MAX_LEN] = {0,};
        (void)SVS_SS_UTILITIES::time2str(strTime, (int32_t)sizeof(strTime), time(NULL));
        char strFileName[MAXNAMELEN + 1] = {0,};
        (void)snprintf(strFileName, sizeof(strFileName), "%s-%s",SVS_STAT_LOG_FILE,strTime);
        (void)ACE_OS::rename(SVS_STAT_LOG_FILE,strFileName);

        return reopenStatLogFile();
    }

    return SVS_RESULT_OK;
}
int32_t SVS_Stat_Manager::reopenStatLogFile()
{
    (void)::close(m_statFileHandle);

    int32_t flags = O_CREAT | O_WRONLY | O_APPEND;

    m_statFileHandle = ACE_OS::open(SVS_STAT_LOG_FILE, flags,ACE_DEFAULT_FILE_PERMS);

    if(0 > m_statFileHandle)
    {
        const int32_t nErrNo = ACE_OS::last_error();
        if(ENOENT == nErrNo)
        {
            if(SVS_RESULT_OK != ACE_OS::mkdir(SVS_STAT_LOG_DIR))
            {
                SVS_LOG((SVS_LM_WARNING, "Make stat log dir failed.dir[%s].",
                    SVS_STAT_LOG_DIR));

                return SVS_RESULT_FAILURE;
            }

            m_statFileHandle = ACE_OS::open(SVS_STAT_LOG_FILE, flags,ACE_DEFAULT_FILE_PERMS);
        }
    }

    if(0 > m_statFileHandle)
    {
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO, "Reopen stat log file successful."));

    return SVS_RESULT_OK;
}

void SVS_Stat_Manager::refreshStatLogLeve(uint32_t newStatLogLevel)
{
    m_nStatLogLevel = newStatLogLevel;
}

// �����ϱ�
int32_t SVS_Stat_Manager::loadReport(const ACE_Message_Block* /*mb*/)
{
    // �ո�������ʱ��ͳ��δ�ﵽ3�����ڣ�δ�ȶ������ϱ�����
    time_t startTimeLen = time(NULL) - m_sysStartTime;
    if((3 * SVS_STAT_SYSTEM_INFO_INTERVAL) >= startTimeLen)
    {
        return SVS_RESULT_OK;
    }

    uint32_t totalCameraNum = 0;
    uint32_t currLoadCameraNum = 0;
    uint32_t systemLoad = 0;
    // ���㸺��
    if(SVS_RESULT_OK != calculateLoad(totalCameraNum,currLoadCameraNum,systemLoad))
    {
        SVS_LOG((SVS_LM_WARNING, "Create load report message failed., load stat failed."));

        return SVS_RESULT_FAILURE;
    }

    // ������Ϣ
    uint32_t msgLen = sizeof(SVS_NRU_LOAD_INFO_REQ_MSG);
    ACE_Message_Block * mb = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, msgLen);

    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create load report message failed.Fail to allocate mb. type[%d], size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
            msgLen ));

        return SVS_ERR_NOMSG;
    }

    // ������Ϣ,ֻ��Ҫ������Ϣ���ͼ��ɣ�����Ϣ���ͱ������д���
    SVS_NRU_LOAD_INFO_REQ_MSG* pSendMsg = (SVS_NRU_LOAD_INFO_REQ_MSG*)(void*)mb->wr_ptr();
    memset(pSendMsg, 0, sizeof(SVS_NRU_LOAD_INFO_REQ_MSG));
    mb->wr_ptr( msgLen );

    // ��װ��Ϣͷ��Ϣ
    FillCommonHeader(   &pSendMsg->Header,
                        SVS_MSG_TYPE_NRU_LOAD_INFO_REQ,
                        CAC_RT_Record_Server::instance()->transaction_no(),
                        (uint16_t)msgLen );

    //memcpy(pSendMsg->ServerID, CAC_RT_Record_Server::instance()->str_id(),sizeof(pSendMsg->ServerID));
    SetNLSDeviceId((uint8_t *)pSendMsg->ServerID,
                sizeof(pSendMsg->ServerID),
                CAC_RT_Record_Server::instance()->str_id());

    NRU_LOAD_INFO *ploadInfo = &pSendMsg->LoadInfo;

    // �ϱ�ƽ��ֵ,�����ϱ����ͳ�Ƶ�ֵ
    SystemInfoStat *pSystemInfo = &m_systemInfoStat[SVS_STAT_EFFECT_INTERVAL_NUM];

    ploadInfo->LoadPercentage   = systemLoad;
    ploadInfo->RecordNumber     = totalCameraNum;
    ploadInfo->RecordingNumber  = currLoadCameraNum;
    ploadInfo->DiskFullStrategy = (uint32_t)CAC_RT_Record_Server::instance()->delete_record_strategy();
    ploadInfo->Bandwidth        = pSystemInfo->m_bandwidthTotal;
    ploadInfo->UsedBandwidth    = pSystemInfo->m_bandwidthUsed / 1024;
    ploadInfo->MemorySize       = pSystemInfo->m_totalMem / 1024;
    ploadInfo->UsedMemorySize   = pSystemInfo->m_usedMem / 1024;
    ploadInfo->UsedCpuLoad      = pSystemInfo->m_usedCpu;
    ploadInfo->IowaitCpuLoad    = (uint32_t)pSystemInfo->m_mediaBuffWaterLevel;
    ploadInfo->UsedStorageSize  = (uint32_t)((m_systemInfoStat[m_systemInfoIndex].m_diskTotalSize - m_systemInfoStat[m_systemInfoIndex].m_diskUsableSize) / 1024);
    ploadInfo->RemainStorageSize= (uint32_t)(m_systemInfoStat[m_systemInfoIndex].m_diskUsableSize / 1024);

    // �������·���������ˣ����ϱ����̿ռ�Ϊ0
    if (!SVS_Disk_Manager::instance().isRecordIndexpathuseable())
    {
        SVS_LOG((SVS_LM_WARNING,
            "Record indexpath is unusable, so remain storage size is 0 in load report."));

        ploadInfo->RemainStorageSize = 0;
    }

    int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( mb );
    if ( 0 != sendResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Add load report message to queue failed."
            "totalCameraNum[%u], currLoadCameraNum[%u], system load[%u]Percent",
            totalCameraNum,
            currLoadCameraNum,
            systemLoad));

        g_p_msg_buffer->free_mb( mb );

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,"Add load report message to queue successfully."
        "system load[%u]Percent,totalCameraNum[%u], currLoadCameraNum[%u];"
        "network bandwidth[%u]Mb, used network bandwidth[%u]Mb;"
        "Memory size[%u]MB, used memory size[%u]MB;"
        "used cpu load[%u]Percent;IO wait load [%u]Percent;"
        "used storage size[%u]MB, remain storage size[%u]MB.",
        ploadInfo->LoadPercentage, ploadInfo->RecordNumber, ploadInfo->RecordingNumber,
        ploadInfo->Bandwidth,ploadInfo->UsedBandwidth,
        ploadInfo->MemorySize,ploadInfo->UsedMemorySize,
        ploadInfo->UsedCpuLoad, ploadInfo->IowaitCpuLoad,
        ploadInfo->UsedStorageSize,ploadInfo->RemainStorageSize));

    return SVS_RESULT_OK;
}

// ���ؼ��㣬�ο����� ���̿ռ䡢CPU���ڴ桢���?ý�建����ˮλ
int32_t SVS_Stat_Manager::calculateLoad(uint32_t &totalCameraNum, uint32_t &currLoadCameraNum,
                                       uint32_t &systemLoad)
{
    // ���������㷨
    uint32_t maxLensNum = CAC_RT_Record_Server::instance()->max_lens();
    uint32_t maxLensPerRaid = CAC_RT_Record_Server::instance()->max_lens_per_raid();
    uint32_t usabelRaid = (uint32_t)SVS_Disk_Manager::instance().getUsableRaidNum();

    totalCameraNum = maxLensPerRaid * usabelRaid;

    // �������raid�������������ϵͳ֧�ֵ����ͷ����ϵͳ���ͷ��
    if(maxLensNum < totalCameraNum)
    {
        totalCameraNum = maxLensNum;
    }

    currLoadCameraNum = CAC_RT_Record_Device_Manager::instance()->get_recording_device_num()
                        + CAC_Bakup_Record_Dev_Manager::instance()->get_bakup_device_num();

    // ��ƽ��ֵ�����������ͳ�Ƶ�ֵ
    SystemInfoStat *pSystemInfo = &m_systemInfoStat[SVS_STAT_EFFECT_INTERVAL_NUM];

    // 1 ����,���û�д��̿ռ���
    SystemInfoStat* pSysInfoStat = &m_systemInfoStat[m_systemInfoIndex];
    float diskLoad = 0;
    if (0 != pSysInfoStat->m_diskTotalSize )
    {
        diskLoad = ((float)( pSysInfoStat->m_diskTotalSize - pSysInfoStat->m_diskUsableSize)
                            / pSysInfoStat->m_diskTotalSize) * SVS_WEIGHT_DISK;
    }

    // 2. ���û�д�?��������
    float bandwidth = 0;
    if (0 != pSystemInfo->m_bandwidthTotal)
    {
        bandwidth = ((float)pSystemInfo->m_bandwidthUsed / pSystemInfo->m_bandwidthTotal) * SVS_WEIGHT_BANDWIGTH;
    }

    // 3. ������ˮλ

    float buffLoad = (pSystemInfo->m_mediaBuffWaterLevel / RTRECORD_UNIT_PERCENT) * SVS_WEIGHT_MEDIA_BUFFER;

    // 4. CPU
    float cpuLoad = ((float)pSystemInfo->m_usedCpu / RTRECORD_UNIT_PERCENT) * SVS_WEIGHT_CPU;

    // 5. �ڴ�
    float memLoad = 0;
    if(pSystemInfo->m_totalMem > 0)
    {
        memLoad = ((float)pSystemInfo->m_usedMem / pSystemInfo->m_totalMem) * SVS_WEIGHT_MEMORY;
    }

    systemLoad = (uint32_t)(diskLoad + bandwidth + buffLoad + cpuLoad + memLoad);

    return SVS_RESULT_OK;
}

// ��ȡ¼����������
int32_t SVS_Stat_Manager::sampleRecordQuality(const char* strDeviceId,
                            SVS_RECORD_QUALITY_REPORT& szReport)
{
    if(NULL == strDeviceId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Sample record quality failed.The parameter is invalid."
            "strDeviceId[0x%08x].",
            strDeviceId));

        return SVS_RESULT_FAILURE;
    }


    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_MapMutex_, -1);
    CameraStat * pCameraStat = NULL;
    if (0 != m_cameraMap.find(strDeviceId, pCameraStat))
    {
        return SVS_RESULT_FAILURE;
    }

    // ���¼�����Ϊ��˵����˵����ͷû����¼�񣬲ɼ����أ�ʧ�ܡ�
    if (NULL == pCameraStat->m_pDeviceStat)
    {
        return SVS_RESULT_FAILURE;
    }

    // ���¼������������Ϊ�գ���ͷû����¼�񣬲ɼ�ʧ�ܷ���
    if ((NULL == pCameraStat->m_pIngressStat) && (NULL == pCameraStat->m_pEgressStat))
    {
        return SVS_RESULT_FAILURE;
    }

    // ���¼������ͳ�ƶ���Ϊ�գ���ɼ��������¼��������Ϊ��
    if(NULL == pCameraStat->m_pIngressStat)
    {
        szReport.FRAME_LOSE_RATE = 0;
        szReport.MAX_FRAME_SIZE = 0;
        szReport.MIN_FRAME_SIZE = 0;
        szReport.TOTAL_RECV_SIZE = 0;
        szReport.TOTAL_FRAME_RECV = 0;
        szReport.TOTAL_IFRAME_RECV = 0;
    }
    else
    {
        // ��ǰֻ��¼��ʧ��֡��������㶪֡��
        szReport.FRAME_LOSE_RATE = pCameraStat->m_pIngressStat->m_sampleLostFrameNum;

        // ֡�ʺ�������������,��ǰֻȥ����Ҫ�����ֵ
        // ��λkbit
        szReport.MAX_FRAME_SIZE =
            (uint32_t)((pCameraStat->m_pIngressStat->m_sampleMaxFrameSize * RTRECORD_UNIT_BYTE) / RTRECORD_UNIT_K);

        // ��λkbit
        szReport.MIN_FRAME_SIZE =
            (uint32_t)((pCameraStat->m_pIngressStat->m_sampleMinFrameSize * RTRECORD_UNIT_BYTE) / RTRECORD_UNIT_K);

        // ��λKB
        szReport.TOTAL_RECV_SIZE =
            (uint32_t)((pCameraStat->m_pIngressStat->m_sampleRecvByteNum) / (uint32_t)RTRECORD_UNIT_K);

        // ��λ��
        szReport.TOTAL_FRAME_RECV = pCameraStat->m_pIngressStat->m_sampleRecvFrameNum;

        // ��λ��
        szReport.TOTAL_IFRAME_RECV = pCameraStat->m_pIngressStat->m_sampleRecvIFrameNum;

        // �ɼ���Ϣ��λ
        pCameraStat->m_pIngressStat->resetSampleInfo();
    }

    // ���¼�����ͳ�ƶ���Ϊ�գ���ɼ��������¼�������Ϊ��
    if (NULL == pCameraStat->m_pEgressStat)
    {
        szReport.TOTAL_RECORD_SIZE = 0;
        szReport.TOTAL_FRAME_RECORD = 0;
        szReport.TOTAL_IFRAME_RECORD = 0;
        szReport.RECORD_FILE_SIZE = 0;
    }
    else
    {
        // ��λKB
        szReport.TOTAL_RECORD_SIZE =
            (uint32_t)(pCameraStat->m_pEgressStat->m_sampleRecordSize / RTRECORD_UNIT_K);

        // ��λ��
        szReport.TOTAL_FRAME_RECORD = pCameraStat->m_pEgressStat->m_sampleRecordFrameNum;

        // ��λ��
        szReport.TOTAL_IFRAME_RECORD = pCameraStat->m_pEgressStat->m_sampleRecordIFrameNum;

        // ��λKB
        szReport.RECORD_FILE_SIZE =
            (uint32_t)(pCameraStat->m_pEgressStat->m_sampleRecordFileSize / RTRECORD_UNIT_K);

        memcpy(szReport.RECORD_FILE_PATH, pCameraStat->m_pEgressStat->m_fileName, sizeof(szReport.RECORD_FILE_PATH));

        // �ɼ���Ϣ��λ
        pCameraStat->m_pEgressStat->resetSampleInfo();
    }
    return SVS_RESULT_OK;
}

int32_t SVS_Stat_Manager::resetSampleInfo(const char* strCameraId)
{
    if(NULL == strCameraId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Reset sample information failed.The parameter is invalid."
            "strCameraId[0x%08x].",
            strCameraId));

        return SVS_RESULT_FAILURE;
    }


    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_MapMutex_, -1);
    CameraStat * pCameraStat = NULL;
    if (0 != m_cameraMap.find(strCameraId, pCameraStat))
    {
        return SVS_RESULT_OK;
    }

    // ���¼�����Ϊ��˵����˵����ͷû����¼�����óɹ���
    if (NULL == pCameraStat->m_pDeviceStat)
    {
        return SVS_RESULT_OK;
    }

    // ���¼������������Ϊ�գ���ͷû����¼�����óɹ���
    if (NULL != pCameraStat->m_pIngressStat)
    {
        pCameraStat->m_pIngressStat->resetSampleInfo();
    }
    // ���¼������������Ϊ�գ���ͷû����¼�����óɹ���
    if (NULL != pCameraStat->m_pEgressStat)
    {
        pCameraStat->m_pEgressStat->resetSampleInfo();
    }

    SVS_LOG((SVS_LM_INFO,
        "Reset sample information success.CameraId[%s].",
        strCameraId));

    return SVS_RESULT_OK;
}


int32_t SVS_Stat_Manager::reportRecordStatus(const ACE_Message_Block* /*mb*/)
{
    SVS_TRACE();
   ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_MapMutex_, -1);
    CAMERA_STAT_MAP_ITER iter(m_cameraMap);
    CAMERA_STAT_MAP_ENTRY *pEntry = NULL;
    CameraStat * pCameraStat = NULL;
    ACE_Message_Block *mb = NULL;
    RECORD_STATUS_INFO * pRecordStatusInfo = NULL;
    MRU_RECORD_STATUS_NOTIFY_REQ* pMsg = NULL;
    uint32_t ulDevNum = 0;
    uint32_t ulThisLoopCtrl = 0;
    uint32_t ulThisLoopDevNum = 0;
    uint32_t ulTotalDevNum = m_cameraMap.current_size();
    SVS_LOG((SVS_LM_INFO,
        "Begin to report record media status. camera number[%u]",
        ulTotalDevNum));
    time_t currTime = time(NULL);
    // �������о�ͷ��ͳ����Ϣ
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        // �����ڴ滺��飬ÿ�η������Ϊ1000����ͷ����Ϣ
        if(NULL == mb)
        {
            ulThisLoopCtrl = 0;
            ulThisLoopDevNum = ulTotalDevNum - ulDevNum;

            // ���ʣ�ྵͷ�����1000������ֻ����1000����ͷ����Ϣ
            if (REPORT_RECORD_STATUS_MAX_DEV_NUM < ulThisLoopDevNum)
            {
                ulThisLoopDevNum = REPORT_RECORD_STATUS_MAX_DEV_NUM;
            }

            // ���뷢����Ϣý���
            uint32_t msgLen = sizeof(MRU_RECORD_STATUS_NOTIFY_REQ)
                                   + (ulThisLoopDevNum - 1) * sizeof(RECORD_STATUS_INFO);

            mb = g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, msgLen );

            if (NULL == mb)
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Report record media status failed as fail to allocate message block."
                    "msg type[%d], block size[%d].thisLoopDevNum[%u], totalDevNum[%u].",
                    CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
                    msgLen,
                    ulThisLoopDevNum,
                    ulTotalDevNum));

                return SVS_RESULT_FAILURE;
            }

            pMsg = (MRU_RECORD_STATUS_NOTIFY_REQ*)(void*)mb->wr_ptr();
            mb->wr_ptr(msgLen);

            // ��װ��Ϣͷ
            FillCommonHeader(&pMsg->Header,
                             SVS_MSG_TYPE_MRU_RECORD_STATUS_NOTIFY_REQ,
                             CAC_RT_Record_Server::instance()->transaction_no(),
                             (uint16_t)msgLen);

            pMsg->RecordStatusCount = ulThisLoopDevNum;
        }

        if(NULL == pMsg)
        {
            SVS_LOG((SVS_LM_ERROR, "Report record media status failed, pMsg[0x%08x].", pMsg ));

            return SVS_RESULT_FAILURE;
        }

        // ͳ�Ƶ�ǰ��ͷ����Ϣ
        pCameraStat = pEntry->int_id_;
        pRecordStatusInfo = &pMsg->RecordStatusInfo[ulThisLoopCtrl];
        getCameraRecordStatus(pCameraStat, pRecordStatusInfo, currTime);

        SVS_LOG((SVS_LM_DEBUG,
            "Record media status info:cameraID[%s], streamId[%u],"
            " recordTaskStatus[0x%04x], recordStatus[0x%04x].",
            pRecordStatusInfo->DeviceID,
            pRecordStatusInfo->StreamID,
            pRecordStatusInfo->PlatRecordTaskStatus,
            pRecordStatusInfo->PlatRecordStatus));

        // ������Ϣװ�������Ϣ���ͳ�ȥ������mb�ÿ�
        ++ulThisLoopCtrl;
        if(ulThisLoopCtrl >= ulThisLoopDevNum)
        {
            //����״̬�ϱ���Ϣ
            int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( mb);

            if ( 0 != sendResult )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Add report record media status message to queue failed, "
                    "return code[%d].",
                    sendResult ));
                g_p_msg_buffer->free_mb( mb );
            }

            mb = NULL;
        }

        // �仯���Ʊ���
        ++ulDevNum;
    }

    //����״̬�ϱ���Ϣ
    if(NULL != mb)
    {
        int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( mb);
        if ( 0 != sendResult )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Add report record media status message to queue failed, "
                "return code[%d].",
                sendResult ));
            g_p_msg_buffer->free_mb( mb );
        }

        mb = NULL;
    }

    SVS_LOG((SVS_LM_INFO, "Report record media status succeccful.camera number[%u].", ulTotalDevNum ));
    return SVS_RESULT_OK;
}

void SVS_Stat_Manager::getCameraRecordStatus(
                        CameraStat * pCameraStat,
                        RECORD_STATUS_INFO * pRecordStatusInfo,
                        const time_t currTime)
{
    SVS_TRACE();
    if((NULL == pCameraStat) || (NULL == pRecordStatusInfo))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get camera record status failed as the parameter is invalid."
            "pCameraStat[0x%08x],pRecordStatusInfo[0x%08x].",
            pCameraStat,
            pRecordStatusInfo));

        return;
    }

    memset(pRecordStatusInfo, 0, sizeof(RECORD_STATUS_INFO));
    memcpy(pRecordStatusInfo->DeviceID, pCameraStat->m_strCameraId, sizeof(pRecordStatusInfo->DeviceID));

    // ���inform��stat����Ϊ�գ�˵����û��ע����������Ǿ�ͷ�Ѿ����ӹ��ˣ�"¼����"+"��"
    if (NULL ==  pCameraStat->m_pInformStat)
    {
        pRecordStatusInfo->StreamID = 0;
        pRecordStatusInfo->PlatRecordTaskStatus = PLAT_RECORD_TASK_STATUS_RECORDING;
        pRecordStatusInfo->PlatRecordStatus = PALT_RECORD_STATUS_NORMAL;

        return ;
    }

    // ���ӹ�ʱ��С���������,��״̬Ϊ"¼����"+"�쳣"
    if(RECORD_MEDIA_STATUS_READY_TIME >= (currTime - pCameraStat->m_pInformStat->m_takeoverTime))
    {
        pRecordStatusInfo->StreamID = 0;
        pRecordStatusInfo->PlatRecordTaskStatus = PLAT_RECORD_TASK_STATUS_RECORDING;
        pRecordStatusInfo->PlatRecordStatus = PALT_RECORD_STATUS_ABNORMAL;

        return ;
    }

    SVS_Device_Stat* pDeviceStat = pCameraStat->m_pDeviceStat;
    // �����inform,û��device,˵���ӹܵľ�ͷ��ʱû��¼������"��¼��"+"��"
    if(NULL == pDeviceStat)
    {
        pRecordStatusInfo->StreamID = 0;
        pRecordStatusInfo->PlatRecordTaskStatus = PLAT_RECORD_TASK_STATUS_UNRECORDED;
        pRecordStatusInfo->PlatRecordStatus = PALT_RECORD_STATUS_NORMAL;

        return;
    }

    // ���¼������ǽ�ֹ¼����ԣ���״̬Ϊ"��¼��"+"��"
    if (CAC_Realtime_Record_Device::RECORD_TYPE_PROBID == pDeviceStat->m_recordStrategyType)
    {
        pRecordStatusInfo->StreamID = pDeviceStat->m_streamId;
        pRecordStatusInfo->PlatRecordTaskStatus = PLAT_RECORD_TASK_STATUS_UNRECORDED;
        pRecordStatusInfo->PlatRecordStatus = PALT_RECORD_STATUS_NORMAL;

        return;
    }

    // �������ոտ�ʼ3*3s֮�ڣ�¼��״̬Ϊ "¼����"+"��"
    if(RECORD_MEDIA_STATUS_READY_TIME >= (currTime - pDeviceStat->m_recordTaskStartTime))
    {
        pRecordStatusInfo->StreamID = pDeviceStat->m_streamId;
        pRecordStatusInfo->PlatRecordTaskStatus = PLAT_RECORD_TASK_STATUS_RECORDING;
        pRecordStatusInfo->PlatRecordStatus = PALT_RECORD_STATUS_NORMAL;

        return;
    }

    // �����Ԥ¼(�����������¼�¼������Ԥ¼)����"��¼��"+"��"
    if ((CAC_Realtime_Record_Device::RECORD_TYPE_EVENT == pDeviceStat->m_recordStrategyType)
        && (0 == pDeviceStat->m_eventTotalNum))
    {
        pRecordStatusInfo->StreamID = pDeviceStat->m_streamId;
        pRecordStatusInfo->PlatRecordTaskStatus = PLAT_RECORD_TASK_STATUS_UNRECORDED;
        pRecordStatusInfo->PlatRecordStatus = PALT_RECORD_STATUS_NORMAL;

        return;
    }

    // ������
    if(CAC_Realtime_Record_Device::RECORD_STATE_RECEIVING_FILE == pDeviceStat->m_recordStatus)
    {
        // ����ǵ�һ�ν�����˵�������һ��������ֻҪ�������״̬���������
        if (1 >= pDeviceStat->m_startRecvStreamTimes)
        {
            pRecordStatusInfo->StreamID = pDeviceStat->m_streamId;
            pRecordStatusInfo->PlatRecordTaskStatus = PLAT_RECORD_TASK_STATUS_RECORDING;
            pRecordStatusInfo->PlatRecordStatus = PALT_RECORD_STATUS_NORMAL;

            return;
        }

        // ����ǽ���״̬,�ҽ������Ŀ�ʼʱ��С��3*3s����"¼����"+"�쳣"
        if(RECORD_MEDIA_STATUS_READY_TIME >= (currTime - pDeviceStat->m_recordStartTime))
        {
            pRecordStatusInfo->StreamID = pDeviceStat->m_streamId;
            pRecordStatusInfo->PlatRecordTaskStatus = PLAT_RECORD_TASK_STATUS_RECORDING;
            pRecordStatusInfo->PlatRecordStatus = PALT_RECORD_STATUS_ABNORMAL;

            return;
        }

        // ����ǽ���״̬,�ҽ������Ŀ�ʼʱ��С��3*3s����"¼����"+"��"
        pRecordStatusInfo->StreamID = pDeviceStat->m_streamId;
        pRecordStatusInfo->PlatRecordTaskStatus = PLAT_RECORD_TASK_STATUS_RECORDING;
        pRecordStatusInfo->PlatRecordStatus = PALT_RECORD_STATUS_NORMAL;

        return;
    }

    // ����Ϊ"¼����"+"�쳣"
    pRecordStatusInfo->StreamID = pDeviceStat->m_streamId;
    pRecordStatusInfo->PlatRecordTaskStatus = PLAT_RECORD_TASK_STATUS_RECORDING;
    pRecordStatusInfo->PlatRecordStatus = PALT_RECORD_STATUS_ABNORMAL;

    return ;
}

//==========================SVS_Stat_Manager END================================


//==========================SVS_Inform_Stat BEGIN==============================
// ¼��֪ͨ����Ϣͳ��
SVS_Inform_Stat::SVS_Inform_Stat()
{
    m_strategyStartTime = 0;
    m_strategyEndTime = 0;
    m_isEventEffective = false;
    m_strategyType = 0;
    memset(m_strCameraId, 0, sizeof(m_strCameraId));
}

SVS_Inform_Stat::~SVS_Inform_Stat()
{
    try
    {
        (void)SVS_Stat_Manager::instance().cameraUnregister(m_strCameraId,
                                        SVS_Stat_Manager::CAMERA_OBJECT_INFORM);
    }
    catch(...)
    {
    }
}

void SVS_Inform_Stat::init(const char * strCameraId)
{
    if(NULL != strCameraId)
    {
        //(void)memcpy(m_strCameraId, strCameraId, SVS_DEVICEID_LEN);
        SetLocalizeDeviceId((uint8_t *)m_strCameraId,
                        sizeof(m_strCameraId),
                        strCameraId);

        (void)SVS_Stat_Manager::instance().cameraRegister(m_strCameraId,
                                        SVS_Stat_Manager::CAMERA_OBJECT_INFORM,
                                        (void*)this);

    }

    m_takeoverTime = time(NULL);
}

void SVS_Inform_Stat::updateStrategyTime(time_t startTime, time_t endTime,
                                        int32_t strategyType, bool isEventEffective)
{
    m_strategyStartTime = startTime;
    m_strategyEndTime = endTime;
    m_isEventEffective = isEventEffective;
    m_strategyType = strategyType;
}
//==========================SVS_Inform_Stat END================================

//==========================SVS_Device_Stat BEGIN===============================
// ¼������ͳ����Ϣ
SVS_Device_Stat::SVS_Device_Stat()
{
    m_recordStartTime = 0;
    m_eventHappenTime = 0;
    m_eventOverTime = 0;
    m_eventTotalNum = 0;
    memset(m_strCameraId, 0, sizeof(m_strCameraId));
    m_recordStrategyType = 0;
    m_recordStatus = 0;
    m_recordTaskStartTime = 0;
    m_streamId = 0;
    m_startRecvStreamTimes = 0;
}

SVS_Device_Stat::~SVS_Device_Stat()
{
    try
    {
        (void)SVS_Stat_Manager::instance().cameraUnregister(m_strCameraId,
                                        SVS_Stat_Manager::CAMERA_OBJECT_DEVICE);
    }
    catch(...)
    {
    }
}

void SVS_Device_Stat::init(const char * strCameraId)
{
    if(NULL != strCameraId)
    {
        SetLocalizeDeviceId((uint8_t *)m_strCameraId,
                        sizeof(m_strCameraId),
                        strCameraId);

        (void)SVS_Stat_Manager::instance().cameraRegister(m_strCameraId,
                                        SVS_Stat_Manager::CAMERA_OBJECT_DEVICE,
                                        (void*)this);

    }
}
// �����¼���Ϣ
void SVS_Device_Stat::updateEventInfo(time_t startTime,time_t endTime, uint32_t eventTotalNum)
{
    m_eventHappenTime = startTime;
    m_eventOverTime = endTime;
    m_eventTotalNum = eventTotalNum ;
    return ;
}

// ¼����ʼ�¼�����ʼ����ʱ��
void SVS_Device_Stat::setRecordStartTime(time_t recordStartTime)
{
    m_recordStartTime = recordStartTime;
    ++m_startRecvStreamTimes;
}

// ¼����Ϣˢ��
void SVS_Device_Stat::updateRecordInfo(int32_t nRecordStrategyType, int32_t nRecordStatus, uint32_t nStreamId)
{
    m_recordStrategyType = nRecordStrategyType;
    m_recordStatus = nRecordStatus;
    m_streamId = nStreamId;
}

void SVS_Device_Stat::setRecordTaskStartTime(time_t ulRecordTaskStartTime)
{
    m_recordTaskStartTime = ulRecordTaskStartTime;

    // ���ԭ�����ڽ�����״̬�������������������0
    if (CAC_Realtime_Record_Device::RECORD_STATE_RECEIVING_FILE != m_recordStatus)
    {
        m_startRecvStreamTimes = 0;
    }
}


//==========================SVS_Device_Stat END================================

//==========================SVS_Ingress_Stat BEGIN==============================
// ¼�������
SVS_Ingress_Stat::SVS_Ingress_Stat()
{
    m_lastRecvPachetTime    = 0;
    m_recvByteNum           = 0;
    m_recvFrameNum          = 0;
    m_lostFrameNum          = 0;
    m_lostPacketNum         = 0;
    m_errFrameNum           = 0;
    memset(m_strCameraId, 0, sizeof(m_strCameraId));
    resetSampleInfo();
}

SVS_Ingress_Stat::~SVS_Ingress_Stat()
{
    try
    {
        (void)SVS_Stat_Manager::instance().cameraUnregister(m_strCameraId,
                                        SVS_Stat_Manager::CAMERA_OBJECT_INGRESS);
    }
    catch(...)
    {
    }
}

void SVS_Ingress_Stat::init(const char * strCameraId)
{
    if(NULL != strCameraId)
    {
        //(void)memcpy(m_strCameraId, strCameraId, SVS_DEVICEID_LEN);
        SetLocalizeDeviceId((uint8_t *)m_strCameraId,
                        sizeof(m_strCameraId),
                        strCameraId);

        (void)SVS_Stat_Manager::instance().cameraRegister(m_strCameraId,
                                        SVS_Stat_Manager::CAMERA_OBJECT_INGRESS,
                                        (void*)this);

    }
}

void SVS_Ingress_Stat::updateLastRecvPacketTime(time_t lastTime)
{
    m_lastRecvPachetTime = lastTime;
}

void SVS_Ingress_Stat::addRecvByteNum(uint32_t newRecvTyteNum)
{
    m_recvByteNum += newRecvTyteNum;
    m_sampleRecvByteNum += newRecvTyteNum;
}
void SVS_Ingress_Stat::addRecvFramNum(uint32_t newRecvFrameNum, uint8_t keyFrame)
{
    m_recvFrameNum += newRecvFrameNum;
    m_sampleRecvFrameNum += newRecvFrameNum;
    if(1 == keyFrame)
    {
        m_sampleRecvIFrameNum += newRecvFrameNum;
    }
}
void SVS_Ingress_Stat::addLostFramNum(uint32_t newLostFramNum)
{
    m_lostFrameNum += newLostFramNum;
    m_sampleLostFrameNum += newLostFramNum;
}
void SVS_Ingress_Stat::addErrFrameNum(uint32_t newErrFramNum)
{
    m_errFrameNum += newErrFramNum;
}
void SVS_Ingress_Stat::addLostPacketNum(uint32_t newLostPacketNum)
{
    m_lostPacketNum += newLostPacketNum;
}

void SVS_Ingress_Stat::updateFrameSize(uint32_t newFrameSize)
{
    if(newFrameSize > m_sampleMaxFrameSize)
    {
        m_sampleMaxFrameSize = newFrameSize;
    }

    if((newFrameSize < m_sampleMinFrameSize) || (0 == m_sampleMinFrameSize))
    {
        m_sampleMinFrameSize = newFrameSize;
    }
}

// ���ñ��������ڵĲ�����Ϣ
void SVS_Ingress_Stat::resetSampleInfo()
{
    m_sampleRecvByteNum     = 0;
    m_sampleRecvFrameNum    = 0;
    m_sampleLostFrameNum    = 0;
    m_sampleMaxFrameSize    = 0;
    m_sampleMinFrameSize    = 0;
    m_sampleRecvIFrameNum   = 0;
}
//==========================SVS_Ingress_Stat END===============================

//==========================SVS_Egress_Stat BEGIN===============================
// �������Ϣ
SVS_Egress_Stat::SVS_Egress_Stat()
{
    (void)memset(m_fileName, 0, sizeof(m_fileName));
    m_indexStartTime    = 0;
    m_indexEndTime      = 0;
    m_writeByteNum      = 0;
    m_writeFrameNum = 0;
    (void)memset(m_strCameraId, 0, sizeof(m_strCameraId));
    resetSampleInfo();
    m_sampleRecordFileSize  = 0;

}

SVS_Egress_Stat::~SVS_Egress_Stat()
{
    try
    {
        (void)SVS_Stat_Manager::instance().cameraUnregister(m_strCameraId,
                                        SVS_Stat_Manager::CAMERA_OBJECT_EGRESS);
    }
    catch(...)
    {
    }
}

void SVS_Egress_Stat::init(const char * strCameraId)
{
    if(NULL != strCameraId)
    {
        //(void)memcpy(m_strCameraId, strCameraId, SVS_DEVICEID_LEN);
        SetLocalizeDeviceId((uint8_t *)m_strCameraId,
                        sizeof(m_strCameraId),
                        strCameraId);

        (void)SVS_Stat_Manager::instance().cameraRegister(m_strCameraId,
                                        SVS_Stat_Manager::CAMERA_OBJECT_EGRESS,
                                        (void*)this);

    }
}
void SVS_Egress_Stat::addWriteByteNum(uint32_t newWriteByteNum)
{
    m_writeByteNum += newWriteByteNum;
    m_sampleRecordSize += newWriteByteNum;
}

void SVS_Egress_Stat::addWriteFrameNum(uint32_t newWriteFrameNum)
{
    m_writeFrameNum += newWriteFrameNum;
    m_sampleRecordFrameNum += newWriteFrameNum;
}

void SVS_Egress_Stat::addWriteIFrameNum(uint32_t newWriteIFrameNum)
{
    m_sampleRecordIFrameNum += newWriteIFrameNum;
}

void SVS_Egress_Stat::updateIndexInfo(time_t startTime, time_t endTime)
{
    m_indexStartTime = startTime;
    m_indexEndTime = endTime;
}

void SVS_Egress_Stat::updateFileName(const char* strFileName)
{
    if(NULL == strFileName)
    {
        return;
    }

    (void)snprintf(m_fileName, sizeof(m_fileName), "%s", strFileName);
}

void SVS_Egress_Stat::updateRecordFileSize(uint64_t newRecordFileSize)
{
    m_sampleRecordFileSize = newRecordFileSize;
}
void SVS_Egress_Stat::resetSampleInfo()
{
    m_sampleRecordSize      = 0;
    m_sampleRecordFrameNum  = 0;
    m_sampleRecordIFrameNum = 0;
}
//==========================SVS_Egress_Stat END================================

//==========================SVS_Media_Output_Stat BEGIN=========================
SVS_Media_Output_Stat::SVS_Media_Output_Stat()
{
    m_ulIoScheduleTimes = 0;
    m_writeByteNum = 0;
    m_writeFrameNum = 0;
    m_IoScheduleFailTimes = 0;
    m_threadIndex = 0;
}
SVS_Media_Output_Stat::SVS_Media_Output_Stat(uint32_t threadIndex)
{
    m_ulIoScheduleTimes = 0;
    m_writeByteNum = 0;
    m_writeFrameNum = 0;
    m_IoScheduleFailTimes = 0;
    // ע��
    m_threadIndex = (uint32_t)SVS_Stat_Manager::instance().mediaOutputThreadRegister(threadIndex,this);
}

SVS_Media_Output_Stat::~SVS_Media_Output_Stat()
{
    // ȥע��,�⣬��ϵͳ�˳�ʱ���̲߳��˳�
    m_ulIoScheduleTimes = 0;
    m_writeByteNum = 0;
    m_writeFrameNum = 0;
    m_IoScheduleFailTimes = 0;
}

void SVS_Media_Output_Stat::addIoScheduleTimes(uint32_t IoScheduleTimes,
                                              uint32_t newWriteByte,
                                              uint32_t newWriteFrameNum)
{
    m_ulIoScheduleTimes += IoScheduleTimes;
    m_writeByteNum += (uint64_t)newWriteByte;
    m_writeFrameNum += (uint64_t)newWriteFrameNum;
}

void SVS_Media_Output_Stat::addIoScheduleFailTimes(uint32_t newIoFailScheduleTimes)
{
    m_IoScheduleFailTimes += newIoFailScheduleTimes;
}
//==========================SVS_Media_Output_Stat END==========================


//==========================SVS_Media_Output_Stat BEGIN=========================
SVS_Signal_Message_Stat::SVS_Signal_Message_Stat()
{
    m_messageCount = 0;
    m_disconnStoreCtrlTimes = 0;
    m_connStoreCtrlFailTimes = 0;
    m_hearbeatTimeoutTimes = 0;
    m_lastDisconnStoreCtrlTime = 0;
    m_lastConnStoreCtrlTime = 0;

    (void)SVS_Stat_Manager::instance().signalMsgStatRegister(this);
}
SVS_Signal_Message_Stat::~SVS_Signal_Message_Stat()
{
    m_messageCount = 0;
    m_disconnStoreCtrlTimes = 0;
    m_connStoreCtrlFailTimes = 0;
    m_hearbeatTimeoutTimes = 0;
    m_lastDisconnStoreCtrlTime = 0;
    m_lastConnStoreCtrlTime = 0;
}
void SVS_Signal_Message_Stat::addSignalMessage(uint32_t messageCount)
{
    m_messageCount += messageCount;
}

void SVS_Signal_Message_Stat::addDisconnStoreCtrlTimes(uint32_t newDisconnTimes)
{
    m_disconnStoreCtrlTimes += newDisconnTimes;
}

void SVS_Signal_Message_Stat::addConnStoreCtrlFailTimes(uint32_t newConnFailTimes)
{
    m_connStoreCtrlFailTimes += newConnFailTimes;
}
void SVS_Signal_Message_Stat::addHeartbeatTimeoutTimes(uint32_t newTimeoutTimes)
{
    m_hearbeatTimeoutTimes += newTimeoutTimes;
}
void SVS_Signal_Message_Stat::updateConnStoreCtrlTime(time_t lastConntime)
{
    m_lastConnStoreCtrlTime = lastConntime;
}
void SVS_Signal_Message_Stat::updateDisconnStoreCtrlTime(time_t lastDisconntime)
{
    m_lastDisconnStoreCtrlTime = lastDisconntime;
}
//==========================SVS_Media_Output_Stat End=========================

//==========================SVS_Media_Output_Stat BEGIN=========================
SVS_Media_Buffer_Stat::SVS_Media_Buffer_Stat()
{
    m_allocTimes    = 0;
    m_FreeTimes     = 0;
    m_allocFailTimes = 0;
    m_allocTmpBuffFailTimes = 0;
}
SVS_Media_Buffer_Stat::~SVS_Media_Buffer_Stat()
{
    m_allocTimes    = 0;
    m_FreeTimes     = 0;
    m_allocFailTimes = 0;
    m_allocTmpBuffFailTimes = 0;
}
void SVS_Media_Buffer_Stat::init(uint32_t BufferLen, uint32_t BufferNum )
{
    (void)SVS_Stat_Manager::instance().mediaBufferStatRegister(this, BufferLen, BufferNum);
}

void SVS_Media_Buffer_Stat::addAllocaTimes(uint32_t newAllocTimes)
{
    ACE_GUARD( ACE_Recursive_Thread_Mutex, pObj, m_mutex);
    m_allocTimes += newAllocTimes;
}

void SVS_Media_Buffer_Stat::addAllcoaFailTimes(uint32_t newAllocFailTimes)
{
    m_allocFailTimes += newAllocFailTimes;
}

void SVS_Media_Buffer_Stat::addAlloctTmpBuffFailTimes(uint32_t newAllocFailTimes)
{
    m_allocTmpBuffFailTimes += newAllocFailTimes;
}

void SVS_Media_Buffer_Stat::addFreeTimes(uint32_t newFreeTimes)
{
    ACE_GUARD( ACE_Recursive_Thread_Mutex, pObj, m_mutex);
    m_FreeTimes += newFreeTimes;
}
//==========================SVS_Media_Output_Stat end=========================

//==========================SVS_Disk_Stat BEGIN=========================
SVS_Disk_Stat::SVS_Disk_Stat()
{
    m_totalSize = 0;
    m_totalSize = 0;
    (void)SVS_Stat_Manager::instance().diskStatRegister(this);
}

SVS_Disk_Stat::~SVS_Disk_Stat()
{
    m_totalSize = 0;
    m_totalSize = 0;
}

void SVS_Disk_Stat::updateDiskSize(uint64_t totalSize, uint64_t usableSize)
{
    m_totalSize = totalSize;
    m_usableSize = usableSize;
}

//==========================SVS_Disk_Stat END=========================


