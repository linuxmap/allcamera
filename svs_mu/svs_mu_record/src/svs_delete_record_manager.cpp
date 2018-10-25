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
 �� �� ��  : handle_del_record
 ��������  : ����ɾ��������Ϣ�����е���Ϣ
             ���ݲ�ͬ����Ϣ����,���ò�ͬ�Ĵ�����
 �������  : mb  --  ��Ϣ��
 �������  :
 �� �� ֵ  :
 �޸���ʷ  :
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
        //ɾ��Ԥ¼�ļ���Ϣ
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
 �� �� ��  : handle_del_record_file
 ��������  : ����ʱɾ���ʹ�����ɾ����Ϣ�����е���Ϣ
             ���ݲ�ͬ����Ϣ���ͣ����ò�ͬ�Ĵ�����
 �������  : mb  --  ��Ϣ��
 �������  :
 �� �� ֵ  :
 �޸���ʷ  :
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
 �� �� ��  : handle_del_prerecord_file
 ��������  : ����ɾ��Ԥ¼�ļ���Ϣ����
 �������  : mb  --  ��Ϣ��
 �������  :
 �� �� ֵ  :
 �޸���ʷ  :
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
 �� �� ��  : handle_del_prerecord_file
 ��������  : ����ɾ��Ԥ¼�ļ���Ϣ����
 �������  : mb  --  ��Ϣ��
 �������  :
 �� �� ֵ  :
 �޸���ʷ  :
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

            // �жϾ�ͷID�Ƿ�Ϸ�
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
 �� �� ��  : handle_message_prerecord
 ��������  : ����ɾ��Ԥ¼�ļ���Ϣ
             ������:
               1������ɾ��ʱ��������Ҫɾ����ʱ���
               2������ʱ��ι���SQL��䣺
                  SELECT count(*)
                  FROM tbl_recordfile_outline_info A
                  WHERE A.Device_Id='00000000010100000299'
                  AND A.Server_Id='00000001106020000001'
                  AND A.Start_Time>=TO_DATE('����Ľ���ʱ��', 'YYYYMMDDHH24MISS')
                  AND A.End_Time<=TO_DATE('����Ŀ�ʼʱ��', 'YYYYMMDDHH24MISS')
                  �����Ƿ���ڸþ�ͷ�ĸ�ʱ��εļ�¼��

                  ��Ҫ���������ʱ�仹������¼���ֶ�¼��
                  ����tbl_recordfile_outline_info���в�ѯ����

               3����������ڣ����ݾ�ͷID����ʼʱ��ͽ���ʱ�乹��ɾ����Ϣ��
                  ����DeleteRecordQueue�����С�
 �������  : mb  --  ��Ϣ��
 �������  :
 �� �� ֵ  :
 �޸���ʷ  :
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

    // �жϵ�ǰʱ���Ƿ񳬹������Ԥ¼ʱ��,���û�г���,��Ϣ�ض��м����ȴ�����
    // �����ж�����Ϊ���ԭRt��������Ԥ¼�ļ�ɾ����Ϣ,�ٵȴ�Ԥ¼ʱ����Del�Ķ���,
    // �ڵȴ������ʱ����,���Ԥ¼ʱ�䱻�Ķ���, Rt���޷�����Ϣ����Del�Ķ���
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
        // ���Ԥ¼����
        lensParam.BeforeSeconds = SVS_PRORECORD_MAX_TIME;
    }

    if ( time(NULL) <= (ADJUST_FORWARD(pDelRecord->DeleteTime, SECONDS_PER_HOUR) + (time_t)lensParam.BeforeSeconds))
    {
        SVS_LOG((SVS_LM_WARNING,
            "[DEL]Current time is smaller than the deleting time."
            "Delete record for prerecord.Enqueue the task again.delete time[%d].",
            pDelRecord->DeleteTime));
        // �������
        if(SVS_RESULT_FAILURE == enqueue_del_task(pDelRecord))
        {
            SVS_LOG((SVS_LM_ERROR,
                "[DEL]Deleting mesage enqueue failed.Delete record for prerecord."));
        }
        return;
    }

    // ����ɾ��ʱ��������ʼʱ��ͽ���ʱ��
    // �����ʱ�����ɾ��ʱ���ǰһ��Сʱ��ʱ���
    time_t tmStart = ADJUST_FORWARD(pDelRecord->DeleteTime, SECONDS_PER_HOUR) - SECONDS_PER_HOUR;
    time_t tmEnd = ADJUST_FORWARD(pDelRecord->DeleteTime, SECONDS_PER_HOUR);

    // ��Store��ѯ��ʱ������Ƿ���������¼
    // Ŀǰһ��ֻ��һ����ͷ
    ACE_Message_Block *msgMb = NULL;
    PSVS_STORESVR_QUERY_INDEX_REQ_MSG pMsg = NULL;
    uint32_t msgLen = sizeof(SVS_STORESVR_QUERY_INDEX_REQ_MSG);
    msgMb = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG , msgLen);
    if (NULL == msgMb)
    {
        // �������
        if(SVS_RESULT_FAILURE == enqueue_del_task(pDelRecord))
        {
            SVS_LOG((SVS_LM_ERROR,
                "[DEL]Deleting mesage enqueue failed.Delete record for prerecord."));
        }
        return;
    }

    pMsg = (PSVS_STORESVR_QUERY_INDEX_REQ_MSG)(void*)msgMb->wr_ptr();
     // ��װ��Ϣͷ
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
        // �������
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
 �� �� ��  : delete_file_normal
 ��������  : ���ݶ�ʱɾ���ļ���Ϣ������ɾ���ļ���
             ����ʱ��Σ�ÿһ���ļ����ڵ���һ��ɾ��������
             ɾ����Ϻ��������ݿ�
 �������  : mb  --  ��Ϣ��
 �������  :
 �� �� ֵ  :
 �޸���ʷ  :
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

    //ɾ��¼���ļ�
    time_t tmIndex = 0;
    for (   tmIndex = pDelRecordFile->StartTime;
            tmIndex < pDelRecordFile->EndTime;
            tmIndex += SECONDS_PER_HOUR)
    {
        //��ɾ��ʱ�����ļ���Сͳ��
        uint64_t ullDeleteFileSize = 0;

        int32_t iRet = delete_file( (char*)pDelRecordFile->LensId, tmIndex, ullDeleteFileSize);

        (void)send_record_delete_resp((char *)pDelRecordFile->LensId, tmIndex, ullDeleteFileSize, iRet);
    }

    return;
}

/*****************************************************************************
 �� �� ��  : delete_file_prerecord
 ��������  : ����Ԥ¼�ļ�������Ϣ������ɾ��Ԥ¼�ļ�
 �������  : mb  --  ��Ϣ��
 �������  :
 �� �� ֵ  :
 �޸���ʷ  :
*****************************************************************************/
void CAC_Delete_Record_Manager::delete_date_dir()const
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO,"[DEL]Begin to delete overdue date directory."));
    // ��ȡ���н����Ĵ���·��
    DISK_INFO_VECTOR diskInfoVector;
    diskInfoVector.clear();
    (void)SVS_Disk_Manager::instance().getAllDiskInfo(diskInfoVector);

    // ��������ļ���·��
    DISK_INFO tempDiskInfo;
    (void)memset(&tempDiskInfo,0 ,sizeof(tempDiskInfo));
    (void)memcpy(tempDiskInfo.DiskPath,
                 CAC_RT_Record_Server::instance()->get_record_indexfile_path(),
                 strlen(CAC_RT_Record_Server::instance()->get_record_indexfile_path()));

    diskInfoVector.push_back(tempDiskInfo);

    // ��Ӿɵ������ļ�·��,�������ļ�·�����ܲ�����
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

    // ��ȡû�е����Ĵ���·��
    uint32_t diskVectSize = diskInfoVector.size();
    SVS_LOG((SVS_LM_INFO, "[DEL]Start delete record date directory.disk number [%u].",diskVectSize));
    //char strLensDir[MAXNAMELEN];

    ACE_Message_Block* mb = NULL;
    PSVS_STORESVR_QUERY_LENS_MINTIME_REQ_MSG pMsg = NULL;
    uint32_t ulMsgLen;

    // ÿ�������¶�ɾ��һ���Ŀ¼
    for (uint32_t diskIndex = 0; diskIndex < diskVectSize; diskIndex++)
    {
        // 1. ��ȡ��ͷĿ¼�б�
        VECTOR_DELETE_LENS_INFO lensIndexInfoVect;
        lensIndexInfoVect.clear();
        // û��Ҫɾ����¼���ļ���ֱ�ӷ���
        if ((SVS_RESULT_OK !=  get_camera_dir_list(diskInfoVector[diskIndex].DiskPath,lensIndexInfoVect))
          || lensIndexInfoVect.empty())
        {
            continue;
        }

        // ��ѯÿ����ͷ��������Сֵ


        uint32_t lensIndex = 0;
        uint32_t lensVectSize = lensIndexInfoVect.size();
        // ��Store ���Ͳ�ѯ��Ϣ
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
 �� �� ��  : delete_file_prerecord
 ��������  : ����Ԥ¼�ļ�������Ϣ������ɾ��Ԥ¼�ļ�
 �������  : mb  --  ��Ϣ��
 �������  :
 �� �� ֵ  :
 �޸���ʷ  :
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

    //��Ȼֻ��Ҫɾ��һ��Сʱ��Χ�ģ��������ǰ�����ʼʱ��ͽ����¼�ѭ������
    time_t tmIndex = 0;
    for (   tmIndex = pDelRecordFile->StartTime;
            tmIndex < pDelRecordFile->EndTime;
            tmIndex += SECONDS_PER_HOUR )
    {
        //��ɾ��ʱ�����ļ���Сͳ��
        uint64_t ullDeleteFileSize = 0;
        (void)delete_file( (char*)pDelRecordFile->LensId, tmIndex, ullDeleteFileSize);
    }

    //ɾ��Ԥ¼�ļ�����Ҫ�������ݿ�

    return;
}

/*****************************************************************************
 �� �� ��  : send_record_delete_resp
 ��������  : ����¼��ɾ����Ӧ
             ����ɾ��ʱ�䣬���������ļ�·��
             ���������ļ�·�������������ļ�����ȡ¼���ļ���·��
             ����¼���ļ�·����¼���ļ�����ɾ��¼���ļ�
             ɾ�������ļ�
 �������  : tmTime  --  ɾ���¼�
 �������  :
 �� �� ֵ  :
 �޸���ʷ  :
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

    //�����Ϣ��
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

        //���ǻ���
        return -1;
    }

    SVS_LOG((SVS_LM_INFO,
        "Send record delete response message to storesvr, "
        "camera[%s], time[%u], storeSpace[%Q], respCode[%d].",
        pszCameraId, tTime, ulStoreSpace, iRespCode));

    return 0;
}

/*****************************************************************************
 �� �� ��  : delete_file
 ��������  : ɾ���ļ�
             ����ɾ��ʱ�䣬���������ļ�·��
             ���������ļ�·�������������ļ�����ȡ¼���ļ���·��
             ����¼���ļ�·����¼���ļ�����ɾ��¼���ļ�
             ɾ�������ļ�
 �������  : tmTime  --  ɾ���¼�
 �������  :
 �� �� ֵ  :
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Delete_Record_Manager::delete_file(char* strLensId, time_t tmTime, uint64_t &ullFileSize) const
{
    SVS_TRACE();

    //�����ļ���
    char strDateTime[TIME_STRING_LEN] = {0};
    char strTime[TIME_STRING_LEN] = {0};
    char strIndexFileName[TIME_STRING_LEN]={0};
    (void)SVS_SS_UTILITIES::time2str(strTime, sizeof(strTime), tmTime);

    (void)ACE_OS::memcpy( strDateTime, strTime, 8 );
    (void)ACE_OS::memcpy( strIndexFileName, strTime, 10 );

    // ��ȡ������·���б�
    INDEX_FILE_VECTOR indexRootPathList;
    get_index_root_path_list(tmTime, indexRootPathList);

    uint32_t rootPathListCount = indexRootPathList.size();
    for(uint32_t uLoop = 0; uLoop <rootPathListCount; uLoop++)
    {
        // ���������ļ�Ŀ¼ȫ·��
        char strFullIndexFiledir[MAXPATHLEN]={0};
        (void)ACE_OS::snprintf(strFullIndexFiledir, sizeof(strFullIndexFiledir),
            "%s/%s/%s/",
//            CAC_RT_Record_Server::instance()->get_record_indexfile_path(),
            indexRootPathList[uLoop].indexfile,
            strLensId,
            strDateTime);

        //��ȡ��Сʱ�������ļ��б�
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

        bool bFileExist = true;         // ����Ƿ����ļ�������

        // ɾ�����������ļ���ָ����¼���ļ�·��
        for(uint32_t i = 0; i < indexfileListVect.size(); i++)
        {
            // ���������ļ�,��ȡ¼���ļ�·��
            BIG_RECORDFILE_PATH_VECTOR vecBigRecordFilePath;
            if ( SVS_RESULT_FAILURE == parse_all_big_recordfile_path(indexfileListVect[i].indexfile, vecBigRecordFilePath))
            {
                SVS_LOG((SVS_LM_ERROR,
                    "[DEL]Parse index file failed.index file name[%s]",
                    indexfileListVect[i].indexfile));

                return STORESVR_DELETE_RECORD_INDEX_FALSE;
            }

            // ���ݴ������ļ��л�ȡ����·�������ļ�ȫ·��
            uint32_t nFilePathIndex = 0;
            for ( nFilePathIndex = 0; nFilePathIndex<vecBigRecordFilePath.size(); nFilePathIndex++)
            {
                SVS_LOG((SVS_LM_DEBUG,
                    "[DEL]Delete the record file succeeded.record file name[%s].",
                    vecBigRecordFilePath[nFilePathIndex].BigFilePath));

                // ��ǰ¼���ļ���С
                struct stat64 st;
                if (0 != ::stat64(vecBigRecordFilePath[nFilePathIndex].BigFilePath, &st))
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Failed to open record file.Stat record file size failed ."
                        "record file name[%s].",
                        vecBigRecordFilePath[nFilePathIndex].BigFilePath));
                    //ʧ��ʱΪ0
                    st.st_size = 0;//lint !e64 base type

                    // ֻҪ��һ���ļ�������, ���Ϊ�ļ�������
                    bFileExist = false;
                }

                ullFileSize += (uint64_t)(st.st_size / RTRECORD_UNIT_K);//lint !e55 base type

                (void)::unlink( vecBigRecordFilePath[nFilePathIndex].BigFilePath );
            }

            // ɾ�������ļ�
            SVS_LOG((SVS_LM_INFO,
                "[DEL]Delete the record index file succeeded.record index file name[%s].",
                indexfileListVect[i].indexfile));

            (void)::unlink(indexfileListVect[i].indexfile);
        }

        // �ļ�������,����ɾ����СΪ0
        if (!bFileExist)
        {
            ullFileSize = 0;
            return STORESVR_DELETE_RECORD_MEDIA_NOT_EXIST;
        }
    }

    return STORESVR_DELETE_RECORD_SUCCESS;
}

/*****************************************************************************
 �� �� ��  : enqueue_del_task
 ��������  : ��¼��ɾ��������뵽�����У�����ɾ�������ܵ��������
 �������  : const SVS_DEL_RECORD *del_record:¼��ɾ������
 �������  :
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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

    // ��䶨ʱɾ��������Ϣ
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
 �� �� ��  : enqueue_del_record
 ��������  : ���������ɾ����������У����嵽ÿ����ͷ��ʱ��ε����񣬴�������
 �������  : uint8_t msg_type:ɾ������
             const uint8_t *lens_id:��ͷID
             time_t start_time:Ҫ��ɾ����¼��Ŀ�ʼʱ��
             time_t end_time:Ҫ��ɾ��¼��Ľ���ʱ��
             uint8_t delete_mode:ɾ��������ȫ��ɾ��������select
 �������  :
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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

    // ��䶨ʱɾ��������Ϣ
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
    // ��Ϣ�����
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
 �� �� ��  : delete_date_dir
 ��������  : �������(db��������Сʱ��)��ɾ������С�������ڵĵ�ǰ�������־Ŀ¼
 �������  : const char * strLensDir:��ͷ��Ŀ¼
             const time_t minTime: db��������С��ʱ��
 �������  :
 �� �� ֵ  : �ɹ�����0��ʧ�ܷ���-1
 �޸���ʷ  :
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
    bool isEmptyCameraDir = true; // �����ͷĿ¼Ϊ�գ���ɾ����ͷĿ¼
    // �����ʧ�ܣ���ͷ�ڸô���û�м�¼��ֱ�ӷ���
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

        // ��Ŀ¼��ֱ��ɾ��
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

        // ������Ŀ¼��ʽ��ֱ��ɾ��
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

        //  ת����ʱ��,���С�ڵ�����С
        (void)memset(strTime, 0 ,sizeof(strTime));
        (void)strncat(strTime, pDirent->d_name, RT_DATE_STR_LEN);
        (void)strcat(strTime,"235959"); // ƴ�ӳɵ���� 23:59:59
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
        else // ������Ҫ����������Ŀ¼����ͷĿ¼��Ϊ��
        {
            isEmptyCameraDir = false;
        }
    }

    // �ر�Ŀ¼
    (void)::closedir(pDir);

    // �����ͷĿ¼Ϊ�գ���ɾ��֮
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
    // �����ʧ�ܣ�����
    if (NULL == (pDir = ::opendir(strDiskPath)))
    {
        SVS_LOG((SVS_LM_WARNING,
            "[DEL]Get camera directory list failed.Open disk directory failed.strDiskPath[%s]",
            strDiskPath));

        return SVS_ERR_PARAM;
    }

    char strBuffer[MAXNAMLEN];
    SVS_DELETE_LENS_INFO szCameraInfo;
    // ÿ�����ͷ��Ϊ����񣬷�ֹ��·���¾�ͷ�����࣬�����ȴ��������ľ�ͷ�����´��ٴ���������ͷ
    uint32_t ulLenDirNum = 0;
    while (NULL != (pDirent = ::readdir(pDir)))
    {
        if ( (0 == strcmp(pDirent->d_name, ".")) || (0 == strcmp(pDirent->d_name, "..")))
        {
            continue;
        }

        // ��Ŀ¼����ӡ�澯��־
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

        // �Ǿ�ͷĿ¼���ȣ���ӡ�澯��־
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

        // ��¼��ͷĿ¼
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

    // �ر�Ŀ¼
    (void)::closedir(pDir);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : is_directory
 ��������  : �ж��Ƿ�ΪĿ¼
 �������  : const char *strDir :Ŀ¼ȫ·��
 �������  :
 �� �� ֵ  : ��Ŀ¼:true,����Ŀ¼:false
 �޸���ʷ  :
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
 �� �� ��  : get_indexfile_list
 ��������  : ��ȡ��ǰ���Сʱ�������ļ��б���Сʱ�����ļ����ܴ��ڶ�����ֱ�Ϊ
             XXXXXXXXXXXX.idx, XXXXXXXXXXXX.idx.1 ... XXXXXXXXXXXX.idx.n
 �������  : const char* strIndexfileDir:�����ļ���־Ŀ¼��
             const char* strIndexfileSub�����ļ������ִ�������:XXXXXXXXXXXX.idx
 �������  : INDEX_FILE_VECTOR &indexfileListVct:�����ļ��б�
 �� �� ֵ  : �ɹ����� 0,ʧ�ܷ��� -1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Delete_Record_Manager::get_indexfile_list(const char* strIndexfileDir,
                            const char* strIndexfileSub,
                            INDEX_FILE_VECTOR &indexfileListVct)const
{
    SVS_TRACE();
    // ��κϷ��Լ��
    if((NULL == strIndexfileDir) ||(NULL == strIndexfileSub))
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEL]Failed to get index file list."
            "The parameter indexfineDir or indexfileSubstr is NULL."));
        return SVS_RESULT_FAILURE;
    }

    // ������¼��������ļ�
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

    // ��СʱĿ¼���ұ��������ļ��б�
    char strBakupIndexfileDir[MAXPATHLEN] = {0,};
    (void)snprintf(strBakupIndexfileDir,
                        MAXNAMELEN,
                        "%s%s/",
                        strIndexfileDir,
                        strIndexfileSub);
    DIR * pdir;
    struct dirent * pdirent;
    pdir = opendir(strBakupIndexfileDir);
    // ����򿪱���Ŀ¼ʧ�ܣ���ʾû�б��ݣ�ֱ�ӷ���
    if (NULL == pdir)
    {
        return SVS_RESULT_OK;
    }

    // ������������Ŀ¼�е������ļ�
    SVS_LOG((SVS_LM_INFO,
        "[DEL]Open bakup index file directory succeeded.directory[%s].",
        strBakupIndexfileDir));

    while(NULL != (pdirent = readdir(pdir)))
    {
        // ������ϼ�Ŀ¼���ǵ�ǰĿ¼
        if((0 == strcmp("." , pdirent->d_name))
        || (0 == strcmp(".." ,pdirent->d_name)))
        {
            continue;
        }

        // ������ǵ�ǰ���Сʱ��¼���ļ�
        if (NULL == strstr(pdirent->d_name, strIndexfileSub))
        {
            continue;
        }

        // �Ѹ������ļ���ӵ������б���
        (void)snprintf(indexFileTemp.indexfile,
                        MAXNAMELEN,
                        "%s%s",
                        strBakupIndexfileDir,
                        pdirent->d_name);
        indexfileListVct.push_back(indexFileTemp);
    }

    // �ر�Ŀ¼
    (void)closedir(pdir);
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : get_index_root_path_list
 ��������  : ¼������·�����ܴ��ڶ��������������ǰ����������������·����
             ���ݴ����ʱ�����������ʱ����Ƚϣ��ж�Ҫ�ҵ�·�����ܴ��ڵ�����·��
 �������  : time_t delTime:¼��ʱ��
 �������  : INDEX_FILE_VECTOR& indexRootPathList:����·���б�
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
void CAC_Delete_Record_Manager::get_index_root_path_list(time_t delTime,
                                    INDEX_FILE_VECTOR& indexRootPathList)const
{
    // ������ʱ��ת��������
    time_t indexPathUpdateTime = CAC_RT_Record_Server::instance()->get_index_path_update_time();
    indexPathUpdateTime = indexPathUpdateTime - (indexPathUpdateTime % SECONDS_PER_HOUR);

    indexRootPathList.clear();
    del_indexfile_name_t szIndexRootPath;
    if(delTime > indexPathUpdateTime)
    {
        // ���Ҫ���ҵ�¼��ʱ���������ʱ��,���������������·������¼������
        memset(&szIndexRootPath, 0, sizeof(szIndexRootPath));
        strcpy(szIndexRootPath.indexfile, CAC_RT_Record_Server::instance()->get_record_indexfile_path());
        indexRootPathList.push_back(szIndexRootPath);
    }
    else if(delTime < indexPathUpdateTime)
    {
        // ���Ҫ���ҵ�¼��ʱ��С������ʱ��,���ھɵ�����·������¼������
        memset(&szIndexRootPath, 0, sizeof(szIndexRootPath));
        strcpy(szIndexRootPath.indexfile, CAC_RT_Record_Server::instance()->get_old_index_path());
        indexRootPathList.push_back(szIndexRootPath);
    }
    else
    {
        // ���Ҫ���ҵ�¼��ʱ���������ʱ��,��������ʱ���Ǹ�Сʱ������������·���ж���¼��
        memset(&szIndexRootPath, 0, sizeof(szIndexRootPath));
        strcpy(szIndexRootPath.indexfile, CAC_RT_Record_Server::instance()->get_record_indexfile_path());
        indexRootPathList.push_back(szIndexRootPath);

        memset(&szIndexRootPath, 0, sizeof(szIndexRootPath));
        strcpy(szIndexRootPath.indexfile, CAC_RT_Record_Server::instance()->get_old_index_path());

        indexRootPathList.push_back(szIndexRootPath);
    }

    return;
}


