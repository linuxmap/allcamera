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

    // 直接调用父类编码CSeq和User-Agent
    if (RET_OK != CRtspMessage::encodeMessage(strMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"encode rtsp describe request message fail."));
        return RET_FAIL;
    }

    if (RTSP_MSG_RSP == getMsgType())
    {
        // 响应消息
        if (RTSP_SUCCESS_OK == m_unStatusCode)
        {
            // 200 OK响应中需要携带SDP信息.
            // Content-Length
            if (0 == m_strSDP.length())
            {
                SVS_LOG((SVS_LM_WARNING,"encode rtsp describe message fail, no sdp info."));

                return RET_FAIL;
            }

            strMessage += RTSP_TOKEN_STR_CONTENT_LENGTH;
            std::stringstream strContentLength;
            strContentLength << m_strSDP.length();    // 结束符后增加了\r\n
            strMessage += strContentLength.str();
            strMessage += RTSP_END_TAG;

            // Content-Type
            strMessage += RTSP_TOKEN_STR_CONTENT_TYPE;
            strMessage += RTSP_CONTENT_SDP;
            strMessage += RTSP_END_TAG;

            // End of Rtsp
            strMessage += RTSP_END_TAG;

            // 最后追加SDP信息
            strMessage += m_strSDP;
        }
        else
        {
            // 其它返回码没有SDP
            // End of Rtsp
            strMessage += RTSP_END_TAG;
        }
    }
    else
    {
        // 请求消息
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
