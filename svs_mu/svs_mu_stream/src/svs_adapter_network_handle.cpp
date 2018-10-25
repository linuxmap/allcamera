#include "svs_log_msg.h"

#include "svs_adapter_network_handle.h"

CHandle::CHandle()
{
    setStatus(NETWORK_HANDLE_STATUS_INIT);
}

CHandle::~CHandle()
{
}

void CHandle::setLocalAddr(const ACE_INET_Addr &localAddr)
{
    m_LocalAddr = localAddr;
}

ACE_INET_Addr& CHandle::getLocalAddr()
{
    return m_LocalAddr;       //lint !e1536
}

void CHandle::setStatus(NETWORK_HANDLE_STATUS status)
{
    m_HandleStatus    = status;
    m_StatusTime      = ACE_OS::gettimeofday();
}

NETWORK_HANDLE_STATUS CHandle::getStatus() const
{
    return m_HandleStatus;
}
