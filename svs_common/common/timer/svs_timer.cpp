//////////////////////////////////////////////////////////////////////
//    All rights reserved.
//
//    Author        : Stone Shi
//    Description    : 定时器实现文件
//    Version        : 1.0
//    Create        : 2004-8-31
//
//////////////////////////////////////////////////////////////////////

#include <ace/OS.h>
#include <ace/Auto_Ptr.h>
#include <ace/FILE_IO.h>
#include <ace/Log_Msg.h>
#include <ace/Sched_Params.h>
#include <ace/Signal.h>
#include <ace/Thread_Manager.h>
#include <ace/INET_Addr.h>
#include <ace/SOCK_Acceptor.h>
#include <ace/SOCK_Stream.h>
#include <ace/SOCK_Dgram.h>
#include <ace/Log_Record.h>
#include <ace/Handle_Set.h>
#include <ace/Timer_Queue.h>
#include <ace/OS_NS_sys_time.h>
#include <ace/Select_Reactor.h>
#include <ace/Reactor.h>
#include <ace/Timer_Heap.h>
#include <ace/FILE_Connector.h>
#include <ace/Hash_Map_Manager_T.h>
#include <ace/Hash_Map_Manager_T.h>
#include <ace/Synch.h>
#include <ace/Svc_Handler.h>
#include <ace/Connector.h>
#include <ace/Reactor.h>
#include <ace/Acceptor.h>
#include "svs_timer.h"
#include "svs_log_msg.h"

//=============================CAC_Timer BEGIN===============================
CAC_Timer::CAC_Timer(ACE_Reactor *pReactor) : ACE_Event_Handler(pReactor)
{
//    assert(NULL != pReactor);
    if ( NULL == pReactor )
    {
        SVS_LOG((SVS_LM_ERROR, "construct CAC_Timer object, pReactor is NULL."));
    }

    timer_id_       = -1;
    p_func_         = NULL;
    p_arg_          = NULL;

    event_id_       = -1;
    this->dummy_    = 0;
    timer_type_ = TIMER_TYPE_ONCE;
    this->reactor(pReactor);
}

CAC_Timer::~CAC_Timer()
{
    p_arg_  = NULL;
    p_func_ = NULL;
}

int32_t CAC_Timer::init(
    int32_t timerId,
    void *pArg,
    TIMER_CALL_BACK pFunc,
    int32_t eventId,
    int32_t nDummy ,int32_t timerType)
{
    timer_id_ = timerId;
    p_arg_ = pArg;
    p_func_ = pFunc;
    event_id_ = eventId;
    dummy_ = nDummy;
    timer_type_ = timerType;
    return 0;
}
int32_t CAC_Timer::handle_timeout(const ACE_Time_Value & /*tv*/, const void * /*arg*/)
{
    if (NULL == p_func_)
    {
        SVS_LOG ((SVS_LM_ERROR, "Call back function is null."));
        return 0;
    }

    p_func_(p_arg_, event_id_, timer_id_, this->dummy_);
    if(TIMER_TYPE_ONCE == timer_type_)
    {
        (void)CAC_Timer_Manager::instance()->free_timer(this);
    }
    return 0;
}

//==============================CAC_Timer END================================

//=========================CAC_Timer_Manager BEGIN===========================

CAC_Timer_Manager *CAC_Timer_Manager::p_timer_manager_ = NULL;


CAC_Timer_Manager::CAC_Timer_Manager(int32_t timerCount): max_free_timers_((uint32_t)timerCount)
{
    p_reactor_ = NULL;
}

CAC_Timer_Manager::~CAC_Timer_Manager()
{
    try
    {
        clear_timer();
    }
    catch(...){}

    p_reactor_ = NULL;
}

bool CAC_Timer_Manager::init(ACE_Reactor *pReactor)
{
//    assert(NULL == p_reactor_);
    if (NULL != p_reactor_)
    {
        SVS_LOG ((SVS_LM_ERROR, "init timerManager failed."));
        return false;
    }

    reactor(pReactor);

    CAC_Timer *pTimer = NULL;


    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, false);
    for(uint32_t i = 0; i < max_free_timers_; i++)
    {
        try
        {
            pTimer = new CAC_Timer(p_reactor_);
        }
        catch(...)
        {
            SVS_LOG( (SVS_LM_DEBUG,"Initialize timer manger failed."));

            clear_timer();
            reactor(NULL);

            return false;
        }

        (void)free_timer_list_.insert_tail(pTimer);
    }

    SVS_LOG( (SVS_LM_DEBUG,"Initialize timer manager succussfully."));

    return true; //lint !e429 定时器对象申请之后放到链表中统一管理，不再释放
}

void CAC_Timer_Manager::clear_timer()
{
    CAC_Timer *pTimer = NULL;

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

void CAC_Timer_Manager::end_reactor_event_loop()
{
    if(NULL != p_reactor_)
    {
        (void)p_reactor_->end_reactor_event_loop();
    }
}


CAC_Timer *CAC_Timer_Manager::allocate_timer_object()
{
    CAC_Timer *pTimer = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, NULL);
    if(free_timer_list_.is_empty())
    {
        try
        {
            pTimer = new CAC_Timer(p_reactor_);
        }
        catch(...)
        {
            SVS_LOG( (SVS_LM_DEBUG,"create timer failed."));
            return NULL;
        }
    }
    else
    {
        pTimer = free_timer_list_.delete_head();
    }

    return pTimer;
}


int32_t CAC_Timer_Manager::alloc_timer(void *pArg, TIMER_CALL_BACK pFunc, int32_t eventId, const ACE_Time_Value &delay, const ACE_Time_Value &interval_, int32_t nDummy)
{
    while(NULL == reactor())
    {
        (void)ACE_OS::sleep(1);
    }

    CAC_Timer *pTimer = this->allocate_timer_object();
    if(NULL == pTimer)
    {
        SVS_LOG((SVS_LM_ERROR, "allocate timer failed."));
        return -1;
    }

    if(ACE_Time_Value::zero == interval_)
    {
        (void)pTimer->init(-1, pArg, pFunc, eventId, nDummy,TIMER_TYPE_ONCE); //必须先初始化，才可以调用reactor()->schedule_timer()
    }
    else
    {
        (void)pTimer->init(-1, pArg, pFunc, eventId, nDummy,TIMER_TYPE_CYCLE); //必须先初始化，才可以调用reactor()->schedule_timer()
    }

    int32_t timerId = reactor()->schedule_timer(pTimer, (void *)pTimer, delay, interval_);
    if(-1 == timerId)
    {
        SVS_LOG((SVS_LM_ERROR, "schedule timer failed."));

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, -1);
        (void)free_timer_list_.insert_tail(pTimer);

        return -1;
    }

    pTimer->timer_id(timerId);

    //ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, -1);
    //timer_map_.bind(timerId, pTimer);

    return timerId;
}

int32_t CAC_Timer_Manager::reset_timer_interval(int32_t timerId, const ACE_Time_Value &intervalTime)
{
//    assert(ACE_Time_Value::zero != interval);
    if (ACE_Time_Value::zero == intervalTime )
    {
        SVS_LOG((SVS_LM_ERROR, "reset timer[%d] failed, intervalTime is error."));
        return -1;
    }
    return reactor()->reset_timer_interval(timerId, intervalTime);
}

//释放定时器
int32_t CAC_Timer_Manager::free_timer(int32_t timerId)
{
    CAC_Timer *pTimer = NULL;
    void **pArg = (void **)(&pTimer);
    int32_t ret = reactor()->cancel_timer(timerId, const_cast<const void **>(pArg));
    if(1 != ret)
    {
//        ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) %N, %l\n\tFail to free timer[%d].\n"), timerId));
//        assert(false);
        SVS_LOG ((SVS_LM_ERROR, "Failed to free timer [%d], ret value[%d].", timerId, ret));
        return -1;
    }

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, false);
    //CAC_Timer *pTmpTimer = NULL;
    //timer_map_.unbind(timerId, pTmpTimer);
    //if(NULL == pTimer)
    //{
    //    pTimer = pTmpTimer;
    //}
    if(NULL != pTimer)
    {
//        if(free_timer_list_.size() < max_free_timers_)  //空闲定时器没有达到上限
//        {
        (void)free_timer_list_.insert_tail(pTimer);
//        }
//        else
//        {
//            delete pTimer; ////空闲定时器过多
//            pTimer = NULL;
//        }
    }

    return 0;
}

// 释放一次性定时器
int32_t CAC_Timer_Manager::free_timer(CAC_Timer *pTimer)
{
    if(NULL != pTimer)
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, false);
//        if(free_timer_list_.size() < max_free_timers_)  //空闲定时器没有达到上限
//        {
        (void)free_timer_list_.insert_tail(pTimer);
//        }
//        else
//        {
//            delete pTimer; ////空闲定时器过多
//            pTimer = NULL;
//        }
    }

    return 0;
}

int32_t CAC_Timer_Manager::suspend_timer(int32_t /*timerId*/)const
{
    return 0;
}

int32_t CAC_Timer_Manager::resume_timer(int32_t /*timerId*/)const
{
    return 0;
}

//==========================CAC_Timer_Manager END============================






