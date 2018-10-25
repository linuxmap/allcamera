/*
 * MduRtspService.cpp
 *
 *  Created on: 2016-5-12
 *      Author:
 */
#include <string.h>
#include "svs_adapter_svs_retcode.h"
#include <vms/vms.h>
#include "svs_rtsp_service.h"
#include "svs_adapter_config.h"
#include "svs_rtsp_protocol.h"
#include "svs_vms_media_setup_resp.h"
#include "svs_vms_media_play_resp.h"



int32_t CRtspSessionCheckTimer::handle_timeout(const ACE_Time_Value& tv, const void* arg)
{
    CMduRtspService::instance().checkSession();
    return 0;
}

CMduRtspService::CMduRtspService()
{
    m_unLocalSessionIndex = 0;
    m_ulCheckTimerId      = 0;
}

CMduRtspService::~CMduRtspService()
{
}

int32_t CMduRtspService::open()
{
    int32_t nRet = RET_OK;

    m_RtspAddr.set(CMduConfig::instance()->getRtspServerPort());
    nRet = m_RtspAcceptor.open(m_RtspAddr, 1);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtsp server port[%s:%d] fail, errno[%d].",
                m_RtspAddr.get_host_addr(),
                m_RtspAddr.get_port_number(),
                errno));
        return RET_FAIL;
    }

    if (0 != m_RtspAcceptor.enable(ACE_NONBLOCK))
    {
        SVS_LOG((SVS_LM_WARNING,"set rtsp server port[%s:%d] NONBLOCK falg fail, errno[%d].",
                m_RtspAddr.get_host_addr(),
                m_RtspAddr.get_port_number(),
                errno));

        return RET_FAIL;
    }

    ACE_Reactor *pReactor = ACE_Reactor::instance();
    if (!pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtsp server fail, can't find reactor instance."));
        return RET_FAIL;
    }

    nRet = pReactor->register_handler(m_RtspAcceptor.get_handle(),
                                      this,
                                      ACE_Event_Handler::ACCEPT_MASK);

    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtsp server fail, register accept mask fail[%d].",
                        ACE_OS::last_error()));
        return RET_FAIL;
    }

    ACE_Time_Value tv(MDU_STATUS_CHECK_INTERVAL, 0);
    m_ulCheckTimerId = pReactor->schedule_timer(&m_SessionCheckTimer, this, tv, tv);
    if (-1 == m_ulCheckTimerId)
    {
        SVS_LOG((SVS_LM_WARNING,"start session status check timer fail."));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"open rtsp server ip[%s] port[%d] success. check timer id[%d]",
                    m_RtspAddr.get_host_addr(),
                    m_RtspAddr.get_port_number(),
                    m_ulCheckTimerId));
    return RET_OK;
}

void CMduRtspService::close() const
{
     ACE_Reactor *pReactor = ACE_Reactor::instance();
    if (!pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"close http live stream server fail, can't find reactor instance."));
        return;
    }
    pReactor->cancel_timer(m_ulCheckTimerId);
    ACE_OS::shutdown(m_RtspAcceptor.get_handle(), SHUT_RDWR);

    SVS_LOG((SVS_LM_INFO,"success to close rtsp server."));
    return;
}

int32_t CMduRtspService::handleSvsMessage(CMduSvsMessage &message)
{
    uint32_t ulLocalIndex = 0;
    if (SVS_MSG_TYPE_MDU_SESSION_SETUP_RESP == message.getMsgType())
    {
        CMduMediaSetupResp* pSetupResp = (CMduMediaSetupResp*)&message;
        ulLocalIndex = pSetupResp->getLocalIndex();
    }
    else if(SVS_MSG_TYPE_MDU_SESSION_PLAY_RESP == message.getMsgType())
    {
        CMduMediaPlayResp* pPlayResp = (CMduMediaPlayResp*)&message;
        ulLocalIndex = pPlayResp->getLocalIndex();
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp service handle svs message fail, MsgType[0x%x] invalid.",
                        message.getMsgType()));

        return RET_FAIL;
    }

    ACE_Guard<ACE_Thread_Mutex> locker(m_MapMutex);
    RTSP_SESSION_MAP_ITER iter = m_RtspSessionMap.find(ulLocalIndex);
    if (m_RtspSessionMap.end() == iter)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp service handle svs message fail, can't find rtsp push session[%u].",
                         ulLocalIndex));
        return RET_FAIL;
    }

    CMduRtspPushSession *pSession = iter->second;
    if (!pSession)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp service handle svs message fail, rtsp push session[%u] is null.",
                         ulLocalIndex));
        return RET_FAIL;
    }

    return pSession->handleSvsMessage(message);
}


int32_t CMduRtspService::handle_input(ACE_HANDLE handle)
{

    ACE_INET_Addr addr;
    ACE_SOCK_Stream stream;
    int32_t nRet = m_RtspAcceptor.accept(stream, &addr);
    if (0 != nRet)
    {
        int32_t iErrCode = ACE_OS::last_error();
        if ((EINTR != iErrCode) && (EWOULDBLOCK != iErrCode) && (EAGAIN
                != iErrCode))
        {
            SVS_LOG((SVS_LM_WARNING,"rtsp server [%s:%d] accept connection fail, close handle, errno[%d].",
                    m_RtspAddr.get_host_addr(),
                    m_RtspAddr.get_port_number(),
                    iErrCode));
            return -1;
        }

        SVS_LOG((SVS_LM_WARNING,"rtsp server[%s:%d] accept connection fail, wait retry, errno[%d].",
                m_RtspAddr.get_host_addr(),
                m_RtspAddr.get_port_number(),
                iErrCode));
        return 0;
    }

    SVS_LOG((SVS_LM_INFO,"rtsp server accepted new rtsp connect[%s:%d].",
                    addr.get_host_addr(),
                    addr.get_port_number()));


    CMduRtspPushSession *pSession = NULL;
    try
    {
        pSession = new CMduRtspPushSession();
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp server create new push session fail."));
        delete pSession;
        stream.close();
        return 0;
    }

    ACE_INET_Addr localAddr;
    stream.get_local_addr(localAddr);
    pSession->setHandle(stream.get_handle(), localAddr);
    if (RET_OK != pSession->open(getLocalSessionIndex(), addr))
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp server create new push session fail."));
        delete pSession;
        stream.close();
        return 0;
    }

    {
        ACE_Guard<ACE_Thread_Mutex> locker(m_MapMutex);
        m_RtspSessionMap[pSession->getSessionIndex()] = pSession;
    }

    return 0;
}


int32_t CMduRtspService::handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask /*close_mask*/)
{
    ACE_Reactor *pReactor = ACE_Reactor::instance();
    if (!pReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"close rtsp server fail, can't find reactor instance."));
        return -1;
    }

    int32_t nRet = pReactor->remove_handler(m_RtspAcceptor.get_handle(),
                                        ACE_Event_Handler::ACCEPT_MASK
                                        | ACE_Event_Handler::DONT_CALL);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"rtsp server handle close fail, unregist handle fail."));
        return -1;
    }
    m_RtspAcceptor.close();

    SVS_LOG((SVS_LM_WARNING,"rtsp server handle close success."));
    return 0;
}

void CMduRtspService::removeSession(CMduRtspPushSession* pSession)
{
    if (NULL == pSession)
    {
        return;
    }

    ACE_Guard<ACE_Thread_Mutex> locker(m_MapMutex);
    RTSP_SESSION_MAP_ITER iter = m_RtspSessionMap.find(pSession->getSessionIndex());
    if (m_RtspSessionMap.end() != iter)
    {
        m_RtspSessionMap.erase(iter);
    }

    SVS_LOG((SVS_LM_INFO,"rtsp server remove rtsp session[%u] success.",
                    pSession->getSessionIndex()));
    delete pSession;

    return;
}

void CMduRtspService::checkSession()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_MapMutex);
    RTSP_SESSION_MAP_ITER iter;
    CMduRtspPushSession* pSession = NULL;
    for (iter = m_RtspSessionMap.begin(); iter != m_RtspSessionMap.end();)
    {
        pSession = iter->second;
        if (pSession->check() != RET_OK)
        {
            m_RtspSessionMap.erase(iter++);

            SVS_LOG((SVS_LM_INFO,"rtsp server remove session success. session index[%u]",
                             pSession->getSessionIndex()));
            delete pSession;
            continue;
        }

        ++iter;
    }

    SVS_LOG((SVS_LM_INFO,"rtsp server check session success. now rtsp session num[%d]",
        m_RtspSessionMap.size()));
}

uint32_t CMduRtspService::getLocalSessionIndex()
{
    return ++m_unLocalSessionIndex;
}

