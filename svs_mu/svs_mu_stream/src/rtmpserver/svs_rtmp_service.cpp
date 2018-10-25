/*
 * StreamRtspService.cpp
 *
 *  Created on: 2016-5-12
 *      Author:
 */
#include <string.h>
#include "svs_adapter_svs_retcode.h"
#include <vms/vms.h>
#include "svs_rtmp_service.h"
#include "svs_adapter_config.h"
#include "svs_vms_media_setup_resp.h"
#include "svs_vms_media_play_resp.h"


int32_t CRtmpSessionCheckTimer::handle_timeout(const ACE_Time_Value& tv, const void* arg)
{
    CStreamRtmpService::instance().checkSession();
    return 0;
}

CStreamRtmpService::CStreamRtmpService()
{
    m_unLocalSessionIndex = 0;
    m_ulCheckTimerId      = 0;
}

CStreamRtmpService::~CStreamRtmpService()
{
}

int32_t CStreamRtmpService::open()
{
    int32_t nRet = RET_OK;


    uint16_t usPort = CStreamConfig::instance()->getRtmpServerPort();
    if(0 == usPort)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtmp server end,the port configure zore."));
        return RET_OK;
    }
    m_RtmpAddr.set(usPort);
    nRet = m_RtmpAcceptor.open(m_RtmpAddr, 1);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtmp server port[%s:%d] fail, errno[%d].",
                m_RtmpAddr.get_host_addr(),
                m_RtmpAddr.get_port_number(),
                errno));
        return RET_FAIL;
    }

    if (0 != m_RtmpAcceptor.enable(ACE_NONBLOCK))
    {
        SVS_LOG((SVS_LM_WARNING,"set rtmp server port[%s:%d] NONBLOCK falg fail, errno[%d].",
                m_RtmpAddr.get_host_addr(),
                m_RtmpAddr.get_port_number(),
                errno));

        return RET_FAIL;
    }

    ACE_Reactor *pReactor = ACE_Reactor::instance();
    if (!pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtmp server fail, can't find reactor instance."));
        return RET_FAIL;
    }

    nRet = pReactor->register_handler(m_RtmpAcceptor.get_handle(),
                                      this,
                                      ACE_Event_Handler::ACCEPT_MASK);

    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtmp server fail, register accept mask fail[%d].",
                        ACE_OS::last_error()));
        return RET_FAIL;
    }

    ACE_Time_Value tv(STREAM_STATUS_CHECK_INTERVAL, 0);
    m_ulCheckTimerId = pReactor->schedule_timer(&m_SessionCheckTimer, this, tv, tv);
    if (-1 == m_ulCheckTimerId)
    {
        SVS_LOG((SVS_LM_WARNING,"start session status check timer fail."));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"open rtmp server ip[%s] port[%d] success. check timer id[%d]",
                    m_RtmpAddr.get_host_addr(),
                    m_RtmpAddr.get_port_number(),
                    m_ulCheckTimerId));
    return RET_OK;
}

void CStreamRtmpService::close() const
{
     ACE_Reactor *pReactor = ACE_Reactor::instance();
    if (!pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"close rtmp live stream server fail, can't find reactor instance."));
        return;
    }
    pReactor->cancel_timer(m_ulCheckTimerId);
    ACE_OS::shutdown(m_RtmpAcceptor.get_handle(), SHUT_RDWR);

    SVS_LOG((SVS_LM_INFO,"success to close rtmp server."));
    return;
}

int32_t CStreamRtmpService::handleSvsMessage(CStreamSvsMessage &message)
{
    uint32_t ulLocalIndex = 0;
    if (SVS_MSG_TYPE_STREAM_SESSION_SETUP_RESP == message.getMsgType())
    {
        CStreamMediaSetupResp* pSetupResp = (CStreamMediaSetupResp*)&message;
        ulLocalIndex = pSetupResp->getLocalIndex();
    }
    else if(SVS_MSG_TYPE_STREAM_SESSION_PLAY_RESP == message.getMsgType())
    {
        CStreamMediaPlayResp* pPlayResp = (CStreamMediaPlayResp*)&message;
        ulLocalIndex = pPlayResp->getLocalIndex();
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp service handle svs message fail, MsgType[0x%x] invalid.",
                        message.getMsgType()));

        return RET_FAIL;
    }

    ACE_Guard<ACE_Thread_Mutex> locker(m_MapMutex);
    RTMP_SESSION_MAP_ITER iter = m_RtmpSessionMap.find(ulLocalIndex);
    if (m_RtmpSessionMap.end() == iter)
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp service handle svs message fail, can't find rtmp push session[%u].",
                         ulLocalIndex));
        return RET_FAIL;
    }

    CStreamRtmpPushSession *pSession = iter->second;
    if (!pSession)
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp service handle svs message fail, rtmp push session[%u] is null.",
                         ulLocalIndex));
        return RET_FAIL;
    }

    return pSession->handleSvsMessage(message);
}


int32_t CStreamRtmpService::handle_input(ACE_HANDLE handle)
{

    ACE_INET_Addr addr;
    ACE_SOCK_Stream stream;
    int32_t nRet = m_RtmpAcceptor.accept(stream, &addr);
    if (0 != nRet)
    {
        int32_t iErrCode = ACE_OS::last_error();
        if ((EINTR != iErrCode) && (EWOULDBLOCK != iErrCode) && (EAGAIN
                != iErrCode))
        {
            SVS_LOG((SVS_LM_WARNING,"rtmp server [%s:%d] accept connection fail, close handle, errno[%d].",
                    m_RtmpAddr.get_host_addr(),
                    m_RtmpAddr.get_port_number(),
                    iErrCode));
            return -1;
        }

        SVS_LOG((SVS_LM_WARNING,"rtmp server[%s:%d] accept connection fail, wait retry, errno[%d].",
                m_RtmpAddr.get_host_addr(),
                m_RtmpAddr.get_port_number(),
                iErrCode));
        return 0;
    }

    SVS_LOG((SVS_LM_INFO,"rtmp server accepted new rtsp connect[%s:%d].",
                    addr.get_host_addr(),
                    addr.get_port_number()));


    CStreamRtmpPushSession *pSession = NULL;
    try
    {
        pSession = new CStreamRtmpPushSession();
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp server create new push session fail."));
        delete pSession;
        stream.close();
        return 0;
    }

    ACE_INET_Addr localAddr;
    stream.get_local_addr(localAddr);
    pSession->setHandle(stream.get_handle(), localAddr);
    if (RET_OK != pSession->open(getLocalSessionIndex(), addr))
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp server create new push session fail."));
        delete pSession;
        stream.close();
        return 0;
    }

    {
        ACE_Guard<ACE_Thread_Mutex> locker(m_MapMutex);
        m_RtmpSessionMap[pSession->getSessionIndex()] = pSession;
    }

    return 0;
}


int32_t CStreamRtmpService::handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask /*close_mask*/)
{
    ACE_Reactor *pReactor = ACE_Reactor::instance();
    if (!pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"close rtmp server fail, can't find reactor instance."));
        return -1;
    }

    int32_t nRet = pReactor->remove_handler(m_RtmpAcceptor.get_handle(),
                                        ACE_Event_Handler::ACCEPT_MASK
                                        | ACE_Event_Handler::DONT_CALL);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"rtmp server handle close fail, unregist handle fail."));
        return -1;
    }
    m_RtmpAcceptor.close();

    SVS_LOG((SVS_LM_WARNING,"rtmp server handle close success."));
    return 0;
}

void CStreamRtmpService::removeSession(CStreamRtmpPushSession* pSession)
{
    if (NULL == pSession)
    {
        return;
    }

    ACE_Guard<ACE_Thread_Mutex> locker(m_MapMutex);
    RTMP_SESSION_MAP_ITER iter = m_RtmpSessionMap.find(pSession->getSessionIndex());
    if (m_RtmpSessionMap.end() != iter)
    {
        m_RtmpSessionMap.erase(iter);
    }

    SVS_LOG((SVS_LM_INFO,"rtmp server remove rtsp session[%u] success.",
                    pSession->getSessionIndex()));
    delete pSession;

    return;
}

void CStreamRtmpService::checkSession()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_MapMutex);
    RTMP_SESSION_MAP_ITER iter;
    CStreamRtmpPushSession* pSession = NULL;
    for (iter = m_RtmpSessionMap.begin(); iter != m_RtmpSessionMap.end();)
    {
        pSession = iter->second;
        if (pSession->check() != RET_OK)
        {
            m_RtmpSessionMap.erase(iter++);

            SVS_LOG((SVS_LM_INFO,"rtmp server remove session success. session index[%u]",
                             pSession->getSessionIndex()));
            delete pSession;
            continue;
        }

        ++iter;
    }

    SVS_LOG((SVS_LM_INFO,"rtmp server check session success. now rtmp session num[%d]",
        m_RtmpSessionMap.size()));
}

uint32_t CStreamRtmpService::getLocalSessionIndex()
{
    return ++m_unLocalSessionIndex;
}

