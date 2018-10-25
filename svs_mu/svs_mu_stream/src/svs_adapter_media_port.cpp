#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_net_conn_manager.h"
#include "svs_adapter_media_port.h"

#include "svs_adapter_network_handle.h"
#include "svs_adapter_rtp_tcp_handle.h"
#include "svs_adapter_rtp_udp_handle.h"
#include "svs_adapter_rtcp_udp_handle.h"
#include "svs_adapter_ehome_handle.h"
#include "svs_utility.h"


CMduMediaPort::CMduMediaPort()
{
    m_unPortNum = 0;
    m_pTcpMediaHandle = NULL;
    m_usEnableEhome = 0;
    m_unEhomePortNum = 0;
}

CMduMediaPort::~CMduMediaPort()
{
    m_pTcpMediaHandle = NULL;
}

int32_t CMduMediaPort::init(uint32_t unIp,
                        uint16_t usTcpMediaPort,
                        const MediaPortConfig *pUdpConfig,
                        uint16_t usEnableEhome,
                        const MediaPortConfig *pEhomeConfig)
{
    if ((0 == unIp)
        || (NULL == pUdpConfig))
    {
        return RET_ERR_PARAM;
    }

    m_InterfaceAddr.set((uint16_t) 0, unIp);

    PORT_LIST list;
    for (uint16_t i = 0; i < pUdpConfig->usPortRangeNum; i++)
    {
        uint16_t usStartPort = pUdpConfig->stPortRange[i].usStartPort;
        for (; usStartPort <= pUdpConfig->stPortRange[i].usEndPort && usStartPort >= pUdpConfig->stPortRange[i].usStartPort; usStartPort++)
        {
            list.push_back(usStartPort);
        }
    }

    (void)createRtpUdpPort(list);

    if (RET_OK != createRtpTcpPort(usTcpMediaPort))
    {
        SVS_LOG((SVS_LM_WARNING,"create rtp tcp port fail."));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"media interface[%s] creat [%d] udp port, tcp media port[%d].",
                m_InterfaceAddr.get_host_addr(),
                m_UnUsedUdpPortMap.size(),
                usTcpMediaPort));
    m_usEnableEhome = usEnableEhome;
    if(!m_usEnableEhome)
    {
        return RET_OK;
    }

    if(!NET_ESTREAM_Init())
    {
        SVS_LOG((SVS_LM_WARNING,"ehome init the stream libary fail."));
        return RET_FAIL;
    }

    list.clear();
    for (uint16_t i = 0; i < pEhomeConfig->usPortRangeNum; i++)
    {
        uint16_t usStartPort = pEhomeConfig->stPortRange[i].usStartPort;
        for (; usStartPort <= pEhomeConfig->stPortRange[i].usEndPort && usStartPort >= pEhomeConfig->stPortRange[i].usStartPort; usStartPort++)
        {
            list.push_back(usStartPort);
        }
    }

    (void)createEhomePort(list);

    return RET_OK;
}


void CMduMediaPort::close()
{
    try
    {
        if (NULL != m_pTcpMediaHandle)
        {
            delete m_pTcpMediaHandle;
        }

    }
    catch(...)
    {}

    try
    {
        ACE_Guard<ACE_RW_Thread_Mutex> locker(m_PortMutex);
        RTP_UDP_PORT_INFO udpInfo;
        UDP_PORT_MAP_ITER iter = m_AllocUdpPortMap.begin();
        for (; iter != m_AllocUdpPortMap.end(); iter++)
        {
            udpInfo = iter->second;
            delete udpInfo.pRtpHandle;
            delete udpInfo.pRtcpHandle;
        }
        m_AllocUdpPortMap.clear();

        iter = m_UnUsedUdpPortMap.begin();
        for (; iter != m_UnUsedUdpPortMap.end(); iter++)
        {
            udpInfo = iter->second;
            delete udpInfo.pRtpHandle;
            delete udpInfo.pRtcpHandle;
        }
        m_UnUsedUdpPortMap.clear();
    }
    catch(...)
    {}

    m_unPortNum = 0;

    if(m_usEnableEhome)
    {
        NET_ESTREAM_Fini();
        CEhomeHandle *pEhomeHandle  = NULL;
        EHOME_PORT_MAP_ITER iter = m_EhomePortMap.begin();
        for(;iter != m_EhomePortMap.end();++iter)
        {
            pEhomeHandle = (CEhomeHandle*)iter->second;
            SVS_DELETE(pEhomeHandle);
        }

        m_EhomePortMap.clear();
    }


    SVS_LOG((SVS_LM_INFO,"mdu media interface [%s] closed.", m_InterfaceAddr.get_host_addr()));
    return;
}

uint32_t CMduMediaPort::getAllPortNum() const
{
    return m_unPortNum;
}



CNetworkHandle* CMduMediaPort::getTcpMediaHandle()
{
    return m_pTcpMediaHandle;
}


int32_t CMduMediaPort::allocRtpUdpPort(CNetworkHandle *&pRtpHandle,
                                   CNetworkHandle *&pRtcpHandle)
{
    ACE_Guard<ACE_RW_Thread_Mutex> locker(m_PortMutex);
    if (m_UnUsedUdpPortMap.empty())
    {
        SVS_LOG((SVS_LM_WARNING,"media interface[%s] has none udp port.",
                m_InterfaceAddr.get_host_addr()));
        return RET_FAIL;
    }

    UDP_PORT_MAP_ITER iter = m_UnUsedUdpPortMap.begin();
    RTP_UDP_PORT_INFO udpInfo = iter->second;

    pRtpHandle  = udpInfo.pRtpHandle;
    pRtcpHandle = udpInfo.pRtcpHandle;

    m_AllocUdpPortMap.insert(std::make_pair(iter->first, udpInfo));
    m_UnUsedUdpPortMap.erase(iter);

    SVS_LOG((SVS_LM_INFO,"success to alloc rtp udp port[%d-%d].",
            pRtpHandle->getLocalAddr().get_port_number(),
            pRtcpHandle->getLocalAddr().get_port_number()));
    return RET_OK;
}

int32_t CMduMediaPort::allocEhomeMediaPort(CHandle *&pHandle)
{
    CEhomeHandle *pEhomeHandle  = NULL;
    CEhomeHandle *pEhomeBestHandle  = NULL;
    uint16_t minCount = 0xFFFF;
    uint16_t usCount = 0;
    EHOME_PORT_MAP_ITER iter = m_EhomePortMap.begin();
    for(;iter != m_EhomePortMap.end();++iter)
    {
        pEhomeHandle = (CEhomeHandle*)iter->second;
        usCount = pEhomeHandle->getStreamCount();
        if(minCount > usCount)
        {
            minCount = usCount;
            pEhomeBestHandle = pEhomeHandle;
        }
    }

    pHandle = pEhomeBestHandle;

    return RET_OK;
}


int32_t CMduMediaPort::releaseRtpTcpPort(CNetworkHandle *&pHandle)const
{
    if (NULL == pHandle)
    {
        SVS_LOG((SVS_LM_ERROR,"Release rtp tcp port fail, handle is null."));
        return RET_FAIL;
    }

    pHandle = NULL;

    SVS_LOG((SVS_LM_INFO,"release tcp handle success."));

    return RET_OK;
}

int32_t CMduMediaPort::releaseRtpUdpPort(CNetworkHandle *&pRtpHandle,
                                     CNetworkHandle *&pRtcpHandle)
{
    if ((NULL == pRtpHandle) || (NULL == pRtcpHandle))
    {
        return RET_FAIL;
    }

    ACE_Guard<ACE_RW_Thread_Mutex> locker(m_PortMutex);
    uint16_t usRtpPort  = pRtpHandle->getLocalAddr().get_port_number();
    UDP_PORT_MAP_ITER iter = m_AllocUdpPortMap.find(usRtpPort);
    if (m_AllocUdpPortMap.end() == iter)
    {
        return RET_FAIL;
    }

    m_UnUsedUdpPortMap.insert(std::make_pair(iter->first, iter->second));
    m_AllocUdpPortMap.erase(iter);

    pRtpHandle  = NULL;
    pRtcpHandle = NULL;

    SVS_LOG((SVS_LM_INFO,"release rtp udp port[%d-%d] success.",
            usRtpPort,
            usRtpPort + 1));

    return RET_OK;
}


int32_t CMduMediaPort::releaseEhomeMediaPort(CHandle *&pEhomeHandle)
{
    //NOTHING TO DO
    return RET_OK;
}

int32_t CMduMediaPort::createRtpTcpPort(uint16_t usTcpMediaPort)
{
    ACE_INET_Addr addr(usTcpMediaPort, m_InterfaceAddr.get_ip_address());
    try
    {
        m_pTcpMediaHandle = new CRtpTcpHandle();
    }
    catch(...)
    {
        delete m_pTcpMediaHandle;
        return RET_ERR_SYS_NEW;
    }

    int32_t nRet = m_pTcpMediaHandle->openHandle(addr);
    if (RET_OK != nRet)
    {
        delete m_pTcpMediaHandle;
        return RET_FAIL;
    }

    return RET_OK;
}


int32_t CMduMediaPort::createRtpUdpPort(PORT_LIST &list)
{

    for (PORT_LIST_ITER iter = list.begin();
            iter != list.end(); iter++)
    {
        uint16_t usRtpPort = *iter;
        if (0 != usRtpPort % 2)
        {
            continue;
        }

        uint16_t usRtcpPort = usRtpPort + 1;
        if (list.end() == find(list.begin(), list.end(), usRtcpPort))
        {
            continue;
        }

        ACE_INET_Addr rtpAddr(usRtpPort, m_InterfaceAddr.get_ip_address());
        ACE_INET_Addr rtcpAddr(usRtcpPort, m_InterfaceAddr.get_ip_address());
        CNetworkHandle *pRtpHandle  = NULL;
        CNetworkHandle *pRtcpHandle = NULL;
        try
        {
            pRtpHandle  = new CRtpUdpHandle();
            pRtcpHandle = new CRtcpUdpHandle();
        }
        catch (...)
        {
            delete pRtpHandle;
            delete pRtcpHandle;
            return RET_ERR_SYS_NEW;
        }

        int32_t nRet = pRtpHandle->openHandle(rtpAddr);
        if (RET_OK != nRet)
        {
            delete pRtpHandle;
            delete pRtcpHandle;
            SVS_LOG((SVS_LM_WARNING,"open rtp udp handle fail, return value %d, rtp udp address[%s:%d]. "
                "continue to open next port.",
                nRet,rtpAddr.get_host_addr(),rtpAddr.get_port_number()));
            continue ;
        }
        nRet = pRtcpHandle->openHandle(rtcpAddr);
        if (RET_OK != nRet)
        {
            delete pRtpHandle;
            delete pRtcpHandle;
            SVS_LOG((SVS_LM_WARNING,"open rtcp udp handle fail, return value %d, rtcp udp address[%s:%d]. "
                "continue to open next port.",
                nRet,rtcpAddr.get_host_addr(),rtcpAddr.get_port_number()));
            continue ;
        }

        {
            ACE_Guard<ACE_RW_Thread_Mutex> locker(m_PortMutex);
            RTP_UDP_PORT_INFO udpInfo;
            udpInfo.pRtpHandle  = pRtpHandle;
            udpInfo.pRtcpHandle = pRtcpHandle;
            m_UnUsedUdpPortMap.insert(std::make_pair(usRtpPort, udpInfo));
            m_unPortNum++;
        }
    }
    return RET_OK;
}

int32_t CMduMediaPort::createEhomePort(PORT_LIST &list)
{
    for (PORT_LIST_ITER iter = list.begin();
            iter != list.end(); iter++)
    {
        uint16_t usEhomePort = *iter;

        ACE_INET_Addr ehomeAddr(usEhomePort, m_InterfaceAddr.get_ip_address());
        CEhomeHandle *pEhomeHandle  = NULL;
        try
        {
            pEhomeHandle  = new CEhomeHandle();
        }
        catch (...)
        {
            return RET_ERR_SYS_NEW;
        }

        int32_t nRet = pEhomeHandle->openHandle(ehomeAddr);
        if (RET_OK != nRet)
        {
            delete pEhomeHandle;
            SVS_LOG((SVS_LM_WARNING,"open ehome handle fail, return value %d, ehome address[%s:%d]. "
                "continue to open next port.",
                nRet,ehomeAddr.get_host_addr(),ehomeAddr.get_port_number()));
            continue ;
        }

        {
            ACE_Guard<ACE_RW_Thread_Mutex> locker(m_PortMutex);
            m_EhomePortMap.insert(std::make_pair(usEhomePort, pEhomeHandle));
            m_unEhomePortNum++;
        }
    }
    return RET_OK;
}







