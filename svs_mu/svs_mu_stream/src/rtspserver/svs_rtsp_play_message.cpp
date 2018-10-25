/*
 * RtspPlayMessage.cpp
 *
 *  Created on: 2016-5-20
 *      Author:
 */
#include <sstream>
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_common.h"
#include "svs_rtsp_defs.h"
#include "svs_rtsp_play_message.h"

CRtspPlayMessage::CRtspPlayMessage()
{
    m_unMethodType  = RTSP_METHOD_PLAY;
    m_dSpeed        = 0;
    m_dScale        = 0;

    m_bHasRange     = false;
    m_stRange.enRangeType = RANGE_TYPE_UTC;
    m_stRange.MediaBeginOffset = OFFSET_CUR;
    m_stRange.MediaEndOffset = OFFSET_END;

    m_strRtpInfo    = "";
    m_strRange      = "";
}

CRtspPlayMessage::~CRtspPlayMessage()
{
}


void CRtspPlayMessage::setSpeed(double nSpeed)
{
    m_dSpeed    = nSpeed;
    return;
}

double CRtspPlayMessage::getSpeed() const
{
    return  m_dSpeed;
}

void CRtspPlayMessage::setScale(double nScale)
{
    m_dScale = nScale;
    return;
}

double CRtspPlayMessage::getScale() const
{
    return m_dScale;
}

bool CRtspPlayMessage::hasRange() const
{
    return m_bHasRange;
}

void CRtspPlayMessage::setRange(const MEDIA_RANGE_S &stRange)
{
    m_bHasRange = true;
    m_stRange.enRangeType = stRange.enRangeType;
    m_stRange.MediaBeginOffset = stRange.MediaBeginOffset;
    m_stRange.MediaEndOffset   = stRange.MediaEndOffset;
    return;
}

void CRtspPlayMessage::getRange(MEDIA_RANGE_S &stRange) const
{
    stRange.enRangeType = m_stRange.enRangeType;
    stRange.MediaBeginOffset = m_stRange.MediaBeginOffset;
    stRange.MediaEndOffset = m_stRange.MediaEndOffset;

    return;
}

void CRtspPlayMessage::getRange(std::string &strRange) const
{
    strRange = m_strRange;
    return;
}

void CRtspPlayMessage::setRtpInfo(const string &strRtpInfo)
{
    m_strRtpInfo = strRtpInfo;
    return;
}

string CRtspPlayMessage::getRtpInfo() const
{
    return m_strRtpInfo;
}

int32_t CRtspPlayMessage::decodeMessage(CRtspPacket& objRtspPacket)
{
    int32_t nRet = CRtspMessage::decodeMessage(objRtspPacket);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_WARNING,"decode rtsp play message fail."));
        return RET_FAIL;
    }

    if (RtspResponseMethod == objRtspPacket.getMethodIndex())
    {
        string strRtpInfo;

        // 从响应消息中解析RTP-Info
        objRtspPacket.getRtpInfo(m_strRtpInfo);
        string::size_type endPos = m_strRtpInfo.find(RTSP_END_TAG);
        if (string::npos != endPos)
        {
            m_strRtpInfo = m_strRtpInfo.substr(0, endPos);
        }

        return RET_OK;
    }

    // 解析Scale

    m_dScale = objRtspPacket.getScale();

    // 解析Speed
    m_dSpeed = objRtspPacket.getSpeed();

    // 解析Range

    objRtspPacket.getRangeTime(m_stRange.enRangeType,
                                   m_stRange.MediaBeginOffset,
                                   m_stRange.MediaEndOffset);

    return RET_OK;
}

int32_t CRtspPlayMessage::encodeMessage(std::string &strMessage)
{
    if (RET_OK != CRtspMessage::encodeMessage(strMessage))
    {
        SVS_LOG((SVS_LM_WARNING,"encode rtsp play message fail."));
        return RET_FAIL;
    }

    stringstream   strValue;
    if (RTSP_MSG_REQ == getMsgType())
    {
        // 请求消息
        // Scale
        if (0 != m_dScale)
        {
            strValue<<m_dScale;
            strMessage += RTSP_TOKEN_STR_SCALE;
            strMessage += strValue.str();
            strMessage += RTSP_END_TAG;
        }

        // Speed
        if (0 != m_dSpeed)
        {
            strValue.str("");
            strValue << m_dSpeed;
            strMessage += RTSP_TOKEN_STR_SPEED;
            strMessage += strValue.str();
            strMessage += RTSP_END_TAG;
        }

        if (m_bHasRange)
        {
            (void)encodeRangeField(strMessage);
        }
    }
    else
    {
        // 响应消息，如果有RTP_INFO则需要带上
        if ("" != m_strRtpInfo)
        {
            strMessage += RTSP_TOKEN_STR_RTPINFO;
            strMessage += m_strRtpInfo;
            strMessage += RTSP_END_TAG;
        }
    }

    // End
    strMessage += RTSP_END_TAG;

    SVS_LOG((SVS_LM_DEBUG,"encode rtsp play message:\n%s", strMessage.c_str()));
    return RET_OK;
}

int32_t CRtspPlayMessage::encodeRangeField(std::string &strMessage)
{
    strMessage += RTSP_TOKEN_STR_RANGE;

    char strTime[32] = { 0 };
    if (RANGE_TYPE_UTC == m_stRange.enRangeType)
    {
        // clock时间
        time_t rangeTime = (time_t) m_stRange.MediaBeginOffset;
        struct tm tmv;

        (void) localtime_r(&rangeTime, &tmv);
        (void) snprintf(strTime, 32, "%04d%02d%02dT%02d%02d%02dZ", tmv.tm_year
                + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min,
                tmv.tm_sec);

        strMessage += RTSP_RANGE_CLOCK;
        strMessage += strTime;
        strMessage += SIGN_H_LINE;

        if (OFFSET_END != m_stRange.MediaEndOffset)
        {
            rangeTime = (time_t) m_stRange.MediaEndOffset;
            (void) localtime_r(&rangeTime, &tmv);
            (void) snprintf(strTime, 32, "%04d%02d%02dT%02d%02d%02dZ",
                    tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                    tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

            strMessage += strTime;
        }

        strMessage += RTSP_END_TAG;
        return RET_OK;
    }

    // npt时间
    strMessage += RTSP_RANGE_NPT;
    if (OFFSET_CUR == m_stRange.MediaBeginOffset)
    {
        strMessage += "0";
    }
    else if (OFFSET_BEGIN == m_stRange.MediaBeginOffset)
    {
        strMessage += "0";
    }
    else
    {
        (void) snprintf(strTime, 32, "%u", m_stRange.MediaBeginOffset);
        strMessage += strTime;
    }
    strMessage += SIGN_H_LINE;

    if (OFFSET_END != m_stRange.MediaEndOffset)
    {
        (void) snprintf(strTime, 32, "%u", m_stRange.MediaEndOffset);
        strMessage += strTime;
    }
    strMessage += RTSP_END_TAG;

    return RET_OK;
}
