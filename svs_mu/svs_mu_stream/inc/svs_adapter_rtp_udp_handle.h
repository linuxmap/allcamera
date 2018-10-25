/*
 * RtpUdpHandle.h
 *
 *  Created on: 2016-1-4
 *      Author:
 */

#ifndef RTPUDPHANDLE_H_
#define RTPUDPHANDLE_H_

#include "svs_adapter_udp_handle.h"

class CRtpUdpHandle : public CUdpHandle
{
public:
    CRtpUdpHandle();
    virtual ~CRtpUdpHandle();

    int32_t handleRecvedData(ACE_Message_Block *pMsg, ACE_INET_Addr &remoteAddr);

    int32_t handleRtspMessage(ACE_Message_Block *pMsg, const ACE_INET_Addr &remoteAddr);

    int32_t handleDummyMessage(ACE_Message_Block *pMsg, const ACE_INET_Addr &remoteAddr);
};

#endif /* RTPUDPHANDLE_H_ */
