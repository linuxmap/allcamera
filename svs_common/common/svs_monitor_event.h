#ifndef _SVS_Monitor_Event_h
#define _SVS_Monitor_Event_h

#include <list>
#include "svs_log_msg.h"

using namespace std;

template <class T>
class CAC_Monitor_Event
{
public:
    CAC_Monitor_Event()
    {
        this->b_running_ = true;
        this->p_T_ = NULL;
    }

    ~CAC_Monitor_Event()
    {
        this->p_T_ = NULL;
    }

    int32_t init(T *pT)
    {
        this->p_T_ = pT;
        return 0;
    }

    int32_t handle_event_loop()
    {
        if(NULL == this->p_T_)
        {
            SVS_LOG( (SVS_LM_WARNING, "event is not inited."));
            return 0;
        }

        SVS_LOG( (SVS_LM_DEBUG, "monitor thread running..." ));

        while(b_running_)
        {
            ACE_Message_Block *mb = NULL;
            int32_t msgCount = this->event_queue_.dequeue_head(mb);
            if(-1 == msgCount)
            {
                SVS_LOG( (SVS_LM_ERROR, "get message from the queue failed." ) );
                break;
            }

            msgCount += 1;
            for(int32_t i = 1; i <= msgCount; i++)
            {
                int32_t ret = this->p_T_->handle_monitor_event(mb);
                if(0 == ret)
                {
                    mb->release();
                }
                else
                {
                    //还没有处理成功再次处理
                    if(-1 == this->event_queue_.enqueue_tail(mb))
                    {
                        SVS_LOG( (SVS_LM_ERROR, "put message into the queue failed." ) );
                        return 0;
                    }
                }

                mb = NULL;
                if(i < msgCount && -1 == this->event_queue_.dequeue_head(mb))
                {
                    SVS_LOG( (SVS_LM_ERROR, "get message from the queue failed." ) );
                    break;
                }
            }

            if(!this->event_queue_.is_empty())
            ACE_OS::sleep(10);
        }
        this->event_queue_.close();

        SVS_LOG( (SVS_LM_DEBUG, "monitor thread exit." ) );
        return 0;
    }

    int32_t handle_event_list_loop()
    {
        const int32_t nWaitTime = 60;
        typedef list<ACE_Message_Block *> MB_LIST;

        MB_LIST mblist;
        ACE_Time_Value *pTv = NULL;
        ACE_Time_Value waitTime;
        ACE_Message_Block *mb = NULL;
        int32_t ret = 0;

        while(b_running_)
        {
            int32_t msgCount = 1;
            mb = NULL;
            while(msgCount)
            {
                if(NULL != pTv)
                {
                    pTv->set(ACE_OS::gettimeofday().sec() + nWaitTime);
                }

                msgCount = event_queue_.dequeue_head(mb, pTv);
                if(-1 == msgCount)
                {
                    if(ETIME == ACE_OS::last_error() || EAGAIN == ACE_OS::last_error())
                    {
                        SVS_LOG( (SVS_LM_ERROR, "get message from the queue timeout." ) );
                        break;
                    }

                    SVS_LOG( (SVS_LM_DEBUG, "monitor thread exit." ) );
                    event_queue_.close();
                    return 0;
                }

                mblist.push_back(mb);
                mb = NULL;
                pTv = &waitTime;
            }

            mb = NULL;
            MB_LIST::iterator iter = mblist.begin();
            for(; iter != mblist.end(); )
            {
                mb = *iter;
                ret = this->p_T_->handle_monitor_event(mb);
                if(0 == ret)
                {
                    iter = mblist.erase(iter);
                    mb->release();
                }
                else
                    iter++;

                mb = NULL;
            }

            if(mblist.empty())
                pTv = NULL;
            else
                pTv = &waitTime;
        }

        this->event_queue_.close();

        SVS_LOG( (SVS_LM_DEBUG, "monitor thread exit." ) );

        return 0;
    }

    ACE_Message_Queue<ACE_SYNCH> *event_queue()
    {
        return &this->event_queue_;
    }

    int32_t stop()
    {
        this->b_running_ = false;
        this->event_queue_.close();
        return 0;
    }

protected:
    bool b_running_;
    T *p_T_;
    ACE_Message_Queue<ACE_SYNCH> event_queue_;
};

#endif //_SVS_Monitor_Event_h



