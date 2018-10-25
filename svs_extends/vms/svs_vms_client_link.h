/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, ��Ϊ�������޹�˾

 ******************************************************************************
  �� �� ��   : ClientLink.h
  �� �� ��   : ����
  �������   : 2010��12��6��
  ����޸�   :
  ��������   : SVSЭ��ջ�ͻ���������ͷ�ļ�
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2010��12��6��
    �޸�����   : �����ļ�

******************************************************************************/

#ifndef _CLIENT_LINK_H_
#define _CLIENT_LINK_H_

#include "svs_vms_stack.h"
#include "svs_vms_stack_comm.h"

class CClientLink:public ILink
{
#ifdef UNITTEST
    friend class MockCClientLink;
#endif

public:
    CClientLink();
    virtual ~CClientLink();

public:
    // ��������˷�����Ϣ
    virtual int32_t sendMsg(const uint32_t unHandleIndex,
                        const char* pszMsg,
                        const uint32_t unMsgLength,
                        const uint32_t unTimeout);

    // �����ͻ��˵�����
    virtual int32_t startConn();

    // ֹͣ�ͻ��˵�����
    virtual int32_t stopConn();

    // ֹͣ�ͻ��˵����ӣ�unHandleIndex��ʶ�ÿͻ������ӵ��ڲ������
    virtual int32_t stopConn(const uint32_t unHandleIndex);

    // ��ݿͻ��˵���������ţ���ȡ�Զ˵�ַ
    virtual int32_t getAddrByIndex(const uint32_t unHandleIndex, ADDR &stAddr);

public:
    UTAPI void setSocketOption();
    void setLinkType(const uint8_t ucLinkType);
    UTAPI void setSockFd(int32_t nSockFd);
    UTAPI void setConnFlag(const uint8_t ucConnFlag);
    UTAPI void setHeartbeatTime();

protected:
    int32_t init(ISvsMsgHandler* pMsgHandler, uint32_t unLinkId, const ADDR* pstLocalAddr, const ADDR* pstRemoteAddr);
    virtual int32_t handleRecv();
    virtual int32_t handleSend();
    virtual void handleTimeout();
    virtual void handleClose();

private:
    // �����������е����
    UTAPI int32_t parseData(uint32_t &unUnProcRecvLen);

    // ��ҵ���֪ͨ�����¼�
    UTAPI void notifyConnection(const uint16_t usEventType);

    // ����������δ���ͳ�����Ϣ֪ͨ��ҵ��㣬ͬʱ��շ�����Ϣ����
    UTAPI void notifyLeftMessage();

    // �������еȴ���Ӧ��ʱ����Ϣ֪ͨ��ҵ��㣬ͬʱ��շ�����Ϣ����
    UTAPI void notifyTimeoutMessage();

    // �������״̬����ʧ�ܷ���-1�����ⲿ����handleClose()
    UTAPI int32_t checkConnectStatus();

    // ������Ϣ���Զ�
    UTAPI int32_t sendMsgToPeer(const char* pszMsgBuff, uint32_t unSendSize, uint32_t &unUnsendSize); // unRealSendSize��ʶʵ�ʷ��ʹ�С

    // ����Ҫ����ķ�����Ϣ�Ľṹ��
    UTAPI int32_t setSendMsgHead(SENDMSGHEAD* &pstSendMsgHead,
                            const uint32_t unBuffSize,
                            const char* pszOrigMsg,
                            const uint32_t unRealSendSize,
                            const uint32_t unTimeout,
                            const time_t tSendTime);

    // ��ӵȴ���Ӧ����Ϣ
    UTAPI void addWaitRespMsg(uint32_t unTransNo, SENDMSGHEAD* pstSendMsgHead);

    // ɾ��ȴ���Ӧ����Ϣ
    UTAPI void delWaitRespMsg(uint32_t unTransNo);

    // �ж��Ƿ�Ϊ��Ҫ�ȴ���Ӧ����Ϣ
    UTAPI bool bWaitRespMsg(uint32_t unTimeout, uint32_t unTransNo)const;

    // ����Ƿ��еȴ���Ӧ��ʱ����Ϣ�����У�֪ͨ��ҵ���
    UTAPI void checkRespMsg();

    // ��������Ƿ�ʱ���Ƿ���Ҫ��Է�������
    UTAPI int32_t checkHearbeatMsg();
    UTAPI bool bIsSockOK()const;

    // ��������״̬�����Ͽ�sock����
    UTAPI void closeSock(const uint8_t ucConnFlag);// ��������״̬�����ر�����

    // ��ʱ��ת��Ϊ�ַ�
    void time2string(time_t tTime, char* pszTime, uint32_t uiTimeLen)const;

    void shutdownSock();

private:
    typedef std::map<uint32_t, SENDMSGHEAD*>    WAIT_RESPMSG_MAP; // �ȴ���Ӧ��Ϣ��map��
    typedef WAIT_RESPMSG_MAP::iterator              WAIT_RESPMSG_MAP_ITOR;
    typedef std::pair<uint32_t, SENDMSGHEAD*>   WAIT_RESPMSG_MAP_PAIR;

private:
    ADDR                        m_stLocalAddr;                // ���صĵ�ַ
    ADDR                        m_stRemoteAddr;               // �Զ˵ĵ�ַ
    char                        m_szLocalIp[STR_IP_LEN];      // ���صĵ�ַ���ַ���ʽ�����ڴ�ӡ��־
    char                        m_szRemoteIp[STR_IP_LEN];     // �Զ˵ĵ�ַ���ַ���ʽ�����ڴ�ӡ��־
    uint8_t               m_ucHeartbeatTimes;          // ����ʱ����
    time_t                      m_tLastRecvHeartbeatTime;   // �ϴν�������ʱ��
    time_t                      m_tLastSendHeartbeatTime;   // �ϴη�������ʱ��
    ssize_t                     m_nUsedBuffLength;          // �Ѿ�ʹ�õĽ�����ݻ������
    char*                       m_pszRecvBuff;              // ���ջ�����
    std::deque<SENDMSGHEAD*>    m_MsgQueue;                 // ��Ϣ����, ���������Ϊ SENDMSGHEAD*
    pthread_mutex_t             m_MsgQueueMutex;            // ��Ϣ���е��߳���
    uint8_t               m_ucConnFlag;               // ����״̬��ʶ
    WAIT_RESPMSG_MAP            m_WaitRespMsgMap;           // �ȴ���Ӧ��Ϣ��map��
    pthread_rwlock_t            m_WaitRespMsgMapLock;
};


#endif

