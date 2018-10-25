/*
 * MduNetMessage.cpp
 *
 *  Created on: 2016-1-16
 *      Author:
 */
#include "string.h"

#include "svs_adapter_nat_message.h"
#include <sys/types.h>


CMduNatMessage::CMduNatMessage()
{
    m_pPeerSession  = 0;
    m_pRecvHandle   = 0;
    memset(m_szSaftyInfo, 0x0, SAFETY_INFO_LEN);
    m_unMsgSeq      = 0;
    m_unLocalIp     = 0;
    m_usLocalPort   = 0;
    m_unNatIp       = 0;
    m_usNatPort     = 0;
}

CMduNatMessage::CMduNatMessage(const CMduNatMessage &natMsg)
{
    m_pPeerSession  = natMsg.m_pPeerSession;        //lint !e1554
    m_pRecvHandle   = natMsg.m_pRecvHandle;         //lint !e1554
    m_unMsgSeq      = natMsg.m_unMsgSeq;
    m_unLocalIp     = natMsg.m_unLocalIp;
    m_usLocalPort   = natMsg.m_usLocalPort;
    m_unNatIp       = natMsg.m_unNatIp;
    m_usNatPort     = natMsg.m_usNatPort;

    memcpy(m_szSaftyInfo, natMsg.m_szSaftyInfo, SAFETY_INFO_LEN);
}

CMduNatMessage::~CMduNatMessage()
{
    m_pPeerSession  = 0;
    m_pRecvHandle   = 0;
    m_unMsgSeq      = 0;
    m_unLocalIp     = 0;
    m_usLocalPort   = 0;
    m_unNatIp       = 0;
    m_usNatPort     = 0;

    memset(m_szSaftyInfo, 0x0, SAFETY_INFO_LEN);
}

void CMduNatMessage::init(void* pRecvHandle,
                        uint32_t  unMsgSeq,
                        const char*   pSaftyInfo,
                        uint32_t  unLocalIp,
                        uint16_t usLocalPort,
                        uint32_t  unNatIp,
                        uint16_t usNatPort,
                        void *pPeerSession)
{
    m_pPeerSession  = pPeerSession;
    m_pRecvHandle   = pRecvHandle;
    m_unMsgSeq      = unMsgSeq;
    m_unLocalIp     = unLocalIp;
    m_usLocalPort   = usLocalPort;
    m_unNatIp       = unNatIp;
    m_usNatPort     = usNatPort;

    if (NULL != pSaftyInfo)
    {
        memcpy(m_szSaftyInfo, pSaftyInfo, SAFETY_INFO_LEN);
    }
    return;
}
