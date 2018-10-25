#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_time.h"

#include "svs_adapter_tcp_handle.h"
#include "svs_adapter_net_conn_manager.h"
#include "svs_adapter_tcp_connect.h"

CTcpHandle::CTcpHandle()
{
}

CTcpHandle::~CTcpHandle()
{
    try
    {
        this->closeHandle();
    }
    catch (...)
    {
    }
}

int32_t CTcpHandle::openHandle(const ACE_INET_Addr &localAddr)
{
    if (NETWORK_HANDLE_STATUS_INIT != getStatus())
    {
        return RET_ERR_STATUS_ABNORMAL;
    }

    setLocalAddr(localAddr);

    int32_t nRet = m_TcpAcceptor.open(getLocalAddr(), 1);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"open tcp handle[%s:%d] fail, errno[%d].",
                         localAddr.get_host_addr(),
                         localAddr.get_port_number(),
                         errno));
        return RET_FAIL;
    }

    if (0 != m_TcpAcceptor.enable(ACE_NONBLOCK))   //����Ϊ������I/O
    {
        SVS_LOG((SVS_LM_WARNING,"set tcp handle[%s:%d] NONBLOCK falg fail, errno[%d].",
                localAddr.get_host_addr(),
                localAddr.get_port_number(),
                errno));

        return -1;
    }

    // �����ӹ�����ע���¼�
    nRet = CNetConnManager::instance()->registHandle(this, ACE_Event_Handler::ACCEPT_MASK);
    if (RET_OK != nRet)
    {
        return RET_FAIL;
    }
    errno = 0;
    setStatus(NETWORK_HANDLE_STATUS_OPEN);
    SVS_LOG((SVS_LM_DEBUG,"tcp handle ip[%s] port[%d] open success.",
                    getLocalAddr().get_host_addr(),
                    getLocalAddr().get_port_number()));
    return RET_OK;
}

void CTcpHandle::closeHandle()
{
    (void)handle_close(m_TcpAcceptor.get_handle(), 0);
    return;
}

int32_t CTcpHandle::startHandle(uint64_t ullStreamID, const ACE_INET_Addr &/*remoteAddr*/)
{
    if ((NETWORK_HANDLE_STATUS_OPEN != getStatus())
         && (NETWORK_HANDLE_STATUS_START != getStatus()))
    {
        SVS_LOG((SVS_LM_WARNING,"start stream[%Q] tcp handle[%s:%d] fail, status[%d] abnormal.",
                    ullStreamID,
                    getLocalAddr().get_host_addr(),
                    getLocalAddr().get_port_number(),
                    getStatus()));

        return RET_ERR_STATUS_ABNORMAL;
    }

    ACE_Write_Guard<ACE_RW_Thread_Mutex> locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() != iter)
    {
        SVS_LOG((SVS_LM_WARNING,"start stream[%Q] tcp handle[%s:%d] fail, stream id already exist.",
                    ullStreamID,
                    getLocalAddr().get_host_addr(),
                    getLocalAddr().get_port_number()));
        return RET_FAIL;
    }

    TCP_CONNECT_INFO info;
    info.StartTime  = SVS_GetSecondTime();
    info.TcpConnect = NULL;
    m_ConnectMap[ullStreamID] = info;

    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        setStatus( NETWORK_HANDLE_STATUS_START);
    }

    SVS_LOG((SVS_LM_INFO,"start stream[%Q] tcp handle[%s:%d], all stream count[%d].",
            ullStreamID,
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number(),
            m_ConnectMap.size()));

    return RET_OK;
}

int32_t CTcpHandle::stopHandle(uint64_t ullStreamID)
{
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        SVS_LOG((SVS_LM_INFO,"stop stream[%Q] tcp handle[%s:%d] fail, status is not open.",
                    ullStreamID,
                    getLocalAddr().get_host_addr(),
                    getLocalAddr().get_port_number()));
        return RET_ERR_STATUS_ABNORMAL;
    }

    ACE_Write_Guard<ACE_RW_Thread_Mutex> locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() == iter)
    {
        SVS_LOG((SVS_LM_INFO,"stop stream[%Q] tcp handle[%s:%d] fail, can't find connect.",
                ullStreamID,
                getLocalAddr().get_host_addr(),
                getLocalAddr().get_port_number()));

        return RET_FAIL;
    }

    CTcpConnect *pConnect = iter->second.TcpConnect;
    if (NULL == pConnect)
    {
        SVS_LOG((SVS_LM_INFO,"tcp handle[%d] stop handle: stream[%Q]'s connect not created.",
                         getLocalAddr().get_port_number(),
                         ullStreamID));
    }
    else
    {
        (void)pConnect->close();
    }

    m_ConnectMap.erase(iter);

    if (0 == m_ConnectMap.size())
    {
        setStatus( NETWORK_HANDLE_STATUS_OPEN);
    }

    SVS_LOG((SVS_LM_INFO,"stop stream[%Q] tcp handle[%s:%d], all stream count[%d].",
            ullStreamID,
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number(),
            m_ConnectMap.size()));
    return RET_OK;
}

int32_t CTcpHandle::startActiveConnect(uint64_t ullStreamID,
                                    const ACE_INET_Addr &remoteAddr,
                                    ACE_INET_Addr &localAddr)
{
    if ((NETWORK_HANDLE_STATUS_OPEN != getStatus())
            && (NETWORK_HANDLE_STATUS_START != getStatus()))
    {
        SVS_LOG((SVS_LM_WARNING,"start stream[%Q] active tcp handle[%s:%d] fail, status[%d] abnormal.",
                ullStreamID,
                getLocalAddr().get_host_addr(),
                getLocalAddr().get_port_number(),
                getStatus()));

        return RET_ERR_STATUS_ABNORMAL;
    }

    ACE_Write_Guard<ACE_RW_Thread_Mutex> locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() != iter)
    {
        SVS_LOG((SVS_LM_WARNING,"start stream[%Q] active tcp handle[%s:%d] fail, stream id already exist.",
                ullStreamID,
                getLocalAddr().get_host_addr(),
                getLocalAddr().get_port_number()));
        return RET_FAIL;
    }

    CTcpConnect *pConnect = createTcpConnect();
    if (NULL == pConnect)
    {
        SVS_LOG((SVS_LM_WARNING,"tcp handle[%u] connect[%s:%d] fail, create active connect fail.",
                getLocalAddr().get_port_number(),
                remoteAddr.get_host_addr(),
                remoteAddr.get_port_number()));
        return RET_FAIL;
    }


    pConnect->setStreamID(ullStreamID);
    pConnect->setTcpHandle(this);

    if (RET_OK != pConnect->start(getLocalAddr(), remoteAddr, true))
    {
        SVS_LOG((SVS_LM_WARNING,"tcp handle[%u] connect[%s:%d] fail, start active connect fail.",
                getLocalAddr().get_port_number(),
                remoteAddr.get_host_addr(),
                remoteAddr.get_port_number()));
        destroyTcpConnect(pConnect);
        return RET_FAIL;
    }

    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        setStatus(NETWORK_HANDLE_STATUS_START);
    }

    TCP_CONNECT_INFO info;
    info.StartTime  = SVS_GetSecondTime();
    info.TcpConnect = pConnect;
    m_ConnectMap[ullStreamID] = info;

    localAddr = pConnect->getLocalAddr();
    SVS_LOG((SVS_LM_INFO,"start stream[%Q] handle[%s:%d] active connect success, "
            "all stream count[%d].",
            ullStreamID,
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number(),
            m_ConnectMap.size()));

    return RET_OK;
}

void CTcpHandle::setInterleaveNum(uint64_t ullStreamID, char cVideoNum, char cAudioNum)
{
    ACE_Read_Guard<ACE_RW_Thread_Mutex> locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() == iter)
    {
        // ��IDδ��Handle��ע�ᣬ��������ʧ��
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] set interleave num fail, can't find stream.",
                ullStreamID));
        return;
    }

    CTcpConnect *pConnect = iter->second.TcpConnect;
    if (NULL == pConnect)
    {
        // ��IDδ��Handle��ע�ᣬ��������ʧ��
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] set interleave num fail, connect is null.",
                ullStreamID));
        return;
    }

    pConnect->setVideoInterleaveNum(cVideoNum);
    pConnect->setAudioInterleaveNum(cAudioNum);
    return;
}

// �������״̬���������״̬�쳣������false
bool CTcpHandle::checkHandleStatus(uint64_t ullStreamID, uint32_t unTimeout)
{

    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        SVS_LOG((SVS_LM_INFO,"stream[%Q] check tcp handle[%d] status fail, current status[%d].",
                        ullStreamID,
                        getLocalAddr().get_port_number(),
                        getStatus()));
        return false;
    }

    ACE_Read_Guard<ACE_RW_Thread_Mutex> locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() == iter)
    {
        // ��IDδ��Handle��ע�ᣬ��������ʧ��
        SVS_LOG((SVS_LM_INFO,"stream[%Q] check tcp handle[%d] status fail, can't find stream.",
                        ullStreamID,
                        getLocalAddr().get_port_number()));
        return false;
    }

    CTcpConnect *pConnect = iter->second.TcpConnect;
    if (NULL == pConnect)
    {

        uint32_t ulNow = SVS_GetSecondTime() ;
        SVS_LOG((SVS_LM_DEBUG,"stream[%Q] check tcp handle[%d] status, connect is null.current time [%u], start time [%u].",
                            ullStreamID,
                            getLocalAddr().get_port_number(),ulNow,iter->second.StartTime));

        //if (SVS_GetSecondTime() - iter->second.StartTime >= unTimeout)STREAM_CONNECT_ABNORMAL_INTERVAL
        if (ulNow - iter->second.StartTime >= 20 )
        {
            SVS_LOG((SVS_LM_INFO,"stream[%Q] check tcp handle[%d] status fail, connect is null.current time [%u], start time [%u].",
                            ullStreamID,
                            getLocalAddr().get_port_number(),ulNow,iter->second.StartTime));
            return false;
        }

        // δ��ʱ�����سɹ�
        return true;
    }

    return pConnect->checkHandleStatus(unTimeout);
}

int32_t CTcpHandle::send_n(uint64_t ullStreamID,
                        const char *pDataBuffer,
                        uint32_t unDataSize,
                        const ACE_INET_Addr &peerAddr)
{
    ACE_Read_Guard<ACE_RW_Thread_Mutex> locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() == iter)
    {
        SVS_LOG((SVS_LM_DEBUG,"tcp handle send message fail, can't find stream id[%Q]",
                ullStreamID));
        return RET_FAIL;
    }

    CTcpConnect *pConn = iter->second.TcpConnect;
    if (NULL == pConn)
    {
        SVS_LOG((SVS_LM_WARNING,"tcp handle send message fail, tcp connect is null. stream id[%Q]",
                ullStreamID));
        return RET_FAIL;
    }

    return pConn->send_n(pDataBuffer, unDataSize, peerAddr);
}

int32_t CTcpHandle::sendMessage(uint64_t ullStreamID,
                            const char *pDataBuffer,
                            uint32_t unDataSize,
                            const ACE_INET_Addr &peerAddr)
{
    ACE_Read_Guard<ACE_RW_Thread_Mutex> locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() == iter)
    {
        SVS_LOG((SVS_LM_DEBUG,"tcp handle send message fail, can't find stream id[%Q]",
            ullStreamID));
        return RET_FAIL;
    }

    CTcpConnect *pConnect = iter->second.TcpConnect;
    if (NULL == pConnect)
    {
        SVS_LOG((SVS_LM_WARNING,"tcp handle send message fail, tcp connect is null. stream id[%Q]",
            ullStreamID));
        return RET_FAIL;
    }

    return pConnect->sendMessage(pDataBuffer, unDataSize, peerAddr);
}

int32_t CTcpHandle::sendMessageBlock(uint64_t ullStreamID,
                                ACE_Message_Block **pMessage,
                                uint32_t unMsgCount,
                                const ACE_INET_Addr &peerAddr)
{
    ACE_Read_Guard<ACE_RW_Thread_Mutex> locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() == iter)
    {
        return RET_FAIL;
    }

    CTcpConnect *pConnect = iter->second.TcpConnect;
    if (NULL == pConnect)
    {
        return RET_FAIL;
    }

    return pConnect->sendMessage(pMessage, unMsgCount, peerAddr);
}


int32_t CTcpHandle::sendv(uint64_t ullStreamID,
                      const iovec* pIovec,
                      uint32_t vecSize,
                      uint32_t unDataSize)
{
    ACE_Read_Guard<ACE_RW_Thread_Mutex> locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() == iter)
    {
        return RET_FAIL;
    }

    CTcpConnect *pConnect = iter->second.TcpConnect;
    if (NULL == pConnect)
    {
        return RET_FAIL;
    }

    return pConnect->sendv(pIovec, vecSize, unDataSize);
}

int32_t CTcpHandle::handle_input(ACE_HANDLE handle)
{
    ACE_INET_Addr addr;
    ACE_SOCK_Stream stream;
    int32_t nRet = m_TcpAcceptor.accept(stream, &addr);
    if (0 != nRet)
    {
        int32_t iErrCode = ACE_OS::last_error();
        SVS_LOG((SVS_LM_WARNING,"tcp handle[%s:%d] accept connection fail, wait retry, errno[%d].",
                            getLocalAddr().get_host_addr(),
                            getLocalAddr().get_port_number(),
                            iErrCode));
        return 0;
    }

    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,
            "tcp handle[%d] close peer[%s:%d] connection. status[%d] is incorrect.",
            getLocalAddr().get_port_number(),
            addr.get_host_addr(),
            addr.get_port_number(),
            getStatus()));
        // ����δ�򿪣����ܽ��ܶԶ�����
        stream.close();
        return RET_OK;
    }

    SVS_LOG((SVS_LM_INFO,"tcp handle[%s:%d] accepted new connect.",
                    getLocalAddr().get_host_addr(),
                    getLocalAddr().get_port_number()));

    // ����һ���µ�����
    CTcpConnect *pConnect = createTcpConnect();
    if (NULL == pConnect)
    {
        stream.close();
        return RET_OK;
    }

    pConnect->set_handle(stream.get_handle());
    pConnect->setTcpHandle(this);

    // ��������
    nRet = pConnect->start(getLocalAddr(), addr, false);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"start tcp connect fail."));
        stream.close();
        destroyTcpConnect(pConnect);
        return RET_OK;
    }

    return 0;
}


int32_t CTcpHandle::handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask)
{
    if (NETWORK_HANDLE_STATUS_INIT == getStatus())
    {
        // �����ѱ��ر�
        return RET_ERR_STATUS_ABNORMAL;
    }

    // ȡ��ע�ᣬ���ٽ�������
    int32_t nRet = CNetConnManager::instance()->unRegistHandle(this,
                    ACE_Event_Handler::ACCEPT_MASK | ACE_Event_Handler::DONT_CALL);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"unregist handle from connect manager fail."));
    }

    m_TcpAcceptor.close();

    // ��������״̬
    setStatus(NETWORK_HANDLE_STATUS_INIT);

    SVS_LOG((SVS_LM_DEBUG,"close tcp handle[%s:%d] success.",
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number()));
    return 0;
}

ACE_HANDLE CTcpHandle::get_handle() const
{
    return m_TcpAcceptor.get_handle();
}

/// ע��һ�����ӣ���CTcpConnect����
int32_t CTcpHandle::addTcpConnect(CTcpConnect* pTcpConnect)
{
    if (NULL == pTcpConnect)
    {
        return RET_FAIL;
    }

    uint64_t ullStreamID = pTcpConnect->getStreamID();
    ACE_Write_Guard<ACE_RW_Thread_Mutex>  locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() == iter)
    {
        // û���ҵ���Ӧ����ID
        SVS_LOG((SVS_LM_WARNING,"tcp handle[%d] add connect fail, can't find stream[%Q].",
                getLocalAddr().get_port_number(),
                ullStreamID));
        return RET_FAIL;
    }

    CTcpConnect* pOldConnect = iter->second.TcpConnect;
    if (NULL != pOldConnect)
    {
        // ����ͬһ��ID�Ѿ���һ��������
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] add new connect, there is a old connect[%p].",
                ullStreamID,
                pOldConnect));

        return RET_FAIL;
    }

    iter->second.StartTime  = SVS_GetSecondTime();
    iter->second.TcpConnect = pTcpConnect;

    SVS_LOG((SVS_LM_INFO,"tcp handle[%d] add stream[%Q] connect success.",
                    getLocalAddr().get_port_number(),
                    ullStreamID));
    return RET_OK;
}

/// ɾ��һ�����ӣ���CTcpConnect����
int32_t CTcpHandle::delTcpConnect(CTcpConnect* pTcpConnect)
{
    if (NULL == pTcpConnect)
    {
        return RET_FAIL;
    }

    uint64_t ullStreamID = pTcpConnect->getStreamID();
    ACE_Write_Guard<ACE_RW_Thread_Mutex> locker(m_ConnectMapMutex);
    CONNECT_MAP_ITER iter = m_ConnectMap.find(ullStreamID);
    if (m_ConnectMap.end() != iter)
    {
        if (pTcpConnect == iter->second.TcpConnect)
        {
            iter->second.StartTime  = SVS_GetSecondTime();
            iter->second.TcpConnect = NULL;
            SVS_LOG((SVS_LM_INFO,"tcp handle[%d] clear stream[%Q]'s connect success.",
                        getLocalAddr().get_port_number(),
                        ullStreamID));
        }
    }

    destroyTcpConnect(pTcpConnect);

    SVS_LOG((SVS_LM_INFO,"tcp handle[%d] del stream[%Q] connect success.",
            getLocalAddr().get_port_number(),
            ullStreamID));
    return RET_OK;
}
