#include <svs_ace_header.h>
#include "vms/vms.h"
#include "svs_log_msg.h"
#include "svs_stat_manager.h"
#include "svs_record_index.h"
#include "svs_real_record_server.h"
#include "svs_rt_record_common.h"
#include "svs_index_list.h"
#include "svs_index_resend_mgr.h"

SVS_Index_List::SVS_Index_List()
{
    reset();
}

SVS_Index_List::~SVS_Index_List()
{
}

int32_t SVS_Index_List::init(const char* szDevID)
{
    //(void)ACE_OS::memcpy(m_szDeviceID, szDevID, SVS_DEVICEID_LEN);
    SetLocalizeDeviceId((uint8_t *)m_szDeviceID,
        sizeof(m_szDeviceID),
        szDevID);

    SVS_LOG((SVS_LM_DEBUG, "Camera[%s] init index list.", m_szDeviceID));

    return SVS_RESULT_OK;
}

void SVS_Index_List::close()
{
    SVS_LOG((SVS_LM_DEBUG, "Camera[%s] close index list.", m_szDeviceID));

    // �������б���
    reset();

    return;
}

//���±���������¼
int32_t SVS_Index_List::updateLocalIndex(time_t firstTime,
                                            time_t lastTime,
                                            uint64_t ullWriteSize,
                                            const char* szFileName,
                                            int32_t iRecordStopFlag/* = 0*/)
{
    SVS_LOG((SVS_LM_DEBUG,
        "Update local index, camera[%s], starttime[%u], endtime[%u], "
        "writesize[%Q], filename[%s], recordstopflag[%d].",
        m_szDeviceID, firstTime, lastTime,
        ullWriteSize, szFileName, iRecordStopFlag));

    uint64_t ullFileSize = 0;

    //�����������¼�Ŀ�ʼʱ���뵱ǰ��ʼʱ�䲻�ȣ����ʾ�������һ��¼��
    if (firstTime != m_localIndexStartTime)
    {
        if ((firstTime > lastTime)
             || (NULL == szFileName))
        {
            SVS_LOG((SVS_LM_WARNING,
                "camera[%s] update index fail. index in memory info:"
                "FirstTime[%u] LastTime[%u] FileName[%s].",
                m_szDeviceID,
                firstTime,
                lastTime,
                szFileName));

            return SVS_ERR_PARAM;
        }

        m_ullIndexFileSize = m_ullCurFileSize;
        ullFileSize = ullWriteSize - m_ullIndexFileSize;


        //�����ؼ�¼�Ŀ�ʼʱ��Ϊ�µĿ�ʼʱ��
        m_localIndexStartTime  = firstTime;

        IndexInfo indexInfo;
        (void)ACE_OS::memset((char*)&indexInfo, 0x0, sizeof(IndexInfo));
        indexInfo.StartTime    = firstTime;
        indexInfo.StopTime     = lastTime;

        //�洢�ռ����,���汾��д����̵��ܴ�С
        indexInfo.VideoFlux = ullFileSize;
        (void)ACE_OS::memcpy(indexInfo.RecordFileName, szFileName, MAXNAMELEN);

        // ���������Ƿ�����ı�־
        indexInfo.iRecordStopFlag = iRecordStopFlag;

        m_localIndexList.push_back(indexInfo);

        //ɾ����Ч�ļ�¼����Ҫ���±���������������Ч��ʼʱ��
        if (m_localIndexList.size() > 1)
        {
            IndexInfo *tmpInfo = &m_localIndexList.front();

            //�����������ȵ�ǰ¼��ο�ʼʱ������Ԥ¼���ʱ����ɾ������
            if (SVS_PRORECORD_MAX_TIME < firstTime - tmpInfo->StopTime)
            {
                SVS_LOG((SVS_LM_INFO,
                    "Camera[%s] drop record index, start time [%d], stop time[%d].",
                    m_szDeviceID,
                    tmpInfo->StartTime,
                    tmpInfo->StopTime));

                m_localIndexList.pop_front();

                //��Ҫ���±���������������Ч��ʼʱ��
                tmpInfo = &m_localIndexList.front();
                m_lastLocalIndexTime = tmpInfo->StartTime;
            }
        }

        SVS_LOG((SVS_LM_INFO,
            "Camera[%s] add record index, start time [%d], stop time[%d], file name[%s].",
            m_szDeviceID,
            firstTime,
            lastTime,
            szFileName));

    }
    else    //���ڿ�ʼʱ�䲻��ģ���ֻ���½���ʱ��
    {
        ullFileSize = ullWriteSize - m_ullIndexFileSize;

        bool bUpdateFlag = false;

        for (IndexListIter iter = m_localIndexList.begin();
             iter != m_localIndexList.end(); iter++)
        {
            if (iter->StartTime == firstTime)
            {
                iter->StopTime = lastTime;

                //�洢�ռ����,���汾��д����̵��ܴ�С
                iter->VideoFlux = ullFileSize;
                // ���¼�������Ƿ�����ı�־
                iter->iRecordStopFlag = iRecordStopFlag;
                SVS_LOG((SVS_LM_INFO,
                    "Camera[%s] update record index, start time [%d], stop time[%d], file name[%s].",
                    m_szDeviceID,
                    firstTime,
                    lastTime,
                    szFileName));

                bUpdateFlag = true;

                break;
            }
        }

        if ( !bUpdateFlag )
        {
            SVS_LOG((SVS_LM_WARNING,
                "camera[%s] update index fail, start time [%d], stop time[%d], file name[%s].",
                m_szDeviceID,
                firstTime,
                lastTime,
                szFileName));

            return SVS_RESULT_FAILURE;
        }
    }

    // ��¼����ǰΪֹ�ܵ��ļ���С
    m_ullCurFileSize = ullWriteSize;


    //���±������������ʱ��
    m_localIndexStopTime  = lastTime;

    // ��¼������������Ч��ʼʱ��,��ʱ���������ݿ��в���������������
    if (0 == m_lastLocalIndexTime)
    {
        m_lastLocalIndexTime = firstTime;
    }
    // ������ݿ��е������Ѿ������µ��ˣ����ø���
    if(m_lastDBIndexTime >= m_localIndexStopTime)
    {
        SVS_LOG((SVS_LM_INFO,
            "Need't update index to db.lastDBIndexTime[%u], localIndexStopTime[%u]",
            m_lastDBIndexTime, m_localIndexStopTime));
        return SVS_RESULT_OK;
    }

    //���Դ������ݿ�����
    handleDBIndex();

    return SVS_RESULT_OK;
}

//���һ�����ݿ�������ʼ��¼
int32_t SVS_Index_List::updateDBIndex(time_t startTime, time_t stopTime)
{
    if (0 == startTime)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera[%s] update db index fail, start time[%d] invalid.",
            m_szDeviceID,
            startTime));

        return SVS_RESULT_FAILURE;
    }

    ACE_Guard<ACE_Mutex> locker(m_dbIndexMutex);

    //�������ݿ�������¼
    for (IndexListIter iter = m_dbIndexList.begin();
         iter != m_dbIndexList.end(); iter++)
    {
        if (iter->StartTime == startTime)
        {
            iter->StopTime = stopTime;

            SVS_LOG((SVS_LM_INFO,
                "Camera[%s] update db index, start time [%d], stop time[%d].",
                m_szDeviceID,
                startTime,
                stopTime));

            return SVS_RESULT_OK;
        }
    }

    //û���ҵ���¼��������һ��
    IndexInfo indexInfo;
    (void)ACE_OS::memset((char*)&indexInfo, 0x0, sizeof(IndexInfo));
    indexInfo.StartTime = startTime;
    indexInfo.StopTime  = stopTime;
    m_dbIndexList.push_back(indexInfo);

    SVS_LOG((SVS_LM_INFO,
        "Camera[%s] add db index, start time [%d], stop time[%d].",
        m_szDeviceID,
        startTime,
        stopTime));

    return SVS_RESULT_OK;
}

int32_t SVS_Index_List::updateDBIndexStartTime(const time_t startTime)
{
    // ������޸ĵ�������ʽ��ǰ��ͷʹ�õ�������
    if ((0 != m_dbIndexStartTime) && (m_dbIndexStartTime < startTime))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Notify the recording camera that record start index is notified."
            "Notified camera id[%s],current section start time[%u] , new index start time[%u].",
            m_szDeviceID,
            m_dbIndexStartTime,
            startTime));

        m_dbIndexStartTime = startTime;
        return SVS_RESULT_OK;
    }

    SVS_LOG((SVS_LM_INFO,
        "Porcess record start index notified message finished."
        "CameraId[%s] ,old section start time[%u] => new index start time[%u ].",
        m_szDeviceID,
        m_dbIndexStartTime,
        startTime));

    return SVS_RESULT_OK;
}

void SVS_Index_List::handleDBIndex()
{
    //�¼�¼���ʱ��ܶ�ʱ�����ܻ��ж�����¼
    ACE_Guard<ACE_Mutex> locker(m_dbIndexMutex);

    IndexListIter iter = m_dbIndexList.begin();

    //�������ݿ�������¼
    while (iter != m_dbIndexList.end())
    {
        // ���ݿ�ʼʱ��ͽ���ʱ��������������ı���������¼
        int32_t nRet = processDBIndex(iter->StartTime, iter->StopTime);

        if (SVS_RESULT_OK != nRet)
        {
            return;
        }

        //��ǰ���ݿ������Ľ���ʱ��Ϊ0
        //����ʷ�����ݿ�������¼����������
        if ((0 != iter->StopTime)
            && (iter->StopTime <= m_localIndexStopTime))
        {
            // ���������Ѿ�������ɣ���Ҫɾ����������¼
            SVS_LOG((SVS_LM_INFO,
                "Camera[%s] has processed db index[%d:%d].",
                m_szDeviceID,
                iter->StartTime,
                iter->StopTime));

            iter = m_dbIndexList.erase(iter);

            //���ݿ�������ʼʱ����0 ???
            m_dbIndexStartTime   = 0;

            // ����������һ��
            continue;
        }

        // ��ǰ������û�д����꣬�����������¼Ҳ�Ȳ�����(���������Ҳ������,
        // ֻ���¼�¼���Ҽ���ܶ�ʱ���ܳ���)
        break;
    }

    return;
}

int32_t SVS_Index_List::processDBIndex(time_t startTime, time_t stopTime)
{
    //������ݿ������ȱ��������Ľ���ʱ�仹�磬���ݲ�����
//    if (startTime > m_localIndexStopTime)
    if (startTime >= m_localIndexStopTime)
    {
        SVS_LOG((SVS_LM_INFO,
            "Camera[%s] process db index[%u:%u], local index[%u:%u].",
            m_szDeviceID,
            startTime,
            stopTime,
            m_localIndexStartTime,
            m_localIndexStopTime));

        return SVS_RESULT_OK;
    }

    if((startTime >= stopTime) && (0 != stopTime))
    {
        SVS_LOG((SVS_LM_INFO, "camera[%s] process db index[%d:%d], local index[%d:%d].",
                   m_szDeviceID,
                   startTime,
                   stopTime,
                   m_localIndexStartTime,
                   m_localIndexStopTime));

        return SVS_RESULT_OK;
    }

    // ���ݿ������Ѿ�ȷ�����ӱ���������¼�в��Ҷ�Ӧ����ʵ������
    // �����û�������ݿ��и������������ָ���Ŀ�ʼʱ���¼
    // ��������ݿ��¼��ʼ����
    time_t indexStartTime = startTime;
    time_t indexStopTime  = stopTime;
    int32_t operType          = RECORD_INDEX_TYPE_INSERT;

    //������ݿ������Ѿ�����,��m_dbIndexStartTime����Ϊ������ʼʱ��
    if (0 != m_dbIndexStartTime)
    {
        indexStartTime     = m_dbIndexStartTime;
        operType           = RECORD_INDEX_TYPE_UPDATE;
    }
    else
    {
        SVS_LOG((SVS_LM_DEBUG, "Init camera[%s] db start and last time to [%u].", m_szDeviceID, indexStartTime));

        // �����ʼʱ��ȱ�����������Чʱ��С����ض�����???
        //��¼��ʼ��һ��ʱ��ſ�ʼ������ʼ��������
        if (indexStartTime < m_lastLocalIndexTime)
        {
            indexStartTime = m_lastLocalIndexTime;
        }

        //���ݿ�������ʼʱ��ͽ���ʱ������
        m_dbIndexStartTime = indexStartTime;
        m_lastDBIndexTime  = indexStartTime;
    }

    //ȷ�����������Ľ���ʱ��Ϊ��ǰ¼��Ľ���ʱ��
    if ((0 == indexStopTime)
        || (indexStopTime > m_localIndexStopTime) )
    {
        //��Ҫһֱ¼���������δ����
        indexStopTime     = m_localIndexStopTime;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Camera[%s] begin to process index[%u:%u].",
        m_szDeviceID,
        indexStartTime,
        indexStopTime));

    int32_t iRecordStopFlag = 0;
    if ((0 != stopTime) && (indexStopTime >= stopTime))
    {
        iRecordStopFlag = 1;
    }

    int32_t partitionRet = partitionDBRecordIndex(operType, indexStartTime,
                                        indexStopTime, iRecordStopFlag);

    return partitionRet;
}

int32_t SVS_Index_List::partitionDBRecordIndex(int32_t operType,
                                                time_t indexStartTime,
                                                time_t indexStopTime,
                                                int32_t iRecordStopFlag)
{
    SVS_LOG((SVS_LM_DEBUG,
        "Partition db record index, camera[%s], starttime[%u], endtime[%u], "
        "opertype[%d], recordstopflag[%d].",
        m_szDeviceID, indexStartTime, indexStopTime,
        operType, iRecordStopFlag));

    bool bLocalFlag      = false;
    int32_t  iLocalRecordStopFlag = iRecordStopFlag;
    time_t  dbStartTime  = 0;
    time_t  dbStopTime   = 0;
    uint64_t    ullFileSize = 0;

    IndexListIter iter = m_localIndexList.begin();

    //��¼�������뱾����������У��
    while ( (iter != m_localIndexList.end())
             && (indexStopTime > iter->StartTime))
    {
         // �������������Ѿ����ڣ�ֱ������,ͨ����������������
        if (indexStartTime >= iter->StopTime)
        {
            SVS_LOG((SVS_LM_INFO,
                "Camera[%s] record index overtime, start time [%d], stop time[%d].",
                m_szDeviceID,
                indexStartTime,
                indexStopTime));

            iter++;

            continue;
        }

        dbStartTime = iter->StartTime;
        dbStopTime  = iter->StopTime;

        // �����������������ʼʱ��ȼƻ��е����ݿ⿪ʼʱ���
        // ����Ҫ����һ���¼�¼��ͬʱ�޸����ݿ������Ŀ�ʼʱ��
        if (dbStartTime > indexStartTime)
        {
            SVS_LOG((SVS_LM_DEBUG, "camera[%s] db start and last time set to [%d].",
                           m_szDeviceID,
                           dbStartTime));

            m_dbIndexStartTime = dbStartTime;
            m_lastDBIndexTime  = dbStartTime;
            //������ݿ�����������ֱ������Ϊ��ǰ��С
            ullFileSize = iter->VideoFlux;

            if (!iRecordStopFlag)
            {
                iLocalRecordStopFlag = iter->iRecordStopFlag;
            }

            operType           = RECORD_INDEX_TYPE_INSERT;
        }
        else
        {
            //���������ζ�Ӧ�Ĵ�СΪƽ�����ʺ�Ĵ�С
            ullFileSize = (iter->VideoFlux/(uint64_t)(dbStopTime - dbStartTime)) * (uint64_t)(dbStopTime - indexStartTime);//lint !e571 !e776 for store cal

            if (!iRecordStopFlag)
            {
                iLocalRecordStopFlag = iter->iRecordStopFlag;
            }

            dbStartTime        = indexStartTime;
        }

        // �жϱ��������Ľ���ʱ���Ƿ��������ݿ⿪ʼʱ��
        if (dbStopTime > indexStopTime)
        {
            //�����ض�������С
            ullFileSize = (ullFileSize/(uint64_t)(dbStopTime - dbStartTime)) * (uint64_t)(indexStopTime - dbStartTime);//lint !e571 !e776 for store cal
            dbStopTime         = indexStopTime;
        }

        // �����ݿ��з���������Ҫ���ǿ�0������
        struct tm tmStart;
        struct tm tmEnd;
        (void)ACE_OS::localtime_r( &dbStartTime, &tmStart );
        (void)ACE_OS::localtime_r( &dbStopTime, &tmEnd );

        //���촦������һ����¼
        if ( (tmStart.tm_yday != tmEnd.tm_yday) )
        {
            tmEnd.tm_min = 0;
            tmEnd.tm_sec = 0;
            tmEnd.tm_hour = 0;

            time_t  stop_time = ACE_OS::mktime(&tmEnd);

            // ��������ʱ��¼�ļ���С
            m_ullIndexFileSize = m_ullCurFileSize;

            //�����ض�������С
            ullFileSize = (ullFileSize/(uint64_t)(dbStopTime - dbStartTime)) * (uint64_t)(stop_time - dbStartTime);//lint !e571 !e776 for store cal

            // ���ڿ���������ֱ����Ϊ���������Ѿ�����
            iLocalRecordStopFlag = 1;

            // �ȸ���ǰ��ȷ���Ĳ������»����һ����0��ļ�¼
            (void)sendDBRecordIndex(operType,
                                    0,
                                    dbStartTime,
                                    (stop_time - 1),
                                    ullFileSize,
                                    iter->RecordFileName,
                                    iLocalRecordStopFlag);

            // ����һ��0�㵽��ǰʱ��ļ�¼,��ʼʱ����ڽ���ʱ�䣬Ϊ��Ч�ԣ�������̴�СΪ1KB
            iLocalRecordStopFlag = 0;
            ullFileSize = (uint64_t)RTRECORD_UNIT_K;
            (void)sendDBRecordIndex(RECORD_INDEX_TYPE_INSERT,
                                    0,
                                    stop_time,
                                    dbStopTime,
                                    ullFileSize,
                                    iter->RecordFileName,
                                    iLocalRecordStopFlag);

             // ���������Ŀ�ʼʱ��Ϊ0��
             m_dbIndexStartTime = stop_time;
             m_lastDBIndexTime  = stop_time;
             m_lastLocalIndexTime = dbStopTime;

             SVS_LOG((SVS_LM_DEBUG,
                "Camera[%s] db start and last time set to [%d].",
                m_szDeviceID,
                stop_time));
        }
        else
        {
            (void)sendDBRecordIndex(operType,
                                    0,
                                    dbStartTime,
                                    dbStopTime ,
                                    ullFileSize,
                                    iter->RecordFileName,
                                    iLocalRecordStopFlag);
        }

        // ���±��������Ŀ�ʼʱ��
        m_lastLocalIndexTime = dbStopTime;

        // �������ݿ�����������ɣ�ֱ�ӷ��أ�����ת����һ��
        if (indexStopTime <= iter->StopTime)
        {
            bLocalFlag = true;
            break;
        }

        // ת������һ��
        iter++;
    }

    if (!bLocalFlag)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera[%s] can't find local index by time[%d:%d].",
            m_szDeviceID,
            indexStartTime,
            indexStopTime));

        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

int32_t SVS_Index_List::sendDBRecordIndex(   int32_t         nType,
                                        time_t      nChangeBeforeStartTime,
                                        time_t      nStartTime,
                                        time_t      nEndTime,
                                        uint64_t    ullDataSize,
                                        const char* szFilePath,
                                        int32_t         iRecordStopFlag)
{
    SVS_LOG((SVS_LM_DEBUG,
        "send record index message, CameraId[%s], opertype[%d],start time[%u] "
        "end time[%u],last change start time[%u], ullDataSize[%Q]B, "
        "szFilePath[%s], record stop flag[%d].",
        m_szDeviceID,
        nType,
        nStartTime,
        nEndTime,
        nChangeBeforeStartTime,
        ullDataSize,
        szFilePath,
        iRecordStopFlag));

    // ������̿ռ�Ϊ0,�����Ϊ1KB,��Ӱ����Ӫ(�ڿ�ʼ¼��ĵ�һ��ý���д��ɣ����Ұ����˿�30s�̿����������0��
    // ��Ϊ�ϲ����������ɺ��ٸ��µ�д��С��Ա����)
    if (0 == ullDataSize)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Send index message.The data size is 0 and adjust it to 1KB."
            "CameraId[%s], opertype[%d],start time[%u] "
            "end time[%u],last change start time[%u], ullDataSize[%Q]B, szFilePath[%s].",
            m_szDeviceID,
            nType,
            nStartTime,
            nEndTime,
            nChangeBeforeStartTime,
            ullDataSize,
            szFilePath));

        ullDataSize = (uint64_t)RTRECORD_UNIT_K;
    }

    //����¼��������Ϣ
    uint32_t msgLen = sizeof(Update_Record_Index);

    ACE_Message_Block *mb =  g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK, msgLen );

    if ( NULL == mb )
    {
        char strStartTime[SVS_STR_TIME_MAX_LEN];
        (void)time2string(strStartTime, sizeof(strStartTime), nStartTime );

        char strEndTime[SVS_STR_TIME_MAX_LEN];
        (void)time2string(strEndTime, sizeof(strEndTime), nEndTime );

        char strChangeBeforeStartTime[SVS_STR_TIME_MAX_LEN];
        (void)time2string(  strChangeBeforeStartTime,
                            sizeof(strChangeBeforeStartTime),
                            nChangeBeforeStartTime );

        SVS_LOG((SVS_LM_DEBUG, "Add record index task to queue failed."
            " Allocate message block failed.CameraId[%s],start time[%s:%d] "
            "end time[%s:%d],last change start time[%s:%d].",
            m_szDeviceID,
            strStartTime,
            nStartTime,
            strEndTime,
            nEndTime,
            strChangeBeforeStartTime,
            nChangeBeforeStartTime ));

        return SVS_RESULT_FAILURE;
    }

    Update_Record_Index *pIndex = (Update_Record_Index *)(void*)mb->wr_ptr();
    pIndex->Type = nType;
    pIndex->StartTime = nStartTime;
    pIndex->EndTime  = nEndTime;
    pIndex->VideoFlux = ullDataSize / (uint64_t)RTRECORD_UNIT_K;
    pIndex->ChangeBeforeStartTime = nChangeBeforeStartTime;
    (void)ACE_OS::memset(pIndex->RecordFilePath, 0x0, MAXNAMELEN + 1);

    //������ǰ¼���ļ��Ĵ���·��
    char *szPathEnd = ACE_OS::strstr((char*)szFilePath, m_szDeviceID);
    if (NULL != szPathEnd)
    {
        (void)ACE_OS::memcpy(pIndex->RecordFilePath,
                             szFilePath,
                             ((uint32_t)(szPathEnd - szFilePath) - 1));
    }

    mb->wr_ptr( msgLen );

    SetLocalizeDeviceId((uint8_t *)(pIndex->LensId),
        sizeof(pIndex->LensId),
        m_szDeviceID);

    // �����ϱ�״̬
    pIndex->RecordReportType = REPORT_INDEX_TYPE_NORMAL;
    if (iRecordStopFlag)
    {
        pIndex->RecordReportType = REPORT_INDEX_TYPE_OVER;
        (void)CIndexResendMgr::instance()->RecordPastIndex(pIndex);
    }

    CAC_RT_Record_Server *pServer = CAC_RT_Record_Server::instance();

    if( NULL == pServer )
    {
        SVS_LOG((SVS_LM_ERROR,
            "The sigleton of a class is NULL, the class name[CAC_RT_Record_Server]."));

        g_p_msg_buffer->free_mb( mb );

        return SVS_RESULT_FAILURE;
    }

    //¼��������Ϣ�����
    ACE_Time_Value tv(ACE_OS::gettimeofday());
    tv.set(tv.sec(), tv.usec() + SVS_ENQUEUE_RECORD_INDEX_TIMEOUT);

    if ( SVS_RESULT_FAILURE == pServer->enqueue_record_index( mb, &tv ) )
    {
        int32_t lastError = ACE_OS::last_error();

        char strStartTime[SVS_STR_TIME_MAX_LEN];
        (void)time2string(strStartTime, sizeof(strStartTime), nStartTime );

        char strEndTime[SVS_STR_TIME_MAX_LEN];
        (void)time2string(strEndTime, sizeof(strEndTime), nEndTime );

        char strChangeBeforeStartTime[SVS_STR_TIME_MAX_LEN];
        (void)time2string(  strChangeBeforeStartTime,
                            sizeof(strChangeBeforeStartTime),
                            nChangeBeforeStartTime );

        SVS_LOG((SVS_LM_ERROR,
            "Add record index task to queue failed."
            "The queue enqueue failed. cameraId[%s],start time[%s:%d],"
            "end time[%s:%d],last change start time[%s:%d]",
            m_szDeviceID,
            strStartTime,
            nStartTime,
            strEndTime,
            nEndTime,
            strChangeBeforeStartTime,
            nChangeBeforeStartTime ));

        g_p_msg_buffer->free_mb(mb);

        // ���´򿪶���
        if ( EWOULDBLOCK != lastError )
        {
            (void)pServer->reopen_record_index_queue();
        }
    }

    return SVS_RESULT_OK;
}

void SVS_Index_List::reset()
{
    (void)ACE_OS::memset(m_szDeviceID, 0x0, SVS_DEVICEID_LEN + 1);

    // ����������¼
    m_localIndexStartTime   = 0;
    m_localIndexStopTime    = 0;
    m_localIndexList.clear();

    // ���ݿ�������¼
    m_dbIndexStartTime      = 0;
    m_dbIndexList.clear();

    // ������������Ч��ʼʱ��
    m_lastLocalIndexTime    = 0;

    // ��ʷ���һ�θ������ݿ�������ʱ��
    m_lastDBIndexTime       = 0;

    // ����ʱ��¼�ļ���С
    m_ullCurFileSize        = 0;
    m_ullIndexFileSize      = 0;

    SVS_LOG((SVS_LM_DEBUG, "Camera[%s] reset index list.", m_szDeviceID));
}

