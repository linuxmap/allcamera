/*
 * CRtpTcpHandle.h
 *
 *  Created on: 2016-1-6
 *      Author:
 */

#ifndef CRTPTCPHANDLE_H_
#define CRTPTCPHANDLE_H_

#include "svs_adapter_tcp_handle.h"

class CRtpTcpHandle : public CTcpHandle
{
public:
    CRtpTcpHandle();
    virtual ~CRtpTcpHandle();

    CTcpConnect* createTcpConnect();

    void destroyTcpConnect(CTcpConnect *&pConnect);
};

#endif /* CRTPTCPHANDLE_H_ */
