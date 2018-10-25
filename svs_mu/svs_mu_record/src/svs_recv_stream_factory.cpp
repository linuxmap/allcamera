#include "svs_ace_header.h"
#include "vms/vms.h"
#include "svs_log_msg.h"
#include "svs_stat_manager.h"
#include "svs_recv_rt_media_buffer.h"
#include "svs_media_recv_manager.h"
#include "svs_recv_stream_factory.h"

CAC_Recv_Stream_Factory::CAC_Recv_Stream_Factory()
                  : object_size_(sizeof(CAC_Recv_Stream))
{
    max_lens_           = 0;
    max_recv_thread_    = 0;
    p_object_buffer_    = NULL;
}

CAC_Recv_Stream_Factory::~CAC_Recv_Stream_Factory()
{
    try
    {
        SVS_DELETE( p_object_buffer_, SVS_DELETE_MULTI );
    }
    catch(...)
    {
    }

}

/*****************************************************************************
函 数 名  : init
功能描述  : 初始化
输入参数  : uint32_t nMaxLens:最大镜头数量
输出参数  : 无
返 回 值  : 成功:0  失败:-1
修改历史  :
*****************************************************************************/
int32_t CAC_Recv_Stream_Factory::init(uint32_t ulMaxLens, uint32_t ulMaxRcvThread)
{
    SVS_LOG((SVS_LM_DEBUG,
        "Begin to init recv stream factory, "
        "max camera num[%d], max recv thread num[%d].",
        ulMaxLens,
        ulMaxRcvThread));

    max_lens_         = ulMaxLens;
    max_recv_thread_  = ulMaxRcvThread;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);

    try
    {
        p_object_buffer_ = new char[object_size_ * ulMaxLens];
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL, "Allocating recv stream object poll failed."));
        return SVS_RESULT_FAILURE;
    }

    if ( NULL == p_object_buffer_ )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Allocating recv stream object poll failed."));
        return SVS_RESULT_FAILURE;
    }

    // 将创建的收流对象指针都放到空闲链表中
    char *pch = p_object_buffer_;
    for (uint32_t i = 0; i < ulMaxLens; i++, pch += object_size_)
    {
        //先将接收流对象的指针放入空闲队列
        (void)free_list_.insert_tail( pch );

        device_using_thread_vector_.push_back( 0 );
    }

    // 每个线程所挂的镜头数初始化
    for (uint32_t i = 0; i < max_recv_thread_; i++)
    {
        //接收流线程处理的镜头数初始化时为0
        device_num_pre_thread_vector_.push_back( 0 );
    }

    SVS_LOG((SVS_LM_WARNING, "Success to init recv stream factory."));

    // init失败后对象不可用，分配的内存都放在析构函数中释放,此处屏蔽lint告警
    return SVS_RESULT_OK;  //lint !e429
}

/*****************************************************************************
函 数 名  : create
功能描述  : 创建一个接收流对象
输入参数  : uint32_t
输出参数  : 无
返 回 值  : CAC_Recv_Stream*:接收流对象指针
修改历史  :
*****************************************************************************/
CAC_Recv_Stream *CAC_Recv_Stream_Factory::create()
{
    SVS_TRACE();

    if ( NULL == p_object_buffer_ )
    {
        SVS_LOG( (SVS_LM_CRITICAL, "The pointer p_object_buffer_ is NULL.") );
        return NULL;
    }

    char *pch                        = NULL;
    uint32_t nIndex             = 0;
    uint32_t MostFreeThreadNo = 0;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, NULL);
        if (free_list_.is_empty())
        {
            SVS_LOG((SVS_LM_ERROR,
                "Fail to create recv stream object because free recv stream list is empty."));
            return NULL;
        }

        // 从空闲链表头取出一个收流对象指针,计算该收流对象索引号,并将其设置到对应的正在使用的收流对象数组中
        pch = free_list_.delete_head();
        nIndex = (uint32_t)(pch - p_object_buffer_) / object_size_;

        MostFreeThreadNo = most_free_thread_no();
        device_using_thread_vector_[ nIndex ] = MostFreeThreadNo;
        device_num_pre_thread_vector_[ MostFreeThreadNo ]++;
    }

    CAC_Recv_Stream *pStream = NULL;
    ACE_Reactor *pReactor = SVS_Media_Recv_Manager::instance().getRecvReactor(MostFreeThreadNo);
    CSVS_Recv_RT_Media_Buffer *pBuffer = SVS_Media_Recv_Manager::instance().getMediaBuffer(MostFreeThreadNo);
    if ((NULL == pReactor) || (NULL == pBuffer))
    {
        return NULL;
    }

    try
    {
        //符合SVS协议编码器
        pStream = new (pch) CAC_Recv_Stream(pReactor, pBuffer, MostFreeThreadNo);

    }
    catch(...)
    {
        SVS_DELETE(pStream);
    }

    SVS_LOG((SVS_LM_INFO, "Success to create a recv stream object."));
    return pStream;
}

/*****************************************************************************
函 数 名  : destroy
功能描述  : 释放一个接收流对象
输入参数  : CAC_Recv_Stream*:接收流对象指针
输出参数  : 无
返 回 值  : 成功:0  失败:-1
修改历史  :
*****************************************************************************/
int32_t CAC_Recv_Stream_Factory::destroy(CAC_Recv_Stream *pStream)
{
    SVS_TRACE();
    if (NULL == pStream)
    {
        SVS_LOG((SVS_LM_INFO, "Success to destroy recv stream object pointer."));
        return SVS_RESULT_OK;
    }
    if ( NULL == p_object_buffer_ )
    {
        SVS_LOG( (SVS_LM_CRITICAL, "The pointer p_object_buffer_ is NULL.") );
        return SVS_RESULT_FAILURE;
    }

    // 将需要释放的收流对象指针放到空闲列表中
    pStream->~CAC_Recv_Stream();

    char *pch = (char *)pStream;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    (void)free_list_.insert_tail(pch);
    uint32_t nIndex = (uint32_t)(pch - p_object_buffer_) / object_size_;

    uint32_t ThreadNo = device_using_thread_vector_[ nIndex ];
    device_using_thread_vector_[ nIndex ] = 0;
    device_num_pre_thread_vector_[ ThreadNo ]--;

    SVS_LOG((SVS_LM_INFO, "Success to destroy recv stream object pointer."));
    return SVS_RESULT_OK;
}

uint32_t CAC_Recv_Stream_Factory::most_free_thread_no()
{
    uint32_t FreeThreadNo = 0;
    uint32_t MinDeviceNum = RECORD_MAX_LENS_NUM;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, 0 );
    vector<uint32_t>::iterator it = device_num_pre_thread_vector_.begin();
    for ( uint32_t i = 0; it != device_num_pre_thread_vector_.end(); i++, ++it )
    {
        if ( *it < MinDeviceNum )
        {
            FreeThreadNo = i;
            MinDeviceNum = *it;
        }

        if (0 == MinDeviceNum)
        {
            break;
        }
    }

    return FreeThreadNo;
}


