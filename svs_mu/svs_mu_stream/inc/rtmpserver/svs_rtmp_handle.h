/*
 * StreamRtspPushSession.h
 *
 *  Created on: 2016-5-16
 *      Author:
 */

#ifndef STREAMRTMPHANDLE_H_
#define STREAMRTMPHANDLE_H_

#include "svs_ace_header.h"


class CStreamRtmpSendHandle
{
public:
    CStreamRtmpSendHandle(){};
    virtual ~CStreamRtmpSendHandle(){};

public:
    virtual ACE_INET_Addr getPeerAddr()const = 0;

    virtual ACE_INET_Addr getLocalAddr()const = 0;

    virtual int32_t sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount) = 0;


};

#endif /* STREAMRTMPHANDLE_H_ */
