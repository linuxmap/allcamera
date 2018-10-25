#ifndef _SVS_Record_Inform_h
#define _SVS_Record_Inform_h

#include <list>
#include <vector>

#include "svs_record_front_device.h"
#include "svs_strategy_list.h"
#include "ace/Guard_T.h"
#include "ace/Recursive_Thread_Mutex.h"

using namespace std;

class CAC_Record_Inform_Manager;
class CAC_Record_Inform;

/*
 * CLASS: CAC_Record_Inform
 * DESCRIPTION: ¼��֪ͨ��
 */
class CAC_Record_Inform : public CAC_Record_Front_Device
{
public:
    // const char *strFrontDevId --- �豸ID�� ���ַ��������ַ���
    CAC_Record_Inform(  const char *strFrontDevId,
                        uint8_t encoderType,
                        CAC_Record_Inform_Manager *pManager );
    virtual ~CAC_Record_Inform();
public:

    // ��¼����Լ���<stratVector>��ʼ��¼��֪ͨ����
    UTAPI bool init_strate(const STRAT_VECTOR& stratVector);

    // ��ʱ���ص���������⾵ͷ��¼�����
    UTAPI int32_t  on_check_strategy(int32_t eventId, int32_t timerId);

    // ִ��¼�����
    UTAPI int32_t  exec_strategy();

    // ����¼����ԣ����´����ݿ�����豸��¼������б�ѡ����Ч��¼�����ִ�С�
    // ���û��¼����ԣ���ô����true, �����߿���ɾ�����豸�����򷵻�false.
    UTAPI bool renew_strategy(const STRAT_VECTOR& stratVector);

    // ����ͷֹͣ��Ϣ
    UTAPI int32_t  off_line();

    // ��ǰ�Ƿ�����Ч���¼�¼�����
    UTAPI bool effect_event_stratetegy() const
    {
        // �����û�в���״̬�����¼�¼����Կ϶�����Ч
        if(RECORDING_NONE == record_flags_)
        {
            return false;
        }

        return (strat_list_.get_event_strategy_time() > 0);
    }

    // ֹͣ¼��
    UTAPI int32_t stop_record( );

    UTAPI int32_t stop_inform();

    // ¼����Ա�ʶ
    UTAPI uint8_t record_flags()  const
    {
        return record_flags_;
    }

    // ��ͷ״̬��ʶ
    UTAPI uint8_t lens_state()  const
    {
        return lens_state_;
    }

    // ��ͷ״̬��ʶ
    UTAPI uint8_t state()  const
    {
        return state_;
    }

    //��ʼ����ͷ
    UTAPI int32_t init_lens();

    // ����¼�����
    UTAPI int32_t update_record_parameter(    uint32_t nBeforeSeconds,
                                    uint32_t nAfterSeconds );

    // ��������
    UTAPI uint8_t encoder_type() const
    {
        return encoder_type_;
    }

    // ��ǰ���Խ���ʱ��
    UTAPI time_t curr_strategy_end_time()
    {
        return strat_list_.end_time();
    }

    // �¼�¼��Ԥ¼ʱ��
    UTAPI uint32_t event_before_record_time() const
    {
        return event_before_record_time_;
    }

    // �¼�¼���¼ʱ��
    UTAPI uint32_t event_after_record_time() const
    {
        return event_after_record_time_;
    }

    // �����豸״̬
    UTAPI void device_status( uint8_t deviceStatus )
    {
        lens_state_  = deviceStatus;
    }

    UTAPI int32_t alarm_event_req(const char * strAlarmDeviceId, uint32_t eventType, uint16_t controlCode);

protected:
    // �����豸״̬
    UTAPI void state(uint8_t byState)
    {
        ACE_GUARD(ACE_Recursive_Thread_Mutex, pObj, mutex_);
        if (FRONT_DEVICE_STATE_STOP != this->state_)
        {
            state_ = byState;
        }
    }

    // ����¼��֪ͨ��ͬʱ����������¼�񣬻���ֹͣ¼��
    UTAPI int32_t  inform_recording();
    // ���ö�ʱ��
    UTAPI int32_t set_timer( uint8_t byState,
                    const ACE_Time_Value &delayTime,
                    const ACE_Time_Value &intervalTime = ACE_Time_Value::zero );

    // �ͷŶ�ʱ��
    UTAPI int32_t  kill_timer(uint8_t byState);
    // ��ʼ����ͷ��¼�����
    UTAPI int32_t init_record_parameters();
    UTAPI int32_t informEvent();
private:
        CAC_Record_Inform();

public:
    enum SVS_FRONT_DEVICE_STATE
    {
        FRONT_DEVICE_STATE_INIT = 0,           //��ʼ��״̬
        FRONT_DEVICE_STATE_QUERY_STRATEGY, //��ѯ¼�����
        FRONT_DEVICE_STATE_INFORMING,      //¼��֪ͨ״̬
    };

    enum SVS_RECORD_TYPE
    {
        RECORD_TYPE_NORMAL          = 0x00, //��ͨ������ʱ��¼��
        RECORD_TYPE_MOTIONDETECT    = 0x01, //��̬���
        RECORD_TYPE_ALARM           = 0x02, //�澯¼��
        RECORD_TYPE_MAX,
    };

protected:
    // ����������
    uint8_t encoder_type_;
    // ¼�����ͱ�ʶ
    uint8_t record_flags_;
    // ��ͷ������ָ��
    CAC_Record_Inform_Manager *p_manager_;
    // ���Զ�ʱ��ID
    int32_t timer_id_;

    // ¼������б�
    CAC_Strategy_List strat_list_;
    //�¼�¼��Ԥ¼ʱ������λ����
    uint32_t event_before_record_time_;
    //�¼�¼���¼ʱ������λ����
    uint32_t event_after_record_time_;

    SVS_Inform_Stat inform_stat_;

    //�¼�����
    int32_t total_event_;

    //¼���¼���ΪRECORD_EVENT_CONTROL_TYPE_MAX(=5)��, ÿ��¼���¼�������
    //��ͬ�澯Դ����. ͬһ�澯Դ���Բ�������¼���¼�.
    vector <EVENTMAP > event_vector_;
#ifdef UNITTEST
    friend class CAC_Record_InformAutoMock;
    friend class TEST_CAC_Record_Inform;
#endif
};

/**
 * @class CAC_Record_Inform_Manager
 * @brief ��ͷ¼��֪ͨ������, �ǵ�����
 */
class CAC_Record_Inform_Manager
{
protected:
    typedef struct tagDistributeLensInfo
    {
        DEVICE_DISTRIBUTE_INFO DistributeInfo;
        uint32_t RespCode;
    }DistributeLensInfo;

    typedef vector <DistributeLensInfo> DISTRIBUTELENSINFO_VECTOR;

    typedef struct TagLocalLensInfo
    {
        char          LensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
        uint8_t EncoderType;
    }LocalLensInfo;

    typedef vector <LocalLensInfo> LENSINFO_VECTOR;

public:
    static CAC_Record_Inform_Manager * instance();

    UTAPI bool init(uint32_t nMaxLens);

    UTAPI void strategy_time_manager(CAC_Timer_Manager *pTimerManager)
    {
        this->p_strategy_timer_manager_ = pTimerManager;
    }

    UTAPI int32_t remove_device(uint32_t internalId);

    UTAPI const uint32_t current_device_size()
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, 0);
        return device_map_.current_size();
    }

    UTAPI int32_t check_strategy_loop(    ACE_Reactor *pReactor,
                                CAC_Timer_Manager *pTimerManager );

    // ����¼����Ը���֪ͨ��Ϣ,�ɹ�����0��ʧ�ܷ���-1
    UTAPI int32_t renew_strategy(const ACE_Message_Block *mb);

    // �������Ը澯�������ĸ澯�¼�
    UTAPI int32_t alarm_event_req(const ACE_Message_Block *mb);

    // ��ָ����������ü�������������˶����Ѿ�Ϊֹͣ״̬�����ü���Ϊ������ô�����ٴ˶���
    // ����ɹ����أ���ʧ�ܷ���-1.
    UTAPI int32_t decrease_reference(CAC_Record_Inform *pDevice);

    UTAPI int32_t set_timer(  uint32_t eventId,
                    const ACE_Time_Value &delayTime,
                    const ACE_Time_Value &intervalTime )
    {
        int32_t allocResult =  p_strategy_timer_manager_->alloc_timer(
                                this,
                                check_strategy_timer_callback,
                                (int32_t)eventId,
                                delayTime,
                                intervalTime );

        return allocResult;
    }

    UTAPI int32_t kill_timer(int32_t timerId)
    {
        int32_t freeTimerResult = p_strategy_timer_manager_->free_timer(timerId);

        return freeTimerResult;
    }

    UTAPI int32_t reset_timer_interval(int32_t timerId, const ACE_Time_Value& intervalTime)
    {
        int32_t result = p_strategy_timer_manager_->reset_timer_interval(timerId, intervalTime);

        return result;
    }
    // ֹͣ����¼�񣬲�������еľ�ͷ����
    UTAPI int32_t stop_all_record();

    UTAPI int32_t on_check_strategy(int32_t eventId, int32_t timerId);

    UTAPI int32_t debug_get_all_lens_info(  char *strLensInfo, const uint32_t szLensInfo );

    UTAPI CAC_Record_Inform * get_device(const char *strFrontDevId);

    // �����豸¼�����
    UTAPI int32_t update_record_parameter(const ACE_Message_Block *mb);

    // �����豸¼��������Ϣ
    UTAPI int32_t handle_record_task(const ACE_Message_Block *mb);

    UTAPI int32_t start_record(const SVS_DEVICE_DISTRIBUTE_REQ_MSG * pReqMsg);

    UTAPI int32_t stop_record(const SVS_DEVICE_DISTRIBUTE_REQ_MSG * pReqMsg);

    UTAPI void send_distribute_reponse(uint16_t msgNo,
                                    uint32_t distributeType,
                                    //DISTRIBUTELENSINFO_VECTOR & lensVect
                                    const char *pszLensId,
                                    uint32_t  uiResponseCode)const;

    // ��ȡָ����ͷ�Ĳ���
    UTAPI int32_t get_lens_param(const char* strLensId, SVS_LENS_PARAMETER& lensParameter);

protected:
    CAC_Record_Inform_Manager();
    virtual ~CAC_Record_Inform_Manager();

    // ��ʼ�����NRU������ľ�ͷ����
    UTAPI int32_t init_lens_param();

    // ����ָ����ͷ�Ĳ���
    UTAPI int32_t update_lens_param(const char* strLensId,
                                    const SVS_LENS_PARAMETER & lensParameter,
                                    bool isDelLens = false);

    // ��������<nId>�����豸ID��ѯ¼��֪ͨ�����������ѯ��¼��֪ͨ���ڣ�
    // ��ô���������ü���.
    // �ɹ�����¼��֪ͨ����ָ�룬���򷵻�NULL��
    UTAPI CAC_Record_Inform * get_device(uint32_t nId);

    static void check_strategy_timer_callback(  void *pArg,
                                                int32_t eventId,
                                                int32_t timerId,
                                                int32_t /*nDummy*/ );

    // ��Ӿ�ͷ<strLensId>,������ͷ���󣬲���ӵ�map�С�<pDevice>���ؾ�ͷ����
    // �ɹ�����0��ʧ�ܷ��ش�����롣
    UTAPI int32_t add_device( const char *strLensId,
                    int32_t encoderType,
                    CAC_Record_Inform *&pDevice );

    // ɾ����ͷ<strDevId>, �ɹ�����0��ʧ�ܷ���-1��
    UTAPI int32_t remove_device(const char *strDevId);

    // ����������
    UTAPI uint32_t next_internal_id();

    // ��ѯָ������������<encoderType>�Ƿ���¼������������б��С�
    // �����ѯ������true, ���򷵻�false.
    UTAPI bool encodertype(uint8_t encoderType);

    UTAPI int32_t check_strategy();

    // ����ָ����ͷID<strDeviceId>��¼����ԡ��ɹ�����0��ʧ�ܷ���-1
    UTAPI int32_t renew_strategy(const char *strDeviceId,
                        const STRAT_VECTOR &stratVector);

    // �澯��ת��
    UTAPI int32_t alarm_code_translate(   uint8_t alarmCode,
                                uint32_t &eventType,
                                uint8_t &alarmType ) const;

    UTAPI int32_t check_device_strategy(const SVS_DEVICE_DISTRIBUTE_REQ_MSG* pReqMsg) const;

    UTAPI int32_t fill_strategy_list(uint32_t uiStrategyCount,
                                const RECORD_STRATEGY_INFO *pStrategyInfo,
                                STRAT_VECTOR &stratVector) const;
    UTAPI void sdcard_alarm_code_translate( uint8_t  alarmCode,
                                                    uint32_t &eventType,
                                                    uint8_t &alarmType ) const;

     UTAPI void intelligence_alarm_code_translate( uint8_t  alarmCode,
                                                    uint32_t &eventType,
                                                    uint8_t &alarmType ) const;

public:
    typedef ACE_Hash_Map_Manager < const char *, CAC_Record_Inform *, ACE_Null_Mutex >  SVS_DEVICE_MAP;
    typedef ACE_Hash_Map_Manager <uint32_t, CAC_Record_Inform *, ACE_Null_Mutex>  SVS_STREAM_MAP;
    typedef ACE_Hash_Map_Iterator < const char *, CAC_Record_Inform *, ACE_Null_Mutex > SVS_DEVICE_MAP_ITER;
    typedef ACE_Hash_Map_Entry < const char *, CAC_Record_Inform * > SVS_DEVICE_MAP_ENTRY;
    typedef ACE_Hash_Map_Manager <ACE_CString, SVS_LENS_PARAMETER, ACE_Null_Mutex> SVS_LENS_PARAMETER_MAP;
protected:
    // ���ͷ��
    uint32_t max_lens_;

    // �豸״̬
    uint8_t state_;
    enum FRONTDEV_MANAGER_STATE
    {
        FDM_STATE_CLOSE = 0,                //��ʼ��״̬
        FDM_STATE_QUERY_DEVICE,         //��ѯ�豸
        FDM_STATE_QUERY_DEVSTRATEGY,    //��ѯ�豸�Ĳ���
        FDM_STATE_CHECK_STRATEGY,       //���¼�����
        FDM_STATE_START_RECORD,         //����¼��
        FDM_STATE_RUNNING,              //����״̬
        FDM_STATE_SHUTDOWN             //����״̬
    };

    // �洢¼���ļ���ǰ���豸�б�
    SVS_DEVICE_MAP device_map_;
    SVS_LENS_PARAMETER_MAP lens_parameter_map_;
    ACE_Recursive_Thread_Mutex lens_parameter_map_mutex_;

    // ��һ���ڲ�ID
    ACE_Reactor *p_strategy_reactor_;
    CAC_Timer_Manager *p_strategy_timer_manager_;
    uint32_t next_internal_id_;
    ACE_Recursive_Thread_Mutex map_mutex_;

    // ����������豸�����ӳ��, �����ڶ�ʱ��
    SVS_STREAM_MAP request_map_;

#ifdef UNITTEST
    friend class CAC_Record_Inform_ManagerAutoMock;
    friend class TEST_CAC_Record_Inform_Manager;
#endif
};
#endif //_SVS_Record_Inform_h



