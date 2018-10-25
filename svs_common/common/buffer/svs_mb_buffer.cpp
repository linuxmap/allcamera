#include "svs_ace_header.h"
#include "svs_mb_buffer.h"
#include "svs_log_msg.h"

CAC_Mb_Buffer::CAC_Mb_Buffer()
{
    SVS_LOG(( SVS_LM_DEBUG,
                            "[DESTROY] ~CAC_Mb_Buffer. this:0x%08x.",
                            this));
    this->mb_size_ = 256;
    this->free_mb_count_ = 300;
    free_mb_list_ = NULL;
    max_free_mb_ = 300;
}



CAC_Mb_Buffer::~CAC_Mb_Buffer()
{
    try
    {
        SVS_LOG(( SVS_LM_DEBUG,
                                "[DESTROY] ~CAC_Mb_Buffer. this:0x%08x.",
                                this));
        if (NULL != free_mb_list_)
        {
            (void)free_mb_list_->release();
        }
        free_mb_list_ = NULL;
        mb_size_ = 0;
        free_mb_count_ = 0;
        max_free_mb_ = 0;
    }
    catch(...)
    {
    }
}



int32_t CAC_Mb_Buffer::init(int32_t nBufSize, int32_t nNum)
{
    max_free_mb_ = (int16_t)nNum;
    this->mb_size_ = (size_t)nBufSize;
    this->free_mb_count_ = nNum;

    ACE_Message_Block *mb = NULL;
    for(int32_t i = 0; i < nNum; i++)
    {
        //ACE_NEW_RETURN(mb, ACE_Message_Block(mb_size_, ACE_Message_Block::MB_NORMAL), -1);
        try
        {
            mb = new ACE_Message_Block(mb_size_, ACE_Message_Block::MB_NORMAL);
        }
        catch(...)
        {
            if (NULL != mb)
            {
                delete mb;
                mb = NULL;
            }
            SVS_LOG(( SVS_LM_ERROR,
                                    "Allocate block memory failed"));
            return -1;
        }
        if(NULL == mb->data_block() || NULL == mb->base())
        {
            SVS_LOG(( SVS_LM_ERROR,
                        "init CAC_Mb_Buffer ACE_Message_Block failed. errorno: %d.",
                        ACE_OS::last_error()));
            delete mb;
            mb = NULL;
            return -1;
        }

        mb->cont(free_mb_list_);
        free_mb_list_ = mb;
        mb = NULL;
    }

    return 0;
}



ACE_Message_Block *CAC_Mb_Buffer::alloc_mb()
{
    ACE_Message_Block *mb = NULL;

    {
        ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, this->mutex_, NULL);

        if(NULL != free_mb_list_)
        {
            mb = this->free_mb_list_;
            this->free_mb_list_ = mb->cont();
            mb->cont(NULL);
            this->free_mb_count_--;

            return mb;
        }
    }

    try
    {
        mb = new ACE_Message_Block(mb_size_);
    }
    catch(...)
    {
        if (NULL != mb)
        {
            delete mb;
            mb = NULL;
        }
        SVS_LOG(( SVS_LM_ERROR,
                                "Allocate block memory failed"));
        return NULL;
    }

    if(NULL == mb->data_block() || NULL == mb->base())
    {
        SVS_LOG(( SVS_LM_ERROR,
                    "allocate ACE_Message_Block failed. errorno: %d.",
                    ACE_OS::last_error()));
        delete mb;
        mb = NULL;
    }
    return mb;
}


ACE_Message_Block *CAC_Mb_Buffer::alloc_mb(size_t mbSize)
{
    if(mbSize <= this->mb_size_)
    {
        return alloc_mb();
    }
    else
    {
        ACE_Message_Block *mb = NULL;
        try
        {
            mb = new ACE_Message_Block(mbSize);
        }
        catch(...)
        {
            if (NULL != mb)
            {
                delete mb;
                mb = NULL;
            }
            SVS_LOG(( SVS_LM_ERROR,
                                    "Allocate block memory failed"));
            return NULL;
        }

        if(NULL == mb->data_block() || NULL == mb->base())
        {
            SVS_LOG(( SVS_LM_ERROR,
                        "allocate ACE_Message_Block failed. errorno: %d.",
                        ACE_OS::last_error()));
            delete mb;
            mb = NULL;
        }
        return mb;
    }
}


int32_t CAC_Mb_Buffer::free_mb(ACE_Message_Block *mb)
{
    if(mb->size() != this->mb_size_)
    {
        (void)mb->release();
        mb = NULL;
        return 0;
    }

    mb->reset();

    {
        ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, mutex_, this->sys_error_exit());

        if(free_mb_count_ < max_free_mb_ || ACE_Message_Block::MB_NORMAL == mb->msg_type())
        {
            mb->cont(this->free_mb_list_);
            this->free_mb_list_ = mb;
            mb = NULL;
            free_mb_count_++;
            return 0;
        }
    }

    (void)mb->release();
    mb = NULL;
    return 0;
}


int32_t CAC_Mb_Buffer::reset(int32_t iCount)
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, pObj, mutex_, this->sys_error_exit());

    if(free_mb_count_ <= iCount)
    {
        return 0;
    }

    if(NULL == this->free_mb_list_)
    {
        return 0;
    }

    //SVS_LOG((SVS_LM_INFO, ("(%P|%t) 1 mb count: %d.\n"), free_mb_count_));

    ACE_Message_Block *blk = free_mb_list_;
    free_mb_list_ = NULL;
    this->free_mb_count_ = 0;

    ACE_Message_Block *mb = NULL;

    do
    {
        mb = blk;
        blk = mb->cont();
        mb->cont(NULL);

        if(ACE_Message_Block::MB_NORMAL == mb->msg_type())
        {
            this->free_mb_count_++;
            mb->cont(this->free_mb_list_);
            this->free_mb_list_ = mb;
        }
        else
        {
           (void) mb->release();
        }

        mb = NULL;
    }while(NULL != blk);

    mb = NULL;

    //SVS_LOG((SVS_LM_INFO, ("(%P|%t) 2 mb count: %d.\n"), free_mb_count_));

    return 0;
}


int32_t CAC_Mb_Buffer::sys_error_exit() const
{
    SVS_LOG(( SVS_LM_INFO, "*************** lock failed *************"));
    ACE_OS::exit(-1);
    return 0;
}


