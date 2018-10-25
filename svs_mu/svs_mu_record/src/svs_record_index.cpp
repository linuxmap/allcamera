
/*****************************************************************************
   版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文件名          : SVS_Record_Index.cpp
  版本号          : 1.0
  生成日期        : 2008-12-01
  最近修改        :
  功能描述        : 创建和更新录像索引文件
  函数列表        :
  修改历史        :
  1 日期          : 2008-12-01
    修改内容      : 生成
 *******************************************************************************/
#include <fcntl.h>
#include "md5.h"
#include "svs_stat_manager.h"
#include "svs_rt_record_common.h"
#include "svs_record_index.h"
#include "svs_real_record_server.h"
#include "svs_file_lock.h"

//========================PUBLIC FUNCTION BEGIN=================================
/*****************************************************************************
 函 数 名  : md5_encode
 功能描述  : 对指定长度的字串进行md5加密
 输入参数  : uint8_t* strmd5Src:待加密的字串
             uint32_t md5SrcLen:待加密的字串长度
             uint8_t* md5Result:加密结果
 输出参数  : NA
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t md5_encode(uint8_t* strMd5Src, const uint32_t md5SrcLen, uint8_t* md5Result)
{
    SVS_TRACE();
    if ((NULL == strMd5Src)
        || (NULL == md5Result)
        || (0 == md5SrcLen))
    {
        SVS_LOG((SVS_LM_ERROR,
                   "Fail to encode md5 as the input pointer is invalid. "
                   "strMd5Src[0x%08x], md5Result[0x%08x], md5SrcLen[%u].",
                   strMd5Src, md5Result, md5SrcLen));
        return SVS_RESULT_FAILURE;
    }

    (void)ACE_OS::memset(md5Result, 0, sizeof(md5Result));
    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);//lint !e1055 !e534 !e746 for declare in md5.h
    MD5_Update(&md5Ctx, (uint8_t*)strMd5Src, md5SrcLen);//lint !e1055 !e746 !e534 for declare in md5.h
    MD5_Final(md5Result, &md5Ctx);//lint !e1055 !e534 !e746 for declare in md5.h

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : md5_decode
 功能描述  : 对指定长度的字串进行再次加密，和输入的摘要比较是否相等
 输入参数  : uint8_t* strmd5Src:待解密密的字串
             uint32_t md5SrcLen:待解密的字串长度
             uint8_t* md5Result:文件摘要
 输出参数  : NA
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t md5_decode(uint8_t*       strMd5Src,
               const uint32_t  md5SrcLen,
               const uint8_t* md5Resume)
{
    SVS_TRACE();
    if ((NULL == strMd5Src)
        || (NULL == md5Resume)
        || (0 == md5SrcLen))
    {
        SVS_LOG((SVS_LM_ERROR,
                   "Fail to decode md5 as the input is invalid. "
                   "strMd5Src[0x%08x], md5Resume[0x%08x], md5SrcLen[%u].",
                   strMd5Src, md5Resume, md5SrcLen));
        return SVS_RESULT_FAILURE;
    }

    uint8_t md5Result[MD5CODELEN] = {0,};
    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);//lint !e1055 !e534 for declare in md5.h
    MD5_Update(&md5Ctx, strMd5Src, md5SrcLen);//lint !e1055 !e534 for declare in md5.h
    MD5_Final(md5Result, &md5Ctx);//lint !e1055 !e534 for declare in md5.h

    if (0 == strncmp((char*)md5Result, (char*)md5Resume, MD5CODELEN))
    {
        return SVS_RESULT_OK;
    }

    return SVS_RESULT_FAILURE;
}

/*****************************************************************************
 函 数 名  : lock_file
 功能描述  : 给指定的文件句柄加锁
 输入参数  : int32_t fileHandle:要加锁的文件句柄
 输出参数  : NA
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t lock_file(int32_t fileHandle)
{
    SVS_TRACE();
    if (0 > fileHandle)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to lock file as file handle is invalid. file handle[%d].",
            fileHandle));
        return SVS_RESULT_FAILURE;
    }

    // 查看fileHandle是否可以加写锁
    int32_t ret = inquire_lock(fileHandle, O_WRONLY, 0, SEEK_SET, 0);
    if (0 > ret)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Fail to get file write lock as file handle has been locked. file handle[%d]",
            fileHandle));
        return SVS_RESULT_FAILURE;
    }

    // 给文件加上写锁
    ret = write_lock(fileHandle, 0, SEEK_SET, 0);
    if (0 > ret)
    {
        SVS_LOG((SVS_LM_WARNING, "Fail to add write lock to file. file handle[%d].",fileHandle));
        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : unlock_file
 功能描述  : 取出文件的写锁
 输入参数  : int32_t fileHandle
 输出参数  : NA
 返 回 值  : 成功:0
             失败:-1
 修改历史  :
*****************************************************************************/
int32_t unlock_file(int32_t fileHandle)
{
    return un_lock(fileHandle, 0, SEEK_SET, 0);
}

//========================PUBLIC FUNCTION END===================================


//========================CSVS_Record_Indexfile BEGIN==========================
CSVS_Record_Indexfile::CSVS_Record_Indexfile()
{
    (void)ACE_OS::memset(record_index_buffer_, 0, sizeof(record_index_buffer_));
    (void)ACE_OS::memset(str_file_path_, 0, sizeof(str_file_path_));
    (void)ACE_OS::memset(str_lens_id_, 0, sizeof(str_lens_id_));
    curr_big_file_cursor_   = 0;
    cluster_offset_         = 0;
    cluster_null_flag       = true;
    cluster_start_timetick_ = 0;
    curr_sec_cursor_        = 0;
    indexfile_size_         = 0;

    m_RecordFileSize        = 0;

    indexFileOpenFlag_ = false;
}

CSVS_Record_Indexfile::~CSVS_Record_Indexfile()
{
    try
    {
        (void)indexfile_.close();
        indexFileOpenFlag_ = false;
    } catch (...) {}
}

/*****************************************************************************
 函 数 名  : init
 功能描述  : 初始化
 输入参数  : const char* strLensId:镜头ID
 输出参数  : 无
 返 回 值  : 成功:true
             失败:false
 修改历史  :
*****************************************************************************/
bool CSVS_Record_Indexfile::init(const char * strLensId)
{
    SVS_TRACE();

    SetLocalizeDeviceId((uint8_t *)str_lens_id_, sizeof(str_lens_id_), strLensId);

    return true;
}

/*****************************************************************************
 函 数 名  : reset
 功能描述  : 将所有成员变量复位
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 无
 修改历史  :
*****************************************************************************/
void CSVS_Record_Indexfile::reset()
{
    SVS_TRACE();
    (void)ACE_OS::memset(record_index_buffer_, 0, sizeof(record_index_buffer_));
    (void)ACE_OS::memset(str_file_path_, 0, sizeof(str_file_path_));

    (void)indexfile_.close();

    curr_big_file_cursor_   = 0;
    indexfile_size_         = 0;
    cluster_offset_         = 0;
    cluster_null_flag       = true;
    cluster_start_timetick_ = 0;
    m_RecordFileSize        = 0;

    keyframe_vec_.clear();

    indexFileOpenFlag_ = false;

    return;
}

int32_t CSVS_Record_Indexfile::close()
{
    reset();
    return SVS_RESULT_OK;
}

uint64_t CSVS_Record_Indexfile::get_record_file_size()const
{
    return m_RecordFileSize;
}

void CSVS_Record_Indexfile::set_record_file_size(uint64_t file_size)
{
    m_RecordFileSize = file_size;
}


/*****************************************************************************
 函 数 名  : create_indexfile
 功能描述  : 创建录像索引文件
 输入参数  : time_t dateTime:根据该时间创建录像文件
             char* recordFilePath:录像文件存放路径
             uint64_t recordFileSize:录像文件大小
 输出参数  : 无
 返 回 值  : 成功:0   失败:-1
 修改历史  :
*****************************************************************************/
int32_t CSVS_Record_Indexfile::create_indexfile(time_t   dataTime,
                                            char*    recordFilePath,
                                            uint64_t recordFileSize,
                                            bool medaDataPartedFlag)
{
    if (NULL == recordFilePath)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to create index file, because the pointer recordFilePath is null."
            " camera[%s].",
            str_lens_id_));
        return SVS_RESULT_FAILURE;
    }

    // 复位
    reset();

    // 将需要创建索引文件的时间dataTime转换为整点时间，单位:秒
    time_t fileTime = dataTime - (dataTime % BIG_RECORDFILE_TIME_SLICE);

    // 根据索引文件对应的整点时间，来创建目录，如果该镜头没有对应的目录，还会创建镜头目录
    if (SVS_RESULT_OK != make_dir(fileTime))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to create directory for index file. camera[%s].",
            str_lens_id_));
        return SVS_RESULT_FAILURE;
    }

    // 根据日期，生成索引文件名称，保存在str_file_path_中
    uint32_t pathLen = ACE_OS::strlen(str_file_path_);
    (void)SVS_SS_UTILITIES::time2bigfilename(str_file_path_ + pathLen,
                                             (int32_t)(sizeof(str_file_path_) - pathLen),
                                             fileTime,
                                             "idx");

    // 3.判断录像索引文件是否已经存在
    bool isNewFile = true;
    struct stat fd;
    if (0 == ::stat(str_file_path_, &fd))
    {
        indexfile_size_ = (uint32_t)fd.st_size;
    }
    else
    {
        indexfile_size_ = 0;
    }

    // 如果索引文件已经存在,就要看缓冲区的大小是否够用
    if (indexfile_size_ > 0)
    {
        isNewFile = false;

        // 如果缓冲区太小
        if (RECORD_INDEXFILE_BUF_MAX < (indexfile_size_ + sizeof(SVS_BIGFILE_INFO) + sizeof(SVS_INDEXFILE_HEADER)))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Fail to create index file, because old file size is larger than buffer size. "
                "camera[%s], indexFile[%s], old file size[%u], buffer size[%d].",
                str_lens_id_,
                str_file_path_,
                indexfile_size_,
                RECORD_INDEXFILE_BUF_MAX));
            return SVS_RESULT_FAILURE;
        }
    }
    else
    {
        SVS_LOG((SVS_LM_INFO,
            "Need to create a new index file.cameraId[%s]",
            str_lens_id_));
    }

    SVS_LOG((SVS_LM_INFO,
        "Camera needs to create new index file flag is %d. "
        "camera[%s], current index file size[%u].",
        (int32_t)isNewFile,
        str_lens_id_,
        indexfile_size_));

    // 创建录像索引文件
    int32_t flags = O_CREAT | O_RDWR | O_LARGEFILE;
    ACE_FILE_Connector connector;
    int32_t connResult = connector.connect( indexfile_,
                                        ACE_FILE_Addr(str_file_path_),
                                        0,
                                        ACE_Addr::sap_any,
                                        0,
                                        flags,
                                        ACE_DEFAULT_FILE_PERMS );

    if (0 != connResult)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to create index file. camera[%s], IndexFile[%s].",
            str_lens_id_,
            str_file_path_));
        return SVS_RESULT_FAILURE;
    }
    // 事件录像时太多日志，直接置0
    ACE_OS::last_error(0);

    SVS_LOG((SVS_LM_INFO,
        "Success to create index file. camera[%s], IndexFile[%s].",
        str_lens_id_,
        str_file_path_));

    char * pos = record_index_buffer_;
    SVS_INDEXFILE_HEADER* pIndexHeader;
    SVS_BIGFILE_INFO* pBigFileInfo;
    uint32_t increaseLen = 0;
    if (isNewFile)
    {
        // 如果是新建的录像索引文件
        increaseLen = sizeof(SVS_BIGFILE_INFO) - sizeof(SVS_BIGFILECLUSTER_INFO);

        // 填充录像头信息
        pIndexHeader = (SVS_INDEXFILE_HEADER*)pos;
        (void)ACE_OS::memcpy(pIndexHeader, SVS_INDEXFILE_HEADER_STR, sizeof(SVS_INDEXFILE_HEADER_STR));
        pos += sizeof(SVS_INDEXFILE_HEADER);
        pIndexHeader->IndexFileTotalLen = sizeof(SVS_INDEXFILE_HEADER);

        // 填写第一个大文件信息
        pBigFileInfo = (SVS_BIGFILE_INFO*)pos;
        pBigFileInfo->StartTime = dataTime;
        pBigFileInfo->EndTime = dataTime;
        (void)ACE_OS::memcpy(pBigFileInfo->FilePath, recordFilePath, strlen(recordFilePath));
        pBigFileInfo->FileInfoTotalLen = 0;

        curr_big_file_cursor_ = (uint32_t)((char*)pBigFileInfo - record_index_buffer_);
    }
    else
    {
        // 如果索引文件已经存在，解析索引文件，
        bool isNewBigFile = true;
        if (SVS_RESULT_OK != parse_indexfile(isNewBigFile, recordFilePath))
        {
            SVS_LOG((SVS_LM_ERROR, "Fail to parse current index file. camera[%s], current index file[%s].",
                str_lens_id_,
                str_file_path_));
            return SVS_RESULT_FAILURE;
        }

        // 索引头信息
        pos = record_index_buffer_;
        pIndexHeader = (SVS_INDEXFILE_HEADER*)((void *)pos);

        // 大文件信息
        pos = record_index_buffer_ + curr_big_file_cursor_;
        pBigFileInfo = (SVS_BIGFILE_INFO*)((void *)pos);

        // 如果与上次最后一个大录像文件路径不同，添加一个大文件信息
        if (isNewBigFile)
        {
            increaseLen = sizeof(SVS_BIGFILE_INFO) - sizeof(SVS_BIGFILECLUSTER_INFO);

            // 大文件信息
            pBigFileInfo->StartTime = dataTime;
            pBigFileInfo->EndTime = dataTime;
            (void)ACE_OS::memcpy(pBigFileInfo->FilePath, recordFilePath, strlen(recordFilePath));
        }
        else
        {
            increaseLen = sizeof(SVS_BIGFILE_SECTION_INFO) - sizeof(SVS_BIGFILECLUSTER_INFO);
        }
    }

    // 更新长度信息
    pIndexHeader->IndexFileTotalLen += increaseLen;
    pBigFileInfo->FileInfoTotalLen += increaseLen;

    pBigFileInfo->MetaDataPartedFlag = SVS_META_DATA_PARTED;
    if (!medaDataPartedFlag)
    {
        pBigFileInfo->MetaDataPartedFlag = SVS_META_DATA_DISPARTED;
    }

    uint32_t sectionInfoLen = sizeof(SVS_BIGFILE_SECTION_INFO) - sizeof(SVS_BIGFILECLUSTER_INFO);

    indexfile_size_  = (uint32_t)pIndexHeader->IndexFileTotalLen;
    curr_sec_cursor_ = indexfile_size_ - sectionInfoLen;

    // 填写该段信息,无论怎样都需要创建新段
    SVS_BIGFILE_SECTION_INFO* pBigFileSecInfo =
        (SVS_BIGFILE_SECTION_INFO*)((void *)(record_index_buffer_ + curr_sec_cursor_));
    pBigFileSecInfo->StartTime = dataTime;
    pBigFileSecInfo->EndTime = dataTime;
    pBigFileSecInfo->FileOff = recordFileSize;
    pBigFileSecInfo->SectionTotalLen = sectionInfoLen;

    // 更新索引文件打开标识
    indexFileOpenFlag_ = true;

    SVS_LOG((SVS_LM_INFO, "Success to create index file. Indexfile[%s], camera[%s].",
        str_file_path_,
        str_lens_id_));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : make_dir
 功能描述  : 创建索引文件所在的目录
 输入参数  : time_t fileTime 索引文件目录的名称,转换成形如2009033011,标识09年3月30号11点对应的目录
 输出参数  : 无
 返 回 值  : 成功:0  失败:-1
 修改历史  :
*****************************************************************************/
int32_t CSVS_Record_Indexfile::make_dir(time_t fileTime)
{
    SVS_TRACE();

    // 1.创建镜头级目录
    (void)ACE_OS::memset(str_file_path_, 0, sizeof(str_file_path_));
    int32_t printfResult = ACE_OS::snprintf(str_file_path_,
                                         sizeof(str_file_path_),
                                         "%s/%s/",
                                         CAC_RT_Record_Server::instance()->get_record_indexfile_path(),
                                         str_lens_id_);

    if (0 > printfResult)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to create camera level directory as buffer for filepath is over. "
            "Camera[%s], return value[%d].",
            str_lens_id_,
            printfResult));
        return SVS_RESULT_FAILURE;
    }

    int32_t iRet = ACE_OS::mkdir(str_file_path_);
    if ((0 != iRet) && (EEXIST != ACE_OS::last_error()))
    {
        // 如果是创建镜头目录失败，并且失败原因不是因为目录已经存在，则为错误情况
        SVS_LOG((SVS_LM_ERROR,
            "Fail to create camera level directory. directory[%s], camera[%s].",
            str_file_path_,
            str_lens_id_));
        return SVS_RESULT_FAILURE;
    }

    // 2.创建日期级目录
    char strDayTime[TIME_STRING_LEN] = {0,};
    (void)SVS_SS_UTILITIES::time2str(strDayTime, sizeof(strDayTime), fileTime);
    (void)ACE_OS::memset(strDayTime + RT_DATE_STR_LEN, 0, sizeof(strDayTime) - RT_DATE_STR_LEN);
    (void)ACE_OS::strncat(str_file_path_, strDayTime, RT_DATE_STR_LEN);
    (void)ACE_OS::strcat(str_file_path_, "/");
    iRet = ACE_OS::mkdir(str_file_path_);
    if ((0 != iRet)
        && (EEXIST != ACE_OS::last_error()))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to create date level directory. dircectory[%s], camera[%s].",
            str_file_path_,
            str_lens_id_));
        return SVS_RESULT_FAILURE;
    }
    // 事件录像时太多日志，直接置0
    ACE_OS::last_error(0);

    SVS_LOG((SVS_LM_INFO,
        "Success to create the record index directory. "
        "directory[%s], camera[%s].",
        str_file_path_,
        str_lens_id_));
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : update_indexfile
 功能描述  : 把内存中的录像索引内容同步到录像索引文件中
 输入参数  : time_t endTime
             lastFrameTime:把内存中的录像索引信息刷新到录像索引文件中，每30s刷新
             一次
 输出参数  : 无
 返 回 值  : 成功:0
             其他:-1
 修改历史  :
*****************************************************************************/
int32_t CSVS_Record_Indexfile::update_indexfile(time_t endTime)
{
    // 如果当前没有簇信息，则直接返回
    SVS_LOG((SVS_LM_DEBUG,
        "Update index file. camera[%s], key frame num[%u], endTime[%u].",
        str_lens_id_,
        keyframe_vec_.size(),
        endTime));

    // 当前簇信息为空或入参非法
    if (cluster_null_flag || (endTime == 0))
    {
        return SVS_RESULT_OK;
    }

    // 如果索引没有打开，不允许更新索引
    if(!indexFileOpenFlag_)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to update index file as Has not open index file. camera[%s].",
            str_lens_id_));
        return SVS_RESULT_FAILURE;
    }

    uint8_t keyFrameCount   = (uint8_t)keyframe_vec_.size();
    uint32_t keyClusterSize = sizeof(SVS_BIGFILECLUSTER_INFO)
                              + ((keyFrameCount - 1) * sizeof(KEYFRAME_INFO));
    if (RECORD_INDEXFILE_BUF_MAX < (indexfile_size_ + keyClusterSize))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Fail to update index file as buffer is overflow. camera[%s].",
            str_lens_id_));
        return SVS_RESULT_FAILURE;
    }
    /*lint -e826*/   //消息体含变长字段,该PC-LINT告警不需要修改
    char* pos = NULL;

    // 1.更新录像索引文件大小信息
    pos = record_index_buffer_;
    SVS_INDEXFILE_HEADER* pIndexfileHeader = (SVS_INDEXFILE_HEADER*)(void*)pos;
    pIndexfileHeader->IndexFileTotalLen  = indexfile_size_;
    pIndexfileHeader->IndexFileTotalLen += keyClusterSize;

    // 2.更新大文件信息，长度和结束时间
    pos = record_index_buffer_ + curr_big_file_cursor_;
    SVS_BIGFILE_INFO* pBigfileInfo = (SVS_BIGFILE_INFO*)(void*)pos;
    pBigfileInfo->FileInfoTotalLen += keyClusterSize;
    pBigfileInfo->EndTime = endTime;

    // 3.更新段信息，长度和结束时间
    pos = record_index_buffer_ + curr_sec_cursor_;
    SVS_BIGFILE_SECTION_INFO* pSecInfo = (SVS_BIGFILE_SECTION_INFO*)(void*)pos;
    pSecInfo->SectionTotalLen += keyClusterSize;
    pSecInfo->EndTime = endTime;

    // 4.增加一个新簇，长度，是否包含关键帧，
    pos = record_index_buffer_ + indexfile_size_;
    SVS_BIGFILECLUSTER_INFO* pClusterInfo = (SVS_BIGFILECLUSTER_INFO*)(void*)pos;//lint !e826 just cast
    pClusterInfo->ClusterTotalLen = keyClusterSize;
    /*lint +e826*/   //消息体含变长字段,该PC-LINT告警不需要修改
    // 该簇是否有关键帧，0:没有，1:有
    pClusterInfo->KeyFrameFlag = 0;
    if (0 < keyFrameCount)
    {
        pClusterInfo->KeyFrameFlag = 1;
    }

    pClusterInfo->FirstTimeTick = cluster_start_timetick_;
    pClusterInfo->ClusterFileOffset = cluster_offset_;
    pos = (char*)pClusterInfo->KeyFrameInfo;
    for (uint8_t i = 0; i < keyFrameCount; i++)
    {
        (void)ACE_OS::memcpy(pos, (char*)&keyframe_vec_[i], sizeof(KEYFRAME_INFO));
        pos += sizeof(KEYFRAME_INFO);
    }

    // 5.更新索引文件总大小
    indexfile_size_ += keyClusterSize;

    // 6.生成md5
    uint8_t md5Temp[MD5CODELEN];
    uint32_t md5SrcLen = indexfile_size_
                              - (SVS_RECORD_FILE_MAGIC_DATE_LEN + MD5CODELEN);
    pos = record_index_buffer_ + (SVS_RECORD_FILE_MAGIC_DATE_LEN + MD5CODELEN);

    // 如果md5 失败，则不写索引文件
    if (SVS_RESULT_OK != md5_encode((uint8_t*)pos, md5SrcLen, md5Temp))
    {
        SVS_LOG((SVS_LM_WARNING, "Failt to encode index file with md5. Camera[%s].",
            str_lens_id_));
    }
    else
    {
        (void)ACE_OS::memcpy((char*)pIndexfileHeader->Md5code, (char*)md5Temp, MD5CODELEN);

        // 7.把内容写入索引文件
        ACE_HANDLE fileHandle = indexfile_.get_handle();
        (void)ACE_OS::lseek(fileHandle, 0, SEEK_SET);

        // 如果获取文件锁失败
        if (SVS_RESULT_OK != lock_file(fileHandle))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Fail to get index file write lock. camera[%s].",
                str_lens_id_));
        }
        else
        {
            int32_t writeRet = ACE_OS::write(fileHandle, record_index_buffer_, indexfile_size_);
            (void)unlock_file(fileHandle);
            if (writeRet != (int32_t)indexfile_size_)
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Fail to write index file. camera[%s], indexfile size[%u].",
                    str_lens_id_,
                    indexfile_size_));
            }
        }
    }

    // 8.清理簇缓存信息
    cluster_offset_   = 0;
    cluster_null_flag = true;
    cluster_start_timetick_ = 0;
    keyframe_vec_.clear();

    SVS_LOG((SVS_LM_INFO, "Success to update index file. camera[%s].", str_lens_id_));
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : update_key_frame
 功能描述  : 把新写到录像文件的mb中所包含的关键帧信息，添加到当前簇信息中
 输入参数  : KEYFRAME_VECTOR & keyFrameVec:mb中包含的全部关键帧信息,第一个成员是
             表示该此写的媒体数据中第一个帧在大录像文件中的偏移
 输出参数  : 无
 返 回 值  : 成功:0
             其他:-1
 修改历史  :
*****************************************************************************/
int32_t CSVS_Record_Indexfile::update_key_frame(KEYFRAME_VECTOR & keyFrameVec)
{
    SVS_TRACE();

    uint8_t keyFrameSize = (uint8_t)keyFrameVec.size();


    // 如果当前簇信息为空，则表示第一次更新关键帧信息
    if (cluster_null_flag)
    {
        cluster_offset_ = keyFrameVec[0].FileOffset;
        cluster_start_timetick_ = keyFrameVec[0].TimeTick;
        cluster_null_flag = false; // 更新簇标识
        SVS_LOG((SVS_LM_DEBUG,
            "Update cluster offset. camera[%s], offset[%Q], start timetick[%u].",
            str_lens_id_,
            cluster_offset_,
            cluster_start_timetick_));
    }


    for (uint8_t i = 1; i < keyFrameSize; i++)
    {
        keyframe_vec_.push_back(keyFrameVec[i]);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : parse_indexfile
 功能描述  : 解析录像索引文件，找出最后一个大文件的位置，如果当前需要创建一个大
             文件信息，则大文件的位置当前录像索引文件的最后
 输入参数  :
 输出参数  : 无
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t CSVS_Record_Indexfile::parse_indexfile(bool &isNewBigFile, char* recordFilePath)
{
    SVS_TRACE();

    ACE_HANDLE fileHandle = indexfile_.get_handle();

    // 读取现有录像索引文件的内容
    (void)ACE_OS::lseek(fileHandle, 0, SEEK_SET );
    int32_t readRet = ACE_OS::read (fileHandle, record_index_buffer_, indexfile_size_);
    if ((int32_t)indexfile_size_ != readRet)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to read index file. camera[%s], Indexfile[%s], errno[%d].",
            str_lens_id_,
            str_file_path_,
            errno));
        return SVS_RESULT_FAILURE;
    }

    // md5校验
    char* pos = record_index_buffer_;
    SVS_INDEXFILE_HEADER* pIndexHeader = (SVS_INDEXFILE_HEADER*)(void*)pos;

    uint32_t md5SrcLen = indexfile_size_
                              - (SVS_RECORD_FILE_MAGIC_DATE_LEN + MD5CODELEN);
    pos = record_index_buffer_ + (SVS_RECORD_FILE_MAGIC_DATE_LEN + MD5CODELEN);

    if (SVS_RESULT_OK != md5_decode((uint8_t*)pos, md5SrcLen, pIndexHeader->Md5code))
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to decode index file and begin to unlink the invalid index file. "
            "camera[%s], indexfile[%s].",
            str_lens_id_,
            str_file_path_));

        // 索引文件已经被修改，则删除之
        (void)indexfile_.close();
        (void)unlink(str_file_path_);
        return SVS_RESULT_FAILURE;
    }

    // 查找最后一个大录像文件
    SVS_BIGFILE_INFO* pBigFileInfo = NULL;
    pos = record_index_buffer_ + sizeof(SVS_INDEXFILE_HEADER);
    int32_t fileSize = (int32_t)(indexfile_size_ - sizeof(SVS_INDEXFILE_HEADER));
    while (fileSize > 0)
    {
        pBigFileInfo = (SVS_BIGFILE_INFO*)((void *)pos);
        fileSize -= (int32_t)pBigFileInfo->FileInfoTotalLen;
        pos += pBigFileInfo->FileInfoTotalLen;
    }

    if (NULL == pBigFileInfo)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to parse index file, because index file is too small. "
            "camera[%s], indexfile[%s], indexfile size[%u].",
            str_lens_id_,
            str_file_path_,
            indexfile_size_));
        return SVS_RESULT_FAILURE;
    }

    // 比较最后一个大文件路径信息,看是否需要创建一个新的媒体文件
    if (0 == strncmp((char*)pBigFileInfo->FilePath, recordFilePath, FILEPATHLEN))
    {
        isNewBigFile = false;
        SVS_LOG((SVS_LM_DEBUG,
            "Camera does not need to create new big file info. camera[%s], old big file[%s].",
            str_lens_id_,
            recordFilePath));
        curr_big_file_cursor_ = (uint32_t)((char*)pBigFileInfo - record_index_buffer_);
    }
    else
    {
        SVS_LOG((SVS_LM_DEBUG,
            "Camera needs to create a new big file. camera[%s], old big file[%s], last big file[%s].",
            str_lens_id_,
            recordFilePath,
            pBigFileInfo->FilePath));
        isNewBigFile = true;
        curr_big_file_cursor_ = indexfile_size_;
    }

    return SVS_RESULT_OK;
}

//========================CSVS_Record_Indexfile END============================

/*****************************************************************************
 函 数 名  : parse_indexfile
 功能描述  : 根据输入的录像索引文件，解析出文件中全部的大录像文件路径。录像删除
             部分使用该功能函数。
 输入参数  : char * indexfilePath:录像索引文件全路径
 输出参数  : BIG_RECORDFILE_PATH_VECTOR & bigRecordfilePathVec:大录像文件路径列表
             ，具有成员唯一性
 返 回 值  : 成功,返回 0 否则返回-1
 修改历史  :
*****************************************************************************/
int32_t parse_all_big_recordfile_path(const char * indexfilePath,
                         BIG_RECORDFILE_PATH_VECTOR & bigRecordfilePathVec)
{
    SVS_TRACE();
    if (NULL == indexfilePath)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to parse all big file path as the pointer indexfilePath is NULL."));
        return SVS_RESULT_FAILURE;
    }

    bigRecordfilePathVec.clear();

    struct stat fd;
    if (0 != ::stat(indexfilePath, &fd))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Fail to parse all big file path. Indexfile[%s].",
            indexfilePath));
        return SVS_RESULT_FAILURE;
    }

    if ((uint32_t)fd.st_size < sizeof(SVS_INDEXFILE_HEADER))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to parse all big file path as index file size is too small. "
            "Indexfile[%s], Indexfile size[%d].",
            indexfilePath,
            fd.st_size));
        return SVS_RESULT_FAILURE;
    }

    char* indexfileBuffer;
    (void)SVS_NEW(indexfileBuffer, (uint32_t)(fd.st_size + 1));
    if (NULL == indexfileBuffer)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to parse all big file path as allocate buffer failed. Indexfile[%s].",
            indexfilePath));
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Begin to parse all big file path. Indexfile[%s], Indexfile size[%d].",
        indexfilePath,
        fd.st_size));

    // 读索引文件内容到内存
    ACE_HANDLE fileHandle = ACE_OS::open(indexfilePath, O_RDONLY);
    if (ACE_INVALID_HANDLE == fileHandle)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to parse all big file path as open index file failed. Indexfile[%s].",
            indexfilePath));
        SVS_DELETE(indexfileBuffer, SVS_DELETE_MULTI);
        return SVS_RESULT_FAILURE;
    }

    (void)ACE_OS::lseek(fileHandle, 0, SEEK_SET);
    ssize_t readRet = ACE_OS::read(fileHandle, indexfileBuffer, (uint32_t)fd.st_size);
    if (fd.st_size != readRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to parse all big file path as read index file failed. "
            "Indexfile[%s], Indexfile size[%u], read result[%u].",
            indexfilePath,
            fd.st_size,
            readRet));
        SVS_DELETE(indexfileBuffer, SVS_DELETE_MULTI);
        (void)ACE_OS::close(fileHandle);
        return SVS_RESULT_FAILURE;
    }

    // md5校验
    char* pos = indexfileBuffer;
    SVS_INDEXFILE_HEADER* pIndexHeader = (SVS_INDEXFILE_HEADER*)(void*)pos;

    uint32_t md5SrcLen = (uint32_t)fd.st_size
                              - (SVS_RECORD_FILE_MAGIC_DATE_LEN + MD5CODELEN);
    pos = indexfileBuffer + (SVS_RECORD_FILE_MAGIC_DATE_LEN + MD5CODELEN);

    if (SVS_RESULT_OK != md5_decode((uint8_t*)pos, md5SrcLen, pIndexHeader->Md5code))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to parse all big file path as md5 decode failed. Indexfile[%s].",
            indexfilePath));
        (void)ACE_OS::close(fileHandle);
        return SVS_RESULT_FAILURE;
    }

    BIG_RECORDFILE_PATH_VECTOR tempBigRecordfilePathVec;
    BIG_RECORDFILE_PATH tempBigRecordfilePath;
    uint16_t vectorLoop;
    bool newFilePathFalg;
    int32_t fileSize = fd.st_size - (int32_t)sizeof(SVS_INDEXFILE_HEADER);
    pos = indexfileBuffer + sizeof(SVS_INDEXFILE_HEADER);
    SVS_BIGFILE_INFO * pBigfileInfo = (SVS_BIGFILE_INFO *)(void *)pos;
    while (fileSize > 0)
    {
        newFilePathFalg = true;
        pBigfileInfo = (SVS_BIGFILE_INFO*)(void*)pos;
        for (vectorLoop = 0; vectorLoop < bigRecordfilePathVec.size(); vectorLoop++)
        {
            // 如果vect中已经存在给大文件路径，则标记
            if (0 == strncmp((char*)pBigfileInfo->FilePath,
                             bigRecordfilePathVec[vectorLoop].BigFilePath,
                             FILEPATHLEN))
            {
                newFilePathFalg = false;
                break;
            }
        }

        // 如果没有在当前vector中没有该录像文件路径记录，则添加一个新成员
        if (newFilePathFalg)
        {
            (void)ACE_OS::memset(tempBigRecordfilePath.BigFilePath, 0, sizeof(tempBigRecordfilePath.BigFilePath));
            (void)ACE_OS::memcpy(tempBigRecordfilePath.BigFilePath,
                                 (char*)pBigfileInfo->FilePath,
                                 ACE_OS::strlen((char*)pBigfileInfo->FilePath));
            bigRecordfilePathVec.push_back(tempBigRecordfilePath);
        }

        fileSize -= (int32_t)pBigfileInfo->FileInfoTotalLen;
        pos += pBigfileInfo->FileInfoTotalLen;
    }

    // 释放资源
    tempBigRecordfilePathVec.clear();
    SVS_DELETE(indexfileBuffer, SVS_DELETE_MULTI);
    (void)ACE_OS::close(fileHandle);
    return SVS_RESULT_OK;
}


