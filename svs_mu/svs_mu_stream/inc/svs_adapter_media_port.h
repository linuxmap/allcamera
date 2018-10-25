#ifndef __CSTREAMMEDIAPORT_H__
#define __CSTREAMMEDIAPORT_H__

#include <map>
#include "svs_ace_header.h"
#include "svs_adapter_config.h"
#include "svs_adapter_network_handle.h"

/// ����RTP Udp�˿ڶ�
typedef struct _stRTP_UDP_PORT_INFO
{
    CNetworkHandle* pRtpHandle;
    CNetworkHandle* pRtcpHandle;
}RTP_UDP_PORT_INFO;


/// ����RTP TCP���ö˿���Ϣ�ṹ
typedef struct _stRTP_TCP_PORT_INFO
{
    uint32_t    unUseCount;          /// �������
    CNetworkHandle* pHandle;
}RTP_TCP_REUSE_INFO;

/// ����UDP���ö˿ڽṹ
typedef struct _stREUSED_UDP_PORT_INFO
{
    uint32_t      unUseCount;
    RTP_UDP_PORT_INFO stPort;
}RTP_UDP_REUSE_INFO;

/// ����һ���˿��б�
typedef std::list<uint16_t>               PORT_LIST;
typedef std::list<uint16_t>::iterator     PORT_LIST_ITER;

/// ����˿ڼ�����ӳ���
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

    /// ��ʼ��
    int32_t init(uint32_t unIp,
             uint16_t usTcpMediaPort,
             const MediaPortConfig *pUdpConfig,
             uint16_t usEnableEhome,
             const MediaPortConfig *pEhomeConfig);

    void close();


    uint32_t getAllPortNum() const;

    /// ����һ��TCP�˿�
    CNetworkHandle* getTcpMediaHandle();


    /// ����һ��UDP�˿�(һ���˿�(ż��)��������RTP����һ���˿���������RTCP��)
    int32_t allocRtpUdpPort(CNetworkHandle *&pRtpHandle,
                        CNetworkHandle *&pRtcpHandle);

    /// ����һ��ehome�˿�
    int32_t allocEhomeMediaPort(CHandle *&pHandle);

    /// �ͷ�һ��TCP�˿�
    int32_t releaseRtpTcpPort(CNetworkHandle *&pHandle)const;

    /// �ͷ�һ��UDP�˿�(һ���˿�(ż��)��������RTP����һ���˿���������RTCP��)
    int32_t releaseRtpUdpPort(CNetworkHandle *&pRtpHandle,
                          CNetworkHandle *&pRtcpHandle);



    /// �ͷ�һ��ehome�˿�
    int32_t releaseEhomeMediaPort(CHandle *&pEhomeHandle);

private:
    int32_t createRtpTcpPort(uint16_t usTcpMediaPort);

    int32_t createRtpUdpPort(PORT_LIST &list);

    int32_t createEhomePort(PORT_LIST &list);

private:
    ACE_INET_Addr         m_InterfaceAddr;            /// ���������ĵ�ַ��ֻ��IP��Ч

    ACE_RW_Thread_Mutex   m_PortMutex;             /// Udp�˿ڲ�����

    uint32_t              m_unPortNum;
    UDP_PORT_MAP          m_UnUsedUdpPortMap;         /// δ�����RTP Udp�˿��б�
    UDP_PORT_MAP          m_AllocUdpPortMap;          /// �ѷ����RTP Udp�˿��б�

    uint16_t              m_usEnableEhome;
    uint32_t              m_unEhomePortNum;
    EHOME_PORT_MAP        m_EhomePortMap;             /// Ehome�˿��б�

    CNetworkHandle*       m_pTcpMediaHandle;

};

#endif // __CMEDIAINTERFACE_H__
