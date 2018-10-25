/*
 * RtpTcpConnect.h
 *
 *  Created on: 2016-1-6
 *      Author:
 */

#ifndef RTPTCPCONNECT_H_
#define RTPTCPCONNECT_H_

#include "svs_adapter_tcp_connect.h"

// ����RTP��֯��־
#define RTP_INTERLEAVE_FLAG    '$'

// ���彻֯��Ϣ�ĳ���
#define RTP_INTERLEAVE_LENGTH   4

enum _enPARSE_DATA_INFO_RET
{
    PARSE_RET_OK,
    PARSE_RET_FAIL,
    PARSE_RET_DATA_LACK
};

class CRtpTcpConnect : public CTcpConnect
{
public:
    CRtpTcpConnect();
    virtual ~CRtpTcpConnect();

    int32_t handleRecvedData(const char* pData, uint32_t unDataSize);

private:
    int32_t parseDataInfo(const char* pData, uint32_t unDataSize);

    int32_t parseRtspPacket(const char* pData, uint32_t unDataSize);

    int32_t parseRtpRtcpPacket(const char* pData, uint32_t unDataSize);

    int32_t handleMediaData(const char* pData, uint32_t unDataSize);

    int32_t handleControlMessage(const char* pData, uint32_t unDataSize);

    int32_t getPlayType();

private:
    uint32_t            m_unSize;
    uint32_t            m_unDataOffset;
    uint16_t            m_usInnerMsgType;
    bool                m_bServiceType;
};

#endif /* RTPTCPCONNECT_H_ */
