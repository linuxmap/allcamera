#include "svs_adapter_signal_buffer_manager.h"
#include "svs_log_msg.h"


//=========================CSignalBuffer BEGIN=================================
CSignalBuffer::CSignalBuffer()
{
    reset();
}

CSignalBuffer::~CSignalBuffer()
{
    try
    {
        (void)clear();
    }
    catch(...)
    {
    }
}

void CSignalBuffer::reset()
{
    mb_size_ = 0;
    free_mb_count_ = 0;
    max_free_mb_count_ = 0;

    mb_type_ = 0;
    mb_len_  = 0;

    inited_flag_ = false;

    free_mb_list_ = NULL;
}

/*****************************************************************************
函 数 名  : init
功能描述  : 初始化消息缓冲区
输入参数  : mbCount :   消息块个数
            mbSize  :   消息块大小
            mbType  :   消息类型
            mbLen   :   消息长度种类
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/

int32_t CSignalBuffer::init(   uint32_t mbCount,
                            uint32_t mbSize,
                            uint8_t mbType,
                            uint8_t mbLen)
{
    //已初始化，返回失败
    if ( inited_flag_ )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Initialize message block buffer failed."
            "Message block buffer has been initialized."));
        return -1;
    }

    max_free_mb_count_ = mbCount;
    free_mb_count_ = mbCount;
    mb_size_ = mbSize;
    mb_type_ = mbType;
    mb_len_  = mbLen;

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to initialize message block buffer."
        "message block count[%d], size[%d], type[%d] ,length[%d].",
        free_mb_count_,
        mb_size_,
        mb_type_,
        mb_len_ ));

    ACE_Message_Block *mb = NULL;
    uint32_t i = 0;

    SVS_Mb_Buffer_Info *pMbBuffInfo = NULL;

    //分配内存
    for( i = 0; i < mbCount; i++ )
    {
        try
        {
            mb = new ACE_Message_Block( mbSize );
        }
        catch (...)
        {
            break;
        }

        if( (NULL == mb->data_block() )
            || (NULL == mb->base()))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Initialize message block buffer failed."
                "Failed to allocate memory." ));

            delete mb;
            mb = NULL;
            break;
        }

        pMbBuffInfo = (SVS_Mb_Buffer_Info*)(void*)mb->base();
        pMbBuffInfo->type = mb_type_;
        pMbBuffInfo->length = mb_len_;

        mb->rd_ptr( sizeof(SVS_Mb_Buffer_Info) );
        mb->wr_ptr( sizeof(SVS_Mb_Buffer_Info) );

        mb->next( free_mb_list_ );
        free_mb_list_ = mb;

        mb = NULL;
    }

    //分配失败，清空原来已分配的内存
    if ( i != mbCount )
    {
        (void)clear();
        // clear()接口即是释放所有申请的空间，这时屏蔽lint告警
        return -1;   //lint !e429
    }

    inited_flag_ = true;

    // 成功返回，所有空间在析构时释放，这里屏蔽lint告警
    return 0;  //lint !e429
}


/*****************************************************************************
函 数 名  : alloc_mb
功能描述  : 从缓冲区中分配空闲消息块，尽可能的分配
输入参数  : allocCount  :   消息块个数
输出参数  : 无
返 回 值  : 成功,返回 空闲消息块指针（可能少于指定的消息块个数） 否则返回NULL
修改历史  :
*****************************************************************************/

ACE_Message_Block *CSignalBuffer::alloc_mb( uint32_t allocCount )
{
    //检查是否已初始化
    if ( !inited_flag_ )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Allocate message block failed."
            "Message block buffer hasn't been initialized."));
        return NULL;
    }

    if ( 0 == allocCount )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Allocate message block failed."
            "The count is 0."));
        return NULL;
    }

    ACE_Message_Block *mbFirst = NULL;
    ACE_Message_Block *mbTemp = NULL;
    ACE_Message_Block *mbCont = NULL;

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to allocate message block."
        "this type message block count[%d], size[%d], type[%d], length[%d]."
        "Allocate count[%d] this time.",
        free_mb_count_,
        mb_size_,
        mb_type_,
        mb_len_,
        allocCount ));

    {
        ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, mutex_, NULL);

        //没有空闲块，返回失败
        if ( NULL == free_mb_list_ )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Allocate message block failed.Not have free message block."
                "Free message block size is [%u].",
                free_mb_count_));
            return NULL;
        }

        mbFirst = free_mb_list_;
        free_mb_list_ = free_mb_list_->next();
        mbFirst->next( NULL );
        --free_mb_count_;

        mbCont = mbFirst;

        for ( uint32_t i = 1; i < allocCount; i++ )
        {
            if ( NULL == free_mb_list_ )
            {
                break;
            }

            mbTemp = free_mb_list_;
            free_mb_list_ = free_mb_list_->next();
            mbTemp->next( NULL );
            mbTemp->cont( NULL );

            --free_mb_count_;

            mbCont->cont( mbTemp );
            mbCont = mbTemp;
        }
    }

    return mbFirst;
}

/*****************************************************************************
函 数 名  : alloc_mb
功能描述  : 从缓冲区中分配空闲消息块
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 空闲消息块指针 否则返回NULL
修改历史  :
*****************************************************************************/

ACE_Message_Block *CSignalBuffer::alloc_mb()
{
    if ( !inited_flag_ )
    {
         SVS_LOG((SVS_LM_ERROR,
            "Allocate message block failed."
            "Message block buffer hasn't been initialized."));
        return NULL;
    }

    ACE_Message_Block *mb = NULL;

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to allocate message block."
        "this type message block count[%d], size[%d], type[%d], length[%d].",
        free_mb_count_,
        mb_size_,
        mb_type_,
        mb_len_));
    {
        ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, mutex_, NULL);

        //没有空闲块，返回失败
        if ( NULL == free_mb_list_ )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Allocate message block failed.Not have free message block."
                "Free message block number is [%u].",
                free_mb_count_));
            return NULL;
        }

        mb = free_mb_list_;
        free_mb_list_ = free_mb_list_->next();
        mb->next( NULL );

        free_mb_count_--;
    }

    return mb;

}

/*****************************************************************************
函 数 名  : free_mb
功能描述  : 将指定的消息块放入缓冲区中
输入参数  : mb :   消息块指针
输出参数  : 无
返 回 值  : 成功,返回 0 否则则返回-1
修改历史  :
*****************************************************************************/

int32_t CSignalBuffer::free_mb(ACE_Message_Block *mb)
{
    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Free message block failed.The parameter mb is NULL."));
        return -1;
    }

    ACE_Message_Block *mbCont = mb->cont();
    ACE_Message_Block *mbFree = mb;

    ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, mutex_, -1);
    while ( NULL != mbFree )
    {
        mbFree->cont( NULL );
        mbFree->next( NULL );

        if( mbFree->size() != mb_size_ )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Free message block failed.Size is invalid."
                "This type message block size is [%u],"
                "but this message block number is[%d].",
                mb_size_,
                mbFree->size()));
        }
        else
        {
            mbFree->reset();
            mbFree->rd_ptr( sizeof(SVS_Mb_Buffer_Info) );
            mbFree->wr_ptr( sizeof(SVS_Mb_Buffer_Info) );

            mbFree->next( free_mb_list_ );
            free_mb_list_ = mbFree;

            free_mb_count_++;
        }

        mbFree = mbCont;
        if ( NULL == mbCont )
        {
            break;
        }
        mbCont = mbCont->cont();
    }


    SVS_LOG((SVS_LM_DEBUG,
        "Free message block finished."
        "this type message block count [%d],size[%d],type[%d], length[%d]",
        free_mb_count_,
        mb_size_,
        mb_type_,
        mb_len_ ));

    return 0;
}

/*****************************************************************************
函 数 名  : clear
功能描述  : 清空缓冲区所有消息块
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则则返回-1
修改历史  :
*****************************************************************************/

int32_t CSignalBuffer::clear( )
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, mutex_, -1);

    //清空缓冲区中的所有消息块
    ACE_Message_Block *mbDelete = free_mb_list_;

    for ( ; NULL != free_mb_list_;  )
    {
        mbDelete = free_mb_list_;
        free_mb_list_ = free_mb_list_->next();
        mbDelete->next( NULL );

        try
        {
            delete mbDelete;
            mbDelete = NULL;
        }
        catch (...)
        {
        }
    }

    reset();

    return 0;
}
//=========================CSignalBuffer END=================================

//=========================CSignalBufferManager BEGIN========================
CSignalBufferManager::CSignalBufferManager()
{
    memset((void*)mb_buff_, 0, sizeof(mb_buff_) );
    inited_flag_ = false;
}

CSignalBufferManager::~CSignalBufferManager()
{
    try
    {
        (void)clear();
    }
    catch(...)
    {
    }
}

/*****************************************************************************
函 数 名  : init
功能描述  : 初始化各缓冲区队列
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t CSignalBufferManager::init()
{
    //已经初始化过了，直接返回失败
    if ( inited_flag_ )
    {
        SVS_LOG((SVS_LM_ERROR,
        "Initialize the message block manager failed."
        "It has been initialized."));
        return -1;
    }

    uint32_t buffCount[MB_TYPE_MAX_VALUE][MB_LEN_MAX_VALUE] =
        {
            { 40000, 20000, 10000, 160, 100 }, //其它信令消息类型
        };
    uint32_t buffSize[MB_LEN_MAX_VALUE] =
        { SVS_128_BYTE, SVS_256_BYTE, SVS_1K_BYTE, SVS_4K_BYTE, SVS_64K_BYTE };

    CSignalBuffer *pMbBuffer = NULL;
    bool initFailureFlag = false;
    for ( uint8_t i = 0; i < MB_TYPE_MAX_VALUE; i++ )
    {
        for ( uint8_t j = 0; j < MB_LEN_MAX_VALUE; j++ )
        {
            if ( 0 == buffCount[i][j] )
            {
                mb_buff_[i][j] = NULL;
                continue;
            }

            //创建缓冲区
            try
            {
                pMbBuffer = new CSignalBuffer;
            }
            catch (...)
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Initialize the message block manager failed."
                    "Allocate message block buffer failed." ));
                initFailureFlag = true;
                break;
            }

            //初始化缓冲区
            mb_buff_[i][j] = pMbBuffer;
            int32_t initResult = pMbBuffer->init(   buffCount[i][j],
                                                buffSize[j] + sizeof(SVS_Mb_Buffer_Info),
                                                i,
                                                j );

            if ( 0 != initResult )
            {
                initFailureFlag = true;
                break;
            }
        }
    }

    //初始化失败，清理缓冲区
    if ( initFailureFlag )
    {
        (void)clear();
        return -1;
    }

    inited_flag_ = true;
    return 0;

}

/*****************************************************************************
函 数 名  : clear
功能描述  : 清空缓冲区中所有队列
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则则返回-1
修改历史  :
*****************************************************************************/
int32_t CSignalBufferManager::clear()
{
    for ( uint8_t i = 0; i < MB_TYPE_MAX_VALUE; i++ )
    {
        for ( uint8_t j = 0; j < MB_LEN_MAX_VALUE; j++ )
        {
            if ( NULL != mb_buff_[i][j] )
            {
                (void)mb_buff_[i][j]->clear();

                delete mb_buff_[i][j];
                mb_buff_[i][j] = NULL;
            }
        }
    }

    inited_flag_ = false;

    return 0;
}

/*****************************************************************************
函 数 名  : alloc_mb
功能描述  : 从缓冲区中分配空闲消息块
输入参数  : mbType  :   消息块类型
            mbSize  :   消息块大小
            mbCount :   消息块个数
输出参数  : 无
返 回 值  : 成功,返回 空闲消息块指针 否则返回NULL
修改历史  :
*****************************************************************************/

ACE_Message_Block *CSignalBufferManager::alloc_mb(uint8_t mbType,
                                                    uint32_t mbSize,
                                                    uint32_t mbCount ) const
{
    //获取消息缓冲区指针
    CSignalBuffer *pMbBuffer = get_mb_buffer( mbType, mbSize );
    if ( NULL == pMbBuffer )
    {
        return NULL;
    }

    ACE_Message_Block *mb = pMbBuffer->alloc_mb( mbCount );

    return mb;
}

/*****************************************************************************
函 数 名  : free_mb
功能描述  : 将指定的消息块放入缓冲区中
输入参数  : mb :   消息块指针
输出参数  : 无
返 回 值  : 成功,返回 0 否则则返回-1
修改历史  :
*****************************************************************************/

int32_t CSignalBufferManager::free_mb(ACE_Message_Block *mb) const
{
    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Free message block failed. The parmater mb is NULL."));
        return -1;
    }

    ACE_Message_Block *mbTemp = mb;
    ACE_Message_Block *mbFree = NULL;

    ACE_Message_Block *mbFreeBuff[MB_TYPE_MAX_VALUE][MB_LEN_MAX_VALUE] = { {NULL,}, };
    while ( NULL != mbTemp )
    {
        mbFree = mbTemp;
        mbTemp = mbTemp->cont();

        SVS_Mb_Buffer_Info* pMbBuffInfo = (SVS_Mb_Buffer_Info*)(void*)mbFree->base();
        mbFree->cont( mbFreeBuff[pMbBuffInfo->type][pMbBuffInfo->length] );
        mbFreeBuff[pMbBuffInfo->type][pMbBuffInfo->length] = mbFree;
    }

    for ( uint8_t i = 0; i < MB_TYPE_MAX_VALUE; i++ )
    {
        for ( uint8_t j = 0; j < MB_LEN_MAX_VALUE; j++ )
        {
            if ( NULL != mbFreeBuff[i][j] )
            {
                (void)mb_buff_[i][j]->free_mb( mbFreeBuff[i][j] );
            }
        }
    }

    return 0;
}

/*****************************************************************************
函 数 名  : get_mb_buffer
功能描述  : 获取指定类型/大小的消息块所在的缓冲区
输入参数  : mbType  :   消息类型
            mbSize  :   消息大小
输出参数  : 无
返 回 值  : 成功,返回 缓冲区指针 否则返回NULL
修改历史  :
*****************************************************************************/
CSignalBuffer *CSignalBufferManager::get_mb_buffer(  uint8_t mbType,
                                                        uint32_t mbSize ) const
{
    switch ( mbType )
    {
    case MB_TYPE_NORMAL_EVENT:  //其它信令消息
        return get_mb_buffer_normal_event(mbSize);
    default :
        SVS_LOG((SVS_LM_ERROR,
            "Get message block failed. The message block type is invalid."
            "message block type[%d]",
            mbType ));
        return NULL;
    }
}
//=========================CSignalBufferManager END========================

//==================CSignalBufferTss BEGIN=================
CSignalBufferTss::CSignalBufferTss()
{
    reset();
}

CSignalBufferTss::~CSignalBufferTss()
{
    try
    {
        clear();
    }
    catch(...)
    {
    }
}

/*****************************************************************************
函 数 名  : clear
功能描述  : 释放所有资源
输入参数  : 无
输出参数  : 无
返 回 值  : 无
修改历史  :
*****************************************************************************/
void CSignalBufferTss::clear()
{

    for ( int32_t i = 0; i < THREAD_MSG_MAX_TYPE; i++ )
    {
        for ( int32_t j = 0; j < THREAD_MSG_MAX_LEN; j++ )
        {
            if ( NULL != free_msg_buffer_[i][j] )
            {
                (void)CSignalBufferManager::instance()->free_mb( free_msg_buffer_[i][j] );
                free_msg_buffer_[i][j] = NULL;
            }

            alloced_msg_buffer_count_[i][j] = 0;
        }
    }

    if ( NULL != used_msg_buffer_ )
    {
        (void)CSignalBufferManager::instance()->free_mb( used_msg_buffer_ );
        used_msg_buffer_ = NULL;
        used_msg_buffer_count_ = 0;
    }
}

/*****************************************************************************
函 数 名  : init
功能描述  : 初始化
输入参数  : 无
输出参数  : 无
返 回 值  : 无
修改历史  :
*****************************************************************************/
void CSignalBufferTss::init()
{
    //clear();
    reset();
}

/*****************************************************************************
函 数 名  : reset
功能描述  : 复位所有成员变量
输入参数  : 无
输出参数  : 无
返 回 值  : 无
修改历史  :
*****************************************************************************/
void CSignalBufferTss::reset()
{
    for ( int32_t i = 0; i < THREAD_MSG_MAX_TYPE; i++ )
    {
        for ( int32_t j = 0; j < THREAD_MSG_MAX_LEN; j++ )
        {
            if ( NULL != free_msg_buffer_[i][j] )
            {
                free_msg_buffer_[i][j] = NULL;
            }

            alloced_msg_buffer_count_[i][j] = 0;
        }
    }

    used_msg_buffer_ = NULL;
    used_msg_buffer_count_ = 0;
}


/*****************************************************************************
函 数 名  : alloc_msg_mb
功能描述  : 根据消息类型申请消息缓冲区
输入参数  : mbType  :   消息类型
            mbSize :    消息大小
输出参数  : 无
返 回 值  : 成功,返回 申请到的消息缓冲区 否则返回NULL
修改历史  :
*****************************************************************************/
ACE_Message_Block *
CSignalBufferTss::alloc_mb( uint8_t mbType,
                                          uint32_t mbSize )
{
    ACE_Message_Block *mb = NULL;

    //从消息大小转换成消息长度类型
    uint8_t mbLen = CSignalBufferManager::instance()->mb_size_to_len_type( mbType, mbSize );

    if ( CSignalBufferManager::MB_LEN_MAX_VALUE == mbLen )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Allocate message block failed.The message block size is invalid."
            "The parameter message block size[%d], type[%d].",
            mbSize,
            mbType ));

        return NULL;
    }

    if ( NULL == free_msg_buffer_[mbType][mbLen] )
    {
        free_msg_buffer_[mbType][mbLen] =
            CSignalBufferManager::instance()->alloc_mb( mbType, mbSize, SVS_ALLOC_MSG_BUFF_COUNT );

        if ( NULL == free_msg_buffer_[mbType][mbLen] )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Failed to allocate message block."
                "Not have free message block of this type."
                "message block type[%d], size[%d], allocate count[%d]",
                mbType,
                mbLen,
                SVS_ALLOC_MSG_BUFF_COUNT ));

            return NULL;
        }
    }

    mb = free_msg_buffer_[mbType][mbLen];
    free_msg_buffer_[mbType][mbLen] = free_msg_buffer_[mbType][mbLen]->cont();
    mb->cont( NULL );

    ++alloced_msg_buffer_count_[mbType][mbLen];

    return mb;
}

/*****************************************************************************
函 数 名  : free_msg_mb
功能描述  : 释放指定的消息块
输入参数  : freeMb  :   要释放的消息块
输出参数  : 无
返 回 值  : 无
修改历史  :
*****************************************************************************/

void CSignalBufferTss::free_mb(ACE_Message_Block *mb)
{
    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Free message block failed.The parameter mb is NULL."));
        return ;
    }

    if ( NULL == used_msg_buffer_ )
    {
        used_msg_buffer_ = mb;
    }
    else
    {
        mb->cont( used_msg_buffer_ );
        used_msg_buffer_ = mb;
    }

    ++used_msg_buffer_count_;

    SVS_Mb_Buffer_Info* pMbBuffInfo = (SVS_Mb_Buffer_Info*)(void*)mb->base();
    --alloced_msg_buffer_count_[pMbBuffInfo->type][pMbBuffInfo->length];

    // 批量释放,当遇到消息大于4K的分配时，直接释放
    if ( (SVS_FREE_MSG_BUFF_COUNT <= used_msg_buffer_count_)
       || (pMbBuffInfo->length >= CSignalBufferManager::MB_LEN_4K_BYTE))
    {
       (void)CSignalBufferManager::instance()->free_mb( used_msg_buffer_ );
        used_msg_buffer_ = NULL;
        used_msg_buffer_count_ = 0;
    }
}
//==================CSignalBufferTss END=================

