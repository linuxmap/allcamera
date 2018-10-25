/*****************************************************************************
   版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文件名          : SVS_Record_Index.h
  版本号          : 1.0
  生成日期        : 2008-12-01
  最近修改        :
  功能描述        : 创建和更新录像索引文件
  函数列表        :
  修改历史        :
  1 日期          : 2008-12-01
    修改内容      : 生成
 *******************************************************************************/
#ifndef _SVS_Record_Index_h
#define _SVS_Record_Index_h
#include <svs_ace_header.h>
#include "svs_rt_record_common.h"
#include <ace/Vector_T.h>
using namespace std;

typedef ACE_Vector <KEYFRAME_INFO> KEYFRAME_VECTOR;

typedef struct _BIG_RECORDFILE_PATH
{
    char BigFilePath[FILEPATHLEN + 1];
}BIG_RECORDFILE_PATH,*PBIG_RECORDFILE_PATH;


typedef ACE_Vector <BIG_RECORDFILE_PATH> BIG_RECORDFILE_PATH_VECTOR;

// 存放录像索引文件最大缓存区空间
#define RECORD_INDEXFILE_BUF_MAX          (128*1024)

#pragma pack(push, 1)

struct Update_Record_Index
{
    int32_t     Type;          //0: 更新结束时间,1: 插入新索引, 2.修改开始时间
    char    LensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1]; //镜头ID.
    time_t  ChangeBeforeStartTime;   //修改之前的开始时间
    time_t  StartTime;  //录像开始时间
    time_t  EndTime;    //录像结束时间
    uint64_t    VideoFlux;
    int32_t     RecordReportType;   // 索引上报的几种类型
    char    RecordFilePath[MAXNAMELEN + 1];       //更新磁盘分区时, 填写空
};

#pragma pack(pop)

enum RECORD_INDEX_OPERATE
{
    RECORD_INDEX_OPERATE_OPEN = 0,       // 打开一个录像文件
    RECORD_INDEX_OPERATE_REOPEN,        // 重新打开一个录像文件
    RECORD_INDEX_OPERATE_CLOSE,          // 关闭录像文件

    RECORD_INDEX_OPERATE_NONE            // 无操作
};

/*
 * CLASS: CAC_Record_Indexfile
 * DESCRIPTION: 录像索引文件实现类
 */
class CSVS_Record_Indexfile
{
public:
    CSVS_Record_Indexfile();
    virtual ~CSVS_Record_Indexfile();

    // 初始化
    bool init(const char * strLensId);

    // 创建一个录像索引文件
    int32_t create_indexfile(time_t dataTime,
                              char* recordFilePath,
                              uint64_t recordFileSize,
                              bool medaDataPartedFlag);

    // 更新关键帧信息
    int32_t update_key_frame(KEYFRAME_VECTOR &keyFrameVec);

    // 把内存中的录像索引内容同步到录像索引文件中
    int32_t update_indexfile(time_t endTime);

    int32_t close();

    // 获取录像文件的大小
    uint64_t get_record_file_size()const;

    // 设置录像文件的大小
    void set_record_file_size(uint64_t file_size);


protected:
    // 解析当前时间片的录像索引文件
    int32_t parse_indexfile(bool & isNewBigFile, char * recordFilePath);

    // 创建目录
    int32_t make_dir(time_t fileTime);

    // 切换时间片的时候，清理相关内容
    void reset();

protected:

    // 索引文件内容缓存
    char record_index_buffer_[RECORD_INDEXFILE_BUF_MAX];

    // 索引文件句柄
    ACE_FILE_IO indexfile_;

    // 文件打开标识
    bool indexFileOpenFlag_;

    // 索引文件路径
    char str_file_path_[MAXNAMLEN + 1];

    // 镜头ID
    char str_lens_id_[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    // 当前索引文件中最后一个大文件信息的在索引文件中的偏移
    uint32_t curr_big_file_cursor_;

    // 当前索引文件中最后一个段信息在索引文件中的偏移
    uint32_t curr_sec_cursor_;

    // 当前索引文件总大小
    uint32_t indexfile_size_;

    // 当前簇的关键帧信息
    KEYFRAME_VECTOR keyframe_vec_;

    // 当前簇在大文件的偏移
    uint64_t cluster_offset_;

    // 当前簇信息是否为空标志
    bool cluster_null_flag;

    // 当前镞起始时间戳
    uint32_t cluster_start_timetick_;
   // 增加索引文件对应的当前录像文件的大小
   uint64_t       m_RecordFileSize;
// 为单元测试增加友元类
#ifdef UNITTEST
    friend class CSVS_Record_Indexfile_Test;
#endif
};

// 根据输入的录像索引文件，解析出文件中全部的大录像文件路径
int32_t parse_all_big_recordfile_path(const char* indexfilePath, BIG_RECORDFILE_PATH_VECTOR& bigRecordfilePathVec);
#endif //_SVS_Record_Index_h


