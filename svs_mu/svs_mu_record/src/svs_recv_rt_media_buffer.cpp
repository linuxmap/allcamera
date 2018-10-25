#include "svs_log_msg.h"
#include "svs_rt_record_common.h"
#include "svs_stat_manager.h"
#include "svs_recv_rt_media_buffer.h"
#include "svs_real_record_server.h"

SVS_Media_Block::SVS_Media_Block()
{
    m_pOwner = NULL;
    m_bTmpfsMemory = false;

    m_bLastMemoryBufferFlag = false;
}

SVS_Media_Block::SVS_Media_Block(size_t nSize)
                 : ACE_Message_Block(nSize)
{
    m_pOwner = NULL;

    m_bTmpfsMemory = false;
    m_bLastMemoryBufferFlag = false;
}

SVS_Media_Block::SVS_Media_Block(CSVS_Recv_RT_Media_Buffer *pOwner, size_t nSize)
                 : ACE_Message_Block(nSize)
{
    m_pOwner = pOwner;
    m_bTmpfsMemory = false;
    m_bLastMemoryBufferFlag = false;
}
SVS_Media_Block::SVS_Media_Block(CSVS_Recv_RT_Media_Buffer *pOwner, size_t nSize, bool bTmpfsMemory )
                : ACE_Message_Block( nSize )
{
    m_pOwner = pOwner;
    m_bTmpfsMemory = bTmpfsMemory;
    m_bLastMemoryBufferFlag = false;
}

SVS_Media_Block::~SVS_Media_Block()
{
    m_pOwner = NULL;
}

ACE_Message_Block *SVS_Media_Block::release()
{
    if (NULL != m_pOwner)
    {
         SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)this->base();
         if(NULL != pRecvMediaHeader->IndexList)
         {
            pRecvMediaHeader->IndexList->clear();
            SVS_DELETE(pRecvMediaHeader->IndexList);
         }

         if(NULL != pRecvMediaHeader->IndexBreakList)
         {
            pRecvMediaHeader->IndexBreakList->clear();
            SVS_DELETE(pRecvMediaHeader->IndexBreakList);
         }

        (void)m_pOwner->free_buffer(this, m_bTmpfsMemory, m_bLastMemoryBufferFlag);

        return NULL;
    }
    return NULL;
}

CSVS_Recv_RT_Media_Buffer::CSVS_Recv_RT_Media_Buffer():
    init_flag_(false),
    buffer_len_( 0 )
{
    alloc_buffer_times_ = 0;
    alloc_buffer_failure_times_ = 0;

    serve_lens_num_ = 0;
    curr_serve_lens_num_ = 0;
    free_memory_buffer_num_ = 0;
    free_tmpfs_buffer_num_ = 0;
    used_tmpfs_buffer_flag_ = false;
}


CSVS_Recv_RT_Media_Buffer::~CSVS_Recv_RT_Media_Buffer()
{
    try
    {
        //释放缓冲区
        if( init_flag_ )
        {
            (void)close();
        }
    }
    catch(...)
    {
        init_flag_ = false;
    }
}

// 别处释放内存
/*****************************************************************************
函 数 名  : init
功能描述  : 初化缓冲区
输入参数  : uint32_t BufferLen:缓存区长度;
            uint32_t BufferNum:缓存区块数
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Recv_RT_Media_Buffer::init(uint32_t BufferLen, uint32_t BufferNum,
            uint32_t tmpfsBufferLen, uint32_t tmpfsBufferNum, uint32_t serveLensNum )
{
    SVS_TRACE();
    ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, SVS_RESULT_FAILURE );
    // 如果已初始化,直接返回
    if ( init_flag_ )
    {
        return SVS_RESULT_OK;
    }

    // 外层已经校验配置的合法性，这里不需要在检查
//    // 缓存区大小范围合法性检测
//    if( SVS_RECV_RT_MEDIA_DATA_BUFFER_MIN_LEN + SVS_RECORD_FILE_ALIGN_LEN > BufferLen )
//    {
//        return SVS_RESULT_FAILURE;
//    }
//
//    if ( SVS_RECV_RT_MEDIA_DATA_BUFFER_MAX_LEN + SVS_RECORD_FILE_ALIGN_LEN < BufferLen )
//    {
//        return SVS_RESULT_FAILURE;
//    }

    // 缓存区个数判断
    if ( 0 == BufferNum )
    {
        return SVS_RESULT_FAILURE;
    }

    //判是否大于最大支持的缓冲区个数
    uint32_t maxBufferNum = SVS_RECV_RT_MEDIA_DATA_POOL_MAX_LEN / BufferLen;
    if ( BufferNum > maxBufferNum )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Initialize the media buffer failed.The number of media buffer is too big."
            "real buffer number[%u],maximal supporting number[%u],media buffer size[%d].",
            BufferNum,
            maxBufferNum,
            BufferLen ) );
        return SVS_RESULT_FAILURE;
    }

    //清除缓冲区列表
    used_buffer_list_.clear();
    free_buffer_list_.clear();


    //申请缓冲区,初始化缓冲列表
    uint32_t i = 0;

    serve_lens_num_ = serveLensNum;
    curr_serve_lens_num_ = 0;

    tmpfs_used_buffer_list_.clear();
    tmpfs_free_buffer_list_.clear();

    used_tmpfs_buffer_flag_ = false;
    // 如果需要开辟tmpfs内存，则开辟
    if ( tmpfsBufferNum > 0 )
    {
        int32_t iRet = SVS_RESULT_FAILURE;
        iRet = init_tempfs_buffer(tmpfsBufferNum,tmpfsBufferLen);

        if(SVS_RESULT_OK != iRet)
        {
            return iRet;
        }
    }

   ACE_Message_Block *pMediaBuff = NULL;
   //申请缓冲区,初始化缓冲列表
    for (  i = 0; i < BufferNum; i++ )
    {
        try
        {
            pMediaBuff = new SVS_Media_Block( this, BufferLen );
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_CRITICAL, "Allocate media buffer failed."));
            break;
        }

        // 申请的内存判断
        if ( (NULL == pMediaBuff->data_block())
            || (NULL == pMediaBuff->base()))
        {
            SVS_LOG((SVS_LM_CRITICAL, "Allocate media buffer failed."));
            SVS_DELETE ( pMediaBuff );

            break;
        }
        free_buffer_list_.push_back( pMediaBuff );
    }

    //申请缓冲失败,清除先前申请的缓冲区
    if( i != BufferNum )
    {
        ACE_Message_Block *pFreeBuff = NULL;
        list<ACE_Message_Block*>::iterator it = free_buffer_list_.begin();
        while( it != free_buffer_list_.end() )
        {
            pFreeBuff = *it;

            SVS_DELETE( pFreeBuff );
            ++it;
        }

        free_buffer_list_.clear();

        // 函数中已使用SVS_DELETE删除了申请的空间，这里屏蔽lint告警
        return SVS_RESULT_FAILURE;    //lint !e429
    }

    //置初始化标志为true
    init_flag_ = true;
    buffer_len_ = BufferLen;

    // 初始化统计
    media_buffer_stat_.init(buffer_len_, tmpfsBufferNum + BufferNum);

    //成功返回，申请的空间将在析构时调用close接口释放，这里直接屏蔽lint告警
    return SVS_RESULT_OK;    //lint !e429
}

int32_t CSVS_Recv_RT_Media_Buffer::init_tempfs_buffer(uint32_t tmpfsBufferNum,
                                                     uint32_t tmpfsBufferLen)
{
    used_tmpfs_buffer_flag_ = true;
    char tmpfsName[128] = {0};
    int32_t tmpfsFd = -1;
    ACE_Message_Block *pBuff = NULL;
    for ( uint32_t i = 0; i < tmpfsBufferNum; i++ )
    {
        try
        {
            pBuff = new SVS_Media_Block( this, sizeof(SVS_Recv_Media_Header), true );
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_CRITICAL, "Allocate tmpfs media buffer failed."));

            return SVS_RESULT_FAILURE;
        }

        // 申请的内存判断
        if ( (NULL == pBuff->data_block())
            || (NULL == pBuff->base()))
        {
            SVS_LOG((SVS_LM_CRITICAL, "Allocate tmpfs media buffer failed."));
            SVS_DELETE ( pBuff );

            return SVS_RESULT_FAILURE;
        }

        // 创建tmpfs文件
        (void)snprintf( tmpfsName, sizeof( tmpfsName ), "%s/svs_buffer_%llx_%04u",
                        CAC_RT_Record_Server::instance()->tmpfs_path(),
                        (uint64_t)this, (uint32_t)i );
        tmpfsFd = open(tmpfsName, O_CREAT|O_RDWR );

        if ( -1 == tmpfsFd )
        {
            SVS_LOG((SVS_LM_CRITICAL, "Create tmpfs media buffer failed."));
            SVS_DELETE ( pBuff );

            return SVS_RESULT_FAILURE;
        }

        SVS_Recv_Media_Header *pRecvMediaHeader =
            (SVS_Recv_Media_Header*)(void*)pBuff->base();
        (void)ACE_OS::memset( (void*)pRecvMediaHeader, 0, sizeof(SVS_Recv_Media_Header) );
        // 置tmpfs标志
        pRecvMediaHeader->IsTmpfsMemory = true;
        pRecvMediaHeader->TmpfsFd = tmpfsFd;
        pRecvMediaHeader->TmpfsDatalen = tmpfsBufferLen;
        pRecvMediaHeader->TmpfsPos = 0; //i * tmpfsBufferLen;
        off_t seekPos = lseek( pRecvMediaHeader->TmpfsFd, (off_t)pRecvMediaHeader->TmpfsPos, SEEK_SET );
        if ( (off_t)-1 == seekPos )
        {
            SVS_LOG((SVS_LM_CRITICAL,
                "Create tmpfs media buffer failed. "
                "lseek failure. seekPos[%d], TmpfsPos[%d].",
                seekPos, pRecvMediaHeader->TmpfsPos ));
            SVS_DELETE ( pBuff );

            return SVS_RESULT_FAILURE;
        }

        tmpfs_free_buffer_list_.push_back( pBuff );
    }

//成功返回，申请的空间将在析构时调用close接口释放，这里直接屏蔽lint告警
    return SVS_RESULT_OK;//lint !e429
}//lint !e429

/*****************************************************************************
函 数 名  : close
功能描述  : 清空所有申请的缓冲区
输入参数  : NA
输出参数  : NA
返 回 值  : 正确返回0,失败返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Recv_RT_Media_Buffer::close()
{
    SVS_TRACE();
    ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, SVS_RESULT_FAILURE );

    ACE_Message_Block *pBuff = NULL;
    //还未初始化
    if( !init_flag_ )
    {
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG( (SVS_LM_WARNING,
        "Begin to close the media buffer manager and release buffer.") );

    // 清理空闲链表中的缓冲区
    list<ACE_Message_Block*>::iterator itFreeBuffer = free_buffer_list_.begin();
    while( itFreeBuffer != free_buffer_list_.end() )
    {
        pBuff = *itFreeBuffer;

        SVS_DELETE( pBuff );

        ++itFreeBuffer;
    }
    free_buffer_list_.clear();

    // 清理已经使用列表中的缓存区
    list<ACE_Message_Block*>::iterator itUsedBuffer = used_buffer_list_.begin();
    while( itUsedBuffer != used_buffer_list_.end() )
    {
        pBuff = *itUsedBuffer;

        SVS_DELETE( pBuff );

        ++itUsedBuffer;
    }
    used_buffer_list_.clear();

    //置未初始化标志
    init_flag_ = false;

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : alloc_tmpfs_buffer
功能描述  : 获取tmpfs缓冲区
输入参数  : NA
输出参数  : NA
返 回 值  : 失败返回NULL,成功返回申请到的缓冲区
修改历史  :
*****************************************************************************/
ACE_Message_Block* CSVS_Recv_RT_Media_Buffer::alloc_tmpfs_buffer()
{
    ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, NULL );

    //还未初始化
    if ( !init_flag_ )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Get media tmpfs buffer failed,because media buffer manager is not initialized."));
        return NULL;
    }

    if ( tmpfs_free_buffer_list_.empty() )
    {
        ++alloc_buffer_failure_times_;
        if(0 == (alloc_buffer_failure_times_ % 5000))
        {
            SVS_LOG( (SVS_LM_CRITICAL,
                "Get media tmpfs buffer failed.Free media buffer list is empty."
                "used tmpfs buffer list size[%d],allicate media buffer failure times[%d].",
                tmpfs_used_buffer_list_.size(),
                alloc_buffer_failure_times_ ) );
        }
        media_buffer_stat_.addAlloctTmpBuffFailTimes(1);
        return NULL;
    }
    // 从共享内存队列中取缓冲块
    ACE_Message_Block* pTmpBuff = *(tmpfs_free_buffer_list_.begin());
    tmpfs_free_buffer_list_.pop_front();
    tmpfs_used_buffer_list_.push_back( pTmpBuff );

    // 打印调试信息
    if(  0 == ( ++alloc_buffer_times_ % PRINT_RECV_BUFFER_STATUS_NUM ) )
    {
        SVS_LOG( (SVS_LM_INFO,
            "Free media buffer list size is [%d] ,"
            "used media buffer list size is [%d] , "
            "Free tmpfs media buffer list size is [%d] ,"
            "used tmpfs media buffer list size is [%d] and "
            "allocate media buffer times is [%d].",
            free_buffer_list_.size(),
            used_buffer_list_.size(),
            tmpfs_free_buffer_list_.size(),
            tmpfs_used_buffer_list_.size(),
            alloc_buffer_times_ ) );
    }

    media_buffer_stat_.addAllocaTimes(1);

    --free_tmpfs_buffer_num_;

    return pTmpBuff;
}
/*****************************************************************************
函 数 名  : alloc_buffer
功能描述  : 获取缓冲区
输入参数  : bLensFirstAllocFlag: 镜头第1次分配缓冲区
输出参数  : NA
返 回 值  : 失败返回NULL,成功返回申请到的缓冲区
修改历史  :
*****************************************************************************/
ACE_Message_Block* CSVS_Recv_RT_Media_Buffer::alloc_buffer( bool bLensFirstAllocFlag )
{
    SVS_TRACE();

    ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, NULL );

    //还未初始化
    if ( !init_flag_ )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Get media buffer failed,because media buffer manager is not initialized."));
        return NULL;
    }

    // 不是镜头第1次申请，判断去掉保留块后，是否还有其它缓冲
    if ( !bLensFirstAllocFlag )
    {
        // 如果没有tmpfs缓冲区，也可以正常分配
        if ( used_tmpfs_buffer_flag_
            && (free_memory_buffer_num_ < (serve_lens_num_ - curr_serve_lens_num_) ))
        {
            ++alloc_buffer_failure_times_;
            if(0 == (alloc_buffer_failure_times_ % 50000))
            {
                SVS_LOG( (SVS_LM_ERROR,
                    "Get media buffer failed. free_memory_buffer_num_[%d] serve_lens_num_[%d] curr_serve_lens_num_[%d]",
                    free_memory_buffer_num_,
                    serve_lens_num_,
                    curr_serve_lens_num_));
            }
            media_buffer_stat_.addAllcoaFailTimes(1);
            return NULL;
        }
    }

    //没有空闲缓冲区,返回NULL
    if (free_buffer_list_.empty() )
    {
        ++alloc_buffer_failure_times_;
        if(0 == (alloc_buffer_failure_times_ % 50000))
        {
            SVS_LOG( (SVS_LM_CRITICAL,
                "Get media buffer failed.Free media buffer list is empty."
                "used buffer list size[%d],allicate media buffer failure times[%d].",
                used_buffer_list_.size(),
                alloc_buffer_failure_times_ ) );
        }
        media_buffer_stat_.addAllcoaFailTimes(1);
        return NULL;
    }

    if ( bLensFirstAllocFlag )
    {
        ++curr_serve_lens_num_;
    }

    //从空闲缓冲列表中取一个空闲缓冲区
    ACE_Message_Block* pTmpBuff = *(free_buffer_list_.begin());
    free_buffer_list_.pop_front();

    used_buffer_list_.push_back( pTmpBuff );

    // 打印调试信息
    if(  0 == ( ++alloc_buffer_times_ % PRINT_RECV_BUFFER_STATUS_NUM ) )
    {
        SVS_LOG( (SVS_LM_INFO,
            "Free media buffer list size is [%d] ,"
            "used media buffer list size is [%d] , "
            "Free tmpfs media buffer list size is [%d] ,"
            "used tmpfs media buffer list size is [%d] and "
            "allocate media buffer times is [%d].",
            free_buffer_list_.size(),
            used_buffer_list_.size(),
            tmpfs_free_buffer_list_.size(),
            tmpfs_used_buffer_list_.size(),
            alloc_buffer_times_ ) );
    }

    media_buffer_stat_.addAllocaTimes(1);

    --free_memory_buffer_num_;

    return pTmpBuff;

}

/*****************************************************************************
函 数 名  : free_buffer
功能描述  : 释放指定的缓冲区
输入参数  : ACE_Message_Block* buffer:要释放的缓存区指针
输出参数  : NA
返 回 值  : 成功返回0,失败返回-1
修改历史  :
*****************************************************************************/
int32_t CSVS_Recv_RT_Media_Buffer::free_buffer( ACE_Message_Block* buffer,
        bool bTmpfsMemory, bool bLastMemoryBuffer )
{
    SVS_TRACE();
    if ( NULL == buffer )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Free media buffer failed.The parameter buffer is NULL."));
        return SVS_RESULT_FAILURE;
    }

    ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, SVS_RESULT_FAILURE );

    //还未初始化
    if ( !init_flag_ )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Free media buffer failed.The media buffer manager is not initializted."));
        return SVS_RESULT_FAILURE;
    }

    if ( bTmpfsMemory )
    {
        SVS_Recv_Media_Header *pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)buffer->base();

        off_t seekPos = lseek( pRecvMediaHeader->TmpfsFd, (off_t)pRecvMediaHeader->TmpfsPos, SEEK_SET );
        if ( (off_t)-1 == seekPos )
        {
            SVS_LOG( (SVS_LM_ERROR,
                "Free tmpfs media buffer failed. lseek failured."));
        }
        tmpfs_used_buffer_list_.remove( buffer );
        tmpfs_free_buffer_list_.push_back( buffer );
    }
    else
    {
        used_buffer_list_.remove( buffer );
        free_buffer_list_.push_back( buffer );
    }

    if ( bTmpfsMemory )
    {
        ++free_tmpfs_buffer_num_;
    }
    else
    {
        ++free_memory_buffer_num_;
        // 镜头最后1个内存块
        if ( bLastMemoryBuffer )
        {
            --curr_serve_lens_num_;
        }
    }

    media_buffer_stat_.addFreeTimes(1);
    return SVS_RESULT_OK;
}

