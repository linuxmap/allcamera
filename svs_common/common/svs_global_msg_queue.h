#ifndef _SVS_Global_Msg_Queue_h
#define _SVS_Global_Msg_Queue_h
//  ˳��ѭ������,
// ��ȥ��ʹ�ö���״̬,ת���Լ�ά��״̬

#include "svs_log_msg.h"
#include "svs_ace_header.h"

template <int32_t max_msg_queue = 200>

class CAC_Msg_Queue_Array
{
public:

    enum QUEUE_STATE
    {
        QUEUE_STATE_UNUSED  = 0,    // ����״̬, δʹ��
        QUEUE_STATE_USED    = 1,    // ����״̬, ��ʹ��
    };

    // ������
    struct ACE_Message_Queue_ITEM
    {
        QUEUE_STATE                   state; // ��Ϣ������״̬ȡֵ�� ö�� QUEUE_STATE
        ACE_Message_Queue <ACE_SYNCH> queue; // ��Ϣ����
    };

    CAC_Msg_Queue_Array()
    {
        brunning_ = true;
        next_index_ = 0;

        for (int32_t i = 0; i < max_msg_queue; i++)
        {
            queue_[i].state = QUEUE_STATE_UNUSED;
            queue_[i].queue.close();

            if ((ACE_Message_Queue_Base::DEACTIVATED != queue_[i].queue.state()) )
            {
                SVS_LOG ((SVS_LM_ERROR, "queue[%d] status[%d] is not DEACTIVATED.",
                            i, queue_[i].queue.state() ));
            }
        }
    }

    ~CAC_Msg_Queue_Array()
    {}

    /*
     * FUNCTION: int32_t alloc_queue()
     * DESCRIPTION: ����һ������
     * INPUT��
     *    ACE_Message_Queue<ACE_SYNCH> **pQueue -- ���ط������Ϣ����ָ�롣
     * OUTPUT:
     *    ʧ�ܷ���-1���ɹ����ض�������
     */
    int32_t alloc_queue(ACE_Message_Queue < ACE_SYNCH > **pQueue)
    {
        if ( NULL != *pQueue )
        {
            SVS_LOG((SVS_LM_ERROR, "create queue failed, *pQueue is not NULL."));
            return -1;
        }

        int32_t i = 0;

        ACE_GUARD_RETURN(ACE_Thread_Mutex, obj, mutex_, -1);

        for (i = next_index_; i < max_msg_queue; i++)
        {
            if (QUEUE_STATE_UNUSED == queue_[i].state)
            {
                const size_t wm = 3 * 1024 * 1024;
                queue_[i].queue.open(wm, wm);

                if ((ACE_Message_Queue_Base::ACTIVATED != queue_[i].queue.state()) )
                {
                    SVS_LOG ((SVS_LM_ERROR, "queue[%d] status[%d] is not ACTIVATED.",
                                i, queue_[i].queue.state() ));
                    continue;
                }
                queue_[i].state = QUEUE_STATE_USED;

                *pQueue = &queue_[i].queue;
                next_index_ = i+1;

                if (next_index_ == max_msg_queue)
                {
                    next_index_ = 0;
                }

                return i;
           }
        }

        // ��ͷ��ʼ��Ѱ���ö���
        for (i = 0; i < next_index_; i++)
        {
            //if (ACE_Message_Queue_Base::DEACTIVATED == queue_[i].state())
            if (QUEUE_STATE_UNUSED == queue_[i].state)
            {
                const size_t wm = 3 * 1024 * 1024;
                queue_[i].queue.open(wm, wm);

                if ((ACE_Message_Queue_Base::ACTIVATED != queue_[i].queue.state()) )
                {
                    SVS_LOG ((SVS_LM_ERROR, "queue[%d] status[%d] is not ACTIVATED.",
                                i, queue_[i].queue.state() ));
                    continue;
                }

                queue_[i].state = QUEUE_STATE_USED;

                *pQueue = &queue_[i].queue;
                next_index_ = i+1;
                if (next_index_ == max_msg_queue)
                {
                    next_index_ = 0;
                }

                return i;
           }
        }

        SVS_LOG((  SVS_LM_ERROR,
                    "apply queue failed: no free queue, next index:%d.",
                    next_index_));

        return -1;
    }

    // ������Ϣ�����������ж��е��ͷŲ���
    int32_t free_queue(int32_t index)
    {
        if ((0 > index) || (index >= max_msg_queue))
        {
            SVS_LOG((SVS_LM_ERROR, "free queue failed, invalid queueId[%d].", index ));
            return 0;
        }

        ACE_GUARD_RETURN(ACE_Thread_Mutex, obj, mutex_, -1);

        queue_[index].state = QUEUE_STATE_UNUSED;
        queue_[index].queue.close();

        return 0;
    }

    // ������Ϣ����������������в���
    int32_t send_msg(int32_t index, ACE_Message_Block *mb)
    {
        if ((0 > index) || (index >= max_msg_queue))
        {
            SVS_LOG((SVS_LM_ERROR, "put message into queue failed. invalid queueId[%d].", index ));
            return -1;
        }

        ACE_Time_Value tv = ACE_Time_Value(ACE_OS::gettimeofday())
                                + ACE_Time_Value(0, 10000);

        if (-1 == queue_[index].queue.enqueue_tail(mb, &tv))
        {
            SVS_LOG(( SVS_LM_ERROR,
                        "put message into queue[%d] failed. MsgCount[%d], ByteCount[%d] HighWaterMark[%d]",
                        index,
                        queue_[index].queue.message_count(),
                        queue_[index].queue.message_bytes(),
                        queue_[index].queue.high_water_mark()));

            return -1;
        }

        return 0;
    }

    // �����ж��н��л���
    int32_t stop()
    {
        brunning_ = false;

        ACE_GUARD_RETURN(ACE_Thread_Mutex, obj, mutex_, -1);

        for (int32_t i = 0; i < max_msg_queue; i++)
        {
            queue_[i].state = QUEUE_STATE_UNUSED;
            queue_[i].queue.close();
        }

        return 0;
    }

    bool running()
    {
        return brunning_;
    }

protected:

    bool brunning_;

    ACE_Message_Queue_ITEM  queue_[max_msg_queue];  // ��Ϣ��������
    int32_t                     next_index_;    // ����������Ϣ���е���һ����������

    ACE_Thread_Mutex        mutex_;
};

#endif //_SVS_Global_Msg_Queue_h
