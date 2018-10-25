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

    // ����ļ������Ч������д�̹߳ر�egress������closeֱ�ӹر�
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

        // ���һ���ر���Ϣ,���ʧ�ܣ��ͷ��ڴ�
        pthread_mutex_lock(&m_mediaCallbackParamMutex);
        pFuncParam->m_ulWriteIndex = ++m_ulWriteIndex;

        int32_t nRet = SVS_Disk_Manager::instance().writeFile(m_RecordFileFD, NULL, &write_call_back, pFuncParam);
        if(SVS_RESULT_OK == nRet)
        {
            m_mediaCallbackParamList.push_back(pFuncParam);
            pthread_mutex_unlock(&m_mediaCallbackParamMutex);

            return SVS_RESULT_OK;
        }

        // �������д��Ϣʧ��
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

        // �����û��д�������򷵻�
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
�� �� ��  : write
��������  : ��ý������д�뵽¼���ļ��У���Recv_Streamֱ�ӵ���
�������  : mb  :   ��Ҫд��ý�����ݿ�
�������  : ��
�� �� ֵ  : �ɹ����� 0 ���򷵻�-1
�޸���ʷ  :
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

    // �����ص��ӿڣ���IO����ִ��ʵ�ʵ�д�����󣬽�����ص�
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

    // ����Ƿ���Ҫ�л��ļ�(�л��ļ��͵�һ�δ�)
    if ( recvFrameTime >= (m_LastSwitchRecordTime + BIG_RECORDFILE_TIME_SLICE ))
    {
        // ¼���ļ����������´򿪣���Ҫ�����ļ����´�
        pFuncParam->m_ulChangeRecordFile = 1;
    }

    bool bControl = true; //only for compile warnning
    do
    {
        if ( (EGRESS_STATUS_WRITING != m_EgressStatus)
            || (1 == pFuncParam->m_ulChangeRecordFile) )
        {
            bool medaDataPartedFlag = false;
            // ��һ���µ�¼���ļ�
            nRet = open_file(recvFrameTime,medaDataPartedFlag);
            if (0 != nRet)
            {
                SVS_LOG((SVS_LM_WARNING, "Camera[%s] open file fail.", m_szDeviceID));
                SVS_DELETE(pFuncParam);

                return SVS_RESULT_FAILURE;
            }

            // ���ļ��ɹ�������״̬
            m_EgressStatus = EGRESS_STATUS_WRITING;
            pFuncParam->m_ullRecordFileSize = m_ullRecordFileSize;

            // ���±������ļ���·��
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
                // ����Ѿ����´�����¼���ļ���дʧ�ܣ��򷵻�ʧ��
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
                // �������´��ļ�����һ��
                pFuncParam->m_ulChangeRecordFile = 1;
                continue;
            }
        }
        // �ѻص����������б�
        m_mediaCallbackParamList.push_back(pFuncParam);
        pthread_mutex_unlock(&m_mediaCallbackParamMutex);

        // д��ɹ����˳�ѭ��
        break;
    }    while (bControl);        // �ڳ����п��ƺ�ʱ�˳�ѭ��

    // �յ�������������
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

//        // ɾ���ص������Ĳ���
//        SVS_DELETE(pCallBackParam);
        return nRet;
    }

//    // ɾ���ص������Ĳ���
//    SVS_DELETE(pCallBackParam);
    return 0;
}

/*****************************************************************************
 �� �� ��  : open_file
 ��������  : ��¼���ļ���¼���ļ���������������
 �������  : ��Ҫд���¼���һ֡ʱ�䣬���ڼ���¼���ļ�����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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

    // �رվɵ�¼���ļ�
    if ( -1 != m_RecordFileFD )
    {
        SVS_LOG((SVS_LM_INFO,
            "camera[%s] close record file[%s], fd[%d].",
            m_szDeviceID,
            m_szRecordFilePath,
            m_RecordFileFD));

        // ɾ��Ԥ¼�ļ�
        (void)delete_prorecord_file(dataTime);

        (void)SVS_Disk_Manager::instance().closeFile(m_RecordFileFD);
    }

    // ǰ��¼�񱸷ݺ�ƽ̨ʵʱ¼���ļ���չ����ͬ
    char * fileExName = (char*)SVS_RT_FILE_EXNAME;
    if ( 0 != m_timeRemove )
    {
        fileExName = (char*)SVS_BACKUP_FILE_EXNAME;
    }

    // ��¼���ļ�
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

        // �������û�д��̿���ʱ���ı�Egress״̬
        if (SVS_ERR_NORAID == nRet)
        {
            m_EgressStatus = EGRESS_STATUS_NODISK;
        }
        else
        {
            // ��������
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

    // ��ʼ¼���ļ���С
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

    // ����ͳ����Ϣ
    m_egressStat.updateFileName(m_szRecordFilePath);
    m_egressStat.updateRecordFileSize(m_ullRecordFileSize);

    // 7. ���һ���л��ļ�ʱ��
    m_LastSwitchRecordTime = fileTime;

    SVS_LOG((SVS_LM_DEBUG,
        "camera[%s] success to open file[%s], fd[%d]",
        m_szDeviceID, m_szRecordFilePath, m_RecordFileFD));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : on_write_finish
��������  : ����д������Ļص�����д����Ϊ�ɹ�ʱ�����¹ؼ�֡��Ϣ��������Ϣ��
            ��д��ʧ��ʱ�����г�����
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
*****************************************************************************/
int32_t SVS_File_Egress::on_write_finish(int32_t write_result, void * param)
{
    CallBackParam *pCallBackParam = (CallBackParam*)param;
    if (NULL == pCallBackParam)
    {
        return SVS_RESULT_FAILURE;
    }

    // �����ص������б��޸Ļص���ʶ
    pthread_mutex_lock(&m_mediaCallbackParamMutex);
    bool bIsFind = false;
    CallbackParamListIter iter = m_mediaCallbackParamList.begin();
    while(iter != m_mediaCallbackParamList.end())
    {
        // ��ַ��idֵһ����Ϊͬһ������,��֤��Ϣδ���𴦸Ķ�
        if((*iter)->m_ulWriteIndex == pCallBackParam->m_ulWriteIndex)
        {
            (*iter)->m_bIsFinished = true;
            (*iter)->m_nWriteRet = write_result;
            bIsFind = true;

            if(NULL != pCallBackParam->m_pMediaData)
            {
                // д����̵�����������
                m_ullWriteDataSize += pCallBackParam->m_pMediaData->length();
            }

            break;
        }

        iter++;
    }

    // ���û���ҵ���������������
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

    // ��������
    time_t currTime = time(NULL);
    uint32_t ulListSize = m_mediaCallbackParamList.size();
    iter = m_mediaCallbackParamList.begin();
    while(iter != m_mediaCallbackParamList.end())
    {

        // ����Ѿ�д����ˣ���ֱ�ӵ���д��������
        if((*iter)->m_bIsFinished)
        {
            pCallBackParam = m_mediaCallbackParamList.front();
            m_mediaCallbackParamList.pop_front();
            --ulListSize;
            // ���д����ˣ��������Ĺر���Դ����,֪ͨMedia processor�ر����
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

                // �漰���ͷ�egress���������ͷ���
                pthread_mutex_unlock(&m_mediaCallbackParamMutex);

                // ֹͣ��������
                close_finished();

                return SVS_RESULT_OK;
            }

            (void)handleWriteCallBackParam(pCallBackParam->m_nWriteRet, pCallBackParam);

            // �ͷ���Դ
            SVS_DELETE(pCallBackParam);

            // ���list���д�����Ļص��������Ž���while�������������(������ʹ��egress��Ա������egress�ᱻclose_finished�ر�)
            if(0 < ulListSize)
            {
                // ��ָ��ָ���ʼ��λ�ã���֤��������ʱ˳����е�
                iter = m_mediaCallbackParamList.begin();
                continue;
            }
        }
        else if((currTime - (*iter)->m_ulWriteTime) >= BIG_RECORDFILE_TIME_SLICE)
        {
            // ���ʱ���Ѿ��ܾ��ˣ�˵��ǰ���д�߳����������־¼���쳣
            // ����ɾ���ڵ㣬��Ϊɾ���ˣ�д�ͻص���¼���С�Ͳ�һ���ˣ�egress��Ӧ�ü����ʹ���
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
        else // ����û��д������Ҳû�г�ʱ�ģ�����������������
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
            // ������������,�������д¼��״̬�����óɴ���״̬������д¼��ʱ���´��ļ�
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
        // д����ʧ��,�������д¼��״̬�����óɴ���״̬������д¼��ʱ���´��ļ�
        if(EGRESS_STATUS_WRITING == m_EgressStatus)
        {
            m_EgressStatus = EGRESS_STATUS_ERROR;
        }

        SVS_LOG((SVS_LM_WARNING,
            "camera[%s] write media data fail, errcode[%d].",
            m_szDeviceID,
            write_result));
    }

//    // д����̵�����������
//    m_ullWriteDataSize += pCallBackParam->m_pMediaData->length();

    // ���״̬�ǹر�״̬��������д���ˣ���ر���ɣ�֪ͨmedia process
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
�� �� ��  : update_record_index
��������  : ����¼���ļ�����
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
�޸���ʷ  :
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

    // ����л��˴��̻����л���¼���ļ�
    // ��Ҫ���´������ļ�
    if ((1 == pCallBackParam->m_ulChangeDisk)
        || (1 == pCallBackParam->m_ulChangeRecordFile))
    {
        if (0 != m_LastWriteRecordTime)
        {
            // ���¾ɵ������ļ�
            (void)m_RecordIndexFile.update_indexfile(m_LastWriteRecordTime);

            // ������Ϣ���������ݿ��������Ϣ
            update_db_record_index(NULL);

            // �رվɵ������ļ�
            (void)m_RecordIndexFile.close();
        }

        // ���´��������ļ�
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

        // �������ļ�������¼���ļ��ĳ�ʼ��С
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

    // 7. ����д��¼���ļ���һ֡ʱ��
    if ( 0 == m_FirstWriteRecordTime)
    {
        m_FirstWriteRecordTime = recvFrameTime;
    }

    // ��ӡһ�������е�������¼
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

    // ������ǰmb�Ĺؼ�֡��Ϣ
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
�� �� ��  : parse_key_frame
��������  : ������ǰд��¼���ļ��е�ý���Ĺؼ�֡��ʱ�������¼���ļ�����ƫ��λ��
            �ѽ���������Ϣд��¼�������ļ����Ա��ط�ʱ���ٶ�λ
�������  : mb  :   ��Ҫд��ý�����ݿ�
�������  : ��
�� �� ֵ  : NA
�޸���ʷ  :
*****************************************************************************/
void SVS_File_Egress::parse_key_frame(const CallBackParam *pCallBackParam)
{
    ACE_Message_Block *pMsgBlock = pCallBackParam->m_pMediaData;
    SVS_Recv_Media_Header *pRecvMediaHeader = NULL;
    pRecvMediaHeader = (SVS_Recv_Media_Header*)(void*)pMsgBlock->base();

    uint32_t ulChangeDiskFlag      = pCallBackParam->m_ulChangeDisk;    // �Ƿ��л�����
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
            // ��ȡmb��һ֡�ڴ��ļ��е�ƫ�ƣ�����vector�����Բ��ǹؼ�֡�����ڴ�ƫ��
            pMediaAllHeader         = (SVS_NEW_ALL_HEADER*)((void *)currPos);
            keyFrameInfo.TimeTick   = pMediaAllHeader->FrameHeaer.nTimeTick;
            keyFrameInfo.FileOffset = recordFileSize;
            bInsertClusterOffset    = false;

            keyFrameVec.push_back(keyFrameInfo);
        }

        pMediaAllHeader = (SVS_NEW_ALL_HEADER*)((void *)currPos);

        // ����ǹؼ���
        if ( 1 == pMediaAllHeader->FrameHeaer.bKeyFrame)
        {
            // ����ÿ�������һ���ؼ�֡,��ֹ�ؼ��巺�ģ�ʱ�����λ����ת������
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

        // ���µ�ǰ¼���ļ�����
        recordFileSize += pMediaAllHeader->ExtendFrameHeader.nAdjustFrameSize;
        currPos += pMediaAllHeader->ExtendFrameHeader.nAdjustFrameSize;

        // �����Ҫ���������ļ�
        if ((NULL != pRecvMediaHeader->IndexList) && (!pRecvMediaHeader->IndexList->empty()))
        {
            SVS_Media_Index *mediaIndex = &pRecvMediaHeader->IndexList->front();
            if (pMediaAllHeader->PacketHeader.m_dwFrameID == mediaIndex->LastIndexFrameID)
            {
                // ���¹ؼ�֡��Ϣ
                (void)m_RecordIndexFile.update_key_frame(keyFrameVec);
                keyFrameVec.clear();

                // ���������ļ�
                (void)m_RecordIndexFile.update_indexfile( mediaIndex->LastIndexRecvTime);
                m_LastWriteRecordTime = mediaIndex->LastIndexRecvTime;

                // �������ݿ���������¼
                update_db_record_index(pCallBackParam->m_RecordFileName);
                if (1 == ulChangeDiskFlag)
                {
                    ulChangeDiskFlag = 0;
                }
                pRecvMediaHeader->IndexList->pop_front();

                bInsertClusterOffset = true;
            }
        }

        // ���������Ҫ�ϳ�����
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

    // ���¹ؼ�֡��Ϣ
    if ( 0 < keyFrameVec.size() )
    {
        (void)m_RecordIndexFile.update_key_frame(keyFrameVec);
        keyFrameVec.clear();
    }

    // 6. ����д��¼���ļ������һ֡ʱ��
    m_LastWriteRecordTime = pRecvMediaHeader->LastFrameRecvTime;

    // ��ֹ���л�����ʱ�պ�û�и�����������ʱҲ��Ҫ�����ݿ��в���һ��������¼
    if (1 == ulChangeDiskFlag)
    {
        update_db_record_index(pCallBackParam->m_RecordFileName);
        ulChangeDiskFlag = 0;
    }
    // ����¼���ļ���С
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

    // ���û�������ϼ�¼�����˳�
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
        // 1.���¾ɵ������ļ�
        // �������������ĵ�һ֡�������ˣ����޸Ķ�ǰ�����ʱ��
        if(0 == mediaIndex->LastIndexRecvTime)
        {
            mediaIndex->LastIndexRecvTime = m_LastWriteRecordTime;
        }

        // ���¹ؼ�֡��Ϣ
        (void)m_RecordIndexFile.update_key_frame(keyFrameVec);
        keyFrameVec.clear();

        // ���������ļ�
        (void)m_RecordIndexFile.update_indexfile( mediaIndex->LastIndexRecvTime);
        m_LastWriteRecordTime = mediaIndex->LastIndexRecvTime;

        // �������ݿ���������¼
        update_db_record_index(pCallBackParam->m_RecordFileName);

        // ��ֹ���л�����ʱ�պ�û�и�����������ʱҲ��Ҫ�����ݿ��в���һ��������¼
        if (1 == ulChangeDiskFlag)
        {
            update_db_record_index(pCallBackParam->m_RecordFileName);
            ulChangeDiskFlag = 0;
        }

        update_db_record_index(NULL);

        // �رվɵ������ļ�
        (void)m_RecordIndexFile.close();

        // 3.���´��������ļ�
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

        // �������ļ�������¼���ļ��ĳ�ʼ��С
        m_RecordIndexFile.set_record_file_size(fileSize);

        // 4.���±���������¼
        m_FirstWriteRecordTime = mediaIndex->NewIndexRecvTime;

        m_LastWriteRecordTime = mediaIndex->NewIndexRecvTime;

        update_db_record_index(pCallBackParam->m_RecordFileName);

        pRecvMediaHeader->IndexBreakList->pop_front();

        return 1;
    }

    return SVS_RESULT_OK;
}
/*****************************************************************************
�� �� ��  : delete_prorecord_file
��������  : ɾ��Ԥ¼�ļ�
�������  : deleteTime:����ɾ���ĸ�Сʱ��Ԥ¼�ļ�
�������  :
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t SVS_File_Egress::delete_prorecord_file(time_t deleteTime)
{
    SVS_TRACE();

    // �����ǰ��¼�񱸷�(ʵʱ¼��m_timeRemoveΪ0)������ɾ��Ԥ¼��?
    if(0 != m_timeRemove )
    {
        return SVS_RESULT_OK;
    }

    // ����Ǹո�����¼����ǰ��û����Ҫɾ����Ԥ¼¼���ļ�
    if (0 == m_LastWriteRecordTime)
    {
        return SVS_RESULT_OK;
    }

    time_t lastDbRecordIndex = m_IndexList.LastDBRecordIndex();
    // ����ϴ����������ʱ��Ϊ0.˵��ֻ����Ԥ¼������û����⣬���Դ���ɾ��Ԥ¼����
    // ���Ҫɾ�������Сʱû��������⣬ҲҪɾ��Ԥ¼
    if((0 == lastDbRecordIndex)
    || (lastDbRecordIndex <= (deleteTime - BIG_RECORDFILE_TIME_SLICE)))
    {
        (void)add_delete_task(DEL_RECORD_TYPE_PRERECORD,deleteTime);
    }
    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : add_delete_task
��������  : ���¼��ɾ������
�������  : deleteType:ɾ�����ͣ�ɾ��Ԥ¼�ļ������������д
            ��ɾ��������ɾ��Ԥ¼��ʱ��
�������  : NA
�� �� ֵ  : �ɹ�����0;ʧ�ܷ���-1
�޸���ʷ  :
*****************************************************************************/
int32_t SVS_File_Egress::add_delete_task(uint8_t deleteType,
                                              time_t deleteTime)
{
    SVS_TRACE();
    // ����ռ�
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

    // ����ɾ��Ԥ¼����
    SVS_DEL_RECORD *pDelRecord = (SVS_DEL_RECORD *)((void *)mb->rd_ptr());
    pDelRecord->DeleteTime = deleteTime;
    pDelRecord->MsgType = deleteType;

    //memcpy(pDelRecord->LensId, m_szDeviceID, LENSDEVID_LEN);
    //pDelRecord->LensId[LENSDEVID_LEN] = '\0';
    SetLocalizeDeviceId(pDelRecord->LensId,
        sizeof(pDelRecord->LensId),
        m_szDeviceID);

    //ɾ�����������
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
    // ���µ�ǰ�������ļ�
    (void)m_RecordIndexFile.update_indexfile(m_LastWriteRecordTime);

    int32_t iRecordStopFlag = 1;
    // ������Ϣ���������ݿ��������Ϣ
    update_db_record_index(m_szRecordFilePath, iRecordStopFlag);

    // �ر�¼���ļ�
    if (-1 != m_RecordFileFD)
    {
        (void)SVS_Disk_Manager::instance().closeFile(m_RecordFileFD);
        SVS_Disk_Manager::instance().freeVfd(m_RecordFileFD);
        m_RecordFileFD = -1;
    }

    // �ر������ļ�
    (void)m_RecordIndexFile.close();

    // ɾ��Ԥ¼�ļ�������еĻ�
    time_t endTime = m_LastWriteRecordTime;
    endTime += BIG_RECORDFILE_TIME_SLICE - (endTime % BIG_RECORDFILE_TIME_SLICE);
    (void)delete_prorecord_file(endTime);

    m_IndexList.close();

    // ������ʱ���б�Ϊ�գ�˵��������!!!!
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


