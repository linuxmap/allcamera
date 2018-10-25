/*
 * MduRtspPushSession.h
 *
 *  Created on: 2016-5-16
 *      Author:
 */

#ifndef MDURTMPHANDLE_H_
#define MDURTMPHANDLE_H_

#include "svs_ace_header.h"


class CMduRtmpSendHandle
{
public:
    CMduRtmpSendHandle(){};
    virtual ~CMduRtmpSendHandle(){};

public:
    virtual ACE_INET_Addr getPeerAddr()const = 0;

    virtual ACE_INET_Addr getLocalAddr()const = 0;

    virtual int32_t sendMediaData(ACE_Message_Block **pMbArray, uint32_t MsgCount) = 0;


};

#endif /* MDURTMPHANDLE_H_ */
