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
�� �� ��  : init
��������  : ��ʼ����Ϣ������
�������  : mbCount :   ��Ϣ�����
            mbSize  :   ��Ϣ���С
            mbType  :   ��Ϣ����
            mbLen   :   ��Ϣ��������
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/

int32_t CSignalBuffer::init(   uint32_t mbCount,
                            uint32_t mbSize,
                            uint8_t mbType,
                            uint8_t mbLen)
{
    //�ѳ�ʼ��������ʧ��
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

    //�����ڴ�
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

    //����ʧ�ܣ����ԭ���ѷ�����ڴ�
    if ( i != mbCount )
    {
        (void)clear();
        // clear()�ӿڼ����ͷ���������Ŀռ䣬��ʱ����lint�澯
        return -1;   //lint !e429
    }

    inited_flag_ = true;

    // �ɹ����أ����пռ�������ʱ�ͷţ���������lint�澯
    return 0;  //lint !e429
}


/*****************************************************************************
�� �� ��  : alloc_mb
��������  : �ӻ������з��������Ϣ�飬�����ܵķ���
�������  : allocCount  :   ��Ϣ�����
�������  : ��
�� �� ֵ  : �ɹ�,���� ������Ϣ��ָ�루��������ָ������Ϣ������� ���򷵻�NULL
�޸���ʷ  :
*****************************************************************************/

ACE_Message_Block *CSignalBuffer::alloc_mb( uint32_t allocCount )
{
    //����Ƿ��ѳ�ʼ��
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

        //û�п��п飬����ʧ��
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
�� �� ��  : alloc_mb
��������  : �ӻ������з��������Ϣ��
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� ������Ϣ��ָ�� ���򷵻�NULL
�޸���ʷ  :
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

        //û�п��п飬����ʧ��
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
�� �� ��  : free_mb
��������  : ��ָ������Ϣ����뻺������
�������  : mb :   ��Ϣ��ָ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 �����򷵻�-1
�޸���ʷ  :
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
�� �� ��  : clear
��������  : ��ջ�����������Ϣ��
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 �����򷵻�-1
�޸���ʷ  :
*****************************************************************************/

int32_t CSignalBuffer::clear( )
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, mutex_, -1);

    //��ջ������е�������Ϣ��
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
�� �� ��  : init
��������  : ��ʼ��������������
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSignalBufferManager::init()
{
    //�Ѿ���ʼ�����ˣ�ֱ�ӷ���ʧ��
    if ( inited_flag_ )
    {
        SVS_LOG((SVS_LM_ERROR,
        "Initialize the message block manager failed."
        "It has been initialized."));
        return -1;
    }

    uint32_t buffCount[MB_TYPE_MAX_VALUE][MB_LEN_MAX_VALUE] =
        {
            { 40000, 20000, 10000, 160, 100 }, //����������Ϣ����
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

            //����������
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

            //��ʼ��������
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

    //��ʼ��ʧ�ܣ���������
    if ( initFailureFlag )
    {
        (void)clear();
        return -1;
    }

    inited_flag_ = true;
    return 0;

}

/*****************************************************************************
�� �� ��  : clear
��������  : ��ջ����������ж���
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 �����򷵻�-1
�޸���ʷ  :
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
�� �� ��  : alloc_mb
��������  : �ӻ������з��������Ϣ��
�������  : mbType  :   ��Ϣ������
            mbSize  :   ��Ϣ���С
            mbCount :   ��Ϣ�����
�������  : ��
�� �� ֵ  : �ɹ�,���� ������Ϣ��ָ�� ���򷵻�NULL
�޸���ʷ  :
*****************************************************************************/

ACE_Message_Block *CSignalBufferManager::alloc_mb(uint8_t mbType,
                                                    uint32_t mbSize,
                                                    uint32_t mbCount ) const
{
    //��ȡ��Ϣ������ָ��
    CSignalBuffer *pMbBuffer = get_mb_buffer( mbType, mbSize );
    if ( NULL == pMbBuffer )
    {
        return NULL;
    }

    ACE_Message_Block *mb = pMbBuffer->alloc_mb( mbCount );

    return mb;
}

/*****************************************************************************
�� �� ��  : free_mb
��������  : ��ָ������Ϣ����뻺������
�������  : mb :   ��Ϣ��ָ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 �����򷵻�-1
�޸���ʷ  :
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
�� �� ��  : get_mb_buffer
��������  : ��ȡָ������/��С����Ϣ�����ڵĻ�����
�������  : mbType  :   ��Ϣ����
            mbSize  :   ��Ϣ��С
�������  : ��
�� �� ֵ  : �ɹ�,���� ������ָ�� ���򷵻�NULL
�޸���ʷ  :
*****************************************************************************/
CSignalBuffer *CSignalBufferManager::get_mb_buffer(  uint8_t mbType,
                                                        uint32_t mbSize ) const
{
    switch ( mbType )
    {
    case MB_TYPE_NORMAL_EVENT:  //����������Ϣ
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
�� �� ��  : clear
��������  : �ͷ�������Դ
�������  : ��
�������  : ��
�� �� ֵ  : ��
�޸���ʷ  :
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
�� �� ��  : init
��������  : ��ʼ��
�������  : ��
�������  : ��
�� �� ֵ  : ��
�޸���ʷ  :
*****************************************************************************/
void CSignalBufferTss::init()
{
    //clear();
    reset();
}

/*****************************************************************************
�� �� ��  : reset
��������  : ��λ���г�Ա����
�������  : ��
�������  : ��
�� �� ֵ  : ��
�޸���ʷ  :
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
�� �� ��  : alloc_msg_mb
��������  : ������Ϣ����������Ϣ������
�������  : mbType  :   ��Ϣ����
            mbSize :    ��Ϣ��С
�������  : ��
�� �� ֵ  : �ɹ�,���� ���뵽����Ϣ������ ���򷵻�NULL
�޸���ʷ  :
*****************************************************************************/
ACE_Message_Block *
CSignalBufferTss::alloc_mb( uint8_t mbType,
                                          uint32_t mbSize )
{
    ACE_Message_Block *mb = NULL;

    //����Ϣ��Сת������Ϣ��������
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
�� �� ��  : free_msg_mb
��������  : �ͷ�ָ������Ϣ��
�������  : freeMb  :   Ҫ�ͷŵ���Ϣ��
�������  : ��
�� �� ֵ  : ��
�޸���ʷ  :
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

    // �����ͷ�,��������Ϣ����4K�ķ���ʱ��ֱ���ͷ�
    if ( (SVS_FREE_MSG_BUFF_COUNT <= used_msg_buffer_count_)
       || (pMbBuffInfo->length >= CSignalBufferManager::MB_LEN_4K_BYTE))
    {
       (void)CSignalBufferManager::instance()->free_mb( used_msg_buffer_ );
        used_msg_buffer_ = NULL;
        used_msg_buffer_count_ = 0;
    }
}
//==================CSignalBufferTss END=================

