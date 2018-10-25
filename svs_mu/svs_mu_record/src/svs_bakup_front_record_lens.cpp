/*****************************************************************************
   ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �ļ���          : SVS_Bakup_Front_Record_Lens.cpp
  �汾��          : 1.0
  ��������        : 2008-8-15
  ����޸�        :
  ��������        : ǰ��¼�񱸷ݾ�ͷ������ͱ��ݾ�ͷ�࣬������ʵ�ֶ�ÿ�����ݾ�ͷ
                    ����Ĺ������ݾ�ͷ������ݱ���������ƽ̨��ǰ��¼��������
                    �ص������ͨ��¼��ط������ǰ��¼�񱸷ݵ�ƽ̨�Ĵ��������ϡ�
  �����б�        :
  �޸���ʷ        :
  1 ����          : 2008-8-15
    �޸�����      : ����
 *******************************************************************************/
#include "svs_ace_header.h"
#include "svs_auto_reference_ptr.h"
#include "svs_rt_record_common.h"
#include <vector>
#include "svs_timer.h"

#include "svs_stat_manager.h"
#include "svs_recv_stream.h"
#include "svs_real_record_server.h"
#include "svs_record_index.h"
#include "svs_real_record_lens.h"
#include "svs_record_inform.h"
#include "svs_bakup_front_record_lens.h"
#include "svs_mb_buffer.h"
#include "svs_media_processor.h"

typedef CAC_Auto_Reference_Ptr <CAC_Bakup_Record_Dev_Manager,
             CAC_Bakup_Record_Dev *> SVS_BAKUP_RECORD_DEV_AUTO_PTR;
//======================CAC_Bakup_Record_Dev BEGIN=========================
/*****************************************************************************
�� �� ��  : CAC_Bakup_Record_Dev
��������  : ���ݾ�ͷ�Ĺ��캯��
�������  : const char* strLensId:��ͷID
            const uint32_t nIndex:��ͷ�ڲ�����
�������  : ��
�� �� ֵ  : �ɹ�:1   ʧ��:0
�޸���ʷ  :
*****************************************************************************/
CAC_Bakup_Record_Dev::CAC_Bakup_Record_Dev(const char*    strLensId,
                                           const uint32_t nIndex )
                                    : CSVS_Record_Lens(strLensId, nIndex)
{
    referenc_count_     = 1;
    state_              = RECORDBAK_STATE_INIT;

    router_id_          = 0;
    status_start_time_  = time(NULL);

    // ��ʼ����Ա����
    referenc_count_ = 1;
    state_ = RECORDBAK_STATE_INIT;

    // ��ͷ���ͱ�ʾ����Ϊǰ��¼�񱸷�
    lens_type_ = LENS_TYPE_RECORDBAK;

    cur_replay_sect_index_ = 0;
    router_id_  = 0;
    router_msgno_ = 0;

    time_remove_value = 0;

    m_ulIOHandlerIndex = 0;
}
CAC_Bakup_Record_Dev::CAC_Bakup_Record_Dev()
{
    referenc_count_     = 1;
    state_              = RECORDBAK_STATE_INIT;

    router_id_          = 0;
    status_start_time_  = time(NULL);

    // ��ʼ����Ա����
    referenc_count_ = 1;
    state_ = RECORDBAK_STATE_INIT;
    m_ulIOHandlerIndex = (uint32_t)IO_INVALID_HANDLE;

    // ��ͷ���ͱ�ʾ����Ϊǰ��¼�񱸷�
    lens_type_ = LENS_TYPE_RECORDBAK;

    cur_replay_sect_index_ = 0;
    router_id_  = 0;
    router_msgno_ = 0;

    time_remove_value = 0;
}

CAC_Bakup_Record_Dev::~CAC_Bakup_Record_Dev()
{
    try
    {
        replay_sections_vector_.clear();
    }catch(...){
    }
    cur_replay_sect_index_ = 0;
}

/*****************************************************************************
�� �� ��  : reset
��������  : ��λ�������ã�������Դ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::reset()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO, "Reset backup camera. cameraId[%s].", str_frontdev_id_));

    try
    {
        replay_sections_vector_.clear();
    }catch(...)
    {

    }
    cur_replay_sect_index_ = 0;

    m_ulIOHandlerIndex = (uint32_t)IO_INVALID_HANDLE;

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : init_device
��������  : ��ʼ��ǰ��¼���豸
�������  : const ACE_Message_Block *mb:ǰ��¼�񱸷�������Ϣ
�������  : ��
�� �� ֵ  : �ɹ�:true   ʧ��:false
�޸���ʷ  :
*****************************************************************************/
bool CAC_Bakup_Record_Dev::init_device(const ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO,
        "Begin initialize backup camera, cameraID[%s], internalId[%d].",
        str_frontdev_id_,
        internal_id_));

    (void)set_status(RECORDBAK_STATE_INIT);

    //��������һ�����ֻ��һ��FILEINFO���Ҳ�����RECORDBAK_SECTION_MAX_NUM��SECTIONINFO
    uint32_t frontFileInfoSize = sizeof(FILEINFO)
                        + (sizeof(SECTIONINFO) * (RECORDBAK_SECTION_MAX_NUM - 1));

    FILEINFO* frontFileInfo = NULL;
    try
    {
        frontFileInfo = (FILEINFO*)new char[frontFileInfoSize];
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Allocating index buffer for backup camera failed."));

        char * pTemp = (char*)(void*)frontFileInfo;
        SVS_DELETE(pTemp, SVS_DELETE_MULTI);
        frontFileInfo = NULL;
        (void)stop_backup_record();

        return false;
    }

    (void)ACE_OS::memset(frontFileInfo, 0, frontFileInfoSize);

    //����ǰ��¼��������Ϣ,����������
    SVS_RECORDERSVR_DISTRIBUTE_RECORDBAK_TASK_REQ *pReq =
        (SVS_RECORDERSVR_DISTRIBUTE_RECORDBAK_TASK_REQ*)(void*)(mb->rd_ptr());

    // ������Ϣ��С����У�飬���С����С���ȣ���ɱ���Խ���ȡ
    uint32_t uiCheckLen = offsetof(FILEINFO, SectionInfo) +
                offsetof(SVS_RECORDERSVR_DISTRIBUTE_RECORDBAK_TASK_REQ, FileInfo);
    if (pReq->Header.PacketLength < uiCheckLen)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Request backup record message length invalid, camera[%s], msgLen[%d], minLen[%d].",
            str_frontdev_id_, pReq->Header.PacketLength, uiCheckLen));

        char * pTemp = (char*)(void*)frontFileInfo;
        SVS_DELETE(pTemp, SVS_DELETE_MULTI);
        frontFileInfo = NULL;
        (void)stop_backup_record();

        return false;
    }

    //��ȡ��������Ϣ���ܹ������Ķ���Ϣ���������ܹ���Ҫ�Ļ������ռ�
    uint8_t* pData = (uint8_t*)pReq->FileInfo;
    uint8_t sectNum = pReq->FileInfo[0].SectionSum;
    if (pReq->SectionCount != sectNum && pReq->SectionCount > 0xFF)
    {
        SVS_LOG((SVS_LM_WARNING,
            "The total section number too big, so backup maximal section number."
            "cameraID[%s],SectionCount[%u], SectionSum[%d], maximal number[%d].",
            str_frontdev_id_,
            pReq->SectionCount,
            sectNum,
            RECORDBAK_SECTION_MAX_NUM));

        sectNum = RECORDBAK_SECTION_MAX_NUM;
    }
    uint32_t dataSize = sizeof(FILEINFO) + ((sectNum - 1) * sizeof(SECTIONINFO));

    (void)ACE_OS::memcpy(frontFileInfo, pData, dataSize);
    frontFileInfo->SectionSum = sectNum;

    //�ѱ����������ط�vector
    time_t currTime = time(NULL); // ��ǰϵͳʱ��
    replay_sections_vector_.clear(); // ��ջطŻ�����

    SECTIONINFO * pSectInfo = NULL;
    for(int32_t i = 0;i < frontFileInfo->SectionSum;i++)
    {
        pSectInfo = &frontFileInfo->SectionInfo[i];
        RTSECTIONINFO tempIndexSect;

        SVS_LOG((SVS_LM_DEBUG,
            "Put backup task to list, backup camera[%s], indexFile section[%u], timeSpan[%s-%s].",
            str_frontdev_id_,
            pSectInfo->SectionID,
            pSectInfo->TimeSpan.StartTime,
            pSectInfo->TimeSpan.EndTime));

        tempIndexSect.StartTime = SVS_SS_UTILITIES::str2time((char*)pSectInfo->TimeSpan.StartTime);
        tempIndexSect.EndTime = SVS_SS_UTILITIES::str2time((char*)pSectInfo->TimeSpan.EndTime);

        //�����ʼʱ�䲻С�ڽ���ʱ�䣬�������˷Ƿ���
        if ((tempIndexSect.EndTime > currTime) || (tempIndexSect.StartTime >= tempIndexSect.EndTime))
        {
            SVS_LOG((SVS_LM_WARNING,
                "Discard invalid backup section, backup camera[%s], sectionID[%u], timeSpan[%s-%s].",
                str_frontdev_id_,
                pSectInfo->SectionID,
                pSectInfo->TimeSpan.StartTime,
                pSectInfo->TimeSpan.EndTime));

            continue;
        }

        //�Ϸ��Ķ� ѹ��vector
        replay_sections_vector_.push_back(tempIndexSect);
    }

    // 4.�ͷŻ���
    char * pTemp = (char*)(void*)frontFileInfo;
    SVS_DELETE(pTemp, SVS_DELETE_MULTI);
    frontFileInfo = NULL;

    if (1 < replay_sections_vector_.size())
    {
        delete_overlap_sections(replay_sections_vector_);

        // ���տ�ʼʱ�䣬����Ҫ�طŵĶ�����
        uint32_t tempSectNum;
        RTSECTIONINFO tempSectInfo;
        tempSectNum = replay_sections_vector_.size();
        for (uint32_t i = 1; i < tempSectNum; i++)
        {
            for(uint32_t j = tempSectNum -1 ; j >= i; j--)
            {
                if(replay_sections_vector_[j].StartTime < replay_sections_vector_[j-1].StartTime)
                {
                    tempSectInfo = replay_sections_vector_[j];
                    replay_sections_vector_[j] = replay_sections_vector_[j-1];
                    replay_sections_vector_[j-1] = tempSectInfo;
                }
            }
        }
    }

    if(0 == replay_sections_vector_.size())
    {
        SVS_LOG((SVS_LM_WARNING, "No sections needed to playback, backup camera[%s].",
            str_frontdev_id_));

       (void)stop_backup_record();

       return false;
    }

    //��ӡ������Ҫ�طŵĶ���Ϣ
    uint32_t tempSectNum = replay_sections_vector_.size();
    uint32_t j = 0;
    char strStartTime[TIME_STRING_LEN];
    char strEndTime[TIME_STRING_LEN];

    for (j = 0; j < tempSectNum; j++)
    {
        (void)SVS_SS_UTILITIES::time2str(strStartTime, sizeof(strStartTime), replay_sections_vector_[j].StartTime);
        (void)SVS_SS_UTILITIES::time2str(strEndTime, sizeof(strEndTime), replay_sections_vector_[j].EndTime);

        SVS_LOG((SVS_LM_INFO,
            "Output playback section info, backup camera[%s], sectionID[%u], time[%s-%s].",
            str_frontdev_id_,
            j,
            strStartTime,
            strEndTime));
    }

    //����·��
    (void)request_router();

    return true;
}

/*****************************************************************************
�� �� ��  : delete_overlap_sections
��������  : ���ε�ǰ���ظ�����������Ϣ
�������  : SECTIONINFO_VECTOR & sectVector:ǰ����Ҫ�طŵ�����������
�������  : ��
�� �� ֵ  : ��
�޸���ʷ  :
*****************************************************************************/
void CAC_Bakup_Record_Dev::delete_overlap_sections(SECTIONINFO_VECTOR & sectVector)const
{
    SVS_TRACE();

    // 1.�����ص��Ķ�ȥ��
    uint32_t tempSectNum;
    RTSECTIONINFO tempSectInfo;
    SECTIONINFO_VECTOR tempSectVector;
    tempSectVector.clear();
    tempSectVector.push_back(sectVector[0]);
    uint32_t currIndex = 1;
    bool isOver = false;
    while(currIndex < sectVector.size())
    {
        isOver = false;
        tempSectNum = tempSectVector.size();
        // �͵�ǰ���ж�ƥ��
        for (uint32_t m = 0; m < tempSectNum; m++)
        {
            // 1.����Ѿ������ö�
            if ((tempSectVector[m].StartTime <= sectVector[currIndex].StartTime)
             && (tempSectVector[m].EndTime >= sectVector[currIndex].EndTime))
            {
                isOver = true;
                break;
            }
            // �����������ǰ�벿��
            else if((tempSectVector[m].StartTime <= sectVector[currIndex].StartTime)
             && (tempSectVector[m].EndTime > sectVector[currIndex].StartTime)
             && (tempSectVector[m].EndTime < sectVector[currIndex].EndTime))
            {
                // ��ȡ����һ�μ���δƥ�����
                isOver = true;
                tempSectInfo.StartTime = tempSectVector[m].EndTime + 1;
                tempSectInfo.EndTime = sectVector[currIndex].EndTime;
                sectVector.push_back(tempSectInfo);
                break;
            }
            // �������һ���Ѿ�����
            else if((tempSectVector[m].StartTime < sectVector[currIndex].EndTime)
             && (tempSectVector[m].EndTime >= sectVector[currIndex].EndTime)
             && (tempSectVector[m].StartTime > sectVector[currIndex].StartTime))
            {
                // ��ȡǰ��һ�μ���δƥ�����
                isOver = true;
                tempSectInfo.StartTime = sectVector[currIndex].StartTime;
                tempSectInfo.EndTime = tempSectVector[m].StartTime - 1;
                sectVector.push_back(tempSectInfo);
                break;
            }
            // ��������˵�ǰ��ĳ��
            else if((tempSectVector[m].StartTime > sectVector[currIndex].StartTime)
             && (tempSectVector[m].EndTime < sectVector[currIndex].EndTime))
            {
                // �ֱ��ȡ���ߵ����˼���δƥ�����
                isOver = true;

                // ǰ��δ�ص���һ��
                tempSectInfo.StartTime = sectVector[currIndex].StartTime;
                tempSectInfo.EndTime = tempSectVector[m].StartTime - 1;
                sectVector.push_back(tempSectInfo);

                // ����δ�ص���һ��
                tempSectInfo.StartTime = tempSectVector[m].EndTime + 1;
                tempSectInfo.EndTime = sectVector[currIndex].EndTime;
                sectVector.push_back(tempSectInfo);
                break;
            }
        }

        // ������͵�ǰ���κε㽻���������
        if (!isOver)
        {
            tempSectVector.push_back(sectVector[currIndex]);
        }

        ++currIndex;
    }

    // �ؿ���
    sectVector.clear();
    for(uint32_t n = 0;n < tempSectVector.size();n++)
    {
        sectVector.push_back(tempSectVector[n]);
    }
    return;
}

/*****************************************************************************
 �� �� ��  : send_stop_recv_msg
 ��������  : ����:����ֹͣ��Ϣ�������߳���д¼���߳�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::send_stop_recv_msg()
{
    if((uint32_t)IO_INVALID_HANDLE == m_ulIOHandlerIndex)
    {
        return SVS_RESULT_OK;
    }

    // ��ý�崦�������Ƴ�ע��ľ��
    int32_t nRet = SVS_Media_Processor::instance().stop_record(m_ulIOHandlerIndex);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "backup device stop record failed.backup cameraId[%s] ,io handle[%u].",
            str_frontdev_id_,
            m_ulIOHandlerIndex));
        return SVS_RESULT_FAILURE;
    }


    m_ulIOHandlerIndex = (uint32_t)IO_INVALID_HANDLE;

    SVS_LOG((SVS_LM_DEBUG,
        "Backup device stop record success.backup cameraId[%s]",
        str_frontdev_id_));
    return 0;
}

/******************************************************************************
�� �� �� : drop_next_section
�������� : ���ö�ʱ������һ��ʱ�䣬�ٽ�����һ�λط�
������� : ��
������� : ��
����ֵ   : �ɹ�:0   ʧ��:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::drop_next_section()
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    if (state_ != RECORDBAK_STATE_RECEIVING_FILE)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Backup camera current status is not recv stream. backup camera[%s], state[%d].",
            str_frontdev_id_, state_));
        return SVS_RESULT_OK;
    }

    // ������һ�λط���ɣ���ֹͣ����
    if (cur_replay_sect_index_ >=  replay_sections_vector_.size() - 1)
    {
        SVS_LOG((SVS_LM_INFO,
            "Backup camera has finished backup and will stop camera. backup camera[%s].",
            str_frontdev_id_));
        (void)stop_backup_record();
        return SVS_RESULT_OK;
    }

    // �ط���һ��
    {
        char strTime1[TIME_STRING_LEN];
        char strTime2[TIME_STRING_LEN];
        char strTime3[TIME_STRING_LEN];
        char strTime4[TIME_STRING_LEN];
        (void)SVS_SS_UTILITIES::time2str(strTime1, sizeof(strTime1),replay_sections_vector_[cur_replay_sect_index_].StartTime);
        (void)SVS_SS_UTILITIES::time2str(strTime2, sizeof(strTime2),replay_sections_vector_[cur_replay_sect_index_].EndTime);
        (void)SVS_SS_UTILITIES::time2str(strTime3, sizeof(strTime3),replay_sections_vector_[cur_replay_sect_index_ + 1].StartTime);
        (void)SVS_SS_UTILITIES::time2str(strTime4, sizeof(strTime4),replay_sections_vector_[cur_replay_sect_index_ + 1].EndTime);

        SVS_LOG((SVS_LM_INFO,
            "Backup camera has finished playback current section and start to plakback next section."
            "backup camera[%s], "
            "current section[%u:%s-%s],"
            "next section[%u:%s-%s].",
            str_frontdev_id_,
            cur_replay_sect_index_,strTime1,strTime2,
            cur_replay_sect_index_ + 1,strTime3,strTime4));
    }

    // �ر�·��
    (void)shutdown_router();

    // ֹͣ������
    (void)send_stop_recv_msg();

    // ����״̬Ϊ����״̬
    (void)set_status(RECORDBAK_STATE_SLEEP);

    return SVS_RESULT_OK;
}

/******************************************************************************
�� �� �� : handle_routerinfo
�������� : ����StoreSvr���ص�VTDU��ַ����VTDUע��
������� : const void *pbyMsg:StoreSvr��Ӧ��Ϣ
������� : ��
����ֵ   : �ɹ�:0   ʧ��:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::handle_routerinfo(const void *pbyMsg)
{
    SVS_TRACE();

    SVS_CLIENTSVR_SETUP_MEDIA_STREAM_RESP_MSG *pRouterRep = (SVS_CLIENTSVR_SETUP_MEDIA_STREAM_RESP_MSG*)pbyMsg;

    // �жϵ�ǰ�Ƿ��Ѿ�����·����
    if ((RECORDBAK_STATE_REQUEST_ROUTER != state_) && (RECORDBAK_STATE_REQUEST_ROUTER < state_))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Backup camera can not deal new route in current status."
            "backup camera[%s], streamID[%u], current state[0x%04x], camera's routeID[%u].",
            str_frontdev_id_,
            pRouterRep->StreamID,
            state_,
            router_id_));

        return SVS_RESULT_FAILURE;
    }

    // ����·��id��vtdu��ַ
    router_id_ = pRouterRep->StreamID;
    (void)vtdu_addr_.set(htons((uint16_t)pRouterRep->MduPort), pRouterRep->MduIp, 0);

    (void)set_status(RECORDBAK_STATE_CONNECT_VTDU);
    if (SVS_RESULT_OK != start_backup_record())
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to send notify vtdu message and stop backup camera. backup camera[%s].",
            str_frontdev_id_));

        (void)stop_backup_record();
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Success to send notify vtdu message. backup camera[%s], routeID[%u], VTDU[%s:%u].",
        str_frontdev_id_,
        router_id_,
        vtdu_addr_.get_host_addr(),
        vtdu_addr_.get_port_number()));

    // ���õȴ�VTDU��Ӧ��ʱ��
    (void)set_status(RECORDBAK_STATE_CONNECT_VTDU);

    return SVS_RESULT_OK;
}
int32_t CAC_Bakup_Record_Dev::play_media_request()
{
    // ���뷢��
    // ����ͳ�ʼ��������Ϣ
    uint16_t respMsgLen = sizeof(SVS_CLIENTSVR_PLAY_MEDIA_STREAM_REQ_MSG);

    ACE_Message_Block *respMsgMb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, respMsgLen );
    if ( NULL == respMsgMb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Allocating msg block failed. msg type[%d], msg size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
            respMsgLen ));

        return -1;
    }

    SVS_CLIENTSVR_PLAY_MEDIA_STREAM_REQ_MSG* pRespMsg =
        (SVS_CLIENTSVR_PLAY_MEDIA_STREAM_REQ_MSG*)(void*)respMsgMb->wr_ptr();

    respMsgMb->wr_ptr( respMsgLen );

    (void)ACE_OS::memset(pRespMsg, 0, respMsgLen);

    // ��Ϣ��
    router_msgno_ = CAC_RT_Record_Server::instance()->transaction_no();

    // ��װ��Ϣͷ
    FillCommonHeader(   &pRespMsg->Header,
                        SVS_MSG_TYPE_CLIENT_PLAY_MEDIA_STREAM_REQ,
                        router_msgno_,
                        respMsgLen);

    SetNLSDeviceId((uint8_t *)pRespMsg->DeviceID,
        sizeof(pRespMsg->DeviceID),
        str_frontdev_id_);

    (void)ACE_OS::memset(pRespMsg->UserID, 0, USERID_LEN);

    SetNLSDeviceId((uint8_t *)pRespMsg->SessionID,
        sizeof(pRespMsg->SessionID),
        CAC_RT_Record_Server::instance()->str_id());

    pRespMsg->StreamID = router_id_;
    pRespMsg->BusinessType = BUSINESS_TYPE_FRONT_DOWNLOAD;


    // ������������Ϣ
    int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( respMsgMb );
    if ( 0 != sendResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to send play request msg. msg type[0x%04x], return code[%d].",
            pRespMsg->Header.MsgType,
            sendResult ));
         g_p_msg_buffer->free_mb( respMsgMb );

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Success to send play media message. backup camera[%s], msgNo[%u].",
        str_frontdev_id_,
        router_msgno_));

    return SVS_RESULT_OK;

}
/******************************************************************************
�� �� �� : notify_vtdu
�������� : ����socket����VTDU����ע����Ϣ
������� : ��
������� : ��
����ֵ   : �ɹ�:0   ʧ��:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::start_backup_record()
{

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    time_remove_value = time(NULL) - replay_sections_vector_[cur_replay_sect_index_].StartTime;
    SVS_LOG((SVS_LM_INFO, "Start to backup front record.IO handle[%u], time remove value[%d]",
        m_ulIOHandlerIndex,
        time_remove_value));

    // ��media processor����io handler
    int32_t nRet = SVS_Media_Processor::instance().alloc_handle(
                                                str_frontdev_id_,
                                                LENS_TYPE_RECORDBAK,
                                                0,
                                                m_ulIOHandlerIndex);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Connecte with VTDU failed. alloc io handler failed."
            "cameraId[%s].",
            str_frontdev_id_));

        return SVS_RESULT_FAILURE;
    }

    // ����
    nRet = SVS_Media_Processor::instance().add_record_index(m_ulIOHandlerIndex,
                                               replay_sections_vector_[cur_replay_sect_index_].StartTime,
                                               0);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "cameraId[%s] set index start time[%d] fail.",
            str_frontdev_id_,
            replay_sections_vector_[cur_replay_sect_index_].StartTime));

        return SVS_RESULT_FAILURE;
    }

    // ��ʼ¼��
    ACE_INET_Addr addr((uint16_t)0);
    nRet = SVS_Media_Processor::instance().start_record(m_ulIOHandlerIndex,
                                                        router_id_,
                                                        addr,
                                                        vtdu_addr_,
                                                        time_remove_value);
    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
                   "Connecte with VTDU failed. start recv failed."
                   "cameraId[%s].",
                   str_frontdev_id_));
        return SVS_RESULT_FAILURE;
    }

    (void)set_status(RECORDBAK_STATE_CONNECT_VTDU);

    return SVS_RESULT_OK;
}


/*****************************************************************************
 �� �� ��  : stop_backup_record
 ��������  : ֹͣ���ݾ�ͷ������ɾ����ʱ����Ȼ��ֹͣ·�ɣ�֪ͨ�����̺߳�д�ļ�
             �߳��˳������ɾ����ͷ����
 �������  : bool procRouter:��ʶ�Ƿ�ֹͣ·��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::stop_backup_record(bool procRouter)
{
    SVS_TRACE();
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
        if (state_ >= RECORDBAK_STATE_WAITING_OVER)
        {
            SVS_LOG((SVS_LM_WARNING,"Backup camera has stopped now. backup camera[%s].",str_frontdev_id_));
            return SVS_RESULT_OK;
        }

        SVS_LOG((SVS_LM_INFO,
            "Begin to stop backup camera. backup camera[%s] stat[%d].",
            str_frontdev_id_,
            state_));

        // ����״̬Ϊֹͣ
        (void)set_status(RECORDBAK_STATE_WAITING_OVER);
    }

    // ֹͣ·��
    if (procRouter)
    {
        (void)shutdown_router();
    }

    (void)set_status(FRONT_DEVICE_STATE_STOP);

    SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(CAC_Bakup_Record_Dev_Manager::instance(), this);

    // ֹͣ¼��
    (void)SVS_Media_Processor::instance().stop_record(m_ulIOHandlerIndex);

    return SVS_RESULT_OK;
}

int32_t CAC_Bakup_Record_Dev::set_status(uint8_t newStatus)
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    SVS_LOG((SVS_LM_DEBUG,
        "The backup camera begin to set status."
        "backup cameraId[%s], old status start time[%d],state[0x%04x=>0x%04x].",
        str_frontdev_id_,
        status_start_time_,
        state_,
        newStatus ));

    state_ = newStatus;
    status_start_time_ = time(NULL);
    return SVS_RESULT_OK;
}

/******************************************************************************
�� �� �� : on_timer
�������� : ��ʱ���ص����������ݱ����豸��ǰ״̬����ȷ����ʱ�������ԣ�����Ӧ����
������� : ��
������� : ��
����ֵ   : ��
*****************************************************************************/
void CAC_Bakup_Record_Dev::on_timer(int32_t, int32_t,int32_t nDummy)
{
    // �����ӿ�
    if (CAC_Bakup_Record_Dev::TIMER_TYPE_CHECK_STATUS != nDummy)
    {
        return;
    }

    switch (state_)
    {
        case RECORDBAK_STATE_REQUEST_ROUTER:
        {
            return request_router_on_timer();
        }

        case RECORDBAK_STATE_CONNECT_VTDU:
        {
            return conn_vtdu_on_timer();
        }

        case RECORDBAK_STATE_RECEIVING_FILE:
        {
            return revc_stream_on_timer();
        }
        case RECORDBAK_STATE_WAITING_OVER:
        {
            // ����ǵȴ�����
            SVS_LOG((SVS_LM_WARNING,
                "Backup camera wait for end timed out."
                "backup camera[%s], "
                "current status[0x%04x].",
                str_frontdev_id_,
                state_));
            (void)stop_backup_record();
            return;
        }
        case RECORDBAK_STATE_SLEEP:
        {
            return sleep_on_timer();
        }
        default:
        {
            SVS_LOG((SVS_LM_ERROR,
                "Backup camera current state is invalid. backup camera[%s], status[0x%04x].",
                str_frontdev_id_,
                state_));
            break;
        }
    }

    return;
}

void CAC_Bakup_Record_Dev::request_router_on_timer()
{
    time_t currTime = time(NULL);
    // �����û�е����ʱʱ�䣬���ô���
    if ((currTime - status_start_time_) < REQUEST_ROUTER_INTERVAL_TIME)
    {
        return ;
    }

    // ·�ɵȴ���ʱ���������ݹ���
    SVS_LOG((SVS_LM_WARNING,
        "Backup camera request route timed out and stop backup camera. backup camera[%s].",
        str_frontdev_id_));
    (void)stop_backup_record();

    return ;
}

/*****************************************************************************
 �� �� ��  : recv_stream_on_timer
 ��������  : ��ʱ���ص���¼��״̬�ǽ�����״̬���жϽ�����д¼���Ƿ�ʱ
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CAC_Bakup_Record_Dev::revc_stream_on_timer()
{
    // ������ʱ��ֹͣǰ��¼�񱸷�
    if (SVS_RESULT_OK != SVS_Media_Processor::instance().get_handle_status(m_ulIOHandlerIndex))
    {
        // ֹͣǰ��¼�񱸷�
        SVS_LOG((SVS_LM_WARNING,
            "Backup camera write record file timed out. backup camera[%s].", str_frontdev_id_));

        (void)stop_backup_record();

        return ;
    }

    // ��������յ������Ѿ��յ�������Ҫ�ģ���ֹͣ����
    time_t lastRecvTime = 0 ;

    int32_t nRet = SVS_Media_Processor::instance().get_last_recv_time(lastRecvTime,m_ulIOHandlerIndex);
    if(SVS_RESULT_OK != nRet)
    {
        // ֹͣǰ��¼�񱸷�
        SVS_LOG((SVS_LM_WARNING,
            "Get last receive time failed, so stop backup record.Backup camera[%s].", str_frontdev_id_));

        (void)stop_backup_record();

        return ;
    }

//    lastRecvTime -= time_remove_value;

    if (lastRecvTime >=  replay_sections_vector_[cur_replay_sect_index_].EndTime)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Current section receive finished, so drop next section.lastRecvTime[%d],currSectEndTime[%d]",
            lastRecvTime,
            replay_sections_vector_[cur_replay_sect_index_].EndTime));

        (void)drop_next_section();
    }

    return ;
}

void CAC_Bakup_Record_Dev::conn_vtdu_on_timer()
{
    // ������Ϊ-1����û�г�ʱ�����ò�ѯ״̬
    time_t currTime = time(NULL);
    if(((uint32_t)IO_INVALID_HANDLE == m_ulIOHandlerIndex)
    && (VTDU_SEND_DATA_MIN_TIMEOUT > (currTime - status_start_time_)))
    {
        return;
    }

    int32_t result = SVS_Media_Processor::instance().get_handle_status(m_ulIOHandlerIndex);
    // �Ѿ����յ�VTDU����Ӧ����ת��״̬
    if (SVS_RESULT_OK == result)
    {
        (void)set_status(RECORDBAK_STATE_RECEIVING_FILE);
        return ;
    }
    else if(SVS_ERR_WAIT_RESP == result ) // ������ڵȴ�VTDU��Ӧ������û�г�ʱ
    {
        return ;
    }

    // ����VTDU��ʱ��ֹͣǰ��¼�񱸷�
    SVS_LOG((SVS_LM_ERROR,
        "Backup camera connect to VTDU timed out and stop backup camera. backup camera[%s].",
        str_frontdev_id_));
    (void)stop_backup_record();

    return ;
}

void CAC_Bakup_Record_Dev::sleep_on_timer()
{
    // �����ѯ̫Ƶ������������5s�Ժ��ٲ�ѯһ��
    time_t currTime = time(NULL);
    // ���ʱ���Ѿ���ʱ������������·��,���û�г�ʱ���������
    if ((currTime - status_start_time_) < REQUEST_ROUTER_DELAY_TIME)
    {
        return ;
    }

    uint32_t handleIndex = (uint32_t)IO_INVALID_HANDLE;
    // �жϸľ�ͷ�ĵײ�handle�Ƿ��Ѿ��ͷţ������û���ͷţ���ȴ�
    int32_t nRet = SVS_Media_Processor::instance().get_handle_close_status(str_frontdev_id_,
                                                                    LENS_TYPE_RECORDBAK,
                                                                    0,handleIndex);
    // ���handle�Ѿ��ͷţ���ʾ�Ѿ���ɱ�����
    if(SVS_RESULT_OK == nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Current section write finished, so drop next section.currSectEndTime[%d]",
            replay_sections_vector_[cur_replay_sect_index_].EndTime));

        ++cur_replay_sect_index_;

        (void)request_router();
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,
            "backup Camera's Hanlde is closing, so wait it. cameraId[%s], handle index[%u].",
            str_frontdev_id_, handleIndex));

        status_start_time_ = currTime;
    }

    return ;
}

/*****************************************************************************
 �� �� ��  : proc_send_report
 ��������  : �����ͱ���
 �������  : uint16_t
             uint32_t timeTick:ʱ���
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::proc_eos_packet()
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO,
        "Begin to deal eos packet. backup camera[%s], current playback section[%u].",
        str_frontdev_id_,
        cur_replay_sect_index_));

    (void)drop_next_section();


    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : request_router
 ��������  : ǰ��¼��ط�·�����󣬸ò���ȫ�����ÿͻ���¼��طŽӿڣ��ӿ����漰
             ��UserIDͳһ��дΪ�գ�SessionIDͳһ��дΪRtRecord���豸ID��ʵ��ʱ
             RtRecordSvrģ��ͻ���Ӧ�÷�����
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::request_router()
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO,
        "Begin to request route, "
        "backup camera[%s], current sectionID[%u], total section num[%d].",
        str_frontdev_id_,
        cur_replay_sect_index_,
        replay_sections_vector_.size()));

    // ����طŵĶ��±��Ѿ�Խ�磬��ֹͣ����
    if (cur_replay_sect_index_ >= replay_sections_vector_.size())
    {
        SVS_LOG((SVS_LM_ERROR,
            "Stop backup because current playback section exceed total playback section."
            "backup camera[%s], current playback section[%u], total playback section[%u].",
            str_frontdev_id_,
            cur_replay_sect_index_,
            replay_sections_vector_.size()));

        (void)stop_backup_record();
    }

    // �ͷ�ԭ����·��
    if (router_id_ > 0)
    {
        (void)shutdown_router();
    }

    // ����ͳ�ʼ��������Ϣ
    uint16_t respMsgLen = sizeof(SVS_CLIENTSVR_SETUP_MEDIA_STREAM_REQ_MSG);

    ACE_Message_Block *respMsgMb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, respMsgLen );
    if ( NULL == respMsgMb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Allocating msg block failed. msg type[%d], msg size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
            respMsgLen ));

        return -1;
    }


    SVS_CLIENTSVR_SETUP_MEDIA_STREAM_REQ_MSG* pRespMsg =
        (SVS_CLIENTSVR_SETUP_MEDIA_STREAM_REQ_MSG*)(void*)respMsgMb->wr_ptr();

    respMsgMb->wr_ptr( respMsgLen );

    (void)ACE_OS::memset(pRespMsg, 0, respMsgLen);

    // ��Ϣ��
    router_msgno_ = CAC_RT_Record_Server::instance()->transaction_no();

    // ��װ��Ϣͷ
    FillCommonHeader(   &pRespMsg->Header,
                        SVS_MSG_TYPE_CLIENT_SETUP_MEDIA_STREAM_REQ,
                        router_msgno_,
                        respMsgLen);

    SetNLSDeviceId((uint8_t *)pRespMsg->DeviceID,
        sizeof(pRespMsg->DeviceID),
        str_frontdev_id_);

    (void)ACE_OS::memset(pRespMsg->UserID, 0, USERID_LEN);

    SetNLSDeviceId((uint8_t *)pRespMsg->SessionID,
        sizeof(pRespMsg->SessionID),
        CAC_RT_Record_Server::instance()->str_id());

    pRespMsg->BusinessType = BUSINESS_TYPE_FRONT_DOWNLOAD;

    pRespMsg->RecvProtocol = RECV_PROTOCOL_SVS_TCP;

    // �طŵ�ǰsection��ʱ�����Ϣ
    (void)SVS_SS_UTILITIES::time2str((char*)pRespMsg->TimeSpan.StartTime,
                     sizeof(pRespMsg->TimeSpan.StartTime),
                     replay_sections_vector_[cur_replay_sect_index_].StartTime);
    (void)SVS_SS_UTILITIES::time2str((char*)pRespMsg->TimeSpan.EndTime,
                     sizeof(pRespMsg->TimeSpan.EndTime),
                     replay_sections_vector_[cur_replay_sect_index_].EndTime);

    // ���ͻط�������Ϣ
    int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( respMsgMb );
    if ( 0 != sendResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to send playback msg. msg type[0x%04x], return code[%d].",
            pRespMsg->Header.MsgType,
            sendResult ));
         g_p_msg_buffer->free_mb( respMsgMb );

        return SVS_RESULT_FAILURE;
    }

    // ����Ϣ�Ŷ�Ӧ���豸��Ϣ����map��
    (void)CAC_Bakup_Record_Dev_Manager::instance()->add_msgno_map(this, router_msgno_);

    // ���õȴ�·�ɶ�ʱ��
    (void)set_status(RECORDBAK_STATE_REQUEST_ROUTER);

    SVS_LOG((SVS_LM_INFO,
        "Success to send setup media message. backup camera[%s], msgNo[%u].",
        str_frontdev_id_,
        router_msgno_));

    return SVS_RESULT_OK;

}

/*****************************************************************************
 �� �� ��  : shutdown_router
 ��������  : ��¼��������������ǰ��¼��ط�ֹͣ��Ϣ��ֱ��ģ��ͻ���Ӧ�÷�����
             ����ǰ��¼��ط�ֹͣ����
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::shutdown_router()
{
    SVS_TRACE();

    // ���¼��idΪ0������Ҫ����ֹͣ·����Ϣ
    if (0 == router_id_)
    {
        SVS_LOG((SVS_LM_WARNING,"RouteID is 0. No need to send stop route message."));
        return SVS_RESULT_OK;
    }

    SVS_LOG((SVS_LM_INFO,
        "Send tear down media request message. backup camera[%s], routeID[%u].",
        str_frontdev_id_,
        router_id_));

    // ����ֹͣ¼����Ϣ
    uint16_t respMsgLen = sizeof(SVS_CLIENTSVR_TEARDOWN_MEDIA_STREAM_REQ_MSG);

    ACE_Message_Block *respMsgMb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG, respMsgLen );
    if ( NULL == respMsgMb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Allocating msg block failed. msg type[%d], msg size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_SEND_MSG,
            respMsgLen ));

        return -1;
    }

    SVS_CLIENTSVR_TEARDOWN_MEDIA_STREAM_REQ_MSG* pRespMsg =
        (SVS_CLIENTSVR_TEARDOWN_MEDIA_STREAM_REQ_MSG*)(void*)respMsgMb->wr_ptr();
    respMsgMb->wr_ptr( respMsgLen );

    (void)ACE_OS::memset(pRespMsg, 0, respMsgLen);

    FillCommonHeader(   &pRespMsg->Header,
                        SVS_MSG_TYPE_CLIENT_TEARDOWN_MEDIA_STREAM_REQ,
                        CAC_RT_Record_Server::instance()->transaction_no(),
                        respMsgLen);

    (void)ACE_OS::memset(pRespMsg->UserID, 0, USERID_LEN);
    (void)ACE_OS::memcpy(pRespMsg->SessionID,
                         CAC_RT_Record_Server::instance()->str_id(),
                         SESSIONID_LEN);
    pRespMsg->StreamID = router_id_;

    // ֹͣǰ��¼��
    pRespMsg->BusinessType = BUSINESS_TYPE_FRONT_DOWNLOAD;



    // ������Ϣ
    int32_t sendResult = CAC_RT_Record_Server::instance()->send_msg( respMsgMb );
    if ( 0 != sendResult )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to send shutdown route message. msg type[0x%04x], return code[%d].",
            pRespMsg->Header.MsgType,
            sendResult ));
        g_p_msg_buffer->free_mb( respMsgMb );
        return SVS_RESULT_FAILURE;
    }

    // ���·��ID
    router_id_ = 0;

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : debug_get_device_status
 ��������  : ��ȡ��¼���豸��״̬����INT��ת����string�ͣ�telent������
 �������  : NA
 �������  : string &strDeviceStatus:¼���豸״̬string��ʾ
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CAC_Bakup_Record_Dev::debug_get_device_status(string &strDeviceStatus)const
{
    switch (state_)
    {
        case RECORDBAK_STATE_INIT:
            strDeviceStatus = "Initing";
            break;

        case RECORDBAK_STATE_SLEEP:
            strDeviceStatus = "WaitingForRepalyNextSection";
            break;

        case RECORDBAK_STATE_REQUEST_ROUTER:
            strDeviceStatus = "RequestingRouter";
            break;

        case RECORDBAK_STATE_CONNECT_VTDU:
            strDeviceStatus = "ConnectingToVTDU";
            break;

        case RECORDBAK_STATE_RECEIVING_FILE:
            strDeviceStatus = "ReceivingRecordMediaStream";
            break;

        case RECORDBAK_STATE_WAITING_OVER:
            strDeviceStatus = "WaitingToOver";
            break;

        case FRONT_DEVICE_STATE_STOP:
            strDeviceStatus = "Stoping";
            break;

        default:
            strDeviceStatus = "StatusIsInvalid";
            break;
    }

    return;
}

/*****************************************************************************
 �� �� ��  : debug_get_device_info
 ��������  : ���Ի�ȡ�豸��Ϣ
 �������  : char *strDeviceInfo:���ݾ�ͷID
             uint32_t szDeviceInfo:
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::debug_get_device_info( char *strDeviceInfo, uint32_t ulBuffLen )
{
    string strDeviceStatus;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, obj, mutex_, false);
    debug_get_device_status(strDeviceStatus);

    char strTime[SVS_STR_TIME_MAX_LEN];

    // ��ǰ�طŵĶ���Ϣ
    RTSECTIONINFO *pBackupSectInfo = NULL;
    pBackupSectInfo = &replay_sections_vector_[cur_replay_sect_index_];

    (void)time2string(strTime, sizeof(strTime), pBackupSectInfo->StartTime);
    string strSectStartTime = strTime;

    (void)time2string(strTime, sizeof(strTime), pBackupSectInfo->EndTime);
    string strSectEndTime = strTime;

    (void)ACE_OS::snprintf( strDeviceInfo, ulBuffLen,
                      "\tCameraId               [%s]\n\t"
                      "RouteId                  [%u]\n\t"
                      "ReferencCount            [%d]\n\t"
                      "Status                   [%s:0x%04x]\n\t"
                      "InternalId               [%d]\n\t"
                      "IOProcesserHandleIndex   [%u]\n\t"
                      "NeedPlaybackSectionCnt   [%d]\n\t"
                      "CurPlaybackSectionID     [%d]\n\t"
                      "CurPlaybackSection       [%s--%s]\n\t",
                      str_frontdev_id_,
                      router_id_,
                      referenc_count_,
                      strDeviceStatus.c_str(), state_,
                      internal_id_,
                      m_ulIOHandlerIndex,
                      replay_sections_vector_.size(),
                      cur_replay_sect_index_,
                      strSectStartTime.c_str(),strSectEndTime.c_str());

    if((uint32_t)IO_INVALID_HANDLE != m_ulIOHandlerIndex)
    {
        uint32_t writeSize = strlen(strDeviceInfo);
        uint32_t leftBuffSize = ulBuffLen - writeSize;
        SVS_Media_Processor::instance().debug_get_handle_info(m_ulIOHandlerIndex,
                                            &strDeviceInfo[writeSize],
                                            leftBuffSize);
    }
    return 0;
}
//======================CAC_Bakup_Record_Dev END=========================


//======================CAC_Bakup_Record_Dev_Manager BEGIN=========================
CAC_Bakup_Record_Dev_Manager::CAC_Bakup_Record_Dev_Manager()
{
    max_bakup_device_ = 0;
}

CAC_Bakup_Record_Dev_Manager::~CAC_Bakup_Record_Dev_Manager()
{
    max_bakup_device_ = 0;
}

/*****************************************************************************
 �� �� ��  : init
 ��������  : ��ʼ��ǰ��¼���豸������
 �������  : uint32_t nMaxLens:ϵͳ��ͷ���������ʵʱ¼��ͱ��ݾ�ͷ֮��
 �������  : ��
 �� �� ֵ  : �ɹ�:true ʧ��:false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_Bakup_Record_Dev_Manager::init(uint32_t nMaxLens)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO,
        "Init CAC_Bakup_Record_Dev_Manager, "
        "System max camera num[%u].",
        nMaxLens));

    max_bakup_device_ = nMaxLens;

    // �����������ݾ�ͷ����ռ䣬����free_list_������
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, false);

    uint32_t lensIndex = 0;
    char *pch = NULL;
    for (lensIndex = 0; lensIndex < nMaxLens; lensIndex++)
    {
        try
        {
            pch = new char[sizeof(CAC_Bakup_Record_Dev)];
        }
        catch (...)
        {
            // ������뱸�ݾ�ͷ����ռ䣬�˳�ѭ�����ͷ��Ѿ�����Ŀռ�
            SVS_LOG((SVS_LM_ERROR, "create CAC_Bakup_Record_Dev failed."));

            break;
        }

        // ��������
        *((uint32_t *)pch) = lensIndex;
        (void)free_list_.insert_tail(pch);
        using_vector_.push_back(NULL);
    }

    // ���뻺��ʧ�ܣ������ǰ����Ļ�����
    if (lensIndex != nMaxLens)
    {
        ACE_DLList_Iterator <char> iter( free_list_ );
        while (!iter.done ())
        {
            char *freeBuff = iter.next ();
            (void)iter.remove ();
            SVS_DELETE( freeBuff, SVS_DELETE_MULTI );
        }

        // ǰ������SVS_DELETE�ͷ����ڴ棬����lint�澯
        using_vector_.clear();
        return false;       //lint !e429
    }

    // ʧ��ʱ��������������ڴ棬�ɹ�����뵽���������У�ֱ��Ӧ���˳�ʱ���ͷ�
    return true;   //lint !e429
}

/*****************************************************************************
 �� �� ��  : bakup_record_notify
 ��������  : ����ǰ��¼�񱸷���Ϣ���ݣ��жϵ�ǰ��ͷ�Ƿ���ڣ���������򲻴�����
             ���񣬷��򴴽����ݾ�ͷ����
 �������  : const ACE_Message_Block * mb:ǰ��¼�񱸷���Ϣ
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::bakup_record_notify(const ACE_Message_Block * mb)
{
    SVS_TRACE();

    uint32_t rtCameraNum =
        CAC_RT_Record_Device_Manager::instance()->get_recording_device_num();

    uint32_t backupCameraNum = get_bakup_device_num();

    SVS_LOG((SVS_LM_INFO,
        "Begin to deal backup record file message, "
        "realtime record camera num[%u], backup camera num[%u].",
        rtCameraNum,
        backupCameraNum));

    //��Ϣ�Ϸ��Լ��
    SVS_RECORDERSVR_DISTRIBUTE_RECORDBAK_TASK_REQ* pReq =
        (SVS_RECORDERSVR_DISTRIBUTE_RECORDBAK_TASK_REQ*)(void*)mb->rd_ptr();

    //����ļ���������0
    if (0 == pReq->FileCount)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Backup front-end record file failed, because front-end record index num is 0 in msg block."));

        return SVS_RESULT_OK;
    }

    //��þ�ͷID��Ϣ,���ݾ�ͷID��ȡ���ݾ�ͷ����
    char strLensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {'\0'};

    SetLocalizeDeviceId((uint8_t *)strLensId,
                    sizeof(strLensId),
                    (const char *)pReq->DeviceID,
                    NLS_DEVICE_ID_MAX_LEN);

    CAC_Bakup_Record_Dev * pBakupDev = NULL;

    //���ҵ�ǰ���ݵ��豸
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
        // �����ϴν�����ʱ�䣬����ϴα��ݽ�����ʱ��С��2���ӣ��򲻱���
        // �����ϴν���ʱ����0������ͷ���ڱ��ݻ�û���ҵ������ǿ��������첽��֤
        time_t lastEndTime = 0;
        time_t nowTime = time(NULL);
        ACE_CString strCameraId = strLensId;
        (void)backup_time_map_.find(strCameraId, lastEndTime);
        ACE_OS::last_error(0);
        if(SVS_BACKUP_MIN_DISTANCE_TIME > (nowTime - lastEndTime))
        {
            SVS_LOG((SVS_LM_WARNING,
                "Can't backup record now.camera end backup task just now."
                "cameraId[%s], last endTime[%u], now[%u].",
                strLensId, lastEndTime, nowTime));

            return SVS_RESULT_OK;
        }

        (void)bakup_device_map_.find((char*)strLensId, pBakupDev);
    }

    // �����ͷ���ڱ���ǰ��¼�񣬽������ݱ�������
    if (NULL != pBakupDev)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Backup camera is working now, so it can not deal this backup task."
            "backup camera[%s].",
            strLensId));
        return SVS_RESULT_OK;
    }

    // �������¼��ľ�ͷ�����ڱ���ǰ��¼��ľ�ͷ���ﵽ�����ߣ��򲻱���
    uint32_t systemLensNum = backupCameraNum + rtCameraNum;
    if (systemLensNum >= max_bakup_device_)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Total camera number has reached the max and can not deal this backup task, "
            "total camera num[%lu], max num[%lu].",
            systemLensNum,
            max_bakup_device_));

        return SVS_RESULT_OK;
    }

    //��ӱ��ݾ�ͷ����
    pBakupDev = add_device(strLensId);
    if (NULL == pBakupDev)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to add backup camera, backup camera[%s].", strLensId));
        return SVS_RESULT_FAILURE;
    }

    //��ʼ�����ݾ�ͷ����
    (void)pBakupDev->init_device(mb);

    // �����ϴν���ʱ��Ϊ0
    (void)backup_time_map_.rebind((char*)strLensId, 0);
    ACE_OS::last_error(0);
    SVS_LOG((SVS_LM_INFO, "Success to add backup camera. backup camera[%s].", strLensId));
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : add_device
 ��������  : ���ӱ��ݾ�ͷ����
 �������  : const char * strDevId:��Ҫ����ǰ��¼��ľ�ͷID
 �������  : CAC_Bakup_Record_Dev* :����ӵľ�ͷ����ָ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
CAC_Bakup_Record_Dev* CAC_Bakup_Record_Dev_Manager::add_device(const char * strDevId)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "Begin to add backup camera, backup camera[%s].", strDevId));

    // �ж��Ƿ���ڿ��еı��ݾ�ͷ����ռ�
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);

    if (free_list_.is_empty())
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to add camera because free_list_ is empty, backup camera[%s].",
            strDevId));

        return NULL;
    }

    // ��ȡ��ű��ݾ�ͷ����Ŀռ�
    char *pch = free_list_.delete_head();

    CAC_Bakup_Record_Dev *pTmpBakupDev = NULL;
    uint32_t nIndex = *((uint32_t *)(void*)pch);

    try
    {
        pTmpBakupDev = new (pch) CAC_Bakup_Record_Dev(strDevId, nIndex );
    }
    catch (...)
    {
        // �������ռ�ʧ�ܣ���ӡ��־��ʧ���˳�
        SVS_LOG((SVS_LM_ERROR,
            "Allocating memery for backup camera failed, backup camera[%s].",
            strDevId));

        pTmpBakupDev = NULL;
        return NULL;
    }

    // �ѱ��ݾ�ͷ�������map��
    int32_t bindResult = bakup_device_map_.bind(pTmpBakupDev->get_frontdev_id(), pTmpBakupDev);

    // ����ѱ��ݾ�ͷ�������map��ʧ��,��ô�Ѹþ�ͷ�����ٲ����������
    if (0 != bindResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Add backup camera to map failed, backup camera[%s].", strDevId));

        // �ͷű��ݶ���Ŀռ�
        *((uint32_t *)(void*)pch) = nIndex;
        (void)free_list_.insert_tail(pch);

        pTmpBakupDev->~CAC_Bakup_Record_Dev();
        // ����pclint�澯����placement new���ڴ���Ҫ�Żض��У�û���ڴ�й©
        pTmpBakupDev = NULL;//lint !e423
        return NULL;
    }

    // �ѱ��ݾ�ͷ����������ڱ���ǰ��¼��ľ�ͷvector
    using_vector_[nIndex] = pTmpBakupDev;

    return pTmpBakupDev;
}

/*****************************************************************************
 �� �� ��  : handle_routerinfo
 ��������  : ����¼�������������ص�ǰ��¼��ط�·����Ӧ��Ϣ
 �������  : const ACE_Message_Block * mb:·����Ӧ��Ϣ����
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::handle_routerinfo(const ACE_Message_Block * mb)
{
    SVS_TRACE();

    // �����Ϸ����ж�
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to handler route info, the pointer mb is NULL."));
        return SVS_RESULT_FAILURE;
    }

    SVS_CLIENTSVR_SETUP_MEDIA_STREAM_RESP_MSG  *pMsg =
        (SVS_CLIENTSVR_SETUP_MEDIA_STREAM_RESP_MSG*)(void*)mb->rd_ptr();
    uint16_t msgNo = reset_transactionno(pMsg->Header.TransactionNo);
    CAC_Bakup_Record_Dev *pBakDevice =get_device_by_msgno(msgNo);

    // ���û���ҵ���Ӧ�ı��ݾ�ͷ
    if (NULL == pBakDevice)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to handler route info because can not find backup camera. TransactionNo[%u].",
            msgNo));
        return SVS_RESULT_FAILURE;
    }

    SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBakDevice);
    (void)pBakDevice->handle_routerinfo(pMsg);

    // ɾ����Ϣmap�еı��ݾ�ͷ����
    (void)delete_msgno_map(msgNo);
    return SVS_RESULT_OK;

}
int32_t CAC_Bakup_Record_Dev_Manager::play_media_request(uint32_t streamId)
{
    // ����¼��ID��ȡ���ݾ�ͷ����
    CAC_Bakup_Record_Dev *pBakDevice = get_device_by_routerid(streamId);
    if (NULL == pBakDevice)
    {
        return SVS_RESULT_OK;
    }

    {
        SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBakDevice);
        if (SVS_RESULT_OK != pBakDevice->play_media_request())
        {
            return SVS_RESULT_FAILURE;
        }
    }

    return SVS_RESULT_OK;

}

/*****************************************************************************
 �� �� ��  : timer_callback
 ��������  : ��ʱ���ص�����
 �������  : int32_t eventId:�����豸���ݱ�ʾid��
             int32_t timerId:��ʱ��id��
             void * pArg:�������ָ��
 �������  : ��
 �� �� ֵ  : ��
*****************************************************************************/
void CAC_Bakup_Record_Dev_Manager::timer_callback(void * pArg, int32_t eventId, int32_t timerId, int32_t nDummy)
{
    SVS_TRACE();

    uint32_t msgLen = sizeof(SVS_TIMER_CALL_BACK_MSG);
    ACE_Message_Block *mb =
        g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK, msgLen );
    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR, "Allocating msg block failed. msg type[%d], msg size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK,
            msgLen ));

        return ;
    }

    SVS_TIMER_CALL_BACK_MSG* pMsg = (SVS_TIMER_CALL_BACK_MSG*)(void*)mb->wr_ptr();
    if(CAC_Bakup_Record_Dev::TIMER_TYPE_CHECK_STATUS == nDummy)
    {
        pMsg->MsgType = SVS_TIMER_CALL_BACK_MSG::BACKUP_RECORD_STATUS_TYPE;
    }
    else
    {
        pMsg->MsgType = SVS_TIMER_CALL_BACK_MSG::RECORDBAK_Dev_Manager_TYPE;
    }

    pMsg->ProcObject    = pArg;
    pMsg->EventId       = eventId;
    pMsg->TimerId       = timerId;
    pMsg->Dummy         = nDummy;
    mb->wr_ptr( msgLen );

    (void)CAC_RT_Record_Server::instance()->add_timer_callback_msg( mb );
    return;
}

/*****************************************************************************
 �� �� ��  : on_timer
 ��������  : ��ʱ���ص��������������¼�id�ҵ����ݾ�ͷ����Ȼ�����ٸ�id��Ӧ�Ķ�ʱ��
 �������  : int32_t eventId:�����豸���ݱ�ʾid��
             int32_t timerId:��ʱ��id
 �������  : ��
 �� �� ֵ  : ��
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::on_timer(int32_t eventId, int32_t timerId, int32_t nDummy)
{
    // ����Ǿ�ͷ״̬�������
    if (CAC_Bakup_Record_Dev::TIMER_TYPE_CHECK_STATUS == nDummy)
    {
        CAC_Bakup_Record_Dev *pBackupDevice = NULL;
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);
        BAKUP_DEV_MAP_ITER iter(bakup_device_map_);
        BAKUP_DEV_MAP_ENTRY *pEntry = NULL;
        std::list<CAC_Bakup_Record_Dev *> pBackupDevList;
        for (; 0 != iter.next(pEntry); (void)iter.advance())
        {
            pBackupDevice = pEntry->int_id_;
            (void)pBackupDevice->increase_reference();
            pBackupDevList.push_back(pBackupDevice);
            pBackupDevice->on_timer(eventId, timerId, nDummy);
        }

        while (!pBackupDevList.empty())
        {
            pBackupDevice = pBackupDevList.front();
            pBackupDevList.pop_front();
            SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBackupDevice);
        }

        return SVS_RESULT_OK;
    }

    eventId -= RECORD_MAX_LENS_NUM;
    CAC_Bakup_Record_Dev *pBakDevice = get_device((uint32_t)eventId);
    if (NULL != pBakDevice)
    {
        SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBakDevice);
        pBakDevice->on_timer(eventId, timerId,nDummy);
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,
            "Can not find backup camera. timerID[%d], eventID[%d].",
            eventId,
            timerId));
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : get_device
 ��������  : ���ݱ��ݾ�ͷ���豸id��ȡ���ݾ�ͷ����ָ��
 �������  : const char * strDevId:���ݾ�ͷID
 �������  : ��
 �� �� ֵ  : �ɹ�����:����ָ��,ʧ�ܷ���:NULL
*****************************************************************************/
CAC_Bakup_Record_Dev *CAC_Bakup_Record_Dev_Manager::get_device(const char * strDevId)
{
    SVS_TRACE();

    CAC_Bakup_Record_Dev * pBakDevice = NULL;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);
    if (0 != bakup_device_map_.find(strDevId, pBakDevice))
    {
        return NULL;
    }

    if (-1 == pBakDevice->increase_reference())
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to get backup camera because backup camera referrence is invalid. backup camera[%s].",
            strDevId));
        return NULL;
    }

    return pBakDevice;
}

/*****************************************************************************
 �� �� ��  : get_device
 ��������  : ���ݱ��ݾ�ͷ���ڲ�id��ȡ���ݾ�ͷ����ָ��
 �������  : const uint32_t internalId:���ݾ�ͷ�ڲ�ID
 �������  : ��
 �� �� ֵ  : �ɹ�����:����ָ��,ʧ�ܷ���:NULL
*****************************************************************************/
CAC_Bakup_Record_Dev* CAC_Bakup_Record_Dev_Manager::get_device(const uint32_t internalId)
{
    SVS_TRACE();

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);
    CAC_Bakup_Record_Dev *pBakDevice = using_vector_[internalId];
    if (NULL == pBakDevice)
    {
        return NULL;
    }

    // ���ü�����1
    int32_t increaseResult = pBakDevice->increase_reference();

    // ������1ʧ��,����NULL
    if (SVS_RESULT_FAILURE == increaseResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to get backup camera. InternalID[%u].", internalId));
        return NULL;
    }

    return pBakDevice;
}

/*****************************************************************************
 �� �� ��  : get_device_by_msgno
 ��������  : ������Ϣ��ˮ�ţ���ȡ�����豸��ַ
 �������  : uint16_t msgNo:��Ϣ��ˮ�ţ���RtRecordSvr�ڲ�Ψһ
 �������  : ��
 �� �� ֵ  : �ɹ�����:����ָ��,ʧ�ܷ���:NULL
*****************************************************************************/
CAC_Bakup_Record_Dev* CAC_Bakup_Record_Dev_Manager::get_device_by_msgno(uint16_t msgNo)
{
    SVS_TRACE();

    CAC_Bakup_Record_Dev *pBakDevice = NULL;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);
    if (0 != bakup_device_msgno_map_.find(msgNo, pBakDevice))
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to get backup camera by message number. msgNo[%u].", msgNo));
        return NULL;
    }

    // ���ü�����1
    int32_t increaseResult = pBakDevice->increase_reference();

    // ������1ʧ��,����NULL
    if (SVS_RESULT_FAILURE == increaseResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail increase backup camera referrence. msgNo[%u].", msgNo));
        return NULL;
    }

    return pBakDevice;
}

/*****************************************************************************
 �� �� ��  : get_device_by_routerid
 ��������  : ���ݸ���·��ID��ȡ���ݾ�ͷ����
 �������  : const uint32_t RouterId:·��ID
 �������  : ��
 �� �� ֵ  : �ɹ�����:����ָ��,ʧ�ܷ���:NULL
*****************************************************************************/
CAC_Bakup_Record_Dev* CAC_Bakup_Record_Dev_Manager::get_device_by_routerid(const uint32_t RouterId)
{
    SVS_TRACE();

    CAC_Bakup_Record_Dev *pBakDevice = NULL;
    CAC_Bakup_Record_Dev *pBakDeviceTemp = NULL;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);
    BAKUP_DEV_MAP_ITER iter(bakup_device_map_);
    BAKUP_DEV_MAP_ENTRY *pEntry = NULL;
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        pBakDeviceTemp = pEntry->int_id_;
        if (RouterId == pBakDeviceTemp->get_router_id())
        {
            pBakDevice = pBakDeviceTemp;
            break;
        }
    }

    // û���ҵ�,����NULL
    if (NULL == pBakDevice)
    {
//        SVS_LOG((SVS_LM_ERROR, "Fail to get backup camera by route ID. routeID[%u]", RouterId));
        return NULL;
    }

    // ���ҳɹ�,���ü�����1��,���ظö���ָ��
    int32_t increaseResult = pBakDevice->increase_reference();
    if (SVS_RESULT_FAILURE == increaseResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to increace backup camera referrence. routeID[%u].", RouterId));
        return NULL;
    }

    return pBakDevice;
}

/*****************************************************************************
 �� �� ��  : decrease_reference
 ��������  : �豸���ü�����������Ѿ�û�еط���ʹ���ˣ����ͷž�ͷ����
 �������  : CAC_Bakup_Record_Dev * pBakDevice:���ݾ�ͷ����ָ��
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::decrease_reference(CAC_Bakup_Record_Dev * pBakDevice)
{
    SVS_TRACE();

    // �����ж�
    if (NULL == pBakDevice)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to decrease backup camera reference. The pointer pBakDevice is NULL."));
        return SVS_RESULT_FAILURE;
    }

    uint32_t totalDev = 0;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);

        // �����ͷ���ڱ�ʹ��
        if (1 != pBakDevice->decrease_reference())
        {
            pBakDevice = NULL;
            return SVS_RESULT_OK;
        }

        // �����ͷ����ʹ�ã���������
        (void)bakup_device_msgno_map_.unbind(pBakDevice->get_router_msgno());
        if (0 != bakup_device_map_.unbind(pBakDevice->get_frontdev_id()))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Fail to delete backup camera because can not find backup camera in map. "
                "backup camera[%s].",
                pBakDevice->get_frontdev_id()));
        }
        // �����ϴν���ʱ��Ϊ��ǰʱ��
        ACE_CString strCameraId = pBakDevice->get_frontdev_id();
        (void)backup_time_map_.rebind(strCameraId, time(NULL));
        ACE_OS::last_error(0);
        using_vector_[pBakDevice->internal_id()] = NULL;
        totalDev = bakup_device_map_.current_size();
    }

    SVS_LOG((SVS_LM_WARNING,
        "Success to stop backup camera. backup camera[%s],current backup camera num[%d].",
        pBakDevice->get_frontdev_id(),
        totalDev ));

    // ��λ,���豸������ж���
    uint32_t id = pBakDevice->internal_id();
    (void)pBakDevice->reset();
    pBakDevice->~CAC_Bakup_Record_Dev();
    *((uint32_t *)(void*)pBakDevice) = id;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
    (void)free_list_.insert_tail((char *)pBakDevice);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : proc_send_report
 ��������  : ���յ����ȷ��ͱ����Ժ󣬷��ؽ��Ƚ��ձ��棬��Ϊ���ÿͻ��˻طŽӿڣ�
             RtRecordSvr���漰������������ע�������ݣ�ֱ�ӷ������ձ��漴�ɡ�
 �������  : const ACE_Message_Block * mb:���ȱ�����Ϣ����
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::proc_eos_packet(uint32_t streamId)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO,
        "Recv eos packet. streamID[%d],",
        streamId));

    if (0 == streamId)
    {
        return SVS_RESULT_OK;
    }

    // ����¼��ID��ȡ���ݾ�ͷ����
    CAC_Bakup_Record_Dev *pBakDevice = get_device_by_routerid(streamId);
    if (NULL == pBakDevice)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Recv eos packet but can not find backup camera. streamID[%d].",
            streamId));
        return SVS_RESULT_OK;
    }

    {
        SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBakDevice);
        if (SVS_RESULT_OK != pBakDevice->proc_eos_packet())
        {
            return SVS_RESULT_FAILURE;
        }
    }

    return SVS_RESULT_OK;


}

/*****************************************************************************
 �� �� ��  : stop_device
 ��������  : ���ݾ�ͷIDֹͣ���ݾ�ͷ����
 �������  : const char * strDevId:��ͷID
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::stop_device(const char * strDevId)
{
    SVS_TRACE();

    // ���豸
    CAC_Bakup_Record_Dev *pBakDevice = NULL;
    pBakDevice = get_device(strDevId);
    if (NULL == pBakDevice)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Fail to stop backup camera because can not find camera in map. "
            "backup camera[%s].",
            strDevId));
        return SVS_RESULT_OK;
    }

    {
        SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBakDevice);
        (void)pBakDevice->stop_backup_record();
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : stop_device
 ��������  : ָֹͣ��¼��ID��ǰ��¼�񱸷ݾ�ͷ
 �������  : const unit32_t RouterId:·��ID
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::stop_device(const uint32_t RouterId)
{
    SVS_TRACE();

    // ����ָ��·�ɵ�ǰ�˱����豸
    CAC_Bakup_Record_Dev *pBakDevice = get_device_by_routerid( RouterId );
    if (NULL == pBakDevice)
    {
        return SVS_RESULT_OK;
    }

    // ֹͣǰ��¼�񱸷�
    SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBakDevice);
    int32_t stopRecordResult = pBakDevice->stop_backup_record();

    return stopRecordResult;
}

/*****************************************************************************
 �� �� ��  : stop_all_device
 ��������  : ֹͣ����ǰ��¼�񱸷ݾ�ͷ
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::stop_all_device()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "Begin to stop all backup cameras."));

    int32_t retryTime = 0;
    int32_t totalRetryTime = 0;
    CAC_Bakup_Record_Dev * pBakDev = NULL;
    uint32_t bakDevNum = 0;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);
        bakDevNum = bakup_device_map_.current_size();
    }

    while (0 < bakDevNum)
    {
        // �������ֹͣ�豸�����Ѿ��ﵽ�����ˣ���ǿ���˳��˳�
        if (SVS_RETRY_STOP_RECORD_TOTAL_TIMES < ++totalRetryTime)
        {
            SVS_LOG((SVS_LM_ERROR,"Fail to stop backup camera and force to exit. try times[%d].",
                totalRetryTime));
            break;
        }
        //retryTime���㣬��֤�����while����ÿ�δ�0��ʼ����
        retryTime = 0;
        SVS_LOG((SVS_LM_INFO, "Wait for stopping backup camera number [%u].", bakDevNum));

        {
            ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);
            BAKUP_DEV_MAP_ITER iter(bakup_device_map_);
            BAKUP_DEV_MAP_ENTRY *pEntry = NULL;
            std::list<CAC_Bakup_Record_Dev *> pBackupDevList;
            // ֹͣ����¼��
            for (; 0 != iter.next(pEntry); (void)iter.advance())
            {
                pBakDev = pEntry->int_id_;
                (void)pBakDev->increase_reference();
                pBackupDevList.push_back(pBakDev);
                if (0 != pBakDev->stop_backup_record(false))
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Fail to stop backup camera. backup camera[%s].",
                        pBakDev->get_frontdev_id()));
                }
            }

            while (!pBackupDevList.empty())
            {
                pBakDev = pBackupDevList.front();
                pBackupDevList.pop_front();
                SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBakDev);
            }
        }

        // �ȴ����б��ݽ���
        while (0 < bakDevNum)
        {
            {
                ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);
                bakDevNum = bakup_device_map_.current_size();
            }

            SVS_LOG((SVS_LM_INFO, "Current backup camera number [%u].", bakDevNum));

            ACE_Time_Value tv( 0, SVS_CHECK_STOP_RECORD_INTERVAL_TIME );
            (void)ACE_OS::sleep( tv );

            // ����ֹͣSVS_RETRY_STOP_RECORD_TIMES��
            if (SVS_RETRY_STOP_RECORD_TIMES < retryTime++)
            {
                break;
            }
        }
    }

    // �õ�ǰ״̬Ϊ��ѯ�豸״̬
    SVS_LOG((SVS_LM_INFO, "Success to stop all backup camera."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : debug_all_bakup_device_info
 ��������  : �������б��ݾ�ͷ��Ϣ������Ϣ�ŵ����Ի�����
 �������  : char* debugBuf:���Ի�����
             const uint32_t bufLen:����������
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::
debug_all_bakup_device_info(char* debugBuf,const uint32_t bufLen)
{
    SVS_TRACE();

    (void)ACE_OS::snprintf( debugBuf,
                            bufLen,
                            "\tStatus specification [0:Init,1:Sleep,2:Request route,3:Connect VTDU,"
                            "4:Receice stream,5:Waiting over,32:Stop]\n\n"
                            "\t       BackupCameraID        BackupCameraStatus\n" );

    CAC_Bakup_Record_Dev *pDevice = NULL;
    uint16_t deviceNumInit = 0;
    uint16_t deviceNumSleep = 0;
    uint16_t deviceNumReqVtdu = 0;
    uint16_t deviceNumConnVtdu = 0;
    uint16_t deviceNumRecvFile = 0;
    uint16_t deviceNumOver = 0;
    uint16_t deviceNumStop = 0;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);

    BAKUP_DEV_MAP_ITER iter(bakup_device_map_);
    BAKUP_DEV_MAP_ENTRY *pEntry = NULL;
    size_t writeLen = 0;
    int32_t lensCount = 0;

    int32_t curBufLen = 0;
    for (; 0 != iter.next(pEntry); (void)iter.advance() )
    {
        pDevice = pEntry->int_id_;
        writeLen = ACE_OS::strlen( debugBuf );

        //�����������꣬�˳�ѭ��
        curBufLen = (int32_t)(bufLen-writeLen);
        if( 0 >= curBufLen )
        {
            break;
        }
       (void)ACE_OS::snprintf(   &debugBuf[writeLen],
                            (uint32_t)curBufLen,
                            "\t%s     %5d     \n",
                            pDevice->get_frontdev_id(),
                            pDevice->get_state());
        switch(pDevice->get_state())
        {
            case RECORDBAK_STATE_INIT:
                deviceNumInit++;
                break;
            case RECORDBAK_STATE_SLEEP:
                deviceNumSleep++;
                break;
            case RECORDBAK_STATE_REQUEST_ROUTER:
                deviceNumReqVtdu++;
                break;
            case RECORDBAK_STATE_CONNECT_VTDU:
                deviceNumConnVtdu++;
                break;
            case RECORDBAK_STATE_RECEIVING_FILE:
                deviceNumRecvFile++;
                break;
            case RECORDBAK_STATE_WAITING_OVER:
                deviceNumOver++;
                break;
            default:
                deviceNumStop++;
                break;
        }
        ++lensCount;
    }
    writeLen = ACE_OS::strlen( debugBuf );
    curBufLen = (int32_t)(bufLen-writeLen);
    if( 0 < curBufLen )
    {
        (void)ACE_OS::snprintf(   &debugBuf[writeLen],
                            (uint32_t)curBufLen,
                            "\n"
                            "\t  CameraNum            [%5d]\n"
                            "\t  Init                 [%5d]\n"
                            "\t  Sleep                [%5d]\n"
                            "\t  Request route        [%5d]\n"
                            "\t  Connect VTDU         [%5d]\n"
                            "\t  Receice stream       [%5d]\n"
                            "\t  Waiting over         [%5d]\n"
                            "\t  Stop                 [%5d]\n"
                            "\n\n",
                            lensCount,
                            deviceNumInit,
                            deviceNumSleep,
                            deviceNumReqVtdu,
                            deviceNumConnVtdu,
                            deviceNumRecvFile,
                            deviceNumOver,
                            deviceNumStop);
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : debug_get_device_info
 ��������  : telnet���Խӿڣ���ȡָ����ͷ����ϸ��Ϣ
 �������  : const char *strFrontDevId:ǰ�˾�ͷID
             char *strDeviceInfo:���ǰ�˾�ͷ��Ϣ������
             uint32_t szDeviceInfo:����������
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::
debug_get_device_info( const char *strFrontDevId, char *strDeviceInfo, uint32_t szDeviceInfo )
{
    if ((NULL == strDeviceInfo) || (NULL == strFrontDevId))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to get backup camera. strDeviceInfo[0x%08x], strFrontDevId[0x%08x].",
            strDeviceInfo,
            strFrontDevId));
        return SVS_RESULT_FAILURE;
    }

    CAC_Bakup_Record_Dev *pBakupDevice = NULL;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
    if (0 != bakup_device_map_.find(strFrontDevId, pBakupDevice))
    {
        (void)ACE_OS::snprintf( strDeviceInfo,
                                szDeviceInfo,
                                "Backup camera is not found. backup camera[%s]",
                                strFrontDevId  );
        return 1;
    }

    int32_t getDeviceInfoResult = pBakupDevice->debug_get_device_info(
                                    strDeviceInfo,
                                    szDeviceInfo );

    return getDeviceInfoResult;
}

/*****************************************************************************
 �� �� ��  : is_lens_bakup_record
 ��������  : ���ݾ�ͷid����ָ����ͷ�Ƿ���ǰ��¼�񱸷�
 �������  : const char * strLensId:��ͷID
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
bool CAC_Bakup_Record_Dev_Manager::is_lens_bakup_record(const char * strLensId)
{
    SVS_TRACE();

    CAC_Bakup_Record_Dev *pBakupDevice = get_device(strLensId);

    if (NULL == pBakupDevice)
    {
        return false;
    }

    SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBakupDevice);

    return true;
}

/*****************************************************************************
 �� �� ��  : update_backup_device_flag
 ��������  : ˢ��ǰ��¼�񱸷ݾ�ͷ��Ϣ�������ͷ��󱸷ݵĽ���ʱ���Ѿ�����1��
             Сʱ�ˣ���ɾ�������ͷ��ǰ��¼�񱸷ݽ�����¼
 �������  : ��
 �������  : ��
 �� �� ֵ  : �ɹ�����:0,ʧ�ܷ���:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::update_backup_device_flag()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);
    SVS_LOG((SVS_LM_INFO,
        "The size of backup last end time flag map is [%u]. ",
        backup_time_map_.current_size()));

    // ��������map
    ACE_CString strCameraId;
    time_t lastEndTime = 0;
    time_t nowTime = time(NULL);
    BACKUP_TIME_MAP_ITER iter(backup_time_map_);
    BACKUP_TIME_MAP_ENTRY *pEntry = NULL;
    std::list<ACE_CString> cameraList;
    for (; 0 != iter.next(pEntry); (void)iter.advance())
    {
        strCameraId = pEntry->ext_id_;
        lastEndTime = pEntry->int_id_;
        SVS_LOG((SVS_LM_DEBUG,
            "Delete backup device last end time flag."
            "caemraId[%s], now time[%u], last end time[%u]",
            strCameraId.c_str(),
            nowTime,
            lastEndTime));

        // �������ʱ��Ϊ0����ʾ���ڱ��ݣ�������
        if(0 == lastEndTime)
        {
            continue;
        }

        // �������ʱ�仹û�д���20���ӣ�������
        if(SVS_BACKUP_FALG_HOLD_TIME_LEN > (nowTime - lastEndTime))
        {
            continue;
        }

        // ���Ľ���ʱ���Ѿ�����һ��Сʱ�ˣ�����������ͷ�ı��ݽ���ʱ��
        cameraList.push_back(strCameraId);
    }

    while (!cameraList.empty())
    {
        strCameraId = cameraList.front();
        cameraList.pop_front();
        (void)backup_time_map_.unbind(strCameraId.c_str());
        SVS_LOG((SVS_LM_DEBUG,
            "Delete backup device last end time flag.caemraId[%s]",
            strCameraId.c_str()));
    }

    return SVS_RESULT_OK;
}
//======================CAC_Bakup_Record_Dev_Manager END=========================


