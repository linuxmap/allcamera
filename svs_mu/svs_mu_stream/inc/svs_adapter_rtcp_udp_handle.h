/*
 * RtcpUdpHandle.h
 *
 *  Created on: 2016-1-4
 *      Author:
 */

#ifndef RTCPUDPHANDLE_H_
#define RTCPUDPHANDLE_H_

#include "svs_adapter_udp_handle.h"

class CRtcpUdpHandle : public CUdpHandle
{
public:
    CRtcpUdpHandle();
    virtual ~CRtcpUdpHandle();

    int32_t handleRecvedData(ACE_Message_Block *pMsg, ACE_INET_Addr &remoteAddr);
    int32_t handleDummyMessage(ACE_Message_Block *pMsg, const ACE_INET_Addr &remoteAddr);
};

#endif /* RTCPUDPHANDLE_H_ */
