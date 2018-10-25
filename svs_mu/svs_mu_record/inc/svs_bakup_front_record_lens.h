/*****************************************************************************
   ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾
 ******************************************************************************
  �ļ���          : SVS_Bakup_Front_Record_Lens.h
  �汾��          : 1.0
  ��������        : 2008-8-15
  ����޸�        :
  ��������        : ǰ��¼�񱸷ݾ�ͷ������ͱ��ݾ�ͷ�࣬������ʵ�ֶ�ÿ�����ݾ�ͷ
                    ����Ĺ������ݾ�ͷ������ݱ���������ƽ̨��ǰ��¼��������
                    �ص������ͨ��¼��ط������ǰ��¼�񱸷ݵ�ƽ̨�Ĵ��������ϡ�
  �����б�        :
  �޸���ʷ        :
  1 ����          : 2008-8-15
    �޸�����      : ���� *******************************************************************************/
#ifndef _SVS_BAKUP_FRONT_RECORD_LENS_H_
#define _SVS_BAKUP_FRONT_RECORD_LENS_H_

// ����������ÿ����Ҫ���ݵĶ���������ݶ�
#define RECORDBAK_SECTION_MAX_NUM 255
typedef struct RTSECTIONINFO_
{
    time_t StartTime;
    time_t EndTime;
} RTSECTIONINFO, *PRTSECTIONINFO;

typedef ACE_Vector <RTSECTIONINFO> SECTIONINFO_VECTOR;

// ǰ��¼�񱸷��豸��״̬
enum BAKUP_DEVICE_STATE
{
    RECORDBAK_STATE_INIT = 0,        //��ʼ��״̬
    RECORDBAK_STATE_SLEEP,
    RECORDBAK_STATE_REQUEST_ROUTER, //���������ϵͳ����VTDU
    RECORDBAK_STATE_CONNECT_VTDU,   //����VTDU��֪ͨ���ն˿�
    RECORDBAK_STATE_RECEIVING_FILE, //�����ļ���
    RECORDBAK_STATE_WAITING_OVER,   //�ȴ�����
};

// ǰ��¼�񱸷��豸��
class CAC_Bakup_Record_Dev : public CSVS_Record_Lens
{
public:
    enum
    {
        TIMER_TYPE_NORMAL = 1,
        TIMER_TYPE_CHECK_STATUS = 2,
    };
    CAC_Bakup_Record_Dev(const char *strLensId, const uint32_t nIndex );
    virtual ~CAC_Bakup_Record_Dev();
    // --------------------�豸��������ýӿ�-----------------------------
    // ��ʼ��
    UTAPI bool init_device(const ACE_Message_Block *mb);

    // ֹͣ����ǰ��¼��
    UTAPI int32_t  stop_backup_record(bool procRouter = true);

    // ·������
    UTAPI int32_t  request_router();

    // ֹͣ·��
    UTAPI int32_t  shutdown_router();

    // ·����Ϣ����
    UTAPI int32_t  handle_routerinfo(const void *pbyMsg);

    // ��ͷ����λ
    UTAPI int32_t  reset();

    UTAPI void on_timer(int32_t eventId, int32_t timerId ,int32_t nDummy);

    // ��ȡ������Ϣ
    UTAPI int32_t debug_get_device_info(char * strDeviceInfo, uint32_t ulBuffLen);

    // ��ȡ�豸״̬
    UTAPI uint8_t get_state() const
    {
        return state_;
    }

    // ��ȡ·��ID
    UTAPI routerid_t get_router_id()  const
    {
        return router_id_;
    }

public:
    // ���������
    UTAPI int32_t proc_eos_packet();
    int32_t play_media_request();

    UTAPI uint16_t get_router_msgno() const
    {
        return router_msgno_;
    }

    // �ط���һ��
    UTAPI int32_t drop_next_section();

private:
    // ��VTDU����ע����Ϣ
    UTAPI int32_t  start_backup_record();
    // ����ǰ��¼�񱸷�������Ϣ��db

    // ���ñ���״̬
    UTAPI int32_t set_status(uint8_t newStatus);

    // ����·�ɶ�ʱ������
    UTAPI void request_router_on_timer();

    // ����VTDU��ʱ������
    UTAPI void conn_vtdu_on_timer();

    // ������״��Ķ�ʱ������
    UTAPI void revc_stream_on_timer();

    // �ȴ�¼�������ʱ��
    UTAPI void sleep_on_timer();

    // ��ȡ�豸״̬--������� string
    UTAPI void debug_get_device_status(string & strDeviceStatus)const;
private:
    //UTAPI void parse_flat_record_index(DTRecordIndex &stRecordIndex);

    // ��ѯƽ̨¼������
    //UTAPI int32_t  query_record_info();

    // ���ɱ��ݲ���
    //UTAPI int32_t  build_recordbak_strategy();

    // ɸѡ����ָ���ƽ̨�����ص���ǰ��������Ϣ
    //UTAPI void filter_section(const RTSECTIONINFO& sectInfo, SECTIONINFO_VECTOR &resultSectVector);

    //UTAPI int32_t  bubble_up_sort_sectioninfo(SECTIONINFO_VECTOR & vecSectionInfo) const;

    // ���ε�ǰ���ظ�����������Ϣ
    UTAPI void delete_overlap_sections(SECTIONINFO_VECTOR & sectVector)const;

    // ƴ��������������
    //UTAPI void join_replay_sections(SECTIONINFO_VECTOR & sectVector) const;

    // ����ֹͣ��������Ϣ
    UTAPI int32_t send_stop_recv_msg();
public:

    enum RECORD_REPLAR_TYPE
    {
        RECORD_REPLAR_TYPE_FRONT = 0x04    // ǰ��¼��ط�
    };
private:
    CAC_Bakup_Record_Dev();
private:
    // =======================BEING ��ͷ�������Գ�Ա============================
    // ·��ID
    routerid_t router_id_;

    // ����·����Ϣ��Ӧ����ϢID��
    uint16_t router_msgno_;

    //VTDU��ַ
    ACE_INET_Addr vtdu_addr_;

    uint32_t    m_ulIOHandlerIndex;

    // ��ǰ¼���ļ�
    ACE_FILE_IO record_file_;

    // ��ǰ״̬����ʼʱ��
    time_t status_start_time_;

    // =======================BEGIN ���������Ա================================
    // ��ǰ���ڻطŵ�section��
    uint32_t cur_replay_sect_index_;

    // ���յ���һ֡ʱϵͳʱ��͵�ǰ��Ҫ�طŵ�¼�����ʼʱ���ֵ
    time_t time_remove_value;

    // ��Ҫ�ط������section��Ϣ
    SECTIONINFO_VECTOR replay_sections_vector_;

    // ƽ̨������section��Ϣ
    //SECTIONINFO_VECTOR flat_sections_vector_;// Ϊ��Ԫ����������Ԫ��
#ifdef UNITTEST
    friend class TEST_CAC_Bakup_Record_Dev;
#endif
};

// ǰ��¼�񱸷��豸������
class CAC_Bakup_Record_Dev_Manager
{
public:
    // ����
    static CAC_Bakup_Record_Dev_Manager *instance()
    {
        static CAC_Bakup_Record_Dev_Manager manager;
        return &manager;
    }

    // ��ʼ��������nMaxLensָ��¼�����ͷ��������ʵʱ¼��ͱ��ݾ�ͷ
    bool init(uint32_t nMaxLens);

    // ��ǰ����ǰ��¼�񱸷ݵľ�ͷ��
    uint32_t get_bakup_device_num()
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, 0);
        return bakup_device_map_.current_size();
    }

    int32_t  decrease_reference(CAC_Bakup_Record_Dev *pBakDevice);

    // ��������֪ͨ
    int32_t  bakup_record_notify(const ACE_Message_Block *mb);

    // ǰ��¼��ط�·�ɴ���
    int32_t  handle_routerinfo(const ACE_Message_Block *mb);

    int32_t add_msgno_map(CAC_Bakup_Record_Dev* bakDevice, uint16_t msgNo)
    {
        // �������Ѷ������map
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, 0);
        return bakup_device_msgno_map_.bind(msgNo, bakDevice);
    }

    int32_t delete_msgno_map(uint16_t msgNo)
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, 0);
        return bakup_device_msgno_map_.unbind(msgNo);
    }

    // ɾ��ָ��ID�ı��ݾ�ͷ����
    int32_t  stop_device(const char *strDevId);

    // ɾ��ָ��·��ID�ľ�ͷ
    int32_t  stop_device(const uint32_t routerId);

    // ֹͣ���б�������
    int32_t  stop_all_device();

    // ���������
    int32_t  proc_eos_packet(uint32_t streamId);
    int32_t play_media_request(uint32_t streamId);


    // ��ʱ�����ں�Ĵ�����
    int32_t on_timer(int32_t eventId, int32_t timerId, int32_t nDummy);

    int32_t debug_all_bakup_device_info(char* debugBuf,
                                                const uint32_t bufLen);

    static void timer_callback(void *pArg, int32_t eventId, int32_t timerId, int32_t nDummy );

    int32_t debug_get_device_info(const char * strFrontDevId,
                                    char * strDeviceInfo,
                                    uint32_t szDeviceInfo);

    // ��ѯ��ͷ�Ƿ���ǰ��¼�񱸷�
    bool is_lens_bakup_record(const char* strLensId);

    int32_t update_backup_device_flag();
protected:
    // ���캯��
    CAC_Bakup_Record_Dev_Manager();

    // ��������
    ~CAC_Bakup_Record_Dev_Manager();

    // ��ӱ��ݾ�ͷ����
    CAC_Bakup_Record_Dev * add_device( const char *strDevId );

    // ���ݾ�ͷID��ȡ���ݾ�ͷ����
    CAC_Bakup_Record_Dev * get_device(const char* strDevId);

    // �����ڲ�ID��ȡ�豸��ͷ����
    CAC_Bakup_Record_Dev * get_device(const uint32_t RouterId);

    // ����·��ID��ȡ�豸��ͷ����
    CAC_Bakup_Record_Dev * get_device_by_routerid(const uint32_t RouterId);

    // ������Ϣ�Ż�ȡ���ݾ�ͷ����
    CAC_Bakup_Record_Dev * get_device_by_msgno(uint16_t msgNo);
protected:
    typedef ACE_Hash_Map_Manager < const char *, CAC_Bakup_Record_Dev *, ACE_Null_Mutex >
    BAKUP_DEV_MAP;

   typedef ACE_Hash_Map_Manager < uint16_t, CAC_Bakup_Record_Dev *, ACE_Null_Mutex >
    BAKUP_DEV_MSGNO_MAP;

   typedef ACE_Hash_Map_Iterator < const char *, CAC_Bakup_Record_Dev *, ACE_Null_Mutex >
    BAKUP_DEV_MAP_ITER;

    typedef ACE_Hash_Map_Entry < const char *, CAC_Bakup_Record_Dev * >
    BAKUP_DEV_MAP_ENTRY;

    typedef ACE_Hash_Map_Manager < ACE_CString, time_t, ACE_Null_Mutex > BACKUP_TIME_MAP;

    typedef ACE_Hash_Map_Iterator < ACE_CString, time_t, ACE_Null_Mutex > BACKUP_TIME_MAP_ITER;

    typedef ACE_Hash_Map_Entry < ACE_CString, time_t> BACKUP_TIME_MAP_ENTRY;

    // ��󱸷ݾ�ͷ��
    uint32_t max_bakup_device_;

    // ���ݾ�ͷID�뱸�ݶ���ӳ���
    BAKUP_DEV_MAP bakup_device_map_;

    // ��Ϣ����źͱ��ݾ�ͷ����ӳ���
    BAKUP_DEV_MSGNO_MAP bakup_device_msgno_map_;

    vector <CAC_Bakup_Record_Dev *> using_vector_;

    ACE_DLList <char> free_list_;

    // ��ͷ������
    ACE_Recursive_Thread_Mutex map_mutex_;
    BACKUP_TIME_MAP backup_time_map_;

// Ϊ��Ԫ����������Ԫ��
#ifdef UNITTEST
    friend class TEST_CAC_Bakup_Record_Dev_Manager;
#endif
};
#endif // _SVS_BAKUP_FRONT_RECORD_LENS_H_


