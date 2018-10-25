/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : SVS_IO_Scheduler.cpp
  �� �� ��   : ����
  ��������   : 2009��8��21��
  ����޸�   :
  ��������   : NRU��IO������(������),ʵ�ֶ��ļ�IO�ĵ���,��֤�Դ��̵Ķ�д�ܹ�ƽ��,
               ���ȹ��������ڶ�����ͳ��ģ��
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2009��8��21��
    �޸�����   : �����ļ�

******************************************************************************/
#include "svs_log_msg.h"
#include "svs_stat_manager.h"
#include "svs_mb_buffer.h"
#include "svs_rt_record_common.h"
#include "svs_daemon_thread.h"
#include "svs_record_disk.h"
#include "svs_real_record_server.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"
#include "svs_index_list.h"
#include "svs_file_egress.h"

extern CFluxStat g_FluxStat;
// extern int32_t g_alarmStatus[ALARM_INDEX_NUM];

SVS_IO_Scheduler::SVS_IO_Scheduler()
{
    m_pOutputQueue = NULL;

    m_threadIndex = 0;
    m_bRunning = false;
    m_threadTotal = 0;
    m_debugWriteFile = 1;
}

SVS_IO_Scheduler::~SVS_IO_Scheduler()
{
    if(NULL != m_pOutputQueue)
    {
        delete[] m_pOutputQueue;
        m_pOutputQueue = NULL;
    }
}

/*****************************************************************************
 �� �� ��  : open
 ��������  : ��ʼ��,Ϊÿ��raid�鴴��һ�����У�Ϊÿ��raid�鴴��N(��ȡ����)��д
             �̡߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
*****************************************************************************/
int32_t SVS_IO_Scheduler::open(void*)
{
    m_debugWriteFile = CAC_RT_Record_Server::instance()->debug_write_file();
    // ��ȡ¼���߳��������Ϊÿ��raid����߳�����ϵͳ֧�ֵ���󲢷�����raid�����ĳ˻�
    m_threadTotal = CAC_RT_Record_Server::instance()->output_thread_num_per_raid()
                   * CAC_RT_Record_Server::instance()->sync_work_raid_group_num();

    // ������Ƶ���������ָ�뻺��
    try
    {
        m_pOutputQueue = new ACE_Message_Queue <ACE_SYNCH> *[m_threadTotal];
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Allocate output queue failed."));
        return SVS_ERR_NOMEM;
    }

    if ( NULL == m_pOutputQueue )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Allocate output queue failed."));
        return SVS_ERR_NOMEM;
    }

    // �򿪶���
    for(uint32_t i = 0; i < m_threadTotal; i++)
    {
        try
        {
            m_pOutputQueue[i] = new ACE_Message_Queue<ACE_SYNCH>(SVS_IO_TASK_QUEUE_WM, SVS_IO_TASK_QUEUE_WM);
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_CRITICAL, "Allocating output queue failed. queue index[%d].", i));
            return SVS_ERR_NOMEM;
        }

        (void)m_pOutputQueue[i]->close();
        if(0 != m_pOutputQueue[i]->open(SVS_IO_TASK_QUEUE_WM,SVS_IO_TASK_QUEUE_WM))
        {
            SVS_LOG((SVS_LM_CRITICAL,
                "Open IO schedule service failed. Open media queue failed.queue id[%d]",
                i));
            return SVS_ERR_QUEUE;
        }
    }

    SVS_LOG((SVS_LM_INFO, "Begin to create io schedule thread.thread number[%u]",
        m_threadTotal));

    // �����̶߳�ջ��С
    size_t *stack_size = NULL;
    try
    {
        stack_size = new size_t[m_threadTotal];
    }
    catch(...)
    {
        SVS_LOG( (SVS_LM_CRITICAL, "Allocate stack memory failed." ) );
        SVS_DELETE( m_pOutputQueue, SVS_DELETE_MULTI);
        SVS_DELETE(stack_size, SVS_DELETE_MULTI);
        return SVS_ERR_NOMEM;
    }

    // ��ʼ���߳�ջ���ֵ
    uint32_t i = 0;
    for ( i = 0; i < m_threadTotal; i++)
    {
        stack_size[i] = SVS_THREAD_STATCK_MAX;
    }

    m_bRunning = true;

    // ����дý�����߳�
    int32_t activateResult = activate(
         THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED,
         (int32_t)m_threadTotal,
         0,
         ACE_DEFAULT_THREAD_PRIORITY,
         -1,
         0,
         0,
         0,
         stack_size,
         0 );

    SVS_DELETE( stack_size, SVS_DELETE_MULTI );

    return activateResult;
}

/*****************************************************************************
 �� �� ��  : svc
 ��������  : ����д¼���̡߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
*****************************************************************************/
int32_t SVS_IO_Scheduler::svc(void)
{
    g_p_msg_buffer->init();

    ACE_hthread_t currThreadID = (ACE_hthread_t)ACE_OS::thr_self();
    SVS_LOG((SVS_LM_WARNING, "Current thread ID[%u,%d]",currThreadID, currThreadID ));

    (void)signal(SIGPIPE, SIG_IGN);
    int32_t threadIndex = add_index();
    int32_t iRet = 0;
    switch(threadIndex)
    {
        case -1: //����
        {
            SVS_LOG((SVS_LM_CRITICAL, "The thread id is unknown ."));
            return SVS_RESULT_FAILURE;
        }
        default:
        {
            //д¼���ļ��߳�
            iRet = IO_process_thread( threadIndex );
        }
    }

    g_p_msg_buffer->clear();
    return iRet;
}

/*****************************************************************************
 �� �� ��  : close
 ��������  : �رյ�����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
*****************************************************************************/
int32_t SVS_IO_Scheduler::close(u_long)
{
    if(m_bRunning)
    {
        delete[] m_pOutputQueue;
        m_pOutputQueue = NULL;
        m_bRunning = false;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : add_IO
 ��������  : ����IO���񡣸���IO�������ͷ����������д¼���߳���Ҫ����raid��ID����
             Ӧ�÷���Ķ��С�
 �������  : int32_t fd:�ļ����
             int32_t raidIndex
 �������  : NA
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
*****************************************************************************/
int32_t SVS_IO_Scheduler::add_IO(int32_t fd,
                       int32_t queueIndex,
                       IO_Operation oper,
                       ACE_Message_Block *pData,
                       IO_CallBack_Func pFunc,
                       void *pParam)const
{
    if((NULL == m_pOutputQueue) || (0 > queueIndex) || ((int32_t)m_threadTotal <= queueIndex))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Add IO task failed. The parameter is invalid."
            "fd[%d],queueIndex[%d],total queue num[%u], m_pOutputQueue[0x%08x]",
            fd, queueIndex, m_threadTotal,m_pOutputQueue));
        return SVS_ERR_PARAM;
    }

    switch(oper)
    {
        case IO_READ:
        case IO_FFLUSH:
        {
        //1 TODO �����ýӿ�
        }
        break;
        case IO_WRITE:
        case IO_CLOSE:
        {
            // ������Ϣ
            uint32_t msgLen = sizeof(tagIO_Data);
            ACE_Message_Block *mb =
            g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK, msgLen );

            if ( NULL == mb )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Add IO task failed. Allocate message block failed."
                    "operation type[%d], msg type[%d],msg len[%d]",
                    oper,
                    CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK,
                    msgLen));
                return SVS_ERR_PARAM;
            }

            tagIO_Data *pTagIoData = (tagIO_Data *)(void*)mb->wr_ptr();
            pTagIoData->m_nFD = fd;
            pTagIoData->m_Oper = oper;
            pTagIoData->m_pData = pData;
            pTagIoData->m_Func = pFunc;
            pTagIoData->m_pParam = pParam;
            mb->wr_ptr( msgLen );

            // IO��Ϣ�����,����г�ʱ����Ϊ1ms
            ACE_Time_Value tvDelay(0,1000);
            ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;
            if(-1 == m_pOutputQueue[queueIndex]->enqueue_tail(mb,&tvTimeout))
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Add IO task failed. Message enqueue failed.operation type[%d]",
                    oper));
                g_p_msg_buffer->free_mb(mb);
                return SVS_ERR_QUEUE;
            }

        }
        break;

        default:
            SVS_LOG((SVS_LM_ERROR,
                "Add IO task failed. operation type is invalid.operation type[%d]",
                oper));
        break;
    }

    return SVS_RESULT_OK;
}


int32_t SVS_IO_Scheduler::IO_process_thread(int32_t threadIndex)const
{
    SVS_LOG((SVS_LM_WARNING, "The IO process thread[%d] start to running...",threadIndex));

    if(NULL == m_pOutputQueue)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The IO process thread[%d] start failed.m_pOutputQueue is NULL.",
            threadIndex));

        return SVS_RESULT_FAILURE;
    }

    // ��ȡ����
    ACE_Message_Queue <ACE_SYNCH> *pOutputQueue = NULL;
    pOutputQueue = m_pOutputQueue[threadIndex];

    char threadName[MAX_THREAD_NAME] = {0};
    (void)snprintf(threadName,sizeof(threadName),"MediaOutputThread[%d]",threadIndex);
    CThread_Stat_Reporter reporter(threadName);
    SVS_Media_Output_Stat pCMediaOutputStat((uint32_t)threadIndex);
    ACE_Time_Value tvDelay(1);

    ACE_Message_Block *mb = NULL;
    tagIO_Data * pTagIoData = NULL;
//    uint32_t lWriteTimes = 0;
    ACE_Time_Value ctrlTv = ACE_OS::gettimeofday();
    ACE_Message_Block *pTmpfsDataMb = NULL;
    uint32_t tempBufferSize = CAC_RT_Record_Server::instance()->recv_media_buffer_size() ;
    pTmpfsDataMb = alloc_mb(tempBufferSize);
    if(NULL == pTmpfsDataMb)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Allocate temp media buffer failed."));

        return SVS_RESULT_FAILURE;
    }

    uint32_t writeTime = 0;
    while ( m_bRunning )
    {
        ACE_Time_Value currTime = ACE_OS::gettimeofday();
        uint32_t cost =  (uint32_t)((currTime.sec() - ctrlTv.sec()) * 1000 * 1000 + (currTime.usec() - ctrlTv.usec()));

        // �ж��Ƿ���Ҫ����д¼�񣬷�0������
        if (0 == CAC_RT_Record_Server::instance()->get_write_media_flowctrl_flag())
        {
            writeTime = g_FluxStat.WriteBlockTime();
            if(cost < writeTime)
            {
                (void)usleep(1000);
                continue;
            }

            ctrlTv = currTime;
        }
        currTime += tvDelay;
        if (SVS_RESULT_FAILURE == pOutputQueue->dequeue_head(mb, &currTime))
        {
            if( EWOULDBLOCK != ACE_OS::last_error() )
            {
                SVS_LOG((SVS_LM_ERROR,
                    "The thread of writing record file gets media message block failed."
                    "The thread index is [%d] and the queue pointer address is [0x%08x].",
                    threadIndex,
                    pOutputQueue));
            }

            ACE_OS::last_error(0);
            reporter.ReportStat();
            continue;
        }

        reporter.ReportStat();

        pTagIoData = (tagIO_Data*)(void*)mb->rd_ptr();

        switch(pTagIoData->m_Oper)
        {
            case IO_READ:
            {
                if (SVS_RESULT_OK != read_media_data(pTagIoData))
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Read media data failed.",
                        "The thread index is [%d] and the queue pointer address is [0x%08x].",
                        threadIndex,
                        pOutputQueue));
                }
            }
            break;

            case IO_WRITE:
            {
                if (SVS_RESULT_OK != write_media_data(pTagIoData, &pCMediaOutputStat, pTmpfsDataMb))
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Write media data failed.",
                        "The thread index is [%d] and the queue pointer address is [0x%08x].",
                        threadIndex,
                        pOutputQueue));
                }

//                ++lWriteTimes;
                // ���õĵ�����־�����ô�ӡ��ÿ�λ���Ҫ���㣬��ʱ
//                if( 0 == (lWriteTimes % PRINT_WRITE_FILE_NUM) )
//                {
//                    SVS_LOG((SVS_LM_INFO,
//                        "The number of media message block been processed is [%u]."
//                        "and there are [%d] media message block waiting for writed."
//                        "The thread id is [%u]",
//                        lWriteTimes,
//                        pOutputQueue->message_count(),
//                        threadIndex) );
//                }
            }
            break;

            case IO_CLOSE:
            {
                (void)close_file(pTagIoData);
            }
            break;

            case IO_FFLUSH:
            {
            }
            break;

            default:
                SVS_LOG((SVS_LM_CRITICAL, "----------------"));
                break;
        }

        g_p_msg_buffer->free_mb(mb);

    }
    SVS_LOG((SVS_LM_WARNING, "The IO process thread[%d] exit",threadIndex));

    return SVS_RESULT_OK;
}

int32_t SVS_IO_Scheduler::write_media_data(const tagIO_Data* pTagIoData,
                                    SVS_Media_Output_Stat* pCmediaOutputStat,
                                    ACE_Message_Block *pTmpfsDataMb)const
{
    int32_t fd = pTagIoData->m_nFD;
    ACE_Message_Block * mb = pTagIoData->m_pData;
    // �����д�����,��֪ͨҵ���
    if(NULL == mb)
    {
        if (NULL != pTagIoData->m_Func)
        {
            (void)pTagIoData->m_Func(fd, SVS_RESULT_OK, pTagIoData->m_pParam);

            return SVS_RESULT_OK;
        }

        SVS_LOG((SVS_LM_ERROR,
            "Write media data failed.The parameter is invalid."
            "pTagIoData->m_nFD[%d],pTagIoData->m_pData[0x%08x]",
            fd,
            mb));

        return SVS_ERR_PARAM;
    }

    if(-1 >= fd )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Write media data failed.The parameter is invalid."
            "pTagIoData->m_nFD[%d],pTagIoData->m_pData[0x%08x]",
            fd,
            mb));
        return SVS_ERR_PARAM;
    }

    int32_t iRet = SVS_RESULT_OK;
    uint32_t writeDataLen = mb->length();
    ssize_t iSendRet = -1;
    output_record_file(pTagIoData,pTmpfsDataMb,writeDataLen,iSendRet);
    if((size_t)iSendRet != writeDataLen )
    {
        const int32_t nAceErrCode = ACE_OS::last_error();
        SVS_LOG((SVS_LM_ERROR,
            "Occur error while write recording file."
            "[%u] bytes were needed to writed,but real result is [%d] bytes."
            "fd[%d]. ",
            writeDataLen,
            iSendRet,
            fd));

        iRet = convert_err_no(nAceErrCode);

        // �����쳣֪ͨ�����̹���ģ��
        (void)SVS_Disk_Manager::instance().notifyDiskExcept(fd, iRet);

        pCmediaOutputStat->addIoScheduleFailTimes(1);
    }
    else
    {
        // д��ɹ�
        SVS_Recv_Media_Header *pRecvHeader = (SVS_Recv_Media_Header *)(void*)mb->base();
        pCmediaOutputStat->addIoScheduleTimes(1, writeDataLen, pRecvHeader->RecvedFrameCount);
    }

    // �����ص���֪ͨд����.�����ûص������
    if(NULL != pTagIoData->m_Func)
    {
        SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)mb->base();
        if ( pRecvMediaHeader->IsTmpfsMemory )
        {
            CallBackParam *pCallBackParam = (CallBackParam*)pTagIoData->m_pParam;
            pCallBackParam->m_pMediaData = pTmpfsDataMb;

            pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)pTmpfsDataMb->base();
        }

        (void)pTagIoData->m_Func(fd, iRet, pTagIoData->m_pParam);
    }

    // �ͷ���Ϣ�飬ֱ��release���ú����Ѿ�������
    (void)mb->release();

    return SVS_RESULT_OK;
    // ��Ҫ����У����ﲻ�ͷţ���д¼������ĵط��ͷ�
}//lint !e429

//1 TODO�����ӿڣ��ݲ�ʵ��
int32_t SVS_IO_Scheduler::read_media_data(const tagIO_Data* /*pTagIoData*/)const
{
    return SVS_RESULT_OK;
}

int32_t SVS_IO_Scheduler::close_file(const tagIO_Data* pTagIoData)const
{
    if(-1 > pTagIoData->m_nFD)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Close file failed. The parameter is invalid.pTagIoData->m_nFD[%d]",
            pTagIoData->m_nFD));
        return SVS_ERR_PARAM;
    }

    if(SVS_RESULT_OK != ACE_OS::close((ACE_HANDLE)pTagIoData->m_nFD))
    {
        SVS_LOG((SVS_LM_ERROR, "Close file failed. fd[%d]",pTagIoData->m_nFD));
        return SVS_ERR_INVALIDFD;
    }

    // �ر��ļ��ɹ��󣬺����ص�.�����ûص������
    if(NULL != pTagIoData->m_Func)
    {
        (void)pTagIoData->m_Func(SVS_RESULT_OK,SVS_DISK_ERR_OK,pTagIoData->m_pParam);
    }

    return SVS_RESULT_OK;
}

int32_t SVS_IO_Scheduler::add_index()
{
    int32_t oldIndex;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_RESULT_FAILURE);
    oldIndex = m_threadIndex++;
    return oldIndex;
}

void SVS_IO_Scheduler::statMediaQueueInfo(char* strBuffer, uint32_t nBuffSize)
{
    // �����жϺ������δ��ɳ�ʼ������ͳ��
    if((NULL == strBuffer)  || (!m_bRunning) || (NULL == m_pOutputQueue))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stat media queue information failed."
            "the parameter strBuffer[0x%08x],m_bRunning[%d],m_pOutputQueue[0x%08x].",
            strBuffer,
            m_bRunning,
            m_pOutputQueue));

        return ;
    }

    (void)snprintf(strBuffer, nBuffSize,
            "\n\t IO Schedule Queue Info: \n");

    size_t writeLen = 0;
    int32_t leaveLen = 0;

    for (uint32_t i = 0 ; i < m_threadTotal; i++)
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
                "\n\t QueueIndex[%u]:\n"
//                "\t QueueAddress[0x%08x]\n"
                "\t QueueHighLevel[%u]\n"
                "\t QueueLowLevel[%u]\n"
                "\t QueueState[%d]\n"
                "\t QueueLength[%u]\n"
                "\t QueueSize[%u]\n"
                "\t QueueMsgCount[%u]\n"
                "\t QuueuFullFlag[%d]\n",
                i,
//                m_pOutputQueue[i],
                m_pOutputQueue[i]->high_water_mark(),
                m_pOutputQueue[i]->low_water_mark(),
                m_pOutputQueue[i]->state(),
                m_pOutputQueue[i]->message_length(),
                m_pOutputQueue[i]->message_bytes(),
                m_pOutputQueue[i]->message_count(),
                m_pOutputQueue[i]->is_full());
    }

    return;
}
void SVS_IO_Scheduler::output_record_file(const tagIO_Data* pTagIoData,
                                    ACE_Message_Block *pTmpfsDataMb,
                                    uint32_t& writeDataLen,
                                    ssize_t& iSendRet)const
{
    int32_t fd = pTagIoData->m_nFD;
    ACE_Message_Block * mb = pTagIoData->m_pData;

    // ����ر���д���Կ��أ���¼���ļ���Ҫ�������뵽¼���ļ�
    if(0 == m_debugWriteFile)
    {
        iSendRet = (ssize_t)writeDataLen;

        return ;
    }

    SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)mb->base();
    // tmpfs��ʽ�Ļ���飬�ȴ�tmpfs�ж�ȡ����
    if ( pRecvMediaHeader->IsTmpfsMemory )
    {
        pTmpfsDataMb->reset();
        off_t seekPos = lseek( pRecvMediaHeader->TmpfsFd, (off_t)pRecvMediaHeader->TmpfsPos, SEEK_SET );
        if ( seekPos != (off_t)-1 )
        {
            // ��4K����,����4K���봦��
            uint32_t basePos     = (uint32_t)(uint64_t)(char*)pTmpfsDataMb->base();
            uint32_t alignPos    = basePos + sizeof( SVS_Recv_Media_Header );
            if( 0 != (alignPos % SVS_RECORD_FILE_ALIGN_LEN) )
            {
                alignPos = (alignPos + SVS_RECORD_FILE_ALIGN_LEN) & 0xfffff000;
            }
            pTmpfsDataMb->rd_ptr( (uint32_t)(alignPos - basePos) );
            pTmpfsDataMb->wr_ptr( (uint32_t)(alignPos - basePos) );

            pRecvMediaHeader->MediaDataStartPos    = alignPos - basePos;      //ý�����ݿ�ʼƫ��λ��

            // ����ý�����ͷ
            (void)ACE_OS::memcpy( pTmpfsDataMb->base(), mb->base(), sizeof(SVS_Recv_Media_Header));

            // ��ȡý������
            int32_t readResult = read(pRecvMediaHeader->TmpfsFd,(void*)pTmpfsDataMb->wr_ptr(), pRecvMediaHeader->TmpfsDatalen);
            if ( (uint32_t )readResult == pRecvMediaHeader->TmpfsDatalen )
            {
                pTmpfsDataMb->wr_ptr( pRecvMediaHeader->TmpfsDatalen );
                writeDataLen = pRecvMediaHeader->TmpfsDatalen;
                iSendRet = ACE_OS::write( (ACE_HANDLE)fd, pTmpfsDataMb->rd_ptr(), writeDataLen);
            }
            else
            {
                SVS_LOG((SVS_LM_ERROR,
                            "Occur error while read tmpxfs data. readResult[%d], dataLen[%d], fd[%d]. ",
                            readResult,
                            pRecvMediaHeader->TmpfsDatalen,
                            pRecvMediaHeader->TmpfsFd ));
            }

        }
        else
        {
            SVS_LOG((SVS_LM_ERROR,
                        "Occur error while seek tmpxfs. seekPos[%d], dataLen[%d], fd[%d]. ",
                        seekPos,
                        pRecvMediaHeader->TmpfsDatalen,
                        pRecvMediaHeader->TmpfsFd ));
        }
    }
    else
    {
        iSendRet = ACE_OS::write( (ACE_HANDLE)fd, mb->rd_ptr(), writeDataLen );
    }

    return ;
}
int32_t SVS_IO_Scheduler::convert_err_no(const int32_t sysErrNum)const
{
    int32_t iRet = 0;
    // �����������ͣ�֪ͨ�����̹���ģ��
    if((ENOENT == sysErrNum) || (EINVAL == sysErrNum))
    {
        iRet = SVS_ERR_INVALIDFD;
    }
    else if((EAGAIN == sysErrNum) || (EIO == sysErrNum))
    {
        iRet = SVS_ERR_IOERR;
    }
    else if(ENOSPC == sysErrNum)
    {
        iRet = SVS_ERR_NOSPACE;
    }
    else
    {
        iRet = SVS_RESULT_FAILURE;
    }

    return iRet;
}

ACE_Message_Block* SVS_IO_Scheduler::alloc_mb(uint32_t mbSize)const
{
    ACE_Message_Block* mb = NULL;
    try
    {
        mb = new ACE_Message_Block(mbSize);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Allocate mb failed."));
        return NULL;
    }

    // ������ڴ��ж�
    if ( (NULL == mb->data_block())
        || (NULL == mb->base()))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Allocate mb failed."));
        SVS_DELETE ( mb );
        return NULL;
    }

    return mb;
    // �����ҵ����ͷţ����ﲻ�ͷţ�
}//lint !e429

