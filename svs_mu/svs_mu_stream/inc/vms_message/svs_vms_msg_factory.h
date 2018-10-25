/*
 * MduMsgFactory.h
 *
 *  Created on: 2010-12-30
 *      Author:
 */

#ifndef MDUMSGFACTORY_H_
#define MDUMSGFACTORY_H_

#include "svs_ace_header.h"
#include "svs_vms_message.h"

class CMduMsgFactory
{
public:
    static CMduMsgFactory* instance()
    {
        if (NULL == g_mduMsgFactoy)
        {
            try
            {
                g_mduMsgFactoy = new CMduMsgFactory;
            }
            catch(...)
            {
            }
        }

        return g_mduMsgFactoy;
    }
    virtual ~CMduMsgFactory();

    /// ��ݽ��յ�����Ϣ������һ����Ϣ��
    /// ע�⣺�´�������Ϣ�����˴������ݿ�
    UTAPI int32_t createSvsMsg(const char* pMsg,
            uint32_t unMsgLength,
            CMduSvsMessage* &pMessage)const;

    /// �����Ϣ���ʹ���һ����Ϣ��
    /// ע�⣺��ΪSVS��Ϣ�пɱ��ֶΣ����Գ�����Ҫ���ָ��
    UTAPI int32_t createSvsMsg(uint32_t unMsgType,
                    uint32_t unMsgLength,
                    uint32_t unTransNo,
                    CMduSvsMessage* &pMessage)const;

    /// ɾ����Ϣ
    UTAPI void destroySvsMsg(CMduSvsMessage* &pMessage)const;

    /// ��ȡ������Ϣ�Ĵ����
    UTAPI uint32_t getReqTransactionNo();

    /// ��ȡ��Ӧ��Ϣ�Ĵ����
    UTAPI uint32_t getRespTransactionNo(uint32_t unReqTransNo)const;

private:
#define MDU_RESP_TRANS_NO_MASK  0x8000          /// ������Ӧ����ŵı�־

private:
    CMduMsgFactory();

    /// �����Ϣ���ʹ���һ����Ϣ
    UTAPI CMduSvsMessage* createSvsMsgByType(uint32_t unMsgType)const;

    UTAPI CMduSvsMessage* createReqMsg(uint32_t unMsgType)const;
    UTAPI CMduSvsMessage* createRespMsg(uint32_t unMsgType)const;
private:
    static CMduMsgFactory* g_mduMsgFactoy;

    uint32_t     m_unTransNo;
    ACE_Thread_Mutex m_TransNoMutex;

#ifdef UNITTEST
    friend class MockCMduMsgFactory;
#endif
};

#endif /* MDUMSGFACTORY_H_ */
