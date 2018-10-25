#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_port_manager.h"
#include "svs_adapter_config.h"
CMduPortManager::CMduPortManager()
{
    m_pSvsFrameHandle = NULL;
}

CMduPortManager::~CMduPortManager()
{
    try
    {
        if (NULL != m_pSvsFrameHandle)
        {
            delete m_pSvsFrameHandle;
        }

        while (!m_MediaInterfaceMap.empty())
        {
            CMduMediaPort *pMediaPort = m_MediaInterfaceMap.begin()->second;
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

int32_t CMduPortManager::initManager()
{
    uint32_t unInternalIp = CMduConfig::instance()->getInternalMediaIp();

    MDU_IP_LIST ipList;
    CMduConfig::instance()->getExternalMediaIpList(ipList);

    ipList.push_front(unInternalIp);

    MediaPortConfig *pUdpConfig = NULL;
    uint16_t usEnableEhome = 0;
    MediaPortConfig *pEhomeConfig = NULL;
    CMduConfig::instance()->getUdpMediaPortConfig(pUdpConfig);
    usEnableEhome = CMduConfig::instance()->getEnableEhome();
    CMduConfig::instance()->getEhomeMediaPortConfig(pEhomeConfig);

    for (MDU_IP_LIST::iterator iter = ipList.begin(); iter != ipList.end(); iter++)
    {
        SVS_LOG((SVS_LM_INFO,"start create media port, ip=[0x%x].", *iter));
        CMduMediaPort *pMediaPort = NULL;
        try
        {
            pMediaPort = new CMduMediaPort;
        }
        catch(...)
        {
            SVS_LOG((SVS_LM_CRITICAL,"create media port fail, ip=[%x].", *iter));
            return RET_FAIL;
        }

        int32_t nRet = pMediaPort->init(*iter, CMduConfig::instance()->getTcpMediaPort(),
                                        pUdpConfig,usEnableEhome,pEhomeConfig);
        if (RET_OK != nRet)
        {
            delete pMediaPort;
            SVS_LOG((SVS_LM_CRITICAL,"init media port config fail, ip=[%x].", *iter));
            return RET_FAIL;
        }
        m_MediaInterfaceMap[*iter] = pMediaPort;
    }


    SVS_LOG((SVS_LM_INFO,"CMduPortManager::initManager success."));
    return RET_OK;
}

void CMduPortManager::closeManager()
{
    SVS_LOG((SVS_LM_CRITICAL,"CMduPortManager::closeManager begin."));

    if (NULL != m_pSvsFrameHandle)
    {
        delete m_pSvsFrameHandle;
        m_pSvsFrameHandle = NULL;
    }

    while (!m_MediaInterfaceMap.empty())
    {
        CMduMediaPort *pMediaPort = m_MediaInterfaceMap.begin()->second;
        if (pMediaPort)
        {
            delete pMediaPort;
        }
        m_MediaInterfaceMap.erase(m_MediaInterfaceMap.begin());
    }
    SVS_LOG((SVS_LM_CRITICAL,"CMduPortManager::closeManager end."));
    return;
}

int32_t CMduPortManager::allocRtpTcpPort(uint32_t unSpecifyIp,
                                  CNetworkHandle *&pHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager alloc tcp port at [%s].", addr.get_host_addr()));
    CMduMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        return RET_FAIL;
    }

    pHandle = pMediaPort->getTcpMediaHandle();

    return RET_OK;
}

int32_t CMduPortManager::allocRtpUdpPort(uint32_t unSpecifyIp,
                                      CNetworkHandle *&pRtpHandle,
                                      CNetworkHandle *&pRtcpHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager alloc udp port at [%s].", addr.get_host_addr()));

    CMduMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        return RET_FAIL;
    }

    return pMediaPort->allocRtpUdpPort(pRtpHandle, pRtcpHandle);
}

int32_t CMduPortManager::allocEhomePort(uint32_t unSpecifyIp, CHandle *&pHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager alloc ehome port at [%s].", addr.get_host_addr()));

    CMduMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        return RET_FAIL;
    }

    return pMediaPort->allocEhomeMediaPort(pHandle);
}


int32_t CMduPortManager::releaseRtpTcpPort(uint32_t unSpecifyIp,
                                    CNetworkHandle *&pHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager release tcp port at [%s].", addr.get_host_addr()));

    CMduMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Release rtp tcp port fail, get media port fail. specify ip[%x].",
            unSpecifyIp));
        return RET_FAIL;
    }

    return pMediaPort->releaseRtpTcpPort(pHandle);
}

int32_t CMduPortManager::releaseRtpUdpPort(uint32_t unSpecifyIp,
                                        CNetworkHandle *&pRtpHandle,
                                        CNetworkHandle *&pRtcpHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager release udp port at [%s].", addr.get_host_addr()));

    CMduMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        return RET_FAIL;
    }

    return pMediaPort->releaseRtpUdpPort(pRtpHandle, pRtcpHandle);
}

int32_t CMduPortManager::allocMruSvsPort(uint32_t unSpecifyIp, CNetworkHandle *&pHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager alloc svs port at [%s].", addr.get_host_addr()));

    pHandle = m_pSvsFrameHandle;
    return RET_OK;
}

int32_t CMduPortManager::releaseMruSvsPort(uint32_t unSpecifyIp,
                                       CNetworkHandle *&pHandle) const
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager release svs port at [%s].", addr.get_host_addr()));

    pHandle = NULL;

    return RET_OK;
}



int32_t CMduPortManager::releaseEhomePort(uint32_t unSpecifyIp, CHandle *&pHandle)
{
    ACE_INET_Addr addr((uint16_t)0, unSpecifyIp);
    SVS_LOG((SVS_LM_INFO,"port manager release ehome port at [%s].", addr.get_host_addr()));

    CMduMediaPort *pMediaPort = getMediaPort(unSpecifyIp);
    if (NULL == pMediaPort)
    {
        return RET_FAIL;
    }

    return pMediaPort->releaseEhomeMediaPort(pHandle);
}


CMduMediaPort* CMduPortManager::getMediaPort(uint32_t unSpecifyIp)
{
    MEDIA_INTERFACE_MAP_ITER iter = m_MediaInterfaceMap.find(unSpecifyIp);
    if (m_MediaInterfaceMap.end() == iter)
    {
        return NULL;
    }

    return (CMduMediaPort*)iter->second;
}

