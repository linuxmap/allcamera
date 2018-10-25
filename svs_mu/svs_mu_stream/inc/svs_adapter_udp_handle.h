#ifndef __CUdpConnectHandle_H__
#define __CUdpConnectHandle_H__

#include <svs_adapter_network_handle.h>

class CUdpHandle : public CNetworkHandle
{
public:
    /** Default constructor */
    CUdpHandle();

    /** Default destructor */
    virtual ~CUdpHandle();

    int32_t openHandle(const ACE_INET_Addr &localAddr);

    void closeHandle();

    int32_t startHandle(uint64_t ullStreamID, const ACE_INET_Addr &remoteAddr);

    int32_t stopHandle(uint64_t ullStreamID);

    int32_t sendMessage(uint64_t ullStreamID,
                    const char *pDataBuffer,
                    uint32_t unDataSize,
                    const ACE_INET_Addr &peerAddr);

    // �������״̬���������״̬�쳣������false
    bool checkHandleStatus(uint64_t ullStreamID, uint32_t unTimeout);

    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask);

    ACE_HANDLE get_handle() const;

    int32_t handle_input(ACE_HANDLE handle);

    virtual int32_t handleRecvedData(ACE_Message_Block *pMsg, ACE_INET_Addr &remoteAddr) = 0;

protected:
    /// �����ӻ�δ��ʼʱ��������������ô˷�������
    void recvInvalidData();

protected:
    ACE_SOCK_Dgram           m_UdpSocket;
    uint32_t            m_ulLastInvalidAlarmTime;   // ��һ����Ч���ݸ澯ʱ��
    uint32_t            m_ulLastRecvTime;
    ACE_INET_Addr            m_RemoteAddr;

    uint32_t             m_unStreamCount;     /// �����������ĸ���
    uint64_t       m_ullStreamID;        /// ֻ�����һ����ID��������ʱ��ID��RTP��չͷ��ȡ
    ACE_Thread_Mutex         m_ListMutex;

    uint32_t            m_enPlayType;
};

#endif // __CUdpConnectHandle_H__
