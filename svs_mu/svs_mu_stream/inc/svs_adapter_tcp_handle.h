#ifndef __CTcpHandle_H__
#define __CTcpHandle_H__

#include <map>
#include "svs_adapter_network_handle.h"
//#include "svs_adapter_tcp_connect.h"

class CTcpConnect;

typedef struct _stTcpConnectInfo
{
    uint32_t StartTime;
    CTcpConnect* TcpConnect;
} TCP_CONNECT_INFO;



class CTcpHandle : public CNetworkHandle
{
public:
    CTcpHandle();

    virtual ~CTcpHandle();

    int32_t openHandle(const ACE_INET_Addr &localAddr);

    void closeHandle();

    int32_t startHandle(uint64_t ullStreamID, const ACE_INET_Addr &remoteAddr);

    int32_t startActiveConnect(uint64_t ullStreamID,
                           const ACE_INET_Addr &remoteAddr,
                           ACE_INET_Addr &localAddr);

    void setInterleaveNum(uint64_t ullStreamID, char cVideoNum, char cAudioNum);

    int32_t stopHandle(uint64_t ullStreamID);

    int32_t send_n(uint64_t ullStreamID,
                const char *pDataBuffer,
                uint32_t unDataSize,
                const ACE_INET_Addr &peerAddr);

    int32_t sendMessage(uint64_t ullStreamID,
                    const char *pDataBuffer,
                    uint32_t unDataSize,
                    const ACE_INET_Addr &peerAddr);


    int32_t sendMessageBlock(uint64_t ullStreamID,
                        ACE_Message_Block **pMessage,
                        uint32_t unMsgCount,
                        const ACE_INET_Addr &peerAddr);

    int32_t sendv(uint64_t ullStreamID,
            const iovec* pIovec,
            uint32_t vecSize,
            uint32_t unDataSize);

    bool checkHandleStatus(uint64_t ullStreamID, uint32_t unTimeout);

    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask);

    int32_t handle_input(ACE_HANDLE handle);

    ACE_HANDLE get_handle() const;

    virtual CTcpConnect* createTcpConnect() = 0;

    virtual void destroyTcpConnect(CTcpConnect *&pConnect) = 0;

    int32_t addTcpConnect(CTcpConnect* pTcpConnect);

    int32_t delTcpConnect(CTcpConnect* pTcpConnect);

public:
    typedef std::map<uint64_t, TCP_CONNECT_INFO>           CONNECT_MAP;
    typedef std::map<uint64_t, TCP_CONNECT_INFO>::iterator CONNECT_MAP_ITER;

protected:
    ACE_SOCK_Acceptor      m_TcpAcceptor; 

    CONNECT_MAP            m_ConnectMap; 
    ACE_RW_Thread_Mutex    m_ConnectMapMutex;
};

#endif // __CTcpHandle_H__
