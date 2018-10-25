#ifndef __SVS_MEDIA_PROCESSOR_H__
#define  __SVS_MEDIA_PROCESSOR_H__

#include <map>
#include <string>
#include <svs_ace_header.h>

// 定义消息处理线程数
#define  PROC_MSG_THREAD_NUM      1

// 定义媒体数据处理线程数
#define  PROC_MEDIA_THREAD_NUM      2

// 定义输入/输出类型，停止通知时需要传入
typedef enum tagHandleType
{
    INPUT_HANDLE    =  0x01,
    OUTPUT_HANDLE   =  0x02,
}HandleType;

// 定义接口消息类型及结构定义
// 该类型消息是放在Handle_Info->m_MsgQueue中的
typedef enum tagMediaEventType
{
    EVENT_START_RECORD             = 0x01,   // 开始录像
    EVENT_STOP_RECORD              = 0x02,   // 停止录像
    EVENT_RECORD_KEY_FRAME        = 0x03,   // 抽帧录像
    EVENT_SET_TIME_REMOVE         = 0x04,  // 设置回放平移时间
    EVENT_STOP_FINISH_NOTIFY      = 0x05, // FileEgress停止完成通知

    EVENT_RETRY                     = 0xFF, // 重试通知，当上一消息不能完成时需要重试
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

    // 初始化
    int32_t open(void* );

    // 媒体处理线线程
    int32_t svc(void);

    int32_t close(u_long);

    // 向处理器申请输入、输出的handle,返回handle的索引
    int32_t alloc_handle(const char* szDevID,
                           int32_t in_type,
                           int32_t out_type,
                           uint32_t &ulIndex);

    // 获取句柄状态，正常时返回OK。异常时直接返回错误码
    int32_t get_handle_status(uint32_t ulIndex);

    // 获取当前最新的收录像时间
    int32_t get_last_recv_time(time_t &lastRecvTime, uint32_t ulIndex);

    // 获取当前罪行写录像时间
    int32_t get_last_write_time(time_t &lastWriteTime, uint32_t ulIndex);

    // 获取底层关闭情况
    int32_t get_handle_close_status(const char * szDevID, int32_t in_type, int32_t out_type, uint32_t & handleIndex);

    // 开始进行录像
    int32_t start_record(uint32_t ulIndex,
                          uint32_t ulRouteID,
                          const ACE_INET_Addr &local_addr,
                          const ACE_INET_Addr &remote_addr,
                           time_t timeRemove = 0);

    // 停止录像
    int32_t stop_record(uint32_t ulIndex);

    // 启停抽帧录像，传入启动/停止的开关
    int32_t record_key_frame(uint32_t ulIndex, bool bSwitch);

    // 设置平台与回放录像平移时间
    int32_t set_time_remove(uint32_t ulIndex, time_t remove_time);

    // Egress停止完成后通知processor释放所有资源
    int32_t stop_finish_notify(uint32_t ulIndex);

    // 盘满删除覆盖通知
    int32_t overwriteRecordNotify(const uint32_t ulIndex,const time_t newIndexStartTime);

    // 增加一条数据库索引
    int32_t add_record_index(const uint32_t ulIndex,
                                    const time_t indexStartTime,
                                    const time_t indexStopTime);

    void debug_get_handle_info(const uint32_t ulIndex,
                                      char* strBuff,
                                      const uint32_t luBuffSize)const;

    int32_t write_media(uint32_t ulIndex, ACE_Message_Block* pMediaData);

    int32_t recv_close_notify(uint32_t ulIndex);

    // 获取正在使用的handle的信息
    int32_t statAllHandleInfo(uint32_t &startHandle, char * strBuffer, uint32_t nBuffSize)const;

    // 获取media队列信息
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

    // Processor管理的单个handle信息
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
//镜头ID和索引表，一个镜头会有实时和备份两个索引
typedef std::multimap<string, uint32_t>             DEV_INDEX_MAP;
typedef std::multimap<string, uint32_t>::iterator   DEV_INDEX_MAP_ITER;

private:
    SVS_Media_Processor();

    // 消息处理线程
    void procMessageThread();

    // 媒体处理线程
    void procMediaThread(uint32_t Index);

    int32_t enqueue_message(const MediaEventMsg &msg);

    int32_t getHandleIndex(uint32_t &ulIndex);

    void freeHandleIndex(uint32_t ulIndex);

    ACE_Message_Block* allocMessage();

    void freeMessage(ACE_Message_Block*);

    // 处理应用层发出的消息
    void handle_message(ACE_Message_Block*);

    // 与重试消息的处理区分开
    int32_t handle_stand_message(const MediaEventMsg *pEventMsg);

    // 处理重试消息
    int32_t handle_retry_message(const MediaEventMsg *pEventMsg);

    // 处理录像启动消息
    int32_t handleStartRecordMsg(const MediaEventMsg * pEventMsg);

    // 处理录像停止消息
    int32_t handleStopRecordMsg(const MediaEventMsg * pEventMsg);

    // 处理录像停止完成消息消息
    int32_t handleStopFinishNotifyMsg(const MediaEventMsg * pEventMsg);

    // 处理媒体消息
    void handle_media(ACE_Message_Block*);

    uint32_t getThreadIndex()
    {
        ACE_Guard<ACE_Mutex> locker(m_ThreadIndexMutex);
        return m_ulThreadIndex++;
    };

    // 线程退出标志
    volatile bool  m_bThreadControlFlag;
    uint32_t  m_ulThreadIndex;
    ACE_Mutex      m_ThreadIndexMutex;

    // 镜头ID与索引映射表
    DEV_INDEX_MAP  m_DevIndexMap;
    ACE_RW_Mutex   m_DevIndexMapMutex;

    // handle句柄索引映射表
    uint32_t   m_ulMaxHandleNum;
    Handle_Info     *m_HandleArray;
    ACE_RW_Mutex    m_HandleArrayMutex;

    // 空闲句柄索引
    std::list<uint32_t>   m_FreeIndexList;
    ACE_Mutex                  m_FreeIndexMutex;

    // 空闲通知消息列表
    std::list<ACE_Message_Block*> m_FreeMsgList;
    ACE_Mutex                     m_FreeMsgListMutex;
private:
    // 数据队列
    ACE_Message_Queue<ACE_MT_SYNCH>  m_MediaQueue[PROC_MEDIA_THREAD_NUM];

};
#endif /* __SVS_MEDIA_PROCESSOR_H__ */
