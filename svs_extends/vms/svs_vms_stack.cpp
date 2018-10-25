#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "svs_vms_stack_comm.h"
#include "svs_vms_client_link.h"
#include "svs_vms_server_link.h"
#include "svs_vms_stack.h"
#include "svs_vms_system_api.h"

CLinkFactory*  CLinkFactory::m_pLinkFactory = NULL;
ISvsLogger* g_pLogger = NULL;

ILink::ILink()
{
    m_pMsgHandler = NULL;
    m_nSockFd = INVALID_SOCK_FD;
    m_ucLinkType = 0;
    m_unLinkId = 0;
    m_bDeleted = false;
}

ILink::~ILink()
{
    m_pMsgHandler = NULL;
    m_nSockFd = INVALID_SOCK_FD;
    m_ucLinkType = 0;
    m_unLinkId = 0;
    m_bDeleted = true;
}

uint8_t ILink::getLinkType()const
{
   return m_ucLinkType;
}

uint32_t ILink::getLinkId()const
{
    return m_unLinkId;
}

int32_t ILink::getSockFd()const
{
    return m_nSockFd;
}

void ILink::setDeleteFlag()
{
    m_bDeleted = true;
}

bool ILink::getDeleteFlag()const
{
    return m_bDeleted;
}

CLinkFactory::CLinkFactory()
{
    m_bRunning = false;     // �߳����б�ʶ
    m_tRecvThreadId = 0; // �����߳�id
    m_tHeartbeatThreadId = 0; // �����߳�id
    m_nEpollFd = INVALID_SOCK_FD;
    m_unCurLinkId = 0;
    memset(m_stEpollEvent, 0x0, sizeof(struct epoll_event)*MAX_EPOLL_EVENT);

    m_unHeartBeatInterval = MIN_HEARTBEAT_INTERVAL;
}/*lint !e1744*/ // �������init�����г�ʼ�����Ա��жϷ���ֵ

CLinkFactory::~CLinkFactory()
{
    m_bRunning = false;
}

CLinkFactory* CLinkFactory::getInstance()
{
    if (NULL != m_pLinkFactory)
    {
        return m_pLinkFactory;
    }

    m_pLinkFactory = new CLinkFactory;
    return m_pLinkFactory;
}

// ��ʼ��, ��д��־��ע�ᵽЭ��ջ��
int32_t CLinkFactory::init(uint32_t unHeartBeatInterval, ISvsLogger* pLogger)
{
    // �ж�ȫ��д��־�����ָ�룬��ֹ�ظ���ʼ��
    if (NULL != g_pLogger)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "No need to init link factory instance, it has already been inited.");

        return SVS_STACK_OK;
    }

    if ((MIN_HEARTBEAT_INTERVAL > unHeartBeatInterval)
            && (MAX_HEARTBEAT_INTERVAL < unHeartBeatInterval))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
                    "Fail to init link factory, the heartbeat interval[%u] invalid.",
                    unHeartBeatInterval);
        return SVS_STACK_FAIL;
    }

    if (NULL == pLogger)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init link factory, the write log pointer is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    g_pLogger = pLogger;
    m_unHeartBeatInterval = unHeartBeatInterval;

    // ��ʼ��CSystem����
    if (NULL == CSystem::instance())
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init link factory, create system instance failed.");
        return SVS_STACK_FAIL;
    }

    // ����linkMap���߳���, ʹ��Ĭ������
    if (SVS_STACK_OK != pthread_mutex_init(&m_LinkMapLock, NULL))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init link factory, create link map lock failed.");

        return SVS_STACK_FAIL;
    }

    // ����timerMap���߳���
    if (SVS_STACK_OK != pthread_mutex_init(&m_TimerMapLock, NULL))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init link factory, create timer map mutex failed.");

        // ����Ѿ���������
        (void)pthread_mutex_destroy(&m_LinkMapLock);
        return SVS_STACK_FAIL;
    }

    // ����epoll���
    m_nEpollFd = epoll_create(MAX_EPOLL_FD);
    if (0 > m_nEpollFd)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to init link factory, create epoll handle failed.");

        // ����Ѿ���������
        (void)pthread_mutex_destroy(&m_LinkMapLock);
        (void)pthread_mutex_destroy(&m_TimerMapLock);
        return SVS_STACK_FAIL;
    }

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Success to init link factory, EpollFd=%d.", m_nEpollFd);

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
        "\n===== Init svs stack success, CompileTime:%s %s =====\n",
        __DATE__, __TIME__);

    return SVS_STACK_OK;
}


// ��������������߳�
int32_t CLinkFactory::run()
{
    if (m_bRunning)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "No need to start svs stack thread, the threads are already running.");

        return SVS_STACK_OK;
    }

    // �������б�ʶ
    m_bRunning = true;

    // �����߳�����
    pthread_attr_t  attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);

    if (0 != CSystem::instance()->pthread_create(&m_tRecvThreadId, &attr, invokeRecvThread, (void*)this))
    {
        return SVS_STACK_ERR_CREATE_THREAD_FAIL;
    }

    if (0 != CSystem::instance()->pthread_create(&m_tHeartbeatThreadId, &attr, invokeTimerThread, (void*)this))
    {
        return SVS_STACK_ERR_CREATE_THREAD_FAIL;
    }

    return SVS_STACK_OK;
}


void CLinkFactory::exit()
{
    if (!m_bRunning)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "No need to stop svs stack thread, the threads are already exited.");

        return;
    }

    m_bRunning = false;

    (void)CSystem::instance()->pthread_join(m_tRecvThreadId, NULL);
    (void)CSystem::instance()->pthread_join(m_tHeartbeatThreadId, NULL);

    ILink* pLink = NULL;
    for (LINKMAP_ITOR itor = m_LinkMap.begin(); itor != m_LinkMap.end(); itor++)
    {
        pLink = itor->second;
        if (NULL == pLink)
        {
            continue;
        }

        // ��link����sever�˽��տͻ������Ӷ����ģ���ô�˴��Ȳ��ͷţ���server�����ͷ�
        if (LINK_TYPE_INNER == pLink->getLinkType())
        {
            continue;
        }

        (void)pLink->stopConn();
        delete pLink;
        pLink = NULL;
    }
    m_LinkMap.clear();

    // ���map�Ķ�д��
    (void)CSystem::instance()->pthread_mutex_destroy(&m_LinkMapLock);

    // ��ٶ�ʱ���߳�ʹ�õ���
    (void)CSystem::instance()->pthread_mutex_destroy(&m_TimerMapLock);

    return;
}

uint32_t CLinkFactory::getHeartBeatInterval() const
{
    return m_unHeartBeatInterval;
}

void* CLinkFactory::invokeRecvThread(void *argc)
{
    if (NULL == argc)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ALT,
            "Fail to start recv data thread, pointer argc is NULL.");

        return NULL;
    }

    // ��������߳�
    CLinkFactory* pLinkFactory = (CLinkFactory*)argc;
    pLinkFactory->recvThread();
    return NULL;
}

void* CLinkFactory::invokeTimerThread(void *argc)
{
    if (NULL == argc)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ALT,
            "Fail to start heartbeat thread, pointer argc is NULL.");

        return NULL;
    }

    // �����߳�
    CLinkFactory* pLinkFactory = (CLinkFactory*)argc;
    pLinkFactory->timerThread();
    return NULL;
}

/*****************************************************************************
 �� �� ��  : CLinkFactory.recvThread
 ��������  : �������Ӻ���ݵ��߳�
 �������  : ��
 �� �� ֵ  : void CLinkFactory::
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��6��
    ��    ��   : file
    �޸�����   : ����ɺ���

*****************************************************************************/
void CLinkFactory::recvThread()
{
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Begin to start recv data thread, ThreadId=%u.", m_tRecvThreadId);

    int32_t nReadyCount = 0;
    ILink* pLink = NULL;
    int32_t iRet = SVS_STACK_OK;
    while (m_bRunning)
    {
        // �ȴ�epoll�¼��ķ���,���ó�ʱʱ�䣬��ֹ���쳣���ֹͣ�߳�ʱһֱ�ȴ�
        nReadyCount = CSystem::instance()->epoll_wait(m_nEpollFd, m_stEpollEvent, MAX_EPOLL_EVENT, EPOLL_WAIT_EVENT_TIME);

        // ���¼�������߳�ʱ
        for (int32_t i = 0; i < nReadyCount; i++)
        {
            // ����epoll�¼��Ĵ�����
            iRet = SVS_STACK_OK;

            // ���epoll�¼��е�fd������link�������ö����Ҳ�����˵���Ѿ���ɾ��
            pLink = (ILink*)m_stEpollEvent[i].data.ptr;
            if (NULL == pLink)
            {
                continue;
            }

            // �жϸö����Ƿ��Ѿ���ɾ��
            if (pLink->getDeleteFlag())
            {
                continue;
            }

            VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
                "Recv epoll event, LinkIndex=%u, "
                "Event=%d[%d:EPOLLIN, %d:EPOLLOUT, %d:EPOLLERR, %d:EPOLLHUP].",
                pLink->getLinkId(),
                m_stEpollEvent[i].events, EPOLLIN, EPOLLOUT, EPOLLERR, EPOLLHUP);


            if (m_stEpollEvent[i].events & (EPOLLERR | EPOLLHUP))/*lint !e655*/// ��epoll�¼����ж�
            {
                // �д�����
                VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
                    "Recv epoll error event, LinkIndex=%u, "
                    "LinkType=%d[0x1:server, 0x2:client, 0x3:inner], "
                    "Event=%d[%d:EPOLLIN, %d:EPOLLOUT, %d:EPOLLERR, %d:EPOLLHUP].",
                    pLink->getLinkId(),
                    pLink->getLinkType(),
                    m_stEpollEvent[i].events, EPOLLIN, EPOLLOUT, EPOLLERR, EPOLLHUP);

                iRet = SVS_STACK_FAIL;
            }
            else if (m_stEpollEvent[i].events & EPOLLIN)
            {
                // epoll���¼�
                iRet = pLink->handleRecv();
            }
            else if (m_stEpollEvent[i].events & EPOLLOUT)
            {
                // epollд�¼�
                iRet = pLink->handleSend();
            }

            // �жϴ���epoll�Ĵ����룬��ΪSVS_STACK_FAIL, ����handleClose()
            if (SVS_STACK_FAIL == iRet)
            {
                pLink->handleClose();
            }
        }

        // ����map�������д�ɾ��Ķ���delete��
        (void)pthread_mutex_lock(&m_LinkMapLock);
        LINKMAP_ITOR curItor = m_LinkMap.begin();
        LINKMAP_ITOR preItor = curItor;
        while (curItor != m_LinkMap.end())
        {
            preItor = curItor;
            curItor++;
            pLink = preItor->second;
            if (pLink->getDeleteFlag())
            {
                VMS_SVS_LOG(
                    SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
                    "Delete the link object, pLink=%p.");

                m_LinkMap.erase(preItor);
                delete pLink;
                pLink = NULL;
            }
        }
        (void)pthread_mutex_unlock(&m_LinkMapLock);

    }

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
        "Stop svs stack recv data thread, ThreadId=%u.", m_tRecvThreadId);

    return ;
}

//�����߳�
void CLinkFactory::timerThread()
{
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Begin to start heartbeat thread, ThreadId=%u.", m_tHeartbeatThreadId);

    LINKMAP_ITOR linkItor;// ��ǰ�ĵ����
    LINKMAP_ITOR preItor;// ֮ǰ�ĵ����
    while(m_bRunning)
    {
        (void)pthread_mutex_lock(&m_TimerMapLock);
        linkItor = m_TimerMap.begin();
        while (linkItor != m_TimerMap.end())
        {
            ILink* pLink = linkItor->second;
            preItor = linkItor;
            linkItor++;
            if (NULL == pLink)
            {
                m_TimerMap.erase(preItor);
                continue;
            }

            // ���������Ӧ��Ϣ�Ƿ�ʱ
            pLink->handleTimeout();
        }
        (void)pthread_mutex_unlock(&m_TimerMapLock);

        (void)sleep(HEARTBEAT_THREAD_WORK_INTERVAL);  //Ĭ��ÿ��ѭ��һ��
    }

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
        "Stop svs stack heartbeat thread, ThreadId=%u.", m_tHeartbeatThreadId);

    return;
}


int32_t CLinkFactory::createServerLink(const ADDR* pstServerAddr, ISvsMsgHandler* pMsgHandler, ILink* &pLink)
{
    if(NULL == pstServerAddr)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to create server link, the pointer ServerAddr is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    if(NULL == pMsgHandler)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to create server link, the pointer MsgHandler is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    char szServerIp[STR_IP_LEN] = {0};
    struct in_addr stIp;
    stIp.s_addr = htonl(pstServerAddr->unIp);
    strncpy(szServerIp, inet_ntoa(stIp), STR_IP_LEN - 1);

    // �����������˵�link����
    try
    {
        pLink = new CServerLink;
    }
    catch (...)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to create server Link, alloc memory fail. ServerIp=%s, ServerPort=%d.",
            szServerIp, pstServerAddr->usPort);

        return SVS_STACK_ERR_MEMORY_ALLOCATE_FAIL;
    }

    // ����һ��linkIndex
    uint32_t unLinkId = allocLinkIndex();

    // ��ʼ��link����
    int32_t nRet = pLink->init(pMsgHandler, unLinkId, pstServerAddr, NULL);
    if (SVS_STACK_OK != nRet)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to create server Link, init link failed. ServerIp=%s, ServerPort=%d.",
            szServerIp, pstServerAddr->usPort);

        delete pLink;
        pLink = NULL;
        return SVS_STACK_FAIL;
    }

    // ��link����ŵ�map����
    (void)CSystem::instance()->pthread_mutex_lock(&m_LinkMapLock);
    m_LinkMap.insert(LINKMAP_PAIR(unLinkId, pLink));
    (void)CSystem::instance()->pthread_mutex_unlock(&m_LinkMapLock);

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_INF,
        "Success to create server link, ServerIp=%s, ServerPort=%d, LinkIndex=%u.",
        szServerIp, pstServerAddr->usPort, unLinkId);

    return SVS_STACK_OK;
}

int32_t CLinkFactory::createClientLink(const ADDR* pstServerAddr,
                                    const ADDR* pstClientAddr,
                                    ISvsMsgHandler* pMsgHandler,
                                    ILink*  &pLink)
{
    if(NULL == pstServerAddr)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to create client link, the pointer ServerAddr is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    if(NULL == pstClientAddr)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to create client link, the pointer ClientAddr is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    if(NULL == pMsgHandler)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to create server link, the pointer MsgHandler is NULL.");

        return SVS_STACK_ERR_POINTER_NULL;
    }

    struct in_addr stIp;
    char szServerIp[STR_IP_LEN] = {0};
    stIp.s_addr = htonl(pstServerAddr->unIp);
    strncpy(szServerIp, inet_ntoa(stIp), STR_IP_LEN - 1);

    char szClientIp[STR_IP_LEN] = {0};
    stIp.s_addr = htonl(pstClientAddr->unIp);
    strncpy(szClientIp, inet_ntoa(stIp), STR_IP_LEN - 1);

    try
    {
        pLink = new CClientLink;
    }
    catch (...)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to create server Link, alloc memory fail. ServerIp=%s, ServerPort=%d, "
            "ClientIp=%s, ClientPort=%d.",
            szServerIp, pstServerAddr->usPort,
            szClientIp, pstClientAddr->usPort);

        return SVS_STACK_ERR_MEMORY_ALLOCATE_FAIL;
    }

    // ����һ��linkIndex
    uint32_t unLinkId = allocLinkIndex();

    // ��ʼ����link����
    int32_t nRet = pLink->init(pMsgHandler, unLinkId, pstClientAddr, pstServerAddr);
    if (SVS_STACK_OK != nRet)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_ERR,
            "Fail to create client Link, init link failed. "
            "ServerIp=%s, ServerPort=%d, ClientIp=%s, ClientPort=%d.",
            szServerIp, pstServerAddr->usPort,
            szClientIp, pstClientAddr->usPort);

        delete pLink;
        pLink = NULL;
        return SVS_STACK_FAIL;
    }

    // ��link����ŵ�map��
    (void)CSystem::instance()->pthread_mutex_lock(&m_LinkMapLock);
    m_LinkMap.insert(LINKMAP_PAIR(unLinkId, pLink));
    (void)CSystem::instance()->pthread_mutex_unlock(&m_LinkMapLock);

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_INF,
        "Success to create client link, "
        "ServerIp=%s, ServerPort=%d, ClientIp=%s, ClientPort=%d, LinkIndex=%u.",
        szServerIp, pstServerAddr->usPort,
        szClientIp, pstClientAddr->usPort, unLinkId);

    return SVS_STACK_OK;
}

// �ͷ����Ӷ���
// ��Ϊserver����ֹͣ��server�ϵ����м���ǰ�Ѿ����������ӣ�����ٸö���
// ��Ϊclient����ֹͣ��client�ϵ����ӣ�����ٸö���
int32_t CLinkFactory::destroyLink(ILink* &pLink)const
{
    if (NULL == pLink)
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
            "No need to destroy link object, the pointer is NULL. ");

        return SVS_STACK_OK;
    }

    uint8_t ucLinkType = pLink->getLinkType();

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_NTC,
        "Begin to destroy link object, pLink=%p, LinkType=%u.",
        pLink, ucLinkType);

    // ��Ϊ�ͻ��˵����ӡ�����server���ն��������ӣ���epoll�¼���ɾ���LinkMap��ɾ��
    if (LINK_TYPE_CLIENT == ucLinkType || LINK_TYPE_INNER == ucLinkType)
    {
        // ֹͣ�ͻ�������
        (void)pLink->stopConn();

        // ����link�����״̬Ϊ��ɾ����recv�߳�ͳһɾ��link����
        pLink->setDeleteFlag();
        pLink = NULL;

        return SVS_STACK_OK;
    }

    // ��Ϊ�������˵����ӣ���epoll�¼���ɾ�����µĿͻ���ɾ������LinkMapɾ��
    if (LINK_TYPE_SERVER == ucLinkType)
    {
        // ֹͣserver�������е����ӣ���ɾ�����µ����ж�����
        (void)pLink->stopConn();

        // ֹͣserver�ļ���
        CServerLink* pServerLink = dynamic_cast<CServerLink*>(pLink);
        if (NULL != pServerLink)
        {
            pServerLink->stopListen();
        }

        // ����link�����״̬Ϊ��ɾ����recv�߳�ͳһɾ��link����
        pLink->setDeleteFlag();
        pLink = NULL;

        return SVS_STACK_OK;
    }

    // �����Ͳ���ȷ����ֱ�ӽ��ö���ɾ��
    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
        "Directly destroy the link object, link type is invalid. "
        "pLink=%p, LinkType=%u.",
        pLink, ucLinkType);

    delete pLink;
    pLink = NULL;

    return SVS_STACK_OK;
}

// ����һ��link��id
uint32_t CLinkFactory::allocLinkIndex()
{
    return ++m_unCurLinkId;
}

int32_t CLinkFactory::addEpollEvent(const int32_t nSockFd, struct epoll_event* pEpEvent)
{
    if (NULL == pEpEvent)
    {
        return SVS_STACK_ERR_POINTER_NULL;
    }

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
        "Begin to add epoll event, SockFd=%d, epEvent=%d.", nSockFd, pEpEvent->events);

    // ��ָ����socket���epoll�¼�
    if (0 != CSystem::instance()->epoll_ctl(m_nEpollFd, EPOLL_CTL_ADD, nSockFd, pEpEvent))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to add epoll event, SockFd=%d, epEvent=%d.",
            nSockFd, pEpEvent->events);

        return SVS_STACK_ERR_ADD_EPOLL_EVENT_FAIL;
    }

    return SVS_STACK_OK;
}

int32_t CLinkFactory::delEpollEvent(const int32_t nSockFd, struct epoll_event* pEpEvent)
{
    if (NULL == pEpEvent)
    {
        return SVS_STACK_ERR_POINTER_NULL;
    }

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
        "Begin to delete epoll event, SockFd=%d, epEvent=%d.", nSockFd, pEpEvent->events);

    // ��ָ����socket���epoll�¼�
    if (0 != CSystem::instance()->epoll_ctl(m_nEpollFd, EPOLL_CTL_DEL, nSockFd, pEpEvent))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to delete epoll event, SockFd=%d, epEvent=%d.",
            nSockFd, pEpEvent->events);

        return SVS_STACK_ERR_ADD_EPOLL_EVENT_FAIL;
    }

    return SVS_STACK_OK;
}

int32_t CLinkFactory::modEpollEvent(const int32_t nSockFd, struct epoll_event* pEpEvent)
{
    if (NULL == pEpEvent)
    {
        return SVS_STACK_ERR_POINTER_NULL;
    }

    VMS_SVS_LOG(SVS_STACK_LOG_TYPE_DBG, SVS_STACK_LOG_LEVEL_INF,
        "Begin to modify epoll event, SockFd=%d, epEvent=%d.", nSockFd, pEpEvent->events);

    // ��ָ����socket���epoll�¼�
    if (0 != CSystem::instance()->epoll_ctl(m_nEpollFd, EPOLL_CTL_MOD, nSockFd, pEpEvent))
    {
        VMS_SVS_LOG(SVS_STACK_LOG_TYPE_RUN, SVS_STACK_LOG_LEVEL_WAR,
            "Fail to modify epoll event, SockFd=%d, epEvent=%d.",
            nSockFd, pEpEvent->events);

        return SVS_STACK_ERR_ADD_EPOLL_EVENT_FAIL;
    }

    return SVS_STACK_OK;
}

// ����������link����
void CLinkFactory::addLinkForTimeout(const ILink* pLink)
{
    // ��ȡ�����̵߳�id
    pthread_t tCurThreadId = pthread_self();

    if (m_tHeartbeatThreadId != tCurThreadId)
    {
        // ����ǰ�߳�id��timerLock���ڵ������߳�id��ͬ����Ҫ����
        (void)CSystem::instance()->pthread_mutex_lock(&m_TimerMapLock);
        m_TimerMap.insert(LINKMAP_PAIR(pLink->getLinkId(), const_cast<ILink*>(pLink)));
        (void)CSystem::instance()->pthread_mutex_unlock(&m_TimerMapLock);
    }
    else
    {
        m_TimerMap.insert(LINKMAP_PAIR(pLink->getLinkId(), const_cast<ILink*>(pLink)));
    }

    return;
}

// ɾ���������link����
void CLinkFactory::delLinkForTimeout(const ILink* pLink)
{
    // ��ȡ�����̵߳�id
    pthread_t tCurThreadId = pthread_self();

    if (m_tHeartbeatThreadId != tCurThreadId)
    {
        (void)CSystem::instance()->pthread_mutex_lock(&m_TimerMapLock);
        m_TimerMap.erase(pLink->getLinkId());
        (void)CSystem::instance()->pthread_mutex_unlock(&m_TimerMapLock);
    }
    else
    {
        m_TimerMap.erase(pLink->getLinkId());
    }

    return;
}

