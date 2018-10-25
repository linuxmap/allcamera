#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <list>
#include <time.h>
#include <vms/vms.h>
#include "svs_vms_client_link.h"
#include "svs_vms_system_api.h"

CClientLink::CClientLink():ILink()
{
    // ��ʼ������ĳ�Ա����
    m_ucLinkType = LINK_TYPE_CLIENT;
    m_nUsedBuffLength = 0;
    m_ucHeartbeatTimes = 0;
    m_tLastRecvHeartbeatTime = 0;   // �ϴν�������ʱ��
    m_tLastSendHeartbeatTime = 0;   // �ϴη�������ʱ��
    m_pszRecvBuff = NULL;
    memset(&m_stLocalAddr, 0x0, sizeof(m_stLocalAddr));
    memset(&m_stRemoteAddr, 0x0, sizeof(m_stRemoteAddr));
    memset(m_szLocalIp, 0x0, STR_IP_LEN);
    memset(m_szRemoteIp, 0x0, STR_IP_LEN);
    m_ucConnFlag = CONN_FLAG_IDLE;
    return;
}/*lint !e1401*/// m_MsgQueueMutex, m_WaitRespMsgMapLock ��initʱ���г�ʼ��

CClientLink::~CClientLink()
{
    if(NULL != m_pszRecvBuff)
    {
        delete []m_pszRecvBuff;
        m_pszRecvBuff = NULL;
    }

    (void)pthread_mutex_destroy(&m_MsgQueueMutex);
    (void)pthread_rwlock_destroy(&m_WaitRespMsgMapLock);
}

int32_t CClientLink::init(ISvsMsgHandler* pMsgHandler, uint32_t unLinkId, const ADDR* pstLocalAddr, const ADDR* pstRemoteAddr)
{
    if (NULL == pstLocalAddr)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init client link, the client local address pointer is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    if (NULL == pstRemoteAddr)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init client link, the server addresss pointer is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    if (NULL == pMsgHandler)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init client link, the message handler is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    // ��ʼ�����Ͷ��е��߳���
    if (0 != CSystem::instance()->pthread_mutex_init(&m_MsgQueueMutex, NULL))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init client link, init send queue mutex failed.");

        return SVS_STACK_FAIL;
    }

    // ��ʼ���ȴ���Ӧ��Ϣmap����
    if (0 != CSystem::instance()->pthread_rwlock_init(&m_WaitRespMsgMapLock, NULL))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init client link, init wait resp message map lock failed.");

        return SVS_STACK_FAIL;
    }

    // ��ʼ�����ջ�����
    try
    {
        m_pszRecvBuff = new char[MAX_RECV_BUFF];
    }
    catch(...)
    {
        m_pszRecvBuff = NULL;
        return SVS_STACK_ERR_MEMORY_ALLOCATE_FAIL;
    }
    memset(m_pszRecvBuff, 0x0, MAX_RECV_BUFF);


    m_unLinkId = unLinkId;
    m_pMsgHandler = pMsgHandler;
    m_stLocalAddr.unIp = pstLocalAddr->unIp;
    m_stLocalAddr.usPort = pstLocalAddr->usPort;
    struct in_addr stIp;
    stIp.s_addr = htonl(pstLocalAddr->unIp);
    strncpy(m_szLocalIp, inet_ntoa(stIp), STR_IP_LEN - 1);

    m_stRemoteAddr.unIp = pstRemoteAddr->unIp;
    m_stRemoteAddr.usPort = pstRemoteAddr->usPort;
    stIp.s_addr = htonl(pstRemoteAddr->unIp);
    strncpy(m_szRemoteIp, inet_ntoa(stIp), STR_IP_LEN - 1);

    m_ucConnFlag = CONN_FLAG_IDLE;
    return SVS_STACK_OK;
}


/*****************************************************************************
 �� �� ��  : CClientLink.startConn
 ��������  : �����ͻ�����������˵�����
 �������  : ��
 �� �� ֵ  : int32_t CClientLink::
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��6��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
int32_t CClientLink::startConn()
{
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Begin to start client connection, LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d.",
        m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort);

    // ����socket
    errno = 0; // ���errer code
    m_nSockFd = CSystem::instance()->socket(AF_INET, SOCK_STREAM, 0);
    if (m_nSockFd < 0)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to start client connection, create socket failed. "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, errno=%d.",
            m_szLocalIp, m_stLocalAddr.usPort,
            m_szRemoteIp, m_stRemoteAddr.usPort, errno);

        return SVS_STACK_FAIL;
    }

    // ����socket���ԣ�����Ϊ������
    setSocketOption();

    // �ж��û��Ƿ����˿ͻ��˵�ַ����û����Ͳ���Ҫ��
    if (INVALID_PORT != m_stLocalAddr.usPort && INVALID_IP != m_stLocalAddr.unIp)
    {
        // �󶨱��ص�ַ
        struct sockaddr_in clientAddr;
        memset(&clientAddr, 0x0, sizeof(clientAddr));
        clientAddr.sin_family = AF_INET;
        clientAddr.sin_addr.s_addr = htonl(m_stLocalAddr.unIp);
        clientAddr.sin_port = htons(m_stLocalAddr.usPort);
        errno = 0; // ���errer code
        if (0 != CSystem::instance()->bind(m_nSockFd, (sockaddr *)&clientAddr, sizeof(clientAddr)))/*lint !e740*///��ַ����ת��Ϊsockaddr
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
                "Fail to start client connection, bind local socket failed. SockFd=%d, LinkIndex=%u, "
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, errno=%d.",
                m_nSockFd, m_unLinkId, m_szLocalIp, m_stLocalAddr.usPort,
                m_szRemoteIp, m_stRemoteAddr.usPort, errno);

            return SVS_STACK_FAIL;
        }
    }

    // ����server�˵�ַ�� �������������
    errno = 0;
    struct sockaddr_in serverAddr;
    (void)CSystem::instance()->memset(&serverAddr, 0x0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(m_stRemoteAddr.unIp);
    serverAddr.sin_port = htons(m_stRemoteAddr.usPort);
    if (0 != CSystem::instance()->connect(m_nSockFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)))/*lint !e740*///��ַ����ת��Ϊsockaddr
    {
        // ������Ϊ��˵����������ʧ�ܣ�ע�������¼�
        if ((EINPROGRESS != errno) && (EWOULDBLOCK != errno))
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
                "Fail to start client connection, async connect to server failed. SockFd=%d, "
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, errno=%d.",
                m_nSockFd, m_szLocalIp, m_stLocalAddr.usPort,
                m_szRemoteIp, m_stRemoteAddr.usPort, errno);

            return SVS_STACK_FAIL;
        }

        // ��������״̬Ϊ��������, �ȵ��������ӳɹ���ʧ�ܣ���֪ͨҵ���
        setConnFlag(CONN_FLAG_CONNECTING);

        // ע���¼�
        epoll_event epEvent;
        epEvent.data.ptr = (void*)this;
        epEvent.events = EPOLLOUT;
        int32_t nRet = CLinkFactory::getInstance()->addEpollEvent(m_nSockFd, &epEvent);
        if (SVS_STACK_OK != nRet)
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
                "Fail to start client connection, register epoll write event failed. "
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, retCode=%d.",
                m_szLocalIp, m_stLocalAddr.usPort,
                m_szRemoteIp, m_stRemoteAddr.usPort, nRet);

            return SVS_STACK_FAIL;
        }

        // �����������
        errno = 0;
        return SVS_STACK_OK;
    }

    // ���ӳɹ�����������״̬����¼����ʼʱ��
    setConnFlag(CONN_FLAG_CONNECTED);
    m_tLastSendHeartbeatTime = time(NULL);
    m_tLastRecvHeartbeatTime = time(NULL);

    // ע���������
    CLinkFactory::getInstance()->addLinkForTimeout(this);

    // �첽���ӳɹ���ֱ��ע����¼�
    epoll_event epEvent;
    epEvent.data.ptr = (void*)this;
    epEvent.events = EPOLLIN;
    (void)CLinkFactory::getInstance()->addEpollEvent(m_nSockFd, &epEvent);

    // ֪ͨҵ���
    ConnEvent stEvent;
    stEvent.EventTime = time(NULL);
    stEvent.EventType = EVENT_TYPE_CONNECTED;
    stEvent.stPeerAddr.unIp = m_stRemoteAddr.unIp;
    stEvent.stPeerAddr.usPort = m_stRemoteAddr.usPort;
    stEvent.unHandleIndex = m_unLinkId;
    m_pMsgHandler->handleConnEvent(&stEvent);

    return SVS_STACK_OK;
}

// ֹͣ���ӣ����������linkFactory��ɾ��
int32_t CClientLink::stopConn()
{
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_INF,
        "Stop the connection, LinkIndex=%u, LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d.",
        m_unLinkId, m_szLocalIp, m_stLocalAddr.usPort,
        m_szRemoteIp, m_stRemoteAddr.usPort);

    // ע���������¼�
    CLinkFactory::getInstance()->delLinkForTimeout(this);

    // ҵ�������ֹͣ����ʱ��ֻ�ر����ӣ�֪ͨʣ�����Ϣ������֪ͨҵ������
    //handleClose();
    shutdownSock();
    notifyLeftMessage();
    notifyTimeoutMessage();

    return SVS_STACK_OK;
}

int32_t CClientLink::stopConn(const uint32_t unHandleIndex)
{
    // �ж�handleindex�Ƿ�ͱ��ر������ͬ
    if (m_unLinkId != unHandleIndex)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail stop connection by index, the given index differs from the correct index. "
            "givenIndex=%u, correctIndex=%u, LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d.",
            unHandleIndex, m_unLinkId, m_szLocalIp, m_stLocalAddr.usPort,
            m_szRemoteIp, m_stRemoteAddr.usPort);

        return SVS_STACK_FAIL;
    }

    (void)stopConn();
    return SVS_STACK_OK;
}

void CClientLink::shutdownSock()
{
    // �жϸ�socket����Ƿ��Ѿ��ͷţ����Ѿ��ͷţ�����������
    if (CONN_FLAG_CLOSED == m_ucConnFlag)
    {
        return;
    }

    // ��������״̬
    setConnFlag(CONN_FLAG_CLOSED);

    // ע���epoll�¼�
    struct epoll_event epEvent;
    memset(&epEvent, 0, sizeof(epEvent));
    epEvent.data.ptr = (void*)this;
    epEvent.events = (uint32_t)(EPOLLIN | EPOLLOUT); /*lint !e655*/// int����ֵ��֮��ֵ
    int32_t nRet = CLinkFactory::getInstance()->delEpollEvent(m_nSockFd, &epEvent);
    if (SVS_STACK_OK != nRet)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to delete epoll event when shutdown the socket. SockFd=%d, LinkIndex=%u, "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, retCode=0x%x.",
            m_nSockFd, m_unLinkId, m_szLocalIp, m_stLocalAddr.usPort,
            m_szRemoteIp, m_stRemoteAddr.usPort, nRet);
    }

    // �Ͽ����Ӳ��ر�socket�������sockFdΪ��Чֵ
    (void)CSystem::instance()->shutdown(m_nSockFd, SHUT_RDWR);
    (void)CSystem::instance()->close(m_nSockFd);
    m_nSockFd = INVALID_SOCK_FD;

    return;
}


/*****************************************************************************
 �� �� ��  : CClientLink.sendMsg
 ��������  : �첽������Ϣ���˴�ֻ�ǽ���Ϣ�ŵ����Ͷ�����
 �������  : ��
 �� �� ֵ  : int32_t CClientLink::
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��6��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
int32_t CClientLink::sendMsg(const uint32_t unHandleIndex,
                           const  char* pszMsg,
                           const uint32_t unMsgLength,
                           const uint32_t unTimeout)
{
    if (unHandleIndex != m_unLinkId)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to send message, the input handle index differs from local saved handle index."
            "InputHandleIndex=%u, LocakHandleIndex=%u, SockFd=%d, "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d.",
            unHandleIndex, m_unLinkId, m_nSockFd,
            m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort);

        return SVS_STACK_FAIL;
    }

    if (CONN_FLAG_CONNECTED != m_ucConnFlag)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "No need to send message, the connection to remote has disconnected."
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, LinkIndex=%u, SockFd=%d.",
            m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort,
            unHandleIndex, m_nSockFd);

        return SVS_STACK_ERR_DISCONN;
    }

    if (NULL == pszMsg)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to send message, the input message pointer is NULL. "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d.",
            m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort);

        return SVS_STACK_ERR_POINTER_NULL;
    }

    SVS_MSG_HEADER* pstHead = (SVS_MSG_HEADER*)pszMsg;/*lint !e826*/
    if (pstHead->PacketLength != unMsgLength)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to send message, the to be send message size differs from the size of SvsHead."
            "SendMsgSize=%d, HeadMsgSize=%d, LocalIp=%s, LocalPort=%d, "
            "RemoteIp=%s, RemotePort=%d, LinkIndex=%u, SockFd=%d.",
            unMsgLength, pstHead->PacketLength, m_szLocalIp, m_stLocalAddr.usPort,
            m_szRemoteIp, m_stRemoteAddr.usPort, unHandleIndex, m_nSockFd);

        return SVS_STACK_ERR_MSG_LENGTH_INVALID;
    }

    int32_t iRet = SVS_STACK_OK;

    if (m_MsgQueue.empty())
    {
        uint32_t unRealSendSize = 0;
        iRet = sendMsgToPeer(pszMsg, unMsgLength, unRealSendSize);
        if (SVS_STACK_OK != iRet)
        {
            closeSock(CONN_FLAG_CLOSING);
            return SVS_STACK_FAIL;
        }

        if (unRealSendSize < unMsgLength)
        {
            SENDMSGHEAD* pstSendMsgHead = NULL;
            iRet = setSendMsgHead(pstSendMsgHead, unMsgLength, pszMsg, unRealSendSize, unTimeout, 0);
            if (SVS_STACK_OK != iRet)
            {
                return iRet;
            }

            (void)CSystem::instance()->pthread_mutex_lock(&m_MsgQueueMutex);
            m_MsgQueue.push_front(pstSendMsgHead);
            (void)CSystem::instance()->pthread_mutex_unlock(&m_MsgQueueMutex);

            struct epoll_event epEvent;
            (void)CSystem::instance()->memset(&epEvent, 0, sizeof(epEvent));
            epEvent.data.ptr = (void*)this;
            epEvent.events = (uint32_t)(EPOLLIN | EPOLLOUT); /*lint !e655*/
            (void)CLinkFactory::getInstance()->modEpollEvent(m_nSockFd, &epEvent);

            return SVS_STACK_OK;
        }

        if (bWaitRespMsg(unTimeout, pstHead->TransactionNo))
        {
            SENDMSGHEAD* pstSendMsgHead = NULL;
            iRet = setSendMsgHead(pstSendMsgHead, unMsgLength, pszMsg, unRealSendSize, unTimeout, time(NULL));
            if (SVS_STACK_OK != iRet)
            {
                return iRet;
            }

            addWaitRespMsg(pstHead->TransactionNo, pstSendMsgHead);
        }
        return SVS_STACK_OK;
    }


    uint32_t nQueueSize = m_MsgQueue.size();
    if (MAX_QUEUE_SIZE <= nQueueSize)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to send message to remote, the message queue is full. "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, LinkIndex=%u, SockFd=%d.",
            m_szLocalIp, m_stLocalAddr.usPort,
            m_szRemoteIp, m_stRemoteAddr.usPort,
            m_unLinkId, m_nSockFd);

        return SVS_STACK_ERR_QUEUE_FULL;
    }

    SENDMSGHEAD* pstSendMsgHead = NULL;
    iRet = setSendMsgHead(pstSendMsgHead, unMsgLength, pszMsg, 0, unTimeout, 0);
    if (SVS_STACK_OK != iRet)
    {
        return iRet;
    }

    (void)CSystem::instance()->pthread_mutex_lock(&m_MsgQueueMutex);
    m_MsgQueue.push_back(pstSendMsgHead);
    (void)CSystem::instance()->pthread_mutex_unlock(&m_MsgQueueMutex);

    return SVS_STACK_OK;
}

int32_t CClientLink::getAddrByIndex(const uint32_t unHandleIndex, ADDR &stAddr)
{
    if (m_unLinkId != unHandleIndex)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to get address by index, the given index differs from the correct index. "
            "givenIndex=%d, correctIndex=%d.", unHandleIndex, m_unLinkId);
        return SVS_STACK_ERR_LINK_NOT_EXIST;
    }

    stAddr.unIp = m_stRemoteAddr.unIp;
    stAddr.usPort = m_stRemoteAddr.usPort;
    return SVS_STACK_OK;
}


/*****************************************************************************
 �� �� ��  : CClientLink.recvData
 ��������  : �������
 �������  : ��
 �� �� ֵ  : SVS_STACK_OK: �ɹ�
             SVS_STACK_FAIL: ���������߳�Ҫ����handleClose()��������Դ
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��6��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
int32_t CClientLink::handleRecv()
{
    // �ж����ӱ�ʶ�����Ѿ��������Ͳ��ٷ������
    if (CONN_FLAG_CONNECTED != m_ucConnFlag)
    {
        if (CONN_FLAG_CONNTIMEOUT == m_ucConnFlag)
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
                "No need to recv data, the connection to remote has timed out."
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, "
                "LinkIndex=%u, SockFd=%d.",
                m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort,
                m_unLinkId, m_nSockFd);

            // ����SVS_STACK_FAIL,��epoll�̵߳���handleClose()
            return SVS_STACK_FAIL;
        }

        return SVS_STACK_ERR_DISCONN;
    }

    if (NULL == m_pszRecvBuff)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to recv data from remote, the recv buffer pointer is NULL.");

        // ����SVS_STACK_FAIL,��epoll�̵߳���handleClose()
        return SVS_STACK_FAIL;
    }

    // ���յ���ݣ��������ʱ����
    m_ucHeartbeatTimes = 0;

    // �������
    char *pBuffPos = m_pszRecvBuff + m_nUsedBuffLength; // ��ʼ������ݵĻ������ַ
    size_t nRemainBuffLen = (size_t)(MAX_RECV_BUFF - m_nUsedBuffLength); // ���Խ��յ���ݵ��ܴ�С
    if (0 == nRemainBuffLen)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_EMR,
                "Fail to recv data, recv buffer is full. "
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, "
                "SockFd=%d, LinkIndex=%u",
                m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort,
                m_nSockFd, m_unLinkId);
        return SVS_STACK_OK;
    }
    errno = 0; // ������errno
    ssize_t nRecvResult = CSystem::instance()->recv(m_nSockFd, pBuffPos, nRemainBuffLen, 0);
    if(0 >= nRecvResult)
    {
        // �жϽ���ʧ�ܴ�����, �����Դ���⣬������(return 0)
        if (bIsSockOK())
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_INF,
                "Fail to recv data from remote, the resource is busy, will try again. "
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, SockFd=%d, LinkIndex=%u.",
                m_szLocalIp, m_stLocalAddr.usPort,
                m_szRemoteIp, m_stRemoteAddr.usPort, m_nSockFd, m_unLinkId);

            return SVS_STACK_OK;
        }

        // ֪ͨҵ�������¼�
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_EMR,
            "Fail to recv data from remote, maybe the remote has close the connection. "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, "
            "SockFd=%d, LinkIndex=%u, recvLength=%d, error=%d.",
            m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort,
            m_nSockFd, m_unLinkId, nRecvResult, errno);

        // ����SVS_STACK_FAIL,��epoll�̵߳���handleClose()
        return SVS_STACK_FAIL;
    }

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
        "Recv data from remote, RecvSize=%d, LocalIp=%s, LocalPort=%d, "
        "RemoteIp=%s, RemotePort=%d, SockFd=%d, LinkIndex=%u.",
        nRecvResult, m_szLocalIp, m_stLocalAddr.usPort,
        m_szRemoteIp, m_stRemoteAddr.usPort, m_nSockFd, m_unLinkId);

    // ��Ľ�����ݳ���
    m_nUsedBuffLength += nRecvResult;

    // �������
    uint32_t unUnProcRecvLen = 0; //��ʣ�������û�н���
    if (SVS_STACK_OK != parseData(unUnProcRecvLen))
    {
        // ������ʧ�ܣ�����ʧ��
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to parse recv data. LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, "
            "SockFd=%d, LinkIndex=%u.",
            m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort,
            m_nSockFd, m_unLinkId);

        // ������Ϣʧ�ܣ������������
        m_nUsedBuffLength = 0;

        //1 todo, ������Ϣʧ�ܣ��Ƿ���Ҫ�����Ӷϵ�����֪ͨҵ���
        // ����SVS_STACK_FAIL,��epoll�̵߳���handleClose()
        return SVS_STACK_FAIL;
    }

    pBuffPos = m_pszRecvBuff + ((uint32_t)m_nUsedBuffLength - unUnProcRecvLen);
    (void)CSystem::instance()->memmove(m_pszRecvBuff, pBuffPos, unUnProcRecvLen);//û�н�������ݱ���
    m_nUsedBuffLength = (int32_t)unUnProcRecvLen;

    return SVS_STACK_OK;
}

/*****************************************************************************
 �� �� ��  : CClientLink.handleSend
 ��������  : �������
 �������  : ��
 �� �� ֵ  : SVS_STACK_OK: �ɹ�
             SVS_STACK_FAIL: ���������߳�Ҫ����handleClose()��������Դ
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��6��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
int32_t CClientLink::handleSend()
{
    if (CONN_FLAG_CONNECTING == m_ucConnFlag)
    {
        int32_t nRet = checkConnectStatus();
        if (SVS_STACK_FAIL == nRet)
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
                "Fail to connect to remote, LocalIp=%s, LocalPort=%d, "
                "RemoteIp=%s, RemotePort=%d, SockFd=%d, LinkIndex=%u.",
                m_szLocalIp, m_stLocalAddr.usPort,
                m_szRemoteIp, m_stRemoteAddr.usPort,
                m_nSockFd, m_unLinkId);

            return SVS_STACK_FAIL;
        }

        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_INF,
            "Connect to remote success, LocalIp=%s, LocalPort=%d, "
            "RemoteIp=%s, RemotePort=%d, SockFd=%d, LinkIndex=%u.",
            m_szLocalIp, m_stLocalAddr.usPort,
            m_szRemoteIp, m_stRemoteAddr.usPort,
            m_nSockFd, m_unLinkId);

        setConnFlag(CONN_FLAG_CONNECTED);
        m_tLastSendHeartbeatTime = time(NULL);
        m_tLastRecvHeartbeatTime = time(NULL);

        CLinkFactory::getInstance()->addLinkForTimeout(this);

        struct epoll_event epEvent;
        epEvent.data.ptr = (void*)this;
        epEvent.events = EPOLLIN;
        (void)CLinkFactory::getInstance()->modEpollEvent(m_nSockFd, &epEvent);

        notifyConnection(EVENT_TYPE_CONNECTED);
        return SVS_STACK_OK;
    }

    if (CONN_FLAG_CONNECTED != m_ucConnFlag)
    {
        if (CONN_FLAG_CONNTIMEOUT == m_ucConnFlag)
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
                "No need to send data, the connection to remote has timed out."
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, "
                "LinkIndex=%u, SockFd=%d.",
                m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort,
                m_unLinkId, m_nSockFd);

            return SVS_STACK_FAIL;
        }

        return SVS_STACK_OK;
    }

    struct epoll_event epEvent;
    epEvent.data.ptr = (void*)this;
    epEvent.events = EPOLLIN;
    (void)CLinkFactory::getInstance()->modEpollEvent(m_nSockFd, &epEvent);

    char* pszMsg = NULL;
    SENDMSGHEAD* pstSendHead = NULL;
    while (!m_MsgQueue.empty())
    {
        (void)CSystem::instance()->pthread_mutex_lock(&m_MsgQueueMutex);
        pstSendHead = m_MsgQueue.front();
        (void)CSystem::instance()->pthread_mutex_unlock(&m_MsgQueueMutex);
        if (NULL == pstSendHead)
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
                "Fail to send message, the message pointer is NULL."
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, "
                "SockFd=%d, LinkIndex=%u.",
                m_szLocalIp, m_stLocalAddr.usPort,
                m_szRemoteIp, m_stRemoteAddr.usPort,
                m_nSockFd, m_unLinkId);

            (void)CSystem::instance()->pthread_mutex_lock(&m_MsgQueueMutex);
            m_MsgQueue.pop_front();
            (void)CSystem::instance()->pthread_mutex_unlock(&m_MsgQueueMutex);
            continue;
        }

        pszMsg = (char*)pstSendHead;
        SVS_MSG_HEADER* pstHead = (SVS_MSG_HEADER*)(pszMsg + SENDMSGHEAD_SIZE);/*lint !e826*/ // ָ��ת��Ϊ��Ӧ�Ľṹ��
        uint32_t unRealSendSize = 0;
        uint32_t unSendSize = pstHead->PacketLength - pstSendHead->unSendSize;// ������Ϣ����
        int32_t iRet = sendMsgToPeer((pszMsg + SENDMSGHEAD_SIZE +pstSendHead->unSendSize), unSendSize, unRealSendSize);
        if (SVS_STACK_OK != iRet)
        {
            // ������ʧ�ܣ�˵�������Ѿ��Ͽ�������SVS_STACK_FAIL,��epoll�̵߳���handleClose()
            return SVS_STACK_FAIL;
        }

        // �޸��Ѿ����͵���ݴ�С
        pstSendHead->unSendSize += unRealSendSize;

        // ��δ������ϣ��ȴ��´��ٷ������ôӶ�����ȡ������Ϣ
        if (pstSendHead->unSendSize < pstHead->PacketLength)
        {
            // �޸�epoll��ע���¼��������д�¼�
            epEvent.events = (uint32_t)(EPOLLIN | EPOLLOUT);/*lint !e655*/// int����ֵ��֮��ֵ
            (void)CLinkFactory::getInstance()->modEpollEvent(m_nSockFd, &epEvent);

            return SVS_STACK_OK;
        }

        // ȫ��������ϣ��Ӷ�����ɾ��ö���
        (void)CSystem::instance()->pthread_mutex_lock(&m_MsgQueueMutex);
        m_MsgQueue.pop_front();
        (void)CSystem::instance()->pthread_mutex_unlock(&m_MsgQueueMutex);

        // ���÷���ʱ��
        pstSendHead->tSendTime = time(NULL);

        if (bWaitRespMsg(pstSendHead->unTimeout, pstHead->TransactionNo))
        {
            // ����Ϣ��Ҫ�ȴ���Ӧ��������Ϣ�ŵ���ʱ���map��
            addWaitRespMsg(pstHead->TransactionNo, pstSendHead);
        }
        else
        {
            // ���͵���Ϣ����Ҫ����Ӧ�������Ѿ�������ϣ�ɾ�����Ϣ
            delete []pszMsg;
            pszMsg = NULL;
        }
    }

    return SVS_STACK_OK;
}

/*****************************************************************************
 �� �� ��  : CClientLink.handleTimeout
 ��������  : ��������
 �������  : ��
 �� �� ֵ  : SVS_STACK_OK: �ɹ�
             SVS_STACK_FAIL: ���������߳�Ҫ����handleClose()��������Դ
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��11��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
void CClientLink::handleTimeout()
{
    // �ж����ӱ�ʶ�����Ѿ��������Ͳ��ټ������
    if (CONN_FLAG_CONNECTED != m_ucConnFlag)
    {
//        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
//            "No need to check heartbeat, the connection to remote has disconnected."
//            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, LinkIndex=%u, SockFd=%d.",
//            m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort,
//            m_unLinkId, m_nSockFd);

        return;
    }

    // �ж������Ƿ�ʱ���Ƿ���Ҫ��Է�������
    int32_t iRet = checkHearbeatMsg();
    if (SVS_STACK_OK != iRet)
    {
        return;
    }

    // �ж��Ƿ�����Ӧ��ʱ����Ϣ�����У�֪ͨҵ���
    checkRespMsg();

    return;
}

/*****************************************************************************
 �� �� ��  : CClientLink.handleClose
 ��������  : �ر�����
 �������  : ��
 �� �� ֵ  : int32_t CClientLink::
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��6��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
void CClientLink::handleClose()
{
    // �жϸ�socket����Ƿ��Ѿ��ͷţ����Ѿ��ͷţ�����������
    if (CONN_FLAG_CLOSED == m_ucConnFlag)
    {
        return;
    }

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
        "Handle close, SockFd=%d, LinkIndex=%u, pLink=%p, "
        "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d.",
        m_nSockFd, m_unLinkId, this, m_szLocalIp, m_stLocalAddr.usPort,
        m_szRemoteIp, m_stRemoteAddr.usPort);

    // �ȼ�¼�µ�ǰ���ӵ�״̬
    uint8_t ucConnFlag = m_ucConnFlag;

    shutdownSock();

//    // ��������״̬
//    setConnFlag(CONN_FLAG_CLOSED);
//
//    // ע���epoll�¼�
//    struct epoll_event epEvent;
//    memset(&epEvent, 0, sizeof(epEvent));
//    epEvent.data.ptr = (void*)this;
//    epEvent.events = (uint32_t)(EPOLLIN | EPOLLOUT); /*lint !e655*/// int����ֵ��֮��ֵ
//    int32_t nRet = CLinkFactory::getInstance()->delEpollEvent(m_nSockFd, &epEvent);
//    if (SVS_STACK_OK != nRet)
//    {
//        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
//            "Fail to delete epoll event when close the socket. SockFd=%d, LinkIndex=%u, "
//            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, retCode=0x%x.",
//            m_nSockFd, m_unLinkId, m_szLocalIp, m_stLocalAddr.usPort,
//            m_szRemoteIp, m_stRemoteAddr.usPort, nRet);
//    }
//
//    // �Ͽ����Ӳ��ر�socket�������sockFdΪ��Чֵ
//    (void)CSystem::instance()->shutdown(m_nSockFd, SHUT_RDWR);
//    (void)CSystem::instance()->close(m_nSockFd);
//    m_nSockFd = INVALID_SOCK_FD;

    if (CONN_FLAG_CONNTIMEOUT == ucConnFlag)
    {
        // ��֮ǰ��״̬Ϊ����ʱ��֪ͨ����ʱ�¼�
        notifyConnection(EVENT_TYPE_TIMEOUT);
    }
    else
    {
        // ������Ϊ�����¼�֪ͨ
        notifyConnection(EVENT_TYPE_DISCONNECT);
    }

    return;
}

/*****************************************************************************
 �� �� ��  : CClientLink.parseData
 ��������  : �����Ѿ����յ������
 �������  : ��
 �� �� ֵ  : int32_t CClientLink::
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��6��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
int32_t CClientLink::parseData(uint32_t &unUnProcRecvLen)
{
    unUnProcRecvLen = (uint32_t)m_nUsedBuffLength;
    if (NULL == m_pszRecvBuff)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to parse recv data, the rece buffer is NULL, "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, "
            "LinkIndex=%u, SockFd=%d.",
            m_szLocalIp, m_stLocalAddr.usPort,
            m_szRemoteIp, m_stRemoteAddr.usPort,
            m_unLinkId, m_nSockFd);

        return SVS_STACK_FAIL;
    }

    // ��ݲ�����Ϣͷ���ȣ������ȡ���
    if ((uint32_t)m_nUsedBuffLength < SVS_MSG_HEADER_SIZE)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
            "No need to parse recv data, the total recv length is less than SVS head size. "
            "SvsHeadSize=%d, TotalRecvLength=%d, LinkIndex=%u, SockFd=%d, "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d.",
            SVS_MSG_HEADER_SIZE, m_nUsedBuffLength, m_unLinkId, m_nSockFd,
            m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort);

        return SVS_STACK_OK;
    }

    // �ֽ��ÿ����Ϣ
    char *pBuffPos = m_pszRecvBuff;
    for( ; ; )
    {
        SVS_MSG_HEADER  *pMsgHead = reinterpret_cast<SVS_MSG_HEADER *>(pBuffPos);
        if(NULL == pMsgHead)
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
                "Fail to parse recv data, can not cast to Svs message, LinkIndex=%u, SockFd=%d, "
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d.",
                m_unLinkId, m_nSockFd,
                m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort);

            return SVS_STACK_FAIL;
        }

        // �ж���Ϣͷ����Ϣ��С�Ƿ�Ϸ�
        if(SVS_MSG_HEADER_SIZE > pMsgHead->PacketLength)
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
                "Fail to parse recv data, the message packet length is invalid."
                "MsgPackLength=%d, TotalRecvLength=%d, MsgType=0x%x, "
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, "
                "LinkIndex=%u, SockFd=%d.",
                pMsgHead->PacketLength, unUnProcRecvLen, pMsgHead->MsgType,
                m_szLocalIp, m_stLocalAddr.usPort,
                m_szRemoteIp, m_stRemoteAddr.usPort,
                m_unLinkId, m_nSockFd);

            return SVS_STACK_ERR_MSG_LENGTH_INVALID;
        }

        // δ������ݳ��ȣ�����1����Ϣ�ĳ���
        if (unUnProcRecvLen < pMsgHead->PacketLength)
        {
            break;
        }

        // �жϰ汾���Ƿ�Ϸ�
        if (SVS_MSG_PROTOCOL_VERSION != pMsgHead->ProtocolVersion)
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
                "Fail to parse recv data, the protocol version is invalid."
                "proVersion=0x%x, validProVersion=0x%x, MsgType=0x%x, "
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d.",
                pMsgHead->ProtocolVersion, SVS_MSG_PROTOCOL_VERSION, pMsgHead->MsgType,
                m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort);

            return SVS_STACK_ERR_PARSE_MSG;
        }

        if (SVS_MSG_TYPE_HEARTBEAT == pMsgHead->MsgType)
        {
            m_tLastRecvHeartbeatTime = time(NULL);
        }
        else
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
                "Recv message from remote, MsgType=0x%x, MsgLength=%d, TranNo=0x%x, "
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, "
                "SockFd=%d, LinkIndex=%u.",
                pMsgHead->MsgType, pMsgHead->PacketLength, pMsgHead->TransactionNo,
                m_szLocalIp, m_stLocalAddr.usPort,
                m_szRemoteIp, m_stRemoteAddr.usPort, m_nSockFd, m_unLinkId);

            // ������Ϣ�������Ѿ������ɹ���֪ͨҵ��㴦�����Ϣ
            Result stResult;
            stResult.ucResultType = RESULT_TYPE_RECV;
            stResult.nResult = SVS_STACK_OK;
            m_pMsgHandler->handleMessage(pBuffPos, pMsgHead->PacketLength, m_unLinkId, stResult);

            // �ӵȴ���Ӧ��Ϣ��map����ɾ��
            delWaitRespMsg(pMsgHead->TransactionNo);
        }

        pBuffPos = pBuffPos + pMsgHead->PacketLength;
        unUnProcRecvLen -= pMsgHead->PacketLength;

        // δ������ݳ��ȣ�����1����Ϣͷ�ĳ���
        if(unUnProcRecvLen < SVS_MSG_HEADER_SIZE)
        {
            break;
        }

    }

    return SVS_STACK_OK;
}

void CClientLink::notifyConnection(const uint16_t usEventType)
{
    // ����ʧ�ܣ���ʱ�Ѿ�������֪ͨӦ�ò�����¼�
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Notify connection event, EventType=%d[0:Disconn, 1:Conn, 2:Timeout], "
        "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, SockFd=%d, LinkIndex=%u.",
        usEventType, m_szLocalIp, m_stLocalAddr.usPort,
        m_szRemoteIp, m_stRemoteAddr.usPort, m_nSockFd, m_unLinkId);

    // ֪ͨҵ��������¼�
    ConnEvent stEvent;
    stEvent.EventTime = time(NULL);
    stEvent.EventType = usEventType;
    stEvent.stPeerAddr.unIp = m_stRemoteAddr.unIp;
    stEvent.stPeerAddr.usPort = m_stRemoteAddr.usPort;
    stEvent.unHandleIndex = m_unLinkId;
    m_pMsgHandler->handleConnEvent(&stEvent);

    // �ж���Ϊ�����������Ͷ�����ʣ�����Ϣ֪ͨ��ҵ���
    if (EVENT_TYPE_DISCONNECT == usEventType || EVENT_TYPE_TIMEOUT == usEventType)
    {
        notifyLeftMessage();
        notifyTimeoutMessage();
    }

    return;
}

// ������������ʣ�����Ϣ֪ͨ��ҵ��㣬���ͷ���ռ�õ��ڴ�
void CClientLink::notifyLeftMessage()
{
    char* pszMsg = NULL;    // ָ�򻺳����׵�ַ
    SVS_MSG_HEADER* pstHead = NULL;
    SENDMSGHEAD*    pstSendMsgHead = NULL;
    Result stResult;
    stResult.ucResultType = RESULT_TYPE_SEND;
    stResult.nResult = SVS_STACK_ERR_DISCONN;

    while (!m_MsgQueue.empty())
    {
        // �Ӷ�����ȡ����Ϣ���ٷ��ͣ�ȥ��Ϣ����Ҫ�����ֹ���̷߳���
        (void)CSystem::instance()->pthread_mutex_lock(&m_MsgQueueMutex);
        pstSendMsgHead = m_MsgQueue.front();
        m_MsgQueue.pop_front();
        (void)CSystem::instance()->pthread_mutex_unlock(&m_MsgQueueMutex);

        if (NULL == pstSendMsgHead)
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
                "Fail to send message, the message pointer is NULL");
            continue;
        }

        pszMsg = (char*)pstSendMsgHead;
        pstHead = (SVS_MSG_HEADER*)(pszMsg + SENDMSGHEAD_SIZE);/*lint !e826*/ // ָ��ת��Ϊ��Ӧ�Ľṹ��
        m_pMsgHandler->handleMessage((pszMsg + SENDMSGHEAD_SIZE), pstHead->PacketLength, m_unLinkId, stResult);

        delete []pszMsg;
        pszMsg = NULL;
    }

    // ���һ������Ϣ����֪ҵ��㣬����δ���͵���Ϣ�Ѿ�ȫ���������
    m_pMsgHandler->handleMessage(NULL, 0, m_unLinkId, stResult);

    return;
}

// ���ȴ���Ӧ��Ϣ��map�е���Ϣ֪ͨ��ҵ��㣬���ͷ���ռ�õ��ڴ�
void CClientLink::notifyTimeoutMessage()
{
    char* pszMsg = NULL;// ָ�򻺳����׵�ַ
    SVS_MSG_HEADER* pstHead = NULL;
    Result stResult;
    stResult.ucResultType = RESULT_TYPE_TIMEOUT;
    stResult.nResult = SVS_STACK_OK;

    (void)CSystem::instance()->pthread_rwlock_rdlock(&m_WaitRespMsgMapLock);
    WAIT_RESPMSG_MAP_ITOR itor = m_WaitRespMsgMap.begin();
    while (itor != m_WaitRespMsgMap.end())
    {
        pszMsg = (char*) (itor->second);
        if (NULL == pszMsg)
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
                    "Fail to notify timeout message when disconnect, the message pointer is NULL");
            continue;
        }

        pstHead = (SVS_MSG_HEADER*) (pszMsg + SENDMSGHEAD_SIZE);/*lint !e826*/// ָ��ת��Ϊ��Ӧ�Ľṹ��
        m_pMsgHandler->handleMessage((pszMsg + SENDMSGHEAD_SIZE),  pstHead->PacketLength, m_unLinkId, stResult);

        delete[] pszMsg;
        pszMsg = NULL;

        m_WaitRespMsgMap.erase(itor);
        itor = m_WaitRespMsgMap.begin();

    }
    (void)CSystem::instance()->pthread_rwlock_unlock(&m_WaitRespMsgMapLock);

    // ���һ������Ϣ����֪ҵ��㣬����δ���յ���Ӧ����Ϣ���Ѿ�֪ͨ���
    m_pMsgHandler->handleMessage(NULL, 0, m_unLinkId, stResult);

    return;
}

// ����socket������
void CClientLink::setSocketOption()
{
    // ���õ�ַ���ñ�ʶ
    int32_t nReuseAddr = 1;
    if (0 > CSystem::instance()->setsockopt(m_nSockFd, SOL_SOCKET, SO_REUSEADDR, (void*)&nReuseAddr, sizeof(nReuseAddr)))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to set client's socket option, set address reuse failed, SockFd=%d.",
            m_nSockFd);
    }

    // ���ý��ջ������С
    int32_t nSize = MAX_SOCKET_BUFFSIZE;
    if (0 > CSystem::instance()->setsockopt(m_nSockFd, SOL_SOCKET, SO_RCVBUF, (void*)&nSize, sizeof(int32_t)))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to set client's socket option, set recv buffer size failed. "
            "SockFd=%d, SetRecvBuffSize=%d.", m_nSockFd, nSize);
    }

    // ���÷��ͻ������С
    nSize =  MAX_SOCKET_BUFFSIZE;
    if (0 > CSystem::instance()->setsockopt(m_nSockFd, SOL_SOCKET, SO_SNDBUF, (void*)&nSize, sizeof(int32_t)))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to set client's socket option, set send buffer size failed. "
            "SockFd=%d, SetSendBuffSize=%d.", m_nSockFd, nSize);
    }

    // ��������Ϊ������ʽ
    if (0 > fcntl(m_nSockFd, F_SETFL, fcntl(m_nSockFd, F_GETFL)|O_NONBLOCK))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to set client's socket option, set nonblock failed, SockFd=%d.", m_nSockFd);
    }

    return;
}

// ���ø���������
void CClientLink::setLinkType(const uint8_t ucLinkType)
{
    m_ucLinkType = ucLinkType;
    return;
}

// ���øÿͻ������ӵ�socket���
void CClientLink::setSockFd(int32_t nSockFd)
{
    m_nSockFd = nSockFd;
    return;
}

// ��������״̬
void CClientLink::setConnFlag(const uint8_t ucConnFlag)
{
    // ����ǰ�Ѿ�Ϊ�ر�״̬���Ͳ�����������״̬
    if (CONN_FLAG_CLOSED == m_ucConnFlag)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Can not set connection flag, the current connection flag is already closed. "
            "CurFlag=%u, SetFlag=%u.",
            m_ucConnFlag, ucConnFlag);

        return;
    }

    m_ucConnFlag = ucConnFlag;
    return;
}

// ��������ʼ��ʱʱ��
void CClientLink::setHeartbeatTime()
{
    m_tLastSendHeartbeatTime = time(NULL);
    m_tLastRecvHeartbeatTime = m_tLastSendHeartbeatTime;
    return;
}

// �������״̬����ʧ�ܷ���-1�����ⲿ����handleClose()
int32_t CClientLink::checkConnectStatus()
{
    int32_t lErrorNo = 0;
    socklen_t len = sizeof(lErrorNo);
    if (CSystem::instance()->getsockopt(m_nSockFd, SOL_SOCKET, SO_ERROR, (void*)&lErrorNo, &len) < 0)
    {
        setConnFlag(CONN_FLAG_CLOSING);
        return SVS_STACK_FAIL;
    }

    if (lErrorNo != 0)
    {
        setConnFlag(CONN_FLAG_CLOSING);
        return SVS_STACK_FAIL;
    }

    setConnFlag(CONN_FLAG_CONNECTED);
    return SVS_STACK_OK;
}

/*****************************************************************************
 �� �� ��  : CClientLink.sendMsgToPeer
 ��������  : ������Ϣ���Զˣ�������Ϊ��������ô����-1��֪ͨ�����������Ѿ��Ͽ�
 �������  : unRealSendSize��ʶʵ�ʷ�����Ϣ��С
 �� �� ֵ  : int32_t CClientLink::
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��19��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
int32_t CClientLink::sendMsgToPeer(const char* pszMsgBuff,
                                    uint32_t unSendSize,
                                    uint32_t &unRealSendSize)
{
    SVS_MSG_HEADER* pstHead = (SVS_MSG_HEADER*)pszMsgBuff;/*lint !e826*/ // ָ��ת��Ϊ��Ӧ�Ľṹ��
    unRealSendSize = 0;

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
        "Send message to remote, MsgType=0x%x, MsgLength=%d, TransNo=0x%x.",
        pstHead->MsgType, pstHead->PacketLength, pstHead->TransactionNo);

    errno = 0; // ������errno
    int32_t nSendRet = CSystem::instance()->send(m_nSockFd, pszMsgBuff, unSendSize, 0);
    if (nSendRet < 0)
    {
        // ����ʧ�ܣ����η��ͽ���ȴ��´η���
        if (bIsSockOK())
        {
            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
                "Fail to send message to remote this time, maybe the resource is busy, "
                "will try again. MsgType=0x%x, MsgLength=%d, MsgTranNo=0x%x, "
                "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, SockFd=%d.",
                pstHead->MsgType, pstHead->PacketLength, pstHead->TransactionNo,
                m_szLocalIp, m_stLocalAddr.usPort,
                m_szRemoteIp, m_stRemoteAddr.usPort, m_nSockFd);

            return SVS_STACK_OK;
        }

        // ����ʧ�ܺ󣬷���SVS_STACK_FAIL, �ϲ�����handleClose()���д���
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to send message to remote this time, the remote has close the connection. "
            "MsgType=0x%x, MsgLength=%d, MsgTranNo=0x%x, "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, SockFd=%d.",
            pstHead->MsgType, pstHead->PacketLength, pstHead->TransactionNo,
            m_szLocalIp, m_stLocalAddr.usPort,
            m_szRemoteIp, m_stRemoteAddr.usPort, m_nSockFd);

        return SVS_STACK_FAIL;
    }

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
        "Send message to remote this time, MsgType=0x%x, MsgLength=%d, MsgTranNo=0x%x, "
        "SendSize=%d, LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, SockFd=%d.",
        pstHead->MsgType, pstHead->PacketLength, pstHead->TransactionNo,
        nSendRet, m_szLocalIp, m_stLocalAddr.usPort,
        m_szRemoteIp, m_stRemoteAddr.usPort, m_nSockFd);

    unRealSendSize = (uint32_t)nSendRet;
    return SVS_STACK_OK;
}

void CClientLink::addWaitRespMsg(uint32_t unTransNo, SENDMSGHEAD* pstSendMsgHead)
{
    SVS_MSG_HEADER* pstMsgHead = (SVS_MSG_HEADER*)(((char*)pstSendMsgHead) + SENDMSGHEAD_SIZE);/*lint !e826*/// ת��Ϊ��Ӧ�Ľṹ��
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
        "Add wait response message to map, TransNo=0x%x, MsgType=0x%x, Timeout=%d, LinkIndex=%u.",
        unTransNo, pstMsgHead->MsgType, pstSendMsgHead->unTimeout, m_unLinkId);

    (void)CSystem::instance()->pthread_rwlock_wrlock(&m_WaitRespMsgMapLock);
    m_WaitRespMsgMap.insert(WAIT_RESPMSG_MAP_PAIR(unTransNo, pstSendMsgHead));
    (void)CSystem::instance()->pthread_rwlock_unlock(&m_WaitRespMsgMapLock);
    return;
}

// ��ݴ����ţ�ɾ���Ӧ��������Ϣ
void CClientLink::delWaitRespMsg(uint32_t unTransNo)
{
    // �ж��Ƿ�Ϊ��Ӧ��Ϣ������Ϊ��Ӧ��Ϣ��ֱ�ӷ���
    if ((unTransNo & RESP_MSG_TRANSNO_MASK) != RESP_MSG_TRANSNO_MASK)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
            "No need to delete the message from wait response map, "
            "the message is not response message. TransNo=0x%x.", unTransNo);
        return;
    }

    // Ϊ��Ӧ��Ϣ����ԭΪԭʼ��������Ϣ�����ţ�Ȼ��ӵȴ�map��ɾ��
    uint32_t unReqTransNo = (unTransNo & REQ_MSG_TRANSNO_MASK);
    (void)CSystem::instance()->pthread_rwlock_wrlock(&m_WaitRespMsgMapLock);
    WAIT_RESPMSG_MAP_ITOR itor = m_WaitRespMsgMap.find(unReqTransNo);
    if (itor != m_WaitRespMsgMap.end())
    {
        char* pszMsg = (char*)(itor->second);
        SVS_MSG_HEADER* pstMsgHead = (SVS_MSG_HEADER*)(pszMsg + SENDMSGHEAD_SIZE);/*lint !e826*/// ת��Ϊ��Ӧ�Ľṹ��
        m_WaitRespMsgMap.erase(unReqTransNo);

        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_INF,
            "Delete the message from wait response map, "
            "MsgType=0x%x, TransNo=0x%x, MsgLength=%u, LinkIndex=%u, pszMsgAddr=0x%08x.",
            pstMsgHead->MsgType, pstMsgHead->TransactionNo, pstMsgHead->PacketLength,
            m_unLinkId, pszMsg);

        delete []pszMsg;
        pszMsg = NULL;
    }
    (void)CSystem::instance()->pthread_rwlock_unlock(&m_WaitRespMsgMapLock);

    return;
}

// ������õ���Ӧ��Ϣ��ʱʱ�䡢�����ţ��ж��Ƿ���Ҫ�ȴ���Ӧ��Ϣ
bool CClientLink::bWaitRespMsg(uint32_t unTimeout, uint32_t unTransNo)const
{
    if (0 == unTimeout)
    {
        return false;
    }

    if ((unTransNo | REQ_MSG_TRANSNO_MASK) == REQ_MSG_TRANSNO_MASK)
    {
        return true;
    }

    return false;
}

// ��������Ƿ�ʱ���Ƿ���Ҫ��Է�������
int32_t CClientLink::checkHearbeatMsg()
{
    // �жϾ����ϴν��������Ƿ�ʱ��ע�⿼���û��޸���ϵͳʱ������
    time_t tCurTime = time(NULL);
    int32_t nRecvTimeRet = abs(tCurTime - m_tLastRecvHeartbeatTime);
    uint32_t unHearbeatInterval = CLinkFactory::getInstance()->getHeartBeatInterval();
    if (nRecvTimeRet >
          (int32_t)((unHearbeatInterval - HEARTBEAT_THREAD_WORK_INTERVAL) * HEARTBEAT_TIMEOUT_TIMES))
    {
        // ���û���ϵͳʱ���޸ģ����ҳ���������ʱ���ʱ������ô����һ�����һ�ν�������ʱ��
        m_tLastRecvHeartbeatTime = tCurTime;
    }
    else if (nRecvTimeRet > (int32_t)(unHearbeatInterval - HEARTBEAT_THREAD_WORK_INTERVAL))
    {
        m_ucHeartbeatTimes++;
        m_tLastRecvHeartbeatTime = tCurTime;
    }

    // �ﵽ���ʱ����֪ͨҵ��㣬��������δ���͵���Ϣ�׸�ҵ���
    if(m_ucHeartbeatTimes > HEARTBEAT_TIMEOUT_TIMES)
    {
        char szLastRecvTime[DATE_TIME_LEN] = {0};
        char szCurTime[DATE_TIME_LEN] = {0};
        time2string(m_tLastRecvHeartbeatTime, szLastRecvTime, DATE_TIME_LEN);
        time2string(tCurTime, szCurTime, DATE_TIME_LEN);

        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "The heartbeat timeout times has reached the max[%d]. "
            "lastRecvHeartbeatTime=%s, currentTime=%s, TimeoutTimes=%d, "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, "
            "LinkIndex=%u, SockFd=%d.",
            HEARTBEAT_TIMEOUT_TIMES,
            szLastRecvTime,
            szCurTime,
            m_ucHeartbeatTimes,
            m_szLocalIp,
            m_stLocalAddr.usPort,
            m_szRemoteIp,
            m_stRemoteAddr.usPort,
            m_unLinkId,
            m_nSockFd);

        // ��������״̬�����Ͽ�sock����
        closeSock(CONN_FLAG_CONNTIMEOUT);

        return SVS_STACK_FAIL;
    }

    // ����Ƿ���Ҫ����������Ϣ��ע�⿼���û��޸�ϵͳʱ������
    int32_t nSendTimeRet = tCurTime - m_tLastSendHeartbeatTime;
    if ((nSendTimeRet > (int32_t)(unHearbeatInterval - HEARTBEAT_THREAD_WORK_INTERVAL)) || (nSendTimeRet <= 0))
    {
        // ���Ѿ������˷��������ʱ�䣬
        // �����û���ϵͳʱ��Ĵ󣬲�����һ�����������ڣ�
        // �����û���ϵͳʱ���С����ô����һ��������Ϣ
        SVS_MSG_HEARTBEAT stHeartMsg;
        memset(&stHeartMsg, 0x0, SVS_MSG_HEARTBEAT_SIZE);
        stHeartMsg.MsgHeader.ProtocolVersion = SVS_MSG_PROTOCOL_VERSION;   // Э������
        stHeartMsg.MsgHeader.MsgType = SVS_MSG_TYPE_HEARTBEAT;
        stHeartMsg.MsgHeader.TransactionNo = 0; // ����������Ϣ��Э��ջ�ڲ����?����������0
        stHeartMsg.MsgHeader.PacketLength = SVS_MSG_HEARTBEAT_SIZE;

        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
            "SvsStack begin to send heartbeat message to remote, "
            "MsgType=0x%x, MsgLength=%d, SockFd=%d, LinkIndex=%u,"
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d.",
            stHeartMsg.MsgHeader.MsgType, stHeartMsg.MsgHeader.PacketLength, m_nSockFd, m_unLinkId,
            m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort);

        m_tLastSendHeartbeatTime = tCurTime;
        (void)sendMsg(m_unLinkId, (char*)&stHeartMsg, stHeartMsg.MsgHeader.PacketLength, 0);
    }

    return SVS_STACK_OK;
}

// ����Ƿ��еȴ���Ӧ��ʱ����Ϣ�����У�֪ͨ��ҵ���
void CClientLink::checkRespMsg()
{
    char* pszMsg = NULL;
    SENDMSGHEAD* pstSendHead = NULL;
    SVS_MSG_HEADER* pstMsgHead = NULL;
    Result stResult;
    stResult.ucResultType = RESULT_TYPE_TIMEOUT;
    stResult.nResult = SVS_STACK_OK;
    time_t curTime = time(NULL);

//    std::list<SENDMSGHEAD*> TimeoutList;
    // ��ų�ʱ����Ϣ�Ĵ����
    std::list<uint32_t> TimeoutList;

    (void)CSystem::instance()->pthread_rwlock_rdlock(&m_WaitRespMsgMapLock);
    WAIT_RESPMSG_MAP_ITOR itor = m_WaitRespMsgMap.begin();
    while (itor != m_WaitRespMsgMap.end())
    {
        pstSendHead = itor->second;
        itor++;

        if (NULL == pstSendHead)
        {
            continue;
        }

        int32_t nTimeout = abs(pstSendHead->tSendTime - curTime);
        int32_t nHeartbeatInterval = (int32_t)CLinkFactory::getInstance()->getHeartBeatInterval();
        if (nTimeout > (nHeartbeatInterval * HEARTBEAT_TIMEOUT_TIMES))
        {
            // ���û��޸���ϵͳʱ�䣬Ŀǰֱ�Ӹ��·���ʱ�䣬����ҵ���֪ͨ��ʱ
            pstSendHead->tSendTime = curTime;
        }
        else if ((uint32_t)nTimeout > (pstSendHead->unTimeout - HEARTBEAT_THREAD_WORK_INTERVAL))
        {
            // ���Ѿ��ﵽ��ʱʱ�䣬֪ͨ��ҵ��㣬ͬʱ�ڳ�ʱ�б���ɾ�����
            pszMsg = (char*)pstSendHead;
            pstMsgHead = (SVS_MSG_HEADER*)(pszMsg + SENDMSGHEAD_SIZE);/*lint !e826*/ // ָ��ת��Ϊ��Ӧ�Ľṹ��
            m_pMsgHandler->handleMessage((char*)pstMsgHead, pstMsgHead->PacketLength, m_unLinkId, stResult);
            TimeoutList.push_back(pstMsgHead->TransactionNo);
        }

    }
    (void)CSystem::instance()->pthread_rwlock_unlock(&m_WaitRespMsgMapLock);

    // �ж��Ƿ�����Ҫɾ��ĵȴ���Ϣ
    if (TimeoutList.empty())
    {
        return;
    }

    (void)CSystem::instance()->pthread_rwlock_wrlock(&m_WaitRespMsgMapLock);
    for (std::list<uint32_t>::iterator listItor = TimeoutList.begin();
        listItor != TimeoutList.end();
        listItor++)
    {
        // Ҳ���ܸո��յ�����Ӧ��Ϣ����������ˣ����Բ�һ�����ҵ�����Ϣ����Ҫ�жϽ��
        uint32_t ulTransactionNo = (uint32_t)*listItor;
        itor = m_WaitRespMsgMap.find(ulTransactionNo);
        if(itor != m_WaitRespMsgMap.end())
        {
            pszMsg = (char*)(itor->second);
            m_WaitRespMsgMap.erase(ulTransactionNo);

            delete []pszMsg;
            pszMsg = NULL;
        }
    }
    (void)CSystem::instance()->pthread_rwlock_unlock(&m_WaitRespMsgMapLock);

    return;
}

bool CClientLink::bIsSockOK()const
{
    if ((EAGAIN == errno)
        || (ETIME == errno)
        || (EWOULDBLOCK == errno)
        || (ETIMEDOUT == errno)
        || (EINTR == errno))
    {
        return true;
    }

    return false;
}

// ��������״̬�����Ͽ�sock����
void CClientLink::closeSock(const uint8_t ucConnFlag)
{
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
        "The connection to remote occur error, set connection flag and shutdown connection. "
        "ConnFlag=%d[0:idle, 1:connecting, 2:connected, 3:conntimeout, 4:closing, 5:closed], "
        "LinkIndex=%u, SockFd=%d, LocalIp=%s, LocalPort=%d, ServerIp=%s, ServerPort=%d.",
        ucConnFlag, m_unLinkId, m_nSockFd,
        m_szLocalIp, m_stLocalAddr.usPort, m_szRemoteIp, m_stRemoteAddr.usPort);

    // ����������״̬
    setConnFlag(ucConnFlag);

    // �Ͽ����ӵ����رվ���ֹ������·��䣬ɾ��epoll�¼�ʱ������
    (void)CSystem::instance()->shutdown(m_nSockFd, SHUT_RDWR);
    return;
}

void CClientLink::time2string(time_t tTime, char* pszTime, uint32_t uiTimeLen)const
{
    if (NULL == pszTime)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to transmit time to string, the buffer pointer is NULL.");

        return;
    }

    struct tm tmv;
    (void)localtime_r(&tTime, &tmv);
    (void)snprintf(pszTime,
                 (size_t)uiTimeLen,
                 "%04d%02d%02d%02d%02d%02d",
                 tmv.tm_year + 1900,
                 tmv.tm_mon + 1,
                 tmv.tm_mday,
                 tmv.tm_hour,
                 tmv.tm_min,
                 tmv.tm_sec);

    return;
}

int32_t CClientLink::setSendMsgHead(SENDMSGHEAD* &pstSendMsgHead,
                                const uint32_t unBuffSize,
                                const char* pszOrigMsg,
                                const uint32_t unRealSendSize,
                                const uint32_t unTimeout,
                                const time_t tSendTime)
{
    char* pszMsgBuff = NULL;
    try
    {
        pszMsgBuff = new char[unBuffSize + SENDMSGHEAD_SIZE];
    }
    catch (...)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to alloc buffer for save send message,. "
            "LocalIp=%s, LocalPort=%d, RemoteIp=%s, RemotePort=%d, LinkIndex=%u, SockFd=%d.",
            m_szLocalIp, m_stLocalAddr.usPort,
            m_szRemoteIp, m_stRemoteAddr.usPort,
            m_unLinkId, m_nSockFd);

        return SVS_STACK_ERR_MEMORY_ALLOCATE_FAIL;
    }

    pstSendMsgHead = (SENDMSGHEAD*)pszMsgBuff;/*lint !e826*/ // ָ��ת��Ϊ��Ӧ�Ľṹ��
    pstSendMsgHead->unSendSize = unRealSendSize;
    pstSendMsgHead->unTimeout = unTimeout;
    pstSendMsgHead->tSendTime = tSendTime;
    (void)CSystem::instance()->memcpy((pszMsgBuff + SENDMSGHEAD_SIZE), pszOrigMsg, unBuffSize);

    return SVS_STACK_OK;
}/*lint !e429*/ // pszMsgBuff���յ���Ӧ��Ϣ������Ӧ��Ϣ��ʱʱ�����ͷ�

