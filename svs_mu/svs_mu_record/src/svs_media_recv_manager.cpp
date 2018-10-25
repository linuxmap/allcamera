#include <svs_ace_header.h>
#include "svs_log_msg.h"
#include "svs_stat_manager.h"
#include "svs_real_record_server.h"
#include "svs_recv_rt_media_buffer.h"
#include "svs_media_recv_manager.h"
#include "svs_daemon_thread.h"

SVS_Media_Recv_Manager::SVS_Media_Recv_Manager()
{
    m_ulRecvThreadNum    = 0;
    m_bThreadControlFlag = false;
    m_nThreadIndex       = 0;
}

SVS_Media_Recv_Manager::~SVS_Media_Recv_Manager()
{
    try
    {
        // �ͷ����з�Ӧ��
        while (!m_pRecvReactorVector.empty())
        {
            ACE_Reactor *pReactor = m_pRecvReactorVector.front();
            (void)m_pRecvReactorVector.erase(m_pRecvReactorVector.begin());
            delete pReactor;
        }

        // �ͷ����л���
        while (!m_pRecvMediaBuffer.empty())
        {
            CSVS_Recv_RT_Media_Buffer *pBuffer = m_pRecvMediaBuffer.front();
            (void)m_pRecvMediaBuffer.erase(m_pRecvMediaBuffer.begin());
            (void)pBuffer->close();
            delete pBuffer;
        }
        SVS_LOG((SVS_LM_WARNING, "Media recv manager exit."));
    }
    catch(...)
    {
    }

}


int32_t SVS_Media_Recv_Manager::open(void *)
{
    // �������շ�Ӧ�������ü��������߳̾ͼ������շ�Ӧ��
    if (0 != create_recv_reactors())
    {
        return -1;
    }

    // �������̵߳ĸ����������ջ�������
    // �������ݿ�Ĵ�С�����ݿ�����������ļ�ָ��
    if (0 != create_recv_buffers())
    {
        return -1;
    }

    // �������շ�Ӧ���߳�
    try
    {
        size_t *stack_size = new size_t[m_ulRecvThreadNum];
        for (uint32_t i = 0; i < m_ulRecvThreadNum; i++)
        {
            stack_size[i] = SVS_THREAD_STATCK_MAX;
        }
        int32_t nRet = activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED,
                        (int32_t)m_ulRecvThreadNum,
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
            SVS_LOG((SVS_LM_ERROR, "Activate media recv thread failed."));
            delete[] stack_size;
            return -1;
        }

        delete[] stack_size;
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR, "open media recv manager failed."));
        return -1;
    }

    SVS_LOG((SVS_LM_INFO, "open media recv manager success."));
    return 0;
}

int32_t SVS_Media_Recv_Manager::svc()
{
    ACE_hthread_t currThreadID = (ACE_hthread_t)ACE_OS::thr_self();
    uint32_t threadIndex = (uint32_t)getThreadIndex();

    char szThreadName[MAXNAMELEN] = {0};
    (void)snprintf(szThreadName, MAXNAMELEN,  "SVS_Media_Recv_Manager[%u]", threadIndex);
    CThread_Stat_Reporter report(szThreadName);
    while ( ( !m_bThreadControlFlag )
            && (0 == m_pRecvReactorVector[threadIndex]->reactor_event_loop_done()))
    {
        ACE_Time_Value tv(1);
        (void) m_pRecvReactorVector[threadIndex]->handle_events(&tv);
        report.ReportStat(1);
    }

    SVS_LOG((SVS_LM_WARNING, "media recv thread[%u] exit.",threadIndex, currThreadID));
    return 0;
}

int32_t SVS_Media_Recv_Manager::close(u_long)
{
    // �����߳��˳���־
    m_bThreadControlFlag = true;

    // �ر����з�Ӧ��
    std::vector<ACE_Reactor *>::iterator iter = m_pRecvReactorVector.begin();
    while( iter != m_pRecvReactorVector.end())
    {
        (void)(*iter)->end_reactor_event_loop();
    }

    // �ȴ������߳��˳�
    (void)wait();

    SVS_LOG((SVS_LM_INFO, "media recv manager close."));
    return 0;
}

ACE_Reactor * SVS_Media_Recv_Manager::getRecvReactor(uint32_t ulIndex)
{
    if (ulIndex >= m_ulRecvThreadNum)
    {
        return NULL;
    }

    return m_pRecvReactorVector[ulIndex];
}

CSVS_Recv_RT_Media_Buffer *SVS_Media_Recv_Manager::getMediaBuffer(uint32_t ulIndex)
{
    if (ulIndex >= m_ulRecvThreadNum)
    {
        return NULL;
    }

    return m_pRecvMediaBuffer[ulIndex];
}

/**** ������˽�к���ʵ��  ****/
int32_t SVS_Media_Recv_Manager::create_recv_reactors()
{
    // ������շ�Ӧ��
    m_ulRecvThreadNum = CAC_RT_Record_Server::instance()->recv_media_thread_num();

    for (uint32_t i = 0; i < m_ulRecvThreadNum; i++ )
    {
        ACE_Reactor* pReactorTmp   = NULL;
        ACE_Dev_Poll_Reactor* pEpollReactorTmp = NULL;

        try
        {
            pEpollReactorTmp = new ACE_Dev_Poll_Reactor(SVS_EPOLL_MAX_HANDLE_NUM);
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_CRITICAL, "Failed to create epoll reactor of receiving record media."));
            return -1;
        }

        try
        {
            pReactorTmp = new ACE_Reactor(pEpollReactorTmp, 1);
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_CRITICAL, "Failed to create reactor of receiving record media."));
            // ��������ʧ�ܣ���Ҫ�����������dev_epollɾ��
            // �����Ѿ�������ڹرյ�ʱ����ͷ�
            delete pEpollReactorTmp;
            return -1;
        }

        m_pRecvReactorVector.push_back( pReactorTmp );
    // ������vector����Ҫ�ͷ�
    }//lint !e429

    SVS_LOG((SVS_LM_INFO, "create recv reactors success, reactor count[%d].",
               m_ulRecvThreadNum));
    return 0;
    // ������vector����Ҫ�ͷ�
}//lint !e429

// ɾ����һ��ramdisk�����ļ�
int32_t SVS_Media_Recv_Manager::clear_tmpfs_recv_buffers()const
{
    // �����ʱ����������Ϊ0���ü����ʱ�ļ�ϵͳ·��
    if(0 == CAC_RT_Record_Server::instance()->tmpfs_recv_media_buffer_num())
    {
        return SVS_RESULT_OK;
    }

    struct dirent *pDirent = NULL;
    char strBuffer[MAXNAMLEN];
    struct stat sstat;

    // �����ʧ�ܣ���ͷ�ڸô���û�м�¼��ֱ�ӷ���
    DIR* pDir = ::opendir( CAC_RT_Record_Server::instance()->tmpfs_path() );
    if (NULL == pDir )
    {
        return SVS_RESULT_FAILURE;
    }

    while (NULL != (pDirent = ::readdir(pDir)))
    {
        if ( (0 == strcmp(pDirent->d_name, ".")) || (0 == strcmp(pDirent->d_name, "..")))
        {
            continue;
        }

        // ֻɾ����svs_buffer_��ʼ���ļ�
        if ( 0 != strncmp( pDirent->d_name, "svs_buffer_", 11 ) )
        {
            continue;
        }

        // ��Ŀ¼��ֱ��ɾ��
        (void)snprintf(strBuffer, sizeof(strBuffer),
                           "%s/%s",
                           CAC_RT_Record_Server::instance()->tmpfs_path(),
                           pDirent->d_name);

        if(0 != stat(strBuffer, &sstat))
        {
            continue;
        }

        if(0 != (S_IFREG & sstat.st_mode))
        {
            (void)::unlink(strBuffer);
        }
    }

    // �ر�Ŀ¼
    (void)::closedir(pDir);

    return 0;
}


int32_t SVS_Media_Recv_Manager::create_recv_buffers()
{
    if(0 == m_ulRecvThreadNum)
    {
        SVS_LOG((SVS_LM_ERROR, "Create recrive buffer failed. m_ulRecvThreadNum is 0."));
        return SVS_RESULT_FAILURE;
    }
    // ����ϴε�buffer
    (void)clear_tmpfs_recv_buffers();

    if (SVS_RESULT_OK != check_tmpfs_recv_buff_conf())
    {
        return SVS_RESULT_FAILURE;
    }
    //�����������ݻ�����
    uint32_t ulBufferSize = CAC_RT_Record_Server::instance()->recv_media_buffer_size();
    uint32_t ulBufferNum = CAC_RT_Record_Server::instance()->recv_media_buffer_num();
    uint32_t ulTmpfsRecvBufferSize = CAC_RT_Record_Server::instance()->write_media_data_stripe_width();
    uint32_t ulTmpfsBufferNum = CAC_RT_Record_Server::instance()->tmpfs_recv_media_buffer_num();
    uint32_t tmpfsRecvBufferNum = ulTmpfsBufferNum / m_ulRecvThreadNum ;
    uint32_t ulMaxLens = CAC_RT_Record_Server::instance()->max_lens();
    uint32_t ulBufferServeLens = ulMaxLens/m_ulRecvThreadNum;
    if ( (ulMaxLens%m_ulRecvThreadNum) != 0 )
    {
        ++ulBufferServeLens;
    }

    // push ��vector��close�ĵط��ͷ�
    uint32_t recvBufferNum = ulBufferNum / m_ulRecvThreadNum ;
    CSVS_Recv_RT_Media_Buffer *pRtMediaBuffer = NULL;
    for (uint32_t i = 0; i < m_ulRecvThreadNum; i++ )
    {
        pRtMediaBuffer = NULL;
        try
        {
            pRtMediaBuffer = new CSVS_Recv_RT_Media_Buffer;
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_CRITICAL, "Allocate memory failed, "
                "Create a object of the class [CSVS_Recv_RT_Media_Buffer] failed, "
                "the object serial number[%d].",
                i));
            return -1;
        }
        m_pRecvMediaBuffer.push_back( pRtMediaBuffer );

        int32_t initBufferResult = pRtMediaBuffer->init( ulBufferSize, recvBufferNum, ulTmpfsRecvBufferSize, tmpfsRecvBufferNum, ulBufferServeLens );
        if( 0 != initBufferResult )
        {
            SVS_LOG((SVS_LM_CRITICAL,
                "Initialize a object of the class [CSVS_Recv_RT_Media_Buffer] failed ."
                "the object serial number[%d].",
                i));
            // ʧ�ܺ�ϵͳ�˳�����ͳһ�ͷţ����ﲻ�õ����ͷ�
            return -1;//lint !e429
        }
    }

    SVS_LOG((SVS_LM_INFO, "create recv buffer success, data size[%d],"
               "data count[%d], buffer count[%d] buffer serve lens[%d].",
               ulBufferSize, ulBufferNum, m_ulRecvThreadNum, ulBufferServeLens));
    // ������vector����Ҫ�ͷ�
    return 0;//lint !e429
}//lint !e429

/*****************************************************************************
 �� �� ��  : check_tmpfs_recv_buff_conf
 ��������  : ������������ramdisk�����Ƿ�Ϸ��������ʱ��������ò��ܴ���800��
             �ٴΣ���ʱ�������*��������СҪС��ramdisk���������Ĵ���ʣ��ռ��С��
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0 ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t SVS_Media_Recv_Manager::check_tmpfs_recv_buff_conf()
{
    uint32_t ulBufferSize = CAC_RT_Record_Server::instance()->recv_media_buffer_size();
    uint32_t ulTmpfsBufferNum = CAC_RT_Record_Server::instance()->tmpfs_recv_media_buffer_num();
    // �����ʱ����������Ϊ0���ü����ʱ�ļ�ϵͳ·��
    if(0 == ulTmpfsBufferNum)
    {
        return SVS_RESULT_OK;
    }
    if(SVS_TEMP_BUFFER_MAX_NUM < ulTmpfsBufferNum)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "The temp buffer number is too biger.maximal[%d],"
            "conf file[%s],conf name[SVS_RECORD]TmpfsRecvMediaBufferNum=%u.",
            SVS_TEMP_BUFFER_MAX_NUM,
            CONF_FILE,
            ulTmpfsBufferNum));

        return SVS_RESULT_FAILURE;
    }

    uint32_t needTmpDiskSize = ulBufferSize * ulTmpfsBufferNum;

    // ��ʱ������ʣ��ռ��С
    struct statvfs hdstat;
    if (-1 == statvfs(CAC_RT_Record_Server::instance()->tmpfs_path(), &hdstat))
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Stat temp buffer path failed.temp buffer path[%s],"
            "conf file[%s],conf name[SVS_RECORD]TmpfsPath.",
            CAC_RT_Record_Server::instance()->tmpfs_path(),
            CONF_FILE));

        return SVS_RESULT_FAILURE;
    }

    uint32_t diskFreeSize = hdstat.f_bsize * hdstat.f_bavail;

    if(needTmpDiskSize > diskFreeSize)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "The free size of the temp buffer path is too samll."
            "The value of [RecvMediaBufferSize * TmpfsRecvMediaBufferNum] must be biger "
            "than the free size of the temp buffer path."
            "[RecvMediaBufferSize * TmpfsRecvMediaBufferNum]=[%u]KB,realFreeSize[%u]KB,"
            "conf file[%s],conf name[SVS_RECORD]TmpfsRecvMediaBufferNum=%u,"
            "conf name[SVS_RECORD]TmpfsPath=%s .",
            needTmpDiskSize / 1024,
            diskFreeSize / 1024,
            CONF_FILE,
            ulTmpfsBufferNum,
            CAC_RT_Record_Server::instance()->tmpfs_path()));

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Check temp buffer path finished.tmpBuffNum=%u,tmpBuffPath=%s .",
        ulTmpfsBufferNum,
        CAC_RT_Record_Server::instance()->tmpfs_path()));

    return SVS_RESULT_OK;
}

