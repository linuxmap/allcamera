#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "svs_vms_client_link.h"
#include "svs_vms_server_link.h"
#include "svs_vms_system_api.h"

CServerLink::CServerLink():ILink()
{
    // ��ʼ�����౾��ĳ�Ա����
    m_ucLinkType = LINK_TYPE_SERVER;
    m_unHandleCnt = 0;
    memset(&m_stLocalAddr, 0x0, sizeof(m_stLocalAddr));// �������ı��ؼ����ַ
    memset(m_szLocalIp, 0x0, STR_IP_LEN);
}/*lint !e1401*/// m_ConnMapLock, m_LinkMapLock'��initʱ���г�ʼ��

CServerLink::~CServerLink()
{
    // �����
    (void)pthread_rwlock_destroy(&m_ConnMapLock);
}

int32_t CServerLink::init(ISvsMsgHandler* pMsgHandler, uint32_t unLinkId, const ADDR* pstLocalAddr, const ADDR* pstRemoteAddr)
{
    if (NULL == pMsgHandler)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init server link, the message handler is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    if (NULL == pstLocalAddr)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init server link, the server address pointer is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    int32_t nRet = CSystem::instance()->pthread_rwlock_init(&m_ConnMapLock, NULL);
    if (SVS_STACK_OK != nRet)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init server link, init the thread lock failed.");

        return SVS_STACK_ERR_INIT_LOCK_FAIL;
    }

    m_pMsgHandler = pMsgHandler;
    m_unLinkId = unLinkId;
    m_stLocalAddr.unIp = pstLocalAddr->unIp;
    m_stLocalAddr.usPort = pstLocalAddr->usPort;

    struct in_addr stIp;
    stIp.s_addr = htonl(pstLocalAddr->unIp);
    strncpy(m_szLocalIp, inet_ntoa(stIp), STR_IP_LEN - 1);

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
        "Success to init server link, LinkIndex=%u, ServerIp=%s, ServerPort=%d.",
        m_unLinkId, m_szLocalIp, m_stLocalAddr.usPort);

    return SVS_STACK_OK;
}

/*****************************************************************************
 �� �� ��  : CServerLink.sendMsg
 ��������  : ������Ϣ�ӿڣ�Ŀǰ���첽���ͣ�ֻ�ǽ���Ϣ�ŵ���Ӧ���ӵķ��Ͷ���
 �������  : ��
 �� �� ֵ  : int32_t CServerLink::
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��11��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
int32_t CServerLink::sendMsg(const uint32_t unHandleIndex,
                            const char* pszMsg,
                            const uint32_t unMsgLength,
                            const uint32_t unTimeout)
{
    (void)CSystem::instance()->pthread_rwlock_rdlock(&m_ConnMapLock);
    CONN_ITOR itor = m_ConnMap.find(unHandleIndex);
    if (itor == m_ConnMap.end())
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to send message to client, can not find client by index. Index=%u.",
            unHandleIndex);

        (void)CSystem::instance()->pthread_rwlock_unlock(&m_ConnMapLock);
        return SVS_STACK_FAIL;
    }

    ILink* pClientLink = itor->second;
    if (NULL == pClientLink)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to send message to client, client pointer is NULL. Index=%u.",
            unHandleIndex);

        (void)CSystem::instance()->pthread_rwlock_unlock(&m_ConnMapLock);
        return SVS_STACK_FAIL;
    }
    (void)CSystem::instance()->pthread_rwlock_unlock(&m_ConnMapLock);

    int32_t iRet = pClientLink->sendMsg(unHandleIndex, pszMsg, unMsgLength, unTimeout);

    return iRet;
}

/*****************************************************************************
 �� �� ��  : CServerLink.startConn
 ��������  : �����������˼���
 �������  : ��
 �� �� ֵ  : int32_t CServerLink::
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��11��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
int32_t CServerLink::startConn()
{
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_INF,
        "Begin to start server listen, ServerIp=%s, ServerPort=%d.",
        m_szLocalIp, m_stLocalAddr.usPort);

    // ����������
    errno = 0; // ���errer code
    m_nSockFd = CSystem::instance()->socket(AF_INET, SOCK_STREAM, 0);
    if (m_nSockFd < 0)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to start server listen, create socket failed. "
            "LocalIp=%s, LocalPort=%d, errno=%d.",
            m_szLocalIp, m_stLocalAddr.usPort, errno);

        return SVS_STACK_FAIL;
    }

    // ����socket����
    setSocketOption();

    // �����ͼ����ַ���а�
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0x0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(m_stLocalAddr.unIp);
    serveraddr.sin_port = htons(m_stLocalAddr.usPort);
    errno = 0; // ���errer code
    if (0 != CSystem::instance()->bind(m_nSockFd, (sockaddr*)&serveraddr, sizeof(serveraddr)))/*lint !e740*///socket��ַ�����ָ�ʽ������
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to start server listen, bind socket failed. "
            "LocalIp=%s, LocalPort=%d, errno=%d.",
            m_szLocalIp, m_stLocalAddr.usPort, errno);

        return SVS_STACK_FAIL;
    }

    // ���÷�������������
    errno = 0; // ���errer code
    if (0 != CSystem::instance()->listen(m_nSockFd, MAX_RECV_BUFF))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to start server listen, listen on the sock failed."
            "listenFd=%d, LocalIp=%s, LocalPort=%d, errno=%d.",
            m_nSockFd, m_szLocalIp, m_stLocalAddr.usPort, errno);

        return SVS_STACK_FAIL;
    }

    // ע��epoll�¼�
    epoll_event epEvent;
    epEvent.data.ptr = (void*)this;
    epEvent.events = EPOLLIN;    // ����Ҫ������¼�����
    int32_t nRet = CLinkFactory::getInstance()->addEpollEvent(m_nSockFd, &epEvent);
    if (SVS_STACK_OK != nRet)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to start server listen, add epoll event failed."
            "listenFd=%d, LocalIp=%s, LocalPort=%d, retCode=%d.",
            m_nSockFd, m_szLocalIp, m_stLocalAddr.usPort, nRet);

        return SVS_STACK_FAIL;
    }

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_INF,
        "Success to start server listen, ListenFd=%d, ServerIp=%s, ServerPort=%d.",
        m_nSockFd, m_szLocalIp, m_stLocalAddr.usPort);

    return SVS_STACK_OK;
}

int32_t CServerLink::stopConn()
{
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Server stop all the connection with client, ServerIndex=%u.", m_unLinkId);

    // ɾ���server�����еĿͻ�������
    ILink* pLink = NULL;
    (void)CSystem::instance()->pthread_rwlock_wrlock(&m_ConnMapLock);
    CONN_ITOR itor = m_ConnMap.begin();
    while (itor != m_ConnMap.end())
    {
        pLink = itor->second;
        m_ConnMap.erase(itor); // ���̴�map��ɾ���ֹ���߳��ٴλ�ȡ���ö���
        (void)CLinkFactory::getInstance()->destroyLink(pLink);
        pLink = NULL;
        itor = m_ConnMap.begin();
    }
//    for (CONN_ITOR itor = m_ConnMap.begin(); itor != m_ConnMap.end(); itor++)
//    {
//        pLink = itor->second;
//        (void)CLinkFactory::getInstance()->destroyLink(pLink);
//        pLink = NULL;
//    }
//
//    m_ConnMap.clear();
    (void)CSystem::instance()->pthread_rwlock_unlock(&m_ConnMapLock);

    return SVS_STACK_OK;
}

// ֹͣserver�ϵ�ĳ�����ӣ��������Ӵ�serverά����map��ɾ��
int32_t CServerLink::stopConn(const uint32_t unHandleIndex)
{
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Server stop the connection with client, ServerIndex=%u, ClientIndex=%u.",
        m_unLinkId, unHandleIndex);

    // �������Ӵ�server������map��ɾ��
    ILink *pLink = NULL;
    (void)pthread_rwlock_wrlock(&m_ConnMapLock);
    CONN_ITOR linkIter = m_ConnMap.find(unHandleIndex);
    if (linkIter != m_ConnMap.end())
    {
        pLink = linkIter->second;
        m_ConnMap.erase(unHandleIndex);
    }
    (void)pthread_rwlock_unlock(&m_ConnMapLock);

    // �������Ӵ�linkFactory�����
    (void)CLinkFactory::getInstance()->destroyLink(pLink);

    return SVS_STACK_OK;
}

int32_t CServerLink::getAddrByIndex(const uint32_t unHandleIndex, ADDR &stAddr)
{
    (void)pthread_rwlock_rdlock(&m_ConnMapLock);
    CONN_ITOR itor = m_ConnMap.find(unHandleIndex);
    if (itor != m_ConnMap.end())
    {
        ILink* pLink = itor->second;
        int32_t nRet = pLink->getAddrByIndex(unHandleIndex, stAddr);
        (void)pthread_rwlock_unlock(&m_ConnMapLock);
        return nRet;
    }

    (void)pthread_rwlock_unlock(&m_ConnMapLock);
    return SVS_STACK_FAIL;
}

/*****************************************************************************
 �� �� ��  : CServerLink.recvData
 ��������  : �����µ�����
 �������  : ��
 �� �� ֵ  : int32_t CServerLink::
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��6��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
int32_t CServerLink::handleRecv()
{
    struct sockaddr_in clientAddr;
    memset((char*)&clientAddr, 0x0, sizeof(clientAddr));
    socklen_t clilen = sizeof(clientAddr);
    int32_t connfd = CSystem::instance()->accept(m_nSockFd, (sockaddr*)&clientAddr, &clilen);/*lint !e740*///socket��ַ�����ָ�ʽ������
    if(connfd < 0)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to accept new connection, ListenSockFd=%d, ServerIp=%s, ServerPort=%d.",
            m_nSockFd, m_szLocalIp, m_stLocalAddr.usPort);

        return SVS_STACK_FAIL;
    }

    // ����һ���ͻ��˵�����
    ILink* pLink;
    ADDR stClientAddr;
    char szClientIp[STR_IP_LEN] = {0};
    memset(&stClientAddr, 0x0, sizeof(stClientAddr));
    stClientAddr.unIp = ntohl(clientAddr.sin_addr.s_addr);
    stClientAddr.usPort = ntohs(clientAddr.sin_port);
    strncpy(szClientIp, inet_ntoa(clientAddr.sin_addr), STR_IP_LEN - 1);

    // �����µ�����
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Server accept new connection, ListenSockFd=%d, connSockFd=%d, "
        "ServerIp=%s, ServerPort=%d, ClientIp=%s, ClientPort=%d.",
        m_nSockFd, connfd, m_szLocalIp, m_stLocalAddr.usPort,
        szClientIp, stClientAddr.usPort);

    int32_t iRet = CLinkFactory::getInstance()->createClientLink(&stClientAddr, &m_stLocalAddr, m_pMsgHandler, pLink);
    if (SVS_STACK_OK != iRet)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to accept new connection, create a new client link failed. ListenSock=%d, "
            "ConnSockFd=%d, ServerIp=%s, ServerPort=%d, ClientIp=%s, ClientPort=%d.",
            m_nSockFd, connfd, m_szLocalIp, m_stLocalAddr.usPort,
            szClientIp, stClientAddr.usPort);

        return SVS_STACK_FAIL;
    }

    CClientLink* pClientLink = dynamic_cast<CClientLink*>(pLink);
    if (NULL == pClientLink)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to accept new connection, cast ILink pointer to ClientLink pointer failed. "
            "ListenSock=%d, ConnSockFd=%d, ServerIp=%s, ServerPort=%d, ClientIp=%s, ClientPort=%d.",
            m_nSockFd, connfd, m_szLocalIp, m_stLocalAddr.usPort,
            szClientIp, stClientAddr.usPort);

        (void)CLinkFactory::getInstance()->destroyLink(pLink);
        return SVS_STACK_FAIL;
    }

    // ���������ӵ�����, sockFd, ��������, ����״̬
    pClientLink->setLinkType(LINK_TYPE_INNER);
    pClientLink->setSockFd(connfd);
    pClientLink->setSocketOption();
    pClientLink->setConnFlag(CONN_FLAG_CONNECTED);

    // ע�������ӵĴ����¼�
    epoll_event epEvent;
    epEvent.data.ptr = (void*)pClientLink;
    epEvent.events = EPOLLIN;                   // ����Ҫ������¼�����
    int32_t nRet = CLinkFactory::getInstance()->addEpollEvent(connfd, &epEvent);
    if (SVS_STACK_OK != nRet)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to accept new connection, add epoll event failed."
            "ListenSockFd=%d, ConnSockFd=%d, ServerIp=%s, ServerPort=%d, "
            "ClientIp=%s, ClientPort=%d, retCode=%d.",
            m_nSockFd, connfd, m_szLocalIp, m_stLocalAddr.usPort,
            szClientIp, stClientAddr.usPort, nRet);

        // ע��epoll�¼�ʧ�ܣ�������Դ
        (void)CLinkFactory::getInstance()->destroyLink(pLink);
        pLink = NULL;

        return SVS_STACK_FAIL;
    }

    // ���������
    (void)CSystem::instance()->pthread_rwlock_wrlock(&m_ConnMapLock);
    m_ConnMap.insert(std::pair<uint32_t, ILink*>(pClientLink->getLinkId(), pClientLink));
    (void)CSystem::instance()->pthread_rwlock_unlock(&m_ConnMapLock);

    // �������ӳɹ�����¼��ǰʱ����Ϊ�շ������ʱ��
    pClientLink->setHeartbeatTime();

    // ע���������
    CLinkFactory::getInstance()->addLinkForTimeout(pClientLink);

    // ֪ͨҵ��㣬�������ӵ���
    ConnEvent stEvent;
    stEvent.EventTime = time(NULL);
    stEvent.EventType = EVENT_TYPE_CONNECTED;
    stEvent.stPeerAddr.unIp = stClientAddr.unIp;
    stEvent.stPeerAddr.usPort = stClientAddr.usPort;
    stEvent.unHandleIndex = pClientLink->getLinkId();
    m_pMsgHandler->handleConnEvent(&stEvent);

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Success to accept new connection, ListenSockFd=%d, ConnSockFd=%d, "
        "ServerIp=%s, ServerPort=%d, ClientIp=%s, ClientPort=%d.",
        m_nSockFd, connfd, m_szLocalIp, m_stLocalAddr.usPort,
        szClientIp, stClientAddr.usPort);

    return SVS_STACK_OK;
}

// �������಻���������Ϣ���ͣ�Ĭ�Ϸ���0
int32_t CServerLink::handleSend()
{
    return SVS_STACK_OK;
}

void CServerLink::handleTimeout()
{
    return;
}

void CServerLink::handleClose()
{
    return;
}

// ֹͣ����
void CServerLink::stopListen()
{
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Begin to stop server listen, ServerIp=%s, ServerPort=%d, SockFd=%d, LinkIndex=%u.",
        m_szLocalIp, m_stLocalAddr.usPort, m_nSockFd, m_unLinkId);

    // ��server����ļ���ʵ�ʴ�epollɾ��, ͬʱ�Ὣlink�����map��ɾ��
    struct epoll_event epEvent;
    memset(&epEvent, 0x0, sizeof(epEvent));
    epEvent.events = EPOLLIN;
    epEvent.data.ptr = (void*)this;
    (void)CLinkFactory::getInstance()->delEpollEvent(m_nSockFd, &epEvent);

    // �Ͽ����Ӳ��ر�socket���, ����sockFdΪ��Чֵ
    (void)shutdown(m_nSockFd, SHUT_RDWR);
    (void)close(m_nSockFd);
    m_nSockFd = INVALID_SOCK_FD;

    return;
}

// ����socket������
void CServerLink::setSocketOption()
{
    // ���÷�������sock����ǿ��Ե�ַ���õ�
    int32_t nReuseAddr = 1;
    errno = 0; // ���errer code
    if(0 > setsockopt(m_nSockFd, SOL_SOCKET, SO_REUSEADDR, (void*)&nReuseAddr, sizeof(int32_t)))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to set server's socket option, set address reuse failed. SockFd=%d.",
            m_nSockFd);
    }

    // ���ý��ջ������С
    int32_t nSize = MAX_SOCKET_BUFFSIZE;//���ջ������С
    errno = 0; // ���errer code
    if(0 > setsockopt(m_nSockFd, SOL_SOCKET, SO_RCVBUF, (void*)&nSize, sizeof(int32_t)))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to set server's socket option, set recv buffer size failed. "
            "SockFd=%d, SetRecvBuffSize=%d.", m_nSockFd, nSize);
    }

    // ���÷��ͻ������С
    nSize =  MAX_SOCKET_BUFFSIZE;// ���ͻ������С
    errno = 0; // ���errer code
    if(0 > setsockopt(m_nSockFd, SOL_SOCKET, SO_SNDBUF, (void*)&nSize, sizeof(int32_t)))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to set server's socket option, set send buffer size failed. "
            "SockFd=%d, SetSendBuffSize=%d.", m_nSockFd, nSize);
    }

    return;
}

