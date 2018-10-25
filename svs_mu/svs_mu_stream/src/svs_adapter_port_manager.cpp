#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_port_manager.h"
#include "svs_adapter_config.h"
CStreamPortManager::CStreamPortManager()
{
    m_pSvsFrameHandle = NULL;
}

CStreamPortManager::~CStreamPortManager()
{
    try
    {
        if (NULL != m_pSvsFrameHandle)
        {
            delete m_pSvsFrameHandle;
        }

        while (!m_MediaInterfaceMap.empty())
        {
            CStreamMediaPort *pMediaPort = m_MediaInterfaceMap.begin()->second;
            if (pMediaPort)
            {
                delete pMediaPort;
            }
            m_MediaInterfaceMap.erase(m_MediaInterfaceMap.begin());
        }
    }
    catch(...)
    {}

    m_pSvsFrameHandle           = NULL;
}

int32_t CStreamPortManager::initManager()
{
    uint32_t unInternalIp = CStreamConfig::instance()->getInternalMediaIp();

    STREAM_IP_LIST ipList;
    CStreamConfig::instance()->getExternalMediaIpList(ipList);

    ipList.push_front(unInternalIp);

    MediaPortConfig *pUdpConfig = NULL;
    uint16_t usEnableEhome = 0;
    MediaPortConfig *pEhomeConfig = NULL;
    CStreamConfig::instance()->getUdpMediaPortConfig(pUdpConfig);
    usEnableEhome = CStreamConfig::instance()->getEnableEhome();
    CStreamConfig::instance()->getEhomeMediaPortConfig(pEhomeConfig);

    for (STREAM_IP_LIST::iterator iter = ipList.begin(); iter != ipList.end(); iter++)
    {
        SVS_LOG((SVS_LM_INFO,"start create media port, ip=[0x%x].", *iter));
        CStreamMediaPort *pMediaPort = NULL;
        try
        {
            pMediaPort = new CStreamMediaPort;
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_CRITICAL,"create media port fail, ip=[%x].", *iter));
            return RET_FAIL;
        }

        int32_t nRet = pMediaPort->init(*iter, CStreamConfig::instance()->getTcpMediaPort(),
                                        pUdpConfig,usEnableEhome,pEhomeConfig);
        if (RET_OK != nRet)
        {
            delete pMediaPort;
            SVS_LOG((SVS_LM_CRITICAL,"init media port config fail, ip=[%x].", *iter));
            return RET_FAIL;
        }
        m_MediaInterfaceMap[*iter] = pMediaPort;
    }


    SVS_LOG((SVS_LM_INFO,"CStreamPortManager::initManager success."));
    return RET_OK;
}

void CStreamPortManager::closeManager()
{
    SVS_LOG((SVS_LM_CRITICAL,"CStreamPortManager::closeManager begin."));

    if (NULL != m_pSvsFrameHandle)
    {
        delete m_pSvsFrameHandle;
        m_pSvsFrameHandle = NULL;
    }

    while (!m_MediaInterfaceMap.empty())
    {
        CStreamMediaPort *pMediaPort = m_MediaInterfaceMap.begin()->second;
        if (pMediaPort)
        {
            delete pMediaPort;
        }
        m_MediaInterfaceMap.erase(m_MediaInterfaceMap.begin());
    }
    SVS_LOG((SVS_LM_CRITICAL,"CStreamPortManager::closeManager end."));
    return;
}

int32_t CStreamPortManager::allocRtpTcpPort(uint32_t unSpecifyIp,
                                  CNetworkHandle *&pHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager alloc tcp port at [%s].", addr.get_host_addr()));
    CStreamMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        return RET_FAIL;
    }

    pHandle = pMediaPort->getTcpMediaHandle();

    return RET_OK;
}

int32_t CStreamPortManager::allocRtpUdpPort(uint32_t unSpecifyIp,
                                      CNetworkHandle *&pRtpHandle,
                                      CNetworkHandle *&pRtcpHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager alloc udp port at [%s].", addr.get_host_addr()));

    CStreamMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        return RET_FAIL;
    }

    return pMediaPort->allocRtpUdpPort(pRtpHandle, pRtcpHandle);
}

int32_t CStreamPortManager::allocEhomePort(uint32_t unSpecifyIp, CHandle *&pHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager alloc ehome port at [%s].", addr.get_host_addr()));

    CStreamMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        return RET_FAIL;
    }

    return pMediaPort->allocEhomeMediaPort(pHandle);
}


int32_t CStreamPortManager::releaseRtpTcpPort(uint32_t unSpecifyIp,
                                    CNetworkHandle *&pHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager release tcp port at [%s].", addr.get_host_addr()));

    CStreamMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Release rtp tcp port fail, get media port fail. specify ip[%x].",
            unSpecifyIp));
        return RET_FAIL;
    }

    return pMediaPort->releaseRtpTcpPort(pHandle);
}

int32_t CStreamPortManager::releaseRtpUdpPort(uint32_t unSpecifyIp,
                                        CNetworkHandle *&pRtpHandle,
                                        CNetworkHandle *&pRtcpHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager release udp port at [%s].", addr.get_host_addr()));

    CStreamMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        return RET_FAIL;
    }

    return pMediaPort->releaseRtpUdpPort(pRtpHandle, pRtcpHandle);
}

int32_t CStreamPortManager::allocMruSvsPort(uint32_t unSpecifyIp, CNetworkHandle *&pHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager alloc svs port at [%s].", addr.get_host_addr()));

    pHandle = m_pSvsFrameHandle;
    return RET_OK;
}

int32_t CStreamPortManager::releaseMruSvsPort(uint32_t unSpecifyIp,
                                       CNetworkHandle *&pHandle) const
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager release svs port at [%s].", addr.get_host_addr()));

    pHandle = NULL;

    return RET_OK;
}



int32_t CStreamPortManager::releaseEhomePort(uint32_t unSpecifyIp, CHandle *&pHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager release ehome port at [%s].", addr.get_host_addr()));

    CStreamMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        return RET_FAIL;
    }

    return pMediaPort->releaseEhomeMediaPort(pHandle);
}


CStreamMediaPort* CStreamPortManager::getMediaPort(uint32_t unSpecifyIp)
{
    MEDIA_INTERFACE_MAP_ITER iter = m_MediaInterfaceMap.find(unSpecifyIp);
    if (m_MediaInterfaceMap.end() == iter)
    {
        return NULL;
    }

    return (CStreamMediaPort*)iter->second;
}

