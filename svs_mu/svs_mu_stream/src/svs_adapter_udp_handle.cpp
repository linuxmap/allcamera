#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_time.h"
#include <vms/vms.h>
#include "svs_adapter_udp_handle.h"

#include "svs_adapter_net_conn_manager.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_session.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_exchange.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_rtp_packet.h"
CUdpHandle::CUdpHandle()
{
    m_ulLastInvalidAlarmTime = 0;
    m_ulLastRecvTime         = 0;
    m_unStreamCount          = 0;
    m_ullStreamID            = 0;

    m_enPlayType          = 0;
}

CUdpHandle::~CUdpHandle()
{
    try
    {
        this->closeHandle();
    }
    catch(...)
    {}
    m_ulLastInvalidAlarmTime = 0;
    m_unStreamCount          = 0;
    m_ullStreamID            = 0;
}

int32_t CUdpHandle::openHandle(const ACE_INET_Addr &localAddr)
{
    if (NETWORK_HANDLE_STATUS_INIT != getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,"open rtp udp address[%s:%d] fail, status[%d] abnormal.",
                        localAddr.get_host_addr(),
                        localAddr.get_port_number(),
                        getStatus()));
        return RET_ERR_STATUS_ABNORMAL;
    }

    // ���汾�ص�ַ
    setLocalAddr(localAddr);

    int32_t nRet = m_UdpSocket.open(localAddr);
    if (-1 == nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"open rtp udp address[%s:%d] fail, errno[%d].",
                localAddr.get_host_addr(),
                localAddr.get_port_number(),
                errno));
        return RET_FAIL;
    }

    // TODO �ڲ�ͬ�����»�������Ҫ����ͬ
    uint32_t nSize = SOCKET_RECV_BUFFER_SIZE;
    if (0 != m_UdpSocket.set_option(SOL_SOCKET, SO_RCVBUF, (void *) &nSize, sizeof(nSize)))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp udp address[%s:%d] set recv buffer fail, errno[%d].",
                localAddr.get_host_addr(),
                localAddr.get_port_number(),
                errno));
    }

    nSize = SOCKET_SEND_BUFFER_SIZE;
    if (0 != m_UdpSocket.set_option(SOL_SOCKET, SO_SNDBUF, (void *) &nSize, sizeof(nSize)))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp udp address[%s:%d] set send buffer fail, errno[%d].",
                localAddr.get_host_addr(),
                localAddr.get_port_number(),
                errno));
    }

    // �����ӹ�����ע�ᣬ�Ϳ��Խ��������
    nRet = CNetConnManager::instance()->registHandle((ACE_Event_Handler*) this,
            ACE_Event_Handler::READ_MASK);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"rtp udp address[%s:%d] regist event fail, errno[%d].",
                                localAddr.get_host_addr(),
                                localAddr.get_port_number(),
                                ACE_OS::last_error()));
        return RET_FAIL;
    }
    errno = 0;
    setStatus( NETWORK_HANDLE_STATUS_OPEN);
    SVS_LOG((SVS_LM_DEBUG,"rtp udp handle ip[%s] port[%d] open success.",
            localAddr.get_host_addr(),
            localAddr.get_port_number()));

    return RET_OK;
}

void CUdpHandle::closeHandle()
{
    int32_t nHandle = m_UdpSocket.get_handle();
    if (ACE_INVALID_HANDLE == nHandle)
    {
        return;
    }

    // �����ӹ�����ע��
    int32_t nRet = CNetConnManager::instance()->unRegistHandle((ACE_Event_Handler*) this,
              ACE_Event_Handler::READ_MASK | ACE_Event_Handler::DONT_CALL);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"unregist rtp udp handle[%s:%d] to connect manager fail.",
                getLocalAddr().get_host_addr(),
                getLocalAddr().get_port_number()));
    }

    (void) ACE_OS::shutdown(nHandle, SHUT_RDWR);
    (void) ACE_OS::close(nHandle);

    // ��������״̬�ص���ʼ״̬
    setStatus( NETWORK_HANDLE_STATUS_INIT);
    SVS_LOG((SVS_LM_DEBUG,"close rtp udp handle[%s:%d] success.",
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number()));
    return;
}

int32_t CUdpHandle::startHandle(uint64_t ullStreamID, const ACE_INET_Addr &remoteAddr)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ListMutex);
    if ((NETWORK_HANDLE_STATUS_OPEN != getStatus())
      &&(NETWORK_HANDLE_STATUS_START != getStatus()))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Start udp handle fail, status [%d] is incorrect.",
            getStatus()));
        return RET_ERR_STATUS_ABNORMAL;
    }

    m_RemoteAddr = remoteAddr;

    CStreamSession *pSession = CStreamSessionFactory::instance()->findSession(ullStreamID);
    if (NULL == pSession)
    {
        SVS_LOG((SVS_LM_WARNING,"session[%Q] udp handle get service type fail.", ullStreamID));
        return RET_FAIL;
    }

    m_enPlayType = pSession->getPlayType();
    CStreamSessionFactory::instance()->releaseSession(pSession);

    m_unStreamCount++;

    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        m_ulLastRecvTime = SVS_GetSecondTime();
        m_ullStreamID    = ullStreamID;
        setStatus( NETWORK_HANDLE_STATUS_START);
    }

    SVS_LOG((SVS_LM_INFO,"start stream[%Q] rtp udp handle[%s:%d], all stream count[%d].",
            ullStreamID,
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number(),
            m_unStreamCount));

    return RET_OK;
}

int32_t CUdpHandle::stopHandle(uint64_t ullStreamID)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ListMutex);
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        return RET_ERR_STATUS_ABNORMAL;
    }

    m_unStreamCount--;

    if (0 == m_unStreamCount)
    {
        m_ullStreamID = 0;
        setStatus( NETWORK_HANDLE_STATUS_OPEN);
    }

    SVS_LOG((SVS_LM_INFO,"stop stream[%Q] rtp udp handle[%s:%d], all stream count[%d].",
            ullStreamID,
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number(),
            m_unStreamCount));
    return RET_OK;
}

int32_t CUdpHandle::sendMessage(uint64_t ullStreamID,
                            const char *pDataBuffer,
                            uint32_t unDataSize,
                            const ACE_INET_Addr &peerAddr)
{
    uint16_t usPort = getLocalAddr().get_port_number();
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] udp handle[%d] send data to [%s:%d] fail, "
                        "handle status[%d] abnormal.",
                        ullStreamID,
                        usPort,
                        peerAddr.get_host_addr(),
                        peerAddr.get_port_number(),
                        getStatus()));
        return RET_FAIL;
    }

    int32_t nRet = m_UdpSocket.send(pDataBuffer, unDataSize, peerAddr);
    if (0 >= nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] udp handle[%d] send data to [%s:%d] fail.",
                ullStreamID,
                usPort,
                peerAddr.get_host_addr(),
                peerAddr.get_port_number()));

        return RET_FAIL;
    }

    return RET_OK;
}

int32_t CUdpHandle::handle_input(ACE_HANDLE handle)
{
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        recvInvalidData();
        return RET_OK;
    }

    ACE_Message_Block *pMsg = CMediaBlockBuffer::instance().allocMediaBlock();
    if (NULL == pMsg)
    {
        SVS_LOG((SVS_LM_WARNING,"rtp udp port[%d] alloc media block fail.",
                getLocalAddr().get_port_number()));
        return RET_OK;
    }

    ACE_INET_Addr remoteAddr;
    STREAM_TRANSMIT_PACKET *pPacket = (STREAM_TRANSMIT_PACKET *)(void*)pMsg->base();
    pMsg->wr_ptr(sizeof(STREAM_TRANSMIT_PACKET) - 1);
    int32_t nSize = m_UdpSocket.recv(pPacket->cData, pMsg->space(), remoteAddr, MSG_DONTWAIT);
    if (0 >= nSize)
    {
        return RET_OK;
    }


    CRtpPacket rtpPacket;
    (void)rtpPacket.ParsePacket(pMsg->wr_ptr(),(uint32_t)nSize);
    nSize -= (int32_t)rtpPacket.GetTailLen();

    pMsg->wr_ptr((size_t)nSize);

#if 0
    if (m_RemoteAddr != remoteAddr)
    {
        CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
        if (SVS_GetSecondTime() - m_ulLastInvalidAlarmTime > INVALID_DATA_ALARM_INTERVAL)
        {
            m_ulLastInvalidAlarmTime = SVS_GetSecondTime();
            SVS_LOG((SVS_LM_WARNING,"stream[%Q] udp port [%d] recieved [%s:%d] invalid data.",
                    m_ullStreamID,
                    getLocalAddr().get_port_number(),
                    remoteAddr.get_host_addr(),
                    remoteAddr.get_port_number());

            SVS_LOG((SVS_LM_WARNING,"stream[%Q] udp port [%d] wait [%s:%d] send data.",
                             m_ullStreamID,
                             getLocalAddr().get_port_number(),
                             m_RemoteAddr.get_host_addr(),
                             m_RemoteAddr.get_port_number());
        }
        return RET_OK;
    }
#endif

    m_ulLastRecvTime = SVS_GetSecondTime();

    int32_t nRet = handleRecvedData(pMsg, remoteAddr);
    if (RET_OK != nRet)
    {
        CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
    }

    return RET_OK;
}

bool CUdpHandle::checkHandleStatus(uint64_t /*ullStreamID*/, uint32_t unTimeout)
{
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        SVS_LOG((SVS_LM_INFO,"check udp port[%d] status fail, current status[%d].",
                getLocalAddr().get_port_number(), getStatus()));
        return false;
    }

    uint32_t ulCostTime = SVS_GetSecondTime() - m_ulLastRecvTime;
    if (unTimeout <= ulCostTime)
    {
        SVS_LOG((SVS_LM_INFO,"check udp port[%d] status fail, not recv any data in [%u]s.",
                        getLocalAddr().get_port_number(), ulCostTime));
        return false;
    }

    SVS_LOG((SVS_LM_DEBUG,"check udp port[%d] status success.",
                    getLocalAddr().get_port_number()));
    return true;
}

int32_t CUdpHandle::handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask /*close_mask*/)
{
    closeHandle();
    return RET_OK;
}

ACE_HANDLE CUdpHandle::get_handle() const
{
    return m_UdpSocket.get_handle();
}


void CUdpHandle::recvInvalidData()
{
    char dataBuf[MAX_UDP_PACK_SIZE] = { 0 };
    ACE_INET_Addr peerAddr;
    uint32_t ulCurTick = SVS_GetSecondTime();
    (void) m_UdpSocket.recv(dataBuf, MAX_UDP_PACK_SIZE, peerAddr, MSG_DONTWAIT);
    if (ulCurTick - m_ulLastInvalidAlarmTime > INVALID_DATA_ALARM_INTERVAL)
    {
        m_ulLastInvalidAlarmTime = ulCurTick;
        SVS_LOG((SVS_LM_WARNING,"udp port [%d] recieved [%s:%d] invalid data.",
                getLocalAddr().get_port_number(),
                peerAddr.get_host_addr(),
                peerAddr.get_port_number()));
        return;
    }

    return;
}
