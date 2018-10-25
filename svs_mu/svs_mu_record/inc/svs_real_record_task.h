#ifndef _Real_Record_Task_h
#define _Real_Record_Task_h
#include "svs_monitor_event.h"
#include "svs_debug_accept_handler.h"
#include "svs_recv_stream_factory.h"
#include "svs_timer.h"
#include "ace/Dev_Poll_Reactor.h"
#include "svs_rt_client_handler.h"
#include "svs_mb_buffer.h"
#include "svs_delete_record_manager.h"
#include <vector>
using namespace std;

extern ACE_TSS <CSVS_Thread_Msg_Buffer_Manager> g_p_msg_buffer;

class CAC_Signal_Task  : public ACE_Task <ACE_MT_SYNCH>
{
public:
    typedef ACE_Acceptor <CSVS_Debug_Accept_Handler, ACE_SOCK_ACCEPTOR> SVS_LOG_ACCEPTOR;
    CAC_Signal_Task();
    virtual ~CAC_Signal_Task();

    UTAPI int32_t close(u_long);

    // ����open�����м���(activate)�˷���󣬱������̡߳�
    UTAPI int32_t  svc(void);

    UTAPI int32_t open(void *);

    //�������¼��ص�����
    UTAPI int32_t  handle_monitor_event(const ACE_Message_Block *mb) const;

    //ֹͣ�����߳�
    UTAPI void stop();

    //����¼������
    UTAPI int32_t enqueue_monitor_event( ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = monitor_event_queue_.enqueue_tail(new_item, timeout);
        return enqueueResult;
    }

    // ɾ�����������
    UTAPI int32_t enqueue_del_task( ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = del_task_queue_.enqueue_tail(new_item, timeout);
        return enqueueResult;
    }

    // ɾ�������
    UTAPI int32_t enqueue_del_record( ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = del_record_queue_.enqueue_tail(new_item, timeout);
        return enqueueResult;
    }

    // ɾ�������
    UTAPI int32_t enqueue_prerecord_del_task( ACE_Message_Block *new_item,
                                                 ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = del_prerecord_task_queue_.enqueue_tail(new_item, timeout);
        return enqueueResult;
    }

    //¼�����������
    UTAPI int32_t enqueue_record_index(   ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = rindex_queue_.enqueue_tail(new_item, timeout);

        return enqueueResult;
    }

    // ���´�¼����������
    UTAPI int32_t reopen_record_index_queue( void )
    {
        //���´򿪶���
        (void)rindex_queue_.close();
        int32_t openResult = rindex_queue_.open(RECORD_FILE_INDEX_QUEUE_WM,
                                            RECORD_FILE_INDEX_QUEUE_WM);
        return openResult;
    }

    //¼�����������
    UTAPI int32_t enqueue_load_report( ACE_Message_Block *new_item,
                                        ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = load_report_queue_.enqueue_tail(new_item, timeout);
        return enqueueResult;
    }

    // ������¼�������ϵͳ�ĸ���¼�������Ϣ���뵽���Ը��¶�����
    UTAPI int32_t  enqueue_record_strategy(ACE_Message_Block *mb, ACE_Time_Value *timeout = 0)
    {
        return renew_strategy_queue_.enqueue_tail( mb, timeout);
    }

    // ���´򿪸����ϱ�����
    UTAPI int32_t reopen_load_report_queue( void )
    {
        //���´򿪶���
        (void)load_report_queue_.close();
        int32_t openResult = load_report_queue_.open();
        return openResult;
    }

    //���ܣ�����Զ�̵��Զ˿�
    UTAPI void remote_debug_port( uint16_t debug_port )
    {
        remote_debug_port_ = debug_port;
    }

    // ��Ӷ�ʱ���ص���Ϣ
    UTAPI int32_t add_timer_callback_msg( ACE_Message_Block* msg );

    // ��Ϣ���������Ϣ
    UTAPI int32_t debug_get_msg_info(  char *strBufferInfo,
                             const uint32_t szBufferInfo )const;

    //��ʼ��һЩ��ʱ������Ϣ����
    int32_t init_task();
protected:
    // ���������ļ������õ����ڣ���ʱ�������ݿ⣬�ﵽ���ڱ������ݿ����ӵ�����
    int32_t db_heartbeart_thread()const;

    // �����ϱ��������߳�
    int32_t load_report_thread();

    // �ַ����յ�����Ϣ
    UTAPI void handle_message(uint32_t nThrIndex, ACE_Message_Block *mb);

    // ��������StoreSvr�Ĵ�����Ϣ
    UTAPI void handle_error_msg(ACE_Message_Block *mb) const;

    // ������¼�������ϵͳ�ĸ���¼�������Ϣ���뵽���Ը��¶�����
    UTAPI int32_t  putinto_renew_strategy(const ACE_Message_Block *mb);

    UTAPI void proc_del_record_msg(const ACE_Message_Block *mb);

    // �������ķ���ƽ̨�е�����������
    UTAPI int32_t  start_connect();

    //��ʱ�������߳�
    UTAPI int32_t  timer_event_loop();

    //����¼������߳�
    UTAPI int32_t  monitor_event_proc_thread();

    //����¼������߳�
    UTAPI int32_t  renew_strategy_loop();

    //¼����Լ���̡߳�ʵʱ�������Ƿ���Ч��
    UTAPI int32_t  strategy_check_thread();

    //����¼���ļ������߳�
    UTAPI int32_t  update_record_thread();

    //����:��Ϣ�����߳�
    UTAPI int32_t proc_message_thread( uint32_t nIndex );

    //�����߳�
    UTAPI int32_t  debug_thread();

    //��ʱ���ص������߳�
    UTAPI int32_t timer_call_back_thread();

    // ������StoreSvrǰ��¼�񱸷���Ϣ������ǰ��¼�񱸷�������Ϣ����
    UTAPI int32_t putinto_bakup_msg( ACE_Message_Block *mb);

    // ǰ��¼�񱸷��������߳�
    UTAPI int32_t proc_bakup_msg_loop();

    // ����¼��״̬�ϱ���ʱ�����߳�
    UTAPI int32_t procReportRecordStatusTaskThread();
    UTAPI void addReportRecordStatusTask();

    //��ʱ�������߳�2
    UTAPI int32_t timer_call_back_second_thread();

    // ��ʱ�ص�¼��״̬����߳�
    UTAPI int32_t  timer_call_back_record_thread();

    // �̣߳�����ɾ�������̣߳���ʼ��ɾ������δ�����ش����ɾ������
    UTAPI int32_t proc_del_record_task_thread();

    // �̣߳�ɾ��¼���ļ��̣߳����غ��ɾ������
    UTAPI int32_t del_record_file_loop();

    // �̣߳�����ɾ��Ԥ¼�ļ�
    UTAPI int32_t del_prerecord_file_loop();

    // �̣߳�ɾ����������Ŀ¼
    UTAPI int32_t del_date_dir_loop();

    // ��ʱ��������:����¼��ɾ����������ڵ������Ƿ�����Ҫɾ����¼��
    //UTAPI void handle_del_timer_timetask();

    // ��ʱ��������:��������ɾ�������������ڵ���ִ��
    UTAPI void handle_del_timer_control();

    // ��ʱ��������:����ɾ��Ԥ¼����
    UTAPI void handle_del_timer_prerecord();

    // ��ʱ��������:�����������Ŀ¼ɾ����ʱ�����ں�ִ��ɾ������
    UTAPI void handle_del_timer_date_dir();

    // ��ʱ���ص�ɾ�������ݶ�ʱ���¼����ͷ���
    static void del_record_timer_callback( void *pArg,
                            int32_t eventId,
                            int32_t timerId,
                            int32_t nDummy );

    // ��¼���ϵͳ�����Ϣ����
    UTAPI int32_t open_queue();

    // ��ɾ�������Ϣ����
    UTAPI int32_t open_del_queue();

    // ������ʱ��
    UTAPI int32_t create_timer();

    // ����ɾ����ض�ʱ��
    UTAPI int32_t create_del_timer();

    // ��������ý�巴Ӧ����
    UTAPI int32_t create_recv_media_reactor_vector();

    // ��
    UTAPI int32_t open_record_reactor();

    //����:�����̺߳�
    UTAPI int32_t add_index()
    {
        int32_t oldIndex;

        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, SVS_RESULT_FAILURE);
        oldIndex = thr_index_++;
        return oldIndex;
    }

    //����:��Զ�̵��Խ�����
    UTAPI int32_t open_debug_acceptor();

    // ����ϵͳ�߳�
    UTAPI int32_t create_thread_system(int32_t threadIndex);

    // ����¼������߳�
    UTAPI int32_t create_threads_Record(int32_t threadIndex);

    // ����ɾ������߳�
    UTAPI int32_t create_threads_del(int32_t threadIndex);

    // ����ý����պͷ�������߳�
    UTAPI int32_t create_threads_media(int32_t threadIndex);

    void handle_del_msg(ACE_Message_Block* mb, bool& bFree);
    void proc_del_prerecord_msg(const ACE_Message_Block* mb)const;
    void proc_del_date_dir_msg(ACE_Message_Block* mb);

protected:
    enum THREAD_INDEX
    {
        CONNECT_THREAD = 0,
        TIMER_CALL_BACK_THREAD,     //��ʱ���ص������߳�
        TIMER_THREAD,
        MONITOR_EVENT_PROC_THREAD,
        RENEW_STRATEGY_THREAD,
        STRATEGY_CHECK_THREAD,     //���Լ���߳�
        UPDATE_RECORD_THREAD,
        DEBUG_THREAD,              //�����߳�
        PROC_BAKUP_TASK_THREAD,   //ǰ��¼�񱸷��������߳�
        PROC_REPORT_RECORD_STATUS_TASK_THREAD,   // ����¼��״̬�ϱ���ʱ����
        TIMER_CALL_BACK_SECOND_THREAD,    //��ʱ���ص������߳�2
        TIMER_CALL_BACK_RECORD_THREAD,    //��ʱ���ص�����¼��״̬����߳�
        //PROC_ALARM_EVENT_THREAD,    //�澯�¼������߳�
        PROC_DEL_RECORD_TASK_THREAD,  // ����ɾ�������߳�
        DEL_RECORD_FILE_THREAD,   //  ִ��ɾ�������߳�
        DEL_PRERECORD_FILE_THREAD, // ɾ��Ԥ¼�ļ��߳�
        DEL_DATE_DIR_THREAD, // ɾ������Ŀ¼�߳�
        LOAD_REPORT_THREAD,
        MAX_THREAD
    };

    enum RT_THREAD_INDEX_MATCH
    {
        RT_THREAD_INDEX_UNFOUND = 99
    };

    enum
    {
        USE_DB_CONN_THREAD_NUM = 12  //��Ҫʹ�����ݿ����ӵ��߳���
    };

    enum SVC_THREAD_CONST
    {
        SVC_NUM = 1
    };

    enum
    {
        RT_LENS_TIMER_MULTIPLE = 3    // ÿ��ǰ����Ҫ�Ķ�ʱ������
    };

    enum  SVS_DEL_TIMER_TYPE
    {
        SVS_DEL_TIMER_TIMETASK  = 1,     // ɾ����ʱ��֮��ʱɾ������
        SVS_DEL_TIMER_CONTROL   = 2,     // ɾ����ʱ��֮��������
        SVS_DEL_TIMER_PRERECORD = 3,
        SVS_DEL_TIMER_DATE_DIR = 4,
        SVS_DEL_TIMER_TYPE_MAX
    };

    enum
    {
        SVS_THREAD_MAX_NUM = 50
    };

    enum
    {
        MSG_SORT_ROUTER_INFO = 0,   // ·��֪ͨ
        MSG_SORT_SCHEDULE_OVER_REQ, // ·�ɽ���
        MSG_SORT_PLATDEV_INFORM_REQ,// �豸״̬֪ͨ
        MSG_SORT_ALARM_INFO,        // �澯�¼�
        SVS_MSG_SORT_MAX_NUM
    };

protected:

    // �߳����
    int32_t thr_index_;

    // ��������״̬
    volatile bool brunning_;

    ACE_Recursive_Thread_Mutex mutex_;

    // �����Ϣ����
    ACE_Message_Queue <ACE_SYNCH> monitor_event_queue_;

    // ���Ը�����Ϣ����
    ACE_Message_Queue <ACE_SYNCH> renew_strategy_queue_;

    // ����ǰ��¼��������Ϣ����
    ACE_Message_Queue <ACE_SYNCH> bakup_task_queue_;

    // ʵʱ¼����������
    ACE_Message_Queue <ACE_SYNCH> rindex_queue_;

    // �����ϱ��������
    ACE_Message_Queue <ACE_SYNCH> load_report_queue_;

    // ���ӷ�Ӧ������������StoreSvr
    ACE_Reactor *p_connect_reactor_;
    // ����epoll��Ӧ������������StoreSvr
    ACE_Dev_Poll_Reactor* p_connect_epoll_reactor_;

    // ��ʱ����Ӧ��
    ACE_Reactor *p_timer_reactor_;

    // ���Է�Ӧ��������¼��֪ͨ
    ACE_Reactor *p_strategy_reactor_;

    // ����¼��֪ͨ
    CAC_Timer_Manager *p_strategy_timer_manager_;

    ACE_Condition <ACE_Thread_Mutex> *p_cond_;
    ACE_Thread_Mutex cond_mutex_;

    // �����̷߳�Ӧ��
    ACE_Reactor *p_debug_reactor_;
    // Զ�̵��Խ�����
    SVS_LOG_ACCEPTOR *p_debug_acceptor_;
    // Զ�̵���epoll��Ӧ��
    ACE_Dev_Poll_Reactor* p_debug_epoll_reactor_;
    //Զ�̵��Զ˿�
    uint16_t remote_debug_port_;

    //�ѳ�ʼ����־
    volatile bool inited_flag_;

    // ��ʱ���ص��������
    ACE_Message_Queue <ACE_SYNCH> timer_call_back_queue_;
    // ��ʱ���ص��������
    ACE_Message_Queue <ACE_SYNCH> timer_call_back_queue_second_;
    // ¼��״̬��ⶨʱ��
    ACE_Message_Queue <ACE_SYNCH> timer_call_back_queue_record_;

    // �澯�¼�����
    //ACE_Message_Queue <ACE_SYNCH> alarm_event_queue_;

    // ��Ϣ����ͳ��
    uint32_t msg_total_count_[SVC_NUM];
    // ������Ϣ����ͳ��
    uint32_t msg_sort_msg_count[SVC_NUM][SVS_MSG_SORT_MAX_NUM];
    CSVS_Thread_Msg_Buffer_Manager* p_msg_buffer_manager_[SVS_THREAD_MAX_NUM];
    // �߳�ID�б�
    ACE_hthread_t thread_id_list_[SVS_THREAD_MAX_NUM];

    // ����ɾ���������˶���
    ACE_Message_Queue <ACE_SYNCH> del_task_queue_;

    // ���ɾ��Ԥ¼������ʱ�������
    ACE_Message_Queue <ACE_SYNCH> del_prerecord_task_queue_;

    // �Գ�ʼɾ������������ɶԾ��徵ͷ��ɾ����Ϣ����˶���
    ACE_Message_Queue <ACE_SYNCH> del_record_queue_;

    // �������غ���Ϣ����˶���
    ACE_Message_Queue <ACE_SYNCH> del_record_file_queue_;

    // ɾ��Ԥ¼�ļ�����˶���
    ACE_Message_Queue <ACE_SYNCH> del_prerecord_file_queue_;

    // ��������Ŀ¼��ʱ��
    ACE_Message_Queue <ACE_SYNCH> del_date_dir_queue_;

    int32_t m_recordStatusCollectTimer;
    ACE_Message_Queue <ACE_SYNCH> m_recordStatusCollectQueue;

    // ɾ�����������
    CAC_Delete_Record_Manager del_manager_;

    // ɾ����������ͳ��
    uint32_t time_control_count;

#ifdef UNITTEST
    friend class CAC_Signal_TaskAutoMock;
    friend class TEST_CAC_Signal_Task;
#endif
};

#endif //_Real_Record_Task_h

