#ifndef __CSTREAMMEDIAPORT_H__
#define __CSTREAMMEDIAPORT_H__

#include <map>
#include "svs_ace_header.h"
#include "svs_adapter_config.h"
#include "svs_adapter_network_handle.h"

/// 定义RTP Udp端口对
typedef struct _stRTP_UDP_PORT_INFO
{
    CNetworkHandle* pRtpHandle;
    CNetworkHandle* pRtcpHandle;
}RTP_UDP_PORT_INFO;


/// 定义RTP TCP重用端口信息结构
typedef struct _stRTP_TCP_PORT_INFO
{
    uint32_t    unUseCount;          /// 分配次数
    CNetworkHandle* pHandle;
}RTP_TCP_REUSE_INFO;

/// 定义UDP重用端口结构
typedef struct _stREUSED_UDP_PORT_INFO
{
    uint32_t      unUseCount;
    RTP_UDP_PORT_INFO stPort;
}RTP_UDP_REUSE_INFO;

/// 定义一个端口列表
typedef std::list<uint16_t>               PORT_LIST;
typedef std::list<uint16_t>::iterator     PORT_LIST_ITER;

/// 定义端口及连接映射表
typedef std::map<uint16_t, CNetworkHandle*>              TCP_PORT_MAP;
typedef std::map<uint16_t, CNetworkHandle*>::iterator    TCP_PORT_MAP_ITER;

typedef std::map<uint16_t, RTP_UDP_PORT_INFO>            UDP_PORT_MAP;
typedef std::map<uint16_t, RTP_UDP_PORT_INFO>::iterator  UDP_PORT_MAP_ITER;

typedef std::map<uint16_t, CHandle*>                     EHOME_PORT_MAP;
typedef std::map<uint16_t, CHandle*>::iterator           EHOME_PORT_MAP_ITER;

class CStreamMediaPort
{
public:
    /** Default constructor */
    CStreamMediaPort();

    /** Default destructor */
    virtual ~CStreamMediaPort();

    /// 初始化
    int32_t init(uint32_t unIp,
             uint16_t usTcpMediaPort,
             const MediaPortConfig *pUdpConfig,
             uint16_t usEnableEhome,
             const MediaPortConfig *pEhomeConfig);

    void close();


    uint32_t getAllPortNum() const;

    /// 分配一个TCP端口
    CNetworkHandle* getTcpMediaHandle();


    /// 分配一对UDP端口(一个端口(偶數)用来接收RTP包，一个端口用来接收RTCP包)
    int32_t allocRtpUdpPort(CNetworkHandle *&pRtpHandle,
                        CNetworkHandle *&pRtcpHandle);

    /// 分配一个ehome端口
    int32_t allocEhomeMediaPort(CHandle *&pHandle);

    /// 释放一个TCP端口
    int32_t releaseRtpTcpPort(CNetworkHandle *&pHandle)const;

    /// 释放一对UDP端口(一个端口(偶數)用来接收RTP包，一个端口用来接收RTCP包)
    int32_t releaseRtpUdpPort(CNetworkHandle *&pRtpHandle,
                          CNetworkHandle *&pRtcpHandle);



    /// 释放一个ehome端口
    int32_t releaseEhomeMediaPort(CHandle *&pEhomeHandle);

private:
    int32_t createRtpTcpPort(uint16_t usTcpMediaPort);

    int32_t createRtpUdpPort(PORT_LIST &list);

    int32_t createEhomePort(PORT_LIST &list);

private:
    ACE_INET_Addr         m_InterfaceAddr;            /// 保存网卡的地址，只有IP有效

    ACE_RW_Thread_Mutex   m_PortMutex;             /// Udp端口操作锁

    uint32_t              m_unPortNum;
    UDP_PORT_MAP          m_UnUsedUdpPortMap;         /// 未分配的RTP Udp端口列表
    UDP_PORT_MAP          m_AllocUdpPortMap;          /// 已分配的RTP Udp端口列表

    uint16_t              m_usEnableEhome;
    uint32_t              m_unEhomePortNum;
    EHOME_PORT_MAP        m_EhomePortMap;             /// Ehome端口列表

    CNetworkHandle*       m_pTcpMediaHandle;

};

#endif // __CMEDIAINTERFACE_H__
