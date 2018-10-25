
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include <vms/vms.h>
#include "svs_adapter_common.h"

#include "svs_adapter_business.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_exchange.h"
#include "svs_adapter_business_manager.h"
#include "svs_adapter_service_task.h"
#include "svs_vms_msg_factory.h"
#include "svs_adapter_service_task.h"
#include "svs_adapter_rtp_to_ps_Processor.h"
#include "svs_adapter_def.h"
#include "svs_adapter_sdp.h"

CMduBusiness::CMduBusiness()
{
    m_nRefCount         = 1;
    memset(m_szContentID, 0x0, CONTENT_ID_LEN + 1);
    m_pRecvSession      = NULL;
    m_pSendSession      = NULL;
    m_enPlayType        = PLAY_TYPE_MAX;

    m_pRecvProcessor    = NULL;
    m_pSendProcessor    = NULL;
    m_OutputRate        = 0;
    m_ullFluxSize       = 0;
    m_unStartTime       = 0;
    m_unEndTime         = 0;
}

CMduBusiness::~CMduBusiness()
{
    try
    {
        (void)stop();
    }
    catch (...)
    {}

    m_pRecvSession      = NULL;
    m_pSendSession      = NULL;
    m_pRecvProcessor    = NULL;
    m_pSendProcessor    = NULL;
}

int32_t CMduBusiness::init(uint64_svs recvSessionId, uint64_svs sendSessionId, PLAY_TYPE enPlayType)
{
    uint64_svs unRecvStream = recvSessionId;
    uint64_svs unSendStream = sendSessionId;
    m_pRecvSession = CMduSessionFactory::instance()->findSession(unRecvStream);
    if (NULL == m_pRecvSession)
    {
        return RET_FAIL;
    }

    m_pSendSession = CMduSessionFactory::instance()->findSession(unSendStream);
    if (NULL == m_pSendSession)
    {
        return RET_FAIL;
    }

    memcpy(m_szContentID, m_pRecvSession->getContentID(), CONTENT_ID_LEN);

    m_enPlayType = enPlayType;


    SVS_LOG((SVS_LM_INFO,"init mdu business, recv session[%Q] send session[%Q] service type[%d].",
                    m_pRecvSession->getStreamId(),
                    m_pSendSession->getStreamId(),
                    m_enPlayType));

    return RET_OK;
}

int32_t CMduBusiness::start()
{
    if ((NULL == m_pRecvSession) || (NULL == m_pSendSession))
    {
        return RET_FAIL;
    }

    if ((MDU_SESSION_STATUS_WAIT_CHANNEL_REDAY <= m_pRecvSession->getStatus())
        && (MDU_SESSION_STATUS_WAIT_CHANNEL_REDAY <= m_pSendSession->getStatus()))
    {
        if (RET_OK != createMediaProcessor())
        {
            return RET_FAIL;
        }
    }

    SVS_LOG((SVS_LM_INFO,"start business success, recv session[%Q], send session[%Q].",
                    getRecvStreamID(),  getSendStreamID()));
    return RET_OK;
}

int32_t CMduBusiness::stop()
{

    destroyMediaProcessor();

    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_Mutex, RET_FAIL);
        if (NULL != m_pRecvSession)
        {
            CMduSessionFactory::instance()->releaseSession(m_pRecvSession);
            m_pRecvSession = NULL;
        }

        if (NULL != m_pSendSession)
        {
            CMduSessionFactory::instance()->releaseSession(m_pSendSession);
            m_pSendSession = NULL;
        }
    }
    return RET_OK;
}

void CMduBusiness::handleInnerMsg(const MDU_INNER_MSG& innerMsg, uint32_t unMsgSize)
{
    if ((NULL == m_pRecvSession) || (NULL == m_pSendSession))
    {
        SVS_LOG((SVS_LM_WARNING,"business handle session[%Q] inner message fail, session invalid.",
                        innerMsg.ullStreamID));
        return;
    }

    if (getRecvStreamID() == innerMsg.ullStreamID)
    {
        (void)m_pRecvSession->handleInnerMessage(innerMsg, unMsgSize, *m_pSendSession);
        return;
    }

    if (getSendStreamID() == innerMsg.ullStreamID)
    {
        (void)m_pSendSession->handleInnerMessage(innerMsg, unMsgSize, *m_pRecvSession);
        return;
    }

    SVS_LOG((SVS_LM_WARNING,"business handle inner message fail, stream id[%Q] not match.",
                    innerMsg.ullStreamID));
    return;
}

const char* CMduBusiness::getContentID() const
{
    return m_szContentID;
}

uint64_svs CMduBusiness::getRecvStreamID() const
{
    if (NULL == m_pRecvSession)
    {
        return 0;
    }

    return m_pRecvSession->getStreamId();
}

uint64_svs CMduBusiness::getSendStreamID() const
{
    if (NULL == m_pSendSession)
    {
        return 0;
    }

    return m_pSendSession->getStreamId();
}


CMduSession* CMduBusiness::getSession(uint64_svs unStreamID) const
{
    if (unStreamID == getRecvStreamID())
    {
        return m_pRecvSession;
    }
    else
    {
        if (unStreamID == getSendStreamID())
        {
            return m_pSendSession;
        }
    }

    return NULL;
}//lint !e1763

void CMduBusiness::statFluxInfo()
{
    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_Mutex);
    if (NULL == m_pRecvProcessor)
    {
        SVS_LOG((SVS_LM_INFO,"get business flux report fail, processor invalid, "
                        "recv stream[%Q] send stream[%Q]",
                        getRecvStreamID(),
                        getSendStreamID()));
        return;
    }

    m_ullFluxSize = m_pRecvProcessor->getFlux(m_unStartTime, m_unEndTime);

    if (m_unEndTime > m_unStartTime)
    {
        uint32_t Interval = m_unEndTime - m_unStartTime;
        m_OutputRate = ((uint32_t)m_ullFluxSize * BYTE_NUM) / (KILO * Interval);
    }

    return;
}

uint32_t CMduBusiness::getInputRate()
{
    uint32_t rate = 0;
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_Mutex, rate);
    if (NULL != m_pRecvProcessor)
    {
        rate = m_pRecvProcessor->getInputRate();
    }

    return rate;
}
uint32_t CMduBusiness::getOutputRate()const
{
    return m_OutputRate;
}
uint64_t CMduBusiness::getFluxszie()const
{
    return m_ullFluxSize;
}

uint32_t CMduBusiness::getStarttime()const
{
    return m_unStartTime;
}

uint32_t CMduBusiness::getEndtime()
{
    return m_unEndTime;
}

PLAY_TYPE CMduBusiness::getPlayType()const
{
    return m_enPlayType;
}

int32_t CMduBusiness::addReference()
{
    return ++m_nRefCount;
}

int32_t CMduBusiness::decReference()
{
    return --m_nRefCount;
}

/*lint -e423*/
void CMduBusiness::createDirectProcessor()
{
    if (NULL == m_pSendSession || NULL == m_pRecvSession)
    {
        SVS_LOG((SVS_LM_WARNING,"mdu business create realtime processor fail, send session or recv session is null."));
        return;
    }

    m_pRecvProcessor = new CDirectProcessor;

    SVS_LOG((SVS_LM_INFO,"create direct processor, recv session[%Q] send session[%Q].",
                    getRecvStreamID(), getSendStreamID()));
    return;
}


int32_t CMduBusiness::createMediaProcessor()
{
    if (NULL == m_pSendSession)
    {
        SVS_LOG((SVS_LM_WARNING,"mdu business create media processor fail, session is null."));
        return RET_FAIL;
    }

    uint32_t unServiceType = m_pSendSession->getPlayType();

    if (NULL != m_pRecvProcessor)
    {
        delete m_pRecvProcessor;
        m_pRecvProcessor = NULL;
    }

    switch (unServiceType)
    {
        case PLAY_TYPE_LIVE:
        case PLAY_TYPE_FRONT_RECORD:
            createDirectProcessor();
            break;
        default:
            SVS_LOG((SVS_LM_WARNING,"create processor fail, service type[%d] invalid, "
                            "recv session[%Q] send session[%Q].",
                            unServiceType,
                            getRecvStreamID(), getSendStreamID()));
            return RET_FAIL;
    }

    if (RET_OK != registMediaProcessor())
    {
        delete m_pRecvProcessor;
        m_pRecvProcessor = NULL;

        delete m_pSendProcessor;
        m_pSendProcessor = NULL;

        return RET_FAIL;
    }

    return RET_OK;
}
/*lint +e423*/

int32_t CMduBusiness::registMediaProcessor()
{
    uint64_svs   recvStreamId = getRecvStreamID();
    uint64_svs   sendStreamId = getSendStreamID();

    if (NULL == m_pRecvProcessor)
    {
        SVS_LOG((SVS_LM_WARNING,
            "register media processor fail, recv processor is null, recv session[%Q] send session[%Q].",
            recvStreamId,
            sendStreamId));
        return RET_FAIL;
    }

    if (NULL == m_pSendSession || NULL == m_pRecvSession)
    {
        SVS_LOG((SVS_LM_WARNING,
            "register media processor fail, session is null, recv session[%Q] send session[%Q].",
            recvStreamId,
            sendStreamId));
        return RET_FAIL;
    }

    uint32_t unRecvTransType = m_pRecvSession->getMediaTransType();
    uint32_t unSendTransType = m_pSendSession->getMediaTransType();

    if (RET_OK != m_pRecvProcessor->Init(sendStreamId))
    {
        SVS_LOG((SVS_LM_WARNING,
            "init recv processor fail recv session[%Q] send session[%Q].",
            recvStreamId,
            sendStreamId));
        return RET_FAIL;
    }

    if (RET_OK != CMduMediaExchange::instance()->addMediaProcessor(recvStreamId,
            m_pRecvProcessor, unRecvTransType, unSendTransType))
    {
        SVS_LOG((SVS_LM_WARNING,"add recv processor to media exchange fail, "
                        "recv session[%Q] send session[%Q].",
                        recvStreamId, sendStreamId));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"business create processor success "
                    "recv from session[%Q] send to session[%Q].",
                    recvStreamId, sendStreamId));
    return RET_OK;
}

void CMduBusiness::destroyMediaProcessor()
{
    uint64_svs   recvStreamId = getRecvStreamID();

    if (NULL == m_pRecvSession)
    {
        SVS_LOG((SVS_LM_WARNING,
            "destroy media processor fail, Recv session is null, recv session[%Q].",
            recvStreamId));
        return;
    }
    uint32_t unTransType = m_pRecvSession->getMediaTransType();

    if (NULL != m_pRecvProcessor)
    {
        ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_Mutex);
        (void)CMduMediaExchange::instance()->delMediaProcessor(getRecvStreamID(), m_pRecvProcessor, unTransType);
        m_pRecvProcessor = NULL;
    }

    if (NULL != m_pSendProcessor)
    {
        (void) CMduMediaExchange::instance()->delMediaProcessor(getSendStreamID(), m_pSendProcessor, unTransType);
        m_pSendProcessor = NULL;
    }
}

void CMduBusiness::Dump(ACE_HANDLE handle)
{
    if (NULL == m_pRecvSession)
    {
        return;
    }

    char buf[KILO + 1] = {0};


    snprintf(buf,
             KILO,
             "Business Info:\n"
             "\tRecv session[%lld] ContentID[%s] peer type[%s] client num[%d] "
             "local addr[%s:%d] peer addr[%s:%d] trans protocol[%s] \n",
             m_pRecvSession->getStreamId(),
             m_pRecvSession->getContentID(),
             CSessionInfo::instance().getPeerType(m_pRecvSession->getPeerType()),
             CMduBusinessManager::instance()->getBusinessCount(m_pRecvSession->getStreamId()),
             m_pRecvSession->getVideoAddr().get_host_addr(),
             m_pRecvSession->getVideoAddr().get_port_number(),
             m_pRecvSession->getPeerAddr().get_host_addr(),
             m_pRecvSession->getPeerAddr().get_port_number(),
             CSessionInfo::instance().getTransProtocol(m_pRecvSession->getTransProtocol()));

    (void)ACE::send(handle, buf, strlen(buf));

    if (NULL != m_pRecvProcessor)
    {
        m_pRecvProcessor->Dump(handle);
    }

    return;
}

void CMduBusiness::DumpSdp(uint64_svs ullStreamId, ACE_HANDLE handle)
{
    if (NULL == m_pRecvSession)
    {
        return;
    }

    std::string  SdpInfo = "";
    char buf[KILO + 1] = {0};

    m_pRecvSession->getSdpInfo(SdpInfo);

    snprintf(buf,KILO,"\tSDPInfon:\n"
                      "\t\t%s\n",
                        SdpInfo.c_str());
    (void)ACE::send(handle, buf, strlen(buf));
    return;
}


