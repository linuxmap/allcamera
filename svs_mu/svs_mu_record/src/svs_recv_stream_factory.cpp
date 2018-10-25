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
�� �� ��  : init
��������  : ��ʼ��
�������  : uint32_t nMaxLens:���ͷ����
�������  : ��
�� �� ֵ  : �ɹ�:0  ʧ��:-1
�޸���ʷ  :
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

    // ����������������ָ�붼�ŵ�����������
    char *pch = p_object_buffer_;
    for (uint32_t i = 0; i < ulMaxLens; i++, pch += object_size_)
    {
        //�Ƚ������������ָ�������ж���
        (void)free_list_.insert_tail( pch );

        device_using_thread_vector_.push_back( 0 );
    }

    // ÿ���߳����ҵľ�ͷ����ʼ��
    for (uint32_t i = 0; i < max_recv_thread_; i++)
    {
        //�������̴߳���ľ�ͷ����ʼ��ʱΪ0
        device_num_pre_thread_vector_.push_back( 0 );
    }

    SVS_LOG((SVS_LM_WARNING, "Success to init recv stream factory."));

    // initʧ�ܺ���󲻿��ã�������ڴ涼���������������ͷ�,�˴�����lint�澯
    return SVS_RESULT_OK;  //lint !e429
}

/*****************************************************************************
�� �� ��  : create
��������  : ����һ������������
�������  : uint32_t
�������  : ��
�� �� ֵ  : CAC_Recv_Stream*:����������ָ��
�޸���ʷ  :
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

        // �ӿ�������ͷȡ��һ����������ָ��,�������������������,���������õ���Ӧ������ʹ�õ���������������
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
        //����SVSЭ�������
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
�� �� ��  : destroy
��������  : �ͷ�һ������������
�������  : CAC_Recv_Stream*:����������ָ��
�������  : ��
�� �� ֵ  : �ɹ�:0  ʧ��:-1
�޸���ʷ  :
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

    // ����Ҫ�ͷŵ���������ָ��ŵ������б���
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


