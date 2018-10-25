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
* @brief   SCC连接器
* Description: Stream服务中与SCC的连接，负责注册、心跳检测及重注册，还有SVS消息的接收/发送
*/
class CStreamSccConnector : public CSvsClientHandle
{
    friend class CBusinessReportTimer;
public:
    CStreamSccConnector();

    virtual ~CStreamSccConnector();

    /// 打开与SCC的连接，相关配置直接从StreamConfig中获取
    int32_t openConnector(ACE_Message_Queue<ACE_SYNCH>* pRcvQueue);

    /// 关闭与SCC的连接
    void closeConnector();

    /// 虚函数实现：处理从连接上接收到的消息
    void handleRecvMessage(ACE_Message_Block *pMsg);

    /// 虚函数实现：处理发送失败的消息
    void handleFailedMessage(ACE_Message_Block *pMsg);

    /// 虚函数实现：处理任务上报
    void handleTaskReport();

    /// 虚函数实现：处理注册响应
    void registerNotify();

    /// 虚函数重载：向服务器发送注册消息
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
    /// 接收消息保存的队列
    ACE_Message_Queue<ACE_SYNCH>   *m_pRecvMsgQueue;

    /// 业务会话上报定时器
    CBusinessReportTimer*           m_pBusinessReportTimer;
    int32_t                         m_lReportTimerID;
};

#endif // __STREAMSCCCONNECTOR_H__
