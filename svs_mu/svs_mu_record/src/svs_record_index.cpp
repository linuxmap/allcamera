
/*****************************************************************************
   ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �ļ���          : SVS_Record_Index.cpp
  �汾��          : 1.0
  ��������        : 2008-12-01
  ����޸�        :
  ��������        : �����͸���¼�������ļ�
  �����б�        :
  �޸���ʷ        :
  1 ����          : 2008-12-01
    �޸�����      : ����
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
 �� �� ��  : md5_encode
 ��������  : ��ָ�����ȵ��ִ�����md5����
 �������  : uint8_t* strmd5Src:�����ܵ��ִ�
             uint32_t md5SrcLen:�����ܵ��ִ�����
             uint8_t* md5Result:���ܽ��
 �������  : NA
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
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
 �� �� ��  : md5_decode
 ��������  : ��ָ�����ȵ��ִ������ٴμ��ܣ��������ժҪ�Ƚ��Ƿ����
 �������  : uint8_t* strmd5Src:�������ܵ��ִ�
             uint32_t md5SrcLen:�����ܵ��ִ�����
             uint8_t* md5Result:�ļ�ժҪ
 �������  : NA
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
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
 �� �� ��  : lock_file
 ��������  : ��ָ�����ļ��������
 �������  : int32_t fileHandle:Ҫ�������ļ����
 �������  : NA
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
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

    // �鿴fileHandle�Ƿ���Լ�д��
    int32_t ret = inquire_lock(fileHandle, O_WRONLY, 0, SEEK_SET, 0);
    if (0 > ret)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Fail to get file write lock as file handle has been locked. file handle[%d]",
            fileHandle));
        return SVS_RESULT_FAILURE;
    }

    // ���ļ�����д��
    ret = write_lock(fileHandle, 0, SEEK_SET, 0);
    if (0 > ret)
    {
        SVS_LOG((SVS_LM_WARNING, "Fail to add write lock to file. file handle[%d].",fileHandle));
        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : unlock_file
 ��������  : ȡ���ļ���д��
 �������  : int32_t fileHandle
 �������  : NA
 �� �� ֵ  : �ɹ�:0
             ʧ��:-1
 �޸���ʷ  :
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
 �� �� ��  : init
 ��������  : ��ʼ��
 �������  : const char* strLensId:��ͷID
 �������  : ��
 �� �� ֵ  : �ɹ�:true
             ʧ��:false
 �޸���ʷ  :
*****************************************************************************/
bool CSVS_Record_Indexfile::init(const char * strLensId)
{
    SVS_TRACE();

    SetLocalizeDeviceId((uint8_t *)str_lens_id_, sizeof(str_lens_id_), strLensId);

    return true;
}

/*****************************************************************************
 �� �� ��  : reset
 ��������  : �����г�Ա������λ
 �������  : ��
 �������  : ��
 �� �� ֵ  : ��
 �޸���ʷ  :
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
 �� �� ��  : create_indexfile
 ��������  : ����¼�������ļ�
 �������  : time_t dateTime:���ݸ�ʱ�䴴��¼���ļ�
             char* recordFilePath:¼���ļ����·��
             uint64_t recordFileSize:¼���ļ���С
 �������  : ��
 �� �� ֵ  : �ɹ�:0   ʧ��:-1
 �޸���ʷ  :
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

    // ��λ
    reset();

    // ����Ҫ���������ļ���ʱ��dataTimeת��Ϊ����ʱ�䣬��λ:��
    time_t fileTime = dataTime - (dataTime % BIG_RECORDFILE_TIME_SLICE);

    // ���������ļ���Ӧ������ʱ�䣬������Ŀ¼������þ�ͷû�ж�Ӧ��Ŀ¼�����ᴴ����ͷĿ¼
    if (SVS_RESULT_OK != make_dir(fileTime))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to create directory for index file. camera[%s].",
            str_lens_id_));
        return SVS_RESULT_FAILURE;
    }

    // �������ڣ����������ļ����ƣ�������str_file_path_��
    uint32_t pathLen = ACE_OS::strlen(str_file_path_);
    (void)SVS_SS_UTILITIES::time2bigfilename(str_file_path_ + pathLen,
                                             (int32_t)(sizeof(str_file_path_) - pathLen),
                                             fileTime,
                                             "idx");

    // 3.�ж�¼�������ļ��Ƿ��Ѿ�����
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

    // ��������ļ��Ѿ�����,��Ҫ���������Ĵ�С�Ƿ���
    if (indexfile_size_ > 0)
    {
        isNewFile = false;

        // ���������̫С
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

    // ����¼�������ļ�
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
    // �¼�¼��ʱ̫����־��ֱ����0
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
        // ������½���¼�������ļ�
        increaseLen = sizeof(SVS_BIGFILE_INFO) - sizeof(SVS_BIGFILECLUSTER_INFO);

        // ���¼��ͷ��Ϣ
        pIndexHeader = (SVS_INDEXFILE_HEADER*)pos;
        (void)ACE_OS::memcpy(pIndexHeader, SVS_INDEXFILE_HEADER_STR, sizeof(SVS_INDEXFILE_HEADER_STR));
        pos += sizeof(SVS_INDEXFILE_HEADER);
        pIndexHeader->IndexFileTotalLen = sizeof(SVS_INDEXFILE_HEADER);

        // ��д��һ�����ļ���Ϣ
        pBigFileInfo = (SVS_BIGFILE_INFO*)pos;
        pBigFileInfo->StartTime = dataTime;
        pBigFileInfo->EndTime = dataTime;
        (void)ACE_OS::memcpy(pBigFileInfo->FilePath, recordFilePath, strlen(recordFilePath));
        pBigFileInfo->FileInfoTotalLen = 0;

        curr_big_file_cursor_ = (uint32_t)((char*)pBigFileInfo - record_index_buffer_);
    }
    else
    {
        // ��������ļ��Ѿ����ڣ����������ļ���
        bool isNewBigFile = true;
        if (SVS_RESULT_OK != parse_indexfile(isNewBigFile, recordFilePath))
        {
            SVS_LOG((SVS_LM_ERROR, "Fail to parse current index file. camera[%s], current index file[%s].",
                str_lens_id_,
                str_file_path_));
            return SVS_RESULT_FAILURE;
        }

        // ����ͷ��Ϣ
        pos = record_index_buffer_;
        pIndexHeader = (SVS_INDEXFILE_HEADER*)((void *)pos);

        // ���ļ���Ϣ
        pos = record_index_buffer_ + curr_big_file_cursor_;
        pBigFileInfo = (SVS_BIGFILE_INFO*)((void *)pos);

        // ������ϴ����һ����¼���ļ�·����ͬ�����һ�����ļ���Ϣ
        if (isNewBigFile)
        {
            increaseLen = sizeof(SVS_BIGFILE_INFO) - sizeof(SVS_BIGFILECLUSTER_INFO);

            // ���ļ���Ϣ
            pBigFileInfo->StartTime = dataTime;
            pBigFileInfo->EndTime = dataTime;
            (void)ACE_OS::memcpy(pBigFileInfo->FilePath, recordFilePath, strlen(recordFilePath));
        }
        else
        {
            increaseLen = sizeof(SVS_BIGFILE_SECTION_INFO) - sizeof(SVS_BIGFILECLUSTER_INFO);
        }
    }

    // ���³�����Ϣ
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

    // ��д�ö���Ϣ,������������Ҫ�����¶�
    SVS_BIGFILE_SECTION_INFO* pBigFileSecInfo =
        (SVS_BIGFILE_SECTION_INFO*)((void *)(record_index_buffer_ + curr_sec_cursor_));
    pBigFileSecInfo->StartTime = dataTime;
    pBigFileSecInfo->EndTime = dataTime;
    pBigFileSecInfo->FileOff = recordFileSize;
    pBigFileSecInfo->SectionTotalLen = sectionInfoLen;

    // ���������ļ��򿪱�ʶ
    indexFileOpenFlag_ = true;

    SVS_LOG((SVS_LM_INFO, "Success to create index file. Indexfile[%s], camera[%s].",
        str_file_path_,
        str_lens_id_));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : make_dir
 ��������  : ���������ļ����ڵ�Ŀ¼
 �������  : time_t fileTime �����ļ�Ŀ¼������,ת��������2009033011,��ʶ09��3��30��11���Ӧ��Ŀ¼
 �������  : ��
 �� �� ֵ  : �ɹ�:0  ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Record_Indexfile::make_dir(time_t fileTime)
{
    SVS_TRACE();

    // 1.������ͷ��Ŀ¼
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
        // ����Ǵ�����ͷĿ¼ʧ�ܣ�����ʧ��ԭ������ΪĿ¼�Ѿ����ڣ���Ϊ�������
        SVS_LOG((SVS_LM_ERROR,
            "Fail to create camera level directory. directory[%s], camera[%s].",
            str_file_path_,
            str_lens_id_));
        return SVS_RESULT_FAILURE;
    }

    // 2.�������ڼ�Ŀ¼
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
    // �¼�¼��ʱ̫����־��ֱ����0
    ACE_OS::last_error(0);

    SVS_LOG((SVS_LM_INFO,
        "Success to create the record index directory. "
        "directory[%s], camera[%s].",
        str_file_path_,
        str_lens_id_));
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : update_indexfile
 ��������  : ���ڴ��е�¼����������ͬ����¼�������ļ���
 �������  : time_t endTime
             lastFrameTime:���ڴ��е�¼��������Ϣˢ�µ�¼�������ļ��У�ÿ30sˢ��
             һ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0
             ����:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Record_Indexfile::update_indexfile(time_t endTime)
{
    // �����ǰû�д���Ϣ����ֱ�ӷ���
    SVS_LOG((SVS_LM_DEBUG,
        "Update index file. camera[%s], key frame num[%u], endTime[%u].",
        str_lens_id_,
        keyframe_vec_.size(),
        endTime));

    // ��ǰ����ϢΪ�ջ���ηǷ�
    if (cluster_null_flag || (endTime == 0))
    {
        return SVS_RESULT_OK;
    }

    // �������û�д򿪣��������������
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
    /*lint -e826*/   //��Ϣ�庬�䳤�ֶ�,��PC-LINT�澯����Ҫ�޸�
    char* pos = NULL;

    // 1.����¼�������ļ���С��Ϣ
    pos = record_index_buffer_;
    SVS_INDEXFILE_HEADER* pIndexfileHeader = (SVS_INDEXFILE_HEADER*)(void*)pos;
    pIndexfileHeader->IndexFileTotalLen  = indexfile_size_;
    pIndexfileHeader->IndexFileTotalLen += keyClusterSize;

    // 2.���´��ļ���Ϣ�����Ⱥͽ���ʱ��
    pos = record_index_buffer_ + curr_big_file_cursor_;
    SVS_BIGFILE_INFO* pBigfileInfo = (SVS_BIGFILE_INFO*)(void*)pos;
    pBigfileInfo->FileInfoTotalLen += keyClusterSize;
    pBigfileInfo->EndTime = endTime;

    // 3.���¶���Ϣ�����Ⱥͽ���ʱ��
    pos = record_index_buffer_ + curr_sec_cursor_;
    SVS_BIGFILE_SECTION_INFO* pSecInfo = (SVS_BIGFILE_SECTION_INFO*)(void*)pos;
    pSecInfo->SectionTotalLen += keyClusterSize;
    pSecInfo->EndTime = endTime;

    // 4.����һ���´أ����ȣ��Ƿ�����ؼ�֡��
    pos = record_index_buffer_ + indexfile_size_;
    SVS_BIGFILECLUSTER_INFO* pClusterInfo = (SVS_BIGFILECLUSTER_INFO*)(void*)pos;//lint !e826 just cast
    pClusterInfo->ClusterTotalLen = keyClusterSize;
    /*lint +e826*/   //��Ϣ�庬�䳤�ֶ�,��PC-LINT�澯����Ҫ�޸�
    // �ô��Ƿ��йؼ�֡��0:û�У�1:��
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

    // 5.���������ļ��ܴ�С
    indexfile_size_ += keyClusterSize;

    // 6.����md5
    uint8_t md5Temp[MD5CODELEN];
    uint32_t md5SrcLen = indexfile_size_
                              - (SVS_RECORD_FILE_MAGIC_DATE_LEN + MD5CODELEN);
    pos = record_index_buffer_ + (SVS_RECORD_FILE_MAGIC_DATE_LEN + MD5CODELEN);

    // ���md5 ʧ�ܣ���д�����ļ�
    if (SVS_RESULT_OK != md5_encode((uint8_t*)pos, md5SrcLen, md5Temp))
    {
        SVS_LOG((SVS_LM_WARNING, "Failt to encode index file with md5. Camera[%s].",
            str_lens_id_));
    }
    else
    {
        (void)ACE_OS::memcpy((char*)pIndexfileHeader->Md5code, (char*)md5Temp, MD5CODELEN);

        // 7.������д�������ļ�
        ACE_HANDLE fileHandle = indexfile_.get_handle();
        (void)ACE_OS::lseek(fileHandle, 0, SEEK_SET);

        // �����ȡ�ļ���ʧ��
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

    // 8.����ػ�����Ϣ
    cluster_offset_   = 0;
    cluster_null_flag = true;
    cluster_start_timetick_ = 0;
    keyframe_vec_.clear();

    SVS_LOG((SVS_LM_INFO, "Success to update index file. camera[%s].", str_lens_id_));
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : update_key_frame
 ��������  : ����д��¼���ļ���mb���������Ĺؼ�֡��Ϣ����ӵ���ǰ����Ϣ��
 �������  : KEYFRAME_VECTOR & keyFrameVec:mb�а�����ȫ���ؼ�֡��Ϣ,��һ����Ա��
             ��ʾ�ô�д��ý�������е�һ��֡�ڴ�¼���ļ��е�ƫ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0
             ����:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Record_Indexfile::update_key_frame(KEYFRAME_VECTOR & keyFrameVec)
{
    SVS_TRACE();

    uint8_t keyFrameSize = (uint8_t)keyFrameVec.size();


    // �����ǰ����ϢΪ�գ����ʾ��һ�θ��¹ؼ�֡��Ϣ
    if (cluster_null_flag)
    {
        cluster_offset_ = keyFrameVec[0].FileOffset;
        cluster_start_timetick_ = keyFrameVec[0].TimeTick;
        cluster_null_flag = false; // ���´ر�ʶ
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
 �� �� ��  : parse_indexfile
 ��������  : ����¼�������ļ����ҳ����һ�����ļ���λ�ã������ǰ��Ҫ����һ����
             �ļ���Ϣ������ļ���λ�õ�ǰ¼�������ļ������
 �������  :
 �������  : ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Record_Indexfile::parse_indexfile(bool &isNewBigFile, char* recordFilePath)
{
    SVS_TRACE();

    ACE_HANDLE fileHandle = indexfile_.get_handle();

    // ��ȡ����¼�������ļ�������
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

    // md5У��
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

        // �����ļ��Ѿ����޸ģ���ɾ��֮
        (void)indexfile_.close();
        (void)unlink(str_file_path_);
        return SVS_RESULT_FAILURE;
    }

    // �������һ����¼���ļ�
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

    // �Ƚ����һ�����ļ�·����Ϣ,���Ƿ���Ҫ����һ���µ�ý���ļ�
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
 �� �� ��  : parse_indexfile
 ��������  : ���������¼�������ļ����������ļ���ȫ���Ĵ�¼���ļ�·����¼��ɾ��
             ����ʹ�øù��ܺ�����
 �������  : char * indexfilePath:¼�������ļ�ȫ·��
 �������  : BIG_RECORDFILE_PATH_VECTOR & bigRecordfilePathVec:��¼���ļ�·���б�
             �����г�ԱΨһ��
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
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

    // �������ļ����ݵ��ڴ�
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

    // md5У��
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
            // ���vect���Ѿ����ڸ����ļ�·��������
            if (0 == strncmp((char*)pBigfileInfo->FilePath,
                             bigRecordfilePathVec[vectorLoop].BigFilePath,
                             FILEPATHLEN))
            {
                newFilePathFalg = false;
                break;
            }
        }

        // ���û���ڵ�ǰvector��û�и�¼���ļ�·����¼�������һ���³�Ա
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

    // �ͷ���Դ
    tempBigRecordfilePathVec.clear();
    SVS_DELETE(indexfileBuffer, SVS_DELETE_MULTI);
    (void)ACE_OS::close(fileHandle);
    return SVS_RESULT_OK;
}


