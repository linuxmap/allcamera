#ifndef __CSTREAMPORTMANAGER_H__
#define __CSTREAMPORTMANAGER_H__
#include <map>
#include "svs_adapter_config.h"
#include "svs_adapter_media_port.h"

class CStreamPortManager
{
public:
    virtual ~CStreamPortManager();

    static CStreamPortManager *instance()
    {
        static CStreamPortManager streamPortManager;
        return &streamPortManager;
    }

    int32_t initManager();

    void closeManager();

    int32_t allocRtpTcpPort(uint32_t unSpecifyIp, CNetworkHandle *&pHandle);

    int32_t allocRtpUdpPort(uint32_t unSpecifyIp,
                        CNetworkHandle *&pRtpHandle,
                        CNetworkHandle *&pRtcpHandle);

    int32_t releaseRtpTcpPort(uint32_t unSpecifyIp, CNetworkHandle *&pHandle);

    int32_t releaseRtpUdpPort(uint32_t unSpecifyIp,
                          CNetworkHandle *&pRtpHandle,
                          CNetworkHandle *&pRtcpHandle);

    int32_t allocMruSvsPort(uint32_t unSpecifyIp, CNetworkHandle *&pHandle);

    int32_t releaseMruSvsPort(uint32_t unSpecifyIp, CNetworkHandle *&pHandle) const;

    int32_t allocEhomePort(uint32_t unSpecifyIp, CHandle *&pHandle);

    int32_t releaseEhomePort(uint32_t unSpecifyIp, CHandle *&pHandle);
private:
    CStreamPortManager();

    CStreamMediaPort* getMediaPort(uint32_t unSpecifyIp);
private:
    typedef std::map<uint32_t, CStreamMediaPort*>  MEDIA_INTERFACE_MAP;
    typedef MEDIA_INTERFACE_MAP::iterator       MEDIA_INTERFACE_MAP_ITER;
private:
    MEDIA_INTERFACE_MAP   m_MediaInterfaceMap;

    CNetworkHandle*       m_pSvsFrameHandle;
};

#endif // __CSTREAMPORTMANAGER_H__
