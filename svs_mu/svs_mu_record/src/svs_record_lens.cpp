#include <sys/statvfs.h>
#include "vms/vms.h"
#include "svs_log_msg.h"
#include "svs_stat_manager.h"
#include "svs_record_index.h"
#include "svs_real_record_lens.h"
#include "svs_record_lens.h"
#include "svs_rt_record_common.h"

//=======================CSVS_Record_Index_List BEGIN===========================
/*****************************************************************************
 �� �� ��  : get_record_index_list
 ��������  : ��ȡָ��ʱ��μ��¼�������б�
 �������  : startTime  :   ¼��ʼʱ��
             endTime    :   ¼�����ʱ��
 �������  : recordIndexList : ¼�������б�
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Record_Index_List::get_record_index_list( time_t startTime,
                                                   time_t endTime,
                                                   list<SVS_Record_Index> &recordIndexList)
{
    SVS_TRACE();
    recordIndexList.clear();

    char strStartTime[TIME_STRING_LEN] = {0,};
    char strEndTime[TIME_STRING_LEN] = {0,};

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to get record index from index list."
        "start time[%d--%s], end time[%d--%s], mapSize[%u]",
        startTime,
        SVS_SS_UTILITIES::time2str(strStartTime, sizeof(strStartTime), startTime),
        endTime,
        SVS_SS_UTILITIES::time2str(strEndTime, sizeof(strEndTime), endTime),
        record_index_map_.size()));


    map< time_t, time_t>::iterator it = record_index_map_.begin();
    SVS_Record_Index recordIndex;
    for( ; it != record_index_map_.end(); ++it )
    {
        SVS_LOG((SVS_LM_DEBUG,
            "find a index section in index list."
            "section information:first time[%d],second time[%d]",
            it->first,
            it->second));

        // �����ǰ��������εĽ���ʱ�䲻Ϊ0(Ϊ0����ʾ�����)
        // ��Ҫ��ȡ�Ŀ�ʼʱ����������ν���ʱ�䣬������
        if ( (startTime > it->second) && ( 0 != it->second) )
        {
            continue;
        }

        // ���Ҫ��ȡ�Ľ���ʱ��ȸö������Ŀ�ʼʱ���磬������
        if ( endTime < it->first )
        {
            continue;
        }

        // �����ǽ������
        // �����ǰ����εĿ�ʼʱ���С��ȡҪȡ�Ŀ�ʼʱ��
        recordIndex.start_time_ = it->first;
        if ( it->first < startTime )
        {
            recordIndex.start_time_ = startTime;
        }

        // // �����ǰ����εĽ���ʱ��Ϊ�����ȡҪȡ�Ľ���ʱ��
        recordIndex.end_time_ = it->second;
        if ( (it->second > endTime) || (0 == it->second)  )
        {
            recordIndex.end_time_ = endTime;
        }

        // �ѹ���������������
        recordIndexList.push_back( recordIndex );

    }

    //������ڵ�¼������
    clear_useless_record_index_list( startTime );

    return 0;
}

/*****************************************************************************
 �� �� ��  : clear_useless_record_index_list
 ��������  : ������õ�¼������
 �������  : startTime  :   ¼��ʼʱ��
 �������  : ��
 �� �� ֵ  : ��
 �޸���ʷ  :
*****************************************************************************/
void CSVS_Record_Index_List::clear_useless_record_index_list( time_t startTime )
{
    SVS_TRACE();

    // �ҵ��ȿ�ʼʱ�����������Σ�������
    map< time_t, time_t>::iterator it = record_index_map_.begin();
    for( ; it != record_index_map_.end();  )
    {
        if ( (startTime >= it->second) && ( 0 != it->second) )
        {
           record_index_map_.erase( it++ );
        }
        else
        {
            ++it;
        }

    }
}
//=======================CSVS_Record_Index_List END=============================

//=======================CSVS_Record_Lens BEGIN===========================

// ��Ԫ�������޲ι��캯��
CSVS_Record_Lens::CSVS_Record_Lens()
    : internal_id_(0)
{
    this->constructor();
}

CSVS_Record_Lens::CSVS_Record_Lens(const char * const strDevId, uint32_t nIndex)
    : internal_id_(nIndex)
{
    this->constructor();

    //(void)ACE_OS::memcpy(str_frontdev_id_, strDevId, SVS_DEVICEID_LEN);
    //str_frontdev_id_[SVS_DEVICEID_LEN] = '\0';
    SetLocalizeDeviceId((uint8_t *)str_frontdev_id_,
            sizeof(str_frontdev_id_),
            strDevId);
}

CSVS_Record_Lens::CSVS_Record_Lens(uint32_t nIndex) : internal_id_(nIndex)
{
    this->constructor();
}
CSVS_Record_Lens::~CSVS_Record_Lens()
{
    try
    {
        SVS_LOG((SVS_LM_DEBUG, "CSVS_Record_Lens[%s] ~CSVS_Record_Lens",
                  str_frontdev_id_));
    }
    catch(...){}
}

/*****************************************************************************
 �� �� ��  : constructor
 ��������  : ��ʼ����Ա����
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CSVS_Record_Lens::constructor()
{
    state_ = 0;

    referenc_count_         = 0;
    lens_type_ = LENS_TYPE_BASE;
}

/*****************************************************************************
 �� �� ��  : increase_reference
 ��������  : ���ü�����������
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Record_Lens::increase_reference()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, -1);
    referenc_count_++;

    SVS_LOG((SVS_LM_DEBUG, "CSVS_Record_Lens[%s] increase_reference = %d",
              str_frontdev_id_, referenc_count_));
    return 0;
}

/*****************************************************************************
 �� �� ��  : decrease_reference
 ��������  : ���ü����ݼ�����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Record_Lens::decrease_reference()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, -1);
    referenc_count_--;

    SVS_LOG((SVS_LM_DEBUG, "CSVS_Record_Lens[%s] decrease_reference=%d.",
              str_frontdev_id_, referenc_count_));
    // ���Ӧ�ü���Ϊ0����ʾ�ޱ����ã����߳̿��԰Ѷ����״̬�ó�ֹͣ״̬
    if( 0 == referenc_count_
        && FRONT_DEVICE_STATE_STOP == this->state_)
    {//����ɾ��
        return 1;
    }

    return 0;
}

/*****************************************************************************
 �� �� ��  : get_frontdev_id
 ��������  : ��ȡ��ͷIDָ��
 �������  : NA
 �������  : NA
 �� �� ֵ  : ���ؾ�ͷID����ĵ�ַ
 �޸���ʷ  :
*****************************************************************************/
const char *CSVS_Record_Lens::get_frontdev_id() const
{
    return str_frontdev_id_;
}

/*****************************************************************************
 �� �� ��  : internal_id
 ��������  : ��ȡ��ͷ������ڲ�IDֵ
 �������  : NA
 �������  : NA
 �� �� ֵ  : ���ظþ�ͷ������ڲ�ID
 �޸���ʷ  :
*****************************************************************************/
uint32_t CSVS_Record_Lens::internal_id() const
{
    return this->internal_id_;
}


/*****************************************************************************
 �� �� ��  : get_device_type
 ��������  : ��ȡ�þ�ͷ�����ʱ��¼������:ʵʱ¼���ǰ��¼�񱸷�
 �������  : NA
 �������  : NA
 �� �� ֵ  : ���ظþ�ͷ�����¼������
 �޸���ʷ  :
*****************************************************************************/
uint8_t CSVS_Record_Lens::get_device_type() const
{
    return lens_type_;
}

//========================CSVS_Record_Lens END============================

