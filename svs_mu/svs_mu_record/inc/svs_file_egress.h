#ifndef __SVS_FILE_EGRESS_H__
#define __SVS_FILE_EGRESS_H__

typedef struct tagCallBackParam
{
    SVS_File_Egress   *m_pFileEgress;           // �������
    uint32_t       m_ulChangeRecordFile;    // �Ƿ��л���¼���ļ�
    uint32_t       m_ulChangeDisk;           // �Ƿ��л��˴���
    ACE_Message_Block   *m_pMediaData;            // �Ѿ�д������ݿ�
    char                m_RecordFileName[MAXNAMELEN + 1];  // �л����¼���ļ���
    uint64_t  m_ullRecordFileSize;              // �л����¼���ļ���С
    bool                m_medaDataPartedFlag;
    uint32_t       m_ulWriteIndex;
    time_t              m_ulWriteTime;
    bool                m_bIsFinished;
    int32_t                 m_nWriteRet;
}CallBackParam;

typedef std::list<CallBackParam*> CallbackParamList;
typedef std::list<CallBackParam*>::iterator  CallbackParamListIter;


class SVS_File_Egress
{
public:
    SVS_File_Egress();

    virtual ~SVS_File_Egress();

    // ��ʼ���������
    int32_t open(const char* szDevID, uint32_t ulIndex);

    // �ر��������Ҫ��¼���ļ��ر�
    int32_t close();

    int32_t write(ACE_Message_Block *pMediaData);

    // ״̬���
    int32_t check_status();

    // ���������ص��ӿ�
    static int32_t write_call_back(int32_t fd, int32_t return_code, void *pParam);

    // д�������Ļص�����
    int32_t on_write_finish(int32_t write_result, void *param);

    // ¼��ɾ��ʱ���޸ĵ�ǰ���ݿ�������ʼʱ��
    int32_t overwrite_record_notify(const time_t newIndexStartTime);

    // ����һ��������¼
    int32_t add_index(const time_t indexStartTime,
                        const time_t indexStopTime);

    // ���д¼��ʱ��
    time_t lastWriteTime()const {return m_LastWriteRecordTime;}

    // ��ȡ¼���ļ�·��
    const char* recordFilePath()const {return m_szRecordFilePath;}

    // ����ƽ̨¼��ʱ��ƫ�ƣ�Ϊǰ��¼��׼���ӿ�
    void setTimeRemove(time_t timeRemove);
private:
    // ����Egress�ڲ�״̬
    typedef enum tagEgressStatus
    {
        EGRESS_STATUS_INIT       = 0x01,         // ��ʼ��
        EGRESS_STATUS_WRITING    = 0x02,         // ����д��
        EGRESS_STATUS_CLOSING    = 0x03,         // �ȴ��ر�
        EGRESS_STATUS_CLOSED     = 0x04,         // �ѹر�

        EGRESS_STATUS_NODISK     = 0x05,          // �����쳣

        EGRESS_STATUS_ERROR      = 0xFF           // �����쳣
    }EgressStatus;
private:
    void reset();

    int32_t update_record_index(const CallBackParam *pCallBackParam);

    void update_db_record_index(const char* newFileName,
                                        int32_t iRecordStopFlag = 0);

    int32_t  delete_prorecord_file(time_t deleteTime);

    int32_t open_file( time_t dataTime ,bool &medaDataPartedFlag);

    void parse_key_frame(const CallBackParam *pCallBackParam);

    int32_t parse_index_seg(const CallBackParam * pCallBackParam,
                             const SVS_NEW_ALL_HEADER * pMediaAllHeader,
                             uint64_t recordFileSize,
                             KEYFRAME_VECTOR &keyFrameVec,
                             uint32_t & ulChangeDiskFlag);

    int32_t add_delete_task(uint8_t deleteType, time_t deleteTime);

    void update_recved_info(const ACE_Message_Block * pMediaData);

    // �ر�egress��ɴ���
    void close_finished();
    // ����д¼��ص�����
    int32_t handleWriteCallBackParam(int32_t write_result, void * param);
private:
    // ��ǰ״̬
    EgressStatus           m_EgressStatus;

    // ����ID
    uint32_t          m_ulIOHandlerIndex;

    // �豸ID
    char                   m_szDeviceID[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    // ��ǰ¼���ļ�
    int32_t                    m_RecordFileFD;

    // ��ǰ¼���ļ���С
    uint64_t     m_ullRecordFileSize;

    // ¼���ļ�ȫ��
    char                   m_szRecordFilePath[MAXNAMELEN + 1];

private:
    // ¼�������ļ���Ϣ��
    CSVS_Record_Indexfile  m_RecordIndexFile;

    SVS_Index_List         m_IndexList;

    // д¼�����
    volatile uint32_t m_ulWriteIndex;

    // ¼��ص�������
    pthread_mutex_t m_mediaCallbackParamMutex;

    // ¼��ص������б�
    CallbackParamList m_mediaCallbackParamList;
private:
    // ¼��ʼʱ�䣬Ҳ�ǽ��յ���һ֡���ݵ�ʱ��
    time_t                 m_FirstWriteRecordTime;

    // ���һ��д��¼���ʱ��
    time_t                 m_LastWriteRecordTime;

    // �յ����������ֽڵ�
    uint64_t    m_ullAllRecvedDataSize;

    // д���ļ������ֽ���
    uint64_t     m_ullWriteDataSize;

    // ��һ���л�¼���ļ���ʱ��
    time_t                 m_LastSwitchRecordTime;

    // �Ƿ��л��˴��̣����ļ�ʱ����
    bool                   m_bHasSwitchDisk;

    time_t m_timeRemove;

    SVS_Egress_Stat m_egressStat;
};

#endif /* __SVS_FILE_EGRESS_H__ */
