/*
 * TcpConnect.cpp
 *
 *  Created on: 2016-1-5
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_time.h"

#include "svs_log_msg.h"
#include "svs_adapter_tcp_handle.h"
#include "svs_adapter_tcp_connect.h"
#include "svs_adapter_network_handle.h"
#include "svs_adapter_net_conn_manager.h"
#include "svs_adapter_media_block_buffer.h"

CTcpConnect::CTcpConnect()
{
    m_pTcpHandle        = NULL;

    m_sockFD            = ACE_INVALID_HANDLE;
    m_ullStreamID       = 0;
    m_ulLastRecvTime    = 0;
    m_ulBufFullTime     = 0;

    m_cVideoInterleaveNum = 0;

    m_cAudioInterleaveNum = 2;

    m_pRecvBuffer       = NULL;
    m_SendList.clear();
    setStatus(NETWORK_HANDLE_STATUS_INIT);
    m_ulSignalMsgBufNum = 0;
    SVS_LOG((SVS_LM_DEBUG,"constructed tcp connect."));
}

CTcpConnect::~CTcpConnect()
{
    try
    {
        if (NULL != m_pRecvBuffer)
        {
            delete m_pRecvBuffer;
            m_pRecvBuffer = NULL;
        }

        for (TCP_SEND_LIST_ITER iter = m_SendList.begin(); iter != m_SendList.end(); iter++)
        {
            CMediaBlockBuffer::instance().freeMediaBlock(*iter);
        }

        SVS_LOG((SVS_LM_DEBUG,"~constructed tcp connect."));
    }
    catch (...)
    {
    }
    m_pTcpHandle    = NULL;
    m_sockFD        = ACE_INVALID_HANDLE;
    m_ullStreamID   = 0;
    m_pRecvBuffer   = NULL;
}

int32_t CTcpConnect::start(const ACE_INET_Addr &localAddr, const ACE_INET_Addr &remoteAddr, bool bActiveFlag)
{
    if (!bActiveFlag && (ACE_INVALID_HANDLE == m_sockFD))
    {
        return RET_FAIL;
    }

    m_localAddr  = localAddr;
    m_remoteAddr = remoteAddr;

    if (NULL == m_pRecvBuffer)
    {
        try
        {
            m_pRecvBuffer = new Static_PreAssign_Buffer;
        }
        catch(...)
        {
            return RET_ERR_SYS_NEW;
        }
        m_pRecvBuffer->wr_ptr(0);
        m_pRecvBuffer->rd_ptr(0);
    }

    if ((bActiveFlag) && (RET_OK != connectPeer()))
    {
        SVS_LOG((SVS_LM_WARNING,"tcp connect[%s:%d] connect peer fail.",
                        m_remoteAddr.get_host_addr(),
                        m_remoteAddr.get_port_number()));
        return RET_FAIL;
    }

    int32_t nRet = setSockOpt();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"tcp connect[%s:%d] set sock option fail.",
                                m_remoteAddr.get_host_addr(),
                                m_remoteAddr.get_port_number()));
        return RET_FAIL;
    }

    m_ulLastRecvTime = SVS_GetSecondTime();
    setStatus(NETWORK_HANDLE_STATUS_START);

    nRet = CNetConnManager::instance()->registHandle(this, ACE_Event_Handler::READ_MASK);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"tcp connect[%s:%d] regist event fail.",
                        m_remoteAddr.get_host_addr(),
                        m_remoteAddr.get_port_number()));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"tcp connect [%s:%d] start success, socket[%d].",
                    m_remoteAddr.get_host_addr(),
                    m_remoteAddr.get_port_number(),
                    m_sockFD));
    return RET_OK;
}

int32_t CTcpConnect::close()
{
    if (NETWORK_HANDLE_STATUS_DISCONNECT == getStatus())
    {
        return -1;
    }

    ACE_OS::shutdown(m_sockFD, SHUT_RDWR);
    setStatus(NETWORK_HANDLE_STATUS_DISCONNECT);

    SVS_LOG((SVS_LM_INFO,"stream[%Q] close tcp connect [%s:%d] success.",
                    m_ullStreamID,
                    m_remoteAddr.get_host_addr(),
                    m_remoteAddr.get_port_number()));
    return RET_OK;
}

int32_t CTcpConnect::setTcpHandle(CTcpHandle *pTcpHandle)
{
    m_pTcpHandle = pTcpHandle;
    return RET_OK;
}

NETWORK_HANDLE_STATUS CTcpConnect::getStatus()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_StatusMutex);
    return m_Status;
}

void CTcpConnect::setStatus(NETWORK_HANDLE_STATUS status)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_StatusMutex);
    m_Status = status;
}

void CTcpConnect::setStreamID(uint64_t ullStreamID)
{
    m_ullStreamID = ullStreamID;
}

uint64_t CTcpConnect::getStreamID() const
{
    return m_ullStreamID;
}

void CTcpConnect::setVideoInterleaveNum(char cNo)
{
    m_cVideoInterleaveNum = cNo;
    return;
}

char CTcpConnect::getVideoInterleaveNum() const
{
    return m_cVideoInterleaveNum;
}

void CTcpConnect::setAudioInterleaveNum(char cNo)
{
    m_cAudioInterleaveNum = cNo;
}

char CTcpConnect::getAudioInterleaveNum() const
{
    return m_cAudioInterleaveNum;
}


void CTcpConnect::set_handle(ACE_HANDLE handle)
{
    m_sockFD = handle;
}

ACE_HANDLE CTcpConnect::get_handle() const
{
    return m_sockFD;
}

ACE_INET_Addr CTcpConnect::getLocalAddr() const
{
    return m_localAddr;
}

int32_t CTcpConnect::send_n(const char *pDataBuffer,
                       uint32_t unDataSize,
                       const ACE_INET_Addr &peerAddr)
{
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        return RET_FAIL;
    }

    ACE_Guard<ACE_Thread_Mutex> myLock(m_SendMutex);
    while (!m_SendList.empty())
    {
        int32_t nRet = sendLeastData();
        if (RET_ERR_DISCONNECT == nRet)
        {
            return nRet;
        }

        if (RET_OK != nRet)
        {
            usleep(1);
            continue;
        }
        break;
    }

    ACE_Time_Value tvOut(1, 0);
    int32_t nSendSize = ACE::send_n(m_sockFD, pDataBuffer, unDataSize, &tvOut);
    if (0 >= nSendSize)
    {
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] tcp connect send_n message data fail, close handle.",
                getStreamID()));
        (void) close();
        return RET_ERR_DISCONNECT;
    }

    return RET_OK;
}

int32_t CTcpConnect::sendMessage(const char *pDataBuffer,
                             uint32_t unDataSize,
                             const ACE_INET_Addr &/*peerAddr*/)
{
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        return RET_FAIL;
    }

    ACE_Guard<ACE_Thread_Mutex> locker(m_SendMutex);
    if (m_ulSignalMsgBufNum >= MAX_BUF_SIGNAL_NUM)
    {
        SVS_LOG((SVS_LM_WARNING,"tcp connect send stream[%Q] message data fail, "
            "signal msg num reach limit[%d].",
            getStreamID(),
            m_ulSignalMsgBufNum));
        return RET_OK;
    }

    if (!m_SendList.empty()
            && (RET_OK != sendLeastData()))
    {
        ACE_Message_Block *pMsgBlock = CMediaBlockBuffer::instance().allocMediaBlock();
        if (NULL == pMsgBlock)
        {
            SVS_LOG((SVS_LM_WARNING,"tcp connect send stream[%Q] message data fail, "
                    "alloc cache buffer fail, close handle.",
                    getStreamID()));

            (void) close();
            return RET_ERR_DISCONNECT;
        }

        pMsgBlock->copy(pDataBuffer, unDataSize);
        m_SendList.push_back(pMsgBlock);

        m_ulSignalMsgBufNum++;

        SVS_LOG((SVS_LM_INFO,"Send list is not empty. cache signal message. "
            "stream id[%Q] send list size[%d] signal msg num[%d]",
            getStreamID(), m_SendList.size(), m_ulSignalMsgBufNum));

        return RET_OK;
    }

    int32_t nSendSize = ACE::send(m_sockFD, pDataBuffer, unDataSize);
    if (0 >= nSendSize)
    {
        int32_t iErrorCode = ACE_OS::last_error();
        if (checkIsDisconnect(iErrorCode))
        {
            SVS_LOG((SVS_LM_WARNING,"stream[%Q] connect send message fail, errno[%d] "
                    ", close handle[%d].",
                    getStreamID(),
                    iErrorCode,
                    m_sockFD));


            (void) close();
            return RET_ERR_DISCONNECT;
        }


        nSendSize = nSendSize > 0 ? nSendSize : 0;
    }

    if (unDataSize != (uint32_t)nSendSize)
    {
        ACE_Message_Block *pMsgBlock = CMediaBlockBuffer::instance().allocMediaBlock();
        if (NULL == pMsgBlock)
        {
            SVS_LOG((SVS_LM_WARNING,"tcp connect send stream[%Q] media data fail, "
                    "alloc cache buffer fail, close handle.",
                     getStreamID()));

            (void)close();
            return RET_ERR_DISCONNECT;
        }

        pMsgBlock->copy(pDataBuffer + nSendSize, unDataSize - (uint32_t)nSendSize);
        m_SendList.push_back(pMsgBlock);
    }

    return RET_OK;
}
/*lint -e818*/
int32_t CTcpConnect::sendMessage(ACE_Message_Block **pMessage,
                            uint32_t unMsgCount,
                            const ACE_INET_Addr &/*peerAddr*/)
{
    if ((NULL == pMessage) || (NETWORK_HANDLE_STATUS_START != getStatus()))
    {
        SVS_LOG((SVS_LM_WARNING,"tcp connect send message fail, msg[%p] status[%d]",
            pMessage,
            getStatus()));
        return RET_FAIL;
    }

    ACE_Guard<ACE_Thread_Mutex> locker(m_SendMutex);
    if (!m_SendList.empty())
    {
        if (RET_OK != sendLeastData())
        {
            return RET_FAIL;
        }
    }

    uint32_t unSendCount = 0;
    int32_t nSendSize            = 0;

    while (unSendCount < unMsgCount)
    {
        if (NULL == pMessage[unSendCount])
        {
            return RET_FAIL;
        }

        if(0 < pMessage[unSendCount]->length())
        {
            nSendSize = ACE::send(m_sockFD, pMessage[unSendCount]->rd_ptr(),
                            pMessage[unSendCount]->length());
            if (0 >= nSendSize)
            {
                int32_t iErrorCode = ACE_OS::last_error();
                if (checkIsDisconnect(iErrorCode))
                {
                    SVS_LOG((SVS_LM_WARNING,"stream[%Q] connect send message block fail, "
                                    "errno[%d], close handle[%d].",
                                    getStreamID(),
                                    iErrorCode,
                                    m_sockFD));
                    (void)close();
                    return RET_ERR_DISCONNECT;
                }

                nSendSize = nSendSize > 0 ? nSendSize : 0;
            }

            if (pMessage[unSendCount]->length() != (uint32_t) nSendSize)
            {
                break;
            }
        }
        unSendCount++;
    }

    if (unSendCount < unMsgCount)
    {
        for (; unSendCount < unMsgCount; unSendCount++)
        {
            ACE_Message_Block *pMsgBlock = pMessage[unSendCount]->duplicate();
            if (NULL == pMsgBlock)
            {
                SVS_LOG((SVS_LM_WARNING,"tcp connect send stream[%Q] media data fail, "
                        "duplicate cache buffer fail, close handle[%d].",
                        getStreamID(),
                        m_sockFD));
                (void)close();

                return RET_ERR_DISCONNECT;
            }

            pMsgBlock->rd_ptr((uint32_t) nSendSize);
            m_SendList.push_back(pMsgBlock);

            nSendSize = 0;
        }
    }

    return RET_OK;
}
/*lint +e818*/

int32_t CTcpConnect::sendv(const iovec* pIovec, uint32_t vecSize, uint32_t unDataSize)
{
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        return RET_FAIL;
    }

    ACE_Guard<ACE_Thread_Mutex> locker(m_SendMutex);
    if (!m_SendList.empty())
    {
        if (RET_OK != sendLeastData())
        {
            return RET_FAIL;
        }
    }

    int32_t nRet = ACE::sendv(m_sockFD, pIovec, (int32_t)vecSize);
    if (0 >= nRet)
    {
        int32_t iErrorCode = ACE_OS::last_error();
        if (checkIsDisconnect(iErrorCode))
        {
            SVS_LOG((SVS_LM_WARNING,"stream[%Q] connect sendv fail, errno[%d], close handle[%d].",
                                getStreamID(),
                                iErrorCode,
                                m_sockFD));
            (void) close();
            return RET_ERR_DISCONNECT;
        }

        nRet = nRet > 0 ? nRet : 0;
    }

    if (unDataSize != (uint32_t)nRet)
    {
        ACE_Message_Block *pMsgBlock = CMediaBlockBuffer::instance().allocMediaBlock();
        if (NULL == pMsgBlock)
        {
            SVS_LOG((SVS_LM_WARNING,"tcp connect send stream[%Q] media data fail, alloc cache fail.",
                            getStreamID()));
            (void)close();
            return RET_ERR_DISCONNECT;
        }

        uint32_t unSize = (uint32_t)nRet;
        for (uint32_t i = 0; i < vecSize; i++)
        {
            if (unSize < pIovec[i].iov_len)
            {
                pMsgBlock->copy((char*)pIovec[i].iov_base + unSize,
                                  pIovec[i].iov_len - unSize);
                unSize = 0;
            }
            else
            {
                unSize -= pIovec[i].iov_len;
            }
        }

        m_SendList.push_back(pMsgBlock);
    }

    return RET_OK;
}
int32_t CTcpConnect::handle_input(ACE_HANDLE handle)
{
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        return -1;
    }

    if ((NULL == m_pRecvBuffer) || (ACE_INVALID_HANDLE == m_sockFD))
    {
        return -1;
    }

    m_ulLastRecvTime = SVS_GetSecondTime();

    int32_t iRecvLen = (int32_t) m_pRecvBuffer->size() - (int32_t) m_pRecvBuffer->length();
    do
    {
        if (iRecvLen <= 0)
        {
            if (SVS_GetSecondTime() - m_ulBufFullTime >= INVALID_DATA_ALARM_INTERVAL)
            {
                SVS_LOG((SVS_LM_INFO,"stream[%Q] tcp connect[%d] recv buffer is full, size[%u] length[%u].",
                         m_ullStreamID,
                         m_localAddr.get_port_number(),
                         m_pRecvBuffer->size(),
                         m_pRecvBuffer->length()));

                m_ulBufFullTime = SVS_GetSecondTime();
            }
            usleep(1);
        }
        else
        {
            ACE_OS::last_error(0);
            iRecvLen = ACE::recv(m_sockFD, m_pRecvBuffer->wr_ptr(), (size_t) iRecvLen);

            if (iRecvLen <= 0)
            {
                int32_t iErrorCode = ACE_OS::last_error();
                if (checkIsDisconnect(iErrorCode))
                {
                    SVS_LOG((SVS_LM_WARNING,"stream[%Q] tcp connect[%d] recv data fail, "
                            "close handle[%d]. errno[%d].",
                            m_ullStreamID,
                            m_localAddr.get_port_number(),
                            m_sockFD,
                            iErrorCode));
                    return -1;
                }

                SVS_LOG((SVS_LM_INFO,"stream[%Q] tcp connect[%d] recv data fail, wait retry. errno[%d].",
                        m_ullStreamID,
                        m_localAddr.get_port_number(),
                        iErrorCode));
                break;
            }

            m_pRecvBuffer->wr_ptr((size_t)(m_pRecvBuffer->length() + (size_t) iRecvLen));
            m_pRecvBuffer->rd_ptr((size_t) 0);
        }
    }while(0);

    size_t processedSize = 0;
    size_t totalSize     = m_pRecvBuffer->length();
    int32_t nSize            = 0;
    do
    {
        nSize = handleRecvedData(m_pRecvBuffer->rd_ptr() + processedSize,
                                 m_pRecvBuffer->length() - processedSize);
        if (nSize < 0)
        {
            SVS_LOG((SVS_LM_WARNING,"stream[%Q] tcp connect[%d] handle recv data fail, close handle[%d]. ",
                    m_ullStreamID,
                    m_localAddr.get_port_number(),
                    m_sockFD));
            return -1;
        }

        if (0 == nSize)
        {
            break;
        }

        processedSize += (size_t)nSize;
    }while(processedSize < totalSize);

    size_t dataSize = m_pRecvBuffer->length() - processedSize;
    (void) m_pRecvBuffer->copy(m_pRecvBuffer->rd_ptr() + processedSize, dataSize);
    m_pRecvBuffer->rd_ptr((size_t) 0);
    m_pRecvBuffer->wr_ptr(dataSize);

    return RET_OK;
}

int32_t CTcpConnect::handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask /*close_mask*/)
{
    if (ACE_INVALID_HANDLE == m_sockFD)
    {
        return -1;
    }

    int32_t nRet = CNetConnManager::instance()->unRegistHandle(this,
                                                            ACE_Event_Handler::READ_MASK
                                                            | ACE_Event_Handler::WRITE_MASK
                                                            | ACE_Event_Handler::DONT_CALL);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] tcp connect [%s:%d] handle close fail, unregist handle fail.",
                                m_ullStreamID,
                                m_remoteAddr.get_host_addr(),
                                m_remoteAddr.get_port_number()));
        return -1;
    }

    ACE_OS::shutdown(m_sockFD, SHUT_RDWR);
    ACE_OS::close(m_sockFD);

    SVS_LOG((SVS_LM_INFO,"stream[%Q] tcp connect [%s:%d] handle close[%d] success.",
                    m_ullStreamID,
                    m_remoteAddr.get_host_addr(),
                    m_remoteAddr.get_port_number(),
                    m_sockFD));

    m_sockFD = ACE_INVALID_HANDLE;
    if (NULL != m_pTcpHandle)
    {
        (void)m_pTcpHandle->delTcpConnect(this);
    }

    return RET_OK;
}

bool CTcpConnect::checkHandleStatus(uint32_t unTimeout)
{
    uint32_t ulCostTime = SVS_GetSecondTime() - m_ulLastRecvTime;
    if (unTimeout <= ulCostTime)
    {
        SVS_LOG((SVS_LM_INFO,"stream[%Q] tcp connect[%d] not recv any data at [%u]s.",
                m_ullStreamID ,
                m_localAddr.get_port_number(),
                ulCostTime));
        return false;
    }

    SVS_LOG((SVS_LM_DEBUG,"check tcp connect[%d] status success.",
                  m_localAddr.get_port_number()));
    return true;
}

int32_t CTcpConnect::setSockOpt()
{
    if (ACE_INVALID_HANDLE == m_sockFD)
    {
        return RET_FAIL;
    }

    uint32_t nSize = SOCKET_RECV_BUFFER_SIZE;
    if (0 != ACE_OS::setsockopt(m_sockFD, SOL_SOCKET, SO_RCVBUF, (const char*)&nSize, sizeof(nSize)))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp tcp connect[%s:%d] set recv buffer fail, errno[%d].",
                m_localAddr.get_host_addr(),
                m_localAddr.get_port_number(),
                errno));
    }

    nSize = SOCKET_SEND_BUFFER_SIZE;
    if (0 != ACE_OS::setsockopt(m_sockFD, SOL_SOCKET, SO_SNDBUF, (const char*)&nSize, sizeof(nSize)))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp tcp connect[%s:%d] set send buffer fail, errno[%d].",
                m_localAddr.get_host_addr(),
                m_localAddr.get_port_number(),
                errno));
    }

    if (ACE_OS::fcntl(m_sockFD, F_SETFL, ACE_OS::fcntl(m_sockFD, F_GETFL) | O_NONBLOCK))
    {
        SVS_LOG((SVS_LM_WARNING,"rtp tcp connect[%s:%d] set O_NONBLOCK fail, errno[%d].",
                        m_localAddr.get_host_addr(),
                        m_localAddr.get_port_number(),
                        errno));
        return RET_FAIL;
    }

    return RET_OK;
}


int32_t CTcpConnect::connectPeer()
{
    ACE_SOCK_Connector peerConnector;
    ACE_SOCK_Stream    sockStream;
    ACE_Time_Value     tv(1);

    if (-1 == peerConnector.connect(sockStream, m_remoteAddr, &tv, ACE_Addr::sap_any, 1))
    {
        SVS_LOG((SVS_LM_WARNING,"stream[%Q] coneect to peer[%s:%d] fail, errno[%d].",
                m_ullStreamID,
                m_remoteAddr.get_host_addr(),
                m_remoteAddr.get_port_number(),
                ACE_OS::last_error()));

        return RET_FAIL;
    }

    m_sockFD    = sockStream.get_handle();
    sockStream.get_local_addr(m_localAddr);

    SVS_LOG((SVS_LM_INFO,"stream[%Q] coneect to peer success, local addr[%s:%d].",
                     m_ullStreamID,
                     m_localAddr.get_host_addr(),
                     m_localAddr.get_port_number()));

    SVS_LOG((SVS_LM_INFO,"stream[%Q] coneect to peer[%s:%d] success.",
                    m_ullStreamID,
                    m_remoteAddr.get_host_addr(),
                    m_remoteAddr.get_port_number()));

    return RET_OK;
}

bool CTcpConnect::checkIsDisconnect(int32_t nErrNo) const
{
    if (EAGAIN == nErrNo
            || ETIME == nErrNo
            || EWOULDBLOCK == nErrNo
            || ETIMEDOUT == nErrNo
            || EINTR == nErrNo)
    {
        return false;
    }
    return true;
}

int32_t CTcpConnect::sendLeastData()
{
    while (!m_SendList.empty())
    {
        ACE_Message_Block *pMsgBlock = m_SendList.front();
        int32_t nRet = ACE::send(m_sockFD, pMsgBlock->rd_ptr(), pMsgBlock->length());
        if (0 >= nRet)
        {
            int32_t iErrorCode = ACE_OS::last_error();
            if (checkIsDisconnect(iErrorCode))
            {
                SVS_LOG((SVS_LM_WARNING,"stream[%Q] tcp connect[%d] send least data fail, "
                                    "errno[%d], close handle[%d].",
                                    m_ullStreamID,
                                    m_localAddr.get_port_number(),
                                    iErrorCode,
                                    m_sockFD));

                (void) close();

                return RET_ERR_DISCONNECT;
            }

            nRet = nRet > 0 ? nRet : 0;
        }

        if (pMsgBlock->length() != (uint32_t) nRet)
        {
            pMsgBlock->rd_ptr((uint32_t) nRet);
            return RET_FAIL;
        }

        CMediaBlockBuffer::instance().freeMediaBlock(pMsgBlock);
        m_SendList.pop_front();
    }
     m_ulSignalMsgBufNum = 0;

    return RET_OK;
}


