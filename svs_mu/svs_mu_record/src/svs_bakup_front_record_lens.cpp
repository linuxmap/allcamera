/*****************************************************************************
   版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文件名          : SVS_Bakup_Front_Record_Lens.cpp
  版本号          : 1.0
  生成日期        : 2008-8-15
  最近修改        :
  功能描述        : 前端录像备份镜头管理类和备份镜头类，管理类实现对每个备份镜头
                    对象的管理，备份镜头负责根据备份任务结合平台和前端录像索引的
                    重叠情况，通过录像回放请求把前端录像备份到平台的磁盘阵列上。
  函数列表        :
  修改历史        :
  1 日期          : 2008-8-15
    修改内容      : 生成
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
函 数 名  : CAC_Bakup_Record_Dev
功能描述  : 备份镜头的构造函数
输入参数  : const char* strLensId:镜头ID
            const uint32_t nIndex:镜头内部索引
输出参数  : 无
返 回 值  : 成功:1   失败:0
修改历史  :
*****************************************************************************/
CAC_Bakup_Record_Dev::CAC_Bakup_Record_Dev(const char*    strLensId,
                                           const uint32_t nIndex )
                                    : CSVS_Record_Lens(strLensId, nIndex)
{
    referenc_count_     = 1;
    state_              = RECORDBAK_STATE_INIT;

    router_id_          = 0;
    status_start_time_  = time(NULL);

    // 初始化成员变量
    referenc_count_ = 1;
    state_ = RECORDBAK_STATE_INIT;

    // 镜头类型标示设置为前端录像备份
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

    // 初始化成员变量
    referenc_count_ = 1;
    state_ = RECORDBAK_STATE_INIT;
    m_ulIOHandlerIndex = (uint32_t)IO_INVALID_HANDLE;

    // 镜头类型标示设置为前端录像备份
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
函 数 名  : reset
功能描述  : 复位对象设置，清理资源
输入参数  : 无
输出参数  : 无
返 回 值  : 成功返回:0,失败返回:-1
修改历史  :
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
函 数 名  : init_device
功能描述  : 初始化前端录像设备
输入参数  : const ACE_Message_Block *mb:前端录像备份索引信息
输出参数  : 无
返 回 值  : 成功:true   失败:false
修改历史  :
*****************************************************************************/
bool CAC_Bakup_Record_Dev::init_device(const ACE_Message_Block *mb)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO,
        "Begin initialize backup camera, cameraID[%s], internalId[%d].",
        str_frontdev_id_,
        internal_id_));

    (void)set_status(RECORDBAK_STATE_INIT);

    //备份任务一次最多只有一个FILEINFO，且不超过RECORDBAK_SECTION_MAX_NUM个SECTIONINFO
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

    //解析前端录像索引信息,拷贝到缓存
    SVS_RECORDERSVR_DISTRIBUTE_RECORDBAK_TASK_REQ *pReq =
        (SVS_RECORDERSVR_DISTRIBUTE_RECORDBAK_TASK_REQ*)(void*)(mb->rd_ptr());

    // 进行消息最小长度校验，如果小于最小长度，则可避免越界读取
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

    //获取该索引信息中总共包含的段信息，并计算总共需要的缓冲区空间
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

    //把备份任务存入回放vector
    time_t currTime = time(NULL); // 当前系统时间
    replay_sections_vector_.clear(); // 清空回放缓冲区

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

        //如果开始时间不小于结束时间，则跳过此非法段
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

        //合法的段 压入vector
        replay_sections_vector_.push_back(tempIndexSect);
    }

    // 4.释放缓存
    char * pTemp = (char*)(void*)frontFileInfo;
    SVS_DELETE(pTemp, SVS_DELETE_MULTI);
    frontFileInfo = NULL;

    if (1 < replay_sections_vector_.size())
    {
        delete_overlap_sections(replay_sections_vector_);

        // 按照开始时间，把需要回放的段排序
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

    //打印最终需要回放的段信息
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

    //请求路由
    (void)request_router();

    return true;
}

/*****************************************************************************
函 数 名  : delete_overlap_sections
功能描述  : 屏蔽掉前端重复的索引段信息
输入参数  : SECTIONINFO_VECTOR & sectVector:前端需要回放的索引段容器
输出参数  : 无
返 回 值  : 无
修改历史  :
*****************************************************************************/
void CAC_Bakup_Record_Dev::delete_overlap_sections(SECTIONINFO_VECTOR & sectVector)const
{
    SVS_TRACE();

    // 1.索引重叠的段去掉
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
        // 和当前所有段匹配
        for (uint32_t m = 0; m < tempSectNum; m++)
        {
            // 1.如果已经包含该段
            if ((tempSectVector[m].StartTime <= sectVector[currIndex].StartTime)
             && (tempSectVector[m].EndTime >= sectVector[currIndex].EndTime))
            {
                isOver = true;
                break;
            }
            // 如果被包含了前半部分
            else if((tempSectVector[m].StartTime <= sectVector[currIndex].StartTime)
             && (tempSectVector[m].EndTime > sectVector[currIndex].StartTime)
             && (tempSectVector[m].EndTime < sectVector[currIndex].EndTime))
            {
                // 截取后面一段加入未匹配队列
                isOver = true;
                tempSectInfo.StartTime = tempSectVector[m].EndTime + 1;
                tempSectInfo.EndTime = sectVector[currIndex].EndTime;
                sectVector.push_back(tempSectInfo);
                break;
            }
            // 如果后面一段已经存在
            else if((tempSectVector[m].StartTime < sectVector[currIndex].EndTime)
             && (tempSectVector[m].EndTime >= sectVector[currIndex].EndTime)
             && (tempSectVector[m].StartTime > sectVector[currIndex].StartTime))
            {
                // 截取前面一段加入未匹配队列
                isOver = true;
                tempSectInfo.StartTime = sectVector[currIndex].StartTime;
                tempSectInfo.EndTime = tempSectVector[m].StartTime - 1;
                sectVector.push_back(tempSectInfo);
                break;
            }
            // 如果包含了当前的某段
            else if((tempSectVector[m].StartTime > sectVector[currIndex].StartTime)
             && (tempSectVector[m].EndTime < sectVector[currIndex].EndTime))
            {
                // 分别截取两边的两端加入未匹配队列
                isOver = true;

                // 前端未重叠的一段
                tempSectInfo.StartTime = sectVector[currIndex].StartTime;
                tempSectInfo.EndTime = tempSectVector[m].StartTime - 1;
                sectVector.push_back(tempSectInfo);

                // 后面未重叠的一段
                tempSectInfo.StartTime = tempSectVector[m].EndTime + 1;
                tempSectInfo.EndTime = sectVector[currIndex].EndTime;
                sectVector.push_back(tempSectInfo);
                break;
            }
        }

        // 如果不和当前的任何点交叠，则加入
        if (!isOver)
        {
            tempSectVector.push_back(sectVector[currIndex]);
        }

        ++currIndex;
    }

    // 回拷贝
    sectVector.clear();
    for(uint32_t n = 0;n < tempSectVector.size();n++)
    {
        sectVector.push_back(tempSectVector[n]);
    }
    return;
}

/*****************************************************************************
 函 数 名  : send_stop_recv_msg
 功能描述  : 功能:发送停止消息给接收线程与写录像线程
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::send_stop_recv_msg()
{
    if((uint32_t)IO_INVALID_HANDLE == m_ulIOHandlerIndex)
    {
        return SVS_RESULT_OK;
    }

    // 从媒体处理器中移除注册的句柄
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
函 数 名 : drop_next_section
功能描述 : 设置定时器休眠一段时间，再进行下一段回放
输入参数 : 无
输出参数 : 无
返回值   : 成功:0   失败:-1
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

    // 如果最后一段回放完成，则停止备份
    if (cur_replay_sect_index_ >=  replay_sections_vector_.size() - 1)
    {
        SVS_LOG((SVS_LM_INFO,
            "Backup camera has finished backup and will stop camera. backup camera[%s].",
            str_frontdev_id_));
        (void)stop_backup_record();
        return SVS_RESULT_OK;
    }

    // 回放下一段
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

    // 关闭路由
    (void)shutdown_router();

    // 停止接收流
    (void)send_stop_recv_msg();

    // 设置状态为休眠状态
    (void)set_status(RECORDBAK_STATE_SLEEP);

    return SVS_RESULT_OK;
}

/******************************************************************************
函 数 名 : handle_routerinfo
功能描述 : 根据StoreSvr返回的VTDU地址，向VTDU注册
输入参数 : const void *pbyMsg:StoreSvr响应消息
输出参数 : 无
返回值   : 成功:0   失败:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::handle_routerinfo(const void *pbyMsg)
{
    SVS_TRACE();

    SVS_CLIENTSVR_SETUP_MEDIA_STREAM_RESP_MSG *pRouterRep = (SVS_CLIENTSVR_SETUP_MEDIA_STREAM_RESP_MSG*)pbyMsg;

    // 判断当前是否已经存在路由了
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

    // 保存路由id和vtdu地址
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

    // 设置等待VTDU响应定时器
    (void)set_status(RECORDBAK_STATE_CONNECT_VTDU);

    return SVS_RESULT_OK;
}
int32_t CAC_Bakup_Record_Dev::play_media_request()
{
    // 申请发流
    // 定义和初始化请求消息
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

    // 消息号
    router_msgno_ = CAC_RT_Record_Server::instance()->transaction_no();

    // 封装消息头
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


    // 发送请求发流消息
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
函 数 名 : notify_vtdu
功能描述 : 创建socket，向VTDU发送注册消息
输入参数 : 无
输出参数 : 无
返回值   : 成功:0   失败:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::start_backup_record()
{

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, mutex_, SVS_RESULT_FAILURE);
    time_remove_value = time(NULL) - replay_sections_vector_[cur_replay_sect_index_].StartTime;
    SVS_LOG((SVS_LM_INFO, "Start to backup front record.IO handle[%u], time remove value[%d]",
        m_ulIOHandlerIndex,
        time_remove_value));

    // 从media processor申请io handler
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

    // 设置
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

    // 开始录像
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
 函 数 名  : stop_backup_record
 功能描述  : 停止备份镜头，首先删除定时器，然后停止路由，通知接收线程和写文件
             线程退出，最后删除镜头对象
 输入参数  : bool procRouter:标识是否停止路由
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
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

        // 设置状态为停止
        (void)set_status(RECORDBAK_STATE_WAITING_OVER);
    }

    // 停止路由
    if (procRouter)
    {
        (void)shutdown_router();
    }

    (void)set_status(FRONT_DEVICE_STATE_STOP);

    SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(CAC_Bakup_Record_Dev_Manager::instance(), this);

    // 停止录像
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
函 数 名 : on_timer
功能描述 : 定时器回调函数，根据备份设备当前状态可以确定定时器的属性，做响应处理
输入参数 : 无
输出参数 : 无
返回值   : 无
*****************************************************************************/
void CAC_Bakup_Record_Dev::on_timer(int32_t, int32_t,int32_t nDummy)
{
    // 保留接口
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
            // 如果是等待结束
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
    // 如果还没有到最大超时时间，则不用处理
    if ((currTime - status_start_time_) < REQUEST_ROUTER_INTERVAL_TIME)
    {
        return ;
    }

    // 路由等待超时，结束备份过程
    SVS_LOG((SVS_LM_WARNING,
        "Backup camera request route timed out and stop backup camera. backup camera[%s].",
        str_frontdev_id_));
    (void)stop_backup_record();

    return ;
}

/*****************************************************************************
 函 数 名  : recv_stream_on_timer
 功能描述  : 定时器回调，录像状态是接收流状态，判断接流和写录像是否超时
 输入参数  : NA
 输出参数  : NA
 返 回 值  : NA
 修改历史  :
*****************************************************************************/
void CAC_Bakup_Record_Dev::revc_stream_on_timer()
{
    // 接流超时，停止前端录像备份
    if (SVS_RESULT_OK != SVS_Media_Processor::instance().get_handle_status(m_ulIOHandlerIndex))
    {
        // 停止前端录像备份
        SVS_LOG((SVS_LM_WARNING,
            "Backup camera write record file timed out. backup camera[%s].", str_frontdev_id_));

        (void)stop_backup_record();

        return ;
    }

    // 如果所接收到的流已经收到了所需要的，则停止接流
    time_t lastRecvTime = 0 ;

    int32_t nRet = SVS_Media_Processor::instance().get_last_recv_time(lastRecvTime,m_ulIOHandlerIndex);
    if(SVS_RESULT_OK != nRet)
    {
        // 停止前端录像备份
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
    // 如果句柄为-1，且没有超时，则不用查询状态
    time_t currTime = time(NULL);
    if(((uint32_t)IO_INVALID_HANDLE == m_ulIOHandlerIndex)
    && (VTDU_SEND_DATA_MIN_TIMEOUT > (currTime - status_start_time_)))
    {
        return;
    }

    int32_t result = SVS_Media_Processor::instance().get_handle_status(m_ulIOHandlerIndex);
    // 已经接收到VTDU的响应，则转换状态
    if (SVS_RESULT_OK == result)
    {
        (void)set_status(RECORDBAK_STATE_RECEIVING_FILE);
        return ;
    }
    else if(SVS_ERR_WAIT_RESP == result ) // 如果正在等待VTDU响应，并且没有超时
    {
        return ;
    }

    // 连接VTDU超时，停止前端录像备份
    SVS_LOG((SVS_LM_ERROR,
        "Backup camera connect to VTDU timed out and stop backup camera. backup camera[%s].",
        str_frontdev_id_));
    (void)stop_backup_record();

    return ;
}

void CAC_Bakup_Record_Dev::sleep_on_timer()
{
    // 避免查询太频繁，限制至少5s以后再查询一次
    time_t currTime = time(NULL);
    // 如果时间已经超时，则重新申请路由,如果没有超时，则不用理会
    if ((currTime - status_start_time_) < REQUEST_ROUTER_DELAY_TIME)
    {
        return ;
    }

    uint32_t handleIndex = (uint32_t)IO_INVALID_HANDLE;
    // 判断改镜头的底层handle是否已经释放，如果还没有释放，则等待
    int32_t nRet = SVS_Media_Processor::instance().get_handle_close_status(str_frontdev_id_,
                                                                    LENS_TYPE_RECORDBAK,
                                                                    0,handleIndex);
    // 如果handle已经释放，表示已经完成备份了
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
 函 数 名  : proc_send_report
 功能描述  : 处理发送报告
 输入参数  : uint16_t
             uint32_t timeTick:时间戳
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
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
 函 数 名  : request_router
 功能描述  : 前端录像回放路由请求，该部分全部借用客户端录像回放接口，接口中涉及
             的UserID统一填写为空，SessionID统一填写为RtRecord的设备ID，实现时
             RtRecordSvr模拟客户端应用服务器
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
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

    // 如果回放的段下标已经越界，则停止备份
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

    // 释放原来的路由
    if (router_id_ > 0)
    {
        (void)shutdown_router();
    }

    // 定义和初始化请求消息
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

    // 消息号
    router_msgno_ = CAC_RT_Record_Server::instance()->transaction_no();

    // 封装消息头
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

    // 回放当前section的时间段信息
    (void)SVS_SS_UTILITIES::time2str((char*)pRespMsg->TimeSpan.StartTime,
                     sizeof(pRespMsg->TimeSpan.StartTime),
                     replay_sections_vector_[cur_replay_sect_index_].StartTime);
    (void)SVS_SS_UTILITIES::time2str((char*)pRespMsg->TimeSpan.EndTime,
                     sizeof(pRespMsg->TimeSpan.EndTime),
                     replay_sections_vector_[cur_replay_sect_index_].EndTime);

    // 发送回放请求消息
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

    // 把消息号对应的设备信息加入map表
    (void)CAC_Bakup_Record_Dev_Manager::instance()->add_msgno_map(this, router_msgno_);

    // 设置等待路由定时器
    (void)set_status(RECORDBAK_STATE_REQUEST_ROUTER);

    SVS_LOG((SVS_LM_INFO,
        "Success to send setup media message. backup camera[%s], msgNo[%u].",
        str_frontdev_id_,
        router_msgno_));

    return SVS_RESULT_OK;

}

/*****************************************************************************
 函 数 名  : shutdown_router
 功能描述  : 向录像管理服务器发送前端录像回放停止消息，直接模拟客户端应用服务器
             发送前端录像回放停止请求。
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::shutdown_router()
{
    SVS_TRACE();

    // 如果录像id为0，则不需要发送停止路由消息
    if (0 == router_id_)
    {
        SVS_LOG((SVS_LM_WARNING,"RouteID is 0. No need to send stop route message."));
        return SVS_RESULT_OK;
    }

    SVS_LOG((SVS_LM_INFO,
        "Send tear down media request message. backup camera[%s], routeID[%u].",
        str_frontdev_id_,
        router_id_));

    // 构造停止录像消息
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

    // 停止前端录像
    pRespMsg->BusinessType = BUSINESS_TYPE_FRONT_DOWNLOAD;



    // 发送消息
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

    // 清空路由ID
    router_id_ = 0;

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : debug_get_device_status
 功能描述  : 获取该录像设备的状态，由INT型转换成string型，telent调试用
 输入参数  : NA
 输出参数  : string &strDeviceStatus:录像设备状态string表示
 返 回 值  : NA
 修改历史  :
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
 函 数 名  : debug_get_device_info
 功能描述  : 调试获取设备信息
 输入参数  : char *strDeviceInfo:备份镜头ID
             uint32_t szDeviceInfo:
 输出参数  : 无
 返 回 值  : 成功:0 失败:-1
 修改历史  :
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev::debug_get_device_info( char *strDeviceInfo, uint32_t ulBuffLen )
{
    string strDeviceStatus;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, obj, mutex_, false);
    debug_get_device_status(strDeviceStatus);

    char strTime[SVS_STR_TIME_MAX_LEN];

    // 当前回放的段信息
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
 函 数 名  : init
 功能描述  : 初始化前端录像设备管理类
 输入参数  : uint32_t nMaxLens:系统镜头最大数，是实时录像和备份镜头之和
 输出参数  : 无
 返 回 值  : 成功:true 失败:false
 修改历史  :
*****************************************************************************/
bool CAC_Bakup_Record_Dev_Manager::init(uint32_t nMaxLens)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO,
        "Init CAC_Bakup_Record_Dev_Manager, "
        "System max camera num[%u].",
        nMaxLens));

    max_bakup_device_ = nMaxLens;

    // 申请最大个备份镜头对象空间，存入free_list_，加锁
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
            // 如果申请备份镜头对象空间，退出循环，释放已经申请的空间
            SVS_LOG((SVS_LM_ERROR, "create CAC_Bakup_Record_Dev failed."));

            break;
        }

        // 保存索引
        *((uint32_t *)pch) = lensIndex;
        (void)free_list_.insert_tail(pch);
        using_vector_.push_back(NULL);
    }

    // 申请缓冲失败，清空以前申请的缓冲区
    if (lensIndex != nMaxLens)
    {
        ACE_DLList_Iterator <char> iter( free_list_ );
        while (!iter.done ())
        {
            char *freeBuff = iter.next ();
            (void)iter.remove ();
            SVS_DELETE( freeBuff, SVS_DELETE_MULTI );
        }

        // 前面已用SVS_DELETE释放了内存，屏蔽lint告警
        using_vector_.clear();
        return false;       //lint !e429
    }

    // 失败时已清理了申请的内存，成功后放入到空闲链表中，直到应用退出时才释放
    return true;   //lint !e429
}

/*****************************************************************************
 函 数 名  : bakup_record_notify
 功能描述  : 解析前端录像备份消息内容，判断当前镜头是否存在，如果存在则不处理备份
             任务，否则创建备份镜头对象
 输入参数  : const ACE_Message_Block * mb:前端录像备份消息
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
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

    //消息合法性检测
    SVS_RECORDERSVR_DISTRIBUTE_RECORDBAK_TASK_REQ* pReq =
        (SVS_RECORDERSVR_DISTRIBUTE_RECORDBAK_TASK_REQ*)(void*)mb->rd_ptr();

    //如果文件数不大于0
    if (0 == pReq->FileCount)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Backup front-end record file failed, because front-end record index num is 0 in msg block."));

        return SVS_RESULT_OK;
    }

    //获得镜头ID信息,根据镜头ID获取备份镜头对象
    char strLensId[LOCALIZE_DEVICE_ID_MAX_LEN + 1] = {'\0'};

    SetLocalizeDeviceId((uint8_t *)strLensId,
                    sizeof(strLensId),
                    (const char *)pReq->DeviceID,
                    NLS_DEVICE_ID_MAX_LEN);

    CAC_Bakup_Record_Dev * pBakupDev = NULL;

    //查找当前备份的设备
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
        // 查找上次结束的时间，如果上次备份结束的时间小于2分钟，则不备份
        // 可能上次结束时间是0，代表镜头正在备份或没有找到，但是可以由下异步保证
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

    // 如果镜头正在备份前端录像，结束备份备份任务
    if (NULL != pBakupDev)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Backup camera is working now, so it can not deal this backup task."
            "backup camera[%s].",
            strLensId));
        return SVS_RESULT_OK;
    }

    // 如果正在录像的镜头和正在备份前端录像的镜头数达到了上线，则不备份
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

    //添加备份镜头对象
    pBakupDev = add_device(strLensId);
    if (NULL == pBakupDev)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to add backup camera, backup camera[%s].", strLensId));
        return SVS_RESULT_FAILURE;
    }

    //初始化备份镜头对象
    (void)pBakupDev->init_device(mb);

    // 更新上次结束时间为0
    (void)backup_time_map_.rebind((char*)strLensId, 0);
    ACE_OS::last_error(0);
    SVS_LOG((SVS_LM_INFO, "Success to add backup camera. backup camera[%s].", strLensId));
    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : add_device
 功能描述  : 增加备份镜头对象
 输入参数  : const char * strDevId:需要备份前端录像的镜头ID
 输出参数  : CAC_Bakup_Record_Dev* :所添加的镜头对象指针
 返 回 值  : 成功返回:0,失败返回:-1
*****************************************************************************/
CAC_Bakup_Record_Dev* CAC_Bakup_Record_Dev_Manager::add_device(const char * strDevId)
{
    SVS_TRACE();

    SVS_LOG((SVS_LM_INFO, "Begin to add backup camera, backup camera[%s].", strDevId));

    // 判断是否存在空闲的备份镜头对象空间
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, NULL);

    if (free_list_.is_empty())
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to add camera because free_list_ is empty, backup camera[%s].",
            strDevId));

        return NULL;
    }

    // 获取存放备份镜头对象的空间
    char *pch = free_list_.delete_head();

    CAC_Bakup_Record_Dev *pTmpBakupDev = NULL;
    uint32_t nIndex = *((uint32_t *)(void*)pch);

    try
    {
        pTmpBakupDev = new (pch) CAC_Bakup_Record_Dev(strDevId, nIndex );
    }
    catch (...)
    {
        // 如果申请空间失败，打印日志，失败退出
        SVS_LOG((SVS_LM_ERROR,
            "Allocating memery for backup camera failed, backup camera[%s].",
            strDevId));

        pTmpBakupDev = NULL;
        return NULL;
    }

    // 把备份镜头对象插入map表
    int32_t bindResult = bakup_device_map_.bind(pTmpBakupDev->get_frontdev_id(), pTmpBakupDev);

    // 如果把备份镜头对象插入map表失败,那么把该镜头索引再插入空闲链表
    if (0 != bindResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Add backup camera to map failed, backup camera[%s].", strDevId));

        // 释放备份对象的空间
        *((uint32_t *)(void*)pch) = nIndex;
        (void)free_list_.insert_tail(pch);

        pTmpBakupDev->~CAC_Bakup_Record_Dev();
        // 屏蔽pclint告警，是placement new，内存需要放回队列，没有内存泄漏
        pTmpBakupDev = NULL;//lint !e423
        return NULL;
    }

    // 把备份镜头对象加入正在备份前端录像的镜头vector
    using_vector_[nIndex] = pTmpBakupDev;

    return pTmpBakupDev;
}

/*****************************************************************************
 函 数 名  : handle_routerinfo
 功能描述  : 处理录像管理服务器返回的前端录像回放路由响应消息
 输入参数  : const ACE_Message_Block * mb:路由响应消息内容
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::handle_routerinfo(const ACE_Message_Block * mb)
{
    SVS_TRACE();

    // 参数合法性判断
    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to handler route info, the pointer mb is NULL."));
        return SVS_RESULT_FAILURE;
    }

    SVS_CLIENTSVR_SETUP_MEDIA_STREAM_RESP_MSG  *pMsg =
        (SVS_CLIENTSVR_SETUP_MEDIA_STREAM_RESP_MSG*)(void*)mb->rd_ptr();
    uint16_t msgNo = reset_transactionno(pMsg->Header.TransactionNo);
    CAC_Bakup_Record_Dev *pBakDevice =get_device_by_msgno(msgNo);

    // 如果没有找到对应的备份镜头
    if (NULL == pBakDevice)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to handler route info because can not find backup camera. TransactionNo[%u].",
            msgNo));
        return SVS_RESULT_FAILURE;
    }

    SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBakDevice);
    (void)pBakDevice->handle_routerinfo(pMsg);

    // 删除消息map中的备份镜头对象
    (void)delete_msgno_map(msgNo);
    return SVS_RESULT_OK;

}
int32_t CAC_Bakup_Record_Dev_Manager::play_media_request(uint32_t streamId)
{
    // 根据录像ID获取备份镜头对象
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
 函 数 名  : timer_callback
 功能描述  : 定时器回调函数
 输入参数  : int32_t eventId:备份设备内容标示id号
             int32_t timerId:定时器id；
             void * pArg:处理对象指针
 输出参数  : 无
 返 回 值  : 无
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
 函 数 名  : on_timer
 功能描述  : 定时器回调处理函数，根据事件id找到备份镜头处理，然后销毁该id对应的定时器
 输入参数  : int32_t eventId:备份设备内容标示id号
             int32_t timerId:定时器id
 输出参数  : 无
 返 回 值  : 无
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::on_timer(int32_t eventId, int32_t timerId, int32_t nDummy)
{
    // 如果是镜头状态检测类型
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
 函 数 名  : get_device
 功能描述  : 根据备份镜头的设备id获取备份镜头对象指针
 输入参数  : const char * strDevId:备份镜头ID
 输出参数  : 无
 返 回 值  : 成功返回:对象指针,失败返回:NULL
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
 函 数 名  : get_device
 功能描述  : 根据备份镜头的内部id获取备份镜头对象指针
 输入参数  : const uint32_t internalId:备份镜头内部ID
 输出参数  : 无
 返 回 值  : 成功返回:对象指针,失败返回:NULL
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

    // 引用计数加1
    int32_t increaseResult = pBakDevice->increase_reference();

    // 计数加1失败,返回NULL
    if (SVS_RESULT_FAILURE == increaseResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to get backup camera. InternalID[%u].", internalId));
        return NULL;
    }

    return pBakDevice;
}

/*****************************************************************************
 函 数 名  : get_device_by_msgno
 功能描述  : 根据消息流水号，获取备份设备地址
 输入参数  : uint16_t msgNo:消息流水号，在RtRecordSvr内部唯一
 输出参数  : 无
 返 回 值  : 成功返回:对象指针,失败返回:NULL
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

    // 引用计数加1
    int32_t increaseResult = pBakDevice->increase_reference();

    // 计数加1失败,返回NULL
    if (SVS_RESULT_FAILURE == increaseResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail increase backup camera referrence. msgNo[%u].", msgNo));
        return NULL;
    }

    return pBakDevice;
}

/*****************************************************************************
 函 数 名  : get_device_by_routerid
 功能描述  : 根据给定路由ID获取备份镜头对象
 输入参数  : const uint32_t RouterId:路由ID
 输出参数  : 无
 返 回 值  : 成功返回:对象指针,失败返回:NULL
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

    // 没有找到,返回NULL
    if (NULL == pBakDevice)
    {
//        SVS_LOG((SVS_LM_ERROR, "Fail to get backup camera by route ID. routeID[%u]", RouterId));
        return NULL;
    }

    // 查找成功,引用计数加1后,返回该对象指针
    int32_t increaseResult = pBakDevice->increase_reference();
    if (SVS_RESULT_FAILURE == increaseResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to increace backup camera referrence. routeID[%u].", RouterId));
        return NULL;
    }

    return pBakDevice;
}

/*****************************************************************************
 函 数 名  : decrease_reference
 功能描述  : 设备引用计数管理，如果已经没有地方在使用了，则释放镜头对象
 输入参数  : CAC_Bakup_Record_Dev * pBakDevice:备份镜头对象指针
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::decrease_reference(CAC_Bakup_Record_Dev * pBakDevice)
{
    SVS_TRACE();

    // 参数判断
    if (NULL == pBakDevice)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Fail to decrease backup camera reference. The pointer pBakDevice is NULL."));
        return SVS_RESULT_FAILURE;
    }

    uint32_t totalDev = 0;
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);

        // 如果镜头还在被使用
        if (1 != pBakDevice->decrease_reference())
        {
            pBakDevice = NULL;
            return SVS_RESULT_OK;
        }

        // 如果镜头不被使用，则析构掉
        (void)bakup_device_msgno_map_.unbind(pBakDevice->get_router_msgno());
        if (0 != bakup_device_map_.unbind(pBakDevice->get_frontdev_id()))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Fail to delete backup camera because can not find backup camera in map. "
                "backup camera[%s].",
                pBakDevice->get_frontdev_id()));
        }
        // 更新上次结束时间为当前时间
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

    // 复位,把设备加入空闲队列
    uint32_t id = pBakDevice->internal_id();
    (void)pBakDevice->reset();
    pBakDevice->~CAC_Bakup_Record_Dev();
    *((uint32_t *)(void*)pBakDevice) = id;

    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, SVS_RESULT_FAILURE);
    (void)free_list_.insert_tail((char *)pBakDevice);

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : proc_send_report
 功能描述  : 接收到进度发送报告以后，返回进度接收报告，因为沿用客户端回放接口，
             RtRecordSvr不涉及播放器，不关注具体内容，直接反馈接收报告即可。
 输入参数  : const ACE_Message_Block * mb:进度报告消息内容
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
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

    // 根据录像ID获取备份镜头对象
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
 函 数 名  : stop_device
 功能描述  : 根据镜头ID停止备份镜头对象
 输入参数  : const char * strDevId:镜头ID
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::stop_device(const char * strDevId)
{
    SVS_TRACE();

    // 找设备
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
 函 数 名  : stop_device
 功能描述  : 停止指定录像ID的前端录像备份镜头
 输入参数  : const unit32_t RouterId:路由ID
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::stop_device(const uint32_t RouterId)
{
    SVS_TRACE();

    // 查找指定路由的前端备份设备
    CAC_Bakup_Record_Dev *pBakDevice = get_device_by_routerid( RouterId );
    if (NULL == pBakDevice)
    {
        return SVS_RESULT_OK;
    }

    // 停止前端录像备份
    SVS_BAKUP_RECORD_DEV_AUTO_PTR ptr(this, pBakDevice);
    int32_t stopRecordResult = pBakDevice->stop_backup_record();

    return stopRecordResult;
}

/*****************************************************************************
 函 数 名  : stop_all_device
 功能描述  : 停止所有前端录像备份镜头
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
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
        // 如果尝试停止设备次数已经达到最大次了，则强行退出退出
        if (SVS_RETRY_STOP_RECORD_TOTAL_TIMES < ++totalRetryTime)
        {
            SVS_LOG((SVS_LM_ERROR,"Fail to stop backup camera and force to exit. try times[%d].",
                totalRetryTime));
            break;
        }
        //retryTime置零，保证下面的while重试每次从0开始计数
        retryTime = 0;
        SVS_LOG((SVS_LM_INFO, "Wait for stopping backup camera number [%u].", bakDevNum));

        {
            ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);
            BAKUP_DEV_MAP_ITER iter(bakup_device_map_);
            BAKUP_DEV_MAP_ENTRY *pEntry = NULL;
            std::list<CAC_Bakup_Record_Dev *> pBackupDevList;
            // 停止所有录像
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

        // 等待所有备份结束
        while (0 < bakDevNum)
        {
            {
                ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);
                bakDevNum = bakup_device_map_.current_size();
            }

            SVS_LOG((SVS_LM_INFO, "Current backup camera number [%u].", bakDevNum));

            ACE_Time_Value tv( 0, SVS_CHECK_STOP_RECORD_INTERVAL_TIME );
            (void)ACE_OS::sleep( tv );

            // 尝试停止SVS_RETRY_STOP_RECORD_TIMES次
            if (SVS_RETRY_STOP_RECORD_TIMES < retryTime++)
            {
                break;
            }
        }
    }

    // 置当前状态为查询设备状态
    SVS_LOG((SVS_LM_INFO, "Success to stop all backup camera."));

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : debug_all_bakup_device_info
 功能描述  : 调试所有备份镜头信息，将信息放到调试缓冲区
 输入参数  : char* debugBuf:调试缓冲区
             const uint32_t bufLen:缓冲区长度
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
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

        //缓冲区已用完，退出循环
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
 函 数 名  : debug_get_device_info
 功能描述  : telnet调试接口，获取指定镜头的详细信息
 输入参数  : const char *strFrontDevId:前端镜头ID
             char *strDeviceInfo:存放前端镜头信息缓冲区
             uint32_t szDeviceInfo:缓冲区长度
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
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
 函 数 名  : is_lens_bakup_record
 功能描述  : 根据镜头id查找指定镜头是否在前端录像备份
 输入参数  : const char * strLensId:镜头ID
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
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
 函 数 名  : update_backup_device_flag
 功能描述  : 刷新前端录像备份镜头信息。如果镜头最后备份的结束时间已经大于1个
             小时了，则删除这个镜头的前端录像备份结束记录
 输入参数  : 无
 输出参数  : 无
 返 回 值  : 成功返回:0,失败返回:-1
*****************************************************************************/
int32_t CAC_Bakup_Record_Dev_Manager::update_backup_device_flag()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, map_mutex_, -1);
    SVS_LOG((SVS_LM_INFO,
        "The size of backup last end time flag map is [%u]. ",
        backup_time_map_.current_size()));

    // 遍历所有map
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

        // 如果结束时间为0，表示正在备份，则跳过
        if(0 == lastEndTime)
        {
            continue;
        }

        // 如果结束时间还没有大于20分钟，则不清理
        if(SVS_BACKUP_FALG_HOLD_TIME_LEN > (nowTime - lastEndTime))
        {
            continue;
        }

        // 最后的结束时间已经大于一个小时了，则清除这个镜头的备份结束时间
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


