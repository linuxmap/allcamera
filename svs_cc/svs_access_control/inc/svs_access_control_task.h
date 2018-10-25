#ifndef __ACCESS_CONTROL_TASK_H__
#define __ACCESS_CONTROL_TASK_H__

#include "svs_ace_header.h"
#include "svs_ac_common.h"
#include "svs_debug_accept_handler.h"

enum ACCESS_TASK_THREAD
{
    ACCESS_TASK_THREAD_TIMER = 0,
    ACCESS_TASK_THREAD_DEBUG = 1,
    ACCESS_TASK_THREAD_MAX
};

class CAccessControlTask : public ACE_Task<ACE_MT_SYNCH>
{
public:
    static CAccessControlTask& instance()
    {
        static CAccessControlTask objAccessControlTask;
        return objAccessControlTask;
    }
    virtual ~CAccessControlTask();

    int32_t open(void *pArg);
    int32_t close(u_long);

    int32_t svc(void);
    int32_t stop();
protected:
    CAccessControlTask();
private:
    int32_t open_debug_acceptor( void );
    int32_t get_index();
    int32_t timer_event_loop();
    int32_t debug_thread();
private:
    typedef ACE_Acceptor <CSVS_Debug_Accept_Handler, ACE_SOCK_ACCEPTOR> SVS_LOG_ACCEPTOR;
    bool              brunning_;
    int32_t           _thr_index;
    ACE_Recursive_Thread_Mutex mutex_;
    ACE_Reactor      *p_timer_reactor_;
    uint16_t          remote_debug_port_;
    ACE_Reactor      *p_debug_reactor_;
    SVS_LOG_ACCEPTOR *p_debug_acceptor_;
};
#endif //__ACCESS_CONTROL_TASK_H__

