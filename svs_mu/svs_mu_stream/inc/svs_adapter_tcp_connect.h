/*
 * TcpConnect.h
 *
 *  Created on: 2016-1-5
 *      Author:
 */

#ifndef TCPCONNECT_H_
#define TCPCONNECT_H_
#include <list>
#include "svs_ace_header.h"
#include "svs_static_preassign_buffer.h"
#include "svs_adapter_network_handle.h"

class CTcpHandle;
/**
* Class name:   CTcpConnect
* \brief  ͨ�õ�TCP������
*
* Description:  ����ʵ����TCP���ӵ�һ�㷽�����������ݵĽ��ռ����ͣ�
*               �����������ݵĴ�����Ҫ���ఴ��ͬ���͵�ý����ʵ��
*/
class CTcpConnect : public ACE_Event_Handler
{
public:
    CTcpConnect();

    virtual ~CTcpConnect();

    /// �༶����£�Connect��Ҫ֧��������Զ˷�������
    int32_t start(const ACE_INET_Addr &localAddr, const ACE_INET_Addr &remoteAddr, bool bActiveFlag);

    /// �ر�TCP����
    int32_t close();

    /// ���ø����
    virtual int32_t setTcpHandle(CTcpHandle *pTcpHandle);

    NETWORK_HANDLE_STATUS getStatus();

    void setStatus(NETWORK_HANDLE_STATUS status);

    /// ������ID,������������ʱ��Ҫ����
    void setStreamID(uint64_t ullStreamID);

    /// ��ȡ��ID
    uint64_t getStreamID() const;

    /// ������Ƶ��֯ͨ���ţ�RTCP�Ľ�֯ͨ���Ų������ã��Զ�ȡ+1
    void setVideoInterleaveNum(char cNo);

    char getVideoInterleaveNum() const;

    /// ������Ƶ��֯ͨ���ţ�RTCP�Ľ�֯ͨ���Ų������ã��Զ�ȡ+1
    void setAudioInterleaveNum(char cNo);

    char getAudioInterleaveNum() const;

    /// �������Ӿ��
    void set_handle(ACE_HANDLE handle);

    /// ��ȡ���Ӿ��
    ACE_HANDLE get_handle() const;

    ACE_INET_Addr getLocalAddr() const;

    int32_t send_n(const char *pDataBuffer,
               uint32_t unDataSize,
               const ACE_INET_Addr &peerAddr);

    /// ������Ϣ
    int32_t sendMessage(const char *pDataBuffer,
                    uint32_t unDataSize,
                    const ACE_INET_Addr &peerAddr);

    int32_t sendMessage(ACE_Message_Block **pMessage,
                    uint32_t unMsgCount,
                    const ACE_INET_Addr &peerAddr);

    int32_t sendv(const iovec* pIovec,
              uint32_t vecSize,
              uint32_t unDataSize);

    /// ��������¼�
    int32_t handle_input(ACE_HANDLE handle);

    /// �������ӹر��¼�
    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask);

    // �������״̬���������״̬�쳣������false
    virtual bool checkHandleStatus(uint32_t unTimeout);

    /// ������յ������ݣ����سɹ�������ֽ�������Ҫ����ʵ��
    virtual int32_t handleRecvedData(const char* pData, uint32_t unDataSize) = 0;

public:
    typedef std::list<ACE_Message_Block*>   TCP_SEND_LIST;
    typedef TCP_SEND_LIST::iterator         TCP_SEND_LIST_ITER;
private:
    bool checkIsDisconnect(int32_t nErrNo) const;

    int32_t setSockOpt();

    int32_t connectPeer();

    // ����֮ǰδ�����������
    int32_t sendLeastData();
   enum
   {
        MAX_BUF_SIGNAL_NUM = 20
    };
protected:
    NETWORK_HANDLE_STATUS       m_Status;
    ACE_Thread_Mutex            m_StatusMutex;

    CTcpHandle*                 m_pTcpHandle;           /// ������Connect��Handle

    ACE_HANDLE                  m_sockFD;               /// ���Ӿ��
    ACE_INET_Addr               m_localAddr;            /// ���ص�ַ
    ACE_INET_Addr               m_remoteAddr;           /// Զ�˵�ַ
    uint32_t                       m_ulLastRecvTime;       // ���һ�ν���ʱ��
    uint32_t                       m_ulBufFullTime;        // ���һ�λ���������ʱ��

    uint64_t                      m_ullStreamID;          /// һ��TCP������ֻ����һ����ID
    char                        m_cVideoInterleaveNum;
    char                        m_cAudioInterleaveNum;

    ACE_Thread_Mutex            m_SendMutex;            /// ��ֹ���߳�ͬʱ��һ�����ӷ�������

    Static_PreAssign_Buffer*    m_pRecvBuffer;          /// ���ջ�����
    TCP_SEND_LIST               m_SendList;             /// δ����������ݻ�����

    uint32_t                      m_ulSignalMsgBufNum;
};

#endif /* TCPCONNECT_H_ */
