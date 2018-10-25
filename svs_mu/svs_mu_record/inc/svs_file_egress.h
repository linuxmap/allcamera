#ifndef __SVS_FILE_EGRESS_H__
#define __SVS_FILE_EGRESS_H__

typedef struct tagCallBackParam
{
    SVS_File_Egress   *m_pFileEgress;           // 输出对象
    uint32_t       m_ulChangeRecordFile;    // 是否切换了录像文件
    uint32_t       m_ulChangeDisk;           // 是否切换了磁盘
    ACE_Message_Block   *m_pMediaData;            // 已经写完的数据块
    char                m_RecordFileName[MAXNAMELEN + 1];  // 切换后的录像文件名
    uint64_t  m_ullRecordFileSize;              // 切换后的录像文件大小
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

    // 初始化，打开输出
    int32_t open(const char* szDevID, uint32_t ulIndex);

    // 关闭输出，需要把录像文件关闭
    int32_t close();

    int32_t write(ACE_Message_Block *pMediaData);

    // 状态检查
    int32_t check_status();

    // 更新索引回调接口
    static int32_t write_call_back(int32_t fd, int32_t return_code, void *pParam);

    // 写入结束后的回调处理
    int32_t on_write_finish(int32_t write_result, void *param);

    // 录像删除时，修改当前数据库索引开始时间
    int32_t overwrite_record_notify(const time_t newIndexStartTime);

    // 增加一条索引记录
    int32_t add_index(const time_t indexStartTime,
                        const time_t indexStopTime);

    // 最后写录像时间
    time_t lastWriteTime()const {return m_LastWriteRecordTime;}

    // 获取录像文件路径
    const char* recordFilePath()const {return m_szRecordFilePath;}

    // 设置平台录像时间偏移，为前端录像准备接口
    void setTimeRemove(time_t timeRemove);
private:
    // 定义Egress内部状态
    typedef enum tagEgressStatus
    {
        EGRESS_STATUS_INIT       = 0x01,         // 初始化
        EGRESS_STATUS_WRITING    = 0x02,         // 正常写入
        EGRESS_STATUS_CLOSING    = 0x03,         // 等待关闭
        EGRESS_STATUS_CLOSED     = 0x04,         // 已关闭

        EGRESS_STATUS_NODISK     = 0x05,          // 磁盘异常

        EGRESS_STATUS_ERROR      = 0xFF           // 其它异常
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

    // 关闭egress完成处理
    void close_finished();
    // 处理写录像回调参数
    int32_t handleWriteCallBackParam(int32_t write_result, void * param);
private:
    // 当前状态
    EgressStatus           m_EgressStatus;

    // 索引ID
    uint32_t          m_ulIOHandlerIndex;

    // 设备ID
    char                   m_szDeviceID[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    // 当前录像文件
    int32_t                    m_RecordFileFD;

    // 当前录像文件大小
    uint64_t     m_ullRecordFileSize;

    // 录像文件全名
    char                   m_szRecordFilePath[MAXNAMELEN + 1];

private:
    // 录像索引文件信息类
    CSVS_Record_Indexfile  m_RecordIndexFile;

    SVS_Index_List         m_IndexList;

    // 写录像序号
    volatile uint32_t m_ulWriteIndex;

    // 录像回调参数锁
    pthread_mutex_t m_mediaCallbackParamMutex;

    // 录像回调参数列表
    CallbackParamList m_mediaCallbackParamList;
private:
    // 录像开始时间，也是接收到第一帧数据的时间
    time_t                 m_FirstWriteRecordTime;

    // 最后一次写入录像的时间
    time_t                 m_LastWriteRecordTime;

    // 收到的总数据字节点
    uint64_t    m_ullAllRecvedDataSize;

    // 写入文件的总字节数
    uint64_t     m_ullWriteDataSize;

    // 上一次切换录像文件的时间
    time_t                 m_LastSwitchRecordTime;

    // 是否切换了磁盘，打开文件时更新
    bool                   m_bHasSwitchDisk;

    time_t m_timeRemove;

    SVS_Egress_Stat m_egressStat;
};

#endif /* __SVS_FILE_EGRESS_H__ */
