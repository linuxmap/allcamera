#include <svs_ace_header.h>
#include <vms/vms.h>
#include "svs_log_msg.h"
#include "svs_utility.h"
#include "svs_timer.h"
#include "svs_stat_manager.h"
#include "svs_rt_record_common.h"
#include "svs_real_record_server.h"
#include "svs_record_inform.h"
#include "svs_record_index.h"
#include "svs_record_disk.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"
#include "svs_delete_record_manager.h"

// extern int32_t g_alarmStatus[ALARM_INDEX_NUM];

CAC_Delete_Record_Manager::CAC_Delete_Record_Manager()
{

}

CAC_Delete_Record_Manager::~CAC_Delete_Record_Manager()
{

}

/*****************************************************************************
 函 数 名  : handle_del_record
 功能描述  : 处理删除任务消息队列中的消息
             根据不同的消息类型,调用不同的处理函数
 输入参数  : mb  --  消息块
 输出参数  :
 返 回 值  :
 修改历史  :
*****************************************************************************/
void CAC_Delete_Record_Manager::handle_del_record(const ACE_Message_Block *mb)const
{
    SVS_TRACE();
    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Precess deleting record message failed."
            "The parameter mb is NULL."));
        return;
    }

    SVS_DEL_RECORD *pDelRecord = (SVS_DEL_RECORD *)((void *)mb->rd_ptr());
    switch(pDelRecord->MsgType)
    {
        //删除预录文件消息
        case DEL_RECORD_TYPE_PRERECORD:
            SVS_LOG((SVS_LM_INFO,
                "[DEL]Delete record for prerecord, deleting message type[%d].",
                pDelRecord->MsgType));
            CAC_RT_Record_Server::instance()->print_msg_del_task(pDelRecord);
            handle_message_prerecord(mb);
            break;

        default:
            SVS_LOG((SVS_LM_WARNING,
                "[DEL]Delete record failed, Unknow deleting message type[%d].",
                pDelRecord->MsgType));
            break;
    }

    return;
}

/*****************************************************************************
 函 数 名  : handle_del_record_file
 功能描述  : 处理定时删除和磁盘满删除消息队列中的消息
             根据不同的消息类型，调用不同的处理函数
 输入参数  : mb  --  消息块
 输出参数  :
 返 回 值  :
 修改历史  :
*****************************************************************************/
void CAC_Delete_Record_Manager::handle_del_record_file(const ACE_Message_Block *mb) const
{
    SVS_TRACE();

    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Precess deleting record message failed.The parameter mb is NULL."));
        return;
    }

    SVS_DEL_RECORD_FILE *pDelRecordFile = (SVS_DEL_RECORD_FILE *)((void *)mb->rd_ptr());
    switch ( pDelRecordFile->MsgType )
    {
        case DEL_RECORD_TYPE_NORMAL:
            SVS_LOG((SVS_LM_DEBUG,
                "[DEL]Precess deleting record file for overdue."));
            delete_file_normal(mb);
            break;
        default:
            SVS_LOG((SVS_LM_WARNING,
                "[DEL]Precess deleting record file failed.Unknow message type[%d]",
                pDelRecordFile->MsgType));
            break;
    }

}

/*****************************************************************************
 函 数 名  : handle_del_prerecord_file
 功能描述  : 处理删除预录文件消息队列
 输入参数  : mb  --  消息块
 输出参数  :
 返 回 值  :
 修改历史  :
*****************************************************************************/
void CAC_Delete_Record_Manager::handle_del_prerecord_file(const ACE_Message_Block *mb) const
{
    SVS_TRACE();

    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Process deleting prerecord file failed, The parameter mb is NULL."));

        return;
    }

    SVS_DEL_RECORD_FILE *pDelRecordFile = (SVS_DEL_RECORD_FILE *)((void *)mb->rd_ptr());

    if(DEL_RECORD_TYPE_PRERECORD != pDelRecordFile->MsgType)
    {
        SVS_LOG((SVS_LM_ERROR,
             "[DEL]Process deleting prerecord file failed.Message type is invalid."
             "this message type [%d],deleting prerecord message type[%d].",
            pDelRecordFile->MsgType,
            DEL_RECORD_TYPE_PRERECORD));

        return;
    }

    delete_file_prerecord(mb);

}
/*****************************************************************************
 函 数 名  : handle_del_prerecord_file
 功能描述  : 处理删除预录文件消息队列
 输入参数  : mb  --  消息块
 输出参数  :
 返 回 值  :
 修改历史  :
*****************************************************************************/
void CAC_Delete_Record_Manager::handle_del_date_dir(const ACE_Message_Block *mb)const
{
    SVS_TRACE();

    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Process deleting overdue date directory failed.The parameter mb is NULL."));
        return;
    }

    PSVS_HEADER pHeader = (PSVS_HEADER)(void*)mb->rd_ptr();
    if  (SVS_MSG_TYPE_STORESVR_QUERY_LENS_MINTIME_REQ == pHeader->MsgType)
    {
        delete_date_dir();
    }
    else if (SVS_MSG_TYPE_STORESVR_QUERY_LENS_MINTIME_RESP == pHeader->MsgType)
    {
        PSVS_STORESVR_QUERY_LENS_MINTIME_RESP_MSG pMsg = NULL;
        pMsg = (PSVS_STORESVR_QUERY_LENS_MINTIME_RESP_MSG)(void*)mb->rd_ptr();

        uint32_t unLensIdx = 0;
        char strLensDir[MAXNAMELEN];
        uint8_t strLensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];
        for (unLensIdx = 0; unLensIdx < pMsg->DeviceCount; unLensIdx++)
        {
            SetLocalizeDeviceId(strLensId,
                sizeof(strLensId),
                pMsg->LensMinTime[unLensIdx].LensId.ID,
                DEVICEID_LEN);

            // 判断镜头ID是否合法
            if (0 == strlen((const char*)strLensId))
            {
                SVS_LOG((SVS_LM_WARNING, "Camera [%s] is invalid.", strLensId));
                continue;
            }

            (void)snprintf(strLensDir,
                            sizeof(strLensDir),
                            "%s/%s",
                            pMsg->FilePath,
                            strLensId);
            (void)delete_date_dir(strLensDir, (time_t)pMsg->LensMinTime[unLensIdx].MinTime);
            (void)sleep(1);
        }
    }

    return;
}

/*****************************************************************************
 函 数 名  : handle_message_prerecord
 功能描述  : 处理删除预录文件消息
             处理步骤:
               1、根据删除时间推算需要删除的时间段
               2、根据时间段构造SQL语句：
                  SELECT count(*)
                  FROM tbl_recordfile_outline_info A
                  WHERE A.Device_Id='00000000010100000299'
                  AND A.Server_Id='00000001106020000001'
                  AND A.Start_Time>=TO_DATE('推算的结束时间', 'YYYYMMDDHH24MISS')
                  AND A.End_Time<=TO_DATE('推算的开始时间', 'YYYYMMDDHH24MISS')
                  查找是否存在该镜头的该时间段的记录。

                  需要考虑在这段时间还有连续录像，手动录像，
                  都在tbl_recordfile_outline_info表中查询即可

               3、如果不存在，根据镜头ID，起始时间和结束时间构造删除消息，
                  放入DeleteRecordQueue队列中。
 输入参数  : mb  --  消息块
 输出参数  :
 返 回 值  :
 修改历史  :
*****************************************************************************/
void CAC_Delete_Record_Manager::handle_message_prerecord(const ACE_Message_Block *mb) const
{
    SVS_TRACE();

    if ( NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Process deleting message for prerecord failed."
            "The parameter mb is NULL."));
        return;
    }

    SVS_DEL_RECORD *pDelRecord = (SVS_DEL_RECORD *)((void *)mb->rd_ptr());

    SVS_LOG((SVS_LM_DEBUG,
        "[DEL]Begin to process deleting message for prerecord, "
        "message type[%d], delete time[%u].",
        pDelRecord->MsgType,
        pDelRecord->DeleteTime));

    if ( DEL_RECORD_TYPE_PRERECORD != pDelRecord->MsgType)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Process deleting message for prerecord failed, "
            "Message type is invalid, "
            "this message type [%d], right message type[%d].",
            pDelRecord->MsgType,
            DEL_RECORD_TYPE_PRERECORD));

        return;
    }

    // 判断当前时间是否超过整点加预录时间,如果没有超过,消息回队列继续等待处理
    // 作此判断是因为如果原Rt部分生成预录文件删除消息,再等待预录时长入Del的队列,
    // 在等待的这段时间内,如果预录时间被改短了, Rt就无法把消息放入Del的队列
    SVS_LENS_PARAMETER lensParam;
    int32_t nResult = CAC_Record_Inform_Manager::instance()->get_lens_param(
                (char *)pDelRecord->LensId,
                lensParam);
    if ( SVS_RESULT_FAILURE == nResult)
    {
        SVS_LOG((SVS_LM_WARNING,
            "[DEL]Get camera's parmater failed.Use maximal default value[%d]s."
            "cameraId[%s].",
            SVS_PRORECORD_MAX_TIME,
            pDelRecord->LensId));
        // 最大预录周期
        lensParam.BeforeSeconds = SVS_PRORECORD_MAX_TIME;
    }

    if ( time(NULL) <= (ADJUST_FORWARD(pDelRecord->DeleteTime, SECONDS_PER_HOUR) + (time_t)lensParam.BeforeSeconds))
    {
        SVS_LOG((SVS_LM_WARNING,
            "[DEL]Current time is smaller than the deleting time."
            "Delete record for prerecord.Enqueue the task again.delete time[%d].",
            pDelRecord->DeleteTime));
        // 重入队列
        if(SVS_RESULT_FAILURE == enqueue_del_task(pDelRecord))
        {
            SVS_LOG((SVS_LM_ERROR,
                "[DEL]Deleting mesage enqueue failed.Delete record for prerecord."));
        }
        return;
    }

    // 根据删除时间推算起始时间和结束时间
    // 推算的时间段是删除时间的前一个小时的时间段
    time_t tmStart = ADJUST_FORWARD(pDelRecord->DeleteTime, SECONDS_PER_HOUR) - SECONDS_PER_HOUR;
    time_t tmEnd = ADJUST_FORWARD(pDelRecord->DeleteTime, SECONDS_PER_HOUR);

    // 向Store查询该时间段内是否有索引记录
    // 目前一次只查一个镜头
    ACE_Message_Block *msgMb = NULL;
    PSVS_STORESVR_QUERY_INDEX_REQ_MSG pMsg = NULL;
    uint32_t msgLen = sizeof(SVS_STORESVR_QUERY_INDEX_REQ_MSG);
    msgMb = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG , msgLen);
    if (NULL == msgMb)
    {
        // 重入队列
        if(SVS_RESULT_FAILURE == enqueue_del_task(pDelRecord))
        {
            SVS_LOG((SVS_LM_ERROR,
                "[DEL]Deleting mesage enqueue failed.Delete record for prerecord."));
        }
        return;
    }

    pMsg = (PSVS_STORESVR_QUERY_INDEX_REQ_MSG)(void*)msgMb->wr_ptr();
     // 封装消息头
    FillCommonHeader(   &pMsg->Header,
                        SVS_MSG_TYPE_STORESVR_QUERY_INDEX_REQ,
                        CAC_RT_Record_Server::instance()->transaction_no(),
                        (uint16_t)msgLen);

    SetNLSDeviceId((uint8_t *)pMsg->NRUID,
                sizeof(pMsg->NRUID),
                CAC_RT_Record_Server::instance()->str_id());


    pMsg->DeviceCount = 1;
    memcpy(pMsg->IndexFlag[0].LensId.ID,pDelRecord->LensId, DEVICEID_LEN);
    pMsg->IndexFlag[0].StartTime = (uint32_t)tmStart;
    pMsg->IndexFlag[0].EndTime = (uint32_t)tmEnd;
    pMsg->IndexFlag[0].HasIndex = false;

    msgMb->wr_ptr(msgLen);

    if (-1 == CAC_RT_Record_Server::instance()->send_msg(msgMb))
    {
        g_p_msg_buffer->free_mb(msgMb);
        msgMb = NULL;
        // 重入队列
        if(SVS_RESULT_FAILURE == enqueue_del_task(pDelRecord))
        {
            SVS_LOG((SVS_LM_ERROR,
                "[DEL]Deleting mesage enqueue failed.Delete record for prerecord."));
        }
        return;
    }

    return;
}

/*****************************************************************************
 函 数 名  : delete_file_normal
 功能描述  : 根据定时删除文件消息，具体删除文件。
             根据时间段，每一个文件周期调用一次删除函数。
             删除完毕后，清理数据库
 输入参数  : mb  --  消息块
 输出参数  :
 返 回 值  :
 修改历史  :
*****************************************************************************/
void CAC_Delete_Record_Manager::delete_file_normal(const ACE_Message_Block *mb) const
{
    SVS_TRACE();

    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Delete record for nomal failed.The parameter mb is NULL."));
        return;
    }

    SVS_DEL_RECORD_FILE *pDelRecordFile = (SVS_DEL_RECORD_FILE *)((void *)mb->rd_ptr());

    char szLensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {0};

    SetLocalizeDeviceId((uint8_t *)szLensId,
        sizeof(szLensId),
        (const char *)pDelRecordFile->LensId);

    SVS_LOG((SVS_LM_DEBUG,
        "[DEL]Begin to delete record file for normal, "
        "message type[%d], cameraId[%s], start time[%u], end time[%u], delete mode[%d].",
        pDelRecordFile->MsgType,
        pDelRecordFile->LensId,
        pDelRecordFile->StartTime,
        pDelRecordFile->EndTime,
        pDelRecordFile->DeleteMode));

    if ( DEL_RECORD_TYPE_NORMAL != pDelRecordFile->MsgType )
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Delete record for normal selecting failed."
            "Deleting type is invalid."
            "this deleting type [%d],right deleting type[%d].",
            pDelRecordFile->MsgType,
            DEL_RECORD_TYPE_NORMAL));

        return;
    }

    //删除录像文件
    time_t tmIndex = 0;
    for (   tmIndex = pDelRecordFile->StartTime;
            tmIndex < pDelRecordFile->EndTime;
            tmIndex += SECONDS_PER_HOUR)
    {
        //在删除时进行文件大小统计
        uint64_t ullDeleteFileSize = 0;

        int32_t iRet = delete_file( (char*)pDelRecordFile->LensId, tmIndex, ullDeleteFileSize);

        (void)send_record_delete_resp((char *)pDelRecordFile->LensId, tmIndex, ullDeleteFileSize, iRet);
    }

    return;
}

/*****************************************************************************
 函 数 名  : delete_file_prerecord
 功能描述  : 根据预录文件处理消息，具体删除预录文件
 输入参数  : mb  --  消息块
 输出参数  :
 返 回 值  :
 修改历史  :
*****************************************************************************/
void CAC_Delete_Record_Manager::delete_date_dir()const
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO,"[DEL]Begin to delete overdue date directory."));
    // 获取所有健康的磁盘路径
    DISK_INFO_VECTOR diskInfoVector;
    diskInfoVector.clear();
    (void)SVS_Disk_Manager::instance().getAllDiskInfo(diskInfoVector);

    // 添加索引文件根路径
    DISK_INFO tempDiskInfo;
    (void)memset(&tempDiskInfo,0 ,sizeof(tempDiskInfo));
    (void)memcpy(tempDiskInfo.DiskPath,
                 CAC_RT_Record_Server::instance()->get_record_indexfile_path(),
                 strlen(CAC_RT_Record_Server::instance()->get_record_indexfile_path()));

    diskInfoVector.push_back(tempDiskInfo);

    // 添加旧的索引文件路径,老索引文件路径可能不存在
    const char* pOldIndexPath = CAC_RT_Record_Server::instance()->get_old_index_path();
    uint32_t iOldIndexPathLen = strlen( pOldIndexPath );
    if ( iOldIndexPathLen > 0 )
    {
        (void)memset(&tempDiskInfo,0 ,sizeof(tempDiskInfo));
        (void)memcpy(tempDiskInfo.DiskPath, pOldIndexPath, iOldIndexPathLen );
         diskInfoVector.push_back(tempDiskInfo);
    }

    if (0 == diskInfoVector.size())
    {
        SVS_LOG((SVS_LM_WARNING,
            "[DEL]Delete record date directory finished.Healthy disk num is 0."));
        return;
    }

    // 获取没有掉链的磁盘路径
    uint32_t diskVectSize = diskInfoVector.size();
    SVS_LOG((SVS_LM_INFO, "[DEL]Start delete record date directory.disk number [%u].",diskVectSize));
    //char strLensDir[MAXNAMELEN];

    ACE_Message_Block* mb = NULL;
    PSVS_STORESVR_QUERY_LENS_MINTIME_REQ_MSG pMsg = NULL;
    uint32_t ulMsgLen;

    // 每个磁盘下都删除一遍空目录
    for (uint32_t diskIndex = 0; diskIndex < diskVectSize; diskIndex++)
    {
        // 1. 获取镜头目录列表
        VECTOR_DELETE_LENS_INFO lensIndexInfoVect;
        lensIndexInfoVect.clear();
        // 没有要删除的录像文件，直接返回
        if ((SVS_RESULT_OK !=  get_camera_dir_list(diskInfoVector[diskIndex].DiskPath,lensIndexInfoVect))
          || lensIndexInfoVect.empty())
        {
            continue;
        }

        // 查询每个镜头的索引最小值


        uint32_t lensIndex = 0;
        uint32_t lensVectSize = lensIndexInfoVect.size();
        // 向Store 发送查询消息
        ulMsgLen = sizeof(SVS_STORESVR_QUERY_LENS_MINTIME_REQ_MSG) + (lensVectSize - 1) * sizeof(SVS_FRONTDEV_ID);
        mb = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, ulMsgLen);
        if (NULL == mb)
        {
            SVS_LOG((SVS_LM_ERROR, "Alloc mb for query lens mintime fail."));
            continue;
        }
        pMsg = (PSVS_STORESVR_QUERY_LENS_MINTIME_REQ_MSG)(void*)mb->wr_ptr();

        FillCommonHeader(&pMsg->Header,
                                 SVS_MSG_TYPE_STORESVR_QUERY_LENS_MINTIME_REQ,
                                 CAC_RT_Record_Server::instance()->transaction_no(),
                                 (uint16_t)ulMsgLen);

        SetNLSDeviceId((uint8_t *)pMsg->NRUID,
                sizeof(pMsg->NRUID),
                CAC_RT_Record_Server::instance()->str_id());

        memcpy(pMsg->FilePath, diskInfoVector[diskIndex].DiskPath, sizeof(pMsg->FilePath));

        for(lensIndex = 0; lensIndex < lensVectSize; ++lensIndex)
        {
            memcpy(pMsg->LensId[lensIndex].ID, lensIndexInfoVect[lensIndex].LensID, DEVICEID_LEN);
        }

        pMsg->DeviceCount = lensIndex;

        mb->wr_ptr(ulMsgLen);

        if (-1 == CAC_RT_Record_Server::instance()->send_msg(mb))
        {
            g_p_msg_buffer->free_mb(mb);
            mb = NULL;
            continue;
        }

        SVS_LOG((SVS_LM_INFO,
            "[DEL]Send query lens mintime to StoreSvr succeeded. Disk path [%s]",
            diskInfoVector[diskIndex].DiskPath));
    }

    SVS_LOG((SVS_LM_INFO,"[DEL]Send all query lens mintime to StoreSvr succeeded."));
    return;
}

/*****************************************************************************
 函 数 名  : delete_file_prerecord
 功能描述  : 根据预录文件处理消息，具体删除预录文件
 输入参数  : mb  --  消息块
 输出参数  :
 返 回 值  :
 修改历史  :
*****************************************************************************/
void CAC_Delete_Record_Manager::delete_file_prerecord(const ACE_Message_Block *mb) const
{
    SVS_TRACE();

    if(NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Delete record file for prerecord failed, The parameter mb is NULL."));

        return;
    }

    SVS_DEL_RECORD_FILE *pDelRecordFile = (SVS_DEL_RECORD_FILE *)((void *)mb->rd_ptr());

    SVS_LOG((SVS_LM_DEBUG,
       "[DEL]Begin to delete record file for prerecord, "
       "message type[%d], cameraId[%s], start time[%u], end time[%u], delete mode[%d]",
        pDelRecordFile->MsgType,
        pDelRecordFile->LensId,
        pDelRecordFile->StartTime,
        pDelRecordFile->EndTime,
        pDelRecordFile->DeleteMode));

    if ( DEL_RECORD_TYPE_PRERECORD != pDelRecordFile->MsgType)
    {
         SVS_LOG((SVS_LM_ERROR,
            "[DEL]Delete record for prerecord failed, "
            "Deleting type is invalid, "
            "this deleting type [%d], right deleting type[%d].",
            pDelRecordFile->MsgType,
            DEL_RECORD_TYPE_PRERECORD));

        return;
    }

    //虽然只需要删除一个小时范围的，不过还是按照起始时间和结束事件循环来做
    time_t tmIndex = 0;
    for (   tmIndex = pDelRecordFile->StartTime;
            tmIndex < pDelRecordFile->EndTime;
            tmIndex += SECONDS_PER_HOUR )
    {
        //在删除时进行文件大小统计
        uint64_t ullDeleteFileSize = 0;
        (void)delete_file( (char*)pDelRecordFile->LensId, tmIndex, ullDeleteFileSize);
    }

    //删除预录文件不需要清理数据库

    return;
}

/*****************************************************************************
 函 数 名  : send_record_delete_resp
 功能描述  : 发送录像删除响应
             根据删除时间，构造索引文件路径
             根据索引文件路径，解析索引文件，获取录像文件的路径
             根据录像文件路径和录像文件名，删除录像文件
             删除索引文件
 输入参数  : tmTime  --  删除事件
 输出参数  :
 返 回 值  :
 修改历史  :
*****************************************************************************/
int32_t CAC_Delete_Record_Manager::send_record_delete_resp(
                                                char *pszCameraId,
                                                time_t tTime,
                                                uint64_t ulStoreSpace,
                                                int32_t iRespCode) const
{
    ACE_Message_Block *pRespMb = NULL;
    pRespMb = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, sizeof(SVS_STORESVR_DELETE_RECORD_RESP));
    if (NULL == pRespMb)
    {
        SVS_LOG((SVS_LM_ERROR, "Allocate mb for delete response failed."));
        return -1;
    }

    //填充消息体
    SVS_STORESVR_DELETE_RECORD_RESP *pRespMsg =
            (SVS_STORESVR_DELETE_RECORD_RESP *)(void *)pRespMb->rd_ptr();
    FillCommonHeader(pRespMsg->Header,
                SVS_MSG_TYPE_STORESVR_DELETE_RECORD_RESP,
                CAC_RT_Record_Server::instance()->transaction_no(),
                sizeof(SVS_STORESVR_DELETE_RECORD_RESP));
    SetNLSDeviceId((uint8_t *)pRespMsg->DeviceID,
                    sizeof(pRespMsg->DeviceID),
                    (const char *)pszCameraId);
    pRespMsg->StartTime = (uint32_t)tTime;
    pRespMsg->EndTime = (uint32_t)(tTime + SECONDS_PER_HOUR);
    pRespMsg->StoreSpace = ulStoreSpace;
    pRespMsg->RespCode = (uint32_t)iRespCode;
    pRespMb->wr_ptr(sizeof(SVS_STORESVR_DELETE_RECORD_RESP));

    int32_t iRet = CAC_RT_Record_Server::instance()->send_msg(pRespMb);
    if (0 != iRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Send record delete response message failed, "
            "camera[%s], delTime[%u].",
            pszCameraId, tTime));

        g_p_msg_buffer->free_mb(pRespMb);

        //考虑缓存
        return -1;
    }

    SVS_LOG((SVS_LM_INFO,
        "Send record delete response message to storesvr, "
        "camera[%s], time[%u], storeSpace[%Q], respCode[%d].",
        pszCameraId, tTime, ulStoreSpace, iRespCode));

    return 0;
}

/*****************************************************************************
 函 数 名  : delete_file
 功能描述  : 删除文件
             根据删除时间，构造索引文件路径
             根据索引文件路径，解析索引文件，获取录像文件的路径
             根据录像文件路径和录像文件名，删除录像文件
             删除索引文件
 输入参数  : tmTime  --  删除事件
 输出参数  :
 返 回 值  :
 修改历史  :
*****************************************************************************/
int32_t CAC_Delete_Record_Manager::delete_file(char* strLensId, time_t tmTime, uint64_t &ullFileSize) const
{
    SVS_TRACE();

    //生成文件名
    char strDateTime[TIME_STRING_LEN] = {0};
    char strTime[TIME_STRING_LEN] = {0};
    char strIndexFileName[TIME_STRING_LEN]={0};
    (void)SVS_SS_UTILITIES::time2str(strTime, sizeof(strTime), tmTime);

    (void)ACE_OS::memcpy( strDateTime, strTime, 8 );
    (void)ACE_OS::memcpy( strIndexFileName, strTime, 10 );

    // 获取索引根路径列表
    INDEX_FILE_VECTOR indexRootPathList;
    get_index_root_path_list(tmTime, indexRootPathList);

    uint32_t rootPathListCount = indexRootPathList.size();
    for(uint32_t uLoop = 0; uLoop <rootPathListCount; uLoop++)
    {
        // 生成索引文件目录全路径
        char strFullIndexFiledir[MAXPATHLEN]={0};
        (void)ACE_OS::snprintf(strFullIndexFiledir, sizeof(strFullIndexFiledir),
            "%s/%s/%s/",
//            CAC_RT_Record_Server::instance()->get_record_indexfile_path(),
            indexRootPathList[uLoop].indexfile,
            strLensId,
            strDateTime);

        //索取该小时的索引文件列表
        INDEX_FILE_VECTOR indexfileListVect;
        indexfileListVect.clear();
        (void)get_indexfile_list(strFullIndexFiledir,strIndexFileName,indexfileListVect);

        if(0 == indexfileListVect.size())
        {
            SVS_LOG((SVS_LM_WARNING,
                "[DEL]Get record index file number is 0."
                "index file name[%s%s].",
                strFullIndexFiledir,
                strIndexFileName));

            return STORESVR_DELETE_RECORD_INDEX_NOT_EXIST;
        }

        bool bFileExist = true;         // 标记是否有文件不存在

        // 删除所有索引文件中指定的录像文件路径
        for(uint32_t i = 0; i < indexfileListVect.size(); i++)
        {
            // 解析索引文件,获取录像文件路径
            BIG_RECORDFILE_PATH_VECTOR vecBigRecordFilePath;
            if ( SVS_RESULT_FAILURE == parse_all_big_recordfile_path(indexfileListVect[i].indexfile, vecBigRecordFilePath))
            {
                SVS_LOG((SVS_LM_ERROR,
                    "[DEL]Parse index file failed.index file name[%s]",
                    indexfileListVect[i].indexfile));

                return STORESVR_DELETE_RECORD_INDEX_FALSE;
            }

            // 根据从索引文件中获取到的路径生成文件全路径
            uint32_t nFilePathIndex = 0;
            for ( nFilePathIndex = 0; nFilePathIndex<vecBigRecordFilePath.size(); nFilePathIndex++)
            {
                SVS_LOG((SVS_LM_DEBUG,
                    "[DEL]Delete the record file succeeded.record file name[%s].",
                    vecBigRecordFilePath[nFilePathIndex].BigFilePath));

                // 当前录像文件大小
                struct stat64 st;
                if (0 != ::stat64(vecBigRecordFilePath[nFilePathIndex].BigFilePath, &st))
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Failed to open record file.Stat record file size failed ."
                        "record file name[%s].",
                        vecBigRecordFilePath[nFilePathIndex].BigFilePath));
                    //失败时为0
                    st.st_size = 0;//lint !e64 base type

                    // 只要有一个文件不存在, 标记为文件不存在
                    bFileExist = false;
                }

                ullFileSize += (uint64_t)(st.st_size / RTRECORD_UNIT_K);//lint !e55 base type

                (void)::unlink( vecBigRecordFilePath[nFilePathIndex].BigFilePath );
            }

            // 删除索引文件
            SVS_LOG((SVS_LM_INFO,
                "[DEL]Delete the record index file succeeded.record index file name[%s].",
                indexfileListVect[i].indexfile));

            (void)::unlink(indexfileListVect[i].indexfile);
        }

        // 文件不存在,设置删除大小为0
        if (!bFileExist)
        {
            ullFileSize = 0;
            return STORESVR_DELETE_RECORD_MEDIA_NOT_EXIST;
        }
    }

    return STORESVR_DELETE_RECORD_SUCCESS;
}

/*****************************************************************************
 函 数 名  : enqueue_del_task
 功能描述  : 把录像删除任务加入到队列中，所有删除任务总的任务队列
 输入参数  : const SVS_DEL_RECORD *del_record:录像删除任务
 输出参数  :
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Delete_Record_Manager::enqueue_del_task(const SVS_DEL_RECORD *del_record) const
{
    SVS_TRACE();
    if(NULL == del_record)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Deleting task enqueue failed.The parameter del_record is NULL."));
        return SVS_RESULT_FAILURE;
    }

    if(del_record->MsgType >= DEL_RECORD_TYPE_MAX)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Deleting task enqueue failed.message type is invalid.message type[%d].",
            del_record->MsgType));
        return SVS_RESULT_FAILURE;
    }

    ACE_Message_Block *mb = NULL;
    try
    {
        mb = new ACE_Message_Block(sizeof(SVS_DEL_RECORD));
    }
    catch(...)
    {
        delete mb;
        mb = NULL;
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Deleting task enqueue failed.Allocate memory failed."));
        return SVS_RESULT_FAILURE;
    }

    // 填充定时删除任务消息
    SVS_DEL_RECORD *pDelRecord = (SVS_DEL_RECORD *)((void *)mb->rd_ptr());
    pDelRecord->MsgType = del_record->MsgType;
    pDelRecord->DeleteTime = del_record->DeleteTime;

    SetLocalizeDeviceId((uint8_t *)pDelRecord->LensId,
        sizeof(pDelRecord->LensId),
        (const char *)del_record->LensId);

    ACE_Time_Value tvTimeout(ACE_OS::gettimeofday());
    tvTimeout.set(tvTimeout.sec(), tvTimeout.usec() + DEL_RECORD_TASK_ENQUEUE_TIMEOUT);
    int32_t enqueueRet = CAC_RT_Record_Server::instance()->enqueue_del_task(mb,&tvTimeout);
    if (-1 == enqueueRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Deleting task enqueue failed."));
        delete mb;
        mb = NULL;
    }

    return enqueueRet;//lint !e429 already enqueue
}//lint !e429 already enqueue

/*****************************************************************************
 函 数 名  : enqueue_del_record
 功能描述  : 经过处理的删除任务入队列，具体到每个镜头和时间段的任务，存在流控
 输入参数  : uint8_t msg_type:删除类型
             const uint8_t *lens_id:镜头ID
             time_t start_time:要被删除的录像的开始时间
             time_t end_time:要被删除录像的结束时间
             uint8_t delete_mode:删除类型是全部删除，还是select
 输出参数  :
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Delete_Record_Manager::enqueue_del_record( uint8_t msg_type,
                                        const uint8_t *lens_id,
                                        time_t start_time,
                                        time_t end_time,
                                        uint8_t delete_mode) const
{
    SVS_TRACE();

    if ( msg_type >= DEL_RECORD_TYPE_MAX )
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Create deleting record file message failed."
            "message type is invalid.message type[%d].",
            msg_type));
        return SVS_RESULT_FAILURE;
    }

    ACE_Message_Block *mb = NULL;
    try
    {
        mb = new ACE_Message_Block(sizeof(SVS_DEL_RECORD_FILE));
    }
    catch(...)
    {
        delete mb;
        mb = NULL;
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Create deleting record file message failed."
            "Allcate memory failed."));
        return SVS_RESULT_FAILURE;
    }

    // 填充定时删除任务消息
    SVS_DEL_RECORD_FILE *delRecordFile = (SVS_DEL_RECORD_FILE *)((void *)mb->rd_ptr());
    delRecordFile->MsgType = msg_type;
    if ( NULL != lens_id )
    {
        SetLocalizeDeviceId((uint8_t *)delRecordFile->LensId,
            sizeof(delRecordFile->LensId),
            (const char *)lens_id);
    }

    delRecordFile->StartTime  = start_time;
    delRecordFile->EndTime    = end_time;
    delRecordFile->DeleteMode = delete_mode;

    ACE_Time_Value tvTimeout(ACE_OS::gettimeofday());
    tvTimeout.set(tvTimeout.sec(), tvTimeout.usec() + DEL_RECORD_TASK_ENQUEUE_TIMEOUT);
    // 消息入队列
    int32_t enqueueRet = CAC_RT_Record_Server::instance()->enqueue_del_record(mb,&tvTimeout);
    if (-1 == enqueueRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Deleting record file message enqueue failed."));
        delete mb;
        mb = NULL;

        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;//lint !e429 already enqueue
}//lint !e429 already enqueue

/*****************************************************************************
 函 数 名  : delete_date_dir
 功能描述  : 根据入参(db中索引最小时间)，删除比做小索引所在的当前还早的日志目录
 输入参数  : const char * strLensDir:镜头级目录
             const time_t minTime: db中索引最小的时间
 输出参数  :
 返 回 值  : 成功返回0；失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Delete_Record_Manager::delete_date_dir(const char * strLensDir, const time_t minTime)const
{
    SVS_TRACE();
    if (NULL == strLensDir)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Deleting record date directory failed."
            "The parameter strLensDir is NULL."));
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "[DEL]Begin to deleting record date directory."
        "directory[%s],record miniaml index[%u] in database.",
        strLensDir,
        minTime));

    DIR* pDir = NULL;
    struct dirent *pDirent = NULL;
    char strBuffer[MAXNAMLEN];
    char strTime[TIME_STRING_LEN];
    bool isEmptyCameraDir = true; // 如果镜头目录为空，则删除镜头目录
    // 如果打开失败，镜头在该磁盘没有记录，直接返回
    if (NULL == (pDir = ::opendir(strLensDir)))
    {
        SVS_LOG((SVS_LM_ERROR, "Open dir fail when delete date dir. dir [%s].", strLensDir));
        return SVS_RESULT_OK;
    }

    while (NULL != (pDirent = ::readdir(pDir)))
    {
        if ( (0 == strcmp(pDirent->d_name, ".")) || (0 == strcmp(pDirent->d_name, "..")))
        {
            continue;
        }

        // 非目录，直接删除
        (void)snprintf(strBuffer, sizeof(strBuffer),
                           "%s/%s",
                           strLensDir,
                           pDirent->d_name);
        if(!is_directory(strBuffer))
        {
            SVS_LOG((SVS_LM_WARNING,
                "[DEL][%s] is not a record date directory ,so delete it.",
                strBuffer));
            (void)::unlink(strBuffer);
            continue;
        }

        // 非日期目录格式，直接删除
        if (RT_DATE_STR_LEN !=  strlen(pDirent->d_name))
        {
            SVS_LOG((SVS_LM_WARNING,
                "[DEL][%s/%s] is not a record date directory ,so delete it.",
                strLensDir,
                pDirent->d_name));
            (void)snprintf(strBuffer, sizeof(strBuffer),
                           "rm -rf %s/%s",
                           strLensDir,
                           pDirent->d_name);
            (void)system(strBuffer);
            continue;
        }

        //  转换成时间,如果小于当天最小
        (void)memset(strTime, 0 ,sizeof(strTime));
        (void)strncat(strTime, pDirent->d_name, RT_DATE_STR_LEN);
        (void)strcat(strTime,"235959"); // 拼接成当天的 23:59:59
        if (SVS_SS_UTILITIES::str2time(strTime) < minTime)
        {
            SVS_LOG((SVS_LM_WARNING,
                "[DEL][%s/%s] is overdue,so delete it.",
                strLensDir,
                pDirent->d_name));
            (void)snprintf(strBuffer, sizeof(strBuffer),
                           "rm -rf %s/%s",
                           strLensDir,
                           pDirent->d_name);
            (void)system(strBuffer);
        }
        else // 存在需要保留的日期目录，则镜头目录不为空
        {
            isEmptyCameraDir = false;
        }
    }

    // 关闭目录
    (void)::closedir(pDir);

    // 如果镜头目录为空，则删除之
    if (isEmptyCameraDir)
    {
        SVS_LOG((SVS_LM_INFO,
            "[DEL]Camera directory is empty, so delete it.camera dir[%s]",
            strLensDir));
        (void)snprintf(strBuffer, sizeof(strBuffer),
                           "rm -rf %s",
                           strLensDir);
        (void)system(strBuffer);
    }

    return SVS_RESULT_OK;
}

int32_t CAC_Delete_Record_Manager::get_camera_dir_list(const char* strDiskPath,
                                             VECTOR_DELETE_LENS_INFO& CameraVect)const
{
    if(NULL == strDiskPath)
    {
        SVS_LOG((SVS_LM_WARNING,
            "[DEL]Get camera directory list failed.The parameter is invalid.strDiskPath[0x%08x]",
            strDiskPath));

        return SVS_ERR_PARAM;
    }

    DIR*  pDir = NULL;
    struct dirent *pDirent = NULL;
    // 如果打开失败，返回
    if (NULL == (pDir = ::opendir(strDiskPath)))
    {
        SVS_LOG((SVS_LM_WARNING,
            "[DEL]Get camera directory list failed.Open disk directory failed.strDiskPath[%s]",
            strDiskPath));

        return SVS_ERR_PARAM;
    }

    char strBuffer[MAXNAMLEN];
    SVS_DELETE_LENS_INFO szCameraInfo;
    // 每次最大镜头数为满规格，防止该路径下镜头数过多，本次先处理满规格的镜头数，下次再处理其他镜头
    uint32_t ulLenDirNum = 0;
    while (NULL != (pDirent = ::readdir(pDir)))
    {
        if ( (0 == strcmp(pDirent->d_name, ".")) || (0 == strcmp(pDirent->d_name, "..")))
        {
            continue;
        }

        // 非目录，打印告警日志
        (void)snprintf(strBuffer, sizeof(strBuffer),
                           "%s/%s",
                           strDiskPath,
                           pDirent->d_name);
        if(!is_directory(strBuffer))
        {
            SVS_LOG((SVS_LM_WARNING,
                "[DEL][%s] is not a record directory on record disk.",
                strBuffer));
//            (void)::unlink(strBuffer);
            continue;
        }

        // 非镜头目录长度，打印告警日志
        if (CHECK_ERROR == CheckDeviceIDEx(pDirent->d_name))
        //if (DEVICEID_LEN !=  strlen(pDirent->d_name))
        {
            SVS_LOG((SVS_LM_WARNING,
                "[DEL][%s/%s] is not a camera directory on record disk.",
                strDiskPath,
                pDirent->d_name));
//
//            (void)snprintf(strBuffer, sizeof(strBuffer),
//                           "rm -rf %s/%s",
//                           strDiskPath,
//                           pDirent->d_name);
//            (void)system(strBuffer);

            continue;
        }

        // 记录镜头目录
        memset(&szCameraInfo, 0 , sizeof(szCameraInfo));

        SetLocalizeDeviceId((uint8_t *)szCameraInfo.LensID,
            sizeof(szCameraInfo.LensID),
            pDirent->d_name);

        CameraVect.push_back(szCameraInfo);
        ++ulLenDirNum;
        if(RECORD_MAX_LENS_NUM <= ulLenDirNum)
        {
            SVS_LOG((SVS_LM_WARNING,
                "[DEL]The number of camera path in this disk path reached maximal value,"
                " so end parsing.strDiskPath[%s] , ulLenDirNum[%u], RECORD_MAX_LENS_NUM[%d].",
                strDiskPath,
                ulLenDirNum,
                RECORD_MAX_LENS_NUM));

            break;
        }

    }

    // 关闭目录
    (void)::closedir(pDir);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : is_directory
 功能描述  : 判断是否为目录
 输入参数  : const char *strDir :目录全路径
 输出参数  :
 返 回 值  : 是目录:true,不是目录:false
 修改历史  :
*****************************************************************************/
bool CAC_Delete_Record_Manager::is_directory(const char *strDir) const
{
    SVS_TRACE();
    struct stat64 sstat;
    if(0 != stat64(strDir, &sstat))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Stat dirrectory failed.directory[%s],",
            strDir));
        return false;
    }

    if(0 != (S_IFDIR & sstat.st_mode))
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*****************************************************************************
 函 数 名  : get_indexfile_list
 功能描述  : 获取当前这个小时的索引文件列表，该小时索引文件可能存在多个，分别为
             XXXXXXXXXXXX.idx, XXXXXXXXXXXX.idx.1 ... XXXXXXXXXXXX.idx.n
 输入参数  : const char* strIndexfileDir:索引文件日志目录级
             const char* strIndexfileSub索引文件公共字串，型如:XXXXXXXXXXXX.idx
 输出参数  : INDEX_FILE_VECTOR &indexfileListVct:索引文件列表
 返 回 值  : 成功返回 0,失败返回 -1
 修改历史  :
*****************************************************************************/
int32_t CAC_Delete_Record_Manager::get_indexfile_list(const char* strIndexfileDir,
                            const char* strIndexfileSub,
                            INDEX_FILE_VECTOR &indexfileListVct)const
{
    SVS_TRACE();
    // 入参合法性检查
    if((NULL == strIndexfileDir) ||(NULL == strIndexfileSub))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Failed to get index file list."
            "The parameter indexfineDir or indexfileSubstr is NULL."));
        return SVS_RESULT_FAILURE;
    }

    // 打开主机录像的索引文件
    del_indexfile_name_t indexFileTemp;
    struct stat st;
    memset(&indexFileTemp, 0 , sizeof(indexFileTemp));
    (void)snprintf(indexFileTemp.indexfile,
                        MAXNAMELEN,
                        "%s%s.idx",
                        strIndexfileDir,
                        strIndexfileSub);
    if(0 == stat(indexFileTemp.indexfile, &st))
    {
        indexfileListVct.push_back(indexFileTemp);
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,
            "[DEL]Stat record index file failed.The index file does not exist maybe."
            "record index file name[%s]",
            indexFileTemp.indexfile));
    }

    // 打开小时目录，找备机索引文件列表
    char strBakupIndexfileDir[MAXPATHLEN] = {0,};
    (void)snprintf(strBakupIndexfileDir,
                        MAXNAMELEN,
                        "%s%s/",
                        strIndexfileDir,
                        strIndexfileSub);
    DIR * pdir;
    struct dirent * pdirent;
    pdir = opendir(strBakupIndexfileDir);
    // 如果打开备份目录失败，表示没有备份，直接返回
    if (NULL == pdir)
    {
        return SVS_RESULT_OK;
    }

    // 继续解析备份目录中的索引文件
    SVS_LOG((SVS_LM_INFO,
        "[DEL]Open bakup index file directory succeeded.directory[%s].",
        strBakupIndexfileDir));

    while(NULL != (pdirent = readdir(pdir)))
    {
        // 如果是上级目录或是当前目录
        if((0 == strcmp("." , pdirent->d_name))
        || (0 == strcmp(".." ,pdirent->d_name)))
        {
            continue;
        }

        // 如果不是当前这个小时的录像文件
        if (NULL == strstr(pdirent->d_name, strIndexfileSub))
        {
            continue;
        }

        // 把该索引文件添加到索引列表中
        (void)snprintf(indexFileTemp.indexfile,
                        MAXNAMELEN,
                        "%s%s",
                        strBakupIndexfileDir,
                        pdirent->d_name);
        indexfileListVct.push_back(indexFileTemp);
    }

    // 关闭目录
    (void)closedir(pdir);
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : get_index_root_path_list
 功能描述  : 录像索引路径可能存在多个，可以有升级前、升级后两个索引路径；
             根据传入的时间跟索引升级时间相比较，判断要找的路径可能存在的索引路径
 输入参数  : time_t delTime:录像时间
 输出参数  : INDEX_FILE_VECTOR& indexRootPathList:索引路径列表
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
void CAC_Delete_Record_Manager::get_index_root_path_list(time_t delTime,
                                    INDEX_FILE_VECTOR& indexRootPathList)const
{
    // 把升级时间转换成整点
    time_t indexPathUpdateTime = CAC_RT_Record_Server::instance()->get_index_path_update_time();
    indexPathUpdateTime = indexPathUpdateTime - (indexPathUpdateTime % SECONDS_PER_HOUR);

    indexRootPathList.clear();
    del_indexfile_name_t szIndexRootPath;
    if(delTime > indexPathUpdateTime)
    {
        // 如果要查找的录像时间大于升级时间,则在升级后的索引路径中找录像索引
        memset(&szIndexRootPath, 0, sizeof(szIndexRootPath));
        strcpy(szIndexRootPath.indexfile, CAC_RT_Record_Server::instance()->get_record_indexfile_path());
        indexRootPathList.push_back(szIndexRootPath);
    }
    else if(delTime < indexPathUpdateTime)
    {
        // 如果要查找的录像时间小于升级时间,则在旧的索引路径中找录像索引
        memset(&szIndexRootPath, 0, sizeof(szIndexRootPath));
        strcpy(szIndexRootPath.indexfile, CAC_RT_Record_Server::instance()->get_old_index_path());
        indexRootPathList.push_back(szIndexRootPath);
    }
    else
    {
        // 如果要查找的录像时间等于升级时间,则是升级时的那个小时，在两个索引路径中都有录像
        memset(&szIndexRootPath, 0, sizeof(szIndexRootPath));
        strcpy(szIndexRootPath.indexfile, CAC_RT_Record_Server::instance()->get_record_indexfile_path());
        indexRootPathList.push_back(szIndexRootPath);

        memset(&szIndexRootPath, 0, sizeof(szIndexRootPath));
        strcpy(szIndexRootPath.indexfile, CAC_RT_Record_Server::instance()->get_old_index_path());

        indexRootPathList.push_back(szIndexRootPath);
    }

    return;
}


