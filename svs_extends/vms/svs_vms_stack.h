/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, ��Ϊ�������޹�˾

 ******************************************************************************
  �� �� ��   : SvsStack.h
  �� �� ��   : ����
  �������   : 2010��12��6��
  ����޸�   :
  ��������   : SVSЭ��ջͷ�ļ�
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2010��12��6��
    �޸�����   : �����ļ�

******************************************************************************/

#ifndef _SVS_STACK_H_
#define _SVS_STACK_H_

#include <pthread.h>
#include <time.h>
#include <sys/epoll.h>
#include <map>
#include <queue>
#include <string.h>
#include "svs_vms_system_api.h"
// ��־����
enum SVS_STACK_LOG_TYPE
{
   SVS_STACK_LOG_TYPE_SEC = 0,  // ��ȫ��־
   SVS_STACK_LOG_TYPE_OPE,      // ������־
   SVS_STACK_LOG_TYPE_RUN,      // ������־
   SVS_STACK_LOG_TYPE_USR,      // �û���־
   SVS_STACK_LOG_TYPE_DBG       // ������־
};

// ��־����
enum SVS_STACK_LOG_LEVEL
{
    SVS_STACK_LOG_LEVEL_EMR,
    SVS_STACK_LOG_LEVEL_ALT,
    SVS_STACK_LOG_LEVEL_CRI,
    SVS_STACK_LOG_LEVEL_ERR,
    SVS_STACK_LOG_LEVEL_WAR,
    SVS_STACK_LOG_LEVEL_NTC,
    SVS_STACK_LOG_LEVEL_INF
};

// �¼�����
enum _EVENT_TYPE_
{
    EVENT_TYPE_DISCONNECT = 0x0,    // �����¼�
    EVENT_TYPE_CONNECTED  = 0x1,    // ���ӳɹ��¼�
    EVENT_TYPE_TIMEOUT    = 0x2     // ����ʱ�¼�
};

// ��Ϣ���͡����յĽ������
enum RESULT_TYPE
{
    RESULT_TYPE_SEND,       // Э��ջ������Ϣ���
    RESULT_TYPE_RECV,       // Э��ջ������Ϣ���
    RESULT_TYPE_TIMEOUT     // Э��ջ���ո���Ϣ����Ӧ��Ϣ��ʱ
};

// �����붨��
enum SVS_STACK_ERR
{
    // ����ӿڵĴ����룬 ͨ�ò��֣�0x0000:�ɹ� 0x1000-0x1FFF:ʧ��
    SVS_STACK_OK = 0x0000,
    SVS_STACK_FAIL = 0x1000,
    SVS_STACK_ERR_QUEUE_FULL = 0x1001,  // ��Ϣ������
    SVS_STACK_ERR_POINTER_NULL = 0x1002,  // ָ��Ϊ��
    SVS_STACK_ERR_MEMORY_ALLOCATE_FAIL = 0x1003, // �����ڴ�ʧ��
    SVS_STACK_ERR_MSG_LENGTH_INVALID = 0x1004,// ��Ϣ���ȷǷ�
    SVS_STACK_ERR_CREATE_THREAD_FAIL = 0x1005,
    SVS_STACK_ERR_DISCONN = 0x1006, // �Ѿ��ͶԶ˶������������ٷ�����Ϣ

    // ����ӿڴ����룬���������� 0x2000-0x2FFF
    SVS_STACK_ERR_INVALID_SERVER_LINK = 0x2000, // ��������link����Ƿ�

    // ����ӿڴ����룬�ͻ������� 0x3000-0x3FFF

    // �ڲ�ʹ�õĴ����룬 0x4000-0x4FFF
    SVS_STACK_ERR_PARSE_MSG             = 0x4000,
    SVS_STACK_ERR_LINK_NOT_EXIST        = 0x4001, // Ҫ������link���󲻴���
    SVS_STACK_ERR_ADD_EPOLL_EVENT_FAIL = 0x4002,  // ���epoll�¼�ʧ��
    SVS_STACK_ERR_INIT_LOCK_FAIL        = 0x4003   // ��ʼ����ʧ��
};

typedef struct _ADDR
{
    uint32_t    unIp;   // ip��ַ��������
    uint16_t  usPort; // �˿ڣ�������
}ADDR;

typedef struct _ConnEvent
{
    uint16_t        EventType;  // �¼����ͣ��μ�EVENT_TYPE
    time_t          EventTime; // �¼�����ʱ��
    uint32_t        unHandleIndex; // ���Ӿ����ڲ�����
    ADDR            stPeerAddr; // �Զ˵ĵ�ַ
}ConnEvent;

typedef struct _Result
{
    uint8_t ucResultType;// ��ʶ�ý���Ƿ�����Ϣ����ǽ�����Ϣ���, �μ�RESULT_TYPE
    int32_t           nResult;      // ��ʶ����Ľ��ԭ��, �μ������
}Result;

enum _enHEARTBEAT_DEF
{
    DEFAULT_HEARTBEAT_INTERVAL     = 15,        // Ĭ������������λ��
    MIN_HEARTBEAT_INTERVAL         = 5,         // ������С���ͼ��ʱ�䣬��λ��
    MAX_HEARTBEAT_INTERVAL         = 300        // ��������ͼ��ʱ�䣬��λ��
};


class ISvsLogger
{
public:
    ISvsLogger(){};
    virtual ~ISvsLogger(){};

public:
    virtual void writeLog(uint8_t ucType,
                            uint8_t ucLevel,
                            const char *pszLog,
                            uint32_t unLogLength,
                            char* pFile,
                            int32_t lLine) = 0;
};


class ISvsMsgHandler
{
public:
    ISvsMsgHandler(){};
    virtual ~ISvsMsgHandler(){};

public:
    /*****************************************************************************
     �� �� ��  : ISvsMsgHandler.handleMessage
     ��������  : ������Ϣ�ӿڣ�����ҵ��㿽��pMsg�������Լ��Ļ�������ҵ����̴߳���
                 (1)��Э��ջ�ɹ��յ�����SVS��Ϣʱ�����øýӿ�֪ͨҵ��㣻
                 (2)��Э��ջ����SVS��Ϣʧ��ʱ�����øýӿ�֪ͨҵ��㣻
                 (3)��Э��ջ���ո���Ϣ����Ӧ��Ϣ��ʱ�����øýӿ�;
     �������  : const char* pszMsg:ָ����Ϣ�׵�ַ
                 const uint32_t unMsgLength:��ʶ��Ϣ����
                 const uint32_t unHandleIndex:��ʶ���ո���Ϣ��handle���ڲ�����
     �������  : Result stResult: Э��ջ�Ը���Ϣ������
     �� �� ֵ  : ��
    *****************************************************************************/
    virtual void handleMessage(const char* pszMsg,
                                const uint32_t unMsgLength,
                                const uint32_t unHandleIndex,
                                Result &stResult) = 0;

    /*****************************************************************************
     �� �� ��  : ISvsMsgHandler.handleConnEvent
     ��������  : ���������¼�, Э��ջ���ȵ��ø�handleConnEvent()������Ȼ�󽫶�Ӧ����������
                 ע:�ýӿ������³����»ᱻЭ��ջ����:
                 (1) ���µ����ӳɹ�
                 (2) ��⵽��ǰ�����Ӷ���
                 (3) �Ѿ������˲�����Ϣ����ʣ�µ���Ϣ��n�����Ժ���Ȼ����ʧ��
     �������  : const ConnEvent* pstEvent:�����¼��ľ�������
     �������  : ��
     �� �� ֵ  : ��
    *****************************************************************************/
    virtual void handleConnEvent(const ConnEvent* pstEvent) = 0;
};

class ILink
{
public:
    ILink();
    virtual ~ILink();

public:
    /*****************************************************************************
     �� �� ��  : ILink.SendMsg
     ��������  : ������Ϣ�ӿ�,�ýӿ�Ϊ�첽�ӿ�,ֻ�ǰ���Ϣ�������ŵ�Э��ջ�ķ��Ͷ�����
                 ע:Ŀǰ���Ͷ��еĴ�С����Ĭ��ֵ���������Ժ����������ⲿָ�����д�С
     �������  : const uint32_t unHandleIndex:������Ϣhandle���ڲ������
                 const uint8_t* pszMsg:Ҫ���͵���Ϣ�׵�ַ
                 const uint32_t unMsgLength:Ҫ���͵���Ϣ����
                 const uint32_t unTimeout:����Ϣ����Ӧ��Ϣ�ĳ�ʱʱ�䣬��λ�룬�������Ϊ��Ӧ��Ϣ������Ϊ0�����ٵȴ�ʱ
     �������  : ��
     �� �� ֵ  : SVS_STACK_OK: �ɹ�
                 SVS_STACK_ERROR_QUEUE_FULL: ���Ͷ�����
    *****************************************************************************/
    virtual int32_t sendMsg(const uint32_t unHandleIndex,
                           const char* pszMsg,
                           const uint32_t unMsgLength,
                           const uint32_t unTimeout) = 0;

    /*****************************************************************************
     �� �� ��  : ILink.startConn
     ��������  : ��������
                 ��server����: ��ָ���˿��Ͽ�ʼ����
                 ��client����: ��ָ���Ķ˿�ע��
     �������  : ��
     �������  : ��
     �� �� ֵ  : virtual int32_t stop
    *****************************************************************************/
    virtual int32_t startConn() = 0;

    /*****************************************************************************
     �� �� ��  : ILink.stopConn
     ��������  : ֹͣ����
                 ��server����: ֹͣ�ü���˿��������Ѿ�����������
                 ��client����: ֹͣ�ͷ������Ѿ�����������
     �������  : ��
     �������  : ��
     �� �� ֵ  : virtual int32_t stop
    *****************************************************************************/
    virtual int32_t stopConn() = 0;

    /*****************************************************************************
     �� �� ��  : ILink.stopConn
     ��������  : �ýӿ��Ǹ�server��ʹ�ã�ֹͣserver�����Ѿ�������ĳ������
     �������  : const uint32_t unHandleIndex: ���ӵ������
     �������  : ��
     �� �� ֵ  : virtual int32_t stop
    *****************************************************************************/
    virtual int32_t stopConn(const uint32_t unHandleIndex) = 0;

    /*****************************************************************************
     �� �� ��  : ILink.getAddrByIndex
     ��������  : ���handle���ڲ������ȡ��handle���ӵĶԶ˵�ַ
     �������  : const uint32_t unHandleIndex:handle������
     �������  : ADDR_INFO &stAddr:�Զ˵ĵ�ַ
     �� �� ֵ  : virtual int32_t getA
    *****************************************************************************/
    virtual int32_t getAddrByIndex(const uint32_t unHandleIndex, ADDR &stAddr) = 0;

    UTAPI uint32_t getLinkId()const;

protected:
    // ��ʼ��link����
    virtual int32_t init(ISvsMsgHandler* pMsgHandler, uint32_t unLinkId, const ADDR* pstLocalAddr, const ADDR* pstRemoteAddr) = 0;

    // ����epoll in�¼�������ֵ��SVS_STACK_FAIL: ��������Ҫ����handleClose��SVS_STACK_OK: �ɹ�
    virtual int32_t handleRecv() = 0;

    // ����epoll out�¼�������ֵ��SVS_STACK_FAIL: ��������Ҫ����handleClose��SVS_STACK_OK: �ɹ�
    virtual int32_t handleSend() = 0;

    // ���?ʱ����ʱ�ص��¼�������ֵ��SVS_STACK_FAIL: ��������Ҫ����handleClose��SVS_STACK_OK: �ɹ�
    virtual void handleTimeout() = 0;

    // ��������¼�
    virtual void handleClose() = 0;

    UTAPI uint8_t getLinkType()const;
    UTAPI int32_t getSockFd()const;
    UTAPI void setDeleteFlag();
    UTAPI bool getDeleteFlag()const;

protected:
    uint8_t   m_ucLinkType;
    ISvsMsgHandler*    m_pMsgHandler;
    int32_t             m_nSockFd;  // ��link��socket���
    uint32_t    m_unLinkId;  // ��link���������id
    bool            m_bDeleted; // ��ʶ�ö����Ƿ���Ҫ��ɾ��
friend class CLinkFactory;
};

class CLinkFactory
{
#ifdef UNITTEST
    friend class MockCLinkFactory;
#endif

public:
    UTAPI ~CLinkFactory();

    static CLinkFactory* getInstance();

    // ��ʼ��, ��д��־��ע�ᵽЭ��ջ��
    UTAPI int32_t init(uint32_t unHeartBeatInterval, ISvsLogger* pLogger);

    // �����߳�
    UTAPI int32_t run();

    // �˳���ֹͣ���е��̣߳�������ǰ�����������ͷ�
    UTAPI void exit();

    // �������������Ӷ���ָ�룬����pMsgHandler�Ǹ������ϵ���Ϣ�������
    UTAPI int32_t createServerLink(const ADDR* pstServerAddr, ISvsMsgHandler* pMsgHandler, ILink* &pLink);

    // �����ͻ������Ӷ���ָ�룬����pMsgHandler�Ǹ������ϵ���Ϣ�������
    UTAPI int32_t createClientLink(const ADDR* pstServerAddr,
                            const ADDR* pstClientAddr,
                            ISvsMsgHandler *pMsgHandler,
                            ILink* &pLink);

    // ��ȡ���õ�������ʱ��
    uint32_t getHeartBeatInterval() const;

    // �ͷ����Ӷ���
    // ��Ϊserver����ֹͣ��server�ϵ����м���ǰ�Ѿ����������ӣ�����ٸö���
    // ��Ϊclient����ֹͣ��client�ϵ����ӣ�����ٸö���
    UTAPI int32_t destroyLink(ILink* &pLink)const;

public:
    static void* invokeRecvThread(void *argc);
    static void* invokeTimerThread(void *argc);

public:
    // ���epoll�¼�
    UTAPI int32_t addEpollEvent(const int32_t nSockFd, struct epoll_event* pEpEvent);

    // ɾ��epoll�¼�
    UTAPI int32_t delEpollEvent(const int32_t nSockFd, struct epoll_event* pEpEvent);

    // �޸�epoll�¼�
    UTAPI int32_t modEpollEvent(const int32_t nSockFd, struct epoll_event* pEpEvent);

    // ������������
    UTAPI void addLinkForTimeout(const ILink* pLink);

    // ��map��ɾ���������link����
    UTAPI void delLinkForTimeout(const ILink* pLink);

private:
    CLinkFactory();

    UTAPI void recvThread();
    UTAPI void timerThread();

    UTAPI uint32_t allocLinkIndex();

public:
   typedef enum
    {
        THREAD_TYPE_RECV,
        THREAD_TYPE_SEND,
        THREAD_TYPE_TIMER
    }THREAD_TYPE;

    typedef struct _THREAD_AGR
    {
        THREAD_TYPE   ucThreadType;
        CLinkFactory* pLinkFactory;
    }THREAD_AGR;

private:
    #define MAX_EPOLL_EVENT 1024
    typedef std::map<uint32_t, ILink*>  LINKMAP;    // <linkId, ILink*>, �������link�Ĺ�ϵ��
    typedef LINKMAP::iterator               LINKMAP_ITOR;
    typedef std::pair<uint32_t, ILink*> LINKMAP_PAIR;

private:
    static CLinkFactory*            m_pLinkFactory;
    bool                            m_bRunning;           // �߳����б�ʶ
    pthread_t                       m_tRecvThreadId;      // �����߳�id
    pthread_t                       m_tHeartbeatThreadId; // �����߳�id
    int32_t                             m_nEpollFd;
    struct epoll_event              m_stEpollEvent[MAX_EPOLL_EVENT];
    uint32_t                    m_unCurLinkId;

    LINKMAP                         m_LinkMap;
    pthread_mutex_t                 m_LinkMapLock;

    LINKMAP                         m_TimerMap;
    pthread_mutex_t                 m_TimerMapLock;

    uint32_t                    m_unHeartBeatInterval;     // ������
};



#endif

