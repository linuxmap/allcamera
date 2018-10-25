#ifndef _SVS_Timer_Ex_h
#define _SVS_Timer_Ex_h

#include "log/svs_log_msg.h"

typedef int32_t (*INT_TIMER_PTR)(void *pArg, void *pKey, int32_t timerId, int32_t Dummy);

/*
* CLASS: CAC_Timer_Ex
* DESCRIPTION: ��ʱ���࣬������Ҫ��ʱ��ʱ���ʹ����������������ʱ��
*/
template<class T>
class CAC_Timer_Ex : public ACE_Event_Handler
{
public:
    //typedef int32_t (*INT_TIMER_PTR)(void *pArg, T key, int32_t timerId, int32_t Dummy);

    CAC_Timer_Ex(ACE_Reactor *pReactor)    : ACE_Event_Handler(pReactor)
    {
//        assert(NULL != pReactor);
        if ( NULL == pReactor )
        {
            SVS_LOG( (SVS_LM_CRITICAL, "construct CAC_Timer_Ex object failed, pReactor is NULL.") );
        }

        timer_id_ = -1;
        p_func_ = NULL;
        p_arg_ = NULL;
        ACE_OS::memset(key_, 0, sizeof(T));
        this->dummy_ = 0;

        reactor(pReactor);
    }

    ~CAC_Timer_Ex()
    {
    }
private:
    CAC_Timer_Ex();
    CAC_Timer_Ex(const CAC_Timer_Ex &obj);
    const CAC_Timer_Ex& operator= (const CAC_Timer_Ex &obj);
public:
    int32_t time_id()
    {
        return timer_id_;
    }

    void timer_id(int32_t timerId)
    {
        timer_id_ = timerId;
    }

    int32_t init(int32_t timerId, void *pArg, INT_TIMER_PTR pFunc, T key, int32_t nDummy)
    {
        timer_id_ = timerId;
        p_arg_ = pArg;
        p_func_ = pFunc;
        ACE_OS::memcpy(key_, key, sizeof(T));
        dummy_ = nDummy;

        return 0;
    }

    int32_t stop();
public:
    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg)
    {
        return p_func_(p_arg_, key_, timer_id_, this->dummy_);
    }

    //virtual int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask);
protected:
    int32_t timer_id_;
    void *p_arg_;    //���û����͵Ķ���
    T key_;          //�û����õĶ�ʱ��ID, ͬһ���û����ܻ����ö����ʱ��
    int32_t dummy_;      //���û����͵Ķ���

    INT_TIMER_PTR p_func_;  //��ʱ���ص�����ָ��
};



/*
* CLASS: CAC_Timer_Manager
* DESCRIPTION: ��ʱ��������
*/

template<class T>
class CAC_Timer_Manager_Ex
{
public:

    typedef ACE_Hash_Map_Manager<int32_t, CAC_Timer_Ex<T> *, ACE_Null_Mutex> SVS_TIMER_MAP;
    /*
    * CONSTRUCTOR: CAC_Timer_Manager()
    * INPUT:
    *   int32_t timerCount --- Ԥ�ȷ���Ķ�ʱ�������������ж�ʱ���������
    */
    CAC_Timer_Manager_Ex(int32_t timerCount)
        : max_free_timers_(timerCount)
    {
        p_reactor_ = NULL;
    }

    ~CAC_Timer_Manager_Ex()
    {
        clear_timer();
    }

    //��������������һ�ε��ã���Ϊ������
    static CAC_Timer_Manager_Ex *instance()
    {
        static CAC_Timer_Manager_Ex timerManagerEx(300);
        return &timerManagerEx;
    }

    /*
    * FUNCTION: bool init()
    * DESCRIPTION: ��ʼ����ʱ�����������
    * INPUT:
    *    none
    * OUTPUT:
    *    �ɹ�����true��ʧ�ܷ���false.
    */
    bool init(ACE_Reactor *pReactor)
    {
//        assert(NULL == p_reactor_);
        if ( NULL != p_reactor_ )
        {
            SVS_LOG( (SVS_LM_CRITICAL, "init timerManager failed, p_reactor is not NULL.") );
            return false;
        }
        reactor(pReactor);

        CAC_Timer_Ex<T> *pTimer = NULL;

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, false);
        for(uint32_t i = 0; i < max_free_timers_; i++)
        {
            pTimer = new CAC_Timer_Ex<T>(p_reactor_);
            if(NULL == pTimer)
            {
                clear_timer();
                reactor(NULL);

                SVS_LOG( (SVS_LM_ERROR,"init timerManager failed."));
                return false;
            }

            free_timer_list_.insert_tail(pTimer);
        }

        SVS_LOG( (SVS_LM_DEBUG,"init timerManager success."));

        return true;
    }


    void end_reactor_event_loop()
    {
        if (NULL != p_reactor_)
        {
            p_reactor_->end_reactor_event_loop();
        }
    }
public:
    ACE_Reactor *reactor() {
        return p_reactor_;
    }


    /*
    * FUNCTION: int32_t alloc_timer()
    * DESCRIPTION: ���䶨ʱ��
    * INPUT:
    *    ACE_Time_Value &tv    --- ��ʱʱ��
    *    void *pArg            --- �û����͵Ķ���
    *    TIMER_CALL_BACK pFunc --- ��ʱ�ص�����
    *    int32_t eventId           --- �¼�ID, �û�����
    *    int32_t nDummy            --- ���û����͵Ķ���
    * OUTPUT:
    *    �ɹ����ض�ʱ��ID��ʧ�ܷ���-1.
    */
    int32_t alloc_timer(void *pArg,
        INT_TIMER_PTR pFunc,
        T key,
        const ACE_Time_Value &delay,
        const ACE_Time_Value &interval = ACE_Time_Value::zero,
        int32_t nDummy = 0 )
    {
        while(NULL == reactor())
        {
            ACE_OS::sleep(1);
        }

        CAC_Timer_Ex<T> *pTimer = NULL;
        {
            ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, -1);
            if(free_timer_list_.is_empty())
            {//û�п��ж�ʱ��������һ���µ�
                pTimer = new CAC_Timer_Ex<T>(p_reactor_);
                if (NULL == pTimer)
                {
                    return -1;
                }
            }
            else
            {
                pTimer = free_timer_list_.delete_head();
            }
        }

//        assert(NULL != pTimer);
        if ( NULL == pTimer )
        {
            SVS_LOG( ( SVS_LM_ERROR, "create timer failed." ));
            return -1;
        }

        pTimer->init(-1, pArg, pFunc, key, nDummy); //�����ȳ�ʼ�����ſ��Ե���reactor()->schedule_timer()

        int32_t timerId = reactor()->schedule_timer(pTimer, (void *)pTimer, delay, interval);
        if(-1 == timerId)
        {
            SVS_LOG( ( SVS_LM_ERROR, "create timer failed." ));

            ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, -1);
            free_timer_list_.insert_tail(pTimer);

            return -1;
        }

        pTimer->timer_id(timerId);
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, -1);
        timer_map_.bind(timerId, pTimer);
        return timerId;
    }

    /*
    * FUNCTION: int32_t free_timer()
    * DESCRIPTION: �ͷŶ�ʱ��
    * INPUT:
    *    int32_t timerId --- ��ʱ��ID.
    * OUTPUT:
    *    �ɹ����ض�ʱ��ID��ʧ�ܷ���-1.
    */
    int32_t free_timer(int32_t timerId)
    {
        CAC_Timer_Ex<T> *pTimer = NULL;
        int32_t ret = reactor()->cancel_timer(timerId, (const void **)&pTimer);
        if(1 != ret && 0 != ret)
        {
            SVS_LOG((SVS_LM_ERROR, "cancel timer[%d] failed.", timerId ));
            return -1;
        }

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, false);
        CAC_Timer_Ex<T> *pTmpTimer = NULL;
        timer_map_.unbind(timerId, pTmpTimer);
        if(NULL == pTimer)
        {
            pTimer = pTmpTimer;

        }

        if(NULL != pTimer)
        {
            if(free_timer_list_.size() < max_free_timers_)  //���ж�ʱ��û�дﵽ����
            {
                free_timer_list_.insert_tail(pTimer);
            }
            else
            {
                delete pTimer; ////���ж�ʱ������
                pTimer = NULL;
            }
        }

        return 0;
    }

    /*
    * FUNCTION: int32_t reset_timer_interval()
    * DESCRIPTION: �޸Ķ�ʱ����ʱ���ʱ��
    * INPUT:
    *    int32_t timerId --- ��ʱ��ID.
    *    const ACE_Time_Value &interval --- ��ʱ���ʱ��
    * OUTPUT:
    *    �ɹ����ض�ʱ��ID��ʧ�ܷ���-1.
    */
    int32_t reset_timer_interval(int32_t timerId, const ACE_Time_Value &intervalTime)
    {
//        assert(ACE_Time_Value::zero != interval);
        if ( ACE_Time_Value::zero == intervalTime )
        {
            SVS_LOG((SVS_LM_ERROR, "reset timer[%d] failed, intervalTime is error."));
            return -1;
        }
        return reactor()->reset_timer_interval(timerId, interval);
    }

    int32_t suspend_timer(int32_t timerId)
    {
        return 0;
    }

    int32_t resume_timer(int32_t timerId)
    {
        return 0;
    }
protected:
    void reactor(ACE_Reactor *pReactor)
    {
        p_reactor_ = pReactor;
    }

    /*
    * FUNCTION: int32_t clear_timer()
    * DESCRIPTION: ����ʱ��
    * INPUT:
    *    none
    * OUTPUT:
    *    none
    */
    void clear_timer()
    {
        CAC_Timer_Ex<T> *pTimer = NULL;

        ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, mutex_);
        while(!free_timer_list_.is_empty())
        {
            pTimer = free_timer_list_.delete_head();
            if(NULL != pTimer)
            {
                delete pTimer;
                pTimer = NULL;
            }
        }
    }
protected:
    const uint32_t  max_free_timers_;   //��������
    ACE_Reactor         *p_reactor_;        //��ʱ����Ӧ��
    SVS_TIMER_MAP       timer_map_;         //�Ѿ�����Ķ�ʱ��
    ACE_DLList<CAC_Timer_Ex<T> >    free_timer_list_;  //���ж�ʱ��
    ACE_Recursive_Thread_Mutex      mutex_;
};

#endif //_SVS_Timer_Ex_h


