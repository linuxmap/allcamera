#ifndef __STREAMSCCCONNECTOR_H__
#define __STREAMSCCCONNECTOR_H__

#include "svs_ace_header.h"
#include "svs_adapter_vms_client_handle.h"
#include "svs_adapter_session.h"

class CBusinessReportTimer : public ACE_Event_Handler
{
public:
    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);
};

/**
* @class   StreamSccConnector
* @brief   SCC������
* Description: Stream��������SCC�����ӣ�����ע�ᡢ������⼰��ע�ᣬ����SVS��Ϣ�Ľ���/����
*/
class CStreamSccConnector : public CSvsClientHandle
{
    friend class CBusinessReportTimer;
public:
    CStreamSccConnector();

    virtual ~CStreamSccConnector();

    /// ����SCC�����ӣ��������ֱ�Ӵ�StreamConfig�л�ȡ
    int32_t openConnector(ACE_Message_Queue<ACE_SYNCH>* pRcvQueue);

    /// �ر���SCC������
    void closeConnector();

    /// �麯��ʵ�֣�����������Ͻ��յ�����Ϣ
    void handleRecvMessage(ACE_Message_Block *pMsg);

    /// �麯��ʵ�֣�������ʧ�ܵ���Ϣ
    void handleFailedMessage(ACE_Message_Block *pMsg);

    /// �麯��ʵ�֣����������ϱ�
    void handleTaskReport();

    /// �麯��ʵ�֣�����ע����Ӧ
    void registerNotify();

    /// �麯�����أ������������ע����Ϣ
    void registServer();

    void connectNotify(const ConnEvent* pstEvent);
protected:
    void startBusinessReportTimer();

    void stopBusinessReportTimer();
private:
    void sendDistributeTask(uint32_t unNum, uint32_t unSeq, const CStreamSession* pSession);
    void setTagIp(uint32_t unIpType, uint32_t unIndex, uint32_t unIpAddr,
                    char* pBuf, uint32_t& unBufLen)const;
private:
    /// ������Ϣ����Ķ���
    ACE_Message_Queue<ACE_SYNCH>   *m_pRecvMsgQueue;

    /// ҵ��Ự�ϱ���ʱ��
    CBusinessReportTimer*           m_pBusinessReportTimer;
    int32_t                         m_lReportTimerID;
};

#endif // __STREAMSCCCONNECTOR_H__
