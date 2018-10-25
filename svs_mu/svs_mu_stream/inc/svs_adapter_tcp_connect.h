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
* \brief  通用的TCP连接类
*
* Description:  本类实现了TCP连接的一般方法，包含数据的接收及发送，
*               不过接收数据的处理需要子类按不同类型的媒体来实现
*/
class CTcpConnect : public ACE_Event_Handler
{
public:
    CTcpConnect();

    virtual ~CTcpConnect();

    /// 多级情况下，Connect需要支持主动向对端发起连接
    int32_t start(const ACE_INET_Addr &localAddr, const ACE_INET_Addr &remoteAddr, bool bActiveFlag);

    /// 关闭TCP连接
    int32_t close();

    /// 设置父句柄
    virtual int32_t setTcpHandle(CTcpHandle *pTcpHandle);

    NETWORK_HANDLE_STATUS getStatus();

    void setStatus(NETWORK_HANDLE_STATUS status);

    /// 设置流ID,主动发起连接时需要设置
    void setStreamID(uint64_t ullStreamID);

    /// 获取流ID
    uint64_t getStreamID() const;

    /// 设置视频交织通道号，RTCP的交织通道号不用设置，自动取+1
    void setVideoInterleaveNum(char cNo);

    char getVideoInterleaveNum() const;

    /// 设置视频交织通道号，RTCP的交织通道号不用设置，自动取+1
    void setAudioInterleaveNum(char cNo);

    char getAudioInterleaveNum() const;

    /// 设置连接句柄
    void set_handle(ACE_HANDLE handle);

    /// 获取连接句柄
    ACE_HANDLE get_handle() const;

    ACE_INET_Addr getLocalAddr() const;

    int32_t send_n(const char *pDataBuffer,
               uint32_t unDataSize,
               const ACE_INET_Addr &peerAddr);

    /// 发送消息
    int32_t sendMessage(const char *pDataBuffer,
                    uint32_t unDataSize,
                    const ACE_INET_Addr &peerAddr);

    int32_t sendMessage(ACE_Message_Block **pMessage,
                    uint32_t unMsgCount,
                    const ACE_INET_Addr &peerAddr);

    int32_t sendv(const iovec* pIovec,
              uint32_t vecSize,
              uint32_t unDataSize);

    /// 处理接收事件
    int32_t handle_input(ACE_HANDLE handle);

    /// 处理连接关闭事件
    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask);

    // 检查连接状态，如果连接状态异常，返回false
    virtual bool checkHandleStatus(uint32_t unTimeout);

    /// 处理接收到的数据，返回成功处理的字节数。需要子类实现
    virtual int32_t handleRecvedData(const char* pData, uint32_t unDataSize) = 0;

public:
    typedef std::list<ACE_Message_Block*>   TCP_SEND_LIST;
    typedef TCP_SEND_LIST::iterator         TCP_SEND_LIST_ITER;
private:
    bool checkIsDisconnect(int32_t nErrNo) const;

    int32_t setSockOpt();

    int32_t connectPeer();

    // 发送之前未发送完的数据
    int32_t sendLeastData();
   enum
   {
        MAX_BUF_SIGNAL_NUM = 20
    };
protected:
    NETWORK_HANDLE_STATUS       m_Status;
    ACE_Thread_Mutex            m_StatusMutex;

    CTcpHandle*                 m_pTcpHandle;           /// 创建本Connect的Handle

    ACE_HANDLE                  m_sockFD;               /// 连接句柄
    ACE_INET_Addr               m_localAddr;            /// 本地地址
    ACE_INET_Addr               m_remoteAddr;           /// 远端地址
    uint32_t                       m_ulLastRecvTime;       // 最近一次接收时间
    uint32_t                       m_ulBufFullTime;        // 最近一次缓冲区满的时间

    uint64_t                      m_ullStreamID;          /// 一条TCP连接上只会有一个流ID
    char                        m_cVideoInterleaveNum;
    char                        m_cAudioInterleaveNum;

    ACE_Thread_Mutex            m_SendMutex;            /// 防止多线程同时对一个连接发送数据

    Static_PreAssign_Buffer*    m_pRecvBuffer;          /// 接收缓冲区
    TCP_SEND_LIST               m_SendList;             /// 未发送完的数据缓冲区

    uint32_t                      m_ulSignalMsgBufNum;
};

#endif /* TCPCONNECT_H_ */
