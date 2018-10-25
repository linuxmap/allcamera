/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, ��Ϊ�������޹�˾

 ******************************************************************************
  �� �� ��   : ServerLink.h
  �� �� ��   : ����
  �������   : 2010��12��6��
  ����޸�   :
  ��������   : SVSЭ��ջ��������������ͷ�ļ�
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2010��12��6��
    �޸�����   : �����ļ�

******************************************************************************/

#ifndef _SERVER_LINK_H_
#define _SERVER_LINK_H_

#include "svs_vms_stack.h"
#include "svs_vms_stack_comm.h"
#include <vms/vms.h>

class CServerLink:public ILink
{
#ifdef UNITTEST
    friend class MockCServerLink;
#endif

public:
    CServerLink();
    virtual ~CServerLink();

public:
    // ��ͻ��˷�����Ϣ
    virtual int32_t sendMsg(const uint32_t unHandleIndex,
                           const char* pszMsg,
                           const uint32_t unMsgLength,
                           const uint32_t unTimeout);

    // �����������˵ļ���
    virtual int32_t startConn();

    // ֹͣ�������˵�ĳ�����ӣ�unHandleIndex��ʶ���ӵ��ڲ������
    virtual int32_t stopConn(const uint32_t unHandleIndex);

    // ֹͣ�������˵���������
    virtual int32_t stopConn();

    // �����������ţ���ȡ�������������ӵĿͻ��˵�ַ
    virtual int32_t getAddrByIndex(const uint32_t unHandleIndex, ADDR &stAddr);

public:
    void stopListen();// ֹͣ����

protected:
    int32_t init(ISvsMsgHandler* pMsgHandler, uint32_t unLinkId, const ADDR* pstLocalAddr, const ADDR* pstRemoteAddr);
    virtual int32_t handleRecv();
    virtual int32_t handleSend();
    virtual void handleTimeout();
    virtual void handleClose();

private:
    UTAPI void setSocketOption();

private:
    typedef std::map<uint32_t, ILink*> CONN_MAP;
    typedef CONN_MAP::iterator             CONN_ITOR;

private:
    ADDR                                    m_stLocalAddr;// �������ı��ؼ����ַ
    char                                    m_szLocalIp[STR_IP_LEN]; // ��ֽṹ��ַ
    uint32_t                            m_unHandleCnt;
    CONN_MAP                                m_ConnMap;// <clientIndex, ILink*>
    pthread_rwlock_t                        m_ConnMapLock;

friend class CLinkFactory;
};

#endif

