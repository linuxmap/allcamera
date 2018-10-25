/*
 * RtspTeardownMessage.cpp
 *
 *  Created on: 2016-5-20
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_rtsp_teardown_message.h"

CRtspTeardownMessage::CRtspTeardownMessage()
{
    m_unMethodType = RTSP_METHOD_TEARDOWN;
}

CRtspTeardownMessage::~CRtspTeardownMessage()
{
}

int32_t CRtspTeardownMessage::encodeMessage(std::string &strMessage)
{
    strMessage.clear();

    // 直接调用父类编码CSeq和User-Agent
    if (RET_OK != CRtspMessage::encodeMessage(strMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"encode rtsp teardown message fail."));
        return RET_FAIL;
    }

    // End
    strMessage += RTSP_END_TAG;

    SVS_LOG((SVS_LM_DEBUG,"encode rtsp teardown message:\n%s", strMessage.c_str()));
    return RET_OK;
}
