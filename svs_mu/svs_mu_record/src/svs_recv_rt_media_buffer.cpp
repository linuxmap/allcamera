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
        //�ͷŻ�����
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

// ���ͷ��ڴ�
/*****************************************************************************
�� �� ��  : init
��������  : ����������
�������  : uint32_t BufferLen:����������;
            uint32_t BufferNum:����������
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Recv_RT_Media_Buffer::init(uint32_t BufferLen, uint32_t BufferNum,
            uint32_t tmpfsBufferLen, uint32_t tmpfsBufferNum, uint32_t serveLensNum )
{
    SVS_TRACE();
    ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, SVS_RESULT_FAILURE );
    // ����ѳ�ʼ��,ֱ�ӷ���
    if ( init_flag_ )
    {
        return SVS_RESULT_OK;
    }

    // ����Ѿ�У�����õĺϷ��ԣ����ﲻ��Ҫ�ڼ��
//    // ��������С��Χ�Ϸ��Լ��
//    if( SVS_RECV_RT_MEDIA_DATA_BUFFER_MIN_LEN + SVS_RECORD_FILE_ALIGN_LEN > BufferLen )
//    {
//        return SVS_RESULT_FAILURE;
//    }
//
//    if ( SVS_RECV_RT_MEDIA_DATA_BUFFER_MAX_LEN + SVS_RECORD_FILE_ALIGN_LEN < BufferLen )
//    {
//        return SVS_RESULT_FAILURE;
//    }

    // �����������ж�
    if ( 0 == BufferNum )
    {
        return SVS_RESULT_FAILURE;
    }

    //���Ƿ�������֧�ֵĻ���������
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

    //����������б�
    used_buffer_list_.clear();
    free_buffer_list_.clear();


    //���뻺����,��ʼ�������б�
    uint32_t i = 0;

    serve_lens_num_ = serveLensNum;
    curr_serve_lens_num_ = 0;

    tmpfs_used_buffer_list_.clear();
    tmpfs_free_buffer_list_.clear();

    used_tmpfs_buffer_flag_ = false;
    // �����Ҫ����tmpfs�ڴ棬�򿪱�
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
   //���뻺����,��ʼ�������б�
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

        // ������ڴ��ж�
        if ( (NULL == pMediaBuff->data_block())
            || (NULL == pMediaBuff->base()))
        {
            SVS_LOG((SVS_LM_CRITICAL, "Allocate media buffer failed."));
            SVS_DELETE ( pMediaBuff );

            break;
        }
        free_buffer_list_.push_back( pMediaBuff );
    }

    //���뻺��ʧ��,�����ǰ����Ļ�����
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

        // ��������ʹ��SVS_DELETEɾ��������Ŀռ䣬��������lint�澯
        return SVS_RESULT_FAILURE;    //lint !e429
    }

    //�ó�ʼ����־Ϊtrue
    init_flag_ = true;
    buffer_len_ = BufferLen;

    // ��ʼ��ͳ��
    media_buffer_stat_.init(buffer_len_, tmpfsBufferNum + BufferNum);

    //�ɹ����أ�����Ŀռ佫������ʱ����close�ӿ��ͷţ�����ֱ������lint�澯
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

        // ������ڴ��ж�
        if ( (NULL == pBuff->data_block())
            || (NULL == pBuff->base()))
        {
            SVS_LOG((SVS_LM_CRITICAL, "Allocate tmpfs media buffer failed."));
            SVS_DELETE ( pBuff );

            return SVS_RESULT_FAILURE;
        }

        // ����tmpfs�ļ�
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
        // ��tmpfs��־
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

//�ɹ����أ�����Ŀռ佫������ʱ����close�ӿ��ͷţ�����ֱ������lint�澯
    return SVS_RESULT_OK;//lint !e429
}//lint !e429

/*****************************************************************************
�� �� ��  : close
��������  : �����������Ļ�����
�������  : NA
�������  : NA
�� �� ֵ  : ��ȷ����0,ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Recv_RT_Media_Buffer::close()
{
    SVS_TRACE();
    ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, SVS_RESULT_FAILURE );

    ACE_Message_Block *pBuff = NULL;
    //��δ��ʼ��
    if( !init_flag_ )
    {
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG( (SVS_LM_WARNING,
        "Begin to close the media buffer manager and release buffer.") );

    // ������������еĻ�����
    list<ACE_Message_Block*>::iterator itFreeBuffer = free_buffer_list_.begin();
    while( itFreeBuffer != free_buffer_list_.end() )
    {
        pBuff = *itFreeBuffer;

        SVS_DELETE( pBuff );

        ++itFreeBuffer;
    }
    free_buffer_list_.clear();

    // �����Ѿ�ʹ���б��еĻ�����
    list<ACE_Message_Block*>::iterator itUsedBuffer = used_buffer_list_.begin();
    while( itUsedBuffer != used_buffer_list_.end() )
    {
        pBuff = *itUsedBuffer;

        SVS_DELETE( pBuff );

        ++itUsedBuffer;
    }
    used_buffer_list_.clear();

    //��δ��ʼ����־
    init_flag_ = false;

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : alloc_tmpfs_buffer
��������  : ��ȡtmpfs������
�������  : NA
�������  : NA
�� �� ֵ  : ʧ�ܷ���NULL,�ɹ��������뵽�Ļ�����
�޸���ʷ  :
*****************************************************************************/
ACE_Message_Block* CSVS_Recv_RT_Media_Buffer::alloc_tmpfs_buffer()
{
    ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, NULL );

    //��δ��ʼ��
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
    // �ӹ����ڴ������ȡ�����
    ACE_Message_Block* pTmpBuff = *(tmpfs_free_buffer_list_.begin());
    tmpfs_free_buffer_list_.pop_front();
    tmpfs_used_buffer_list_.push_back( pTmpBuff );

    // ��ӡ������Ϣ
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
�� �� ��  : alloc_buffer
��������  : ��ȡ������
�������  : bLensFirstAllocFlag: ��ͷ��1�η��仺����
�������  : NA
�� �� ֵ  : ʧ�ܷ���NULL,�ɹ��������뵽�Ļ�����
�޸���ʷ  :
*****************************************************************************/
ACE_Message_Block* CSVS_Recv_RT_Media_Buffer::alloc_buffer( bool bLensFirstAllocFlag )
{
    SVS_TRACE();

    ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, NULL );

    //��δ��ʼ��
    if ( !init_flag_ )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Get media buffer failed,because media buffer manager is not initialized."));
        return NULL;
    }

    // ���Ǿ�ͷ��1�����룬�ж�ȥ����������Ƿ�����������
    if ( !bLensFirstAllocFlag )
    {
        // ���û��tmpfs��������Ҳ������������
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

    //û�п��л�����,����NULL
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

    //�ӿ��л����б���ȡһ�����л�����
    ACE_Message_Block* pTmpBuff = *(free_buffer_list_.begin());
    free_buffer_list_.pop_front();

    used_buffer_list_.push_back( pTmpBuff );

    // ��ӡ������Ϣ
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
�� �� ��  : free_buffer
��������  : �ͷ�ָ���Ļ�����
�������  : ACE_Message_Block* buffer:Ҫ�ͷŵĻ�����ָ��
�������  : NA
�� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
�޸���ʷ  :
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

    //��δ��ʼ��
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
        // ��ͷ���1���ڴ��
        if ( bLastMemoryBuffer )
        {
            --curr_serve_lens_num_;
        }
    }

    media_buffer_stat_.addFreeTimes(1);
    return SVS_RESULT_OK;
}

