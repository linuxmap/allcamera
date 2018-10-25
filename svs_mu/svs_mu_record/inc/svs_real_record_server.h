#ifndef _Real_Record_Server_h
#define _Real_Record_Server_h
#include "svs_config.h"
#include "svs_record_server.h"
#include "svs_rt_client_handler.h"
#include "svs_rt_record_common.h"
#include "svs_real_record_task.h"
#include "svs_recv_rt_media_buffer.h"
#include "svs_mb_buffer.h"
#include <vector>
#include "svs_recv_rt_media_buffer.h"
using namespace std;

enum DELETE_RECORD_STRATEGY
{
    DELETE_RECORD_NONE = 0,
    DELETE_EXPIRE_RECORD,
    OVERWRITE_RECORD,
    DELETE_RECORD_MAX,  //�ڴ�֮ǰ����µ�����.
};

enum RECORD_SERVER_EVENT_TYPE
{
    REFRESH_CONFIG_EVENT_TYPE = 0,
    REFRESH_LOAD_REPORT_INTERVAL_EVENT_TYPE,
    REPORT_RECORD_STATUS_INTERVAL_EVNET_TYPE,
    RECORD_SERVER_EVENT_MAX,
};

/**
 * @class CAC_RT_Record_Server
 * @brief ʵʱ¼���������, �ǵ�����
 */
class CAC_RT_Record_Server : public CSVS_Record_Server
{
private:
    // ��ȡ������ʱ��ʹ�õ�Ĭ��ֵ����,��ȡʧ��ʱ��ֵ�и�λֱ�Ӿ�����������Χֵ�Ĳ��֣��ɵ�λ�ֽھ���
    typedef enum enConfDefaultValueType
    {
        // �����ȡ����ֵ������Ч��Χ������������һ�����
        USE_NOTHING_OVER_RANGE    = 0x0001,
        USE_DEFAULT_VALUE_OVER_RANGE    = 0x0002,
        USE_LIKE_VALUE_OVER_RANGE       = 0x0003,

        // ���������ʧ�ܣ�����������һ�����
        USE_NOTHING_GET_FAIL      = 0x0100,
        USE_DEFAULT_VALUE_GET_FAIL      = 0x0200,
        USE_DEFAULT_MAX,
    }ConfDefaultValueType;

    // ����������
    typedef struct stConfAttribute
    {
        int32_t lMaxValue;
        int32_t lMinValue;
        int32_t lDefaultValue;
        int32_t enDefaultValueType;
    }ConfAttribute;

public: // ͨ���๫���ӿ�
    UTAPI ~CAC_RT_Record_Server();
    // ����
    static CAC_RT_Record_Server *instance()
    {
        static CAC_RT_Record_Server szServerManager;
        return &szServerManager;
    }

    /**
     * ����ʵʱ¼����񣺳�ʼ��������Դ --- �ڴ桢�����̵߳�
     * �ɹ�����true, ���򷵻�false.
     */
    UTAPI bool open_service();

public:// ҵ���๲�ýӿ�

    // ���̹������ϱ��澯֪ͨ
    int32_t   notify_exception(std::vector<string> &lens_vec, int32_t raid_id, int32_t error_code);

    // ��Ӹ����ϱ�����
    void add_load_report_task();

    // ���ø����ϱ���ʱ��
    UTAPI int32_t set_load_report_interval_timer();

    // ���ö�̬����
    UTAPI int32_t refresh_configure();

    // ������Ӧ��
    UTAPI void end_reactor_event_loop();

public:// ��ȡ������ӿں��������ӿ�
    // ��ȡע����Ϣ
    void get_register_info(SVS_RECORDSVR_INTERNAL_REGISTER_REQ_MSG &szRegMsg);

    //����:����֧�ֵ�¼�����ͷ��
    UTAPI uint32_t max_lens() const
    {
        return this->max_lens_;
    }

    // ÿ��raid������֧�ֵ����ͷ��
    UTAPI uint32_t max_lens_per_raid()const
    {
        return this->max_lens_per_raid_;
    }

    // ��RtRecordSvr�Ƿ�ΪĬ�Ϸ�����
    UTAPI bool is_default() const
    {
        return (1 == default_server_);
    }

    // ʵʱ¼������������������
    UTAPI int32_t enqueue_record_index(   ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = signal_task_.enqueue_record_index(new_item, timeout);
        return enqueueResult;
    }


    // ���´���������
    UTAPI int32_t reopen_record_index_queue( void )
    {
        //���´򿪶���
        int32_t openResult = signal_task_.reopen_record_index_queue();
        return openResult;
    }

    // ������¼�������ϵͳ�ĸ���¼�������Ϣ���뵽���Ը��¶�����
    UTAPI int32_t  enqueue_record_strategy(ACE_Message_Block *mb, ACE_Time_Value *timeout = 0)
    {
        return signal_task_.enqueue_record_strategy(mb, timeout);
    }

    // ϵͳ��ص����¼����ض���
    UTAPI int32_t enqueue_monitor_event(  ACE_Message_Block *new_item,
                                ACE_Time_Value *timeout = 0)
    {
        int32_t enqueueResult = signal_task_.enqueue_monitor_event(new_item, timeout);
        return enqueueResult;
    }

    // ¼��ɾ��ģʽ���ȴ�·�߹���ɾ������������
    UTAPI uint8_t delete_record_strategy() const
    {
        return this->delete_record_strategy_;
    }

    // �洢����
    UTAPI uint8_t storage_type() const
    {
        return this->storage_type_;
    }

    // ¼�����ģʽ������д¼���ļ���¼��ģʽ�����ڴ��̿ռ䲻�����¼�����
    UTAPI int32_t debug_write_file() const
    {
        return debug_write_file_;
    }

    // ָֹͣ��·�ɵ�¼��
    UTAPI int32_t stop_record( const ACE_Message_Block *mb ) const;



    // �Ƿ�������״̬
    UTAPI bool is_store_svr_conn( void ) const
    {
        if(NULL == p_connector_)
        {
            return false;
        }

        return p_connector_->is_ready_status();
    }

    // ȡ������״̬
    UTAPI int32_t state() const
    {
        if(NULL == p_connector_)
        {
            return 0;
        }

        return p_connector_->conn_status();
    }

    // ȡ����������
    UTAPI uint8_t server_type() const
    {
        return server_type_;
    }

    // ����:���ɴ����
    UTAPI uint16_t transaction_no()
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, transaction_no_mutex_, 1);
        transaction_no_++;
        if( MSG_TRANSACTION_MAX_NUM < transaction_no_ )
        {
            transaction_no_ = 1;
        }
        return transaction_no_;
    }

    // ȡ����ý�������߳���
    UTAPI uint32_t recv_media_thread_num() const
    {
        return recv_media_thread_num_;
    }

    // ��ȡÿ��raid��д¼���ļ��̸߳���
    UTAPI uint32_t output_thread_num_per_raid() const
    {
        return output_thread_num_per_raid_;
    }

    // ��ȡ�ܵ�д¼���ļ��̸߳���
    UTAPI uint32_t sync_work_raid_group_num() const
    {
        return sync_work_raid_group_num_;
    }

    //���ջ�������С
    uint32_t recv_media_buffer_size() const
    {
        return recv_media_buffer_size_;
    }

    //���ջ���������
    uint32_t recv_media_buffer_num() const
    {
        return recv_media_buffer_num_;
    }

    // ý��������С
    uint32_t write_media_data_stripe_width() const
    {
        return write_media_data_stripe_width_;
    }

    // ��ʱ�������Ļ������
    uint32_t tmpfs_recv_media_buffer_num() const
    {
        return tmpfs_recv_media_buffer_num_;
    }

    const char * tmpfs_path() const
    {
        return tmpfs_path_;
    }

    // ��Ӷ�ʱ���ص�������Ϣ
    UTAPI int32_t add_timer_callback_msg( ACE_Message_Block* msg )
    {
        return signal_task_.add_timer_callback_msg( msg );
    }

    // ��ȡ����¼�¼��ʱ��
    UTAPI uint32_t event_record_lost_timeout() const
    {
        return event_record_lost_timeout_;
    }

    UTAPI uint32_t GetRecordTimetickFlag()const
    {
        return m_ulRecordTimeTick;
    }

    // ������Ϣ�ص�
    static void timer_callback(  void *pArg,
                                           int32_t eventId,
                                           int32_t timerId,
                                           int32_t /*nDummy*/ )
    {
        uint32_t msgLen = sizeof(SVS_TIMER_CALL_BACK_MSG);
        ACE_Message_Block *mb =
            g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_EVENT_MSG, msgLen );
        if ( NULL == mb )
        {
            SVS_LOG((SVS_LM_ERROR,
                "failed to alloc mb. type[%d] size[%d]",
                CSVS_Mb_Buffer_Manager::MB_TYPE_EVENT_MSG,
                msgLen ));
            return ;
        }
        SVS_TIMER_CALL_BACK_MSG* pMsg = (SVS_TIMER_CALL_BACK_MSG*)(void*)mb->wr_ptr();
        pMsg->MsgType = SVS_TIMER_CALL_BACK_MSG::REOCRD_SERVER;
        pMsg->ProcObject = pArg;
        pMsg->EventId = eventId;
        pMsg->TimerId = timerId;
        mb->wr_ptr( msgLen );
        (void)CAC_RT_Record_Server::instance()->add_timer_callback_msg( mb );
   }

    // ��ȡ¼�������ļ���Ÿ�·��
    UTAPI const char * get_record_indexfile_path() const
    {
        return (char*)record_indexfile_path_;
    }

    // ��ȡ��¼�������ļ���Ÿ�·��
    UTAPI const char * get_old_index_path() const
    {
        return (char*)old_index_path_;
    }

    // ��ȡ¼�������ļ���Ÿ�·��
    UTAPI const time_t get_index_path_update_time() const
    {
        return index_path_update_time_;
    }

    // ɾ�����������
    UTAPI int32_t enqueue_del_task(ACE_Message_Block * new_item, ACE_Time_Value * timeout = 0)
    {
        int32_t enqueueResult = signal_task_.enqueue_del_task(new_item, timeout);
        return enqueueResult;
    }

    // ɾ��¼����Ϣ�����
    UTAPI int32_t enqueue_del_record(ACE_Message_Block * new_item, ACE_Time_Value * timeout = 0)
    {
        int32_t enqueueResult = signal_task_.enqueue_del_record(new_item, timeout);
        return enqueueResult;
    }

    // ɾ��Ԥ¼���������
    UTAPI int32_t enqueue_prerecord_del_task(ACE_Message_Block * new_item, ACE_Time_Value * timeout = 0)
    {
        int32_t enqueueResult = signal_task_.enqueue_prerecord_del_task(new_item, timeout);
        return enqueueResult;
    }
    UTAPI void print_msg_del_task(const SVS_DEL_RECORD *del_task)const;

    UTAPI void print_msg_del_record(const SVS_DEL_RECORD_FILE *del_record) const;

    // ��ӡ��Ϣ������Ϣ
    int32_t debug_get_msg_info(  char *strBufferInfo, const uint32_t szBufferInfo )const
    {
        (void)signal_task_.debug_get_msg_info( strBufferInfo, szBufferInfo  );
        return SVS_RESULT_OK;
    }

    UTAPI uint32_t get_check_thread_status_interval()const
    {
        return check_thread_status_interval_;
    }

    UTAPI uint32_t get_restart_server_flag()const
    {
        return restart_server_flag_;
    }

    UTAPI uint32_t get_dump_server_flag()const
    {
        return dump_server_flag_;
    }

    UTAPI uint32_t get_use_direct_io_flag() const
    {
        return use_direct_io_flag_;
    }

    UTAPI uint32_t get_write_media_flowctrl_flag()const
    {
        return write_media_flowctrl_flag_;
    }
    UTAPI uint32_t get_check_disk_interval()const
    {
        return check_disk_interval_;
    }

    UTAPI uint32_t test_bandwidth_total()const
    {
        return test_bandwidth_total_;
    }

    UTAPI uint32_t get_load_report_interval()const
    {
        return load_report_interval_;
    }
protected:
    // ����ģʽ���࣬���캯������
    CAC_RT_Record_Server();

    /**
     *  ��ʼ�����ò��� -- �������ļ���ȡ���������浽�ڴ���
     *  �ɹ�����true,ʧ�ܷ���false.
     */
    UTAPI bool init_configure(const char * const) ;

    UTAPI bool init_system_configure();

    // ��ʼ��¼��ɾ�����������
    UTAPI bool init_delete_record_configure();

    // ��ʼ��ý����ܺ�д�߳�������
    UTAPI bool init_media_thread_configure();

    // ��ʼ���¼�¼�����������
    UTAPI bool init_record_event_configure();

    // ��ʼ��¼��洢���������
    UTAPI bool init_record_storagy_configure();

    // ��ʼ��ý�建�������������
    UTAPI bool init_media_buffer_configure();

    // ��ʼ��¼������·�����������
    UTAPI bool init_record_index_configure();

    // ��ȡ�������͵������ͷ���
    UTAPI bool get_conf_integer(const char * pUpperConfName,
                                    const char * pSubConfName,
                                    const ConfAttribute &szConfAttribute,
                                    int32_t & nOutPutValue)const;

    UTAPI int32_t get_conf_type(int32_t failValueType, int32_t defaultValueType)const;

protected:
    //���б�־
    bool b_running_;

    //¼��ɾ������
    uint8_t delete_record_strategy_;

    //�洢����
    uint8_t storage_type_;

    enum STORAGE_TYPE_EN
    {
        STORAGE_TYPE_MASTER = 0,  // ���ô洢����
        STORAGE_TYPE_SLAVE = 1,   // ���ô洢����
    };

    //���ͷ��
    uint32_t max_lens_;

    // ÿ��raid�����֧�ֵľ�ͷ��������
    uint32_t max_lens_per_raid_;

    // �����ļ����޸ĵ�����ʱ��
    time_t config_file_last_moded_time_;

    CAC_Signal_Task signal_task_;

    int32_t debug_write_file_;      //������--�Ƿ�����д¼���ļ���־ 0:��д������:д

    uint16_t transaction_no_;

    ACE_Recursive_Thread_Mutex transaction_no_mutex_;

    uint32_t recv_media_buffer_size_;  //���ջ�������С
    uint32_t write_media_data_stripe_width_;// ������С

    uint32_t recv_media_buffer_num_;   //���ջ���������
    uint32_t tmpfs_recv_media_buffer_num_;   // tmpfs���ջ���������
    char tmpfs_path_[DISK_NAME_SIZE];   // tmpfs·��

    uint32_t recv_media_thread_num_;   //����ý�����̸߳���

    // ϵͳ֧�ֵ�ͬʱ������raid��
    uint32_t sync_work_raid_group_num_;

    // ÿ��raid��д¼���߳���
    uint32_t output_thread_num_per_raid_;

    //¼���¼���ʧ�󣬳�ʱʱ�䣬��λ����
    uint32_t event_record_lost_timeout_;

    // ¼�������ʹ洢���������Ϣ
    char record_indexfile_path_[DISK_NAME_SIZE + 1];
    // ����ǰ������·��
    char old_index_path_[DISK_NAME_SIZE + 1];

    // ����ʱ��
    time_t index_path_update_time_;

    char record_storage_path_[DISK_NAME_SIZE * SVS_LUN_MAX_NUM * SVS_RAID_MAX_NUM];

    // ֹͣ¼��Ĵ�����ֵ����λ����
    uint32_t disk_lower_limit_;
    // ���̸澯�ָ���ֵ����λ����
    uint32_t disk_alarm_resume_limit_;
    // ���̸澯��ֵ����λ����
    uint32_t disk_alarm_limit_;
    // ���̼�����ڣ���λ����
    uint32_t check_disk_interval_;
    // ���̼����
    uint32_t check_thread_status_interval_;


    // ���������־
    uint32_t restart_server_flag_;
    // Dump�����־
    uint32_t dump_server_flag_;
    // ʹ��DirectIOд�ļ��ı�־
    uint32_t use_direct_io_flag_;

    // д¼�����ؿ���,0Ϊ���أ���0Ϊд¼�����أ�Ĭ�ϲ�����
    uint32_t write_media_flowctrl_flag_;

    // �����ϱ��������
    uint32_t load_report_interval_;
    int32_t load_report_interval_timer_;

    // ���Կ��أ�����������ȡ���������ܴ�������
    uint32_t test_bandwidth_total_;

    char str_conf_buffer_[DISK_NAME_SIZE * SVS_LUN_MAX_NUM * SVS_RAID_MAX_NUM];

    uint32_t m_ulRecordTimeTick;
#ifdef UNITTEST
    friend class TEST_CAC_RT_Record_Server;
    friend class CAC_RT_Record_Device_ManagerAutoMock;
    friend class CAC_RT_Record_ServerAutoMock;
#endif
};
#endif //_Real_Record_Server_h


