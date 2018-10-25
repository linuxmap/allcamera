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

    // 重置所有变量
    reset();

    return;
}

//更新本地索引纪录
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

    //如果新索引纪录的开始时间与当前开始时间不等，则表示新添加了一段录像
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


        //将本地纪录的开始时间为新的开始时间
        m_localIndexStartTime  = firstTime;

        IndexInfo indexInfo;
        (void)ACE_OS::memset((char*)&indexInfo, 0x0, sizeof(IndexInfo));
        indexInfo.StartTime    = firstTime;
        indexInfo.StopTime     = lastTime;

        //存储空间管理,保存本地写入磁盘的总大小
        indexInfo.VideoFlux = ullFileSize;
        (void)ACE_OS::memcpy(indexInfo.RecordFileName, szFileName, MAXNAMELEN);

        // 保存索引是否结束的标志
        indexInfo.iRecordStopFlag = iRecordStopFlag;

        m_localIndexList.push_back(indexInfo);

        //删除无效的记录，需要更新本地索引的最新有效开始时间
        if (m_localIndexList.size() > 1)
        {
            IndexInfo *tmpInfo = &m_localIndexList.front();

            //当本地索引比当前录像段开始时间早于预录最大时长，删除索引
            if (SVS_PRORECORD_MAX_TIME < firstTime - tmpInfo->StopTime)
            {
                SVS_LOG((SVS_LM_INFO,
                    "Camera[%s] drop record index, start time [%d], stop time[%d].",
                    m_szDeviceID,
                    tmpInfo->StartTime,
                    tmpInfo->StopTime));

                m_localIndexList.pop_front();

                //需要更新本地索引的最新有效开始时间
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
    else    //对于开始时间不变的，则只更新结束时间
    {
        ullFileSize = ullWriteSize - m_ullIndexFileSize;

        bool bUpdateFlag = false;

        for (IndexListIter iter = m_localIndexList.begin();
             iter != m_localIndexList.end(); iter++)
        {
            if (iter->StartTime == firstTime)
            {
                iter->StopTime = lastTime;

                //存储空间管理,保存本地写入磁盘的总大小
                iter->VideoFlux = ullFileSize;
                // 添加录像索引是否结束的标志
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

    // 记录到当前为止总的文件大小
    m_ullCurFileSize = ullWriteSize;


    //更新本地索引的最后时间
    m_localIndexStopTime  = lastTime;

    // 记录本地索引的有效开始时间,该时间在向数据库中插入索引后向后更新
    if (0 == m_lastLocalIndexTime)
    {
        m_lastLocalIndexTime = firstTime;
    }
    // 如果数据库中的索引已经是最新的了，则不用更新
    if(m_lastDBIndexTime >= m_localIndexStopTime)
    {
        SVS_LOG((SVS_LM_INFO,
            "Need't update index to db.lastDBIndexTime[%u], localIndexStopTime[%u]",
            m_lastDBIndexTime, m_localIndexStopTime));
        return SVS_RESULT_OK;
    }

    //尝试处理数据库索引
    handleDBIndex();

    return SVS_RESULT_OK;
}

//添加一条数据库索引开始记录
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

    //更新数据库索引纪录
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

    //没有找到记录，新增加一条
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
    // 如果被修改的索引正式当前镜头使用的索引段
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
    //事件录像的时间很短时，可能会有多条记录
    ACE_Guard<ACE_Mutex> locker(m_dbIndexMutex);

    IndexListIter iter = m_dbIndexList.begin();

    //遍历数据库索引纪录
    while (iter != m_dbIndexList.end())
    {
        // 根据开始时间和结束时间查找满足条件的本地索引记录
        int32_t nRet = processDBIndex(iter->StartTime, iter->StopTime);

        if (SVS_RESULT_OK != nRet)
        {
            return;
        }

        //当前数据库索引的结束时间为0
        //对历史的数据库索引纪录，继续处理
        if ((0 != iter->StopTime)
            && (iter->StopTime <= m_localIndexStopTime))
        {
            // 本次索引已经处理完成，需要删除该索引记录
            SVS_LOG((SVS_LM_INFO,
                "Camera[%s] has processed db index[%d:%d].",
                m_szDeviceID,
                iter->StartTime,
                iter->StopTime));

            iter = m_dbIndexList.erase(iter);

            //数据库索引开始时间置0 ???
            m_dbIndexStartTime   = 0;

            // 继续处理下一条
            continue;
        }

        // 当前索引还没有处理完，后面的索引记录也先不处理(正常情况下也不会有,
        // 只在事件录像且间隔很短时可能出现)
        break;
    }

    return;
}

int32_t SVS_Index_List::processDBIndex(time_t startTime, time_t stopTime)
{
    //如果数据库索引比本地索引的结束时间还早，则暂不处理
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

    // 数据库索引已经确定，从本地索引记录中查找对应的真实索引段
    // 如果还没有向数据库中更新索引，则从指定的开始时间记录
    // 否则从数据库记录开始更新
    time_t indexStartTime = startTime;
    time_t indexStopTime  = stopTime;
    int32_t operType          = RECORD_INDEX_TYPE_INSERT;

    //如果数据库索引已经插入,则m_dbIndexStartTime设置为索引开始时间
    if (0 != m_dbIndexStartTime)
    {
        indexStartTime     = m_dbIndexStartTime;
        operType           = RECORD_INDEX_TYPE_UPDATE;
    }
    else
    {
        SVS_LOG((SVS_LM_DEBUG, "Init camera[%s] db start and last time to [%u].", m_szDeviceID, indexStartTime));

        // 如果开始时间比本地索引的有效时间小，则截断索引???
        //在录像开始后一段时间才开始真正开始接收码流
        if (indexStartTime < m_lastLocalIndexTime)
        {
            indexStartTime = m_lastLocalIndexTime;
        }

        //数据库索引开始时间和结束时间设置
        m_dbIndexStartTime = indexStartTime;
        m_lastDBIndexTime  = indexStartTime;
    }

    //确定本次索引的结束时间为当前录像的结束时间
    if ((0 == indexStopTime)
        || (indexStopTime > m_localIndexStopTime) )
    {
        //需要一直录像或者索引未结束
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

    //将录像索引与本地索引进行校验
    while ( (iter != m_localIndexList.end())
             && (indexStopTime > iter->StartTime))
    {
         // 这条本地索引已经过期，直接跳过,通常不会出现这种情况
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

        // 如果该条本地索引开始时间比计划中的数据库开始时间大
        // 则需要插入一条新记录，同时修改数据库索引的开始时间
        if (dbStartTime > indexStartTime)
        {
            SVS_LOG((SVS_LM_DEBUG, "camera[%s] db start and last time set to [%d].",
                           m_szDeviceID,
                           dbStartTime));

            m_dbIndexStartTime = dbStartTime;
            m_lastDBIndexTime  = dbStartTime;
            //如果数据库索引长，则直接设置为当前大小
            ullFileSize = iter->VideoFlux;

            if (!iRecordStopFlag)
            {
                iLocalRecordStopFlag = iter->iRecordStopFlag;
            }

            operType           = RECORD_INDEX_TYPE_INSERT;
        }
        else
        {
            //设置索引段对应的大小为平均码率后的大小
            ullFileSize = (iter->VideoFlux/(uint64_t)(dbStopTime - dbStartTime)) * (uint64_t)(dbStopTime - indexStartTime);//lint !e571 !e776 for store cal

            if (!iRecordStopFlag)
            {
                iLocalRecordStopFlag = iter->iRecordStopFlag;
            }

            dbStartTime        = indexStartTime;
        }

        // 判断本地索引的结束时间是否满足数据库开始时间
        if (dbStopTime > indexStopTime)
        {
            //继续截断索引大小
            ullFileSize = (ullFileSize/(uint64_t)(dbStopTime - dbStartTime)) * (uint64_t)(indexStopTime - dbStartTime);//lint !e571 !e776 for store cal
            dbStopTime         = indexStopTime;
        }

        // 向数据库中发送索引需要考虑跨0点问题
        struct tm tmStart;
        struct tm tmEnd;
        (void)ACE_OS::localtime_r( &dbStartTime, &tmStart );
        (void)ACE_OS::localtime_r( &dbStopTime, &tmEnd );

        //跨天处理，新起一条记录
        if ( (tmStart.tm_yday != tmEnd.tm_yday) )
        {
            tmEnd.tm_min = 0;
            tmEnd.tm_sec = 0;
            tmEnd.tm_hour = 0;

            time_t  stop_time = ACE_OS::mktime(&tmEnd);

            // 更新索引时记录文件大小
            m_ullIndexFileSize = m_ullCurFileSize;

            //继续截断索引大小
            ullFileSize = (ullFileSize/(uint64_t)(dbStopTime - dbStartTime)) * (uint64_t)(stop_time - dbStartTime);//lint !e571 !e776 for store cal

            // 对于跨天的情况，直接认为本条索引已经结束
            iLocalRecordStopFlag = 1;

            // 先根据前面确定的操作更新或插入一条到0点的记录
            (void)sendDBRecordIndex(operType,
                                    0,
                                    dbStartTime,
                                    (stop_time - 1),
                                    ullFileSize,
                                    iter->RecordFileName,
                                    iLocalRecordStopFlag);

            // 插入一条0点到当前时间的记录,起始时间等于结束时间，为有效性，构造磁盘大小为1KB
            iLocalRecordStopFlag = 0;
            ullFileSize = (uint64_t)RTRECORD_UNIT_K;
            (void)sendDBRecordIndex(RECORD_INDEX_TYPE_INSERT,
                                    0,
                                    stop_time,
                                    dbStopTime,
                                    ullFileSize,
                                    iter->RecordFileName,
                                    iLocalRecordStopFlag);

             // 更新索引的开始时间为0点
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

        // 更新本地索引的开始时间
        m_lastLocalIndexTime = dbStopTime;

        // 本条数据库索引处理完成，直接返回，否则转向下一条
        if (indexStopTime <= iter->StopTime)
        {
            bLocalFlag = true;
            break;
        }

        // 转向处理下一条
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

    // 如果磁盘空间为0,则矫正为1KB,不影响运营(在开始录像的第一个媒体块写完成，并且包含了跨30s刻苦的情况会等于0，
    // 因为上层更新索引完成后再更新的写大小成员变量)
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

    //生成录像索引消息
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

    //拷贝当前录像文件的磁盘路径
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

    // 设置上报状态
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

    //录像索引消息入队列
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

        // 重新打开队列
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

    // 本地索引记录
    m_localIndexStartTime   = 0;
    m_localIndexStopTime    = 0;
    m_localIndexList.clear();

    // 数据库索引记录
    m_dbIndexStartTime      = 0;
    m_dbIndexList.clear();

    // 本地索引的有效开始时间
    m_lastLocalIndexTime    = 0;

    // 历史最后一次更新数据库索引的时间
    m_lastDBIndexTime       = 0;

    // 索引时记录文件大小
    m_ullCurFileSize        = 0;
    m_ullIndexFileSize      = 0;

    SVS_LOG((SVS_LM_DEBUG, "Camera[%s] reset index list.", m_szDeviceID));
}

