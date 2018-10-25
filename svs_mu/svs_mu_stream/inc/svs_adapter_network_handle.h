#ifndef __NETWORKHANDLE_H__
#define __NETWORKHANDLE_H__

#include <svs_ace_header.h>

#define SOCKET_RECV_BUFFER_SIZE       (32 * 1024)

#define SOCKET_SEND_BUFFER_SIZE       (32 * 1024)

#define INVALID_DATA_ALARM_INTERVAL   3

#define   MAX_UDP_PACK_SIZE           (2 * 1024)


typedef enum _enNETWORK_HANDLE_STATUS
{
    NETWORK_HANDLE_STATUS_INIT,
    NETWORK_HANDLE_STATUS_OPEN,
    NETWORK_HANDLE_STATUS_START,
    NETWORK_HANDLE_STATUS_DISCONNECT,
    NETWORK_HANDLE_STATUS_ABNORMAL
}NETWORK_HANDLE_STATUS;

class CHandle
{
public:
    /** Default constructor */
    CHandle();

    /** Default destructor */
    virtual ~CHandle();

    virtual int32_t openHandle(const ACE_INET_Addr &localAddr) = 0;

    virtual void closeHandle() = 0;

    virtual int32_t startHandle(uint64_t streamID, const ACE_INET_Addr &remoteAddr) = 0;

    virtual int32_t stopHandle(uint64_t streamID) = 0;

    virtual int32_t sendMessage(uint64_t ullStreamID,
                            const char *pDataBuffer,
                            uint32_t unDataSize,
                            const ACE_INET_Addr &peerAddr) = 0;

    virtual bool checkHandleStatus(uint64_t ullStreamID, uint32_t unTimeout) = 0;

    void setLocalAddr(const ACE_INET_Addr &localAddr);

    ACE_INET_Addr& getLocalAddr();

    void setStatus(NETWORK_HANDLE_STATUS status);

    NETWORK_HANDLE_STATUS getStatus() const;

private:
    ACE_INET_Addr           m_LocalAddr;

    NETWORK_HANDLE_STATUS   m_HandleStatus;
    ACE_Time_Value          m_StatusTime;
};

class CNetworkHandle:public ACE_Event_Handler,public CHandle
{
public:
    /** Default constructor */
    CNetworkHandle(){};

    /** Default destructor */
    virtual ~CNetworkHandle(){};
};

#endif // __NETWORKHANDLE_H__
