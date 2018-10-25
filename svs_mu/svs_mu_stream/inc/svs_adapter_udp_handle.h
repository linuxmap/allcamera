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

    // 检查连接状态，如果连接状态异常，返回false
    bool checkHandleStatus(uint64_t ullStreamID, uint32_t unTimeout);

    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask);

    ACE_HANDLE get_handle() const;

    int32_t handle_input(ACE_HANDLE handle);

    virtual int32_t handleRecvedData(ACE_Message_Block *pMsg, ACE_INET_Addr &remoteAddr) = 0;

protected:
    /// 当连接还未开始时，如果有数据来用此方法接收
    void recvInvalidData();

protected:
    ACE_SOCK_Dgram           m_UdpSocket;
    uint32_t            m_ulLastInvalidAlarmTime;   // 上一次无效数据告警时间
    uint32_t            m_ulLastRecvTime;
    ACE_INET_Addr            m_RemoteAddr;

    uint32_t             m_unStreamCount;     /// 本连接上流的个数
    uint64_t       m_ullStreamID;        /// 只保存第一个流ID，多条流时流ID从RTP扩展头中取
    ACE_Thread_Mutex         m_ListMutex;

    uint32_t            m_enPlayType;
};

#endif // __CUdpConnectHandle_H__
