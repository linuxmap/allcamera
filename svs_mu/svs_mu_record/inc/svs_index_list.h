#ifndef __SVS_INDEX_LIST_H__
#define __SVS_INDEX_LIST_H__

#include <list>

class SVS_Index_List
{
public:
    SVS_Index_List();

    virtual ~SVS_Index_List();

    int32_t init(const char* szFileName);

    void close();

    int32_t updateLocalIndex(time_t firstTime, time_t lastTime,
                    uint64_t ullWriteSize, const char* szFileName,
                    int32_t iRecordStopFlag = 0);

    int32_t updateDBIndex(time_t startTime, time_t stopTime);

    int32_t updateDBIndexStartTime(const time_t startTime);

    time_t LastDBRecordIndex()const
    {
        return m_lastDBIndexTime;
    };

private:
    typedef struct tagIndexInfo
    {
        time_t     StartTime;
        time_t     StopTime;
        uint64_t   VideoFlux;               //索引段总大小
        int32_t        iRecordStopFlag;        // 是否已经停止录像
        char       RecordFileName[MAXNAMELEN + 1];
    }IndexInfo;

    typedef std::list<IndexInfo>            IndexList;
    typedef std::list<IndexInfo>::iterator  IndexListIter;

private:
    // 初始化
    void reset();

    // 处理数据库索引信息
    void handleDBIndex();

    // 根据指定的开始时间和结束时间，将符合条件的本地索引入库
    int32_t processDBIndex(time_t startTime, time_t stopTime);

    // 每30s一个数据库索引更新语句，所以需要把索引分条入库
    int32_t partitionDBRecordIndex(int32_t operType,
                                    time_t indexStartTime,
                                    time_t indexStopTime,
                                    int32_t iRecordStopFlag);

    // 发送一条数据库索引信息
    int32_t sendDBRecordIndex(   int32_t         nType,
                                    time_t      nChangeBeforeStartTime,
                                    time_t      nStartTime,
                                    time_t      nEndTime,
                                    uint64_t    ullDataSize,
                                    const char* szFilePath,
                                    int32_t iRecordStopFlag);

private:
    char              m_szDeviceID[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    // 本地索引记录开始时间(当前纪录)
    time_t            m_localIndexStartTime;

    //本地索引纪录结束时间(当前纪录)
    time_t            m_localIndexStopTime;

    //本地索引纪录列表
    IndexList         m_localIndexList;

    // 数据库索引记录 开始时间
    time_t            m_dbIndexStartTime;

    ACE_Mutex         m_dbIndexMutex;

    //数据库索引纪录列表
    IndexList         m_dbIndexList;

    // 本地索引的有效开始时间
    time_t            m_lastLocalIndexTime;

    // 历史最后一次更新数据库索引的时间
    time_t            m_lastDBIndexTime;

    // 当前总的文件大小
    uint64_t           m_ullCurFileSize;
    // 到前一个索引为止的文件大小
    uint64_t          m_ullIndexFileSize;
};

#endif /* __SVS_INDEX_LIST_H__ */
