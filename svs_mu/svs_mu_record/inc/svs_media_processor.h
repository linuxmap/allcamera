#ifndef __SVS_MEDIA_PROCESSOR_H__
#define  __SVS_MEDIA_PROCESSOR_H__

#include <map>
#include <string>
#include <svs_ace_header.h>

// ������Ϣ�����߳���
#define  PROC_MSG_THREAD_NUM      1

// ����ý�����ݴ����߳���
#define  PROC_MEDIA_THREAD_NUM      2

// ��������/������ͣ�ֹ֪ͣͨʱ��Ҫ����
typedef enum tagHandleType
{
    INPUT_HANDLE    =  0x01,
    OUTPUT_HANDLE   =  0x02,
}HandleType;

// ����ӿ���Ϣ���ͼ��ṹ����
// ��������Ϣ�Ƿ���Handle_Info->m_MsgQueue�е�
typedef enum tagMediaEventType
{
    EVENT_START_RECORD             = 0x01,   // ��ʼ¼��
    EVENT_STOP_RECORD              = 0x02,   // ֹͣ¼��
    EVENT_RECORD_KEY_FRAME        = 0x03,   // ��֡¼��
    EVENT_SET_TIME_REMOVE         = 0x04,  // ���ûط�ƽ��ʱ��
    EVENT_STOP_FINISH_NOTIFY      = 0x05, // FileEgressֹͣ���֪ͨ

    EVENT_RETRY                     = 0xFF, // ����֪ͨ������һ��Ϣ�������ʱ��Ҫ����
}MediaEventType;

enum SVS_MEDIA_IO_HANDLE
{
    IO_INVALID_HANDLE = 0xFFFFFFFF
};

typedef struct tagMediaEventMsg
{
    MediaEventType  eventType;
    uint32_t   ulIndex;

    uint32_t   ulStreamID;
    ACE_INET_Addr   localAddr;
    ACE_INET_Addr   RemoteAddr;

    bool            bSwitchKeyFrameRecord;
    time_t          timeRemove;
}MediaEventMsg;


class SVS_Media_Processor : public ACE_Task<ACE_MT_SYNCH>
{
public:
    virtual ~SVS_Media_Processor();

    static SVS_Media_Processor & instance()
    {
        static SVS_Media_Processor svs_media_processor;
        return svs_media_processor;
    };

    // ��ʼ��
    int32_t open(void* );

    // ý�崦�����߳�
    int32_t svc(void);

    int32_t close(u_long);

    // �������������롢�����handle,����handle������
    int32_t alloc_handle(const char* szDevID,
                           int32_t in_type,
                           int32_t out_type,
                           uint32_t &ulIndex);

    // ��ȡ���״̬������ʱ����OK���쳣ʱֱ�ӷ��ش�����
    int32_t get_handle_status(uint32_t ulIndex);

    // ��ȡ��ǰ���µ���¼��ʱ��
    int32_t get_last_recv_time(time_t &lastRecvTime, uint32_t ulIndex);

    // ��ȡ��ǰ����д¼��ʱ��
    int32_t get_last_write_time(time_t &lastWriteTime, uint32_t ulIndex);

    // ��ȡ�ײ�ر����
    int32_t get_handle_close_status(const char * szDevID, int32_t in_type, int32_t out_type, uint32_t & handleIndex);

    // ��ʼ����¼��
    int32_t start_record(uint32_t ulIndex,
                          uint32_t ulRouteID,
                          const ACE_INET_Addr &local_addr,
                          const ACE_INET_Addr &remote_addr,
                           time_t timeRemove = 0);

    // ֹͣ¼��
    int32_t stop_record(uint32_t ulIndex);

    // ��ͣ��֡¼�񣬴�������/ֹͣ�Ŀ���
    int32_t record_key_frame(uint32_t ulIndex, bool bSwitch);

    // ����ƽ̨��ط�¼��ƽ��ʱ��
    int32_t set_time_remove(uint32_t ulIndex, time_t remove_time);

    // Egressֹͣ��ɺ�֪ͨprocessor�ͷ�������Դ
    int32_t stop_finish_notify(uint32_t ulIndex);

    // ����ɾ������֪ͨ
    int32_t overwriteRecordNotify(const uint32_t ulIndex,const time_t newIndexStartTime);

    // ����һ�����ݿ�����
    int32_t add_record_index(const uint32_t ulIndex,
                                    const time_t indexStartTime,
                                    const time_t indexStopTime);

    void debug_get_handle_info(const uint32_t ulIndex,
                                      char* strBuff,
                                      const uint32_t luBuffSize)const;

    int32_t write_media(uint32_t ulIndex, ACE_Message_Block* pMediaData);

    int32_t recv_close_notify(uint32_t ulIndex);

    // ��ȡ����ʹ�õ�handle����Ϣ
    int32_t statAllHandleInfo(uint32_t &startHandle, char * strBuffer, uint32_t nBuffSize)const;

    // ��ȡmedia������Ϣ
    void statMediaQueueInfo(char * strBuffer, uint32_t nBuffSize);

    uint32_t GetHandleLastError(uint32_t ulIndex);
public:

    typedef enum tagHandleStatus
    {
        HANDLE_STATUS_INVALID   = 0x00,
        HANDLE_STATUS_INIT      = 0x01,
        HANDLE_STATUS_START     = 0x02,
        HANDLE_STATUS_STOP      = 0x03,

        HANDLE_STATUS_BUTT,
    }HandleStatus;

    // Processor����ĵ���handle��Ϣ
    typedef struct tagHandle_Info
    {
        char                         m_szDevID[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
        CAC_Recv_Stream              *m_pRecvStream;
        SVS_File_Egress              *m_pFileEgress;
        int32_t                          m_nInType;
        int32_t                          m_nOutType;
        time_t                       m_StatusTime;
        HandleStatus                 m_Status;
        uint32_t                m_ulRefCount;
        ACE_Message_Queue<ACE_SYNCH> *m_pMediaQueue;
        ACE_Message_Queue<ACE_SYNCH> m_MsgQueue;
        ACE_RW_Mutex                 m_Mutex;
        void reset()
        {
            (void)ACE_OS::memset(m_szDevID, 0x0, SVS_DEVICEID_LEN + 1);
            m_pRecvStream = NULL;
            m_pFileEgress = NULL;
            m_pMediaQueue = NULL;
            m_nInType     = 0;
            m_nOutType    = 0;
            m_ulRefCount  = 1;
            m_StatusTime  = time(NULL);
            m_Status      = HANDLE_STATUS_INVALID;
        }
    } Handle_Info;
public:
//��ͷID��������һ����ͷ����ʵʱ�ͱ�����������
typedef std::multimap<string, uint32_t>             DEV_INDEX_MAP;
typedef std::multimap<string, uint32_t>::iterator   DEV_INDEX_MAP_ITER;

private:
    SVS_Media_Processor();

    // ��Ϣ�����߳�
    void procMessageThread();

    // ý�崦���߳�
    void procMediaThread(uint32_t Index);

    int32_t enqueue_message(const MediaEventMsg &msg);

    int32_t getHandleIndex(uint32_t &ulIndex);

    void freeHandleIndex(uint32_t ulIndex);

    ACE_Message_Block* allocMessage();

    void freeMessage(ACE_Message_Block*);

    // ����Ӧ�ò㷢������Ϣ
    void handle_message(ACE_Message_Block*);

    // ��������Ϣ�Ĵ������ֿ�
    int32_t handle_stand_message(const MediaEventMsg *pEventMsg);

    // ����������Ϣ
    int32_t handle_retry_message(const MediaEventMsg *pEventMsg);

    // ����¼��������Ϣ
    int32_t handleStartRecordMsg(const MediaEventMsg * pEventMsg);

    // ����¼��ֹͣ��Ϣ
    int32_t handleStopRecordMsg(const MediaEventMsg * pEventMsg);

    // ����¼��ֹͣ�����Ϣ��Ϣ
    int32_t handleStopFinishNotifyMsg(const MediaEventMsg * pEventMsg);

    // ����ý����Ϣ
    void handle_media(ACE_Message_Block*);

    uint32_t getThreadIndex()
    {
        ACE_Guard<ACE_Mutex> locker(m_ThreadIndexMutex);
        return m_ulThreadIndex++;
    };

    // �߳��˳���־
    volatile bool  m_bThreadControlFlag;
    uint32_t  m_ulThreadIndex;
    ACE_Mutex      m_ThreadIndexMutex;

    // ��ͷID������ӳ���
    DEV_INDEX_MAP  m_DevIndexMap;
    ACE_RW_Mutex   m_DevIndexMapMutex;

    // handle�������ӳ���
    uint32_t   m_ulMaxHandleNum;
    Handle_Info     *m_HandleArray;
    ACE_RW_Mutex    m_HandleArrayMutex;

    // ���о������
    std::list<uint32_t>   m_FreeIndexList;
    ACE_Mutex                  m_FreeIndexMutex;

    // ����֪ͨ��Ϣ�б�
    std::list<ACE_Message_Block*> m_FreeMsgList;
    ACE_Mutex                     m_FreeMsgListMutex;
private:
    // ���ݶ���
    ACE_Message_Queue<ACE_MT_SYNCH>  m_MediaQueue[PROC_MEDIA_THREAD_NUM];

};
#endif /* __SVS_MEDIA_PROCESSOR_H__ */
