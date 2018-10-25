/******************************************************************************

                  版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文 件 名   : SVS_Record_Trace.cpp
  版 本 号   : 初稿
  生成日期   : 2010年3月10日
  最近修改   :
  功能描述   : 录像质量跟踪类。根据录像跟踪策略，周期性采集录像质量，并周期性上报
               到控制台。
  函数列表   :
  修改历史   :
  1.日    期   : 2010年3月10日
    修改内容   : 创建文件

******************************************************************************/
#include "svs_timer.h"
#include "svs_utility.h"
#include "svs_log_msg.h"
#include "svs_auto_reference_ptr.h"
#include "svs_rt_record_common.h"
#include "svs_stat_manager.h"
#include "svs_console_server.h"
#include "svs_record_trace.h"
#include "svs_real_record_server.h"

typedef CAC_Auto_Reference_Ptr <SVS_Record_Trace_Manager, SVS_Record_Trace_Strategy *>
   SVS_TRACE_DEVICE_AUTO_PTR;
//=================SVS_Record_Trace_Strategy BEGIN==============================
SVS_Record_Trace_Strategy::SVS_Record_Trace_Strategy(
                                const char* strDeviceId,
                                uint32_t interalId):m_ulInternalId(interalId)
{
    memset(m_strDeviceId, 0, sizeof(m_strDeviceId));
    memcpy(m_strDeviceId, strDeviceId, strlen(strDeviceId));

    m_szReportVct.clear();

    m_lStrategyTimer = -1;

    m_ulRefCount = 2;

    m_bSampleFlag = false;
    m_startSampleTime = 0;
}

SVS_Record_Trace_Strategy::~SVS_Record_Trace_Strategy()
{

}

//====================================公有函数==================================
int32_t SVS_Record_Trace_Strategy::reset()
{
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to reset record trace device.cameraId[%s],",
        m_strDeviceId));

    m_szReportVct.clear();

    // 策略定时器
    if(m_lStrategyTimer > -1)
    {
        (void)CAC_Timer_Manager::instance()->free_timer(m_lStrategyTimer);

        m_lStrategyTimer = -1;
    }

    // 遍历所有的策略，释放内存
    std::list<SVS_RECORD_TRACE_POLICY *> strategyList;
    SVS_RECORD_TRACE_POLICY * pTracePolicy = NULL;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_mutex, SVS_RESULT_FAILURE);
    SVS_TRACE_POLICY_MAP_ITER iter(m_strategyMap);
    SVS_TRACE_POLICY_MAP_ENTRY *pEntry = NULL;
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pTracePolicy = pEntry->int_id_;
        strategyList.push_back(pTracePolicy);
    }

    // 把录像跟踪策略对象删除
    while(!strategyList.empty())
    {
        pTracePolicy = strategyList.front();
        strategyList.pop_front();

        // 释放策略对象
        SVS_DELETE(pTracePolicy);
    }

    (void)m_strategyMap.unbind_all();

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Reset record trace device finished.cameraId[%s],",
        m_strDeviceId));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : sync_trace_strategy
功能描述  : 同步子系统本地的录像跟踪策略和控制台的策略保持一致。
            策略类型包括添加、修改和删除；
输入参数  : SVS_RECORD_TRACE_POLICY* tracePolicy:录像跟踪策略内容
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::syncTraceStrategy(const SVS_RECORD_TRACE_POLICY * pTracePolicy)
{
    if(NULL == pTracePolicy)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Sync record trace strategy failed.cameraId[%s]",
            m_strDeviceId));

        return SVS_RESULT_FAILURE;
    }

    char strStrategyId[SVS_RECORD_TRACE_POLICY_ID_LEN + 1] = {0,};
    memcpy(strStrategyId, pTracePolicy->POLICY_SEQ, sizeof(pTracePolicy->POLICY_SEQ));

    char strSyncTime[TIME_STRING_LEN + 1] = {0,};
    memcpy(strSyncTime, pTracePolicy->POLICY_SYNC_TIME, sizeof(pTracePolicy->POLICY_SYNC_TIME));

    char strStartTime[TIME_STRING_LEN + 1] = {0,};
    memcpy(strStartTime, pTracePolicy->BEGIN_TIME, sizeof(pTracePolicy->BEGIN_TIME));

    char strEndTime[TIME_STRING_LEN + 1] = {0,};
    memcpy(strEndTime, pTracePolicy->END_TIME, sizeof(pTracePolicy->END_TIME));

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to sync record trace strategy."
        "CamereId[%s],StrategyId[%s],SyncTime[%s],StrategyTime[%s--%s],"
        "StrategyStatus[%u],IS_ALLTIME[%u], IS_ANALYSE_FILE[%u].",
        m_strDeviceId,
        strStrategyId,
        strSyncTime,
        strStartTime,strEndTime,
        pTracePolicy->POLICY_STATE,
        pTracePolicy->IS_ALLTIME,
        pTracePolicy->IS_ANALYSE_FILE));

    int32_t iRet = SVS_RESULT_OK;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_mutex, SVS_RESULT_FAILURE);
    // 策略类型有添加、修改、删除
    switch(pTracePolicy->POLICY_STATE)
    {
        // 策略状态是删除状态，则把map中断的策略删除掉
        case SVS_RECORD_TRACE_POLICY_STATUS_DEL:
            {
                iRet = delTraceStrategy(strStrategyId);
            }
            break;

        // 策略状态时添加状态，则在map中先查找，如果找到则刷新，否则添加一条策略
        case SVS_RECORD_TRACE_POLICY_STATUS_ADD:
            {
                iRet = addTraceStrategy(pTracePolicy);
            }
            break;

        // 策略状态为修改，则在map中查找，如果找到则刷新，否在添加一条策略
        case SVS_RECORD_TRACE_POLICY_STATUS_MOD:
            {
                iRet = modTraceStrategy(pTracePolicy);
            }
            break;

        default:
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Sync record trace strategy failed as strategy status is invalid."
                "CamereId[%s],StrategyId[%s],SyncTime[%s],StrategyTime[%s--%s],"
                "StrategyStatus[%u],IS_ALLTIME[%u], IS_ANALYSE_FILE[%u].",
                m_strDeviceId,
                strStrategyId,
                strSyncTime,
                strStartTime,strEndTime,
                pTracePolicy->POLICY_STATE,
                pTracePolicy->IS_ALLTIME,
                pTracePolicy->IS_ANALYSE_FILE));

            break;
    }

    // 重新筛选录像跟踪策略
    if(SVS_RESULT_OK != selectTraceStrategy())
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Sync record trace strategy failed as select trace strategy failed."
            "CamereId[%s],StrategyId[%s],SyncTime[%s],StrategyTime[%s--%s],"
            "StrategyStatus[%u],IS_ALLTIME[%u], IS_ANALYSE_FILE[%u].",
            m_strDeviceId,
            strStrategyId,
            strSyncTime,
            strStartTime,strEndTime,
            pTracePolicy->POLICY_STATE,
            pTracePolicy->IS_ALLTIME,
            pTracePolicy->IS_ANALYSE_FILE));

        return SVS_RESULT_FAILURE;
    }

    return iRet;
}

/*****************************************************************************
 函 数 名  : sampleRecordQuality
 功能描述  : 采集录像质量.定时器任务回调函数，定时采集镜头的录像质量
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::sampleRecordQuality()
{
    // 如果策略没有生效，则本采集周期不需要采集
    if (!m_bSampleFlag)
    {
        return SVS_RESULT_OK;
    }

    // 采集录像质量内容
    SVS_RECORD_QUALITY_REPORT szReport;
    memset(&szReport, 0, sizeof(szReport));

    // 采集录像统计信息，如果失败，不用打印日志，因为跟踪策略是广播，镜头不一定在本NRU录像
    int32_t nRet = SVS_Stat_Manager::instance().sampleRecordQuality(m_strDeviceId, szReport);
    if(SVS_RESULT_OK != nRet)
    {
        // 修改统计启示时间
        m_startSampleTime = time(NULL);

        return SVS_RESULT_OK;
    }

    time_t endSampleTime = time(NULL);
    // 如果是第一个采集周期，起始时间修改为当前时间向前推10s
    if (0 == m_startSampleTime)
    {
        m_startSampleTime = endSampleTime - 10;
    }

    uint32_t sampleTimeLen = (uint32_t)(endSampleTime - m_startSampleTime);

    // 构造镜头ID、上报时间、采样开始和结束时间
    memcpy(szReport.LENS_ID, m_strDeviceId, sizeof(szReport.LENS_ID));

    char strTraceTime[TIME_STRING_LEN + 1] = {0,};
    (void)SVS_SS_UTILITIES::time2str(strTraceTime, sizeof(strTraceTime), endSampleTime);
    memcpy(szReport.TRACE_TIME, strTraceTime, sizeof(szReport.TRACE_TIME));

    char strBeginTime[TIME_STRING_LEN + 1] = {0,};
    (void)SVS_SS_UTILITIES::time2str(strBeginTime, sizeof(strBeginTime), m_startSampleTime);
    memcpy(szReport.BEGIN_TIME, strBeginTime, sizeof(szReport.BEGIN_TIME));

    char strEndTime[TIME_STRING_LEN + 1] = {0,};
    (void)SVS_SS_UTILITIES::time2str(strEndTime, sizeof(strEndTime), endSampleTime);
    memcpy(szReport.END_TIME, strEndTime, sizeof(szReport.END_TIME));

    m_startSampleTime = endSampleTime;

    // 计算平均值
    if(0 != sampleTimeLen)
    {
        // 单位kbps
        szReport.CODE_RATE =
            (uint32_t)((szReport.TOTAL_RECV_SIZE * RTRECORD_UNIT_BYTE) / sampleTimeLen);

        szReport.FRAME_RATE = szReport.TOTAL_FRAME_RECV / sampleTimeLen;

        // 丢帧率的算法:丢帧数*100/(丢帧数+收到帧数),FRAME_LOSE_RATE获取到的是丢帧的个数，所以丢帧率算法如下
        uint32_t ulLostRate = 0;
        uint32_t ulTotalFrameNum = szReport.TOTAL_FRAME_RECV + szReport.FRAME_LOSE_RATE;
        if(0 != ulTotalFrameNum)
        {
            ulLostRate = (szReport.FRAME_LOSE_RATE * RTRECORD_UNIT_PERCENT) / (ulTotalFrameNum);
        }
        szReport.FRAME_LOSE_RATE = ulLostRate;

        szReport.AVG_FRAME_SIZE = 0;
        if( 0 != szReport.TOTAL_FRAME_RECV)
        {
            szReport.AVG_FRAME_SIZE =
                (uint32_t)((szReport.TOTAL_RECV_SIZE * RTRECORD_UNIT_BYTE) / szReport.TOTAL_FRAME_RECV);
        }
    }

    SVS_LOG((SVS_LM_DEBUG,
        "[CSL]Sample record quality success."
        "CameraId[%s],traceTime[%s], traceTimeSeg[%s--%s],"
        "codeRate[%u]kpbs, frameRate[%u], frameLostRate[%u]percent, "
        "maxFrameSize[%u]kbit, minFrameSize[%u]kbit, avgFrameSize[%u]kbit, "
        "totalRecvSize[%u]KB, totalRecordSize[%u]KB, "
        "totalFrameRecv[%u], totalFrameRecord[%u], "
        "totalIFrameRecv[%u], totalIFrameRecord[%u], "
        "recordFilePath[%s], recordFileSize[%u]KB",
        m_strDeviceId, szReport.TRACE_TIME,szReport.BEGIN_TIME,szReport.END_TIME,
        szReport.CODE_RATE, szReport.FRAME_RATE, szReport.FRAME_LOSE_RATE,
        szReport.MAX_FRAME_SIZE, szReport.MIN_FRAME_SIZE, szReport.AVG_FRAME_SIZE,
        szReport.TOTAL_RECV_SIZE, szReport.TOTAL_RECORD_SIZE,
        szReport.TOTAL_FRAME_RECV, szReport.TOTAL_FRAME_RECORD,
        szReport.TOTAL_IFRAME_RECV, szReport.TOTAL_IFRAME_RECORD,
        szReport.RECORD_FILE_PATH, szReport.RECORD_FILE_SIZE));

    // 主机序转换成网络序
    szReport.CODE_RATE             = htonl(szReport.CODE_RATE);
    szReport.FRAME_RATE            = htonl(szReport.FRAME_RATE);
    szReport.FRAME_LOSE_RATE       = htonl(szReport.FRAME_LOSE_RATE);
    szReport.MAX_FRAME_SIZE        = htonl(szReport.MAX_FRAME_SIZE);
    szReport.MIN_FRAME_SIZE        = htonl(szReport.MIN_FRAME_SIZE);
    szReport.AVG_FRAME_SIZE        = htonl(szReport.AVG_FRAME_SIZE);
    szReport.TOTAL_RECV_SIZE       = htonl(szReport.TOTAL_RECV_SIZE);
    szReport.TOTAL_RECORD_SIZE     = htonl(szReport.TOTAL_RECORD_SIZE);
    szReport.TOTAL_FRAME_RECV      = htonl(szReport.TOTAL_FRAME_RECV);
    szReport.TOTAL_FRAME_RECORD    = htonl(szReport.TOTAL_FRAME_RECORD);
    szReport.TOTAL_IFRAME_RECV     = htonl(szReport.TOTAL_IFRAME_RECV);
    szReport.TOTAL_IFRAME_RECORD   = htonl(szReport.TOTAL_IFRAME_RECORD);
    szReport.RECORD_FILE_SIZE      = htonl(szReport.RECORD_FILE_SIZE);
    szReport.IS_FROM_FILE_ANALYSE  = htonl(SVS_RECORD_REPORT_TYPE_REALTIME);

    m_szReportVct.push_back(szReport);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : getRecordQualityReports
 功能描述  : 获取录像质量报告内容。
 输入参数  : NA
 输出参数  : RECORD_QUALITY_REPORT_VECTOR & szRecordReportVct:获取录像质量列表
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::getRecordQualityReports(RECORD_QUALITY_REPORT_VECTOR & szRecordReportVct)
{
    uint32_t ulReportsNum = m_szReportVct.size();

    for(uint32_t i = 0; i < ulReportsNum; i++)
    {
        szRecordReportVct.push_back(m_szReportVct[i]);
    }

    m_szReportVct.clear();

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Get record quality reports success."
        "cameraId[%s], report number[%u].",
        m_strDeviceId,
        ulReportsNum));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : isTraceFinished
 功能描述  : 录像质量跟踪是否已经完成。如果策略map中策略个数已经为0，则跟踪完成
             否则录像质量跟踪未完成。
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 录像质量完成返回0,录像质量未完成返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::isTraceFinished()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_mutex, SVS_RESULT_FAILURE);
    if(0 == m_strategyMap.current_size())
    {
        return SVS_RESULT_OK;
    }

    return SVS_RESULT_FAILURE;
}

/*****************************************************************************
 函 数 名  : increase_reference
 功能描述  : 引用计数递增函数
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::increaseReference()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, -1);
    m_ulRefCount++;

    SVS_LOG((SVS_LM_DEBUG,
        "[CSL]SVS_Record_Trace_Strategy[%s] increase_reference = %d",
        m_strDeviceId,
        m_ulRefCount));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : decrease_reference
 功能描述  : 引用计数递减函数
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::decreaseReference()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_RESULT_FAILURE);
    m_ulRefCount--;

    SVS_LOG((SVS_LM_DEBUG,
        "[CSL]SVS_Record_Trace_Strategy[%s] decrease_reference=%d.",
        m_strDeviceId,
        m_ulRefCount));

    // 如果应用计数为0，表示无别处在用，该线程可以把对象的状态置成停止状态
    if( 0 == m_ulRefCount)
    {
        //可以删除
        return 1;
    }

    return SVS_RESULT_OK;
}
const char* SVS_Record_Trace_Strategy::deviceId()const
{
    return m_strDeviceId;
}

//====================================私有函数==================================
/*****************************************************************************
函 数 名  : del_trace_strategy
功能描述  : 添加录像跟踪策略。首先在map中查找，是否策略已经存在，
            如果不存在，则添加一条新的策略
            如果已经存在，则把策略内容更新为新的策略内容；
输入参数  : SVS_RECORD_TRACE_POLICY* pTracePolicy:策略内容
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::addTraceStrategy(const SVS_RECORD_TRACE_POLICY * pTracePolicy)
{
    char strStrategyId[SVS_RECORD_TRACE_POLICY_ID_LEN + 1] = {0,};
    memcpy(strStrategyId, pTracePolicy->POLICY_SEQ, sizeof(pTracePolicy->POLICY_SEQ));

    char strStartTime[TIME_STRING_LEN + 1] = {0,};
    memcpy(strStartTime, pTracePolicy->BEGIN_TIME, sizeof(pTracePolicy->BEGIN_TIME));

    char strEndTime[TIME_STRING_LEN + 1] = {0,};
    memcpy(strEndTime, pTracePolicy->END_TIME, sizeof(pTracePolicy->END_TIME));

    SVS_RECORD_TRACE_POLICY* pTempTraceStrategy = NULL;
    // 先找到策略对象
    int32_t iRet = m_strategyMap.find(ACE_CString(strStrategyId),pTempTraceStrategy);
    if(0 == iRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Failed to add record trace strategy "
            "as this strategy already exist in the map.So update this Strategy.",
            "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s].",
            m_strDeviceId,
            strStrategyId,
            strStartTime,
            strEndTime));

        iRet = modTraceStrategy(pTracePolicy);

        return iRet;
    }

    time_t currTime = time(NULL);
    time_t startTime = SVS_SS_UTILITIES::str2time(strStartTime);
    time_t endTime = SVS_SS_UTILITIES::str2time(strEndTime);

    // 如果策略类型不是7*24的，判断时间的合法性
    if(SVS_RECORD_TRACE_POLICY_ALWAYS_TRACE != pTracePolicy->IS_ALLTIME)
    {
        // 如果策略时间段不合理 ，则不添加策略
        if(startTime >= endTime)
        {
            SVS_LOG((SVS_LM_WARNING,
                "[CSL]Failed to add record trace strategy "
                "as the time segment is invalid.",
                "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s].",
                m_strDeviceId,
                strStrategyId,
                strStartTime,
                strEndTime));

            return SVS_RESULT_FAILURE;
        }

        // 添加策略,如果时间过期了，则不用添加策略
        if(currTime >= endTime)
        {
            char strCurrTime[TIME_STRING_LEN + 1] = {0,};
            (void)SVS_SS_UTILITIES::time2str(strCurrTime, sizeof(strCurrTime), currTime);

            SVS_LOG((SVS_LM_WARNING,
                "[CSL]Failed to add record trace strategy "
                "as the time segment is overtime.",
                "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s],system current time[%s].",
                m_strDeviceId,
                strStrategyId,
                strStartTime,
                strEndTime,
                strCurrTime));

            return SVS_RESULT_OK;
        }
    }

    // 创建策略对象
    pTempTraceStrategy = SVS_NEW(pTempTraceStrategy);
    if (NULL == pTempTraceStrategy)
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Failed to add record trace strategy "
            "as allocte strategy object failed.",
            "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s].",
            m_strDeviceId,
            strStrategyId,
            strStartTime,
            strEndTime));

        return SVS_RESULT_FAILURE;
    }

    // 把策略内容保存，并把策略对象加入到map中
    memcpy(pTempTraceStrategy, pTracePolicy, sizeof(SVS_RECORD_TRACE_POLICY));
    iRet = m_strategyMap.bind(ACE_CString(strStrategyId), pTempTraceStrategy);
    if(0 != iRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Failed to add record trace strategy "
            "as allocte strategy object failed.",
            "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s].",
            m_strDeviceId,
            strStrategyId,
            strStartTime,
            strEndTime));

        SVS_DELETE(pTempTraceStrategy);

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
            "[CSL]Success to add record trace strategy to map.",
            "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s].",
            m_strDeviceId,
            strStrategyId,
            strStartTime,
            strEndTime));

    return SVS_RESULT_OK;
}

// 更新录像跟踪策略
int32_t SVS_Record_Trace_Strategy::modTraceStrategy(const SVS_RECORD_TRACE_POLICY * pTracePolicy)
{
    char strStrategyId[SVS_RECORD_TRACE_POLICY_ID_LEN + 1] = {0,};
    memcpy(strStrategyId, pTracePolicy->POLICY_SEQ, sizeof(pTracePolicy->POLICY_SEQ));

    char strStartTime[TIME_STRING_LEN + 1] = {0,};
    memcpy(strStartTime, pTracePolicy->BEGIN_TIME, sizeof(pTracePolicy->BEGIN_TIME));

    char strEndTime[TIME_STRING_LEN + 1] = {0,};
    memcpy(strEndTime, pTracePolicy->END_TIME, sizeof(pTracePolicy->END_TIME));

    // 先找到策略对象
    SVS_RECORD_TRACE_POLICY* pTempTraceStrategy = NULL;
    int32_t iRet = m_strategyMap.find(ACE_CString(strStrategyId), pTempTraceStrategy);
    if(0 != iRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Failed to update record trace strategy "
            "as this strategy not exist in the map.So add this Strategy.",
            "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s].",
            m_strDeviceId,
            strStrategyId,
            strStartTime,
            strEndTime));

        iRet = addTraceStrategy(pTracePolicy);

        return iRet;
    }

    time_t currTime = time(NULL);
    time_t startTime = SVS_SS_UTILITIES::str2time(strStartTime);
    time_t endTime = SVS_SS_UTILITIES::str2time(strEndTime);

    // 如果策略类型不是7*24的，判断时间的合法性
    if(SVS_RECORD_TRACE_POLICY_ALWAYS_TRACE != pTracePolicy->IS_ALLTIME)
    {
        // 如果策略时间段不合理 ，则删除旧的策略
        if(startTime >= endTime)
        {
            SVS_LOG((SVS_LM_WARNING,
                "[CSL]Failed to update record trace strategy "
                "as the time segment is invalid.So delete this strategy.",
                "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s].",
                m_strDeviceId,
                strStrategyId,
                strStartTime,
                strEndTime));

            delTraceStrategy(strStrategyId);

            return SVS_RESULT_FAILURE;
        }

        // 添加策略,如果时间过期了，则删除该策略
        if(currTime >= endTime)
        {
            char strCurrTime[TIME_STRING_LEN + 1] = {0,};
            (void)SVS_SS_UTILITIES::time2str(strCurrTime, sizeof(strCurrTime), currTime);

            SVS_LOG((SVS_LM_WARNING,
                "[CSL]Failed to update record trace strategy "
                "as the time segment is overtime. So delete this trace strategy.",
                "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s],system current time[%s].",
                m_strDeviceId,
                strStrategyId,
                strStartTime,
                strEndTime,
                strCurrTime));
            delTraceStrategy(strStrategyId);

            return SVS_RESULT_OK;
        }
    }

    // 把策略更新
    memcpy(pTempTraceStrategy, pTracePolicy, sizeof(SVS_RECORD_TRACE_POLICY));
    iRet = m_strategyMap.rebind(ACE_CString(strStrategyId), pTempTraceStrategy);
    if(-1 == iRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Failed to update record trace strategy "
            "as rebind strategy to map failed."
            "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s].",
            m_strDeviceId,
            strStrategyId,
            strStartTime,
            strEndTime));

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
            "[CSL]Success to update record trace strategy to map.",
            "CameraId[%s],StrategyId[%s],StrategyTime[%s--%s],iRet[%d].",
            m_strDeviceId,
            strStrategyId,
            strStartTime,
            strEndTime,
            iRet));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : del_trace_strategy
功能描述  : 删除录像跟踪策略。
输入参数  : const char* strStrategyId:策略ID
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::delTraceStrategy(const char* strStrategyId)
{
    if(NULL == strStrategyId)
    {
        SVS_LOG((SVS_LM_ERROR ,
            "[CSL]Delete record trace strategy failed.The parameter is invalid."
            "strStrategyId[0x%08x].",
            strStrategyId));

        return SVS_RESULT_FAILURE;
    }

    SVS_RECORD_TRACE_POLICY* pTempTraceStrategy = NULL;
    // 先找到策略对象
    int32_t iRet = m_strategyMap.find(ACE_CString(strStrategyId), pTempTraceStrategy);
    if(0 != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Failed to delete record trace strategy "
            "as find strategy from map failed.",
            "CameraId[%s],StrategyId[%s],current strategy number[%u].",
            m_strDeviceId,
            strStrategyId,
            m_strategyMap.current_size()));

        return SVS_RESULT_OK;
    }

    // 发送策略完成通知消息
    sendStrategyDoneNotifyMsg(strStrategyId);

    // 从map中移除策略对象
    iRet = m_strategyMap.unbind(ACE_CString(strStrategyId));
    if(0 != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,
        "[CSL]Failed to delete record trace strategy "
        "as unbind strategy from map failed.",
        "CameraId[%s],StrategyId[%s],current strategy number[%u].",
        m_strDeviceId,
        strStrategyId,
        m_strategyMap.current_size()));

        return SVS_ERR_OTERERR;
    }
    else
    {
        // 释放策略对象的内存
        SVS_DELETE(pTempTraceStrategy);
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Success to delete record trace strategy.",
            "CameraId[%s],StrategyId[%s],current strategy number[%u].",
            m_strDeviceId,
            strStrategyId,
            m_strategyMap.current_size()));
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : select_trace_strategy
功能描述  : 遍历策略map，重新筛选策略，重新设置策略定时器。
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::selectTraceStrategy()
{
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to select trace strategy.CameraId[%s].",
        m_strDeviceId));

    SVS_RECORD_TRACE_POLICY * pTracePolicy = NULL;
    time_t timerTime = 0; // 定时器时间
    time_t policyStartTime = 0;
    time_t policyEndTime = 0;
    time_t currTime = time(NULL);
    timerTime = currTime + SECONDS_PER_DAY; // 设置一个较的大值，当前时间加1整天
    bool bOldSampleFlag = m_bSampleFlag;
    m_bSampleFlag = false;
    char strStrategyId[SVS_RECORD_TRACE_POLICY_ID_LEN + 1] = {0,};
    char strStartTime[TIME_STRING_LEN + 1] = {0,};
    char strEndTime[TIME_STRING_LEN + 1] = {0,};
    std::list<SVS_RECORD_TRACE_POLICY *> strategyList;

    // 遍历所有策略，找到当前生效的策略
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_mutex, SVS_RESULT_FAILURE);
    SVS_TRACE_POLICY_MAP_ITER iter(m_strategyMap);
    SVS_TRACE_POLICY_MAP_ENTRY *pEntry = NULL;
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pTracePolicy = pEntry->int_id_;
        policyStartTime = SVS_SS_UTILITIES::str2time((char*)pTracePolicy->BEGIN_TIME);
        policyEndTime = SVS_SS_UTILITIES::str2time((char*)pTracePolicy->END_TIME);

        memset(strStrategyId, 0, sizeof(strStrategyId));
        memcpy(strStrategyId, pTracePolicy->POLICY_SEQ, sizeof(pTracePolicy->POLICY_SEQ));

        memset(strStartTime, 0, sizeof(strStartTime));
        memcpy(strStartTime, pTracePolicy->BEGIN_TIME, sizeof(pTracePolicy->BEGIN_TIME));

        memset(strEndTime, 0, sizeof(strEndTime));
        memcpy(strEndTime, pTracePolicy->END_TIME, sizeof(pTracePolicy->END_TIME));

        SVS_LOG((SVS_LM_INFO,
            "[CSL]CameraId[%s], strStrategyId[%s], StrategyTime[%s--%s],IS_ALLTIME[%d].",
            m_strDeviceId,
            strStrategyId,
            strStartTime, strEndTime,
            pTracePolicy->IS_ALLTIME));

        // 如果策略是7*24录像跟踪，则不用设置定时器
        if(SVS_RECORD_TRACE_POLICY_ALWAYS_TRACE == pTracePolicy->IS_ALLTIME)
        {
            m_bSampleFlag = true;
            if (m_lStrategyTimer > -1)
            {
                (void)CAC_Timer_Manager::instance()->free_timer(m_lStrategyTimer);
                m_lStrategyTimer = -1;
            }

            SVS_LOG((SVS_LM_INFO,
                "[CSL]Select trace strategy successful.Console always traces record."
                "cameraId[%s],IS_ALLTIME[%u]",
                m_strDeviceId,
                pTracePolicy->IS_ALLTIME));

            resetSampleInfo(bOldSampleFlag, m_bSampleFlag);
            return SVS_RESULT_OK;
        }

        // 如果策略过期，则跳过该策略,并且把策略加入list，后面删除该策略
        if(currTime >= policyEndTime)
        {
            strategyList.push_back(pTracePolicy);

            continue;
        }

        // 当前时间只要落在任何一个策略时间段中，则就得上报录像质量
        if ((currTime >= policyStartTime) && (currTime < policyEndTime))
        {
            m_bSampleFlag = true;
        }

        // 如果策略包含当前时间，且结束时间比定时器时间早，则重新设置定时器时间
        if((policyStartTime <= currTime) && (policyEndTime < timerTime))
        {
            timerTime = policyEndTime;
        }
        // 如果策略还没到时间，且开始时间比定时器时间早，则重新设置定时器时间
        else if((policyStartTime > currTime) && (policyStartTime < timerTime))
        {
            timerTime = policyStartTime;
        }
    }

    // 重新设置定时器
    if (m_lStrategyTimer > -1)
    {
        (void)CAC_Timer_Manager::instance()->free_timer(m_lStrategyTimer);
        m_lStrategyTimer = -1;
    }

    int32_t timerDelay = timerTime - currTime;
    ACE_Time_Value delayTime( timerDelay);
    ACE_Time_Value intervalTime( timerDelay);

    m_lStrategyTimer = CAC_Timer_Manager::instance()->alloc_timer(
                                    SVS_Record_Trace_Manager::instance(),
                                    SVS_Record_Trace_Manager::timerCallback,
                                    (int32_t)m_ulInternalId,
                                    delayTime,
                                    intervalTime,
                                    SVS_TRACE_TIMER_TYPE_STRATEGY);
    if(m_lStrategyTimer <= -1)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Select record strategy failed as allocate timer failed."
            "cameraId[%s].",
            m_strDeviceId));

        return SVS_RESULT_FAILURE;
    }

    resetSampleInfo(bOldSampleFlag, m_bSampleFlag);
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Select record strategy success."
        "cameraId[%s],timer delay[%u]s, timer[%d].",
        m_strDeviceId,
        timerDelay,
        m_lStrategyTimer));

    // 把过去的录像从map中删除
    while(!strategyList.empty())
    {
        pTracePolicy = strategyList.front();
        strategyList.pop_front();
        memset(strStrategyId, 0, sizeof(strStrategyId));
        memcpy(strStrategyId, pTracePolicy->POLICY_SEQ, sizeof(pTracePolicy->POLICY_SEQ));

        memset(strStartTime, 0, sizeof(strStartTime));
        memcpy(strStartTime, pTracePolicy->BEGIN_TIME, sizeof(pTracePolicy->BEGIN_TIME));

        memset(strEndTime, 0, sizeof(strEndTime));
        memcpy(strEndTime, pTracePolicy->END_TIME, sizeof(pTracePolicy->END_TIME));

        // 发送策略完成通知消息
        sendStrategyDoneNotifyMsg(strStrategyId);

        // unbind策略对象
        if( 0 != m_strategyMap.unbind(ACE_CString(strStrategyId)))
        {
            SVS_LOG((SVS_LM_WARNING,
                "[CSL]Find record trace strategy failed."
                "cameraId[%s], strategyId[%s],strategyTime[%s--%s]",
                m_strDeviceId,
                strStrategyId,
                strStartTime,
                strEndTime));

            continue;
        }

        // 释放策略对象
        SVS_DELETE(pTracePolicy);

        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Record trace strategy is over time, so delete it."
            "cameraId[%s], strategyId[%s],strategyTime[%s--%s]",
            m_strDeviceId,
            strStrategyId,
            strStartTime,
            strEndTime));
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : sendStrategyDoneNotifyMsg
功能描述  : 当录像跟踪策略完成后(到结束时间后)，发送策略完成通知消息给控制台。
输入参数  : const char* strStrategyId:录像跟踪策略ID
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
void SVS_Record_Trace_Strategy::sendStrategyDoneNotifyMsg(const char* strStrategyId)const
{
    if(NULL == strStrategyId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Send strategy finised notify message to Console failed.",
            "The parameter is invalid.strStrategyId[0x%08x].",
            strStrategyId));

        return;
    }

    // 发送录像质量报告完成通知消息
    uint32_t ulMbSize = sizeof(NRU_RECORD_QUALITY_DONE_REQ_MSG);

    ACE_Message_Block* mb = allocate_mb(ulMbSize);

    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Send record trace strategy done message failed.Allocate message block failed."
            "cameraId[%s], strategyId.",
            m_strDeviceId,
            strStrategyId));

        return;
    }

    // 封装消息
    NRU_RECORD_QUALITY_DONE_REQ_MSG* pSendDoneMsg = (NRU_RECORD_QUALITY_DONE_REQ_MSG*)(void*)mb->base();
    mb->wr_ptr( ulMbSize);

    memset(pSendDoneMsg, 0, sizeof(NRU_RECORD_QUALITY_DONE_REQ_MSG));

    FillCommonHeader(   pSendDoneMsg->Header,
                        NRU_RECORD_QUALITY_DONE_REQ,
                        CAC_RT_Record_Server::instance()->transaction_no(),
                        (uint16_t)ulMbSize);

    memcpy(pSendDoneMsg->POLICY_ID, strStrategyId, sizeof(pSendDoneMsg->POLICY_ID));

    // 发送消息到消息发送队列
    if(SVS_RESULT_OK != SVS_Console_Server::instance()->sendMsg(mb))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Send record trace strategy done message failed.Send message failed."
            "cameraId[%s], strategyId.",
            m_strDeviceId,
            strStrategyId));

        return;
    }

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Send record trace strategy done message success."
        "cameraId[%s], strategyId.",
        m_strDeviceId,
        strStrategyId));

    return;
}

/*****************************************************************************
函 数 名  : resetSampleInfo
功能描述  : 如果原来跟踪策略没有生效，现在跟踪策略刚刚生效，则把stat原来的数据初始化
输入参数  : bool bOldSampleFlag:旧的采样标记
            bool bNewSampleFlag:新的采样标记
输出参数  : NA
返 回 值  : NA
修改历史  :
*****************************************************************************/
void SVS_Record_Trace_Strategy::resetSampleInfo(bool bOldSampleFlag, bool bNewSampleFlag)
{
    // 如果原来是需要录像跟踪的，当前又不需要跟踪了，需要立即采集一次录像质量
    if ((bOldSampleFlag) && (!m_bSampleFlag))
    {
        m_bSampleFlag = true;
        (void)sampleRecordQuality();
        m_bSampleFlag = false;
    }

    // 原来不需要采样，现在需要采样，则需要把采样时间复位，同时重新设置采样数据
    if(bNewSampleFlag)
    {
        if(!bOldSampleFlag)
        {
            (void)SVS_Stat_Manager::instance().resetSampleInfo(m_strDeviceId);
            m_startSampleTime = time(NULL);
        }
    }
}
//=================SVS_Record_Trace_Strategy END================================

//=================SVS_Record_Trace_Manager BEGIN===============================
SVS_Record_Trace_Manager::SVS_Record_Trace_Manager()
{
    // 录像采样周期性定时器和周期
    m_lSampleTimer = -1;
    m_ulSampleInterval = 0;

    // 录像质量上报周期性定时器和周期
    m_lReportTimer = -1;
    m_ulReportInterval = 0;

    m_ulNextInternalId = 1;
}

SVS_Record_Trace_Manager::~SVS_Record_Trace_Manager()
{
    if(m_lSampleTimer > -1)
    {
        (void)CAC_Timer_Manager::instance()->free_timer(m_lSampleTimer);
        m_lSampleTimer = -1;
        m_ulSampleInterval = 0;
    }

    if(m_lReportTimer > -1)
    {
        (void)CAC_Timer_Manager::instance()->free_timer(m_lReportTimer);
        m_lReportTimer = -1;
        m_ulReportInterval = 0;
    }
}

//====================================公有函数==================================
/*****************************************************************************
函 数 名  : sync_record_trace_strategy
功能描述  : 同步子系统本地的录像跟踪策略和控制台的策略保持一致。
            策略类型包括添加、修改和删除；在处理任何一条策略失败的时候，继续处理
            下一条策略，但是把失败返回给控制台。
输入参数  : const ACE_Message_Block* mb:来自控制台的录像跟踪策略内容，最大100
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::syncRecordTraceStrategy(const ACE_Message_Block* mb)
{
    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Sync record trace strategy failed. The parameter is invalid.mb[0x%08x].",
            mb));

        return SVS_RESULT_FAILURE;
    }

    uint32_t ulRecordErrCode = SVS_RESULT_OK;
    // 解析消息体
    RECORD_QUALITY_POLICY_SYN_REQ_MSG* pMsg = (RECORD_QUALITY_POLICY_SYN_REQ_MSG*)(void*)mb->base();

    // 网络序转换成主机序
    pMsg->POLICY_COUNT = ntohl(pMsg->POLICY_COUNT);
    uint32_t ulSampleInterval = ntohl(pMsg->POLICY_LIST[0].SMAPLE_INTERVAL);
    uint32_t ulReportInterval = ntohl(pMsg->POLICY_LIST[0].REPORT_INTERVAL);
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to Sync record trace strategy. "
        "Strategy number[%u], sample interval[%u], report interval[%u].",
        pMsg->POLICY_COUNT,
        ulSampleInterval,
        ulReportInterval));

    // 重设采样和上报定时器
    int32_t iRet = resetTraceInterval(ulSampleInterval, ulReportInterval);
    if(SVS_RESULT_OK != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Sync record trace stracetegy failed.Reset trace interval failed."));

        ulRecordErrCode = SVS_ERR_OTERERR;
    }

    // 解析每个策略，找到对应的镜头跟踪策略对象处理具体的策略内容
    SVS_RECORD_TRACE_POLICY* pStrategy = NULL;
    char strDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0,};
    SVS_Record_Trace_Strategy * pTraceDevice = NULL;
    for(uint32_t i = 0; i < pMsg->POLICY_COUNT; i++)
    {
        pStrategy = &pMsg->POLICY_LIST[i];

        // 网络序转主机序
        pStrategy->POLICY_STATE = ntohl(pStrategy->POLICY_STATE);
        pStrategy->IS_ALLTIME = ntohl(pStrategy->IS_ALLTIME);
        pStrategy->IS_ANALYSE_FILE = ntohl(pStrategy->IS_ANALYSE_FILE);

        //1 TODO [实时录像质量跟踪]US只处理实时跟踪
        if (SVS_RECORD_TRACE_POLICY_TYPE_REALTIME != pStrategy->IS_ANALYSE_FILE)
        {
            SVS_LOG((SVS_LM_WARNING,
                "[CSL]Record trace type is not realtime trace, so doesn't handle."
                "IS_ANALYSE_FILE[%u].",
                pStrategy->IS_ANALYSE_FILE));

            continue;
        }

        memset(strDeviceId, 0, sizeof(strDeviceId));
        memcpy(strDeviceId, pStrategy->LENS_ID, sizeof(pStrategy->LENS_ID));

        // 查找策略对应的镜头对象
        pTraceDevice = getDevice(strDeviceId);

        // 如果没有找到设备，添加一个策略对应的设备
        if(NULL == pTraceDevice)
        {
            //创建录像设备
            int32_t addResult = addDevice(strDeviceId, pTraceDevice);

            if ( 0 != addResult )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "[CSL]Sync record trace failed."
                    "Create a object for record trace strategy failed, "
                    "cameraId[%s].",
                    strDeviceId));

                ulRecordErrCode = SVS_ERR_OTERERR;

                continue;
            }
        }

        SVS_TRACE_DEVICE_AUTO_PTR ptr(this, pTraceDevice);

        // 更新策略请求
        iRet = pTraceDevice->syncTraceStrategy(pStrategy);
        if (SVS_RESULT_OK != iRet)
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Sync record trace failed."
                "Trace device sync trace strategy failed. "
                "cameraId[%s].",
                strDeviceId));

            ulRecordErrCode = SVS_ERR_OTERERR;

            continue;
        }
    }

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Success to Sync record trace strategy. "
        "Strategy number[%u], sample interval[%u], report interval[%u].",
        pMsg->POLICY_COUNT,
        pMsg->POLICY_LIST[0].SMAPLE_INTERVAL,
        pMsg->POLICY_LIST[0].REPORT_INTERVAL));

    // 构造策略同步响应消息
    uint32_t respMsgLen = sizeof(RECORD_QUALITY_POLICY_SYN_RESP_MSG);
    ACE_Message_Block* respMb = allocate_mb(respMsgLen);

    if(NULL == respMb)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "[CSL]Create Record trace strategy sync response message to Console failed "
            "as allocate mb failed."));

        return SVS_RESULT_FAILURE;
    }

    // 消息号
    uint16_t msgNo = transactionno_respond(pMsg->Header.TransactionNo);

    // 构造策略响应消息
    RECORD_QUALITY_POLICY_SYN_RESP_MSG *respMsg =
        (RECORD_QUALITY_POLICY_SYN_RESP_MSG *)(void *)respMb->base();
    respMb->wr_ptr( respMsgLen);

    memset(respMsg, 0x0, respMsgLen);
    FillCommonHeader(respMsg->Header,
                RECORD_QUALITY_POLICY_SYN_RESP,
                msgNo,
                (uint16_t)respMsgLen);
    respMsg->ERROR_CODE = htonl(ulRecordErrCode);

    // 录像跟踪策略同步响应消息入队列
    int32_t iEnQueueRet = SVS_Console_Server::instance()->sendMsg(respMb);

    if (SVS_RESULT_FAILURE == iEnQueueRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Create Record trace strategy sync response message to Console failed "
            "Enqueue mb to send queue failed."));

        delete respMb;
        respMb = NULL;

        return SVS_ERR_QUEUE;
    }

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Enqueue Record trace strategy sync response message to send queue success."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : decrease_reference
功能描述  : 引用计数自动减
输入参数  : SVS_Record_Trace_Strategy *pDevice:录像跟踪设备对象。通过引用计算
            判断如果没有别人在用，则释放对象；
输出参数  : NA
返 回 值  : 成功返回0,失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::decrease_reference(SVS_Record_Trace_Strategy *pDevice)
{
    if( NULL == pDevice )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "[CSL]Process reference decreasing for record trace object failed."
            "The parameter is NULL.") );

        return SVS_ERR_PARAM;
    }

    size_t nTotal = 0;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_deviceMapMutex, SVS_RESULT_FAILURE);
    if (1 != pDevice->decreaseReference())
    {
        pDevice = NULL;
        return SVS_RESULT_OK;
    }

    if(0 != m_deviceMap.unbind(pDevice->deviceId()))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Fail to delete object from record trace map.trace camera[%s].",
            pDevice->deviceId()));
    }

    nTotal = m_deviceMap.current_size();
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Stop to trace the camera record.CamereaId[%s]"
        "Still has [%d] cameras recording now.",
        pDevice->deviceId(),
        nTotal ));

    // 释放资源
    (void)pDevice->reset();
    SVS_DELETE(pDevice);

    return SVS_RESULT_OK;
}

// 定时器回调函数
void SVS_Record_Trace_Manager::timerCallback(void * pArg, int32_t nEventId, int32_t lTimerId, int32_t nDummy)
{
    uint32_t msgLen = sizeof(SVS_TIMER_CALL_BACK_MSG);

    ACE_Message_Block *mb = allocate_mb(msgLen);

    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR, "[CSL]Timer callback failed as allocate mb failed."));

        return ;
    }

    SVS_TIMER_CALL_BACK_MSG* pMsg = (SVS_TIMER_CALL_BACK_MSG*)(void*)mb->wr_ptr();

    pMsg->MsgType = SVS_TIMER_CALL_BACK_MSG::RECORD_TRACE_TYPE;
    pMsg->ProcObject = pArg;
    pMsg->EventId = nEventId;
    pMsg->TimerId = lTimerId;
    pMsg->Dummy = nDummy;
    mb->wr_ptr( msgLen );

    int32_t iRet = SVS_Console_Server::instance()->addTimerCallbackMsg( mb );
    if(SVS_RESULT_FAILURE == iRet)
    {
        SVS_LOG((SVS_LM_ERROR, "[CSL]Add timer callback message to queue failed."));
        delete mb;
        mb = NULL;
    }

    return;
}

/*****************************************************************************
函 数 名  : onTimer
功能描述  : 处理定时器任务。
输入参数  : int32_t nEventId:内部事件ID。
            int32_t nDummy:定时器任务类型。
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::onTimer(int32_t nEventId, int32_t lTimerId, int32_t nDummy)
{
    SVS_LOG((SVS_LM_DEBUG,
        "[CSL]Begin to handle record trace timer task."
        "nEventId[%d], lTimerId[%d], nDummy[%d]",
        nEventId,
        lTimerId,
        nDummy));

    int32_t iRet = SVS_RESULT_FAILURE;
    switch(nDummy)
    {
        // 如果是策略定时器，则查找录像跟踪对象，处理策略到处处理
        case SVS_TRACE_TIMER_TYPE_STRATEGY:
            {
                SVS_Record_Trace_Strategy* pTraceDevice = getDevice((uint32_t)nEventId);
                if (NULL != pTraceDevice)
                {
                    // 执行镜头的回调函数
                    SVS_TRACE_DEVICE_AUTO_PTR ptr(this, pTraceDevice);
                    iRet = pTraceDevice->selectTraceStrategy();
                }
            }
            break;

        // 如果是录像质量采集定时器任务，则启动录像采集
        case SVS_TRACE_TIMER_TYPE_SAMPLE:
            {
                iRet = sampleRecordQuality();
            }
            break;
        case SVS_TRACE_TIMER_TYPE_REPORT:
            {
                iRet = reportRecordQuality();
            }
            break;
        default:
            SVS_LOG((SVS_LM_WARNING,
                "[CSL]Handle record trace timer task failed."
                "Dummy is invalid.nDummy[%d].",
                nDummy));

            iRet = SVS_RESULT_FAILURE;
    }

    return iRet;
}

//====================================私有函数==================================
/*****************************************************************************
函 数 名  : sampleRecordQuality
功能描述  : 录像质量采集。遍历所有正在进行录像采集的设备对象，让其收集本采样周期
            的采样数据。
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::sampleRecordQuality()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_deviceMapMutex, SVS_RESULT_FAILURE);
    uint32_t ulTraceDevNum = m_deviceMap.current_size();
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to sample record quality.trace device number[%u]",
        ulTraceDevNum));

    SVS_Record_Trace_Strategy* pTraceDevice = NULL;
    // 遍历所有采集对象
    SVS_TRACE_DEVICE_MAP_ITER iter(m_deviceMap);
    SVS_TRACE_DEVICE_MAP_ENTRY *pEntry = NULL;
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pTraceDevice = pEntry->int_id_;

        // 采集该镜头的录像数据
        (void)pTraceDevice->sampleRecordQuality();
    }

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Finish to sample record quality.trace device number[%u]",
        ulTraceDevNum));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : reportRecordQuality
功能描述  : 录像质量上报。遍历所有正在进行录像采集的设备对象，让其收集本采样周期
            的采样数据。
输入参数  : NA
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::reportRecordQuality()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_deviceMapMutex, SVS_RESULT_FAILURE);
    uint32_t ulTraceDevNum = m_deviceMap.current_size();
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to report record quality.trace device number[%u]",
        ulTraceDevNum));

    // 采集设备list，用于收集已经完成采集的镜头对象
    std::list<SVS_Record_Trace_Strategy *> traceDevList;

    SVS_Record_Trace_Strategy* pTraceDevice = NULL;
    RECORD_QUALITY_REPORT_VECTOR szRecordReportVct;

    // 遍历所有采集对象
    SVS_TRACE_DEVICE_MAP_ITER iter(m_deviceMap);
    SVS_TRACE_DEVICE_MAP_ENTRY *pEntry = NULL;
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pTraceDevice = pEntry->int_id_;

        // 如果镜头的录像采集任务完成，即所有的策略已经过期，则收集该镜头对象，后面删除
        if(SVS_RESULT_OK == pTraceDevice->isTraceFinished())
        {
            traceDevList.push_back(pTraceDevice);
        }

        // 获取镜头的本上报周期的采样的数据
        szRecordReportVct.clear();
        (void)pTraceDevice->getRecordQualityReports(szRecordReportVct);

        // 发送录像质量数据
        sendRecordQuality(szRecordReportVct);
    }

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Finish to sample record quality.trace device number[%u]",
        ulTraceDevNum));

    // 释放已经没有策略的录像采集镜头对象
    while(!traceDevList.empty())
    {
        pTraceDevice = traceDevList.front();
        traceDevList.pop_front();
        SVS_TRACE_DEVICE_AUTO_PTR ptr(this, pTraceDevice);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : reset_trace_interval
功能描述  : 如果采样周期变化了，则重新设置采样定时器，并清空本周期的数据；
            如果上报周期变化了，则重设设置上报定时器；
输入参数  : uint32_t sampleInterval:新的采样周期
            uint32_t reportInterval:新的上报周期
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::resetTraceInterval(uint32_t ulSampleInterval, uint32_t ulReportInterval)
{
    // 如果采样周期变化，或者采样定时器为无效，则重新设置采样定时器
    if ((-1 >= m_lSampleTimer) || (m_ulSampleInterval != ulSampleInterval))
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Sample interval is changed."
            "old sample interval[%u], new sample interval[%u], old sample timer[%d].",
            m_ulSampleInterval,
            ulSampleInterval,
            m_lSampleTimer));

        // 释放老的定时器
        if(m_lSampleTimer > -1)
        {
            (void)CAC_Timer_Manager::instance()->free_timer(m_lSampleTimer);
        }

        // 申请新的定时器
        ACE_Time_Value delayTime( (int32_t)ulSampleInterval);
        ACE_Time_Value intervalTime( (int32_t)ulSampleInterval);
        m_lSampleTimer = CAC_Timer_Manager::instance()->alloc_timer(
                                    SVS_Record_Trace_Manager::instance(),
                                    SVS_Record_Trace_Manager::timerCallback,
                                    0,
                                    delayTime,
                                    intervalTime,
                                    SVS_TRACE_TIMER_TYPE_SAMPLE);

        if (m_lSampleTimer <= -1)
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Reset sample timer failed."
                "old sample interval[%u], new sample interval[%u].",
                m_ulSampleInterval,
                ulSampleInterval));

            return SVS_RESULT_FAILURE;
        }

        m_ulSampleInterval = ulSampleInterval;

        SVS_LOG((SVS_LM_INFO,
            "[CSL]Reset sample timer success."
            "sample interval[%u], sample interval[%u].",
            m_ulSampleInterval,
            ulSampleInterval));
    }

    // 如果上报周期变化，或者上报定时器为无效，则重新设置上报定时器
    if ((-1 >= m_lReportTimer) || (m_ulReportInterval != ulReportInterval))
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Record trace Report interval is changed."
            "old report interval[%u], new report interval[%u], old report timer[%d].",
            m_ulReportInterval,
            ulReportInterval,
            m_lReportTimer));

        // 释放老的定时器
        if(m_lReportTimer> -1)
        {
            (void)CAC_Timer_Manager::instance()->free_timer(m_lReportTimer);
        }

        // 申请新的定时器
        ACE_Time_Value delayTime( (int32_t)ulReportInterval);
        ACE_Time_Value intervalTime( (int32_t)ulReportInterval);
        m_lReportTimer = CAC_Timer_Manager::instance()->alloc_timer(
                                    SVS_Record_Trace_Manager::instance(),
                                    SVS_Record_Trace_Manager::timerCallback,
                                    0,
                                    delayTime,
                                    intervalTime,
                                    SVS_TRACE_TIMER_TYPE_REPORT);

        if (m_lReportTimer <= -1)
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Reset report timer failed."
                "old report interval[%u], new report interval[%u].",
                m_ulReportInterval,
                ulReportInterval));

            return SVS_RESULT_FAILURE;
        }

        m_ulReportInterval = ulReportInterval;

        SVS_LOG((SVS_LM_INFO,
            "[CSL]Reset report timer success."
            "report interval[%u], report interval[%u].",
            m_ulReportInterval,
            ulReportInterval));
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : sendRecordQuality
功能描述  : 发送录像质量报告。每次只发送一个镜头的录像采集数据，最大只能发送100
            条采集信息，当大于100条的时候，分多条发送。
            分多次发送
输入参数  : RECORD_QUALITY_REPORT_VECTOR szRecordReportVct:录像采集数据量列表
输出参数  : NA
返 回 值  : NA
修改历史  :
*****************************************************************************/
void SVS_Record_Trace_Manager::sendRecordQuality(RECORD_QUALITY_REPORT_VECTOR &szRecordReportVct)const
{
    uint32_t ulVctSize = szRecordReportVct.size();
    // 如果没有数据需要上报
    if(0 == ulVctSize)
    {
        return;
    }

    char strDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0,};
    memcpy(strDeviceId, szRecordReportVct[0].LENS_ID, sizeof(szRecordReportVct[0].LENS_ID));

    uint32_t ulStartIndex = 0;
    uint32_t ulEndIndex = 0;
    // 每次最多发送100条录像质量采集数据
    for (uint32_t i = 0; i < ulVctSize; i += RTRECORD_UNIT_PERCENT)
    {
        ulStartIndex = i;
        ulEndIndex = i + (RTRECORD_UNIT_PERCENT - 1);
        if (ulEndIndex >= ulVctSize)
        {
            ulEndIndex = ulVctSize - 1;
        }

        uint32_t ulMbSize = sizeof(NRU_RECORD_QUALITY_REQ_MSG)+
                (ulEndIndex - ulStartIndex) * sizeof(SVS_RECORD_QUALITY_REPORT);

        ACE_Message_Block* mb = allocate_mb(ulMbSize);

        if(NULL == mb)
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Send record quality failed.Allocate message block failed."
                "cameraId[%s], vector ulStartIndex[%u], vector ulEndIndex[%u].",
                strDeviceId,
                ulStartIndex,
                ulEndIndex));

            continue;
        }

        // 构造消息内容
        NRU_RECORD_QUALITY_REQ_MSG* sendMsg = (NRU_RECORD_QUALITY_REQ_MSG*)(void*)mb->base();
        mb->wr_ptr(ulMbSize);
        memset(sendMsg, 0, sizeof(ulMbSize));

        FillCommonHeader(   sendMsg->Header,
                        NRU_RECORD_QUALITY_REQ,
                        CAC_RT_Record_Server::instance()->transaction_no(),
                        (uint16_t)ulMbSize);
        sendMsg->LENS_COUNT = htonl((ulEndIndex - ulStartIndex ) + 1);

        uint32_t j = 0;
        for(uint32_t k = ulStartIndex; k <= ulEndIndex;k++)
        {
            memcpy(&sendMsg->LENS_LIST[j], &szRecordReportVct[k], sizeof(sendMsg->LENS_LIST[0]));
            j++;
        }

        // 发送录像质量内容
        if(SVS_RESULT_OK !=  SVS_Console_Server::instance()->sendMsg(mb))
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Send record quality failed.Send message failed."
                "cameraId[%s], vector ulStartIndex[%u], vector ulEndIndex[%u].",
                strDeviceId,
                ulStartIndex,
                ulEndIndex));

            // 释放内存
            delete mb;
            mb = NULL;

            continue;
        }

        SVS_LOG((SVS_LM_INFO,
            "[CSL]Send record quality success."
            "cameraId[%s], vector ulStartIndex[%u], vector ulEndIndex[%u].",
            strDeviceId,
            ulStartIndex,
            ulEndIndex));
    }

    return;
}


// 根据设备ID获取设备对象
SVS_Record_Trace_Strategy* SVS_Record_Trace_Manager::getDevice(const char* strDeviceId)
{
    if(NULL == strDeviceId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Get record trace device failed.The parameter is NULL."));

        return NULL;
    }

    SVS_Record_Trace_Strategy *pDevice = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_deviceMapMutex, NULL);

    int32_t findResult = m_deviceMap.find(strDeviceId, pDevice);

    if ( 0 != findResult )
    {
        return NULL;
    }

    //引用计数加1
    int32_t increaseResult = pDevice->increaseReference();
    if( SVS_RESULT_OK != increaseResult )
    {
        pDevice = NULL;
        return NULL;
    }

    return pDevice;
}

// 根据设备对象内部ID,获取设备对象
SVS_Record_Trace_Strategy* SVS_Record_Trace_Manager::getDevice(uint32_t ulInternalId)
{
    SVS_Record_Trace_Strategy *pTraceDevice = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_deviceMapMutex, NULL);

    int32_t findResult = m_deviceTnternalIdMap.find(ulInternalId, pTraceDevice);

    if ( 0 != findResult )
    {
        return NULL;
    }

    //引用计数加1
    int32_t increaseResult = pTraceDevice->increaseReference();
    if( SVS_RESULT_OK != increaseResult )
    {
        pTraceDevice = NULL;
        return NULL;
    }

    return pTraceDevice;
}


/*****************************************************************************
函 数 名  : add_device
功能描述  : 添加录像跟踪镜头对象.
输入参数  : const char * strLensId:镜头ID
输出参数  : SVS_Record_Trace_Strategy*& pTraceDevice:录像跟踪镜头对象指针地址
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::addDevice(const char* strDeviceId,
                                       SVS_Record_Trace_Strategy*& pTraceDevice)
{
    // 参数判断
    if(NULL == strDeviceId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Create a record object for record trace failed."
            "The parameter is invalid.strDeviceId[0x%08x].",
            strDeviceId));

        return SVS_ERR_PARAM;
    }

    pTraceDevice = NULL;
    SVS_Record_Trace_Strategy *pTmpDevice = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_deviceMapMutex, SVS_RESULT_FAILURE);

    // 尝试在map中查找，避免重复创建的情况
    (void)m_deviceMap.find(strDeviceId, pTmpDevice);
    if (NULL == pTmpDevice)
    {
        //获取内部索引
        uint32_t internalId = nextInternalId();

        if (0 == internalId)
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Fail to allocate an internal id for record trace object. camera[%s].",
                strDeviceId));

            return SVS_ERR_OTERERR;
        }

        // 创建录像跟踪对象
        try
        {
            pTmpDevice = new SVS_Record_Trace_Strategy(strDeviceId, internalId);
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Create a record object for record trace failed."
                "Alloacate memory failed.cameraId[%s]."
                "There are [%u] cameras are being traced now.",
                strDeviceId ,
                m_deviceMap.current_size()));

            return SVS_ERR_NOMEM;
        }

        // 加入map
        int32_t bindResult = m_deviceMap.bind(pTmpDevice->deviceId(), pTmpDevice);

        if ( 0 != bindResult )
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Bind a record trace object for camera to map failed."
                "cameraId[%s],Return code[%d]."
                "There are [%u] cameras are being traced now.",
                strDeviceId,
                bindResult,
                m_deviceMap.current_size()));

            SVS_DELETE(pTmpDevice);

            return SVS_ERR_QUEUE;
        }

        // 把内容部id加入相应map
        bindResult = m_deviceTnternalIdMap.bind(internalId, pTmpDevice);
        if ( 0 != bindResult )
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Bind a record trace object for camera to internal id map failed."
                "cameraId[%s],Return code[%d]."
                "There are [%u] cameras are being traced now.",
                strDeviceId,
                bindResult,
                m_deviceMap.current_size()));

            (void)m_deviceMap.unbind(pTmpDevice->deviceId());
            SVS_DELETE(pTmpDevice);

            return SVS_ERR_QUEUE;
        }

        ACE_OS::last_error(0);

        SVS_LOG((SVS_LM_INFO,
            "[CSL]Create a record trace object for camera succeeded, "
            "cameraId[%s], "
            "There are [%u] cameras are being traced now.",
            strDeviceId,
            m_deviceMap.current_size()));
    }
    else
    {
        (void)pTmpDevice->increaseReference();
    }

    pTraceDevice = pTmpDevice;
    pTmpDevice = NULL;

    return SVS_RESULT_OK;
    // 加入map，从map中解绑定时会释放
}//lint !e429

/*****************************************************************************
 函 数 名  : next_internal_id
 功能描述  : 生成下一个内部索引ID
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 如果内部ID没有达到最大值:下一个内部ID, 如果内部ID达到最大值:1
 修改历史  :
*****************************************************************************/
uint32_t SVS_Record_Trace_Manager::nextInternalId()
{
    uint32_t internalId = 0;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_deviceMapMutex, 0);
    for (uint32_t i = 1; i < UINT_MAX; i++)
    {
        internalId = m_ulNextInternalId++;
        if (-1 == m_deviceTnternalIdMap.find(internalId))
        {
            return internalId;
        }

        if (UINT_MAX == m_ulNextInternalId)
        {
            m_ulNextInternalId = 1;
        }
    }

    return SVS_RESULT_OK;
}
//=================SVS_Record_Trace_Manager END=================================


