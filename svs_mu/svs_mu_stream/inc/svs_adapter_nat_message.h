/*
 * MduNetMessage.h
 *
 *  Created on: 2016-1-16
 *      Author:
 */

#ifndef MDUNETMESSAGE_H_
#define MDUNETMESSAGE_H_
#include <vms/vms.h>
class CMduNatMessage
{
public:
    CMduNatMessage();
    CMduNatMessage(const CMduNatMessage &natMsg);
    virtual ~CMduNatMessage();

    void init(void* pRecvHandle,
            uint32_t  unMsgSeq,
            const char*   pSaftyInfo,
            uint32_t  unLocalIp,
            uint16_t usLocalPort,
            uint32_t  unNatIp,
            uint16_t usNatPort,
            void *pPeerSession);
public:
    void*           m_pPeerSession;
    void*           m_pRecvHandle;
    char            m_szSaftyInfo[SAFETY_INFO_LEN];
    uint32_t    m_unMsgSeq;
    uint32_t    m_unLocalIp;
    uint16_t  m_usLocalPort;

    uint32_t    m_unNatIp;
    uint16_t  m_usNatPort;
};

#endif /* MDUNETMESSAGE_H_ */
