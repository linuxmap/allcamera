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
 函 数 名  : get_record_index_list
 功能描述  : 获取指定时间段间的录像索引列表
 输入参数  : startTime  :   录像开始时间
             endTime    :   录像结束时间
 输出参数  : recordIndexList : 录像索引列表
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
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

        // 如果当前这个索引段的结束时间不为0(为0，表示无穷大)
        // 且要获取的开始时间大于索引段结束时间，则跳过
        if ( (startTime > it->second) && ( 0 != it->second) )
        {
            continue;
        }

        // 如果要获取的结束时间比该段索引的开始时间早，则跳过
        if ( endTime < it->first )
        {
            continue;
        }

        // 下面是交叠情况
        // 如果当前这个段的开始时间较小，取要取的开始时间
        recordIndex.start_time_ = it->first;
        if ( it->first < startTime )
        {
            recordIndex.start_time_ = startTime;
        }

        // // 如果当前这个段的结束时间为无穷大，取要取的结束时间
        recordIndex.end_time_ = it->second;
        if ( (it->second > endTime) || (0 == it->second)  )
        {
            recordIndex.end_time_ = endTime;
        }

        // 把构造后的索引块入列
        recordIndexList.push_back( recordIndex );

    }

    //清除过期的录像索引
    clear_useless_record_index_list( startTime );

    return 0;
}

/*****************************************************************************
 函 数 名  : clear_useless_record_index_list
 功能描述  : 清空无用的录像索引
 输入参数  : startTime  :   录像开始时间
 输出参数  : 无
 返 回 值  : 无
 修改历史  :
*****************************************************************************/
void CSVS_Record_Index_List::clear_useless_record_index_list( time_t startTime )
{
    SVS_TRACE();

    // 找到比开始时间更早的索引段，并擦除
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

// 单元测试用无参构造函数
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
 函 数 名  : constructor
 功能描述  : 初始化成员变量
 输入参数  : NA
 输出参数  : NA
 返 回 值  : NA
 修改历史  :
*****************************************************************************/
void CSVS_Record_Lens::constructor()
{
    state_ = 0;

    referenc_count_         = 0;
    lens_type_ = LENS_TYPE_BASE;
}

/*****************************************************************************
 函 数 名  : increase_reference
 功能描述  : 引用计数递增函数
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
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
 函 数 名  : decrease_reference
 功能描述  : 引用计数递减函数
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CSVS_Record_Lens::decrease_reference()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, -1);
    referenc_count_--;

    SVS_LOG((SVS_LM_DEBUG, "CSVS_Record_Lens[%s] decrease_reference=%d.",
              str_frontdev_id_, referenc_count_));
    // 如果应用计数为0，表示无别处在用，该线程可以把对象的状态置成停止状态
    if( 0 == referenc_count_
        && FRONT_DEVICE_STATE_STOP == this->state_)
    {//可以删除
        return 1;
    }

    return 0;
}

/*****************************************************************************
 函 数 名  : get_frontdev_id
 功能描述  : 获取镜头ID指针
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 返回镜头ID缓存的地址
 修改历史  :
*****************************************************************************/
const char *CSVS_Record_Lens::get_frontdev_id() const
{
    return str_frontdev_id_;
}

/*****************************************************************************
 函 数 名  : internal_id
 功能描述  : 获取镜头对象的内部ID值
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 返回该镜头对象的内部ID
 修改历史  :
*****************************************************************************/
uint32_t CSVS_Record_Lens::internal_id() const
{
    return this->internal_id_;
}


/*****************************************************************************
 函 数 名  : get_device_type
 功能描述  : 获取该镜头对象的时间录像类型:实时录像或前端录像备份
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 返回该镜头对象的录像类型
 修改历史  :
*****************************************************************************/
uint8_t CSVS_Record_Lens::get_device_type() const
{
    return lens_type_;
}

//========================CSVS_Record_Lens END============================

