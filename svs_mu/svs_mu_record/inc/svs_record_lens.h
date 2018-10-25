#ifndef _SVS_Record_Lens_h
#define _SVS_Record_Lens_h
#include "svs_device_state.h"
#include "svs_record_disk.h"
//预先分配录像镜头对象内存, 2006-7-21
#define RECORD_LENS_CACHE
struct SVS_Record_Index
{
    time_t start_time_; //开始时间
    time_t end_time_;   //结束时间
};
// 录像索引管理类
class CSVS_Record_Index_List
{
public:
    CSVS_Record_Index_List()
    {};
    UTAPI ~CSVS_Record_Index_List()
    {}

    //获取指定时间段间的录像索引列表
    UTAPI int32_t get_record_index_list( time_t startTime,
                               time_t endTime,
                               list<SVS_Record_Index> &recordIndexList);

    //添加一个录像索引
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

    //更新录像索引的结束时间
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
    //清空无用的录像索引
    UTAPI void clear_useless_record_index_list( time_t startTime);
private:

    //录像索引列表
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
 * @brief 录像镜头抽象基类
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
    * DESCRIPTION: 递增应用计数
    * INPUT:
    *   NONE
    * OUTPUT:
    *   成功返回０，否则返回-1.
    */

    UTAPI int32_t increase_reference();
    /*
    * FUNCTION: int32_t decrease_reference()
    * DESCRIPTION: 递减引用计数，如果递减后，引用计数等于０，且路由已经停止，那么可以删除次对象
    * INPUT:
    *   NONE
    * OUTPUT:
    *   递减后，如果引用计数等于０，且路由已经停止，那么返回１，否则返回０。
    *   如果发生异常(加锁失败等)返回-1。
    */
    UTAPI int32_t decrease_reference();

    UTAPI const char *get_frontdev_id() const ;

    UTAPI uint32_t internal_id() const ;

    virtual uint8_t get_device_type() const;

protected:
    virtual void constructor();

protected:

    //内部ID，用于定时器
    const uint32_t internal_id_;
    enum REAL_TIME_RECORDING_CONST
    {
        RECORD_CHECK_INTERVAL = 300,   //10分钟
    };

    enum RECORD_DEVICE_STATE
    {
        FRONT_DEVICE_STATE_STOP = 0x20,
    };

    //引用计数器，只有等于0时，才可以释放本对象
    volatile int32_t referenc_count_;

    //前端设备ID
    char str_frontdev_id_[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    volatile uint8_t state_;

    ACE_Recursive_Thread_Mutex mutex_;

    uint8_t  lens_type_;
};
#endif //_SVS_Record_Lens_h


