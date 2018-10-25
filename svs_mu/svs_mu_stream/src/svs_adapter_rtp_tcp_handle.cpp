/*
 * CRtpTcpHandle.cpp
 *
 *  Created on: 2016-1-6
 *      Author:
 */

#include "svs_adapter_rtp_tcp_handle.h"
#include "svs_adapter_rtp_tcp_connect.h"

CRtpTcpHandle::CRtpTcpHandle()
{
}

CRtpTcpHandle::~CRtpTcpHandle()
{
}

CTcpConnect* CRtpTcpHandle::createTcpConnect()
{
    CTcpConnect *pConnect = NULL;
    try
    {
        pConnect = new CRtpTcpConnect();
    }
    catch(...)
    {
        delete pConnect;
        return NULL;
    }

    return pConnect;
}

void CRtpTcpHandle::destroyTcpConnect(CTcpConnect *&pConnect)
{
    try
    {
        delete pConnect;
        pConnect = NULL;
    }
    catch(...)
    {

    }
    return;
}
