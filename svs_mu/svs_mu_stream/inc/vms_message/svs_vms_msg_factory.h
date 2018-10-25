/*
 * StreamMsgFactory.h
 *
 *  Created on: 2010-12-30
 *      Author:
 */

#ifndef STREAMMSGFACTORY_H_
#define STREAMMSGFACTORY_H_

#include "svs_ace_header.h"
#include "svs_vms_message.h"

class CStreamMsgFactory
{
public:
    static CStreamMsgFactory* instance()
    {
        if (NULL == g_streamMsgFactoy)
        {
            try
            {
                g_streamMsgFactoy = new CStreamMsgFactory;
            }
            catch(...)
            {
            }
        }

        return g_streamMsgFactoy;
    }
    virtual ~CStreamMsgFactory();

    /// ��ݽ��յ�����Ϣ������һ����Ϣ��
    /// ע�⣺�´�������Ϣ�����˴������ݿ�
    UTAPI int32_t createSvsMsg(const char* pMsg,
            uint32_t unMsgLength,
            CStreamSvsMessage* &pMessage)const;

    /// �����Ϣ���ʹ���һ����Ϣ��
    /// ע�⣺��ΪSVS��Ϣ�пɱ��ֶΣ����Գ�����Ҫ���ָ��
    UTAPI int32_t createSvsMsg(uint32_t unMsgType,
                    uint32_t unMsgLength,
                    uint32_t unTransNo,
                    CStreamSvsMessage* &pMessage)const;

    /// ɾ����Ϣ
    UTAPI void destroySvsMsg(CStreamSvsMessage* &pMessage)const;

    /// ��ȡ������Ϣ�Ĵ����
    UTAPI uint32_t getReqTransactionNo();

    /// ��ȡ��Ӧ��Ϣ�Ĵ����
    UTAPI uint32_t getRespTransactionNo(uint32_t unReqTransNo)const;

private:
#define STREAM_RESP_TRANS_NO_MASK  0x8000          /// ������Ӧ����ŵı�־

private:
    CStreamMsgFactory();

    /// �����Ϣ���ʹ���һ����Ϣ
    UTAPI CStreamSvsMessage* createSvsMsgByType(uint32_t unMsgType)const;

    UTAPI CStreamSvsMessage* createReqMsg(uint32_t unMsgType)const;
    UTAPI CStreamSvsMessage* createRespMsg(uint32_t unMsgType)const;
private:
    static CStreamMsgFactory* g_streamMsgFactoy;

    uint32_t     m_unTransNo;
    ACE_Thread_Mutex m_TransNoMutex;

#ifdef UNITTEST
    friend class MockCStreamMsgFactory;
#endif
};

#endif /* STREAMMSGFACTORY_H_ */
