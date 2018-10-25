#ifndef _svs_timer_h
#define _svs_timer_h
#include "svs_ace_header.h"

typedef void (*TIMER_CALL_BACK)(void *pArg, int32_t eventId, int32_t timerId, int32_t Dummy);

enum TIMER_TYPE
{
    TIMER_TYPE_ONCE,
    TIMER_TYPE_CYCLE
};


/*
* CLASS: CAC_Timer
* DESCRIPTION: ��ʱ���࣬�����Ҫ��ʱ��ʱ���ʹ����������������ʱ��
*/
class CAC_Timer : public ACE_Event_Handler
{
public:
    CAC_Timer(ACE_Reactor *pReactor);
    virtual ~CAC_Timer();
private:
    CAC_Timer();
    CAC_Timer(const CAC_Timer &obj);
    const CAC_Timer& operator= (const CAC_Timer &obj);
public:
    int32_t time_id() { return timer_id_; }
    void timer_id(int32_t timerId) { timer_id_ = timerId; }

    int32_t init(int32_t timerId, void *pArg, TIMER_CALL_BACK pFunc, int32_t eventType, int32_t nDummy,int32_t timerType);

    int32_t stop();
protected:
    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);
public:
    enum TIMER_VALUE
    {
        TIMER_VALUE_1       = -1,   //û�����ö�ʱ��
        TIMER_VALUE_SETTING = -2,   //��һ���߳���������
    };
protected:
    int32_t timer_id_;
    void *p_arg_;    //���û����͵Ķ���
    int32_t event_id_;   //�û����õĶ�ʱ��ID, ͬһ���û����ܻ����ö����ʱ��
    int32_t dummy_;      //���û����͵Ķ���
    TIMER_CALL_BACK p_func_;  //��ʱ���ص�����ָ��
    int32_t timer_type_;
};
/*
* CLASS: CAC_Timer_Manager
* DESCRIPTION: ��ʱ��������
*/
class CAC_Timer_Manager
{
public:
    typedef ACE_Hash_Map_Manager<int32_t, CAC_Timer *, ACE_Null_Mutex> SVS_TIMER_MAP;
    /*
    * CONSTRUCTOR: CAC_Timer_Manager()
    * INPUT:
    *   int32_t timerCount --- Ԥ�ȷ���Ķ�ʱ�������������ж�ʱ���������
    */
    CAC_Timer_Manager(int32_t timerCount);
    virtual  ~CAC_Timer_Manager();
    //��������������һ�ε��ã���Ϊ������
    static CAC_Timer_Manager *instance()
    {
        if(NULL == p_timer_manager_)
        {
            p_timer_manager_ = new CAC_Timer_Manager(300); //��ʼ��300����ʱ��
        }

        return p_timer_manager_;
    }
    /*
    * FUNCTION: bool init()
    * DESCRIPTION: ��ʼ����ʱ�����������
    * INPUT:
    *    none
    * OUTPUT:
    *    �ɹ�����true��ʧ�ܷ���false.
    */
    bool init(ACE_Reactor *pReactor);
    void end_reactor_event_loop();
public:
    ACE_Reactor *reactor() { return p_reactor_; }

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
    virtual int32_t alloc_timer(void *pArg, TIMER_CALL_BACK pFunc, int32_t eventId, const ACE_Time_Value &delay, const ACE_Time_Value &interval = ACE_Time_Value::zero, int32_t nDummy = 0);
    /*
    * FUNCTION: int32_t free_timer()
    * DESCRIPTION: �ͷŶ�ʱ��
    * INPUT:
    *    int32_t timerId --- ��ʱ��ID.
    * OUTPUT:
    *    �ɹ����ض�ʱ��ID��ʧ�ܷ���-1.
    */
    virtual int32_t free_timer(int32_t timerId);

    virtual int32_t free_timer(CAC_Timer *pTimer);


    /*
    * FUNCTION: int32_t reset_timer_interval()
    * DESCRIPTION: �޸Ķ�ʱ����ʱ���ʱ��
    * INPUT:
    *    int32_t timerId --- ��ʱ��ID.
    *    const ACE_Time_Value &interval --- ��ʱ���ʱ��
    * OUTPUT:
    *    �ɹ����ض�ʱ��ID��ʧ�ܷ���-1.
    */
    virtual int32_t reset_timer_interval(int32_t timerId, const ACE_Time_Value &interval);
    int32_t suspend_timer(int32_t timerId)const;
    int32_t resume_timer(int32_t timerId)const;
protected:
    /**
      * ����һ����ʱ����CAC_Timer����
      * �ɹ����ض�ʱ������ָ�룬ʧ�ܷ���NULL.
      */
    CAC_Timer *allocate_timer_object();
    void reactor(ACE_Reactor *pReactor) { p_reactor_ = pReactor; }
    /*
    * FUNCTION: int32_t clear_timer()
    * DESCRIPTION: ���?ʱ��
    * INPUT:
    *    none
    * OUTPUT:
    *    none
    */
    void clear_timer();
protected:
    ACE_Reactor *p_reactor_;  //��ʱ����Ӧ��
    //SVS_TIMER_MAP timer_map_; //�Ѿ�����Ķ�ʱ��
    ACE_DLList<CAC_Timer> free_timer_list_;  //���ж�ʱ��
    const uint32_t max_free_timers_;     //��������
    ACE_Recursive_Thread_Mutex mutex_;
    static CAC_Timer_Manager *p_timer_manager_;
};
#endif //_svs_timer_h
