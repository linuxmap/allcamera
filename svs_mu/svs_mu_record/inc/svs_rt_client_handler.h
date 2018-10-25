#ifndef _SVS_RT_Client_Handler_h
#define _SVS_RT_Client_Handler_h
#include "svs_rt_record_common.h"
#include "svs_stat_manager.h"
class CSVS_Client_Handler  :public ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_MT_SYNCH>
{
public:
    /* ״̬��*/
    // ����StoreSvr��״̬
    enum RT_CONN_STORE_STATUS
    {
        RT_CONN_STORE_INIT = 0,
        RT_CONN_STORE_DISCONNECTED,
        RT_CONN_STORE_CONNECTING,
        RT_CONN_STORE_LOGGING,
        RT_CONN_STORE_READY,
        RT_CONN_STORE_EXIT
    };

    // �������߳�
    enum RT_HANDLE_CLIENT_THREAD
    {
        RT_HANDLE_CLIENT_SEND_MSG_THREAD = 0,
        RT_HANDLE_CLIENT_CHECK_CONNECT_THREAD,
        RT_HANDLE_CLIENT_MAX_THREAD
    };

    // ���ݽ������
    enum HANDLE_RECV_DATA_RET
    {
        HANDLE_RECV_DATA_ERROR = -1,
        HANDLE_RECV_DATA_SUCCESS = 0,
        HANDLE_RECV_DATA_CONTINUE = 1,
        HANDLE_RECV_DATA_BREAK = 2,
        HANDLE_RECV_DATA_RET_MAX
    };


    enum
    {
        SVS_SERVER_TYPE_STORESVR    = 0,
        SVS_SERVER_TYPE_CONSOLE     = 1,
    };
    typedef ACE_Connector<CSVS_Client_Handler, ACE_SOCK_CONNECTOR> MyConnector;
public:
    CSVS_Client_Handler();
    CSVS_Client_Handler( ACE_Reactor *pReactor,
                        ACE_Message_Queue<ACE_SYNCH> *pMsgQueue,
                        uint32_t nServerType = SVS_SERVER_TYPE_STORESVR);

    virtual ~CSVS_Client_Handler();

    // ��ʼ��
    virtual int32_t open(void *pArg);

    // ������������
    virtual int32_t handle_input(ACE_HANDLE handle);

    // ��ʱ���ص�
    virtual int32_t handle_timeout(const ACE_Time_Value & /*currentTime*/, const void * /*act = 0*/);

    // ���رձ����Ӻ󣬷�Ӧ���ص��ú���
    virtual int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask mask);

    // ����open�����м���(activate)�˷���󣬱������̡߳�
    virtual int32_t svc(void);

   // ������Ϣ
   virtual int32_t send_msg(ACE_Message_Block * mb, ACE_Time_Value * timeout = 0);

    // ����״̬�Ƿ�Ϊ��������״̬
    virtual bool is_ready_status()const
    {
        return (RT_CONN_STORE_READY == conn_status_);
    }

    virtual int32_t conn_status()const
    {
        return conn_status_;
    }

    // �ͷ���Ϣ��
    virtual ACE_Message_Block* alloc_mb(uint32_t mbMsgLen, uint8_t mbType)const;

    // ������Ϣ��
    virtual void free_mb(ACE_Message_Block* mb)const;
protected:
    // ��Ϣ�������ݽ���
    virtual int32_t parse_data();

    // ��ʼ����Ա����
    virtual int32_t construct();

    // ����socket
    virtual void set_socket_option()const;

    // ���ͼ���¼�
    virtual int32_t send_monitor_event(int32_t monitorEventType);

    // ������Ϣ�߳�
    virtual int32_t send_message_thread();

    // ��StoreSvr������״̬����߳�

    virtual int32_t check_connection_status_thread();

    // ��������״̬
    virtual int32_t set_conn_status(int32_t newStatus);

    // ���ɾ�����Ϣ
    virtual ACE_Message_Block* build_msg( const SVS_HEADER* pMsgHeader, uint8_t mbType ) const;

    // ��Ϣ�б������еȴ�����
    virtual int32_t equeue_msg(ACE_Message_Block * mbBatch, ACE_Time_Value tv);

    // �����Ϣ����ͷ�ͳ��ȺϷ���
    virtual int32_t check_data(const uint32_t procDataPos,
                        uint32_t & nextDataPos,
                        SVS_HEADER *& pMsgHeader);

    // �������ݻ�������Ϣ
    virtual int32_t update_buffer_cursor(uint32_t procDataPos, uint32_t nextDataPos);

    // ����ע����Ϣ
    virtual int32_t send_register_msg();

    // ����ע����Ӧ��Ϣ
    virtual int32_t handle_register_response_msg(const SVS_HEADER * pMsgHeader);

    // �������ӷ������������������¼����¼�������.
    virtual int32_t reconnect(void);

    // ������StoreSvr������ע��
    virtual int32_t register2server();

    // �Ͽ�����
    virtual int32_t disconnection(void);

    //����:�����̺߳�
    virtual int32_t add_index()
    {
        int32_t oldIndex;
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, SVS_RESULT_FAILURE);
        oldIndex = thr_index_++;
        return oldIndex;
    }

protected:
    ACE_Recursive_Thread_Mutex mutex_;

    // ���б�־���������false, ���̽�����.�߳�svc()Ӧ�ý���
    bool brunning_;

    // �̺߳�
    int32_t thr_index_;
    // ������Ϣ����
    ACE_Message_Queue<ACE_SYNCH> *p_signal_queue_;

    // ����ʧ�ܴ���
    int16_t connect_fail_count_;

    // ������ʱ����
    volatile uint32_t heartbeat_timeout_times_;

    // ע��ʱ�䣬����ע�ᳬʱ�ж�
    volatile time_t new_loggin_time_;

    // Ӧ�ò�������Ϣ�����
    volatile uint16_t heartbeat_transaction_no_;

    // �������ݳ���
    uint32_t  recv_data_len_;

    // �������ݻ�����
    char recv_data_buffer_[RECV_DATA_BUFF_MAX_LEN];

    // ������Ϣ������
    ACE_Message_Block *send_msg_buffer_;

    // ��StoreSvr������״̬
    volatile int32_t conn_status_;

    // ¼��״̬
    volatile int32_t record_status_;

    // ͳ����Ϣ
    SVS_Signal_Message_Stat signal_msg_stat_;

    ACE_INET_Addr local_addr_;
    ACE_INET_Addr remote_addr_;

    // ����������
    uint32_t server_type_;
};
#endif // _SVS_Client_Handler_h


