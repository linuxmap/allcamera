/*****************************************************************************
   ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �ļ���          : SVS_Record_Index.h
  �汾��          : 1.0
  ��������        : 2008-12-01
  ����޸�        :
  ��������        : �����͸���¼�������ļ�
  �����б�        :
  �޸���ʷ        :
  1 ����          : 2008-12-01
    �޸�����      : ����
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

// ���¼�������ļ���󻺴����ռ�
#define RECORD_INDEXFILE_BUF_MAX          (128*1024)

#pragma pack(push, 1)

struct Update_Record_Index
{
    int32_t     Type;          //0: ���½���ʱ��,1: ����������, 2.�޸Ŀ�ʼʱ��
    char    LensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1]; //��ͷID.
    time_t  ChangeBeforeStartTime;   //�޸�֮ǰ�Ŀ�ʼʱ��
    time_t  StartTime;  //¼��ʼʱ��
    time_t  EndTime;    //¼�����ʱ��
    uint64_t    VideoFlux;
    int32_t     RecordReportType;   // �����ϱ��ļ�������
    char    RecordFilePath[MAXNAMELEN + 1];       //���´��̷���ʱ, ��д��
};

#pragma pack(pop)

enum RECORD_INDEX_OPERATE
{
    RECORD_INDEX_OPERATE_OPEN = 0,       // ��һ��¼���ļ�
    RECORD_INDEX_OPERATE_REOPEN,        // ���´�һ��¼���ļ�
    RECORD_INDEX_OPERATE_CLOSE,          // �ر�¼���ļ�

    RECORD_INDEX_OPERATE_NONE            // �޲���
};

/*
 * CLASS: CAC_Record_Indexfile
 * DESCRIPTION: ¼�������ļ�ʵ����
 */
class CSVS_Record_Indexfile
{
public:
    CSVS_Record_Indexfile();
    virtual ~CSVS_Record_Indexfile();

    // ��ʼ��
    bool init(const char * strLensId);

    // ����һ��¼�������ļ�
    int32_t create_indexfile(time_t dataTime,
                              char* recordFilePath,
                              uint64_t recordFileSize,
                              bool medaDataPartedFlag);

    // ���¹ؼ�֡��Ϣ
    int32_t update_key_frame(KEYFRAME_VECTOR &keyFrameVec);

    // ���ڴ��е�¼����������ͬ����¼�������ļ���
    int32_t update_indexfile(time_t endTime);

    int32_t close();

    // ��ȡ¼���ļ��Ĵ�С
    uint64_t get_record_file_size()const;

    // ����¼���ļ��Ĵ�С
    void set_record_file_size(uint64_t file_size);


protected:
    // ������ǰʱ��Ƭ��¼�������ļ�
    int32_t parse_indexfile(bool & isNewBigFile, char * recordFilePath);

    // ����Ŀ¼
    int32_t make_dir(time_t fileTime);

    // �л�ʱ��Ƭ��ʱ�������������
    void reset();

protected:

    // �����ļ����ݻ���
    char record_index_buffer_[RECORD_INDEXFILE_BUF_MAX];

    // �����ļ����
    ACE_FILE_IO indexfile_;

    // �ļ��򿪱�ʶ
    bool indexFileOpenFlag_;

    // �����ļ�·��
    char str_file_path_[MAXNAMLEN + 1];

    // ��ͷID
    char str_lens_id_[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    // ��ǰ�����ļ������һ�����ļ���Ϣ���������ļ��е�ƫ��
    uint32_t curr_big_file_cursor_;

    // ��ǰ�����ļ������һ������Ϣ�������ļ��е�ƫ��
    uint32_t curr_sec_cursor_;

    // ��ǰ�����ļ��ܴ�С
    uint32_t indexfile_size_;

    // ��ǰ�صĹؼ�֡��Ϣ
    KEYFRAME_VECTOR keyframe_vec_;

    // ��ǰ���ڴ��ļ���ƫ��
    uint64_t cluster_offset_;

    // ��ǰ����Ϣ�Ƿ�Ϊ�ձ�־
    bool cluster_null_flag;

    // ��ǰ����ʼʱ���
    uint32_t cluster_start_timetick_;
   // ���������ļ���Ӧ�ĵ�ǰ¼���ļ��Ĵ�С
   uint64_t       m_RecordFileSize;
// Ϊ��Ԫ����������Ԫ��
#ifdef UNITTEST
    friend class CSVS_Record_Indexfile_Test;
#endif
};

// ���������¼�������ļ����������ļ���ȫ���Ĵ�¼���ļ�·��
int32_t parse_all_big_recordfile_path(const char* indexfilePath, BIG_RECORDFILE_PATH_VECTOR& bigRecordfilePathVec);
#endif //_SVS_Record_Index_h


