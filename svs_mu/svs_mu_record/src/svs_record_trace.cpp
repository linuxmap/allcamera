/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : SVS_Record_Trace.cpp
  �� �� ��   : ����
  ��������   : 2010��3��10��
  ����޸�   :
  ��������   : ¼�����������ࡣ����¼����ٲ��ԣ������Բɼ�¼�����������������ϱ�
               ������̨��
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2010��3��10��
    �޸�����   : �����ļ�

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

//====================================���к���==================================
int32_t SVS_Record_Trace_Strategy::reset()
{
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to reset record trace device.cameraId[%s],",
        m_strDeviceId));

    m_szReportVct.clear();

    // ���Զ�ʱ��
    if(m_lStrategyTimer > -1)
    {
        (void)CAC_Timer_Manager::instance()->free_timer(m_lStrategyTimer);

        m_lStrategyTimer = -1;
    }

    // �������еĲ��ԣ��ͷ��ڴ�
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

    // ��¼����ٲ��Զ���ɾ��
    while(!strategyList.empty())
    {
        pTracePolicy = strategyList.front();
        strategyList.pop_front();

        // �ͷŲ��Զ���
        SVS_DELETE(pTracePolicy);
    }

    (void)m_strategyMap.unbind_all();

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Reset record trace device finished.cameraId[%s],",
        m_strDeviceId));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : sync_trace_strategy
��������  : ͬ����ϵͳ���ص�¼����ٲ��ԺͿ���̨�Ĳ��Ա���һ�¡�
            �������Ͱ�����ӡ��޸ĺ�ɾ����
�������  : SVS_RECORD_TRACE_POLICY* tracePolicy:¼����ٲ�������
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
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
    // ������������ӡ��޸ġ�ɾ��
    switch(pTracePolicy->POLICY_STATE)
    {
        // ����״̬��ɾ��״̬�����map�жϵĲ���ɾ����
        case SVS_RECORD_TRACE_POLICY_STATUS_DEL:
            {
                iRet = delTraceStrategy(strStrategyId);
            }
            break;

        // ����״̬ʱ���״̬������map���Ȳ��ң�����ҵ���ˢ�£��������һ������
        case SVS_RECORD_TRACE_POLICY_STATUS_ADD:
            {
                iRet = addTraceStrategy(pTracePolicy);
            }
            break;

        // ����״̬Ϊ�޸ģ�����map�в��ң�����ҵ���ˢ�£��������һ������
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

    // ����ɸѡ¼����ٲ���
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
 �� �� ��  : sampleRecordQuality
 ��������  : �ɼ�¼������.��ʱ������ص���������ʱ�ɼ���ͷ��¼������
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::sampleRecordQuality()
{
    // �������û����Ч���򱾲ɼ����ڲ���Ҫ�ɼ�
    if (!m_bSampleFlag)
    {
        return SVS_RESULT_OK;
    }

    // �ɼ�¼����������
    SVS_RECORD_QUALITY_REPORT szReport;
    memset(&szReport, 0, sizeof(szReport));

    // �ɼ�¼��ͳ����Ϣ�����ʧ�ܣ����ô�ӡ��־����Ϊ���ٲ����ǹ㲥����ͷ��һ���ڱ�NRU¼��
    int32_t nRet = SVS_Stat_Manager::instance().sampleRecordQuality(m_strDeviceId, szReport);
    if(SVS_RESULT_OK != nRet)
    {
        // �޸�ͳ����ʾʱ��
        m_startSampleTime = time(NULL);

        return SVS_RESULT_OK;
    }

    time_t endSampleTime = time(NULL);
    // ����ǵ�һ���ɼ����ڣ���ʼʱ���޸�Ϊ��ǰʱ����ǰ��10s
    if (0 == m_startSampleTime)
    {
        m_startSampleTime = endSampleTime - 10;
    }

    uint32_t sampleTimeLen = (uint32_t)(endSampleTime - m_startSampleTime);

    // ���쾵ͷID���ϱ�ʱ�䡢������ʼ�ͽ���ʱ��
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

    // ����ƽ��ֵ
    if(0 != sampleTimeLen)
    {
        // ��λkbps
        szReport.CODE_RATE =
            (uint32_t)((szReport.TOTAL_RECV_SIZE * RTRECORD_UNIT_BYTE) / sampleTimeLen);

        szReport.FRAME_RATE = szReport.TOTAL_FRAME_RECV / sampleTimeLen;

        // ��֡�ʵ��㷨:��֡��*100/(��֡��+�յ�֡��),FRAME_LOSE_RATE��ȡ�����Ƕ�֡�ĸ��������Զ�֡���㷨����
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

    // ������ת����������
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
 �� �� ��  : getRecordQualityReports
 ��������  : ��ȡ¼�������������ݡ�
 �������  : NA
 �������  : RECORD_QUALITY_REPORT_VECTOR & szRecordReportVct:��ȡ¼�������б�
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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
 �� �� ��  : isTraceFinished
 ��������  : ¼�����������Ƿ��Ѿ���ɡ��������map�в��Ը����Ѿ�Ϊ0����������
             ����¼����������δ��ɡ�
 �������  : NA
 �������  : NA
 �� �� ֵ  : ¼��������ɷ���0,¼������δ��ɷ���-1
 �޸���ʷ  :
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
 �� �� ��  : increase_reference
 ��������  : ���ü�����������
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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
 �� �� ��  : decrease_reference
 ��������  : ���ü����ݼ�����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::decreaseReference()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_RESULT_FAILURE);
    m_ulRefCount--;

    SVS_LOG((SVS_LM_DEBUG,
        "[CSL]SVS_Record_Trace_Strategy[%s] decrease_reference=%d.",
        m_strDeviceId,
        m_ulRefCount));

    // ���Ӧ�ü���Ϊ0����ʾ�ޱ����ã����߳̿��԰Ѷ����״̬�ó�ֹͣ״̬
    if( 0 == m_ulRefCount)
    {
        //����ɾ��
        return 1;
    }

    return SVS_RESULT_OK;
}
const char* SVS_Record_Trace_Strategy::deviceId()const
{
    return m_strDeviceId;
}

//====================================˽�к���==================================
/*****************************************************************************
�� �� ��  : del_trace_strategy
��������  : ���¼����ٲ��ԡ�������map�в��ң��Ƿ�����Ѿ����ڣ�
            ��������ڣ������һ���µĲ���
            ����Ѿ����ڣ���Ѳ������ݸ���Ϊ�µĲ������ݣ�
�������  : SVS_RECORD_TRACE_POLICY* pTracePolicy:��������
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
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
    // ���ҵ����Զ���
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

    // ����������Ͳ���7*24�ģ��ж�ʱ��ĺϷ���
    if(SVS_RECORD_TRACE_POLICY_ALWAYS_TRACE != pTracePolicy->IS_ALLTIME)
    {
        // �������ʱ��β����� ������Ӳ���
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

        // ��Ӳ���,���ʱ������ˣ�������Ӳ���
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

    // �������Զ���
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

    // �Ѳ������ݱ��棬���Ѳ��Զ�����뵽map��
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

// ����¼����ٲ���
int32_t SVS_Record_Trace_Strategy::modTraceStrategy(const SVS_RECORD_TRACE_POLICY * pTracePolicy)
{
    char strStrategyId[SVS_RECORD_TRACE_POLICY_ID_LEN + 1] = {0,};
    memcpy(strStrategyId, pTracePolicy->POLICY_SEQ, sizeof(pTracePolicy->POLICY_SEQ));

    char strStartTime[TIME_STRING_LEN + 1] = {0,};
    memcpy(strStartTime, pTracePolicy->BEGIN_TIME, sizeof(pTracePolicy->BEGIN_TIME));

    char strEndTime[TIME_STRING_LEN + 1] = {0,};
    memcpy(strEndTime, pTracePolicy->END_TIME, sizeof(pTracePolicy->END_TIME));

    // ���ҵ����Զ���
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

    // ����������Ͳ���7*24�ģ��ж�ʱ��ĺϷ���
    if(SVS_RECORD_TRACE_POLICY_ALWAYS_TRACE != pTracePolicy->IS_ALLTIME)
    {
        // �������ʱ��β����� ����ɾ���ɵĲ���
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

        // ��Ӳ���,���ʱ������ˣ���ɾ���ò���
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

    // �Ѳ��Ը���
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
�� �� ��  : del_trace_strategy
��������  : ɾ��¼����ٲ��ԡ�
�������  : const char* strStrategyId:����ID
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
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
    // ���ҵ����Զ���
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

    // ���Ͳ������֪ͨ��Ϣ
    sendStrategyDoneNotifyMsg(strStrategyId);

    // ��map���Ƴ����Զ���
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
        // �ͷŲ��Զ�����ڴ�
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
�� �� ��  : select_trace_strategy
��������  : ��������map������ɸѡ���ԣ��������ò��Զ�ʱ����
�������  : NA
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t SVS_Record_Trace_Strategy::selectTraceStrategy()
{
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to select trace strategy.CameraId[%s].",
        m_strDeviceId));

    SVS_RECORD_TRACE_POLICY * pTracePolicy = NULL;
    time_t timerTime = 0; // ��ʱ��ʱ��
    time_t policyStartTime = 0;
    time_t policyEndTime = 0;
    time_t currTime = time(NULL);
    timerTime = currTime + SECONDS_PER_DAY; // ����һ���ϵĴ�ֵ����ǰʱ���1����
    bool bOldSampleFlag = m_bSampleFlag;
    m_bSampleFlag = false;
    char strStrategyId[SVS_RECORD_TRACE_POLICY_ID_LEN + 1] = {0,};
    char strStartTime[TIME_STRING_LEN + 1] = {0,};
    char strEndTime[TIME_STRING_LEN + 1] = {0,};
    std::list<SVS_RECORD_TRACE_POLICY *> strategyList;

    // �������в��ԣ��ҵ���ǰ��Ч�Ĳ���
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

        // ���������7*24¼����٣��������ö�ʱ��
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

        // ������Թ��ڣ��������ò���,���ҰѲ��Լ���list������ɾ���ò���
        if(currTime >= policyEndTime)
        {
            strategyList.push_back(pTracePolicy);

            continue;
        }

        // ��ǰʱ��ֻҪ�����κ�һ������ʱ����У���͵��ϱ�¼������
        if ((currTime >= policyStartTime) && (currTime < policyEndTime))
        {
            m_bSampleFlag = true;
        }

        // ������԰�����ǰʱ�䣬�ҽ���ʱ��ȶ�ʱ��ʱ���磬���������ö�ʱ��ʱ��
        if((policyStartTime <= currTime) && (policyEndTime < timerTime))
        {
            timerTime = policyEndTime;
        }
        // ������Ի�û��ʱ�䣬�ҿ�ʼʱ��ȶ�ʱ��ʱ���磬���������ö�ʱ��ʱ��
        else if((policyStartTime > currTime) && (policyStartTime < timerTime))
        {
            timerTime = policyStartTime;
        }
    }

    // �������ö�ʱ��
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

    // �ѹ�ȥ��¼���map��ɾ��
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

        // ���Ͳ������֪ͨ��Ϣ
        sendStrategyDoneNotifyMsg(strStrategyId);

        // unbind���Զ���
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

        // �ͷŲ��Զ���
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
�� �� ��  : sendStrategyDoneNotifyMsg
��������  : ��¼����ٲ�����ɺ�(������ʱ���)�����Ͳ������֪ͨ��Ϣ������̨��
�������  : const char* strStrategyId:¼����ٲ���ID
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
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

    // ����¼�������������֪ͨ��Ϣ
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

    // ��װ��Ϣ
    NRU_RECORD_QUALITY_DONE_REQ_MSG* pSendDoneMsg = (NRU_RECORD_QUALITY_DONE_REQ_MSG*)(void*)mb->base();
    mb->wr_ptr( ulMbSize);

    memset(pSendDoneMsg, 0, sizeof(NRU_RECORD_QUALITY_DONE_REQ_MSG));

    FillCommonHeader(   pSendDoneMsg->Header,
                        NRU_RECORD_QUALITY_DONE_REQ,
                        CAC_RT_Record_Server::instance()->transaction_no(),
                        (uint16_t)ulMbSize);

    memcpy(pSendDoneMsg->POLICY_ID, strStrategyId, sizeof(pSendDoneMsg->POLICY_ID));

    // ������Ϣ����Ϣ���Ͷ���
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
�� �� ��  : resetSampleInfo
��������  : ���ԭ�����ٲ���û����Ч�����ڸ��ٲ��Ըո���Ч�����statԭ�������ݳ�ʼ��
�������  : bool bOldSampleFlag:�ɵĲ������
            bool bNewSampleFlag:�µĲ������
�������  : NA
�� �� ֵ  : NA
�޸���ʷ  :
*****************************************************************************/
void SVS_Record_Trace_Strategy::resetSampleInfo(bool bOldSampleFlag, bool bNewSampleFlag)
{
    // ���ԭ������Ҫ¼����ٵģ���ǰ�ֲ���Ҫ�����ˣ���Ҫ�����ɼ�һ��¼������
    if ((bOldSampleFlag) && (!m_bSampleFlag))
    {
        m_bSampleFlag = true;
        (void)sampleRecordQuality();
        m_bSampleFlag = false;
    }

    // ԭ������Ҫ������������Ҫ����������Ҫ�Ѳ���ʱ�临λ��ͬʱ�������ò�������
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
    // ¼����������Զ�ʱ��������
    m_lSampleTimer = -1;
    m_ulSampleInterval = 0;

    // ¼�������ϱ������Զ�ʱ��������
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

//====================================���к���==================================
/*****************************************************************************
�� �� ��  : sync_record_trace_strategy
��������  : ͬ����ϵͳ���ص�¼����ٲ��ԺͿ���̨�Ĳ��Ա���һ�¡�
            �������Ͱ�����ӡ��޸ĺ�ɾ�����ڴ����κ�һ������ʧ�ܵ�ʱ�򣬼�������
            ��һ�����ԣ����ǰ�ʧ�ܷ��ظ�����̨��
�������  : const ACE_Message_Block* mb:���Կ���̨��¼����ٲ������ݣ����100
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
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
    // ������Ϣ��
    RECORD_QUALITY_POLICY_SYN_REQ_MSG* pMsg = (RECORD_QUALITY_POLICY_SYN_REQ_MSG*)(void*)mb->base();

    // ������ת����������
    pMsg->POLICY_COUNT = ntohl(pMsg->POLICY_COUNT);
    uint32_t ulSampleInterval = ntohl(pMsg->POLICY_LIST[0].SMAPLE_INTERVAL);
    uint32_t ulReportInterval = ntohl(pMsg->POLICY_LIST[0].REPORT_INTERVAL);
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to Sync record trace strategy. "
        "Strategy number[%u], sample interval[%u], report interval[%u].",
        pMsg->POLICY_COUNT,
        ulSampleInterval,
        ulReportInterval));

    // ����������ϱ���ʱ��
    int32_t iRet = resetTraceInterval(ulSampleInterval, ulReportInterval);
    if(SVS_RESULT_OK != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[CSL]Sync record trace stracetegy failed.Reset trace interval failed."));

        ulRecordErrCode = SVS_ERR_OTERERR;
    }

    // ����ÿ�����ԣ��ҵ���Ӧ�ľ�ͷ���ٲ��Զ��������Ĳ�������
    SVS_RECORD_TRACE_POLICY* pStrategy = NULL;
    char strDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0,};
    SVS_Record_Trace_Strategy * pTraceDevice = NULL;
    for(uint32_t i = 0; i < pMsg->POLICY_COUNT; i++)
    {
        pStrategy = &pMsg->POLICY_LIST[i];

        // ������ת������
        pStrategy->POLICY_STATE = ntohl(pStrategy->POLICY_STATE);
        pStrategy->IS_ALLTIME = ntohl(pStrategy->IS_ALLTIME);
        pStrategy->IS_ANALYSE_FILE = ntohl(pStrategy->IS_ANALYSE_FILE);

        //1 TODO [ʵʱ¼����������]USֻ����ʵʱ����
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

        // ���Ҳ��Զ�Ӧ�ľ�ͷ����
        pTraceDevice = getDevice(strDeviceId);

        // ���û���ҵ��豸�����һ�����Զ�Ӧ���豸
        if(NULL == pTraceDevice)
        {
            //����¼���豸
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

        // ���²�������
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

    // �������ͬ����Ӧ��Ϣ
    uint32_t respMsgLen = sizeof(RECORD_QUALITY_POLICY_SYN_RESP_MSG);
    ACE_Message_Block* respMb = allocate_mb(respMsgLen);

    if(NULL == respMb)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "[CSL]Create Record trace strategy sync response message to Console failed "
            "as allocate mb failed."));

        return SVS_RESULT_FAILURE;
    }

    // ��Ϣ��
    uint16_t msgNo = transactionno_respond(pMsg->Header.TransactionNo);

    // ���������Ӧ��Ϣ
    RECORD_QUALITY_POLICY_SYN_RESP_MSG *respMsg =
        (RECORD_QUALITY_POLICY_SYN_RESP_MSG *)(void *)respMb->base();
    respMb->wr_ptr( respMsgLen);

    memset(respMsg, 0x0, respMsgLen);
    FillCommonHeader(respMsg->Header,
                RECORD_QUALITY_POLICY_SYN_RESP,
                msgNo,
                (uint16_t)respMsgLen);
    respMsg->ERROR_CODE = htonl(ulRecordErrCode);

    // ¼����ٲ���ͬ����Ӧ��Ϣ�����
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
�� �� ��  : decrease_reference
��������  : ���ü����Զ���
�������  : SVS_Record_Trace_Strategy *pDevice:¼������豸����ͨ�����ü���
            �ж����û�б������ã����ͷŶ���
�������  : NA
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
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

    // �ͷ���Դ
    (void)pDevice->reset();
    SVS_DELETE(pDevice);

    return SVS_RESULT_OK;
}

// ��ʱ���ص�����
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
�� �� ��  : onTimer
��������  : ����ʱ������
�������  : int32_t nEventId:�ڲ��¼�ID��
            int32_t nDummy:��ʱ���������͡�
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
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
        // ����ǲ��Զ�ʱ���������¼����ٶ��󣬴�����Ե�������
        case SVS_TRACE_TIMER_TYPE_STRATEGY:
            {
                SVS_Record_Trace_Strategy* pTraceDevice = getDevice((uint32_t)nEventId);
                if (NULL != pTraceDevice)
                {
                    // ִ�о�ͷ�Ļص�����
                    SVS_TRACE_DEVICE_AUTO_PTR ptr(this, pTraceDevice);
                    iRet = pTraceDevice->selectTraceStrategy();
                }
            }
            break;

        // �����¼�������ɼ���ʱ������������¼��ɼ�
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

//====================================˽�к���==================================
/*****************************************************************************
�� �� ��  : sampleRecordQuality
��������  : ¼�������ɼ��������������ڽ���¼��ɼ����豸���������ռ�����������
            �Ĳ������ݡ�
�������  : NA
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::sampleRecordQuality()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_deviceMapMutex, SVS_RESULT_FAILURE);
    uint32_t ulTraceDevNum = m_deviceMap.current_size();
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to sample record quality.trace device number[%u]",
        ulTraceDevNum));

    SVS_Record_Trace_Strategy* pTraceDevice = NULL;
    // �������вɼ�����
    SVS_TRACE_DEVICE_MAP_ITER iter(m_deviceMap);
    SVS_TRACE_DEVICE_MAP_ENTRY *pEntry = NULL;
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pTraceDevice = pEntry->int_id_;

        // �ɼ��þ�ͷ��¼������
        (void)pTraceDevice->sampleRecordQuality();
    }

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Finish to sample record quality.trace device number[%u]",
        ulTraceDevNum));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : reportRecordQuality
��������  : ¼�������ϱ��������������ڽ���¼��ɼ����豸���������ռ�����������
            �Ĳ������ݡ�
�������  : NA
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::reportRecordQuality()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_deviceMapMutex, SVS_RESULT_FAILURE);
    uint32_t ulTraceDevNum = m_deviceMap.current_size();
    SVS_LOG((SVS_LM_INFO,
        "[CSL]Begin to report record quality.trace device number[%u]",
        ulTraceDevNum));

    // �ɼ��豸list�������ռ��Ѿ���ɲɼ��ľ�ͷ����
    std::list<SVS_Record_Trace_Strategy *> traceDevList;

    SVS_Record_Trace_Strategy* pTraceDevice = NULL;
    RECORD_QUALITY_REPORT_VECTOR szRecordReportVct;

    // �������вɼ�����
    SVS_TRACE_DEVICE_MAP_ITER iter(m_deviceMap);
    SVS_TRACE_DEVICE_MAP_ENTRY *pEntry = NULL;
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pTraceDevice = pEntry->int_id_;

        // �����ͷ��¼��ɼ�������ɣ������еĲ����Ѿ����ڣ����ռ��þ�ͷ���󣬺���ɾ��
        if(SVS_RESULT_OK == pTraceDevice->isTraceFinished())
        {
            traceDevList.push_back(pTraceDevice);
        }

        // ��ȡ��ͷ�ı��ϱ����ڵĲ���������
        szRecordReportVct.clear();
        (void)pTraceDevice->getRecordQualityReports(szRecordReportVct);

        // ����¼����������
        sendRecordQuality(szRecordReportVct);
    }

    SVS_LOG((SVS_LM_INFO,
        "[CSL]Finish to sample record quality.trace device number[%u]",
        ulTraceDevNum));

    // �ͷ��Ѿ�û�в��Ե�¼��ɼ���ͷ����
    while(!traceDevList.empty())
    {
        pTraceDevice = traceDevList.front();
        traceDevList.pop_front();
        SVS_TRACE_DEVICE_AUTO_PTR ptr(this, pTraceDevice);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : reset_trace_interval
��������  : ����������ڱ仯�ˣ����������ò�����ʱ��������ձ����ڵ����ݣ�
            ����ϱ����ڱ仯�ˣ������������ϱ���ʱ����
�������  : uint32_t sampleInterval:�µĲ�������
            uint32_t reportInterval:�µ��ϱ�����
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::resetTraceInterval(uint32_t ulSampleInterval, uint32_t ulReportInterval)
{
    // ����������ڱ仯�����߲�����ʱ��Ϊ��Ч�����������ò�����ʱ��
    if ((-1 >= m_lSampleTimer) || (m_ulSampleInterval != ulSampleInterval))
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Sample interval is changed."
            "old sample interval[%u], new sample interval[%u], old sample timer[%d].",
            m_ulSampleInterval,
            ulSampleInterval,
            m_lSampleTimer));

        // �ͷ��ϵĶ�ʱ��
        if(m_lSampleTimer > -1)
        {
            (void)CAC_Timer_Manager::instance()->free_timer(m_lSampleTimer);
        }

        // �����µĶ�ʱ��
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

    // ����ϱ����ڱ仯�������ϱ���ʱ��Ϊ��Ч�������������ϱ���ʱ��
    if ((-1 >= m_lReportTimer) || (m_ulReportInterval != ulReportInterval))
    {
        SVS_LOG((SVS_LM_WARNING,
            "[CSL]Record trace Report interval is changed."
            "old report interval[%u], new report interval[%u], old report timer[%d].",
            m_ulReportInterval,
            ulReportInterval,
            m_lReportTimer));

        // �ͷ��ϵĶ�ʱ��
        if(m_lReportTimer> -1)
        {
            (void)CAC_Timer_Manager::instance()->free_timer(m_lReportTimer);
        }

        // �����µĶ�ʱ��
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
�� �� ��  : sendRecordQuality
��������  : ����¼���������档ÿ��ֻ����һ����ͷ��¼��ɼ����ݣ����ֻ�ܷ���100
            ���ɼ���Ϣ��������100����ʱ�򣬷ֶ������͡�
            �ֶ�η���
�������  : RECORD_QUALITY_REPORT_VECTOR szRecordReportVct:¼��ɼ��������б�
�������  : NA
�� �� ֵ  : NA
�޸���ʷ  :
*****************************************************************************/
void SVS_Record_Trace_Manager::sendRecordQuality(RECORD_QUALITY_REPORT_VECTOR &szRecordReportVct)const
{
    uint32_t ulVctSize = szRecordReportVct.size();
    // ���û��������Ҫ�ϱ�
    if(0 == ulVctSize)
    {
        return;
    }

    char strDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0,};
    memcpy(strDeviceId, szRecordReportVct[0].LENS_ID, sizeof(szRecordReportVct[0].LENS_ID));

    uint32_t ulStartIndex = 0;
    uint32_t ulEndIndex = 0;
    // ÿ����෢��100��¼�������ɼ�����
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

        // ������Ϣ����
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

        // ����¼����������
        if(SVS_RESULT_OK !=  SVS_Console_Server::instance()->sendMsg(mb))
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Send record quality failed.Send message failed."
                "cameraId[%s], vector ulStartIndex[%u], vector ulEndIndex[%u].",
                strDeviceId,
                ulStartIndex,
                ulEndIndex));

            // �ͷ��ڴ�
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


// �����豸ID��ȡ�豸����
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

    //���ü�����1
    int32_t increaseResult = pDevice->increaseReference();
    if( SVS_RESULT_OK != increaseResult )
    {
        pDevice = NULL;
        return NULL;
    }

    return pDevice;
}

// �����豸�����ڲ�ID,��ȡ�豸����
SVS_Record_Trace_Strategy* SVS_Record_Trace_Manager::getDevice(uint32_t ulInternalId)
{
    SVS_Record_Trace_Strategy *pTraceDevice = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, m_deviceMapMutex, NULL);

    int32_t findResult = m_deviceTnternalIdMap.find(ulInternalId, pTraceDevice);

    if ( 0 != findResult )
    {
        return NULL;
    }

    //���ü�����1
    int32_t increaseResult = pTraceDevice->increaseReference();
    if( SVS_RESULT_OK != increaseResult )
    {
        pTraceDevice = NULL;
        return NULL;
    }

    return pTraceDevice;
}


/*****************************************************************************
�� �� ��  : add_device
��������  : ���¼����پ�ͷ����.
�������  : const char * strLensId:��ͷID
�������  : SVS_Record_Trace_Strategy*& pTraceDevice:¼����پ�ͷ����ָ���ַ
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t SVS_Record_Trace_Manager::addDevice(const char* strDeviceId,
                                       SVS_Record_Trace_Strategy*& pTraceDevice)
{
    // �����ж�
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

    // ������map�в��ң������ظ����������
    (void)m_deviceMap.find(strDeviceId, pTmpDevice);
    if (NULL == pTmpDevice)
    {
        //��ȡ�ڲ�����
        uint32_t internalId = nextInternalId();

        if (0 == internalId)
        {
            SVS_LOG((SVS_LM_ERROR,
                "[CSL]Fail to allocate an internal id for record trace object. camera[%s].",
                strDeviceId));

            return SVS_ERR_OTERERR;
        }

        // ����¼����ٶ���
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

        // ����map
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

        // �����ݲ�id������Ӧmap
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
    // ����map����map�н��ʱ���ͷ�
}//lint !e429

/*****************************************************************************
 �� �� ��  : next_internal_id
 ��������  : ������һ���ڲ�����ID
 �������  : ��
 �������  : ��
 �� �� ֵ  : ����ڲ�IDû�дﵽ���ֵ:��һ���ڲ�ID, ����ڲ�ID�ﵽ���ֵ:1
 �޸���ʷ  :
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


