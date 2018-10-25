#ifndef _Real_Record_Lens_h
#define _Real_Record_Lens_h
#include <vector>
#include "svs_rt_record_common.h"
#include "svs_strategy_list.h"
#include "svs_record_index.h"
#include "svs_record_lens.h"

class CAC_Recv_Stream;

class SVS_File_Egress;

/**
 * @class CAC_Realtime_Record_Device
 * @brief ʵʱ¼���豸��
 */
class CAC_Realtime_Record_Device : public CSVS_Record_Lens
{
public:

    /**
     * ���캯������ͷID<strLensId>,����������Ϊ<encoderType>,
     */
    CAC_Realtime_Record_Device( const char *strLensId,
                                uint32_t encoderType,
                                uint32_t nIndex );

    virtual ~CAC_Realtime_Record_Device();

    enum SVS_FRONT_DEVICE_STATE
    {
        // ֵ�Ĵ�С����ת���Ⱥ�˳������
        RECORD_STATE_INIT = 0,           //��ʼ��״̬
        RECORD_STATE_SLEEP,          //�ȴ�������Ч
        RECORD_STATE_REQUEST_ROUTER, //���������ϵͳ����VTDU
        RECORD_STATE_CONNECT_VTDU,   //����VTDU��֪ͨ���ն˿�
        RECORD_STATE_RECEIVING_FILE, //�����ļ���
        RECORD_STATE_DISK_FULL,
        RECORD_STATE_WAITING_OVER,   //�ȴ�����
    };

    enum SVS_RECORD_TYPE
    {
        RECORD_TYPE_PROBID = 0, //��ֹ¼��
        RECORD_TYPE_ALWAYS = 1, //����ʱ��¼��
        RECORD_TYPE_EVENT = 2, //�¼�¼��
        RECORD_TYPE_MANUAL = 3, // �ֶ�¼��
        RECORD_TYPE_MAX,
    };

    enum SVS_TIMER_TYPE
    {
        TIMER_TYPE_EVENT_RECORD_OVER = 1,   // �¼�¼���������ʱ��
        TIMER_TYPE_EVENT_RECORD_LOST = 2,   // ¼���¼���ʧ����ʱ��
        TIMER_TYPE_RECORD_STATUS     = 3,   // ¼���¼���ʧ����ʱ��
    };

    // ��ȡһ���������·��ʱ��
    UTAPI int32_t get_rand_request_router_time() const
    {
        ACE_OS::srand( (uint32_t)ACE_OS::time() );
        int32_t randTime = REQUEST_ROUTER_INTERVAL_TIME + ACE_OS::rand() % REQUEST_ROUTER_INTERVAL_TIME;
        return randTime;
    }

    // ��λ�豸����
    UTAPI int32_t reset();


    // ��ʼ���豸����
    UTAPI bool init();

    // �豸���ߴ��� --- �ر�·�ɣ���״̬����Ϊֹͣ״̬����ý����
    // �����̺߳��ļ�����̷߳����˳���Ϣ��
    UTAPI int32_t off_line();

    /**
     * ����ͷ���߻��߾�ͷû������¼�����ʱ�������ã�ֹͣ¼�񣬲�֪ͨý����
     * �����̺߳��ļ�����߳��ͷ���Դ��
     * �ɹ�����0�����򷵻�-1.
     */
    UTAPI int32_t stop_device( bool bSendOverMsg = true );

    // ���������ϵͳ����·��
    UTAPI int32_t request_router();

    // �������ϵͳ���ͽ�����������
    UTAPI int32_t shutdown_router(bool bGetlastError = false);

    /**
     * ����·����Ϣ��<pbyMsg>ָ��·����Ϣ.
     * �ɹ�����0��ʧ�ܷ���-1
     */
    UTAPI int32_t handle_routerinfo(const void *pbyMsg);

    // ��ʱ��ʱ�䴦��
    UTAPI int32_t on_timer(int32_t eventId, int32_t timerId, int32_t nDummy);

    // ��������ֹͣ¼��
    UTAPI int32_t control_record(uint8_t recordFlag, time_t endTime, time_t eventStratEndTime );

    // ¼���¼�����
    UTAPI int32_t event_happen(   const char *strAlarmDeviceId,
                        uint32_t eventType,
                        time_t endTime, bool bIsRecord );

    // ¼���¼�����
    UTAPI int32_t event_over(const char *strAlarmDeviceId, uint32_t eventType);

    // ����¼��ͷ������Ϣ
    UTAPI int32_t update_record_parameter(    uint32_t nBeforeSeconds,
                                    uint32_t nAfterSeconds );

    // ���Ծ�ͷ�豸��ϸ��Ϣ
    UTAPI int32_t debug_get_device_info( char *strDeviceInfo,
                                            uint32_t ulBuffLen );

    // ����: ֹͣ¼��, ���豸״̬Ϊ����·��״̬
    UTAPI int32_t stop_record();

    // ȡ·��id
    UTAPI routerid_t router_id()  const
    {
        return stream_id_;
    }

    // ȡ¼���־
    UTAPI int32_t record_flag()  const
    {
        return record_flag_;
    }

    // ȡ¼��״̬
    UTAPI uint8_t state()  const
    {
        return state_;
    }

    // �����¼�¼�������Чʱ��
    UTAPI int32_t refresh_event_record_outtime(int32_t shiftValue);

    //����¼�¼����Ϣ
    UTAPI int32_t clear_event_record_info();

    int32_t overwrite_record_notify(time_t newIndexStartTime);
protected:

    // ��ȡ�豸״̬--������� string
    UTAPI void debug_get_device_status(string & strDeviceStatus)const;

    // ��ȡ�豸¼��״̬��ʶ--������� string
    UTAPI void debug_get_record_flag(string & strRecordFlag)const;

    // �¼�¼��ʱ
    UTAPI void proc_event_record_over_time();

    // �¼�¼��ʧ
    UTAPI int32_t proc_event_record_lost();

    // ȡ���¼���������ʱ��
    UTAPI int32_t kill_event_record_over_timer();

    // �����¼���������ʱ��
    UTAPI int32_t set_event_record_over_timer(const ACE_Time_Value &delayTime );

    // ȡ��¼���¼���ʧ����ʱ��
    UTAPI int32_t kill_event_record_lost_timer();

    // ����¼���¼���ʧ����ʱ��
    UTAPI int32_t set_event_record_lost_timer(const ACE_Time_Value &delayTime );

    // ¼������ת��
    UTAPI int32_t record_conversion(uint8_t newRecordFlag, bool & bStartRecord);

    // Ԥ¼����
    UTAPI void prerecord_over(uint8_t newRecordFlag);

    // ����¼����ֶ�¼�����
    UTAPI void always_record_over(uint8_t newRecordFlag);

    // �Ƿ������Ч���¼�¼��
    UTAPI bool is_event_record_effective()const
    {
        if ( (0 == event_record_before_seconds_)
          && (0 == total_event_)
          && (0 == event_over_time_))
        {
            return false;
        }

        return true;
    }

    // �ֶ�¼�����
    UTAPI int32_t manual_record_over(const uint8_t newRecordFlag);

    /**
     * ����¼��recordType -- ¼�����͡�
     * ��always_recording()��int evnet_recording()����
     */
    UTAPI int32_t start_record(uint8_t recordType);

    // ��ֹ¼��
    UTAPI int32_t probid_recording();

    // �������Ƶ���ı���UDP SOCKET����ֹͣ������Ϣ
    UTAPI int32_t send_stop_recv_msg();

    // ��������ʵʱý������UDP������������VTDU�����������ݱ���֪ͨ
    // VTDU��ý����ת����ʵʱ¼�������
    UTAPI int32_t notify_vtdu();

    // �����ֶ�¼��״̬֪ͨ��Ϣ��¼����������
    UTAPI int32_t send_status_notify_msg(const int32_t recordStatus,uint8_t recordType);

private:
    // ��������ʱ���ص�
    UTAPI void recv_stream_on_timer();

    // ����·�ɶ�ʱ���ص�
    UTAPI void request_router_on_timer();

    // ���handle�Ƿ��ͷ�״̬
    UTAPI void waiting_over_on_timer();

    // ����VTDU��ʱ���ص�
    UTAPI void conn_vtdu_on_timer();

    // ����¼��״̬
    UTAPI int32_t set_status(uint8_t newStatus);

    // ���¾�ͷ�Ĳ���¼��������Ϣ
    UTAPI void updateRecordTaskStartTime(uint8_t oldRecordFlag, uint8_t newRecordFlag, int32_t oldEventNum);

private:
    CAC_Realtime_Record_Device();
protected:
    //����������
    uint32_t encoder_type_;

    //¼���־
    volatile uint8_t record_flag_;

    //�¼���ʼʱ��
    volatile time_t event_begin_time_;

    //�¼�����ʱ��
    volatile time_t event_over_time_;

    //��Ϊ��ID���ǹؼ���
    routerid_t stream_id_;

    // ·�����볬ʱʱ�䣬������ɵģ���ʱ������ʱ����Ҫ�õ�
    int32_t request_route_overtime_len_;

    //VTDU��ַ
    ACE_INET_Addr vtdu_addr_;

    uint32_t    m_ulIOHandlerIndex;

    // ״̬��ʼʱ��
    time_t status_start_time_;

    //�¼�����
    int32_t total_event_;

    vector <EVENTMAP > event_vector_;

    volatile uint32_t request_route_times_; //����·�ɴ���
    volatile uint32_t connect_vtdu_times_;  //����VTDU����
    volatile uint32_t connect_vtdu_timeout_times_;  //����VTDU��ʱ����

    volatile uint32_t recv_media_data_timeout_times_;

    //===========�¼�¼����ز���======

    //�¼�ǰ¼������
    time_t event_record_before_seconds_;

    //�¼���¼������
    time_t event_record_after_seconds_;

    //�¼���ʼʱ��
    time_t event_record_start_time_;

    //�¼�����ʱ��
    time_t event_record_end_time_;

    //�¼�¼�������ʱ��ID
    int32_t event_record_over_timer_id_;

    //¼���¼���ʧ��ʱ��ID
    int32_t event_record_lost_timer_id_;

    // ¼��ʼʱ��
    time_t  record_start_time_;

    SVS_Device_Stat device_stat_;

};

/**
 * @class CAC_RT_Record_Device_Manager
 * @brief ǰ���豸¼�������
 */
class CAC_RT_Record_Device_Manager
{
protected:
    // ����״̬���豸����
    typedef struct ST_SVS_STATUS_DEV_NUM
    {
        int32_t initStatusDeviceNum;             //��ʼ��״̬�豸����
        int32_t sleepStatusDeviceNum;            //����״̬�豸����
        int32_t diskFullWaitingDeviceNum;         //�������ȴ����̿ռ�״̬
        int32_t requestRouterStatusDeviceNum;   //����·��״̬�豸����
        int32_t connectVtduStatusDeviceNum;     //����VTDU״̬�豸����
        int32_t recvFileStatusDeviceNum;         //��������״̬�豸����
        int32_t waittingOverStatusDeviceNum;    //�ȴ�����״̬�豸����
        int32_t stopStatusDeviceNum;             //�豸ֹͣ״̬�豸����
    }SVS_STATUS_DEV_NUM_T;

    // ��ͷid��¼���豸�����map
    typedef ACE_Hash_Map_Manager < const char *, CAC_Realtime_Record_Device *, ACE_Null_Mutex >
        SVS_DEVICE_MAP;

    // ·��id��¼���豸�����map
    typedef ACE_Hash_Map_Manager <uint32_t, CAC_Realtime_Record_Device *, ACE_Null_Mutex>
        SVS_STREAM_MAP;

    // ��ͷid��¼���豸���������
    typedef ACE_Hash_Map_Iterator < const char *, CAC_Realtime_Record_Device *, ACE_Null_Mutex >
        SVS_DEVICE_MAP_ITER;

    typedef ACE_Hash_Map_Entry < const char *, CAC_Realtime_Record_Device * >
        SVS_DEVICE_MAP_ENTRY;

public:
    // ʵʱ¼���豸������ĵ���
    static CAC_RT_Record_Device_Manager *instance()
    {
        static CAC_RT_Record_Device_Manager szDeviceManager;
        return &szDeviceManager;
    }
    /**
     * ��ʼ��¼��ͷ���������nMaxLensָ��¼�����ͷ��
     * �ɹ�����true,ʧ�ܷ���false.
     */
    UTAPI bool init(uint32_t nMaxLens);

    // ͨ��ǰ��id��ȡʵʱ¼���豸����
    UTAPI CAC_Realtime_Record_Device * get_device(const char *strFrontDevId);

    // ͨ���ڲ�id��ȡʵʱ¼���豸����
    UTAPI CAC_Realtime_Record_Device * get_device(uint32_t nId);

    /**
     * ��������ֹͣ<strLensId>��¼����<recordFlag>ֵ������
     */
    UTAPI int32_t  control_record( const char * const strLensId,
                         uint8_t encoderType,
                         uint8_t recordFlag,
                         time_t endTime,
                         uint32_t eventBeforeSeconds,
                         uint32_t eventAfterSeconds,
                         time_t eventStratEndTime);

    /**
     * �����¼�Դ<strAlarmDeviceId>����Ϊ<eventType>��¼���¼�����������ͷ<strLensId>¼��.
     */
    UTAPI int32_t event_happen(   const char * const strLensId,
                        const char *strAlarmDeviceId,
                        uint32_t eventType,
                        uint8_t encoderType,
                        time_t endTime,
                        uint32_t eventBeforeSeconds,
                        uint32_t eventAfterSeconds,
                        bool bIsRecord);

    /**
     * ����<strAlarmDeviceId>����Ϊ<eventType>��¼���¼���������ֹͣ��ͷ<strLensId>¼��.
     */
    UTAPI int32_t event_over( const char * const strLensId,
                    const char *strAlarmDeviceId,
                    uint32_t eventType );

    // ���ܣ�ȡָ���豸�Ļ�����Ϣ
    UTAPI int32_t debug_get_device_info(  const char *strFrontDevId,
                                char *strDeviceInfo,
                                uint32_t szDeviceInfo );

    // ���ܣ�ȡ�����豸�Ļ�����Ϣ
    UTAPI int32_t debug_get_all_device_info(  char *strDeviceInfo,uint32_t szDeviceInfo );

    // ���ܣ�ȡ�����豸����ϸ��Ϣ
    UTAPI int32_t debug_get_all_device_detail_info( char *strDeviceInfo, uint32_t szDeviceInfo );

    // ����:�ر�ָ��·�ɵ�¼��·��
    UTAPI int32_t shutdown_router( const uint32_t RouterID );

    // ����:����ָ����·��ID�����豸
    UTAPI CAC_Realtime_Record_Device * get_device_by_routerId( uint32_t RouterId );

    // ¼��������¼����
    //UTAPI int32_t handle_record_index( list<ACE_Message_Block*> &recordIndexList ) const;

    /*
     * FUNCTION: void upload_routerinfo()
     * DESCRIPTION: ����ǰ���豸�ϴ�¼���ļ���·�������Ӧ����Ϣ
     * INPUT:
     *    ACE_Message_Block *mb -- ��Ϣ��
     * OUTPUT:
     *   none
     */
    UTAPI int32_t handle_routerinfo(const ACE_Message_Block *mb);


   // �ر������豸��·��
    UTAPI void shutdown_router();


   //  ����ָ���豸¼��
    int32_t start_device_record(const char * const strDevId);

    /*
     * FUNCTION: int32_t stop_device_record()
     * DESCRIPTION: ָֹͣ���豸¼�� --- ����û��¼�������
     * INPUT:
     *   const char * const strDevId  --- �豸ID
     * OUTPUT:
     *   �ɹ�����0��ʧ�ܷ��ش������
     */
    UTAPI int32_t stop_device_record( const char * const strDevId,
                            bool bSendOverMsg = true );

    /*
     * FUNCTION: int32_t device_off_line()
     * DESCRIPTION: �����豸����.
     * INPUT:
     *   const char * const strDevId  --- �豸ID
     * OUTPUT:
     *   �ɹ�����0��ʧ�ܷ��ش������
     */
    UTAPI int32_t device_off_line(const char * const strDevId);


   // ��ָ����������ü�������������˶����Ѿ�Ϊֹͣ״̬�����ü���Ϊ����
   // ��ô�����ٴ˶���
    UTAPI int32_t decrease_reference(CAC_Realtime_Record_Device *pDevice);

    //���ܣ���ȡ��ǰ����¼����豸����
    UTAPI uint32_t get_recording_device_num( void )
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, 0);
        return device_map_.current_size();
    }

    // ��������¼��ʱ�޸������ݿ��֪ͨ����¼��Ķ���
    UTAPI int32_t overwrite_record_notify(char * strLensId,
                                         time_t newIndexStartTime);

    //��ʱ��ʱ�䴦��
    UTAPI int32_t on_timer(int32_t eventId, int32_t timerId, int32_t nDummy);

    // ��ʱ���ص�����
    static void timer_callback( void *pArg,
                                int32_t eventId,
                                int32_t timerId,
                                int32_t nDummy );


    // ����¼�¼��̬����֪ͨ�ӿ�
    UTAPI int32_t refresh_event_record_outtime(int32_t shiftValue);

protected:
    // ����ģʽ�Ĺ��캯����ʹ��protected����
    CAC_RT_Record_Device_Manager();
    UTAPI ~CAC_RT_Record_Device_Manager();

    // ���һ��ʵʱ¼���豸����
    UTAPI int32_t add_device( const char *strFrontDevId,
                    uint8_t encoderType,
                    CAC_Realtime_Record_Device *&pDevice );

    // ���ݴ����¼��������Ϣ����SQL���
    UTAPI int32_t build_update_record_index_sql(  const ACE_Message_Block* mb,
                                        char *sqlBuff,
                                        uint32_t sqlBuffSize) const;

    // �����豸״̬����ʧ��
    UTAPI void count_dev_num_by_status(
                                    const CAC_Realtime_Record_Device * pDevice,
                                    SVS_STATUS_DEV_NUM_T & devStateNum)const;

    UTAPI int32_t bind2readonlyDeviceMap(CAC_Realtime_Record_Device* pDevice);
protected:
    // ���ͷ��
    uint32_t max_lens_;

    // ��ͷID��¼�����ӳ���
    SVS_DEVICE_MAP device_map_;

    // ����ʵʱ¼����豸��������
    vector <CAC_Realtime_Record_Device *> using_vector_;
    ACE_DLList <char> free_list_;

    ACE_Recursive_Thread_Mutex map_mutex_;

    // ���ڶ�ʱ�������map�������ͷŶ�����device map�ֿ�����С���ľ���
    SVS_DEVICE_MAP readonly_device_map_;
    ACE_Recursive_Thread_Mutex readonly_device_map_mutex_;

#ifdef UNITTEST
    friend class TEST_CAC_RT_Record_Device_Manager;
    friend class CAC_RT_Record_Device_ManagerAutoMock;
#endif
};
#endif //_Real_Record_Lens_h


