#include "svs_index_resend_mgr.h"
#include "vms/vms.h"
#include "svs_log_msg.h"
#include "svs_stat_manager.h"
#include "svs_real_record_server.h"

CIndexResendMgr::CIndexResendMgr():m_iTimerId(-1)
{
}

CIndexResendMgr::~CIndexResendMgr()
{
    if( -1 < m_iTimerId)
    {
        (void)CAC_Timer_Manager::instance()->free_timer(m_iTimerId);
        m_iTimerId = -1;
    }
}

int32_t CIndexResendMgr::Init(CAC_Timer_Manager  *pTimerMgr)
{
    if (NULL == pTimerMgr)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Cannot init record index manager via timer manager is NULL."));
        return -1;
    }

    if (-1 != m_iTimerId)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Record index manager already init, timerId[%d].",
            m_iTimerId));
        return -1;
    }

    m_iTimerId = pTimerMgr->alloc_timer(this, TimeCallback,
                        CHECK_UNACKED_INDEX_EVENTID,
                        ACE_Time_Value(CHECK_UNACKED_INDEX_INTERVAL, 0),
                        ACE_Time_Value(CHECK_UNACKED_INDEX_INTERVAL, 0));

    if (-1 == m_iTimerId)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Alloc timer for record index manager failed."));
        return -1;
    }

    SVS_LOG((SVS_LM_INFO,
        "Alloc timer for record index manager success, timerId[%d].",
        m_iTimerId));
    return 0;
}

//����¼������Ψһ�ؼ���: 16���Ƶ�StartTime + DevId + NruId
void CIndexResendMgr::MakeIndexInfoKey(const Update_Record_Index *pInfo, string &strKey)
{
    //˽�к�����������������
    char szKey[INDEX_KEY_LEN] = {0};
    (void)snprintf(szKey, INDEX_KEY_LEN-1, "%08x_%s",
           (uint32_t)pInfo->StartTime, pInfo->LensId);
    strKey = szKey;
}

// ��¼�Ѿ������������������ڴ��У��ȴ���Ӧ��Ϣ�ĵ����������ڴ���ɾ��
int32_t CIndexResendMgr::RecordPastIndex(const Update_Record_Index *pIndex)
{
    if (NULL == pIndex)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Record past index failed via input parameter is NULL."));
        return -1;
    }

    string strIndexKey;
    MakeIndexInfoKey(pIndex, strIndexKey);

    ACE_Guard<ACE_Recursive_Thread_Mutex> _lock(m_mutex);

    // ���Ҷ�Ӧ���������½ṹ��
    RECORD_INDEX_ITER iter = m_indexMap.find(strIndexKey);
    PRecordIndexCacheNode pCacheNode = NULL;
    if (iter == m_indexMap.end())
    {
        //�����ڣ���������һ�����½ṹ�������뵽����
        pCacheNode = SVS_NEW(pCacheNode);
        if(NULL == pCacheNode)
        {
            SVS_LOG((SVS_LM_ERROR,
                "Record past index failed via allocate memory error, "
                "DeviceID[%s], IndexStartTime[%u], IndexEndTime[%u], "
                "FilePath[%s], VideoFlux[%Q]KB.",
                pIndex->LensId,
                pIndex->StartTime,
                pIndex->EndTime,
                pIndex->RecordFilePath,
                pIndex->VideoFlux));

            return -1;
        }

        (void)memcpy(&(pCacheNode->stIndexInfo), pIndex, sizeof(Update_Record_Index));
        // һ�����ط����������������ķ���ģʽ�����޸�Ϊ�ط�ģʽ
        pCacheNode->stIndexInfo.RecordReportType = REPORT_INDEX_TYPE_RECONNECT;
        // ��ʼʱ�����ط�����Ϊ0,�˼���ÿ�ط�һ�Σ�������һ��
        pCacheNode->uiResendTimes = 0;
        // ��¼�µ�ǰ��ʱ�䣬�Ե�ǰʱ��Ϊ׼��������һ�η��͵�ʱ��
        pCacheNode->ulSendTimeTick = time(NULL);
        // ��������������ط��б���
        m_indexMap.insert(make_pair(strIndexKey, pCacheNode));

        SVS_LOG((SVS_LM_DEBUG,
            "Record past index success, "
            "DeviceID[%s], IndexStartTime[%u], IndexEndTime[%u], "
            "FilePath[%s], VideoFlux[%Q]KB, key[%s].",
            pIndex->LensId,
            pIndex->StartTime,
            pIndex->EndTime,
            pIndex->RecordFilePath,
            pIndex->VideoFlux,
            strIndexKey.c_str()));
    }
    else
    {
        pCacheNode = iter->second;
        if (NULL == pCacheNode)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Record past index encounter unmeasured error, "
                "cache node exist for key %s, but cache node is NULL.",
                strIndexKey.c_str()));
            (void)m_indexMap.erase(strIndexKey);
            // ��ɾ���ɹ����ٴγ������
            return RecordPastIndex(pIndex);
        }
        //����Ѿ������ˣ���������ʱ���и��µ������������ڴ��еĽ���ʱ��
        if (pCacheNode->stIndexInfo.EndTime < pIndex->EndTime)
        {
            SVS_LOG((SVS_LM_DEBUG,
                "Reset past index for key %s, "
                "DeviceID[%s], IndexStartTime[%u], IndexEndTime[%u], "
                "FilePath[%s], VideoFlux[%Q]KB; "
                "And pre-saved past index info as follow, "
                "DeviceID[%s], IndexStartTime[%u], IndexEndTime[%u], "
                "FilePath[%s], VideoFlux[%Q]KB, "
                "ResendTimes[%d], resendTimetick[%u].",
                strIndexKey.c_str(),
                pIndex->LensId,
                pIndex->StartTime,
                pIndex->EndTime,
                pIndex->RecordFilePath,
                pIndex->VideoFlux,
                pCacheNode->stIndexInfo.LensId,
                pCacheNode->stIndexInfo.StartTime,
                pCacheNode->stIndexInfo.EndTime,
                pCacheNode->stIndexInfo.RecordFilePath,
                pCacheNode->stIndexInfo.VideoFlux,
                pCacheNode->uiResendTimes,
                pCacheNode->ulSendTimeTick));
            memcpy(&pCacheNode->stIndexInfo, pIndex, sizeof(Update_Record_Index));
            return 0;
        }

        SVS_LOG((SVS_LM_DEBUG,
            "Record past index cancel and keep the pre-saved value, "
            "key[%s], new past index info as follow, "
            "DeviceID[%s], IndexStartTime[%u], IndexEndTime[%u], "
            "FilePath[%s], VideoFlux[%Q]KB; "
            "And pre-saved past index info as follow, "
            "DeviceID[%s], IndexStartTime[%u], IndexEndTime[%u], "
            "FilePath[%s], VideoFlux[%Q]KB, "
            "ResendTimes[%d], resendTimetick[%u].",
            strIndexKey.c_str(),
            pIndex->LensId,
            pIndex->StartTime,
            pIndex->EndTime,
            pIndex->RecordFilePath,
            pIndex->VideoFlux,
            pCacheNode->stIndexInfo.LensId,
            pCacheNode->stIndexInfo.StartTime,
            pCacheNode->stIndexInfo.EndTime,
            pCacheNode->stIndexInfo.RecordFilePath,
            pCacheNode->stIndexInfo.VideoFlux,
            pCacheNode->uiResendTimes,
            pCacheNode->ulSendTimeTick));
    }

    return 0;
}//lint !e429 pCacheNode already saved in m_indexMap

// �����SCC���ص������ϱ���Ӧ��Ϣ
int32_t CIndexResendMgr::HandleReportConfirm(const ACE_Message_Block *mb)
{
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Handle index report confirm failed via mb is NULL."));
        return -1;
    }

    // ����Ϣ���Ƚ���У�飬������������Ϣ�������ṹ��
    const SVS_STORESVR_RECORD_INDEX_INFO_RESP_MSG *pReqMsg =
            (const SVS_STORESVR_RECORD_INDEX_INFO_RESP_MSG *)(void*)mb->rd_ptr();
    if (pReqMsg->Header.PacketLength
        < sizeof(SVS_STORESVR_RECORD_INDEX_INFO_RESP_MSG))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Index report confirm message length invalid, "
            "packetLen[%d], fixLen[%d].",
            pReqMsg->Header.PacketLength,
            sizeof(SVS_STORESVR_RECORD_INDEX_INFO_RESP_MSG)));
        return -1;
    }

    Update_Record_Index stIndex;
    memset(&stIndex, 0x0, sizeof(stIndex));
    SetLocalizeDeviceId((uint8_t *)stIndex.LensId,
                        sizeof(stIndex.LensId),
                        (const char *)pReqMsg->DeviceID,
                        DEVICEID_LEN);
    stIndex.StartTime = (time_t)pReqMsg->IndexStartTime;
    stIndex.EndTime = (time_t)pReqMsg->IndexEndTime;
    SVS_LOG((SVS_LM_INFO,
        "[MSG] Handle index report confirm message, trans[%d], "
        "camera[%s], startTime[%u], endTime[%u], respCode[%d].",
        pReqMsg->Header.TransactionNo,
        stIndex.LensId,
        stIndex.StartTime,
        stIndex.EndTime,
        pReqMsg->RespCode));

    string strIndexKey;
    MakeIndexInfoKey(&stIndex, strIndexKey);
    ACE_Guard<ACE_Recursive_Thread_Mutex> _lock(m_mutex);

    // ���Ҷ�Ӧ���������½ṹ��
    RECORD_INDEX_ITER iter = m_indexMap.find(strIndexKey);
    PRecordIndexCacheNode pCacheNode = NULL;
    if (iter == m_indexMap.end())
    {
        SVS_LOG((SVS_LM_WARNING,
            "Recv index report confirm message but not exists, "
            "key[%s], camera[%s], startTime[%u], endTime[%u], respCode[%d].",
            strIndexKey.c_str(),
            stIndex.LensId,
            stIndex.StartTime,
            stIndex.EndTime,
            pReqMsg->RespCode));
        return 0;
    }

    pCacheNode = iter->second;
    iter->second = NULL;

    SVS_LOG((SVS_LM_INFO,
        "Recv index report confirm message and delete from cache, "
        "key[%s], camera[%s], startTime[%u], endTime[%u], respCode[%d], "
        "resendTimes[%d], resendTimeticks[%u].",
        strIndexKey.c_str(),
        stIndex.LensId,
        stIndex.StartTime,
        stIndex.EndTime,
        pReqMsg->RespCode,
        (NULL==pCacheNode)?0:pCacheNode->uiResendTimes,
        (NULL==pCacheNode)?0:pCacheNode->ulSendTimeTick));

    // ��MAP���Ƴ�������������ǰ���Ѿ���������ֵ�ÿգ����Ժ��Գ�������
    (void)m_indexMap.erase(strIndexKey);
    SVS_DELETE(pCacheNode);

    return 0;
}

// ���δ���յ���Ӧ�Ѿ�����������
int32_t CIndexResendMgr::CheckUnackedIndex()
{
    SVS_LOG((SVS_LM_DEBUG, "Time check unacked index."));

    if (!CAC_RT_Record_Server::instance()->is_store_svr_conn())
    {
        SVS_LOG((SVS_LM_WARNING,
            "Time check unacked index cancel via disconnect with store."));
        return -1;
    }

    std::list<string> delIndexKeyList;
    std::list<string> resendIndexKeyList;
    time_t tCurrent = time(NULL);
    {
        ACE_Guard<ACE_Recursive_Thread_Mutex> _lock(m_mutex);
        RECORD_INDEX_ITER iter = m_indexMap.begin();
        for (; iter != m_indexMap.end(); iter++)
        {
            PRecordIndexCacheNode pCacheNode = iter->second;
            if (NULL == pCacheNode)
            {
                delIndexKeyList.push_back(iter->first);
                continue;
            }

            // �����ط���������ָ���������������ӻ�����ɾ��
            if (pCacheNode->uiResendTimes > UNACKED_INDEX_RESEND_TIMES)
            {
                delIndexKeyList.push_back(iter->first);
                continue;
            }

            // ��ʱ��Ҫ���·���
            if ((tCurrent - pCacheNode->ulSendTimeTick) > UNACKED_INDEX_RESEND_INTERVAL)
            {
                resendIndexKeyList.push_back(iter->first);
            }
        }
    }

    BatchDealIndex(delIndexKeyList, DELETE_INDEX);
    BatchDealIndex(resendIndexKeyList, RESEND_INDEX);
    return 0;
}

// ��ʱ�����Ҫ�ط����ϻ���¼�������Ļص���ʱ��
void CIndexResendMgr::TimeCallback(void *pArg, int32_t eventId, int32_t timerId, int32_t /*nDummy*/)
{
    SVS_TRACE();

    ACE_Message_Block *mb = g_p_msg_buffer->alloc_mb(
                            CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK,
                            sizeof(SVS_TIMER_CALL_BACK_MSG));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to allocate message block. msg type[%d], block size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK,
            sizeof(SVS_TIMER_CALL_BACK_MSG)));

        return;
    }

    SVS_TIMER_CALL_BACK_MSG* pMsg = (SVS_TIMER_CALL_BACK_MSG*)(void*)mb->wr_ptr();
    pMsg->MsgType = SVS_TIMER_CALL_BACK_MSG::UNACKED_INDEX_TIME_CHECK;
    pMsg->ProcObject = pArg;
    pMsg->EventId = eventId;
    pMsg->TimerId = timerId;
    mb->wr_ptr(sizeof(SVS_TIMER_CALL_BACK_MSG));

    // ���ص���Ϣ��ӵ���ʱ���ص�������Ϣ������
    (void)CAC_RT_Record_Server::instance()->add_timer_callback_msg( mb );
}

// �������б����ɾ�����ط��Ȳ���
void CIndexResendMgr::BatchDealIndex(std::list<string> indexKeyList, int32_t iFlag)
{
    std::list<string>::iterator iter = indexKeyList.begin();
    for (; iter != indexKeyList.end(); iter++)
    {
        PRecordIndexCacheNode pCacheNode = NULL;
        ACE_Guard<ACE_Recursive_Thread_Mutex> _lock(m_mutex);
        RECORD_INDEX_ITER iterIndex = m_indexMap.find(*iter);
        if (iterIndex != m_indexMap.end())
        {
            // ���MAP�е�ָ��Ϊ�գ���ɾ���ü�ֵ
            pCacheNode = iterIndex->second;
            if (NULL == pCacheNode)
            {
                m_indexMap.erase(*iter);
                continue;
            }

            // ���ָ��ɾ��������������ֱ�Ӵ�MAP��ɾ��
            if (iFlag == DELETE_INDEX)
            {
                iterIndex->second = NULL;
                m_indexMap.erase(*iter);
                SVS_DELETE(pCacheNode);
                continue;
            }

            // Ĭ�ϱ���Ϊ�ط������������ģʽ����Ϊ�Ƿ�
            if (iFlag != RESEND_INDEX)
            {
                continue;
            }

            // ���·��͸�������
            if (0 == ResendIndex(&pCacheNode->stIndexInfo))
            {
                pCacheNode->uiResendTimes++;
                pCacheNode->ulSendTimeTick = time(NULL);
            }
        }
    }
}

// �ط�����ָ����¼������
int32_t CIndexResendMgr::ResendIndex(Update_Record_Index *pIndex) const
{
    ACE_Message_Block *mb =  g_p_msg_buffer->alloc_mb(
                CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK,
                sizeof(Update_Record_Index));

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_DEBUG,
            "Resend record index failed viad allocate message block error, "
            "CameraId[%s], startTime[%u] endTime[%u].",
            pIndex->LensId,
            pIndex->StartTime,
            pIndex->EndTime));

        return -1;
    }

    //������ǰ¼���ļ��Ĵ���·��
    memcpy(mb->wr_ptr(), pIndex, sizeof(Update_Record_Index));
    mb->wr_ptr(sizeof(Update_Record_Index));

    CAC_RT_Record_Server *pServer = CAC_RT_Record_Server::instance();

    if( NULL == pServer )
    {
        SVS_LOG((SVS_LM_ERROR,
            "The sigleton of a class is NULL, the class name[CAC_RT_Record_Server]."));

        g_p_msg_buffer->free_mb( mb );

        return -1;
    }

    //¼��������Ϣ�����
    ACE_Time_Value tv(ACE_OS::gettimeofday());
    tv.set(tv.sec(), tv.usec() + SVS_ENQUEUE_RECORD_INDEX_TIMEOUT);

    if (-1 == pServer->enqueue_record_index( mb, &tv ) )
    {
        int32_t lastError = ACE_OS::last_error();

        SVS_LOG((SVS_LM_ERROR,
            "Resend record index failed via enqueue error, "
            "CameraId[%s], startTime[%u] endTime[%u].",
            pIndex->LensId,
            pIndex->StartTime,
            pIndex->EndTime));

        g_p_msg_buffer->free_mb(mb);

        // ���´򿪶���
        if ( EWOULDBLOCK != lastError )
        {
            (void)pServer->reopen_record_index_queue();
        }
        return -1;
    }

    return 0;
}

void CIndexResendMgr::DebugGetResendIndexInfo(char *pszBufferInfo,
                                                    const uint32_t uiBufferLen)
{
    memset(pszBufferInfo, 0x0, uiBufferLen);
    size_t iWriteLen = 0;
    int32_t   iLeaveLen = 0;

    ACE_Guard<ACE_Recursive_Thread_Mutex> _lock(m_mutex);
    RECORD_INDEX_ITER iter = m_indexMap.begin();
    int32_t     i = 0;
    for (; iter != m_indexMap.end(); iter++)
    {
        PRecordIndexCacheNode pCacheNode = iter->second;
        if (NULL == pCacheNode)
        {
            continue;
        }

        iWriteLen = ACE_OS::strlen(pszBufferInfo);
        iLeaveLen = (int32_t)(uiBufferLen - iWriteLen);
        if (0 >= iLeaveLen)
        {
            break;
        }
        i++;
        char szStartTime[SVS_STR_TIME_MAX_LEN];
        (void)time2string(szStartTime, sizeof(szStartTime), pCacheNode->stIndexInfo.StartTime);
        char szEndTime[SVS_STR_TIME_MAX_LEN];
        (void)time2string(szEndTime, sizeof(szEndTime), pCacheNode->stIndexInfo.EndTime);
        char szLastResendTime[SVS_STR_TIME_MAX_LEN];
        (void)time2string(szLastResendTime,
                        sizeof(szLastResendTime),
                        pCacheNode->ulSendTimeTick);
        (void)snprintf(&pszBufferInfo[iWriteLen],
                    (uint32_t)iLeaveLen,
                    "\n\tRecordIndex[%d] Camera[%s] TimeRange[%s~%s] Flux[%Q]KB LastSendTime[%s] ResendTimes[%d].",
                    i, pCacheNode->stIndexInfo.LensId,
                    szStartTime,
                    szEndTime,
                    pCacheNode->stIndexInfo.VideoFlux,
                    szLastResendTime,
                    pCacheNode->uiResendTimes);
    }
}

