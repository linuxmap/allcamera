#ifndef _SVS_DELETE_RECORD_MANAGER_H
#define _SVS_DELETE_RECORD_MANAGER_H
#include <svs_ace_header.h>

typedef struct del_indexfile_name_st
{
    char indexfile[MAXNAMELEN];
}del_indexfile_name_t;

typedef ACE_Vector<del_indexfile_name_t> INDEX_FILE_VECTOR;

typedef vector<SVS_DELETE_LENS_INFO>  VECTOR_DELETE_LENS_INFO;

// ¼��ɾ��������
class CAC_Delete_Record_Manager
{
public:
    CAC_Delete_Record_Manager();
    UTAPI ~CAC_Delete_Record_Manager();

    UTAPI void handle_del_record(const ACE_Message_Block *mb)const;

    UTAPI void handle_del_record_file(const ACE_Message_Block *mb) const;

    UTAPI void handle_del_prerecord_file(const ACE_Message_Block *mb) const;

    UTAPI void handle_del_date_dir(const ACE_Message_Block * mb) const;

    UTAPI void handle_message_normal(const ACE_Message_Block *mb)const;

    //UTAPI void handle_message_del_select(const ACE_Message_Block *mb) const;

    //UTAPI void handle_message_del_all(const ACE_Message_Block *mb) const;

    //UTAPI void handle_message_clear_db() const;

    //UTAPI void handle_message_disk_full(const ACE_Message_Block *mb) const;

    UTAPI void handle_message_prerecord(const ACE_Message_Block *mb) const;

    // ɾ������Ŀ¼
    UTAPI void delete_date_dir()const;

    // �����Ĺ���ɾ��
    UTAPI void delete_file_normal(const ACE_Message_Block *mb) const;

    // ����ɾ��¼���ļ�
    //UTAPI void delete_file_disk_full(const ACE_Message_Block *mb) const;

    // ɾ��Ԥ¼�ļ�
    UTAPI void delete_file_prerecord(const ACE_Message_Block *mb) const;

    // ɾ��¼���ļ�
    UTAPI int32_t delete_file(char* strLensId, time_t tmTime, uint64_t &ullFileSize) const;

    UTAPI int32_t enqueue_del_task(const SVS_DEL_RECORD *del_record) const;

    UTAPI int32_t enqueue_del_record(uint8_t msg_type, const uint8_t *lens_id, time_t start_time, time_t end_time, uint8_t delete_mode) const;

    //����¼��ɾ����Ӧ
    UTAPI int32_t send_record_delete_resp(char *pszCameraId,
                                                time_t tTime,
                                                uint64_t ulStoreSpace,
                                                int32_t iRespCode) const;

    UTAPI void get_index_root_path_list(time_t delTime, INDEX_FILE_VECTOR & indexRootPathList)const;

protected:
    // ɾ������Ŀ¼
    UTAPI int32_t delete_date_dir(const char * strLensDir, const time_t minTime) const;

    // �ж��Ƿ�ΪĿ¼
    UTAPI bool is_directory(const char * strDir)const;

    // ��ȡ��ǰ���Сʱ�������ļ��б���Сʱ�����ļ����ܴ��ڶ��
    UTAPI int32_t get_indexfile_list(const char * strIndexfileDir,
                            const char * strIndexfileSub,
                            INDEX_FILE_VECTOR & indexfileListVct)const;

    UTAPI int32_t get_camera_dir_list(const char * strDiskPath, VECTOR_DELETE_LENS_INFO & CameraVect)const;

private:
#ifdef UNITTEST
    friend class TEST_CAC_Delete_Record_Manager;
    friend class CAC_Delete_Record_ManagerAutoMock;
#endif
};
#endif // SVS_Delete_Record_Manager.h


