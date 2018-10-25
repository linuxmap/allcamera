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
        // 释放所有反应器
        while (!m_pRecvReactorVector.empty())
        {
            ACE_Reactor *pReactor = m_pRecvReactorVector.front();
            (void)m_pRecvReactorVector.erase(m_pRecvReactorVector.begin());
            delete pReactor;
        }

        // 释放所有缓存
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
    // 创建接收反应器，配置几个接收线程就几个接收反应器
    if (0 != create_recv_reactors())
    {
        return -1;
    }

    // 按接收线程的个数创建接收缓冲区。
    // 单个数据块的大小和数据块个数由配置文件指定
    if (0 != create_recv_buffers())
    {
        return -1;
    }

    // 创建接收反应器线程
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
    // 设置线程退出标志
    m_bThreadControlFlag = true;

    // 关闭所有反应器
    std::vector<ACE_Reactor *>::iterator iter = m_pRecvReactorVector.begin();
    while( iter != m_pRecvReactorVector.end())
    {
        (void)(*iter)->end_reactor_event_loop();
    }

    // 等待所有线程退出
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

/**** 以下是私有函数实现  ****/
int32_t SVS_Media_Recv_Manager::create_recv_reactors()
{
    // 构造接收反应器
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
            // 本次申请失败，需要把上面申请的dev_epoll删除
            // 其它已经申请的在关闭的时候会释放
            delete pEpollReactorTmp;
            return -1;
        }

        m_pRecvReactorVector.push_back( pReactorTmp );
    // 放入了vector不需要释放
    }//lint !e429

    SVS_LOG((SVS_LM_INFO, "create recv reactors success, reactor count[%d].",
               m_ulRecvThreadNum));
    return 0;
    // 放入了vector不需要释放
}//lint !e429

// 删除上一次ramdisk缓冲文件
int32_t SVS_Media_Recv_Manager::clear_tmpfs_recv_buffers()const
{
    // 如果临时缓存区块数为0则不用检测临时文件系统路径
    if(0 == CAC_RT_Record_Server::instance()->tmpfs_recv_media_buffer_num())
    {
        return SVS_RESULT_OK;
    }

    struct dirent *pDirent = NULL;
    char strBuffer[MAXNAMLEN];
    struct stat sstat;

    // 如果打开失败，镜头在该磁盘没有记录，直接返回
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

        // 只删除以svs_buffer_开始的文件
        if ( 0 != strncmp( pDirent->d_name, "svs_buffer_", 11 ) )
        {
            continue;
        }

        // 非目录，直接删除
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

    // 关闭目录
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
    // 清空上次的buffer
    (void)clear_tmpfs_recv_buffers();

    if (SVS_RESULT_OK != check_tmpfs_recv_buff_conf())
    {
        return SVS_RESULT_FAILURE;
    }
    //创建接收数据缓冲区
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

    // push 到vector在close的地方释放
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
            // 失败后系统退出，会统一释放，这里不用单独释放
            return -1;//lint !e429
        }
    }

    SVS_LOG((SVS_LM_INFO, "create recv buffer success, data size[%d],"
               "data count[%d], buffer count[%d] buffer serve lens[%d].",
               ulBufferSize, ulBufferNum, m_ulRecvThreadNum, ulBufferServeLens));
    // 放入了vector不需要释放
    return 0;//lint !e429
}//lint !e429

/*****************************************************************************
 函 数 名  : check_tmpfs_recv_buff_conf
 功能描述  : 检测挂载上来的ramdisk配置是否合法。如果临时缓存块配置不能大于800，
             再次，临时缓存块数*缓存区大小要小于ramdisk挂载上来的磁盘剩余空间大小。
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0 失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_Media_Recv_Manager::check_tmpfs_recv_buff_conf()
{
    uint32_t ulBufferSize = CAC_RT_Record_Server::instance()->recv_media_buffer_size();
    uint32_t ulTmpfsBufferNum = CAC_RT_Record_Server::instance()->tmpfs_recv_media_buffer_num();
    // 如果临时缓存区块数为0则不用检测临时文件系统路径
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

    // 临时缓存区剩余空间大小
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

