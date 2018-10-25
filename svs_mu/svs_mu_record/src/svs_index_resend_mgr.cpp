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

//生成录像索引唯一关键字: 16进制的StartTime + DevId + NruId
void CIndexResendMgr::MakeIndexInfoKey(const Update_Record_Index *pInfo, string &strKey)
{
    //私有函数，不作参数检验
    char szKey[INDEX_KEY_LEN] = {0};
    (void)snprintf(szKey, INDEX_KEY_LEN-1, "%08x_%s",
           (uint32_t)pInfo->StartTime, pInfo->LensId);
    strKey = szKey;
}

// 记录已经结束的索引到本地内存中，等待响应消息的到来，并从内存中删除
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

    // 查找对应的索引更新结构体
    RECORD_INDEX_ITER iter = m_indexMap.find(strIndexKey);
    PRecordIndexCacheNode pCacheNode = NULL;
    if (iter == m_indexMap.end())
    {
        //不存在，则新生成一个更新结构，并插入到表中
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
        // 一旦入重发管理器，则索引的发送模式必须修改为重发模式
        pCacheNode->stIndexInfo.RecordReportType = REPORT_INDEX_TYPE_RECONNECT;
        // 初始时设置重发次数为0,此计数每重发一次，则自增一次
        pCacheNode->uiResendTimes = 0;
        // 记录下当前的时间，以当前时间为准，计算下一次发送的时间
        pCacheNode->ulSendTimeTick = time(NULL);
        // 保存该条索引导重发列表中
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
            // 在删除成功后，再次尝试添加
            return RecordPastIndex(pIndex);
        }
        //如果已经存在了，但是索引时间有更新的情况，则更新内存中的结束时间
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

// 处理从SCC返回的索引上报响应消息
int32_t CIndexResendMgr::HandleReportConfirm(const ACE_Message_Block *mb)
{
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Handle index report confirm failed via mb is NULL."));
        return -1;
    }

    // 对消息长度进行校验，并将索引的信息拷贝到结构体
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

    // 查找对应的索引更新结构体
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

    // 从MAP中移除该索引，由于前面已经将索引的值置空，可以忽略出错的情况
    (void)m_indexMap.erase(strIndexKey);
    SVS_DELETE(pCacheNode);

    return 0;
}

// 检测未接收到响应已经结束的索引
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

            // 对于重发次数大于指定次数的索引，从缓冲中删除
            if (pCacheNode->uiResendTimes > UNACKED_INDEX_RESEND_TIMES)
            {
                delIndexKeyList.push_back(iter->first);
                continue;
            }

            // 超时需要重新发送
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

// 定时检查需要重发及老化的录像索引的回调定时器
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

    // 将回调消息添加到定时器回调处理消息队列中
    (void)CAC_RT_Record_Server::instance()->add_timer_callback_msg( mb );
}

// 对索引列表进行删除，重发等操作
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
            // 如果MAP中的指针为空，则删除该键值
            pCacheNode = iterIndex->second;
            if (NULL == pCacheNode)
            {
                m_indexMap.erase(*iter);
                continue;
            }

            // 如果指定删除该条索引，则直接从MAP中删除
            if (iFlag == DELETE_INDEX)
            {
                iterIndex->second = NULL;
                m_indexMap.erase(*iter);
                SVS_DELETE(pCacheNode);
                continue;
            }

            // 默认必须为重发，如果是其它模式则认为非法
            if (iFlag != RESEND_INDEX)
            {
                continue;
            }

            // 重新发送该条索引
            if (0 == ResendIndex(&pCacheNode->stIndexInfo))
            {
                pCacheNode->uiResendTimes++;
                pCacheNode->ulSendTimeTick = time(NULL);
            }
        }
    }
}

// 重发发送指定的录像索引
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

    //拷贝当前录像文件的磁盘路径
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

    //录像索引消息入队列
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

        // 重新打开队列
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

