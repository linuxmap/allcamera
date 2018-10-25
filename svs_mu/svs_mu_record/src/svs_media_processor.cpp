#include <svs_ace_header.h>
#include "svs_stat_manager.h"
#include "svs_rt_record_common.h"
#include "svs_real_record_server.h"
#include "svs_recv_stream.h"

#include "svs_index_list.h"
#include "svs_file_egress.h"
#include "svs_media_processor.h"
#include "svs_daemon_thread.h"

SVS_Media_Processor::SVS_Media_Processor()
{
    m_bThreadControlFlag = true;
    m_ulThreadIndex       = 0;

    m_ulMaxHandleNum     = 0;
    m_HandleArray        = NULL;
}

SVS_Media_Processor::~SVS_Media_Processor()
{
    try
    {
        if(NULL != m_HandleArray)
        {
            SVS_DELETE(m_HandleArray, SVS_DELETE_MULTI);
        }
    }
    catch(...)
    {

    }
}

/*****************************************************************************
 �� �� ��  : open
 ��������  : ��ʼ��ý�崦����
 �������  : ��
 �� �� ֵ  : 0  �ɹ�
             -1  ʧ��
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2009��8��27��
    �޸�����   : �����ɺ���

*****************************************************************************/
int32_t SVS_Media_Processor::open(void *)
{
    // ��ȡϵͳ֧�ֵ����ͷ����Ԥ����Handle��Ϳ��������б�
    m_ulMaxHandleNum = CAC_RT_Record_Server::instance()->max_lens();
    m_HandleArray = SVS_NEW(m_HandleArray, m_ulMaxHandleNum);
    if (NULL == m_HandleArray)
    {
        SVS_LOG((SVS_LM_ERROR, "open media processor fail, alloc handle array[%d] fail.",
                   m_ulMaxHandleNum));
        return SVS_ERR_NOMEM;
    }

    for (uint32_t i = 0; i < m_ulMaxHandleNum; i++)
    {
        m_FreeIndexList.push_back(i);
        m_HandleArray[i].reset();
    }

    // �򿪶���
    (void)msg_queue()->close();
    if (0 != msg_queue()->open(SVS_IO_TASK_QUEUE_WM, SVS_IO_TASK_QUEUE_WM))
    {
        SVS_LOG( (SVS_LM_ERROR, "Open media processor fail.Fail to open message queue." ) );
        return SVS_ERR_QUEUE;
    }

    uint32_t ulBufSize = CAC_RT_Record_Server::instance()->recv_media_buffer_size();
    uint32_t ulBufNum  = CAC_RT_Record_Server::instance()->recv_media_buffer_num();
    ulBufNum += CAC_RT_Record_Server::instance()->tmpfs_recv_media_buffer_num();

    uint32_t ulWaterMark = ulBufSize * ulBufNum / PROC_MEDIA_THREAD_NUM;

    // ˮλ�������ͷ������ֹͣ��Ϣ��С
    ulWaterMark += (m_ulMaxHandleNum * sizeof(MediaEventMsg)) / PROC_MEDIA_THREAD_NUM;

    for (int32_t i = 0; i < PROC_MEDIA_THREAD_NUM; i++)
    {
        (void)m_MediaQueue[i].close();
        if (0 != m_MediaQueue[i].open(ulWaterMark, ulWaterMark))
        {
            SVS_LOG( (SVS_LM_ERROR, "Open media processor fail.Fail to open media queue." ) );
            return SVS_ERR_QUEUE;
        }
    }

    // ����ý�崦���߳�
    size_t stack_size[PROC_MEDIA_THREAD_NUM + PROC_MSG_THREAD_NUM] =
        { SVS_THREAD_STATCK_MAX, SVS_THREAD_STATCK_MAX,SVS_THREAD_STATCK_MAX};
    int32_t nRet = activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED,
                        PROC_MEDIA_THREAD_NUM + PROC_MSG_THREAD_NUM,
                        0,
                        ACE_DEFAULT_THREAD_PRIORITY,
                        -1,
                        0,
                        0,
                        0,
                        stack_size,
                        0 );
    if (0 != nRet)
    {
        SVS_LOG((SVS_LM_ERROR, "Activate media processor thread failed."));
        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

int32_t SVS_Media_Processor::svc(void)
{
    uint32_t ulThreadIndex = getThreadIndex();

    if (ulThreadIndex < PROC_MSG_THREAD_NUM)
    {
        procMessageThread();
    }
    else
    {
        procMediaThread(ulThreadIndex - PROC_MSG_THREAD_NUM);
    }

    SVS_LOG((SVS_LM_WARNING, "Media processor thread[%u] exit.", ulThreadIndex));
    return 0;
}

int32_t SVS_Media_Processor::close(u_long)
{
    m_bThreadControlFlag = false;
    (void)wait();
    SVS_LOG((SVS_LM_WARNING, "close media processor."));
    return 0;
}

/*****************************************************************************
 �� �� ��  : alloc_handle
 ��������  : �������,д¼�����
 �������  : ulIndex ����ɹ�������
 �� �� ֵ  : SVS_RESULT_OK  �ɹ�
             other          ʧ��
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2009��9��1��
    �޸�����   : �����ɺ���

*****************************************************************************/
int32_t SVS_Media_Processor::alloc_handle(const char* szDevID,
                                            int32_t in_type,
                                            int32_t out_type,
                                            uint32_t &ulIndex)
{
    if ((NULL == szDevID) || (NULL == m_HandleArray))
    {
        return SVS_ERR_PARAM;
    }

    // ���Ҹþ�ͷ�Ƿ��ظ�����
    {
        ACE_Write_Guard<ACE_RW_Mutex> locker(m_DevIndexMapMutex);
        DEV_INDEX_MAP_ITER iter = m_DevIndexMap.find(szDevID);
        while ((m_DevIndexMap.end() != iter) && (iter->first == szDevID))
        {
            Handle_Info *pHandleInfo = &m_HandleArray[iter->second];
            if ((in_type == pHandleInfo->m_nInType)
                && (out_type == pHandleInfo->m_nOutType))
            {
                return SVS_RESULT_FAILURE;
            }
            iter++;
        }
    }

    // ����һ�����о��
    int32_t nRet = getHandleIndex(ulIndex);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] alloc handle, get handle index fail.",
            szDevID));

        return SVS_RESULT_FAILURE;
    }

    // ��ʼ��HandleInfo�ṹ����Ҫ��������
    {
        Handle_Info *pHandle = m_HandleArray + ulIndex;

        ACE_Write_Guard<ACE_RW_Mutex> locker(pHandle->m_Mutex);

        //�豸ID����
        SetLocalizeDeviceId((uint8_t *)(pHandle->m_szDevID),
                            sizeof(pHandle->m_szDevID),
                            szDevID);

        pHandle->m_nInType     = in_type;
        pHandle->m_nOutType    = out_type;
        pHandle->m_Status      = HANDLE_STATUS_INIT;
        pHandle->m_StatusTime  = time(NULL);
        pHandle->m_pMediaQueue = &m_MediaQueue[ulIndex % PROC_MEDIA_THREAD_NUM];

        // �����������
        pHandle->m_pFileEgress = SVS_NEW(pHandle->m_pFileEgress);
        if (NULL == pHandle->m_pFileEgress)
        {
            pHandle->reset();
            freeHandleIndex(ulIndex);
            SVS_LOG((SVS_LM_WARNING, "camera[%s] alloc handle, create egress handle fail.",
                       szDevID));
            return SVS_RESULT_FAILURE;
        }

        // �����������
        pHandle->m_pRecvStream = CAC_Recv_Stream_Factory::instance().create();
        if (NULL == pHandle->m_pRecvStream)
        {
            SVS_DELETE(pHandle->m_pFileEgress);
            pHandle->reset();
            freeHandleIndex(ulIndex);
            SVS_LOG((SVS_LM_WARNING, "camera[%s] alloc handle, create recv handle fail.",
                       szDevID));
            return SVS_RESULT_FAILURE;
        }
    }

    // ���������ID������
    {
        ACE_Write_Guard<ACE_RW_Mutex> locker(m_DevIndexMapMutex);
        (void)m_DevIndexMap.insert(DEV_INDEX_MAP::value_type(szDevID, ulIndex));
    }

    SVS_LOG((SVS_LM_DEBUG, "camera[%s] alloc handle[%u] success.",
               szDevID, ulIndex));
    return SVS_RESULT_OK;
}

// ��ʼ����¼��
int32_t SVS_Media_Processor::start_record(uint32_t ulIndex,
                                            uint32_t ulRouteID,
                                            const ACE_INET_Addr &local_addr,
                                            const ACE_INET_Addr &remote_addr,
                                            time_t timeRemove)
{
    if (ulIndex >= m_ulMaxHandleNum)
    {
        SVS_LOG((SVS_LM_WARNING, "start record fail, index[%u] invalid.",
                   ulIndex));
        return SVS_ERR_PARAM;
    }

    MediaEventMsg msg;
    msg.eventType   = EVENT_START_RECORD;
    msg.ulIndex     = ulIndex;
    msg.ulStreamID  = ulRouteID;
    msg.localAddr   = local_addr;
    msg.RemoteAddr  = remote_addr;
    msg.timeRemove  = timeRemove;

    int32_t nRet = enqueue_message(msg);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING, "start record fail, index[%u].",
                   ulIndex));
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO, "start record success, index[%u].",
                   ulIndex));

    return SVS_RESULT_OK;
}

// ֹͣ¼��
int32_t SVS_Media_Processor::stop_record(uint32_t ulIndex)
{
    if (ulIndex >= m_ulMaxHandleNum)
    {
        SVS_LOG((SVS_LM_WARNING, "stop record fail, index[%u] invalid.",
                   ulIndex));
        return SVS_ERR_PARAM;
    }

    MediaEventMsg msg;
    msg.eventType   = EVENT_STOP_RECORD;
    msg.ulIndex     = ulIndex;

    int32_t nRet = enqueue_message(msg);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING, "stop record fail, index[%u].",
                   ulIndex));
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO, "stop record success, index[%u].",
                   ulIndex));

    return SVS_RESULT_OK;
}

// ��ͣ��֡¼�񣬴�������/ֹͣ�Ŀ���
int32_t SVS_Media_Processor::record_key_frame(uint32_t ulIndex, bool bSwitch)
{
    if (ulIndex >= m_ulMaxHandleNum)
    {
        SVS_LOG((SVS_LM_WARNING, "record key frame fail, index[%u] invalid.",
                   ulIndex));
        return SVS_ERR_PARAM;
    }

    MediaEventMsg msg;
    msg.eventType             = EVENT_RECORD_KEY_FRAME;
    msg.ulIndex               = ulIndex;
    msg.bSwitchKeyFrameRecord = bSwitch;

    int32_t nRet = enqueue_message(msg);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING, "record key frame fail, index[%u].",
                   ulIndex));
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_DEBUG, "record key frame success, index[%u].",
                   ulIndex));

    return SVS_RESULT_OK;
}


// ����ƽ̨��ط�¼��ƽ��ʱ��
int32_t SVS_Media_Processor::set_time_remove(uint32_t ulIndex, time_t remove_time)
{
    if (ulIndex >= m_ulMaxHandleNum)
    {
        SVS_LOG((SVS_LM_WARNING, "set time remove fail, index[%u] invalid.", ulIndex));

        return SVS_ERR_PARAM;
    }

    MediaEventMsg msg;
    msg.eventType             = EVENT_SET_TIME_REMOVE;
    msg.ulIndex               = ulIndex;
    msg.timeRemove            = remove_time;

    int32_t nRet = enqueue_message(msg);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING, "set time remove fail, index[%u].", ulIndex));

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_DEBUG, "set time remove success, index[%u].", ulIndex));

    return SVS_RESULT_OK;
}

// egressֹͣ��ɺ�֪ͨprocessor
int32_t SVS_Media_Processor::stop_finish_notify(uint32_t ulIndex)
{
    if (ulIndex >= m_ulMaxHandleNum)
    {
        SVS_LOG((SVS_LM_WARNING, "stop finish notify fail, index[%u] invalid.",
                   ulIndex));
        return SVS_ERR_PARAM;
    }

    MediaEventMsg msg;
    msg.eventType             = EVENT_STOP_FINISH_NOTIFY;
    msg.ulIndex               = ulIndex;

    int32_t nRet = enqueue_message(msg);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING, "stop finish notify fail, index[%u].",
                   ulIndex));
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_DEBUG, "stop finish notify success, index[%u].",
                   ulIndex));
    return SVS_RESULT_OK;
}

// ���������β���
int32_t SVS_Media_Processor::add_record_index(const uint32_t ulIndex,
                                                         const time_t indexStartTime,
                                                         const time_t indexStopTime)
{
    // �����ж�
    if ((ulIndex >= m_ulMaxHandleNum)
         || (NULL == m_HandleArray))
    {
        SVS_LOG((SVS_LM_WARNING,
            "start update index failed.The parameter is invalid.handle index[%d]",
            ulIndex));
        return SVS_ERR_PARAM;
    }

    Handle_Info   *pHandleInfo = &m_HandleArray[ulIndex];
    ACE_Read_Guard<ACE_RW_Mutex> locker(pHandleInfo->m_Mutex);

    if (NULL == pHandleInfo->m_pFileEgress)
    {
        SVS_LOG((SVS_LM_WARNING,
                 "start update index fail. "
                 "handle index[%d],handle status[%d], file egress[0x%08x]",
                 ulIndex,
                 pHandleInfo->m_Status,
                 pHandleInfo->m_pFileEgress));

        return SVS_RESULT_FAILURE;
    }

    return pHandleInfo->m_pFileEgress->add_index(indexStartTime, indexStopTime);
}

/*********** �ڲ�˽�к���ʵ�� ************/
int32_t SVS_Media_Processor::enqueue_message(const MediaEventMsg &msg)
{
    size_t msgLen = sizeof(msg);
    ACE_Message_Block *pMsg = allocMessage();
    if (NULL == pMsg)
    {
        SVS_LOG((SVS_LM_WARNING, "enqueue_message fail, alloc message failure."));
        return SVS_ERR_NOMEM;
    }

    MediaEventMsg *pEventMsg = (MediaEventMsg *)(void*)pMsg->base();
    (void)ACE_OS::memcpy(pEventMsg, (const char*)&msg, msgLen);
    pMsg->wr_ptr(msgLen);

    ACE_Time_Value tvDelay(0, 100 * 1000);
    ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;
    int32_t nRet = putq(pMsg, &tvTimeout);

    if (-1 == nRet)
    {
       SVS_LOG((SVS_LM_WARNING, "enqueue message fail, msgcount[%d]",
                  msg_queue()->message_count()));

       freeMessage(pMsg);
       return SVS_ERR_QUEUE;
    }

    return SVS_RESULT_OK;
}
/*****************************************************************************
 �� �� ��  : getHandleIndex
 ��������  : ��ȡһ�����е�����
 �������  : ulIndex ��ȡ��������
 �� �� ֵ  : SVS_RESULT_OK   �ɹ�
             other           ʧ��
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2009��9��1��
    �޸�����   : �����ɺ���

*****************************************************************************/
int32_t SVS_Media_Processor::getHandleIndex(uint32_t &ulIndex)
{
    ACE_Guard<ACE_Mutex> locker(m_FreeIndexMutex);
    if (m_FreeIndexList.empty())
    {
        return SVS_RESULT_FAILURE;
    }

    ulIndex = m_FreeIndexList.front();
    m_FreeIndexList.pop_front();

    if (ulIndex >= m_ulMaxHandleNum)
    {
        SVS_LOG((SVS_LM_WARNING, "get handle index fail, index[%u] invalid.",
                   ulIndex));
        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : freeHandleIndex
 ��������  : �ͷ�һ�����õ�����
 �������  : Ҫ�ͷŵ�����
 �� �� ֵ  : VOID
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2009��9��1��
    �޸�����   : �����ɺ���

*****************************************************************************/
void SVS_Media_Processor::freeHandleIndex(uint32_t ulIndex)
{
    ACE_Guard<ACE_Mutex> locker(m_FreeIndexMutex);
    m_FreeIndexList.push_back(ulIndex);
    return;
}

/*****************************************************************************
 �� �� ��  : allocMessage
 ��������  : ����һ��������Ϣ��
 �������  : VOID
 �� �� ֵ  : �������Ϣָ��
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2009��9��1��
    �޸�����   : �����ɺ���

*****************************************************************************/
ACE_Message_Block* SVS_Media_Processor::allocMessage()
{
    ACE_Message_Block* pMsg = NULL;
    ACE_Guard<ACE_Mutex> locker(m_FreeMsgListMutex);
    if (m_FreeMsgList.empty())
    {
        try
        {
            pMsg = new ACE_Message_Block(sizeof(MediaEventMsg));
        }
        catch(...)
        {
            return NULL;
        }
    }
    else
    {
        pMsg = m_FreeMsgList.front();
        m_FreeMsgList.pop_front();
    }

    //����ACE_Message_Block��release�ӿ��ͷſռ䣬����lint�澯
    return pMsg;
}//lint !e429

/*****************************************************************************
 �� �� ��  : freeMessage
 ��������  : ����ʹ�õ���Ϣ����������Ϣ�б�
 �������  : VOID
 �� �� ֵ  : VOID
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2009��9��1��
    �޸�����   : �����ɺ���

*****************************************************************************/
void SVS_Media_Processor::freeMessage(ACE_Message_Block *pMsg)
{
    if (NULL == pMsg)
    {
        return;
    }

    pMsg->reset();
    ACE_Guard<ACE_Mutex> locker(m_FreeMsgListMutex);
    m_FreeMsgList.push_back(pMsg);

    return;
}

/*****************************************************************************
 �� �� ��  : handle_message
 ��������  : ����ʹ�õ���Ϣ����������Ϣ�б�
 �������  : VOID
 �� �� ֵ  : VOID
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2009��9��1��
    �޸�����   : �����ɺ���

*****************************************************************************/
void SVS_Media_Processor::handle_message(ACE_Message_Block *pMsg)
{
    int32_t nRet = 0;
    MediaEventMsg *pEventMsg = (MediaEventMsg *)(void*)pMsg->base();
    uint32_t ulIndex = pEventMsg->ulIndex;
    if ((NULL == m_HandleArray) || (ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING,
            "handle message fail.handle index valid.ulIndex[%u],message type[%d],m_HandleArray[0x%08x]",
            ulIndex,
            pEventMsg->eventType,
            m_HandleArray));

        freeMessage(pMsg);

        return ;
    }

    Handle_Info   *pHandleInfo = &m_HandleArray[ulIndex];
    ACE_Write_Guard<ACE_RW_Mutex> locker(pHandleInfo->m_Mutex);

    SVS_LOG((SVS_LM_INFO, "Begin handle message, "
        "handle index [%u], m_pRecvStream[0x%08x], m_pFileEgress[0x%08x], "
        "event type[%d], status[%d], queue size[%u].",
        ulIndex,
        pHandleInfo->m_pRecvStream,
        pHandleInfo->m_pFileEgress,
        pEventMsg->eventType,
        pHandleInfo->m_Status,
        pHandleInfo->m_MsgQueue.message_count()));

    if ((HANDLE_STATUS_INVALID == pHandleInfo->m_Status)
        || (NULL == pHandleInfo->m_pRecvStream)
        || (NULL == pHandleInfo->m_pFileEgress))
    {
        SVS_LOG((SVS_LM_WARNING,
            "handle index[%u] message[%d] fail.m_Status[%d],m_pRecvStream[0x%08x],m_pFileEgress[0x%08x]",
            ulIndex,
            pEventMsg->eventType,
            pHandleInfo->m_Status,
            pHandleInfo->m_pRecvStream,
            pHandleInfo->m_pFileEgress));
        freeMessage(pMsg);

        return;
    }

    // ������Ϣ�������ﴦ��
    if (EVENT_RETRY == pEventMsg->eventType)
    {
        nRet = handle_retry_message(pEventMsg);
        if (SVS_RESULT_OK != nRet)
        {
            SVS_LOG((SVS_LM_WARNING, "handle[%u] process retry event fail.",
                       ulIndex));
        }
    }
    else
    {
        // ���������Ϣû�д�������ر������Ϣ֮���������Ϣ���ݴ������ȴ�����
        if ((0 < pHandleInfo->m_MsgQueue.message_count())
            && (EVENT_STOP_FINISH_NOTIFY != pEventMsg->eventType))
        {
            // ����������Ϣ��ֱ�Ӱ���Ϣ����ȴ�����β
            ACE_Time_Value tvDelay(0, 100 * 1000);
            ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;
            nRet = pHandleInfo->m_MsgQueue.enqueue_tail(pMsg, &tvTimeout);

            if (-1 == nRet)
            {
                SVS_LOG((SVS_LM_WARNING, "enqueue redo msgqueue fail, handle[%u] event[%d] msgcount[%d].",
                           ulIndex,
                           pEventMsg->eventType,
                           pHandleInfo->m_MsgQueue.message_count()));
                freeMessage(pMsg);
            }

            return;
        }
        else
        {
            // �����������Ϣ
            nRet = handle_stand_message(pEventMsg);
            if (SVS_RESULT_OK != nRet)
            {
                // ������Ϣʧ�ܣ���Ҫ����
                SVS_LOG((SVS_LM_WARNING, "handle[%u] will retry event[%d], message queue size[%d].",
                           ulIndex,
                           pEventMsg->eventType,
                           pHandleInfo->m_MsgQueue.message_count()));
                ACE_Time_Value tvDelay(0, 100 * 1000);
                ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;
                nRet = pHandleInfo->m_MsgQueue.enqueue_tail(pMsg, &tvTimeout);
                if (-1 == nRet)
                {
                    SVS_LOG((SVS_LM_WARNING, "enqueue redo msgqueue fail, handle[%u] event[%d].",
                               ulIndex, pEventMsg->eventType));
                    freeMessage(pMsg);

                    return;
                }

                // ����������Ϣ
                MediaEventMsg msg;
                msg.eventType = EVENT_RETRY;
                msg.ulIndex   = ulIndex;
                (void)enqueue_message(msg);
                return;
            }

            // ����ɹ���ת����󷵻�
        }
    }

    freeMessage(pMsg);

    return;
}

int32_t SVS_Media_Processor::handle_stand_message(const MediaEventMsg * pEventMsg)
{
    if ((NULL == pEventMsg)
        || (NULL == m_HandleArray)
        || (pEventMsg->ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING, "handle stand message fail, param invalid."));

        return SVS_RESULT_FAILURE;
    }

    uint32_t ulIndex    = pEventMsg->ulIndex;
    Handle_Info *pHandleInfo = &m_HandleArray[ulIndex];

    switch(pEventMsg->eventType)
    {
        case EVENT_START_RECORD:
        {
             return handleStartRecordMsg(pEventMsg);
        }

        case EVENT_STOP_RECORD:
        {
            return handleStopRecordMsg(pEventMsg);
        }

        case EVENT_RECORD_KEY_FRAME:
        {
            if (HANDLE_STATUS_START != pHandleInfo->m_Status)
            {
                SVS_LOG((SVS_LM_WARNING, "cameraId[%s] set record key frame fail, handle[%d] status[%d]",
                          pHandleInfo->m_szDevID,
                          ulIndex,
                          pHandleInfo->m_Status));

                return SVS_RESULT_FAILURE;
            }

            pHandleInfo->m_pRecvStream->record_key_frame(pEventMsg->bSwitchKeyFrameRecord);
        }
        break;

        case EVENT_SET_TIME_REMOVE:
        {
            if (HANDLE_STATUS_START != pHandleInfo->m_Status)
            {
                SVS_LOG((SVS_LM_WARNING, "cameraId[%s] set time remove fail, handle[%d] status[%d]",
                          pHandleInfo->m_szDevID,
                          ulIndex,
                          pHandleInfo->m_Status));

                return SVS_RESULT_FAILURE;
            }

            pHandleInfo->m_pRecvStream->set_time_remove(pEventMsg->timeRemove);
        }
        break;

        case EVENT_STOP_FINISH_NOTIFY:
        {
            return handleStopFinishNotifyMsg(pEventMsg);
        }

        case EVENT_RETRY:
            SVS_LOG((SVS_LM_WARNING,
                      "camera[%s:%u] stand message not contain EVENT_RETRY.",
                      pHandleInfo->m_szDevID,
                      pEventMsg->ulIndex));
        break;

        default:
            SVS_LOG((SVS_LM_ERROR,
                "camera[%s:%u] Handle event failed. event type is unknown.event type[%d]",
                pHandleInfo->m_szDevID,
                pEventMsg->ulIndex,
                pEventMsg->eventType));
            return SVS_RESULT_OK;
    }

    // ����ɹ�������Զ����л���δ�������Ϣ�����ٴ�����������Ϣ
    if (0 < pHandleInfo->m_MsgQueue.message_count())
    {
        MediaEventMsg msg;
        msg.eventType = EVENT_RETRY;
        msg.ulIndex   = ulIndex;
        (void)enqueue_message(msg);
    }

    return 0;
}

int32_t SVS_Media_Processor::handle_retry_message(const MediaEventMsg *pEventMsg)
{
    if ((NULL == pEventMsg)
        || (NULL == m_HandleArray)
        || (pEventMsg->ulIndex >= m_ulMaxHandleNum))
    {
        return SVS_RESULT_FAILURE;
    }

    int32_t nRet                 = 0;
    Handle_Info *pHandleInfo = &m_HandleArray[pEventMsg->ulIndex];

    // �������״̬����ֹͣ�У���ֱ�ӷ��أ�ֹͣ��ɺ���ٴ�����;����������Ϣ
    if (HANDLE_STATUS_STOP == pHandleInfo->m_Status)
    {
        SVS_LOG((SVS_LM_INFO,
            "Handle retry message failed.The handle stopping, so retry later.handle[%d]",
            pEventMsg->ulIndex));

        return SVS_RESULT_OK;
    }

    // ֹͣ�����Ѿ�������ɣ�ȡ��δ�������Ϣ��������
    ACE_Message_Block *pMsg = NULL;
    if (0 < pHandleInfo->m_MsgQueue.message_count())
    {
        ACE_Time_Value tvDelay(0, 100 * 1000);
        ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;
        nRet = pHandleInfo->m_MsgQueue.dequeue_head(pMsg, &tvTimeout);
        if ((-1 == nRet) || (NULL == pMsg))
        {
            SVS_LOG((SVS_LM_WARNING, "process handle[%u] retry message, dequeue message fail.",
                       pEventMsg->ulIndex));
            return SVS_RESULT_FAILURE;
        }

        // ����Ҫ���Ե���Ϣ
        MediaEventMsg *pEvent = (MediaEventMsg *)(void*)pMsg->base();
        nRet = handle_stand_message(pEvent);
        if (SVS_RESULT_OK != nRet)
        {
            // ������Ϣ����ʧ�ܺ��ٽ�������
            SVS_LOG((SVS_LM_WARNING, "process handle[%u] retry messsage fail, event[%d].",
                       pEvent->ulIndex,
                       pEvent->eventType));

            // ���������ʧ����Ϣ��Ϊ��֤������ֹͣ�������״̬
            if (EVENT_START_RECORD == pEvent->eventType)
            {
                // ����״̬
                pHandleInfo->m_Status = HANDLE_STATUS_START;
                pHandleInfo->m_StatusTime  = time(NULL);
            }

            // ���handle�Ķ��л����������ݣ��򴴽�������Ϣ
            if (0 < pHandleInfo->m_MsgQueue.message_count())
            {
                MediaEventMsg msg;
                msg.eventType = EVENT_RETRY;
                msg.ulIndex   = pEventMsg->ulIndex;
                (void)enqueue_message(msg);
            }
        }

        freeMessage(pMsg);
        return nRet;
    }

    return SVS_RESULT_OK;
}


int32_t SVS_Media_Processor::handleStartRecordMsg(const MediaEventMsg * pEventMsg)
{
    if ((NULL == pEventMsg)
        || (NULL == m_HandleArray)
        || (pEventMsg->ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING, "handle start record message fail, param invalid."));

        return SVS_RESULT_FAILURE;
    }

    uint32_t ulIndex    = pEventMsg->ulIndex;

    Handle_Info *pHandleInfo = &m_HandleArray[ulIndex];
    int32_t nRet                 = SVS_RESULT_OK;

    if (HANDLE_STATUS_INIT != pHandleInfo->m_Status)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] will retry process handle[%u] start record message, "
            "current status[%d].",
            pHandleInfo->m_szDevID,
            ulIndex,
            pHandleInfo->m_Status));

        return SVS_RESULT_FAILURE;
    }

    //��ʼ�������������
    nRet = pHandleInfo->m_pFileEgress->open(pHandleInfo->m_szDevID, ulIndex);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s:%u] start record, open egress handle fail.",
            pHandleInfo->m_szDevID,
            ulIndex));

        return SVS_RESULT_FAILURE;
    }

    nRet = pHandleInfo->m_pRecvStream->init(pHandleInfo->m_szDevID);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s:%u] start record, init recv handle fail.",
            pHandleInfo->m_szDevID,
            ulIndex));

        return SVS_RESULT_FAILURE;
    }

    //����ƽ̨¼��ƫ��
    pHandleInfo->m_pFileEgress->setTimeRemove(pEventMsg->timeRemove);
    pHandleInfo->m_pRecvStream->set_time_remove(pEventMsg->timeRemove);

    nRet = pHandleInfo->m_pRecvStream->start_recv(ulIndex,
                                               pHandleInfo->m_nInType,
                                               pEventMsg->ulStreamID,
                                               pEventMsg->localAddr,
                                               pEventMsg->RemoteAddr);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING, "camera[%s:%d] start recv fail.",
                pHandleInfo->m_szDevID,
                ulIndex));

        return SVS_RESULT_FAILURE;
    }

    //����״̬
    pHandleInfo->m_Status = HANDLE_STATUS_START;
    pHandleInfo->m_StatusTime  = time(NULL);

    //����ɹ�������Զ����л���δ�������Ϣ�����ٴ�����������Ϣ
    if (0 < pHandleInfo->m_MsgQueue.message_count())
    {
        MediaEventMsg msg;
        msg.eventType = EVENT_RETRY;
        msg.ulIndex   = ulIndex;
        (void)enqueue_message(msg);
    }

    return SVS_RESULT_OK;
}

int32_t SVS_Media_Processor::handleStopRecordMsg(const MediaEventMsg * pEventMsg)
{
    if ((NULL == pEventMsg)
        || (NULL == m_HandleArray)
        || (pEventMsg->ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING, "handle stop finish notigy message fail, param invalid."));
        return SVS_RESULT_FAILURE;
    }

    uint32_t ulIndex    = pEventMsg->ulIndex;
    Handle_Info *pHandleInfo = &m_HandleArray[ulIndex];

    if (HANDLE_STATUS_START != pHandleInfo->m_Status)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s:%u]will retry process stop record message,"
            "current status[%d].",
            pHandleInfo->m_szDevID,
            ulIndex,
            pHandleInfo->m_Status));

        return SVS_RESULT_FAILURE;
    }

    int32_t nRet = pHandleInfo->m_pRecvStream->stop_recv();
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] stop recv fail.handle[%d] status[%d]",
            pHandleInfo->m_szDevID,
            ulIndex,
            pHandleInfo->m_Status));

        return SVS_RESULT_FAILURE;
    }

    pHandleInfo->m_Status = HANDLE_STATUS_STOP;
    pHandleInfo->m_StatusTime  = time(NULL);

    // ����ɹ�������Զ����л���δ�������Ϣ�����ٴ�����������Ϣ
    if (0 < pHandleInfo->m_MsgQueue.message_count())
    {
        MediaEventMsg msg;
        msg.eventType = EVENT_RETRY;
        msg.ulIndex   = ulIndex;
        (void)enqueue_message(msg);
    }

    return SVS_RESULT_OK;
}

int32_t SVS_Media_Processor::handleStopFinishNotifyMsg(const MediaEventMsg * pEventMsg)
{
    if ((NULL == pEventMsg)
        || (NULL == m_HandleArray)
        || (pEventMsg->ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING, "handle stop finish notigy message fail, param invalid."));
        return SVS_RESULT_FAILURE;
    }

    uint32_t ulIndex    = pEventMsg->ulIndex;
    Handle_Info *pHandleInfo = &m_HandleArray[ulIndex];

    if (HANDLE_STATUS_STOP != pHandleInfo->m_Status)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s:%u] process stop finish notify fail, status[%d].",
            pHandleInfo->m_szDevID,
            ulIndex,
            pHandleInfo->m_Status));

        return SVS_RESULT_FAILURE;
    }

    // ֹͣ���,�ͷ�������Դ
    if (0 == --pHandleInfo->m_ulRefCount)
    {
        (void)CAC_Recv_Stream_Factory::instance().destroy(pHandleInfo->m_pRecvStream);
        SVS_DELETE(pHandleInfo->m_pFileEgress);

        // ɾ��DevID��Index��ӳ�䲢����������ͷ�
        {
            ACE_Write_Guard<ACE_RW_Mutex> Writelocker(m_DevIndexMapMutex);
            DEV_INDEX_MAP_ITER iter = m_DevIndexMap.find(pHandleInfo->m_szDevID);
            while ((m_DevIndexMap.end() != iter)
                    && (iter->first == pHandleInfo->m_szDevID))
            {
                if (iter->second == ulIndex)
                {
                    m_DevIndexMap.erase(iter);
                    break;
                }
                iter++;
            }
        }

        SVS_LOG((SVS_LM_INFO, "Camera[%s] stop finished, handle[%u].",
                   pHandleInfo->m_szDevID, ulIndex));
        pHandleInfo->reset();
        freeHandleIndex(ulIndex);

        return SVS_RESULT_OK;

    }
    else
    {
        pHandleInfo->m_Status  = HANDLE_STATUS_INIT;
        pHandleInfo->m_StatusTime  = time(NULL);
    }

    // ����ɹ�������Զ����л���δ�������Ϣ�����ٴ�����������Ϣ
    if (0 < pHandleInfo->m_MsgQueue.message_count())
    {
        MediaEventMsg msg;
        msg.eventType = EVENT_RETRY;
        msg.ulIndex   = ulIndex;
        (void)enqueue_message(msg);
    }

    return SVS_RESULT_OK;
}

void SVS_Media_Processor::procMessageThread()
{
    ACE_Message_Block *mb = NULL;
    ACE_Time_Value tvDelay(1);
    CThread_Stat_Reporter report("SVS_Media_Processor(MSG)");
    while ( m_bThreadControlFlag )
    {
        ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;
        // ȡ��ý��֪ͨ��Ϣ���д���
        if (-1 == getq(mb, &tvTimeout) )
        {
            report.ReportStat();
            continue;
        }

        // ����ý������
        if (NULL != mb)
        {
            handle_message(mb);
            report.ReportStat(1);
        }
    }

   SVS_LOG((SVS_LM_WARNING, "Media processor: proc message thread exit."));
}

void SVS_Media_Processor::procMediaThread(uint32_t queueIndex)
{
    ACE_Message_Block *mb = NULL;
    char szThreadName[MAXNAMELEN + 1] = {0};
    (void)ACE_OS::snprintf(szThreadName, MAXNAMELEN, "SVS_Media_Processor(MEDIA)[%u]", queueIndex);

    CThread_Stat_Reporter report(szThreadName);
    ACE_Time_Value tvDelay(1);
    while ( m_bThreadControlFlag )
    {
        ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;
        // ȡ��ý��֪ͨ��Ϣ���д���
        if (-1 == m_MediaQueue[queueIndex].dequeue_head(mb, &tvTimeout) )
        {
            report.ReportStat();
            continue;
        }

        // ������˳���Ϣ���رն�Ӧ��Egress
        if (ACE_Message_Block::MB_STOP == mb->msg_type())
        {
            uint32_t ulIndex = *(uint32_t*)(void*)mb->base();
            if ((NULL == m_HandleArray) || (ulIndex >= m_ulMaxHandleNum))
            {
                SVS_LOG((SVS_LM_WARNING,
                    "process stop message fail.handle index valid.ulIndex[%d],m_HandleArray[0x%08x]",
                    ulIndex,
                    m_HandleArray));
            }
            else
            {
                SVS_LOG((SVS_LM_INFO, "process index[%u] stop message.",
                           ulIndex));
                ACE_Read_Guard<ACE_RW_Mutex> locker(m_HandleArray[ulIndex].m_Mutex);
                if (NULL != m_HandleArray[ulIndex].m_pFileEgress)
                {
                    (void)m_HandleArray[ulIndex].m_pFileEgress->close();
                }
            }

            freeMessage(mb);
            report.ReportStat(1);
            continue;
        }

        //ý�����ݣ����ö�Ӧ��Egressд������
        int32_t nRet = SVS_RESULT_FAILURE;
        char *pBuffBase = (char*)mb->base();
        SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)pBuffBase;
        unsigned ulIndex = pRecvMediaHeader->HandlerIndex;

        if ((NULL == m_HandleArray) || (ulIndex >= m_ulMaxHandleNum))
        {
            SVS_LOG((SVS_LM_WARNING,
                      "process media fail.handle index valid.ulIndex[%d],m_HandleArray[0x%08x]",
                      ulIndex,
                      m_HandleArray));
        }
        else
        {
            // ����д��ʱ����Ҫ���Ӷ���
            ACE_Read_Guard<ACE_RW_Mutex> locker(m_HandleArray[ulIndex].m_Mutex);
            if (NULL != m_HandleArray[ulIndex].m_pFileEgress)
            {
                nRet = m_HandleArray[ulIndex].m_pFileEgress->write(mb);
            }
        }

        if (SVS_RESULT_OK != nRet)
        {
            SVS_LOG((SVS_LM_WARNING, "process handle[%u] media fail.", ulIndex));
            (void)mb->release();
        }
        report.ReportStat(1);
    }

    SVS_LOG((SVS_LM_WARNING, "Media processor: proc message thread exit."));
    return;
}

int32_t SVS_Media_Processor::write_media(uint32_t ulIndex, ACE_Message_Block* pMediaData)
{
    if ((NULL == m_HandleArray)
         || (ulIndex >= m_ulMaxHandleNum)
         || (NULL == pMediaData))
    {
        SVS_LOG((SVS_LM_WARNING,
            "process media fail, handle index valid, "
            "ulIndex[%d],m_HandleArray[0x%08x],pMediaData[0x%08x].",
            ulIndex,
            m_HandleArray,
            pMediaData));

        return SVS_ERR_PARAM;
    }

    // ������Ϣ����
    pMediaData->msg_type(ACE_Message_Block::MB_DATA);
    ACE_Read_Guard<ACE_RW_Mutex> locker(m_HandleArray[ulIndex].m_Mutex);

    if (NULL == m_HandleArray[ulIndex].m_pMediaQueue)
    {
        SVS_LOG((SVS_LM_WARNING,
            "index[%u] write media fail, media queue is NULL.",
            ulIndex));

        return SVS_ERR_PARAM;
    }

    ACE_Time_Value tvDelay(0, 100 * 1000);
    ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;
    int32_t nRet = m_HandleArray[ulIndex].m_pMediaQueue->enqueue_tail(pMediaData, &tvTimeout);

    if (-1 == nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "index[%u] write media fail, enqueue media queue fail.",
            ulIndex));

        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

int32_t SVS_Media_Processor::recv_close_notify(uint32_t ulIndex)
{
    if (ulIndex >= m_ulMaxHandleNum)
    {
        SVS_LOG((SVS_LM_WARNING, "recv close notify fail, index[%u] invalid.",
                   ulIndex));
        return SVS_ERR_PARAM;
    }

    ACE_Message_Block *pMsg = allocMessage();
    if (NULL == pMsg)
    {
        SVS_LOG((SVS_LM_WARNING, "index[%u] recv close notify fail, alloc message fail.",
                   ulIndex));
        return SVS_ERR_PARAM;
    }

    // �����˳���Ϣ
    pMsg->msg_type(ACE_Message_Block::MB_STOP);
    uint32_t *pIndex = (uint32_t *)(void*)pMsg->base();
    *pIndex = ulIndex;
    pMsg->wr_ptr(sizeof(uint32_t));

    // ���˳���Ϣ����ָ����ý�����
    {
        ACE_Read_Guard<ACE_RW_Mutex> locker(m_HandleArray[ulIndex].m_Mutex);
        if (NULL == m_HandleArray[ulIndex].m_pMediaQueue)
        {
            SVS_LOG((SVS_LM_WARNING, "index[%u] write media fail, media queue is NULL.",
                       ulIndex));
            freeMessage(pMsg);
            return SVS_ERR_PARAM;
        }

        ACE_Time_Value tvDelay(0, 100 * 1000);
        ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;
        int32_t nRet = m_HandleArray[ulIndex].m_pMediaQueue->enqueue_tail(pMsg, &tvTimeout);
        if (-1 == nRet)
        {
            SVS_LOG((SVS_LM_WARNING, "index[%u] write media fail, enqueue media queue fail.",
                       ulIndex));
            freeMessage(pMsg);
            return SVS_RESULT_FAILURE;
        }
    }

    SVS_LOG((SVS_LM_INFO, "index[%u] recv close notify success.",
              ulIndex));
    return SVS_RESULT_OK;
}

// ------״̬��ȡ��debug��stat�ȸ��ӽӿ�--------
int32_t SVS_Media_Processor::overwriteRecordNotify(const uint32_t ulIndex,
                                                    const time_t newIndexStartTime)
{
    // �����ж�
    if ((NULL == m_HandleArray) || (ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING,
            "notify overwrite record failed.The parameter is invalid."
            "ulIndex[%d],m_HandleArray[0x%08x]",
            ulIndex,
            m_HandleArray));
        return SVS_ERR_PARAM;
    }

    Handle_Info   *pHandleInfo = &m_HandleArray[ulIndex];
    ACE_Read_Guard<ACE_RW_Mutex> locker(pHandleInfo->m_Mutex);
    if ((HANDLE_STATUS_INVALID == pHandleInfo->m_Status)
        || (NULL == pHandleInfo->m_pFileEgress))
    {
        SVS_LOG((SVS_LM_WARNING,
                 "Notify overwrite record finished. "
                 "handle index[%d],handle status[%d], file egress[0x%08x]",
                 ulIndex,
                 pHandleInfo->m_Status,
                 pHandleInfo->m_pFileEgress));
        return SVS_RESULT_OK;
    }

    return pHandleInfo->m_pFileEgress->overwrite_record_notify(newIndexStartTime);
}

// ��ȡ���״̬������ʱ����OK���쳣ʱֱ�ӷ��ش�����
int32_t SVS_Media_Processor::get_handle_status(uint32_t ulIndex)
{
    if ((NULL == m_HandleArray) || (ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING,
            "get handle status fail.handle index valid.ulIndex[%d],m_HandleArray[0x%08x]",
            ulIndex,
            m_HandleArray));
        return SVS_ERR_PARAM;
    }

    Handle_Info *pHandleInfo = &m_HandleArray[ulIndex];

    ACE_Read_Guard<ACE_RW_Mutex> locker(pHandleInfo->m_Mutex);
    // �����û�д�������¼����Ϣ������δ��ʱ���򲻴���
    time_t curTime = ACE_OS::time(NULL);
    if((HANDLE_STATUS_INIT == pHandleInfo->m_Status)
    && (VTDU_SEND_DATA_MIN_TIMEOUT > (curTime - pHandleInfo->m_StatusTime)))
    {
        return SVS_ERR_WAIT_RESP;
    }
    if ((NULL == pHandleInfo->m_pRecvStream)
         || (NULL == pHandleInfo->m_pFileEgress))
    {
        SVS_LOG((SVS_LM_WARNING,
            "get handle status fail.handleIndex[%u],m_pRecvStream[0x%08x],m_pFileEgress[0x%08x]",
            ulIndex,
            pHandleInfo->m_pRecvStream,
            pHandleInfo->m_pFileEgress));
        return SVS_ERR_PARAM;
    }

    int32_t nRet = pHandleInfo->m_pRecvStream->check_status();
    if (SVS_RESULT_OK != nRet)
    {
        return nRet;
    }

    nRet = pHandleInfo->m_pFileEgress->check_status();
    if (SVS_RESULT_OK != nRet)
    {
        return nRet;
    }

    return SVS_RESULT_OK;
}

// ��ȡ��ǰ���µ���¼��ʱ��
int32_t SVS_Media_Processor::get_last_recv_time(time_t & lastRecvTime, uint32_t ulIndex)
{
    if ((NULL == m_HandleArray) || (ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get handle last receive time failed.handle index valid.ulIndex[%d],m_HandleArray[0x%08x]",
            ulIndex,
            m_HandleArray));

        return SVS_ERR_PARAM;
    }

    Handle_Info *pHandleInfo = &m_HandleArray[ulIndex];

    ACE_Read_Guard<ACE_RW_Mutex> locker(pHandleInfo->m_Mutex);
    if (NULL == pHandleInfo->m_pRecvStream)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get handle last receive time failed. handleIndex[%u],m_pRecvStream[0x%08x],m_pFileEgress[0x%08x]",
            ulIndex,
            pHandleInfo->m_pRecvStream,
            pHandleInfo->m_pFileEgress));
        return SVS_ERR_PARAM;
    }

    lastRecvTime = pHandleInfo->m_pRecvStream->last_recv_full_frame_time();

    return SVS_RESULT_OK;
}

// ��ȡ��ǰ����д¼��ʱ��
int32_t SVS_Media_Processor::get_last_write_time(time_t & lastWriteTime, uint32_t ulIndex)
{
    if ((NULL == m_HandleArray) || (ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get handle last write time failed.handle index valid.ulIndex[%d],m_HandleArray[0x%08x]",
            ulIndex,
            m_HandleArray));

        return SVS_ERR_PARAM;
    }

    Handle_Info *pHandleInfo = &m_HandleArray[ulIndex];

    ACE_Read_Guard<ACE_RW_Mutex> locker(pHandleInfo->m_Mutex);
    if (NULL == pHandleInfo->m_pFileEgress)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get handle last write time failed.Write finished. handleIndex[%u],m_pRecvStream[0x%08x],m_pFileEgress[0x%08x]",
            ulIndex,
            pHandleInfo->m_pRecvStream,
            pHandleInfo->m_pFileEgress));
        lastWriteTime = 0;
        return SVS_RESULT_OK;
    }

    lastWriteTime = pHandleInfo->m_pFileEgress->lastWriteTime();

    return SVS_RESULT_OK;
}

// �ж�handle�Ƿ�ر����
int32_t SVS_Media_Processor::get_handle_close_status(const char* szDevID,
                                                        int32_t in_type,
                                                        int32_t out_type,
                                                        uint32_t &handleIndex)

{
    if ((NULL == szDevID) || (NULL == m_HandleArray))
    {
        return SVS_ERR_PARAM;
    }

    // ���Ҹþ�ͷ��Ӧ��handle�Ƿ��Ѿ��ͷţ����û�ͷţ�����ʧ�ܣ����򷵻�OK
    ACE_Write_Guard<ACE_RW_Mutex> locker(m_DevIndexMapMutex);
    DEV_INDEX_MAP_ITER iter = m_DevIndexMap.find(szDevID);
    while ((m_DevIndexMap.end() != iter) && (iter->first == szDevID))
    {
        Handle_Info *pHandleInfo = &m_HandleArray[iter->second];
        if ((in_type == pHandleInfo->m_nInType)
            && (out_type == pHandleInfo->m_nOutType))
        {
            handleIndex = iter->second;
            return SVS_RESULT_FAILURE;
        }
        iter++;
    }

    return SVS_RESULT_OK;
}

void SVS_Media_Processor::debug_get_handle_info(const uint32_t ulIndex,
                                      char * strBuff,
                                      const uint32_t luBuffSize)const
{
    // �����ж�
    if ((NULL == m_HandleArray) || (ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Debug get handle information failed.handle index valid.ulIndex[%d],m_HandleArray[0x%08x]",
            ulIndex,
            m_HandleArray));
        return ;
    }

    Handle_Info *pHandleInfo = &m_HandleArray[ulIndex];

    ACE_Read_Guard<ACE_RW_Mutex> locker(pHandleInfo->m_Mutex);
    if ((NULL == pHandleInfo->m_pRecvStream)
     || (NULL == pHandleInfo->m_pFileEgress))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Debug get handle information failed."
            "index[%u] invalid,RecvStream[0x%08x],FileEgress[0x%08x]",
            ulIndex,
            pHandleInfo->m_pRecvStream,
            pHandleInfo->m_pFileEgress));
        return;
    }

    ACE_INET_Addr localAddr;
    ACE_INET_Addr remoteAddr;
    pHandleInfo->m_pRecvStream->get_local_addr( localAddr );
    pHandleInfo->m_pRecvStream->get_remote_addr( remoteAddr );

    //���ͳ��ʱ��
    uint32_t recvErrorPacketNum =  pHandleInfo->m_pRecvStream->recv_error_packet_num();
    uint32_t recvPacketNum = pHandleInfo->m_pRecvStream->recv_packet_num();
    time_t lastRecvFullFrameTime = pHandleInfo->m_pRecvStream->last_recv_full_frame_time();
    time_t lastWriteTime = pHandleInfo->m_pFileEgress->lastWriteTime();

    char strTime[SVS_STR_TIME_MAX_LEN];

    (void)time2string(strTime, sizeof(strTime), lastRecvFullFrameTime);
    string strLastRecvFullFrameTime = strTime;

    (void)time2string(strTime, sizeof(strTime), lastWriteTime);
    string strLastWriteTime = strTime;

    uint32_t recvByteNum = 0;
    time_t statStartTime = 0;
    time_t statEndTime = 0;
    uint32_t lostFrameNum = 0;
    uint32_t totalFrameNum = 0;
    pHandleInfo->m_pRecvStream->recv_rate_info(statStartTime, statEndTime,
                                               recvByteNum, lostFrameNum,
                                               totalFrameNum);

    (void)time2string(strTime, sizeof(strTime), statStartTime);
    string strStatStartTime = strTime;

    (void)time2string(strTime, sizeof(strTime), statEndTime);
    string strStatEndTime = strTime;

    double rate = (statEndTime - statStartTime) + 1.000001;
    rate = (double)((recvByteNum * RTRECORD_UNIT_BYTE) / rate) / RTRECORD_UNIT_K;

    double lostFrameRate =0;

    if(0 != totalFrameNum)
    {
       lostFrameRate = ((double)lostFrameNum / (double)totalFrameNum)*100;
    }

    (void)ACE_OS::snprintf( strBuff, luBuffSize,
                      "VTDUAddr                 [%s:%u]\n\t"
                      "LocalReceiveAddr         [%s:%u]\n\t"
                      "StreamStatus             [0x%04x]\n\t"
                      "ErrorPackNum             [%d]\n\t"
                      "RecvPackTotalNum         [%d]\n\t"
                      "RecvByteTotalNum         [%d]\n\t"
                      "LastRecvPackTime         [%s]\n\t"
                      "Stat.TimeSection         [%s -- %s]\n\t"
                      "StreamRate               [%8.2f Kbit/s]\n\t"
                      "TotalFrameNum            [%u]\n\t"
                      "LostFrameNum             [%u]\n\t"
                      "LostFrameRate            [%8.5f%%]\n\t"
                      "RecordFilePath           [%s]\n\t"
                      "LastWriteTime            [%s]\n\t",
                      remoteAddr.get_host_addr(), remoteAddr.get_port_number(),
                      localAddr.get_host_addr(),  localAddr.get_port_number(),
                      pHandleInfo->m_pRecvStream->status(),
                      recvErrorPacketNum,
                      recvPacketNum,
                      recvByteNum,
                      strLastRecvFullFrameTime.c_str(),
                      strStatStartTime.c_str(), strStatEndTime.c_str(),
                      rate,
                      totalFrameNum,
                      lostFrameNum,
                      lostFrameRate,
                      pHandleInfo->m_pFileEgress->recordFilePath(),
                      strLastWriteTime.c_str());
    return;
}

// ��ȡ���е�handle info
int32_t SVS_Media_Processor::statAllHandleInfo( uint32_t &startHandle,char* strBuffer, uint32_t nBuffSize)const
{
    // �����ж�
    if((NULL == strBuffer) || (NULL == m_HandleArray))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat all handle info failed."
            "the parameter strBuffer[0x%08x], m_HandleArray[0x%08x]",
            strBuffer,
            m_HandleArray));

        return SVS_RESULT_FAILURE;
    }

    static uint32_t nUsingHandleNum = 0;

    if(startHandle >= m_ulMaxHandleNum)
    {
        nUsingHandleNum = 0;
        return SVS_RESULT_FAILURE;
    }

    size_t writeLen = 0;
    int32_t leaveLen = 0;
    char strTime[SVS_STR_TIME_MAX_LEN];
    Handle_Info * pHandleInfo;
    // ѭ����ȡ����handle����Ϣ
    uint32_t i = startHandle;
    for(; i < m_ulMaxHandleNum; i++)
    {
        writeLen = ACE_OS::strlen(strBuffer);
        //�����������꣬�˳�ѭ��
        leaveLen = (int32_t)(nBuffSize-writeLen);
        if( 300 > leaveLen )
        {
            startHandle = i;

            return SVS_RESULT_OK;
        }

        pHandleInfo = m_HandleArray + i;

        // �����ͷID����Ϊ0����ʶhandle����
        if(0 == strlen(pHandleInfo->m_szDevID))
        {
            continue;
        }

        ++nUsingHandleNum;

        (void)time2string(strTime, sizeof(strTime), pHandleInfo->m_StatusTime);

        (void)ACE_OS::snprintf(   &strBuffer[writeLen],
            (uint32_t)leaveLen,
            "\t HandleIndex[%d]\n"
            "\t CameraId[%s]\n"
            "\t m_pRecvStream[0x%08x]\n"
            "\t m_pFileEgress[0x%08x]\n"
            "\t m_nInType[%d]\n"
            "\t m_nOutType[%d]\n"
            "\t m_StatusTime[%u:%s]\n"
            "\t m_Status[%d]\n"
            "\t m_ulRefCount[%u]\n"
            "\t m_pMediaQueue[0x%08x]\n"
            "\t m_MsgQueueSize[%d]\n\n",
            i,
            pHandleInfo->m_szDevID,
            pHandleInfo->m_pRecvStream,
            pHandleInfo->m_pFileEgress,
            pHandleInfo->m_nInType,
            pHandleInfo->m_nOutType,
            pHandleInfo->m_StatusTime, strTime,
            pHandleInfo->m_Status,
            pHandleInfo->m_ulRefCount,
            pHandleInfo->m_pMediaQueue,
            pHandleInfo->m_MsgQueue.message_count());

    }

    startHandle = i;

    writeLen = ACE_OS::strlen(strBuffer);
    //�����������꣬�˳�ѭ��
    leaveLen = (int32_t)(nBuffSize-writeLen);
    if( 0 == leaveLen )
    {
        return SVS_RESULT_OK;
    }

    (void)ACE_OS::snprintf( &strBuffer[writeLen],
                            (uint32_t)leaveLen,
                    "\n\t TotalHanleNum[%u], UsingHandleNumber[%u]\n",
                    m_ulMaxHandleNum,
                    nUsingHandleNum);

    nUsingHandleNum = 0;

    return SVS_RESULT_OK;
}

void SVS_Media_Processor::statMediaQueueInfo(char* strBuffer, uint32_t nBuffSize)
{
    // �����ж�
    if(NULL == strBuffer)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat media queue information failed."
            "the parameter strBuffer[0x%08x].",
            strBuffer));

        return ;
    }


    (void)snprintf(strBuffer, nBuffSize,
        "\n\t Media Process Queue Info: \n");

    size_t writeLen = 0;
    int32_t leaveLen = 0;
    for (int32_t i = 0; i < PROC_MEDIA_THREAD_NUM; i++)
    {
        writeLen = ACE_OS::strlen(strBuffer);
        //�����������꣬�˳�ѭ��
        leaveLen = (int32_t)(nBuffSize-writeLen);
        if(100 > leaveLen)
        {
            return;
        }

        (void)snprintf(&strBuffer[writeLen],
            (uint32_t)leaveLen,
            "\n\t QueueIndex[%d]:\n"
//            "\t QueueAddress[0x%08x]\n"
            "\t QueueHighLevel[%u]\n"
            "\t QueueLowLevel[%u]\n"
            "\t QueueState[%d]\n"
            "\t QueueLength[%u]\n"
            "\t QueueSize[%u]\n"
            "\t QueueMsgCount[%u]\n"
            "\t QuueuFullFlag[%d]\n",
            i,
//            m_MediaQueue[i],
            m_MediaQueue[i].high_water_mark(),
            m_MediaQueue[i].low_water_mark(),
            m_MediaQueue[i].state(),
            m_MediaQueue[i].message_length(),
            m_MediaQueue[i].message_bytes(),
            m_MediaQueue[i].message_count(),
            m_MediaQueue[i].is_full());
    }

    return;
}

uint32_t SVS_Media_Processor::GetHandleLastError(uint32_t ulIndex)
{
    if ((NULL == m_HandleArray) || (ulIndex >= m_ulMaxHandleNum))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Get Handle LastError.handle index valid.ulIndex[%d],m_HandleArray[0x%08x]",
            ulIndex,
            m_HandleArray));
        return ROUTE_STOP_REASON_MRU_EXCEPTION;
    }

    Handle_Info *pHandleInfo = &m_HandleArray[ulIndex];

    ACE_Read_Guard<ACE_RW_Mutex> locker(pHandleInfo->m_Mutex);

    //��ʱֻ��ȡ����������������
    if (NULL == pHandleInfo->m_pRecvStream)
    {
        return ROUTE_STOP_REASON_OK;
    }

    return pHandleInfo->m_pRecvStream->GetLastError();
}

