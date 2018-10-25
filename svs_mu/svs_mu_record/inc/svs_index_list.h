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
        uint64_t   VideoFlux;               //�������ܴ�С
        int32_t        iRecordStopFlag;        // �Ƿ��Ѿ�ֹͣ¼��
        char       RecordFileName[MAXNAMELEN + 1];
    }IndexInfo;

    typedef std::list<IndexInfo>            IndexList;
    typedef std::list<IndexInfo>::iterator  IndexListIter;

private:
    // ��ʼ��
    void reset();

    // �������ݿ�������Ϣ
    void handleDBIndex();

    // ����ָ���Ŀ�ʼʱ��ͽ���ʱ�䣬�����������ı����������
    int32_t processDBIndex(time_t startTime, time_t stopTime);

    // ÿ30sһ�����ݿ�����������䣬������Ҫ�������������
    int32_t partitionDBRecordIndex(int32_t operType,
                                    time_t indexStartTime,
                                    time_t indexStopTime,
                                    int32_t iRecordStopFlag);

    // ����һ�����ݿ�������Ϣ
    int32_t sendDBRecordIndex(   int32_t         nType,
                                    time_t      nChangeBeforeStartTime,
                                    time_t      nStartTime,
                                    time_t      nEndTime,
                                    uint64_t    ullDataSize,
                                    const char* szFilePath,
                                    int32_t iRecordStopFlag);

private:
    char              m_szDeviceID[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    // ����������¼��ʼʱ��(��ǰ��¼)
    time_t            m_localIndexStartTime;

    //����������¼����ʱ��(��ǰ��¼)
    time_t            m_localIndexStopTime;

    //����������¼�б�
    IndexList         m_localIndexList;

    // ���ݿ�������¼ ��ʼʱ��
    time_t            m_dbIndexStartTime;

    ACE_Mutex         m_dbIndexMutex;

    //���ݿ�������¼�б�
    IndexList         m_dbIndexList;

    // ������������Ч��ʼʱ��
    time_t            m_lastLocalIndexTime;

    // ��ʷ���һ�θ������ݿ�������ʱ��
    time_t            m_lastDBIndexTime;

    // ��ǰ�ܵ��ļ���С
    uint64_t           m_ullCurFileSize;
    // ��ǰһ������Ϊֹ���ļ���С
    uint64_t          m_ullIndexFileSize;
};

#endif /* __SVS_INDEX_LIST_H__ */
