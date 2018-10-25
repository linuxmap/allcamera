#include <errno.h>
#include "svs_log_msg.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_time.h"
#include "svs_adapter_def.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_scc_connector.h"
#include "svs_adapter_config.h"
#include "svs_adapter_common.h"
#include "svs_adapter_service_task.h"
#include "svs_adapter_config.h"
#include "svs_vms_msg_factory.h"
#include "svs_adapter_business_manager.h"
#include "svs_adapter_vms_auth.h"

int32_t CBusinessReportTimer::handle_timeout(const ACE_Time_Value &tv, const void *arg)
{
    CMduServiceTask::instance()->sendBusinessReport();

    return 0;
}

CMduSccConnector::CMduSccConnector()
{
    m_pRecvMsgQueue        = NULL;
    m_lReportTimerID       = -1;
    m_pBusinessReportTimer = NULL;
}

CMduSccConnector::~CMduSccConnector()
{
    m_pRecvMsgQueue = NULL;

    try
    {
        if (NULL != m_pBusinessReportTimer)
        {
            stopBusinessReportTimer();

            delete m_pBusinessReportTimer;
            m_pBusinessReportTimer = NULL;
        }
    }
    catch(...)
    {

    }
}

/**
* Description: ����SCC�����ӣ��������ֱ�Ӵ�MduConfig�л�ȡ
* @param [in] pRcvQueue: ������Ϣ����
* @return RET_OK - �ɹ�, Other - ʧ��
*/
int32_t CMduSccConnector::openConnector(ACE_Message_Queue<ACE_SYNCH>* pRcvQueue)
{
    if (NULL == pRcvQueue)
    {
        return RET_ERR_PARAM;
    }

    m_pRecvMsgQueue = pRcvQueue;

    try
    {
        m_pBusinessReportTimer = new CBusinessReportTimer();
    }
    catch(...)
    {
        return RET_ERR_PARAM;
    }

    uint32_t unServerType      = SERVER_TYPE_MU_STREAM;
    uint32_t unServiceNetType  = CMduConfig::instance()->getServiceNetType();
    const char *pszServiceID             = CMduConfig::instance()->getServiceId();
    const char *pszServiceName           = CMduConfig::instance()->getServiceName();

    int32_t nRet = init(pszServiceID, unServerType, pszServiceName, unServiceNetType,
                    CMduConfig::instance()->getSccHeartbeatInterval(),
                    CMduConfig::instance()->GetRegisterPasswd());
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"init scc connector fail, ret[%d].", nRet));
        return RET_FAIL;
    }

    setServerAddr(CMduConfig::instance()->getSccIp(), CMduConfig::instance()->getSccPort());

    uint32_t   unLocalIp         = CMduConfig::instance()->getServiceIp();
    uint32_t   unLocalPortNum    = 0;
    uint16_t* pusLocalPortList = NULL;
    CMduConfig::instance()->getServicePortList(pusLocalPortList, unLocalPortNum);

    LOCAL_PORT_LIST localList;
    for (uint32_t i = 0; i < unLocalPortNum; i++)
    {
        localList.push_back(pusLocalPortList[i]);
    }

    setLocalAddr(unLocalIp, localList);

    nRet = open(NULL);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"open scc connection fail, ret[0x%x].", nRet));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"open scc connection success."));
    return RET_OK;
}

/**
* Description: �ر���SCC������
* @param ��
* @return ��
*/
void CMduSccConnector::closeConnector()
{
    SVS_LOG((SVS_LM_INFO,"start close scc connection."));
    close();
    m_pRecvMsgQueue = NULL;

    return;
}


/**
* Description: ����������Ͻ��յ�����Ϣ�������麯��ʵ��
* @param [in] pMsg: ���յ�����Ϣ��
* @return ��
*/
void CMduSccConnector::handleRecvMessage(ACE_Message_Block *pMsg)
{
    if (NULL == pMsg)
    {
        return;
    }

    if (NULL == m_pRecvMsgQueue)
    {
        return;
    }

    ACE_Time_Value delayTime = ACE_OS::gettimeofday() + ACE_Time_Value(0, MDU_MAX_QUEUE_DELAY);
    int32_t nRet = m_pRecvMsgQueue->enqueue_tail(pMsg, &delayTime);
    if (-1 == nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"scc connector enqueue recved message fail, retcode[%d].",
                        errno));
        return;
    }
    return;
}

/**
* Description: ���?��ʧ�ܵ���Ϣ�������麯��ʵ��
* @param [in] pMsg: ���յ�����Ϣ��
* @return ��
*/
void CMduSccConnector::handleFailedMessage(ACE_Message_Block *pMsg)
{
    if (NULL == pMsg)
    {
        return;
    }

    if (sizeof(SVS_MSG_HEADER) > pMsg->length())
    {
        return;
    }

    // MDU���Է���ʧ�ܽ��д��?��ӡ�澯��־����
    SVS_MSG_HEADER *pHeader = (SVS_MSG_HEADER *)(void*)pMsg->rd_ptr();
    SVS_LOG((SVS_LM_WARNING,"send scc message fail, msgtype[0x%x] transno[0x%x].",
                    pHeader->MsgType, pHeader->TransactionNo));
    return;
}


void CMduSccConnector::handleTaskReport()
{
    if (HANDLE_STATUS_REGISTED != getStatus())
    {
        return;
    }

    uint32_t unServerIp = 0;
    uint16_t usServerPort = 0;
    uint32_t unLocalIp = 0;
    uint16_t usLocalPort = 0;
    getServerAddr(unServerIp, usServerPort);
    getLocalAddr(unLocalIp, usLocalPort);
    SVS_LOG((SVS_LM_INFO,
        "start send distribute task report to server[0x%x:%d], localAddr[0x%x:%d].",
        unServerIp,
        usServerPort,
        unLocalIp,
        usLocalPort));

    CMduSessionList reportList;

    BUSINESS_LIST businessList;
    CMduBusinessManager::instance()->getAllBusiness(businessList);
    BUSINESS_LIST_ITER iter;
    CMduBusiness* pBusiness = NULL;
    CMduSession* pRecvSession = NULL;
    CMduSession* pSendSession = NULL;
    for (iter = businessList.begin(); iter != businessList.end(); ++iter)
    {
        pBusiness = *iter;
        if (NULL == pBusiness)
        {
            continue;
        }

        pRecvSession = pBusiness->getSession(pBusiness->getRecvStreamID());
        if (NULL == pRecvSession)
        {
            continue;
        }

        if (MDU_SESSION_STATUS_DISPATCHING != pRecvSession->getStatus())
        {
            continue;
        }

        pSendSession = pBusiness->getSession(pBusiness->getSendStreamID());
        if (NULL == pSendSession)
        {
            continue;
        }

        if (MDU_SESSION_STATUS_DISPATCHING != pSendSession->getStatus())
        {
            continue;
        }

        reportList.push_back(pRecvSession);
        reportList.push_back(pSendSession);
    }


    if (reportList.empty())
    {
        sendDistributeTask(0, 0, NULL);
    }
    else
    {
        uint32_t unTaskNum = reportList.size();
        uint32_t unTaskSeq = 0;

        CMduSessionIter sessionIter;
        CMduSession* pSession = NULL;
        for (sessionIter = reportList.begin(); sessionIter != reportList.end(); ++sessionIter)
        {
            pSession = *sessionIter;
            if (NULL == pSession)
            {
                continue;
            }

            unTaskSeq++;
            sendDistributeTask(unTaskNum, unTaskSeq, pSession);
        }
    }

    CMduBusinessManager::instance()->releaseBusiness(businessList);

    uint32_t inputNum = 0;
    uint32_t outputNum = 0;
    uint32_t bidirectionNum = 0;
    uint32_t totalNum = 0;
    CMduSessionFactory::instance()->getSessionCount(inputNum, outputNum, bidirectionNum, totalNum);

    SVS_LOG((SVS_LM_INFO,"stop send distribute task report, all task [%d], report task[%d].",
                    totalNum, reportList.size()));

    return;
}



void CMduSccConnector::registerNotify()
{
    CMduServiceTask::instance()->stopSessions();

    CMduServiceTask::instance()->sendSysLoadReport();

    startBusinessReportTimer();

    return;
}

void CMduSccConnector::setTagIp
(
    uint32_t unIpType,
    uint32_t unIndex,
    uint32_t unIpAddr,
    char* pBuf,
    uint32_t& unBufLen
)const
{
    if (NULL == pBuf)
    {
        return;
    }

    ACE_INET_Addr addr;
    addr.set((uint16_t)0, unIpAddr);
    SVS_TLV_V1 *pTlv = (SVS_TLV_V1*)(void*)pBuf;
    pTlv->Tag = SVS_TLV_TAG_IP;
    pTlv->Length = sizeof(SVS_MDU_IP);

    SVS_MDU_IP* pMduIp = (SVS_MDU_IP*)(void*)(pBuf + TLV_MIN_LENGTH);
    pMduIp->uiIpType = unIpType;
    pMduIp->uiIndex  = unIndex;
    strncpy(pMduIp->szIp, addr.get_host_addr(), SVS_IP_LEN);

    unBufLen += (TLV_MIN_LENGTH + pTlv->Length);

    return;
}

void CMduSccConnector::registServer()
{
    if (HANDLE_STATUS_CONNECTED != m_enStatus)
    {
        return;
    }

    if (NULL == m_pClientLink)
    {
        return;
    }

    SVS_LOG((SVS_LM_INFO,"start regist to server[%s:%d], localAddr[%s:%d].",
            m_szServerIp,
            m_ServerAddr.get_port_number(),
            m_szLocalIp,
            m_LocalAddr.get_port_number()));

    char msgBuffer[KILO] = {0};
    SVS_MSG_SERVER_REGIST_REQ *pReq = (SVS_MSG_SERVER_REGIST_REQ *)msgBuffer;

    pReq->MsgHeader.MsgType         = SVS_MSG_TYPE_SERVER_REGIST_REQ;
    pReq->MsgHeader.ProtocolVersion = SVS_MSG_PROTOCOL_VERSION;
    pReq->MsgHeader.TransactionNo   = 1;
    pReq->MsgHeader.PacketLength    = sizeof(SVS_MSG_SERVER_REGIST_REQ);

    pReq->ResetFlags     = m_unResetFlags;
    pReq->ServerType     = m_unServerType;
    pReq->ServiceNetType = m_unServiceNetType;
    memcpy(pReq->Name, m_szName, SERVER_NAME_LEN);
    pReq->LastRunTime    = m_ulServerLastRunTime;
    pReq->RtspservicePort    = (uint32_t)CMduConfig::instance()->getRtspServerPort();
    pReq->HlsservicePort = (uint32_t)CMduConfig::instance()->getHLSServerPort();
    pReq->RtmpservicePort = (uint32_t)CMduConfig::instance()->getRtmpServerPort();
    memcpy(pReq->BindServerID,CMduConfig::instance()->getServiceId(),SERVER_ID_LEN);

    MDU_IP_LIST  ipList;
    uint32_t unInternalIp = CMduConfig::instance()->getInternalMediaIp();
    CMduConfig::instance()->getExternalMediaIpList(ipList);


    ACE_INET_Addr addr;
    addr.set((uint16_t)0, unInternalIp);
    strncpy(pReq->serviceIP, addr.get_host_addr(), SVS_IP_LEN);

    fillRegisterMd5Sum(pReq,  m_szRegisterPasswd, m_nRegisterPasswdLen);

    int32_t nRet = m_pClientLink->sendMsg(m_unLinkIndex, msgBuffer,
                                      pReq->MsgHeader.PacketLength, 0);
    if (SVS_STACK_OK != nRet)
    {
        m_enStatus = HANDLE_STATUS_DISCONNECT;
        m_ulStatusTime = SVS_GetSecondTime();

        SVS_LOG((SVS_LM_WARNING,"client handle[%d]  send regist msg fail, localAddr[%s:%d] "
                "server[%s:%d].",
                m_unLinkIndex,
                m_szLocalIp,
                m_LocalAddr.get_port_number(),
                m_szServerIp,
                m_ServerAddr.get_port_number()));
        return;
    }

    m_enStatus = HANDLE_STATUS_REGISTING;
    m_ulStatusTime = SVS_GetSecondTime();
    SVS_LOG((SVS_LM_INFO,"client handle[%d] send regist message success, localAddr[%s:%d], "
            "server[%s:%d].",
            m_unLinkIndex,
            m_szLocalIp,
            m_LocalAddr.get_port_number(),
            m_szServerIp,
            m_ServerAddr.get_port_number()));
    return;
}

void CMduSccConnector::connectNotify(const ConnEvent* pstEvent)
{
    if (NULL == pstEvent)
    {
        return;
    }

    if (EVENT_TYPE_CONNECTED != pstEvent->EventType)
    {
        // ���ӶϿ�����Ҫȡ��ҵ���ϱ�ͳ�ƶ�ʱ��
        stopBusinessReportTimer();
    }

    return;
}


void CMduSccConnector::startBusinessReportTimer()
{
    if ((NULL == m_pBusinessReportTimer)
            || (-1 != m_lReportTimerID))
    {
        return;
    }

    ACE_Reactor *pReactor = CMduServiceTask::instance()->getTimerReactor();
    if (NULL == pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"mdu scc connector start business report timer fail, no timer reacotr."));
        return;
    }

    ACE_Time_Value tv(MDU_BUSINESS_REPORT_INTERVAL, 0);
    m_lReportTimerID = pReactor->schedule_timer(m_pBusinessReportTimer, this, tv, tv); //lint !e613
    if (-1 == m_lReportTimerID)
    {
        delete m_pBusinessReportTimer;
        m_pBusinessReportTimer = NULL;
        SVS_LOG((SVS_LM_WARNING,"mdu scc connector start business report timer fail, errno[%d].",
                        ACE_OS::last_error()));
        return;
    }

    SVS_LOG((SVS_LM_INFO,"mdu scc connector start business report timer[%d] success.",
                    m_lReportTimerID));
    return;
}

void CMduSccConnector::stopBusinessReportTimer()
{
    if ((-1 == m_lReportTimerID)
            || (NULL == m_pBusinessReportTimer))
    {
        return;
    }

    ACE_Reactor *pReactor = CMduServiceTask::instance()->getTimerReactor();
    if (NULL == pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"mdu scc connector stop business report timer fail, no timer reacotr."));
        return;
    }

    if (-1 == pReactor->cancel_timer(m_lReportTimerID))
    {
        SVS_LOG((SVS_LM_WARNING,"mdu scc connector stop business report timer fail, errno[%d].",
                        ACE_OS::last_error()));
        return;
    }

    SVS_LOG((SVS_LM_INFO,"mdu scc connector stop business report timer[%d] success.",
                        m_lReportTimerID));
    m_lReportTimerID = -1;
    return;
}


void CMduSccConnector::sendDistributeTask
(
    uint32_t unNum,
    uint32_t unSeq,
    const CMduSession* pSession
)
{
    /*ACE_Message_Block* pMb = CMediaBlockBuffer::instance().allocMediaBlock();
    if (NULL == pMb)
    {
        return;
    }

    memset(pMb->rd_ptr(), 0x0, pMb->capacity());

    SVS_MSG_VIDEO_DISTRIBUTE_TASK_REPORT_REQ* pRequest = NULL;
    pRequest = (SVS_MSG_VIDEO_DISTRIBUTE_TASK_REPORT_REQ*)(void*)pMb->rd_ptr();

    M_COMMON::FillCommonHeader(&pRequest->MsgHeader,
                                SVS_MSG_TYPE_VIDEO_DISTRIBUTE_TASK_REPORT_REQ,
                                getServerID(),
                                CMduMsgFactory::instance()->getReqTransactionNo(),
                                sizeof(SVS_MSG_VIDEO_DISTRIBUTE_TASK_REPORT_REQ));
    pRequest->DistributeTaskNum = unNum;
    pRequest->DistributeTaskSeq = unSeq;

    uint64_t ullStreamId = 0;
    if (NULL != pSession)
    {
        ullStreamId = pSession->getStreamId();
        pRequest->VideoPT = pSession->getVideoPayload();
        pRequest->AudioPT = pSession->getAudioPayload();
    }

    int32_t nRet = sendMessage(pMb->rd_ptr(), pRequest->MsgHeader.PacketLength);

    if (SVS_STACK_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Send mdu distribute task report fail, session[%Q] task seq[%d] task num[%d].",
            ullStreamId,
            unSeq,
            unNum));
    }
    else
    {
        SVS_LOG((SVS_LM_INFO,
            "Send mdu distribute task report success, session[%Q] task seq[%d] task num[%d].",
            ullStreamId,
            unSeq,
            unNum));
    }

    CMediaBlockBuffer::instance().freeMediaBlock(pMb);*/
    return;
}
