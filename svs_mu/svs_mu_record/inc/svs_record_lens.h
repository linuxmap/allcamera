#ifndef _SVS_Record_Lens_h
#define _SVS_Record_Lens_h
#include "svs_device_state.h"
#include "svs_record_disk.h"
//Ԥ�ȷ���¼��ͷ�����ڴ�, 2006-7-21
#define RECORD_LENS_CACHE
struct SVS_Record_Index
{
    time_t start_time_; //��ʼʱ��
    time_t end_time_;   //����ʱ��
};
// ¼������������
class CSVS_Record_Index_List
{
public:
    CSVS_Record_Index_List()
    {};
    UTAPI ~CSVS_Record_Index_List()
    {}

    //��ȡָ��ʱ��μ��¼�������б�
    UTAPI int32_t get_record_index_list( time_t startTime,
                               time_t endTime,
                               list<SVS_Record_Index> &recordIndexList);

    //���һ��¼������
    UTAPI int32_t add_record_index( time_t startTime, time_t endTime )
    {
        SVS_LOG((SVS_LM_DEBUG,
            "Add record index to index list."
            "index start time[%d], index end time[%d].",
            startTime,
            endTime));
        record_index_map_[startTime] = endTime ;
        return 0;
    }

    //����¼�������Ľ���ʱ��
    UTAPI int32_t update_record_index( time_t startTime, time_t endTime )
    {
       char strStartTime[TIME_STRING_LEN] = {0,};
        char strEndTime[TIME_STRING_LEN] = {0,};
        SVS_LOG((SVS_LM_DEBUG,
            "Begin to update record index to index list."
            "index start time[%d--%s],index end time[%d--%s], mapSize[%u]",
            startTime,
            SVS_SS_UTILITIES::time2str(strStartTime, sizeof(strStartTime), startTime),
            endTime,
            SVS_SS_UTILITIES::time2str(strEndTime, sizeof(strEndTime), endTime),
            record_index_map_.size()));
        if ( record_index_map_.end() != record_index_map_.find( startTime ) )
        {
            SVS_LOG((SVS_LM_DEBUG,
                "Update record index list succeeded."
                "start time[%d], end time[%d].",
                startTime,
                endTime ));
            record_index_map_[startTime] = endTime ;
            return 0;
        }
        return -1;
    }
    UTAPI void clear()
    {
        record_index_map_.clear();
    }
protected:
    //������õ�¼������
    UTAPI void clear_useless_record_index_list( time_t startTime);
private:

    //¼�������б�
    map< time_t, time_t> record_index_map_;
};

enum RECORD_LENS_TYPE
{
    LENS_TYPE_BASE = 0,
    LENS_TYPE_REALTIME = 1,
    LENS_TYPE_RECORDBAK = 2
};


/**
 * @class CSVS_Record_Lens
 * @brief ¼��ͷ�������
 */
class CSVS_Record_Lens
{
protected:
    CSVS_Record_Lens(const char * const strLensId, uint32_t nIndex);
    CSVS_Record_Lens(uint32_t nIndex);
public:
    virtual ~CSVS_Record_Lens();
    CSVS_Record_Lens();
public:
    virtual bool init(){return true;}
    /*
    * FUNCTION: int32_t increase_reference()
    * DESCRIPTION: ����Ӧ�ü���
    * INPUT:
    *   NONE
    * OUTPUT:
    *   �ɹ����أ������򷵻�-1.
    */

    UTAPI int32_t increase_reference();
    /*
    * FUNCTION: int32_t decrease_reference()
    * DESCRIPTION: �ݼ����ü���������ݼ������ü������ڣ�����·���Ѿ�ֹͣ����ô����ɾ���ζ���
    * INPUT:
    *   NONE
    * OUTPUT:
    *   �ݼ���������ü������ڣ�����·���Ѿ�ֹͣ����ô���أ������򷵻أ���
    *   ��������쳣(����ʧ�ܵ�)����-1��
    */
    UTAPI int32_t decrease_reference();

    UTAPI const char *get_frontdev_id() const ;

    UTAPI uint32_t internal_id() const ;

    virtual uint8_t get_device_type() const;

protected:
    virtual void constructor();

protected:

    //�ڲ�ID�����ڶ�ʱ��
    const uint32_t internal_id_;
    enum REAL_TIME_RECORDING_CONST
    {
        RECORD_CHECK_INTERVAL = 300,   //10����
    };

    enum RECORD_DEVICE_STATE
    {
        FRONT_DEVICE_STATE_STOP = 0x20,
    };

    //���ü�������ֻ�е���0ʱ���ſ����ͷű�����
    volatile int32_t referenc_count_;

    //ǰ���豸ID
    char str_frontdev_id_[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    volatile uint8_t state_;

    ACE_Recursive_Thread_Mutex mutex_;

    uint8_t  lens_type_;
};
#endif //_SVS_Record_Lens_h


