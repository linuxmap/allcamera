/*
 * CStreamSvsMessage.h
 *
 *  Created on: 2010-12-30
 *      Author:
 */

#ifndef CSTREAMSVSMESSAGE_H_
#define CSTREAMSVSMESSAGE_H_
#include "svs_ace_header.h"
#include <vms/vms.h>
#include "svs_vms_system_api.h"

#define STREAM_SVS_MSAG_HEADER_SIZE  sizeof(SVS_MSG_HEADER)

#define TLV_MIN_LENGTH            (sizeof(uint32_t) + sizeof(uint32_t))

class CStreamSvsMessage
{
public:
    CStreamSvsMessage();
    virtual ~CStreamSvsMessage();

    virtual int32_t create(char* pMsgData, uint32_t unLength);

    virtual int32_t create(uint32_t unLength,
               uint32_t unTransNo);

    virtual uint32_t getMsgType()  = 0;

    virtual int32_t handleMessage() = 0;

    virtual int32_t checkMessage();


    char* getBinaryData() const;

    uint32_t getLength()const;

    int32_t checkValueRange
    (
        uint32_t ulValue,
        uint32_t ulMinValue,
        uint32_t ulMaxValue
    )const;

    virtual void dump() const;
protected:
    int32_t checkTransDirection(uint32_t unPeerType, uint32_t unTransDirection) const;

private:
    int32_t initHeader(uint32_t unTransNo);
private:
    SVS_MSG_HEADER*    m_pMsgHeader;
    char*              m_pBinaryData;
    uint32_t           m_unMsgLength;
    bool               m_bDeleteBuffer;
};

#endif /* CSTREAMSVSMESSAGE_H_ */
