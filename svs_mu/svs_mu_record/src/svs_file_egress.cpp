#include <svs_ace_header.h>
#include "vms/vms.h"

#include "svs_stat_manager.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"
#include "svs_record_index.h"
#include "svs_real_record_server.h"

#include "svs_index_list.h"
#include "svs_file_egress.h"
#include "svs_media_processor.h"

SVS_File_Egress::SVS_File_Egress()
{
    reset();
}

void SVS_File_Egress::reset()
{
    (void)ACE_OS::memset(m_szDeviceID, 0, SVS_DEVICEID_LEN + 1);
    (void)ACE_OS::memset(m_szRecordFilePath, 0x0, sizeof(m_szRecordFilePath));

    m_EgressStatus         = EGRESS_STATUS_INIT;
    m_RecordFileFD         = -1;
    m_ulIOHandlerIndex     = 0;

    m_bHasSwitchDisk       = false;
    m_timeRemove           = 0;
    m_ullRecordFileSize    = 0;
    m_FirstWriteRecordTime = 0;
    m_LastWriteRecordTime  = 0;
    m_ullAllRecvedDataSize = 0;
    m_ullWriteDataSize     = 0;
    m_LastSwitchRecordTime = 0;
    m_ulWriteIndex = 0;
    m_mediaCallbackParamList.clear();
    return;
}

SVS_File_Egress::~SVS_File_Egress()
{
    try
    {
        SVS_LOG((SVS_LM_DEBUG,
            "SVS_File_Egress::~SVS_File_Egress [%s].",
            m_szDeviceID));
    }
    catch(...)
    {
    }
}

int32_t SVS_File_Egress::open(const char * szDevID, uint32_t ulIndex)
{
    reset();

    m_EgressStatus     = EGRESS_STATUS_WRITING;
    m_ulIOHandlerIndex = ulIndex;

    SetLocalizeDeviceId((uint8_t *)(m_szDeviceID),
        sizeof(m_szDeviceID),
        szDevID);

    (void)m_RecordIndexFile.init(szDevID);
    (void)m_IndexList.init(szDevID);
    m_egressStat.init(szDevID);

    pthread_mutex_init(&m_mediaCallbackParamMutex, NULL);

    SVS_LOG((SVS_LM_INFO,
        "camera[%s] open file egress success, handle[%u].",
        m_szDeviceID,
        m_ulIOHandlerIndex));

    return SVS_RESULT_OK;
}

int32_t SVS_File_Egress::close()
{
    m_EgressStatus  = EGRESS_STATUS_CLOSING;

    // 如果文件句柄有效，则用写线程关闭egress，否在close直接关闭
    if(0 <= m_RecordFileFD)
    {
        SVS_LOG((SVS_LM_INFO,
            "Create a close egress message to write thread."
            "cameraId[%s], m_RecordFileFD[%d]",
            m_szDeviceID,
            m_RecordFileFD));

        CallBackParam *pFuncParam = SVS_NEW(pFuncParam);
        if (NULL == pFuncParam)
        {
            SVS_LOG((SVS_LM_WARNING,
                "camera[%s] write data fail, alloc call back param failure.",
                m_szDeviceID));

            return SVS_RESULT_FAILURE;
        }

        (void)ACE_OS::memset(pFuncParam, 0x0, sizeof(CallBackParam));
        pFuncParam->m_pFileEgress = this;
        pFuncParam->m_pMediaData  = NULL;
        pFuncParam->m_bIsFinished = false;
        pFuncParam->m_ulWriteTime = time(NULL);

        // 添加一个关闭信息,如果失败，释放内存
        pthread_mutex_lock(&m_mediaCallbackParamMutex);
        pFuncParam->m_ulWriteIndex = ++m_ulWriteIndex;

        int32_t nRet = SVS_Disk_Manager::instance().writeFile(m_RecordFileFD, NULL, &write_call_back, pFuncParam);
        if(SVS_RESULT_OK == nRet)
        {
            m_mediaCallbackParamList.push_back(pFuncParam);
            pthread_mutex_unlock(&m_mediaCallbackParamMutex);

            return SVS_RESULT_OK;
        }

        // 如果创建写消息失败
        --m_ulWriteIndex;
        pthread_mutex_unlock(&m_mediaCallbackParamMutex);

        SVS_DELETE(pFuncParam);
        SVS_LOG((SVS_LM_ERROR,
            "Add a close egress message to write thread failed."
            "cameraId[%s],return value[%d], m_ullWriteDataSize[%Q],"
            " m_ullAllRecvedDataSize[%Q].",
            m_szDeviceID, nRet,
            m_ullWriteDataSize,
            m_ullAllRecvedDataSize));

        // 如果还没有写结束，则返回
        if(m_ullWriteDataSize != m_ullAllRecvedDataSize)
        {
            return SVS_RESULT_OK;
        }
    }
    SVS_LOG((SVS_LM_INFO,
        "Close file egress.cameraId[%s],"
        "m_ullAllRecvedDataSize[%Q],m_ullWriteDataSize[%Q] m_RecordFileFD[%d].",
        m_szDeviceID,
        m_ullAllRecvedDataSize,
        m_ullWriteDataSize,
        m_RecordFileFD));

    close_finished();

    return SVS_RESULT_OK;
}

int32_t SVS_File_Egress::check_status()
{
    if (EGRESS_STATUS_NODISK == m_EgressStatus)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera[%s] file egress EGRESS_STATUS_NODISK.",
            m_szDeviceID));

        return SVS_ERR_NORAID;
    }

    if (EGRESS_STATUS_WRITING != m_EgressStatus)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Camera[%s] file egress status[%d] error.",
            m_szDeviceID,
            m_EgressStatus));

        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}
/*****************************************************************************
函 数 名  : write
功能描述  : 将媒体数据写入到录像文件中，由Recv_Stream直接调用
输入参数  : mb  :   需要写的媒体数据块
输出参数  : 无
返 回 值  : 成功返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_File_Egress::write(ACE_Message_Block * pMediaData)
{
    if ( NULL == pMediaData )
    {
        SVS_LOG((SVS_LM_WARNING,
            "Write record file failed, "
            "The parameter message block is null, cameraId[%s].",
            m_szDeviceID));

        return SVS_RESULT_FAILURE;
    }

    SVS_Recv_Media_Header *pRecvMediaHeader =
                           (SVS_Recv_Media_Header*)(void*)pMediaData->base();

    time_t recvFrameTime = pRecvMediaHeader->FirstFrameRecvTime;

    int32_t nRet = 0;

    SVS_LOG((SVS_LM_DEBUG,
        "camera[%s] write data, FirstRecvTime[%u], LastRecvTime[%u], "
        "FirstWriteTime[%u], LastWriteTime[%u].",
        m_szDeviceID,
        pRecvMediaHeader->FirstFrameRecvTime,
        pRecvMediaHeader->LastFrameRecvTime,
        m_FirstWriteRecordTime,
        m_LastWriteRecordTime));

    // 创建回调接口，在IO调度执行实际的写操作后，将结果回调
    CallBackParam *pFuncParam = SVS_NEW(pFuncParam);

    if (NULL == pFuncParam)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] write data fail, alloc call back param failure.",
            m_szDeviceID));

        return SVS_RESULT_FAILURE;
    }

    (void)ACE_OS::memset(pFuncParam, 0x0, sizeof(CallBackParam));
    pFuncParam->m_pFileEgress = this;
    pFuncParam->m_pMediaData  = pMediaData;
    (void)ACE_OS::memcpy(pFuncParam->m_RecordFileName, m_szRecordFilePath, MAXNAMELEN);
    pFuncParam->m_bIsFinished = false;
    pFuncParam->m_ulWriteTime = time(NULL);

    // 检查是否需要切换文件(切换文件和第一次打开)
    if ( recvFrameTime >= (m_LastSwitchRecordTime + BIG_RECORDFILE_TIME_SLICE ))
    {
        // 录像文件进行了重新打开，需要索引文件重新打开
        pFuncParam->m_ulChangeRecordFile = 1;
    }

    bool bControl = true; //only for compile warnning
    do
    {
        if ( (EGRESS_STATUS_WRITING != m_EgressStatus)
            || (1 == pFuncParam->m_ulChangeRecordFile) )
        {
            bool medaDataPartedFlag = false;
            // 打开一个新的录像文件
            nRet = open_file(recvFrameTime,medaDataPartedFlag);
            if (0 != nRet)
            {
                SVS_LOG((SVS_LM_WARNING, "Camera[%s] open file fail.", m_szDeviceID));
                SVS_DELETE(pFuncParam);

                return SVS_RESULT_FAILURE;
            }

            // 打开文件成功，更新状态
            m_EgressStatus = EGRESS_STATUS_WRITING;
            pFuncParam->m_ullRecordFileSize = m_ullRecordFileSize;

            // 重新保存新文件的路径
            (void)ACE_OS::memset(pFuncParam->m_RecordFileName, 0x0, MAXNAMELEN + 1);
            (void)ACE_OS::memcpy(pFuncParam->m_RecordFileName, m_szRecordFilePath, MAXNAMELEN);

            if (m_bHasSwitchDisk)
            {
                pFuncParam->m_ulChangeDisk = 1;
                m_bHasSwitchDisk = false;
            }

            pFuncParam->m_medaDataPartedFlag = medaDataPartedFlag;
        }

        pthread_mutex_lock(&m_mediaCallbackParamMutex);
        pFuncParam->m_ulWriteIndex = ++m_ulWriteIndex;
        nRet = SVS_Disk_Manager::instance().writeFile(m_RecordFileFD,
                                                      pMediaData,
                                                      &write_call_back,
                                                      pFuncParam);
        if (SVS_RESULT_OK != nRet)
        {
            --m_ulWriteIndex;
            pthread_mutex_unlock(&m_mediaCallbackParamMutex);

            if (1 == pFuncParam->m_ulChangeRecordFile)
            {
                // 如果已经重新创建了录像文件还写失败，则返回失败
                SVS_LOG((SVS_LM_WARNING,
                    "Camera[%s] write file fail, file[%s], errcode[%d].",
                    m_szDeviceID,
                    m_szRecordFilePath,
                    nRet));

                m_EgressStatus = EGRESS_STATUS_ERROR;
                SVS_DELETE(pFuncParam);
                return nRet;
            }
            else
            {
                if ( SVS_ERR_FD_SWITCH_FILE == nRet )
                {
                    SVS_LOG((SVS_LM_INFO,
                        "Camera[%s] write file fail, fd is switch file status. "
                        "file[%s], errcode[%d],"
                        " retry to create new record file.",
                        m_szDeviceID, m_szRecordFilePath, nRet));
                }
                else
                {
                    SVS_LOG((SVS_LM_WARNING,
                        "Camera[%s] write file fail, file[%s], errcode[%d],"
                        " retry to create new record file.",
                        m_szDeviceID, m_szRecordFilePath, nRet));
                }
                // 尝试重新打开文件再试一次
                pFuncParam->m_ulChangeRecordFile = 1;
                continue;
            }
        }
        // 把回调参数加入列表
        m_mediaCallbackParamList.push_back(pFuncParam);
        pthread_mutex_unlock(&m_mediaCallbackParamMutex);

        // 写入成功，退出循环
        break;
    }    while (bControl);        // 在程序中控制何时退出循环

    // 收到的数据量增加
    update_recved_info(pMediaData);

    return SVS_RESULT_OK;
}

void SVS_File_Egress::update_recved_info(const ACE_Message_Block * pMediaData)
{
    if(NULL == pMediaData)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Update receive information failed.The parameter is invalid."
            "pMediaData[0x%08x].",
            pMediaData));

        return ;
    }

    SVS_Recv_Media_Header *pRecvMediaHeader =
                           (SVS_Recv_Media_Header*)(void*)pMediaData->base();

    if ( pRecvMediaHeader->IsTmpfsMemory )
    {
        m_ullAllRecvedDataSize += pRecvMediaHeader->TmpfsDatalen;
    }
    else
    {
        m_ullAllRecvedDataSize += pMediaData->length();
    }

    return;
}
int32_t SVS_File_Egress::write_call_back(int32_t, int32_t errcode, void *pParam)
{
    SVS_LOG((SVS_LM_DEBUG,
        "file egress call back, errcode[%d], param[0x%x].",
        errcode,
        pParam));

    if (NULL == pParam)
    {
        return -1;
    }

    CallBackParam *pCallBackParam = (CallBackParam*)pParam;
    SVS_File_Egress *pFileEgress    = pCallBackParam->m_pFileEgress;
    if (NULL == pFileEgress)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Write all back failed. pCallBackParam->m_pFileEgress[08%08x]",
            pCallBackParam->m_pFileEgress));
        SVS_DELETE(pCallBackParam);
        return -1;
    }

    int32_t nRet = pFileEgress->on_write_finish(errcode, pCallBackParam);
    if (0 != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] update record index fail.",
            pCallBackParam->m_RecordFileName));

//        // 删除回调函数的参数
//        SVS_DELETE(pCallBackParam);
        return nRet;
    }

//    // 删除回调函数的参数
//    SVS_DELETE(pCallBackParam);
    return 0;
}

/*****************************************************************************
 函 数 名  : open_file
 功能描述  : 打开录像文件，录像文件名称以正点命名
 输入参数  : 将要写入的录像第一帧时间，用于计算录像文件名称
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t SVS_File_Egress::open_file(time_t dataTime, bool & medaDataPartedFlag)
{
    SVS_TRACE();

    time_t fileTime = dataTime - (dataTime % BIG_RECORDFILE_TIME_SLICE);

    SVS_LOG((SVS_LM_DEBUG,
        "Begin to open record file."
        "cameraId[%s],data time[%d] ,file time[%d].",
        m_szDeviceID,
        dataTime,
        fileTime ));

    // 关闭旧的录像文件
    if ( -1 != m_RecordFileFD )
    {
        SVS_LOG((SVS_LM_INFO,
            "camera[%s] close record file[%s], fd[%d].",
            m_szDeviceID,
            m_szRecordFilePath,
            m_RecordFileFD));

        // 删除预录文件
        (void)delete_prorecord_file(dataTime);

        (void)SVS_Disk_Manager::instance().closeFile(m_RecordFileFD);
    }

    // 前端录像备份和平台实时录像文件扩展名不同
    char * fileExName = (char*)SVS_RT_FILE_EXNAME;
    if ( 0 != m_timeRemove )
    {
        fileExName = (char*)SVS_BACKUP_FILE_EXNAME;
    }

    // 打开录像文件
    uint64_t   file_size = 0;
    int32_t tempFileFd = m_RecordFileFD;
    int32_t nRet = SVS_Disk_Manager::instance().openFile(m_szDeviceID, fileTime,
                                                    fileExName,
                                                    file_size, tempFileFd,
                                                    medaDataPartedFlag);

    if ( (SVS_RESULT_OK != nRet ) && (SVS_ERR_CHANGEDISK != nRet))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to open record file."
            "cameraId[%s],record file name[%s], errcode[%d].",
            m_szDeviceID,
            m_szRecordFilePath,
            nRet));

        // 如果返回没有磁盘可用时，改变Egress状态
        if (SVS_ERR_NORAID == nRet)
        {
            m_EgressStatus = EGRESS_STATUS_NODISK;
        }
        else
        {
            // 其它错误
            m_EgressStatus = EGRESS_STATUS_ERROR;
        }
//        m_RecordFileFD = -1;
        return SVS_RESULT_FAILURE;
    }

    m_RecordFileFD = tempFileFd;
    if (SVS_ERR_CHANGEDISK == nRet)
    {
        m_bHasSwitchDisk = true;
        SVS_LOG((SVS_LM_INFO,
            "camera[%s] has switched disk.",
            m_szDeviceID));
    }

    // 初始录像文件大小
    m_ullRecordFileSize = (uint64_t)file_size; //lint !e571
    (void)ACE_OS::memset(m_szRecordFilePath, 0x0, MAXNAMELEN + 1);
    nRet = SVS_Disk_Manager::instance().getFileFullName(m_RecordFileFD, m_szRecordFilePath);
    if (0 != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] get file name fail, errcode[%d].",
            m_szDeviceID,
            nRet));

        return nRet;
    }

    // 更新统计信息
    m_egressStat.updateFileName(m_szRecordFilePath);
    m_egressStat.updateRecordFileSize(m_ullRecordFileSize);

    // 7. 最后一次切换文件时间
    m_LastSwitchRecordTime = fileTime;

    SVS_LOG((SVS_LM_DEBUG,
        "camera[%s] success to open file[%s], fd[%d]",
        m_szDeviceID, m_szRecordFilePath, m_RecordFileFD));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : on_write_finish
功能描述  : 处理写入结束的回调，当写入结果为成功时，更新关键帧信息和索引信息；
            当写入失败时，进行出错处理。
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_File_Egress::on_write_finish(int32_t write_result, void * param)
{
    CallBackParam *pCallBackParam = (CallBackParam*)param;
    if (NULL == pCallBackParam)
    {
        return SVS_RESULT_FAILURE;
    }

    // 遍历回调参数列表，修改回调标识
    pthread_mutex_lock(&m_mediaCallbackParamMutex);
    bool bIsFind = false;
    CallbackParamListIter iter = m_mediaCallbackParamList.begin();
    while(iter != m_mediaCallbackParamList.end())
    {
        // 地址和id值一样的为同一个参数,保证信息未被别处改动
        if((*iter)->m_ulWriteIndex == pCallBackParam->m_ulWriteIndex)
        {
            (*iter)->m_bIsFinished = true;
            (*iter)->m_nWriteRet = write_result;
            bIsFind = true;

            if(NULL != pCallBackParam->m_pMediaData)
            {
                // 写入磁盘的数据量增加
                m_ullWriteDataSize += pCallBackParam->m_pMediaData->length();
            }

            break;
        }

        iter++;
    }

    // 如果没有找到，则是致命错误
    if(!bIsFind)
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Handle write callback failed. Can't find the write callcack parameter index."
            "CameraId[%s], maxIndex[%u], parameter index[%u], "
            "parameter time[%u],pCallBackParam[0x%08x].",
            m_szDeviceID,
            m_ulWriteIndex,
            pCallBackParam->m_ulWriteIndex,
            pCallBackParam->m_ulWriteTime,
            pCallBackParam));
    }

    // 更新索引
    time_t currTime = time(NULL);
    uint32_t ulListSize = m_mediaCallbackParamList.size();
    iter = m_mediaCallbackParamList.begin();
    while(iter != m_mediaCallbackParamList.end())
    {

        // 如果已经写完成了，则直接调用写索引流程
        if((*iter)->m_bIsFinished)
        {
            pCallBackParam = m_mediaCallbackParamList.front();
            m_mediaCallbackParamList.pop_front();
            --ulListSize;
            // 如果写完成了，则做最后的关闭资源处理,通知Media processor关闭完成
            if (NULL == pCallBackParam->m_pMediaData)
            {
                SVS_LOG((SVS_LM_INFO,
                    "Write finished.cameraId[%s], status[0x%04x], "
                    "m_ullAllRecvedDataSize[%Q],m_ullWriteDataSize[%Q].",
                    m_szDeviceID, m_EgressStatus,
                    m_ullAllRecvedDataSize, m_ullWriteDataSize));

                if(m_ullAllRecvedDataSize != m_ullWriteDataSize)
                {
                    SVS_LOG((SVS_LM_WARNING,
                        "Close egress failed as the size of receiving media data "
                        "is unequel to the size of writing media data. "
                        "CameraId[%s], m_EgressStatus[0x%04x], "
                        "m_ullAllRecvedDataSize[%Q], m_ullWriteDataSize[%Q].",
                        m_szDeviceID, m_EgressStatus,
                        m_ullAllRecvedDataSize, m_ullWriteDataSize));

                    m_EgressStatus = EGRESS_STATUS_CLOSED;

                    break;
                }

                // 涉及到释放egress，所以先释放锁
                pthread_mutex_unlock(&m_mediaCallbackParamMutex);

                // 停止结束处理
                close_finished();

                return SVS_RESULT_OK;
            }

            (void)handleWriteCallBackParam(pCallBackParam->m_nWriteRet, pCallBackParam);

            // 释放资源
            SVS_DELETE(pCallBackParam);

            // 如果list还有待处理的回调参数，才进入while，否则结束流程(不能再使用egress成员，可能egress会被close_finished关闭)
            if(0 < ulListSize)
            {
                // 把指针指向最开始的位置，保证索引更新时顺序进行的
                iter = m_mediaCallbackParamList.begin();
                continue;
            }
        }
        else if((currTime - (*iter)->m_ulWriteTime) >= BIG_RECORDFILE_TIME_SLICE)
        {
            // 如果时间已经很久了，说明前面的写线程阻塞，则标志录像异常
            // 不能删除节点，因为删除了，写和回到的录像大小就不一样了，egress的应用计数就错了
            SVS_LOG((SVS_LM_ERROR,
                "Handle record index failed. The write callback is over time."
                "CameraId[%s], currTime[%u], writeTime[%u],writeIndex[%u], maxWriteIndex[%u].",
                m_szDeviceID,
                currTime,
                pCallBackParam->m_ulWriteTime,
                pCallBackParam->m_ulWriteIndex,
                m_ulWriteIndex));

            if(EGRESS_STATUS_WRITING == m_EgressStatus)
            {
                m_EgressStatus = EGRESS_STATUS_ERROR;
            }
        }
        else // 遇到没有写回来，也没有超时的，结束索引处理流程
        {
            SVS_LOG((SVS_LM_DEBUG,
                "Handle record index delayed. Has not write callback."
                "CameraId[%s], currTime[%u], writeTime[%u],writeIndex[%u], maxWriteIndex[%u].",
                m_szDeviceID,
                currTime,
                (*iter)->m_ulWriteTime,
                (*iter)->m_ulWriteIndex,
                m_ulWriteIndex));
        }

        pthread_mutex_unlock(&m_mediaCallbackParamMutex);

        return SVS_RESULT_OK;
    }

    pthread_mutex_unlock(&m_mediaCallbackParamMutex);
    return SVS_RESULT_OK;
}

int32_t SVS_File_Egress::handleWriteCallBackParam(int32_t write_result, void * param)
{
    CallBackParam *pCallBackParam = (CallBackParam*)param;
    int32_t nRet = 0;

    if (NULL == pCallBackParam)
    {
        return SVS_RESULT_FAILURE;
    }

    if (SVS_RESULT_OK == write_result)
    {
        nRet = update_record_index(pCallBackParam);
        if (SVS_RESULT_OK != nRet)
        {
            // 更新索引出错,如果正在写录像状态，则置成错误状态，触发写录像时重新打开文件
            if(EGRESS_STATUS_WRITING == m_EgressStatus)
            {
                m_EgressStatus = EGRESS_STATUS_ERROR;
            }

            SVS_LOG((SVS_LM_WARNING,
                "camera[%s] update record index fail.",
                m_szDeviceID));
        }

        m_egressStat.addWriteByteNum(pCallBackParam->m_pMediaData->length());
    }
    else
    {
        // 写磁盘失败,如果正在写录像状态，则置成错误状态，触发写录像时重新打开文件
        if(EGRESS_STATUS_WRITING == m_EgressStatus)
        {
            m_EgressStatus = EGRESS_STATUS_ERROR;
        }

        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] write media data fail, errcode[%d].",
            m_szDeviceID,
            write_result));
    }

//    // 写入磁盘的数据量增加
//    m_ullWriteDataSize += pCallBackParam->m_pMediaData->length();

    // 如果状态是关闭状态，且数据写完了，则关闭完成，通知media process
    if (EGRESS_STATUS_CLOSED == m_EgressStatus)
    {
        if(m_ullAllRecvedDataSize != m_ullWriteDataSize)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Write media data finished and egress status is closing, "
                "but the size of receiving media data "
                "is unequel to the size of writing media data. "
                "CameraId[%s], m_EgressStatus[0x%04x], "
                "m_ullAllRecvedDataSize[%Q], m_ullWriteDataSize[%Q].",
                m_szDeviceID, m_EgressStatus,
                m_ullAllRecvedDataSize, m_ullWriteDataSize));
        }
        else
        {

            SVS_LOG((SVS_LM_WARNING,
                "Write media data finished, egress status is closing, "
                "and the size of receiving media data "
                "is unequel to the size of writing media data.Close finished."
                "CameraId[%s], m_EgressStatus[0x%04x], "
                "m_ullAllRecvedDataSize[%Q], m_ullWriteDataSize[%Q].",
                m_szDeviceID, m_EgressStatus,
                m_ullAllRecvedDataSize, m_ullWriteDataSize));
            close_finished();
        }
    }
    return nRet;
}

/*****************************************************************************
函 数 名  : update_record_index
功能描述  : 更新录像文件索引
输入参数  : 无
输出参数  : 无
返 回 值  : 成功,返回 0 否则返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_File_Egress::update_record_index(const CallBackParam *pCallBackParam)
{
    SVS_TRACE();
    if ((NULL == pCallBackParam)
        || (NULL == pCallBackParam->m_pMediaData))
    {
        return SVS_RESULT_FAILURE;
    }

    SVS_Recv_Media_Header *pRecvMediaHeader =
                           (SVS_Recv_Media_Header*)(void*)pCallBackParam->m_pMediaData->base();
    time_t recvFrameTime = pRecvMediaHeader->FirstFrameRecvTime;

    // 如果切换了磁盘或者切换了录像文件
    // 需要重新打开索引文件
    if ((1 == pCallBackParam->m_ulChangeDisk)
        || (1 == pCallBackParam->m_ulChangeRecordFile))
    {
        if (0 != m_LastWriteRecordTime)
        {
            // 更新旧的索引文件
            (void)m_RecordIndexFile.update_indexfile(m_LastWriteRecordTime);

            // 发送消息，更新数据库的索引信息
            update_db_record_index(NULL);

            // 关闭旧的索引文件
            (void)m_RecordIndexFile.close();
        }

        // 重新创建索引文件
        if (SVS_RESULT_OK != m_RecordIndexFile.create_indexfile(recvFrameTime,
                                                                (char*)pCallBackParam->m_RecordFileName,
                                                                pCallBackParam->m_ullRecordFileSize,
                                                                pCallBackParam->m_medaDataPartedFlag))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Failed to open record index file, cameraId[%s].",
                m_szDeviceID));
            return SVS_RESULT_FAILURE;
        }

        // 向索引文件中设置录像文件的初始大小
        m_RecordIndexFile.set_record_file_size(pCallBackParam->m_ullRecordFileSize);

        if (1 == pCallBackParam->m_ulChangeDisk)
        {
            m_FirstWriteRecordTime    = recvFrameTime;
            SVS_LOG((SVS_LM_INFO,
                "camera[%s] has changed disk, reset index start time.",
                m_szDeviceID));
        }

        SVS_LOG((SVS_LM_DEBUG,
            "camera[%s] recreate record index success, first write time[%u],"
            "last write time[%u], record file[%s].",
            m_szDeviceID, m_FirstWriteRecordTime, m_LastWriteRecordTime,
            m_szRecordFilePath));
    }

    // 7. 更新写入录像文件第一帧时间
    if ( 0 == m_FirstWriteRecordTime)
    {
        m_FirstWriteRecordTime = recvFrameTime;
    }

    // 打印一下数据中的索引记录
    if (NULL != pRecvMediaHeader->IndexList)
    {
        SVS_LOG((SVS_LM_INFO,
            "camera[%s] has index list[%d], data start time[%u], end time[%u]"
            " frame num[%u].",
            m_szDeviceID,
            pRecvMediaHeader->IndexList->size(),
            pRecvMediaHeader->FirstFrameRecvTime,
            pRecvMediaHeader->LastFrameRecvTime,
            pRecvMediaHeader->RecvedFrameCount));
    }

    // 解析当前mb的关键帧信息
    parse_key_frame(pCallBackParam);

    SVS_LOG((SVS_LM_INFO,
        "camera[%s] write data[%d] finish, FirstRecvTime[%u] LastRecvTime[%u],"
        "FirstWriteTime[%u] LastWriteTime[%u].",
        m_szDeviceID,
        pCallBackParam->m_pMediaData->length(),
        pRecvMediaHeader->FirstFrameRecvTime,
        pRecvMediaHeader->LastFrameRecvTime,
        m_FirstWriteRecordTime,
        m_LastWriteRecordTime));

    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : parse_key_frame
功能描述  : 解析当前写入录像文件中的媒体块的关键帧的时间戳和在录像文件的中偏移位置
            把解析出的信息写入录像索引文件，以备回放时快速定位
输入参数  : mb  :   需要写的媒体数据块
输出参数  : 无
返 回 值  : NA
修改历史  :
*****************************************************************************/
void SVS_File_Egress::parse_key_frame(const CallBackParam *pCallBackParam)
{
    ACE_Message_Block *pMsgBlock = pCallBackParam->m_pMediaData;
    SVS_Recv_Media_Header *pRecvMediaHeader = NULL;
    pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)pMsgBlock->base();

    uint32_t ulChangeDiskFlag      = pCallBackParam->m_ulChangeDisk;    // 是否切换磁盘
    uint32_t frameCount             = pRecvMediaHeader->RecvedFrameCount;
    uint32_t currKeyFrameTime      = 0;
    uint32_t tempKeyFrameTime      = 0;
    uint64_t      recordFileSize        = m_RecordIndexFile.get_record_file_size();
    bool          bInsertClusterOffset = true;
    SVS_NEW_ALL_HEADER* pMediaAllHeader = NULL;
    KEYFRAME_VECTOR keyFrameVec;
    keyFrameVec.clear();
    KEYFRAME_INFO keyFrameInfo;
    m_egressStat.addWriteFrameNum(frameCount);
    char* currPos = (char*)(void*)pMsgBlock->rd_ptr();
    uint32_t ulIFrameNum = 0;
    while(0 < frameCount)
    {
        if (bInsertClusterOffset)
        {
            // 获取mb第一帧在大文件中的偏移，放入vector，可以不是关键帧，用于簇偏移
            pMediaAllHeader         = (SVS_NEW_ALL_HEADER*)((void *)currPos);
            keyFrameInfo.TimeTick   = pMediaAllHeader->FrameHeaer.nTimeTick;
            keyFrameInfo.FileOffset = recordFileSize;
            bInsertClusterOffset    = false;

            keyFrameVec.push_back(keyFrameInfo);
        }

        pMediaAllHeader = (SVS_NEW_ALL_HEADER*)((void *)currPos);

        // 如果是关键桢
        if ( 1 == pMediaAllHeader->FrameHeaer.bKeyFrame)
        {
            // 控制每秒最多有一个关键帧,防止关键桢泛滥，时间戳单位毫秒转化成秒
            tempKeyFrameTime = (pMediaAllHeader->FrameHeaer.nTimeTick / 1000);
            if (tempKeyFrameTime > currKeyFrameTime)
            {
                currKeyFrameTime = tempKeyFrameTime;
                keyFrameInfo.TimeTick = pMediaAllHeader->FrameHeaer.nTimeTick;
                keyFrameInfo.FileOffset = recordFileSize;

                keyFrameVec.push_back(keyFrameInfo);
            }

            ++ulIFrameNum;
        }

        // 更新当前录像文件长度
        recordFileSize += pMediaAllHeader->ExtendFrameHeader.nAdjustFrameSize;
        currPos += pMediaAllHeader->ExtendFrameHeader.nAdjustFrameSize;

        // 如果需要更新索引文件
        if ((NULL != pRecvMediaHeader->IndexList) && (!pRecvMediaHeader->IndexList->empty()))
        {
            SVS_Media_Index *mediaIndex = &pRecvMediaHeader->IndexList->front();
            if (pMediaAllHeader->PacketHeader.m_dwFrameID == mediaIndex->LastIndexFrameID)
            {
                // 更新关键帧信息
                (void)m_RecordIndexFile.update_key_frame(keyFrameVec);
                keyFrameVec.clear();

                // 更新索引文件
                (void)m_RecordIndexFile.update_indexfile( mediaIndex->LastIndexRecvTime);
                m_LastWriteRecordTime = mediaIndex->LastIndexRecvTime;

                // 更新数据库中索引记录
                update_db_record_index(pCallBackParam->m_RecordFileName);
                if (1 == ulChangeDiskFlag)
                {
                    ulChangeDiskFlag = 0;
                }
                pRecvMediaHeader->IndexList->pop_front();

                bInsertClusterOffset = true;
            }
        }

        // 如果索引需要断成两段
        if ((NULL != pRecvMediaHeader->IndexBreakList) && (!pRecvMediaHeader->IndexBreakList->empty()))
        {
            int32_t iRet = SVS_RESULT_FAILURE;
            iRet = parse_index_seg(pCallBackParam,pMediaAllHeader,recordFileSize,keyFrameVec,ulChangeDiskFlag);
            if(SVS_RESULT_FAILURE == iRet)
            {
                return;
            }
            else if(1 == iRet)
            {
                bInsertClusterOffset = true;
            }
        }

        --frameCount;
    }

    // 更新关键帧信息
    if ( 0 < keyFrameVec.size() )
    {
        (void)m_RecordIndexFile.update_key_frame(keyFrameVec);
        keyFrameVec.clear();
    }

    // 6. 更新写入录像文件的最后一帧时间
    m_LastWriteRecordTime = pRecvMediaHeader->LastFrameRecvTime;

    // 防止在切换磁盘时刚好没有更新索引，这时也需要向数据库中插入一条索引记录
    if (1 == ulChangeDiskFlag)
    {
        update_db_record_index(pCallBackParam->m_RecordFileName);
        ulChangeDiskFlag = 0;
    }
    // 更新录像文件大小
    m_RecordIndexFile.set_record_file_size(recordFileSize);
    m_egressStat.updateRecordFileSize(recordFileSize);
    m_egressStat.addWriteIFrameNum(ulIFrameNum);
    return;
}
int32_t SVS_File_Egress::parse_index_seg(const CallBackParam *pCallBackParam,
                                      const SVS_NEW_ALL_HEADER* pMediaAllHeader,
                                      uint64_t      recordFileSize,
                                      KEYFRAME_VECTOR &keyFrameVec,
                                      uint32_t& ulChangeDiskFlag)
{
    if ( ( NULL == pCallBackParam )
    || (NULL == pCallBackParam->m_pMediaData)
    || (NULL == pMediaAllHeader))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Parse index segment failed."
            "The parameter message block is invalid."
            "cameraId[%s], pCallBackParam[0x%08x],pMediaAllHeader[0x%08x]",
            m_szDeviceID,
            pCallBackParam,
            pMediaAllHeader));

        return SVS_RESULT_FAILURE;
    }

    ACE_Message_Block *pMsgBlock = pCallBackParam->m_pMediaData;
    SVS_Recv_Media_Header *pRecvMediaHeader = NULL;
    pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)pMsgBlock->base();

    // 如果没有索引断记录，则退出
    if ((NULL == pRecvMediaHeader->IndexBreakList)
    || (pRecvMediaHeader->IndexBreakList->empty()))
    {
        return SVS_RESULT_OK;
    }

    SVS_Media_Index *mediaIndex = &pRecvMediaHeader->IndexBreakList->front();
    if (pMediaAllHeader->PacketHeader.m_dwFrameID == mediaIndex->NewIndexFrameID)
    {
        SVS_LOG((SVS_LM_DEBUG,
            "New index.camera[%s], keyFlag[%d],nTimeTick[%u],frameId[%u],",
            m_szDeviceID,pMediaAllHeader->FrameHeaer.bKeyFrame,
            pMediaAllHeader->FrameHeaer.nTimeTick,
            pMediaAllHeader->PacketHeader.m_dwFrameID));
        // 1.更新旧的索引文件
        // 如果这个缓存区的第一帧就跳变了，则修改段前的最后时间
        if(0 == mediaIndex->LastIndexRecvTime)
        {
            mediaIndex->LastIndexRecvTime = m_LastWriteRecordTime;
        }

        // 更新关键帧信息
        (void)m_RecordIndexFile.update_key_frame(keyFrameVec);
        keyFrameVec.clear();

        // 更新索引文件
        (void)m_RecordIndexFile.update_indexfile( mediaIndex->LastIndexRecvTime);
        m_LastWriteRecordTime = mediaIndex->LastIndexRecvTime;

        // 更新数据库中索引记录
        update_db_record_index(pCallBackParam->m_RecordFileName);

        // 防止在切换磁盘时刚好没有更新索引，这时也需要向数据库中插入一条索引记录
        if (1 == ulChangeDiskFlag)
        {
            update_db_record_index(pCallBackParam->m_RecordFileName);
            ulChangeDiskFlag = 0;
        }

        update_db_record_index(NULL);

        // 关闭旧的索引文件
        (void)m_RecordIndexFile.close();

        // 3.重新创建索引文件
        uint64_t fileSize = recordFileSize - pMediaAllHeader->ExtendFrameHeader.nAdjustFrameSize;
        if (SVS_RESULT_OK != m_RecordIndexFile.create_indexfile(
                                 mediaIndex->NewIndexRecvTime,
                                (char*)pCallBackParam->m_RecordFileName,
                                fileSize,
                                pCallBackParam->m_medaDataPartedFlag))
        {
            SVS_LOG((SVS_LM_ERROR,
                "Failed to open record index file, cameraId[%s].",
                m_szDeviceID));
            return SVS_RESULT_FAILURE;
        }

        // 向索引文件中设置录像文件的初始大小
        m_RecordIndexFile.set_record_file_size(fileSize);

        // 4.更新本地索引记录
        m_FirstWriteRecordTime = mediaIndex->NewIndexRecvTime;

        m_LastWriteRecordTime = mediaIndex->NewIndexRecvTime;

        update_db_record_index(pCallBackParam->m_RecordFileName);

        pRecvMediaHeader->IndexBreakList->pop_front();

        return 1;
    }

    return SVS_RESULT_OK;
}
/*****************************************************************************
函 数 名  : delete_prorecord_file
功能描述  : 删除预录文件
输入参数  : deleteTime:控制删除哪个小时的预录文件
输出参数  :
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_File_Egress::delete_prorecord_file(time_t deleteTime)
{
    SVS_TRACE();

    // 如果是前端录像备份(实时录像m_timeRemove为0)不处理删除预录操?
    if(0 != m_timeRemove )
    {
        return SVS_RESULT_OK;
    }

    // 如果是刚刚启动录像，则前面没有需要删除的预录录像文件
    if (0 == m_LastWriteRecordTime)
    {
        return SVS_RESULT_OK;
    }

    time_t lastDbRecordIndex = m_IndexList.LastDBRecordIndex();
    // 如果上次添加索引的时间为0.说明只是在预录，索引没有入库，所以创建删除预录任务
    // 如果要删除的这个小时没有索引入库，也要删除预录
    if((0 == lastDbRecordIndex)
    || (lastDbRecordIndex <= (deleteTime - BIG_RECORDFILE_TIME_SLICE)))
    {
        (void)add_delete_task(DEL_RECORD_TYPE_PRERECORD,deleteTime);
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
函 数 名  : add_delete_task
功能描述  : 添加录像删除任务
输入参数  : deleteType:删除类型，删除预录文件或磁盘满覆盖写
            当删除类型是删除预录的时候，
输出参数  : NA
返 回 值  : 成功返回0;失败返回-1
修改历史  :
*****************************************************************************/
int32_t SVS_File_Egress::add_delete_task(uint8_t deleteType,
                                              time_t deleteTime)
{
    SVS_TRACE();
    // 申请空间
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
            "Add deleting record file task failed."
            "Allocate message block failed.cameraId[%s].",
            m_szDeviceID));
        return SVS_RESULT_FAILURE;
    }

    // 构造删除预录任务
    SVS_DEL_RECORD *pDelRecord = (SVS_DEL_RECORD *)((void *)mb->rd_ptr());
    pDelRecord->DeleteTime = deleteTime;
    pDelRecord->MsgType = deleteType;

    //memcpy(pDelRecord->LensId, m_szDeviceID, LENSDEVID_LEN);
    //pDelRecord->LensId[LENSDEVID_LEN] = '\0';
    SetLocalizeDeviceId(pDelRecord->LensId,
        sizeof(pDelRecord->LensId),
        m_szDeviceID);

    //删除任务入队列
    ACE_Time_Value tvTimeout(ACE_OS::gettimeofday());
    tvTimeout.set(tvTimeout.sec(), tvTimeout.usec() + DEL_RECORD_TASK_ENQUEUE_TIMEOUT);
    int32_t enqueueRet = CAC_RT_Record_Server::instance()->enqueue_prerecord_del_task(mb, &tvTimeout);

    if (SVS_RESULT_FAILURE == enqueueRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Add deleting record file task failed, "
            "Enqueue failed.cameraId[%s], deleting task type[%d], time[%u].",
            m_szDeviceID,
            deleteType,
            deleteTime));

        delete mb;
        mb = NULL;
        pDelRecord = NULL;

        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO,
        "Add deleting record file task succeeded."
        "cameraId[%s],deleting task type[%d], time[%u].",
        m_szDeviceID,
        deleteType,
        deleteTime));

    return SVS_RESULT_OK;//lint !e429 already enqueue
}//lint !e429 already enqueue

void SVS_File_Egress::update_db_record_index(const char* newFileName, int32_t iRecordStopFlag/* = 0*/)
{
    SVS_TRACE();

    int32_t nRet = m_IndexList.updateLocalIndex(m_FirstWriteRecordTime,
                                            m_LastWriteRecordTime,
                                            m_ullWriteDataSize,
                                            newFileName,
                                            iRecordStopFlag);

    if (SVS_RESULT_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] update record index fail as select index from memory failed."
            "startTime[%u],endTime[%u],fileName[%s]",
            m_szDeviceID,
            m_FirstWriteRecordTime, m_LastWriteRecordTime, newFileName));
    }

    return;
}

int32_t SVS_File_Egress::overwrite_record_notify(const time_t newIndexStartTime)
{
    SVS_TRACE();
    return m_IndexList.updateDBIndexStartTime(newIndexStartTime);
}

void SVS_File_Egress::setTimeRemove(time_t timeRemove)
{
    m_timeRemove = timeRemove;
}

int32_t SVS_File_Egress::add_index(const time_t indexStartTime,
                                  const time_t indexStopTime)
{
    return m_IndexList.updateDBIndex(indexStartTime, indexStopTime);
}

void SVS_File_Egress::close_finished()
{
    // 更新当前的索引文件
    (void)m_RecordIndexFile.update_indexfile(m_LastWriteRecordTime);

    int32_t iRecordStopFlag = 1;
    // 发送消息，更新数据库的索引信息
    update_db_record_index(m_szRecordFilePath, iRecordStopFlag);

    // 关闭录像文件
    if (-1 != m_RecordFileFD)
    {
        (void)SVS_Disk_Manager::instance().closeFile(m_RecordFileFD);
        SVS_Disk_Manager::instance().freeVfd(m_RecordFileFD);
        m_RecordFileFD = -1;
    }

    // 关闭索引文件
    (void)m_RecordIndexFile.close();

    // 删除预录文件，如果有的话
    time_t endTime = m_LastWriteRecordTime;
    endTime += BIG_RECORDFILE_TIME_SLICE - (endTime % BIG_RECORDFILE_TIME_SLICE);
    (void)delete_prorecord_file(endTime);

    m_IndexList.close();

    // 如果这个时候列表不为空，说明有问题!!!!
    pthread_mutex_lock(&m_mediaCallbackParamMutex);
    if(!m_mediaCallbackParamList.empty())
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "The media callback parameter list is not empty."
            "CameraId[%s], list size[%u].",
            m_szDeviceID,
            m_mediaCallbackParamList.size()));
    }
    pthread_mutex_unlock(&m_mediaCallbackParamMutex);

    (void)SVS_Media_Processor::instance().stop_finish_notify(m_ulIOHandlerIndex);

    return ;
}


