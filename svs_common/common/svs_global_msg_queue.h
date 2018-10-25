#ifndef _SVS_Global_Msg_Queue_h
#define _SVS_Global_Msg_Queue_h
//  顺序循环分配,
// 并去掉使用队列状态,转而自己维护状态

#include "svs_log_msg.h"
#include "svs_ace_header.h"

template <int32_t max_msg_queue = 200>

class CAC_Msg_Queue_Array
{
public:

    enum QUEUE_STATE
    {
        QUEUE_STATE_UNUSED  = 0,    // 队列状态, 未使用
        QUEUE_STATE_USED    = 1,    // 队列状态, 已使用
    };

    // 队列项
    struct ACE_Message_Queue_ITEM
    {
        QUEUE_STATE                   state; // 消息队列项状态取值见 枚举 QUEUE_STATE
        ACE_Message_Queue <ACE_SYNCH> queue; // 消息队列
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
     * DESCRIPTION: 分配一个队列
     * INPUT：
     *    ACE_Message_Queue<ACE_SYNCH> **pQueue -- 返回分配的消息队列指针。
     * OUTPUT:
     *    失败返回-1，成功返回队列索引
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

        // 从头开始搜寻可用队列
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

    // 根据消息队列索引进行队列的释放操作
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

    // 根据消息队列索引进行入队列操作
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

    // 将所有队列进行回收
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

    ACE_Message_Queue_ITEM  queue_[max_msg_queue];  // 消息队列数组
    int32_t                     next_index_;    // 将被分配消息队列的下一个队列索引

    ACE_Thread_Mutex        mutex_;
};

#endif //_SVS_Global_Msg_Queue_h
