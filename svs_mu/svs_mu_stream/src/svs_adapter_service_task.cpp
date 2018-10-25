#include "svs_adapter_svs_def.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include <vms/vms.h>
#include "svs_adapter_common.h"

#include "svs_adapter_def.h"
#include "svs_adapter_service_task.h"
#include "svs_adapter_session.h"
#include "svs_adapter_session_factory.h"
#include "svs_vms_msg_factory.h"
#include "svs_vms_load_Info_req.h"
#include "svs_adapter_inner_msg.h"
#include "svs_adapter_business_manager.h"
#include "svs_adapter_sys_stat.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_daemon_thread.h"
#include "svs_rtsp_service.h"
#include "svs_rtmp_service.h"
#include "svs_vms_media_setup_req.h"

int32_t CLoadReportTimer::handle_timeout(const ACE_Time_Value &tv, const void *arg)
{
    CMduServiceTask::instance()->sendSysLoadReport();
    return 0;
}

CMduServiceTask* CMduServiceTask::g_mduServiceTask = NULL;
CMduServiceTask::CMduServiceTask()
{
    m_bRunFlag        = true;
    m_pSccConnect     = NULL;
    m_unThreadIndex   = 0;
    m_pDebugReactor   = NULL;
    m_pTimerReactor   = NULL;

    m_pLoadReportTimer = NULL;
}

CMduServiceTask::~CMduServiceTask()
{
    try
    {
        closeServiceTask();
    }
    catch(...)
    {

    }

    m_bRunFlag        = false;
    m_pSccConnect     = NULL;
    m_unThreadIndex   = 0;
    m_pDebugReactor   = NULL;
    m_pTimerReactor   = NULL;
    m_pLoadReportTimer = NULL;

}

int32_t CMduServiceTask::openServiceTask()
{
    SVS_LOG((SVS_LM_INFO,"start open mdu service task."));

    m_bRunFlag  = true;
    if (RET_OK != openDeubgger())
    {
        SVS_LOG((SVS_LM_CRITICAL,"Open service task fail, open debugger fail."));
        return RET_FAIL;
    }

    int32_t nRet = openAllMsgQueue();
    if (RET_OK != nRet)
    {
        return RET_FAIL;
    }

    nRet = createServiceThreads();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"create mdu service threads fail, ret[%d].", nRet));
        return RET_FAIL;
    }

    nRet = startSysStat();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"start sys stat fail, ret[%d].", nRet));
        return RET_FAIL;
    }

    //RTSP Server
    nRet = CMduRtspService::instance().open();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"start rtsp service fail, ret[%d].", nRet));
        return RET_FAIL;
    }

    //RTMP Server
    nRet = CMduRtmpService::instance().open();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"start rtmp service fail, ret[%d].", nRet));
        return RET_FAIL;
    }

    nRet = createSccConnect();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"init scc connection fail, ret[%d].", nRet));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"open mdu service task success."));
    return RET_OK;
}

int32_t CMduServiceTask::svc()
{
    (void)signal(SIGPIPE, SIG_IGN);

    uint32_t unThreadIndex = getThreadIndex();

    switch (unThreadIndex)
    {
        case MAIN_REACTOR_THREAD:
        {
            reactorLoopThread();
            break;
        }

        case SCC_MSG_HANDLER_THREAD:
        {
            sccMessageHandleThread();
            break;
        }
        case MEDIA_MSG_HANDLE_THREAD:
        {
            innerMsgHandleThread();
            break;
        }
        case MDU_TIMER_THREAD:
        {
            mduTimerManagerThread();
            break;
        }
        case MDU_DEBUG_THREAD:
        {
            debugThread();
            break;
        }
        default:
        {
            SVS_LOG((SVS_LM_WARNING,"unrecognize thread index[%d].", unThreadIndex));
            break;
        }
    }

    return RET_OK;
}

int32_t CMduServiceTask::sendMsgToSCC(const CMduSvsMessage* pMessage)
{
    if (NULL == pMessage)
    {
        return RET_FAIL;
    }

    if (NULL == m_pSccConnect)
    {
        return RET_FAIL;
    }


    pMessage->dump();
    return m_pSccConnect->sendMessage(pMessage->getBinaryData(), pMessage->getLength());
}

void CMduServiceTask::closeServiceTask()
{
    SVS_LOG((SVS_LM_CRITICAL,"CMduServiceTask::closeServiceTask begin."));
    if (NULL != m_pSccConnect)
    {
        m_pSccConnect->closeConnector();
        delete m_pSccConnect;
        m_pSccConnect = NULL;
    }


    m_bRunFlag = false;


    closeAllMsgQueue();

    (void)wait();

    SVS_LOG((SVS_LM_CRITICAL,"CMduServiceTask::closeServiceTask end."));
    return;
}

int32_t CMduServiceTask::enqueueInnerMessage(ACE_Message_Block* pMsg)
{
    if (NULL== pMsg)
    {
        return RET_FAIL;
    }

    ACE_Time_Value tv = ACE_Time_Value(ACE_OS::gettimeofday()) + ACE_Time_Value(0, MDU_MAX_QUEUE_DELAY);
    int32_t lRet = m_MediaMsgQueue.enqueue_tail(pMsg, &tv);
    if (-1 == lRet)
    {
        SVS_LOG((SVS_LM_ERROR,"enqueueInnerMessage fail."));
        return RET_FAIL;
    }

    return RET_OK;
}

ACE_Reactor*  CMduServiceTask::getTimerReactor()
{
    return m_pTimerReactor;
}

CMduSccConnector* CMduServiceTask::getSccConnector()
{
    return m_pSccConnect;
}


void CMduServiceTask::sendSysLoadReport() const
{
    MDU_IP_LIST ipList;
    uint32_t unInternalIp = CMduConfig::instance()->getInternalMediaIp();
    CMduConfig::instance()->getExternalMediaIpList(ipList);

    ipList.push_front(unInternalIp);
    uint32_t unMsgLen = sizeof(SVS_MSG_MDU_LOAD_INFO_REQ)
                                   + (ipList.size() - 1) * sizeof(NETWORK_CARD_INFO);
    CMduSvsMessage *pSvsMessage = NULL;
    int32_t nRet = CMduMsgFactory::instance()->createSvsMsg(SVS_MSG_TYPE_MDU_LOAD_INFO_REQ,
                                             unMsgLen,
                                             CMduMsgFactory::instance()->getReqTransactionNo(),
                                             pSvsMessage);
    if ((RET_OK != nRet) || (NULL == pSvsMessage))
    {
        CMduMsgFactory::instance()->destroySvsMsg(pSvsMessage);
        SVS_LOG((SVS_LM_INFO,"Send load report to scc fail, create message fail."));
        return;
    }

    CMduLoadInfoReq *pLoadInfo = dynamic_cast<CMduLoadInfoReq *>(pSvsMessage);
    if (NULL == pLoadInfo)
    {
        return;
    }

    if (RET_OK != pLoadInfo->initMsgBody())
    {
        CMduMsgFactory::instance()->destroySvsMsg(pSvsMessage);
        SVS_LOG((SVS_LM_INFO,"Send load report to scc fail, init message fail."));
        return;
    }

    if (RET_OK != pLoadInfo->handleMessage())
    {
        CMduMsgFactory::instance()->destroySvsMsg(pSvsMessage);
        SVS_LOG((SVS_LM_INFO,"Send load report to scc fail."));
        return;
    }

    CMduMsgFactory::instance()->destroySvsMsg(pSvsMessage);
    SVS_LOG((SVS_LM_INFO,"Send load report to scc success."));
    return;
}

void CMduServiceTask::sendBusinessReport()
{
    if (NULL == m_pSccConnect)
    {
        return;
    }

    BUSINESS_LIST businessList;
    BUSINESS_LIST_ITER iter;
    CMduBusinessManager::instance()->getAllBusiness(businessList);

    uint32_t ulBusinessCount = businessList.size();
    uint32_t ulMsgLen = 0;
    if(0 < ulBusinessCount)
    {
        ulMsgLen = sizeof(SVS_MSG_MU_BUSINESS_REPORT_REQ)
            + ((ulBusinessCount -1) * sizeof(SVS_SBUSINESS_REPORT_INFO));
    }
    else
    {
        ulMsgLen = sizeof(SVS_MSG_MU_BUSINESS_REPORT_REQ);
    }

    do
    {

        char *pBuffer = NULL;
        try
        {
            pBuffer = new char[ulMsgLen];
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_ERROR,"report session info to scc fail. allocate message block failed."));
            break;
        }

        SVS_MSG_MU_BUSINESS_REPORT_REQ * pReqMsg
                                        = (SVS_MSG_MU_BUSINESS_REPORT_REQ*) (void*) pBuffer;
        M_COMMON::FillCommonHeader(&pReqMsg->MsgHeader,
                                    SVS_MSG_TYPE_MU_SESSION_REPORT_REQ,
                                    CMduConfig::instance()->getServiceId(),
                                    CMduMsgFactory::instance()->getReqTransactionNo(),
                                    ulMsgLen);
        pReqMsg->BusinessTotalNum = ulBusinessCount;

        SVS_SBUSINESS_REPORT_INFO* pBusinessInfo = NULL;
        uint32_t i = 0;

        for (iter = businessList.begin();iter != businessList.end(); iter++)
        {
            CMduBusiness *pBusiness = *iter;
            pBusinessInfo = &pReqMsg->BusinessInfo[i];
            pBusiness->statFluxInfo();

            memcpy(pBusinessInfo->DeviceID, pBusiness->getContentID(), DEVICE_ID_LEN);
            pBusinessInfo->ServiceType = pBusiness->getPlayType();
            pBusinessInfo->FluxSize = pBusiness->getFluxszie();
            pBusinessInfo->OperateTimeSpan.StartTime = pBusiness->getStarttime();
            pBusinessInfo->OperateTimeSpan.EndTime   = pBusiness->getEndtime();
        }

        if (RET_OK != m_pSccConnect->sendMessage((char*)pReqMsg,
                                                 pReqMsg->MsgHeader.PacketLength))
        {
            delete[] pBuffer;
            SVS_LOG((SVS_LM_WARNING,"report business info to cc fail. send message failed."));
            break;
        }

        SVS_LOG((SVS_LM_DEBUG,"report business info to cc success,size:[%d].",ulBusinessCount));
        delete[] pBuffer;
    }while(false);
    /* release the business */
    CMduBusinessManager::instance()->releaseBusiness(businessList);
    return;
}//lint !e429

void CMduServiceTask::stopSessions()
{
    BUSINESS_LIST businessList;
    CMduBusinessManager::instance()->getAllBusiness(businessList);
    BUSINESS_LIST_ITER iter;
    CMduBusiness* pBusiness = NULL;
    CMduSession* pSession = NULL;
    for (iter = businessList.begin(); iter != businessList.end(); ++iter)
    {
        pBusiness = *iter;
        if (NULL == pBusiness)
        {
            continue;
        }

        pSession = pBusiness->getSession(pBusiness->getSendStreamID());
        if (NULL == pSession)
        {
            continue;
        }

        if (MDU_SESSION_STATUS_DISPATCHING != pSession->getStatus())
        {
            SVS_LOG((SVS_LM_INFO,"stop session[%Q] service type[%u] when regist scc success.",
                        pBusiness->getSendStreamID(), pBusiness->getPlayType()));
            //TODO:
        }
    }

    CMduBusinessManager::instance()->releaseBusiness(businessList);

    return;
}

int32_t CMduServiceTask::openAllMsgQueue()
{
    if (-1 == m_SccRecvQueue.open(MDU_MAX_QUEUE_BYTE_SIZE, MDU_MAX_QUEUE_BYTE_SIZE))
    {
        SVS_LOG((SVS_LM_WARNING,"Fail to open scc recv message queue."));
        return RET_FAIL;
    }

    if (-1 == m_MediaMsgQueue.open(MDU_MAX_QUEUE_BYTE_SIZE, MDU_MAX_QUEUE_BYTE_SIZE))
    {
        SVS_LOG((SVS_LM_WARNING,"Fail to open inner message queue."));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"service task open message queue success."));
    return RET_OK;
}

void CMduServiceTask::closeAllMsgQueue()
{
    (void)m_SccRecvQueue.close();
    (void)m_MediaMsgQueue.close();
    return;
}

int32_t CMduServiceTask::createServiceThreads()
{
    size_t *pStackSize = NULL;
    try
    {
        pStackSize = new size_t[MAX_SERVICE_THREAD];;
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,"create mdu service threads fail, thread num[%d], retcode[%d].",
                        MAX_SERVICE_THREAD,
                        errno));
        if (NULL != pStackSize)
        {
            delete[] pStackSize;
        }
        return RET_ERR_SYS_NEW;
    }

    for (int32_t i = 0; i < MAX_SERVICE_THREAD; i++)
    {
        pStackSize[i] = DEFAULT_THREAD_STACK_SIZE;
    }

    int32_t nRet = activate(THR_NEW_LWP  | THR_JOINABLE,
                        MAX_SERVICE_THREAD,
                        0,
                        ACE_DEFAULT_THREAD_PRIORITY,
                        -1,
                        0,
                        0,
                        0,
                        pStackSize,
                        0);


    delete[] pStackSize;
    pStackSize = NULL;

    if (-1 == nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"create mdu service threads fail, thread num[%d], retcode[%d].",
                        MAX_SERVICE_THREAD,
                        errno));

        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"create mdu service threds success, thread num[%d].",
                    MAX_SERVICE_THREAD));
    return RET_OK;
}

int32_t CMduServiceTask::startSysStat()
{

    if (SVS_STAT_OK != CSvsSysStat::instance().open(NULL))
    {
        SVS_LOG((SVS_LM_WARNING,"mdu service task open sys stat fail."));
        return RET_FAIL;
    }

    MDU_IP_LIST   ipList;
    ACE_INET_Addr  addr;
    uint32_t unInteranlIp = CMduConfig::instance()->getInternalMediaIp();
    CMduConfig::instance()->getExternalMediaIpList(ipList);
    ipList.push_front(unInteranlIp);

    for (MDU_IP_LIST::iterator iter = ipList.begin(); iter != ipList.end(); iter++)
    {
        addr.set((uint16_t) 0, *iter);
        if (SVS_STAT_OK != CSvsSysStat::instance().AddNetwordCard(addr.get_host_addr()))
        {
            SVS_LOG((SVS_LM_WARNING,"sys stat add media interface[%s] fail.", addr.get_host_addr()));
            return RET_FAIL;
        }

        SVS_LOG((SVS_LM_INFO,"sys stat add media interface[%s].", addr.get_host_addr()));
    }

    try
    {
        m_pLoadReportTimer = new CLoadReportTimer;
    }
    catch(...)
    {
        return RET_FAIL;
    }

    while (NULL == m_pTimerReactor)
    {
        usleep(10);
    }

    ACE_Time_Value tv((int32_t) CMduConfig::instance()->getReportPeriod(), 0);
    int32_t TimerId = m_pTimerReactor->schedule_timer(m_pLoadReportTimer, this, tv, tv);   //lint !e613
    if (-1 == TimerId)
    {
        SVS_LOG((SVS_LM_WARNING,"mdu service task alloc sys stat report timer fail."));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"mdu service task start sys stat success, stat report timer[%p].",
            m_pLoadReportTimer));
    return RET_OK;
}

int32_t CMduServiceTask::createSccConnect()
{
    try
    {
        m_pSccConnect = new CMduSccConnector();
    }
    catch(...)
    {
        m_pSccConnect = NULL;
    }

    if (NULL == m_pSccConnect)
    {
        SVS_LOG((SVS_LM_CRITICAL,"create scc connection fail, retcode[%d].", errno));
        return RET_ERR_SYS_NEW;
    }

    int32_t nRet = m_pSccConnect->openConnector(&m_SccRecvQueue);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"CMduServiceTask::createSccConnect fail, retcode[0x%x].", nRet));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"init scc connection success."));
    return RET_OK;
}


void CMduServiceTask::reactorLoopThread()const
{
    SVS_LOG((SVS_LM_INFO,"CMduServiceTask::reactorLoopThread start."));

    ACE_Dev_Poll_Reactor devPollReactor(MDU_MAX_EPOLL_SIZE, true);
    ACE_Reactor reactor(&devPollReactor);
    (void)ACE_Reactor::instance(&reactor);
    CThread_Stat_Reporter report("ace_reactor");

    ACE_Time_Value delaytime;
    while(m_bRunFlag)
    {
        delaytime.set(0, MDU_MAX_TIME_PER_THREAD);
        if (ACE_Reactor::instance()->handle_events(&delaytime) < 0)
        {
            report.ReportStat();
            continue;
        }

        report.ReportStat();
    }

    SVS_LOG((SVS_LM_CRITICAL,"CMduServiceTask::reactorLoopThread exit."));

    return;   //lint !e429
}

void CMduServiceTask::sccMessageHandleThread()
{
    SVS_LOG((SVS_LM_INFO,"CMduServiceTask::sccMessageHandleThread start."));
    ACE_Message_Block *pMsg = NULL;
    ACE_Time_Value delayTime;
    CThread_Stat_Reporter report("SccMsgHandleThread");

    while (m_bRunFlag)
    {
        delayTime = ACE_OS::gettimeofday() + ACE_Time_Value(0, MDU_MAX_QUEUE_DELAY);
        if (-1 == m_SccRecvQueue.dequeue_head(pMsg, &delayTime))
        {
            report.ReportStat();
            continue;
        }

        if (NULL != pMsg)
        {
            handleSccRecvedMessage(pMsg);
            pMsg->release();
        }
        report.ReportStat();
    }

    SVS_LOG((SVS_LM_CRITICAL,"CMduServiceTask::sccMessageHandleThread exit."));

    return;
}

/*lint -e429*/
void CMduServiceTask::mduTimerManagerThread()
{
    SVS_LOG((SVS_LM_INFO,"mdu timer manager thread start."));

    ACE_Reactor_Impl *pImpl = NULL;

    try
    {
        pImpl = new ACE_Dev_Poll_Reactor(MDU_MAX_EPOLL_SIZE, true);
    }
    catch(...)
    {
        pImpl = NULL;
        return;
    }

    try
    {
        m_pTimerReactor = new ACE_Reactor(pImpl, true);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,"create timer reactor fail."));
        return;
    }

    int32_t ret = 0;
    ACE_Time_Value delaytime;
    CThread_Stat_Reporter report("TimerManagerThread");
    while (m_bRunFlag)
    {
        delaytime.set(0, MDU_MAX_TIME_PER_THREAD);
        ret = m_pTimerReactor->handle_events(&delaytime);
        if (ret < 0)
        {
            report.ReportStat();
            continue;
        }
        report.ReportStat();
    }

    SVS_LOG((SVS_LM_INFO,"mdu timer manager thread exit."));
    return;
}

int32_t CMduServiceTask::openDeubgger()
{
    uint16_t usDebugPort = CMduConfig::instance()->getDebugPort();
    uint32_t ulAddr = inet_addr("127.0.0.1");
    ACE_INET_Addr localAddr(usDebugPort,  ACE_NTOHL( ulAddr ));

    ACE_Reactor_Impl *pImpl = NULL;

    try
    {
        pImpl = new ACE_Dev_Poll_Reactor(MDU_MAX_EPOLL_SIZE, true);
    }
    catch(...)
    {
        pImpl = NULL;
        return RET_FAIL;
    }

    try
    {
        m_pDebugReactor = new ACE_Reactor(pImpl, true);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_CRITICAL,"Create debug reactor fail."));
        return RET_FAIL;
    }

    int32_t iRet = m_debugAcceptor.open( localAddr, m_pDebugReactor);
    if ( -1 == iRet )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Open debug port fail. debug addr[%s:%d].",
            localAddr.get_host_addr(),
            localAddr.get_port_number()));
        return RET_FAIL;
    }

    return RET_OK;
}
/*lint +e429*/
void CMduServiceTask::debugThread()
{
    SVS_LOG((SVS_LM_CRITICAL,"CMduServiceTask::debugThread is running."));
    if (NULL == m_pDebugReactor)
    {
        SVS_LOG((SVS_LM_CRITICAL,"CMduServiceTask::debugThread exit, debug reactor is null."));
        return;
    }

    (void)m_pDebugReactor->owner(ACE_OS::thr_self());

    ACE_Time_Value delaytime;
    CThread_Stat_Reporter report("DebugThread");
    while(m_bRunFlag)
    {
        delaytime.set(0, MDU_MAX_TIME_PER_THREAD);
        if (m_pDebugReactor->handle_events(&delaytime) < 0)
        {
            report.ReportStat();
            continue;
        }
        report.ReportStat();
    }

    SVS_LOG((SVS_LM_CRITICAL,"CMduServiceTask::debugThread exit."));

    return;

}


void CMduServiceTask::handleSccRecvedMessage(const ACE_Message_Block *pMsg)const
{
    if ((NULL == pMsg) || (sizeof(SVS_MSG_HEADER) > pMsg->length()))
    {
        return;
    }

    SVS_MSG_HEADER *pHeader = (SVS_MSG_HEADER*)(void*)pMsg->rd_ptr();

    SVS_LOG((SVS_LM_INFO,"start process scc message: msgtype[0x%x] transno[%x]",
                    pHeader->MsgType,
                    pHeader->TransactionNo));

    CMduSvsMessage *pSvsMessage = NULL;
    int32_t nRet = CMduMsgFactory::instance()->createSvsMsg((const char*)pMsg->rd_ptr(),
                                                        pMsg->length(),
                                                        pSvsMessage);
    if ((RET_OK != nRet) || (NULL == pSvsMessage))
    {
        CMduMsgFactory::instance()->destroySvsMsg(pSvsMessage);
        SVS_LOG((SVS_LM_INFO,"parse scc message fail: msgtype[0x%x] transno[%x]",
                             pHeader->MsgType,
                             pHeader->TransactionNo));
        return;
    }


    pSvsMessage->dump();

    nRet = pSvsMessage->handleMessage();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_INFO,"handle scc message fail: msgtype[0x%x] transno[%x]",
                         pHeader->MsgType,
                         pHeader->TransactionNo));
    }

    CMduMsgFactory::instance()->destroySvsMsg(pSvsMessage);

    SVS_LOG((SVS_LM_INFO,"end process scc message: msgtype[0x%x] transno[%x]",
                     pHeader->MsgType,
                     pHeader->TransactionNo));

    return;
}

void CMduServiceTask::innerMsgHandleThread()
{
    SVS_LOG((SVS_LM_INFO,"CMduServiceTask::innerMsgHandleThread start."));
    ACE_Message_Block *pMsg = NULL;
    ACE_Time_Value delayTime;
    CThread_Stat_Reporter report("InnerMsgHandleThread");

    while (m_bRunFlag)
    {
        delayTime = ACE_OS::gettimeofday() + ACE_Time_Value(0, MDU_MAX_QUEUE_DELAY);
        if (-1 == m_MediaMsgQueue.dequeue_head(pMsg, &delayTime))
        {
            report.ReportStat();
            continue;
        }

        if (NULL != pMsg)
        {
            handleInnerMessage(pMsg);
            CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
        }
        report.ReportStat();
    }

    SVS_LOG((SVS_LM_CRITICAL,"CMduServiceTask::innerMsgHandleThread exit."));

    return;
}

uint32_t CMduServiceTask::getThreadIndex()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ThreadIndexMutex);
    return m_unThreadIndex++;
}

void CMduServiceTask::handleInnerMessage(const ACE_Message_Block *pMsg) const
{
    if (NULL == pMsg)
    {
        return;
    }

    MDU_INNER_MSG *pInnerMsg = (MDU_INNER_MSG*)(void*)pMsg->rd_ptr();

    SVS_LOG((SVS_LM_DEBUG,"service task start handle inner message: type[%d].",
            pInnerMsg->usMsgType));

    CMduBusiness *pBusiness = CMduBusinessManager::instance()->findBusiness(pInnerMsg->ullStreamID);
    if (NULL != pBusiness)
    {
        pBusiness->handleInnerMsg(*pInnerMsg, pMsg->length());
        CMduBusinessManager::instance()->releaseBusiness(pBusiness);
        return;
    }

    CMduSession *pSession = CMduSessionFactory::instance()->findSession(pInnerMsg->ullStreamID);
    if (NULL != pSession)
    {
        CMduRtpSession peerSession;
        (void)pSession->handleInnerMessage(*pInnerMsg, pMsg->length(), peerSession);
        CMduSessionFactory::instance()->releaseSession(pSession);
        return;
    }

    SVS_LOG((SVS_LM_WARNING,"handle inner message fail, can't find business or session by id[%Q].",
                     pInnerMsg->ullStreamID));
    return;
}


