/*
 * RtspDescribeReq.cpp
 *
 *  Created on: 2016-5-19
 *      Author:
 */
#include <sstream>
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_rtsp_describe_message.h"
#include "svs_rtsp_protocol.h"

CRtspDescribeMessage::CRtspDescribeMessage()
{
    m_unMethodType = RTSP_METHOD_DESCRIBE;
    m_strSDP = "";
}

CRtspDescribeMessage::~CRtspDescribeMessage()
{
}

int32_t CRtspDescribeMessage::decodeMessage(CRtspPacket& objRtspPacket)
{
    int32_t nRet = CRtspMessage::decodeMessage(objRtspPacket);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"decode rtsp play message fail."));
        return RET_FAIL;
    }

    objRtspPacket.getContent(m_strSDP);

    return RET_OK;
}
int32_t CRtspDescribeMessage::encodeMessage(std::string &strMessage)
{
    strMessage.clear();

    // ֱ�ӵ��ø������CSeq��User-Agent
    if (RET_OK != CRtspMessage::encodeMessage(strMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"encode rtsp describe request message fail."));
        return RET_FAIL;
    }

    if (RTSP_MSG_RSP == getMsgType())
    {
        // ��Ӧ��Ϣ
        if (RTSP_SUCCESS_OK == m_unStatusCode)
        {
            // 200 OK��Ӧ����ҪЯ��SDP��Ϣ.
            // Content-Length
            if (0 == m_strSDP.length())
            {
                SVS_LOG((SVS_LM_WARNING,"encode rtsp describe message fail, no sdp info."));

                return RET_FAIL;
            }

            strMessage += RTSP_TOKEN_STR_CONTENT_LENGTH;
            std::stringstream strContentLength;
            strContentLength << m_strSDP.length();    // ��������������\r\n
            strMessage += strContentLength.str();
            strMessage += RTSP_END_TAG;

            // Content-Type
            strMessage += RTSP_TOKEN_STR_CONTENT_TYPE;
            strMessage += RTSP_CONTENT_SDP;
            strMessage += RTSP_END_TAG;

            // End of Rtsp
            strMessage += RTSP_END_TAG;

            // ���׷��SDP��Ϣ
            strMessage += m_strSDP;
        }
        else
        {
            // ����������û��SDP
            // End of Rtsp
            strMessage += RTSP_END_TAG;
        }
    }
    else
    {
        // ������Ϣ
        // Accept
        strMessage += RTSP_TOKEN_STR_ACCEPT;
        strMessage += RTSP_CONTENT_SDP;
        strMessage += RTSP_END_TAG;

        //end
        strMessage += RTSP_END_TAG;
    }
    SVS_LOG((SVS_LM_DEBUG,"encode rtsp describe message:\n%s",
                     strMessage.c_str()));
    return RET_OK;
}

void CRtspDescribeMessage::setSdp(const std::string &strSdp)
{
    m_strSDP    = strSdp;
    return;
}

std::string CRtspDescribeMessage::getSdp() const
{
    return m_strSDP;
}
