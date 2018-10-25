/*
 * RtspSdp.h
 *
 *  Created on: 2016-5-20
 *      Author:
 */

#ifndef RTSPSDP_H_
#define RTSPSDP_H_

#include <string>
#include <list>

#define STR_SDP_BUF_LEN        512

#define INVALID_PAYLOAD_TYPE   ((uint8_t)-1)

#define STR_SDP_RTPMAP          std::string("rtpmap:")
#define STR_SDP_FMTP            std::string("fmtp:")
#define STR_SDP_CONTROL_ATTR    std::string("control:")

#define STR_SDP_RECVONLY_ATTR   std::string("recvonly")
#define STR_SDP_SENDONLY_ATTR   std::string("sendonly")
#define STR_SDP_SENDRECV_ATTR   std::string("sendrecv")




enum PT_TYPE_VALUE
{
    PT_TYPE_PCMU  = 0,
    PT_TYPE_PCMA  = 8,
    PT_TYPE_MJPEG = 26,
    PT_TYPE_PS    = 96,
    PT_TYPE_MPEG4 = 97,
    PT_TYPE_H264  = 98,
    PT_TYPE_H265  = 99,
    PT_TYPE_AAC   = 100,
    PT_TYPE_MAX   = 0xFF,
};

#define PCMU_AUDIO             std::string("PCMU")
#define PCMA_AUDIO             std::string("PCMA")
#define AAC_AUDIO              std::string("MPEG4-GENERIC")
#define H264_VIDEO             std::string("H264")
#define H265_VIDEO             std::string("H265")
#define MJPEG_VIDEO            std::string("JPEG")
#define MPEG4_VIDEO            std::string("MP4V-ES")


#define PS_VIDEO_RTPMAP         std::string("PS/90000")
#define H264_VIDEO_RTPMAP       std::string("H264/90000")
#define H265_VIDEO_RTPMAP       std::string("H265/90000")
#define MJPEG_VIDEO_RTPMAP      std::string("JPEG/90000")
#define MPEG4_VIDEO_RTPMAP      std::string("MP4V-ES/90000")
#define DEFAULT_AUDIO_PCMU      std::string("PCMU/8000")
#define DEFAULT_AUDIO_PCMA      std::string("PCMA/8000")

#define JUMP_SPACE(psz) {while (isspace(*(psz)) && (  '\0' != *(psz)))(psz)++;}

#define GB28181_SSRC_LEN 10


typedef struct _stSDP_MEDIA_INFO
{
    uint8_t         ucPayloadType;
    uint16_t        usPort;
    std::string     strRtpmap;
    std::string     strFmtp;
    std::string     strControl;
}SDP_MEDIA_INFO;

typedef std::list<SDP_MEDIA_INFO>     MEDIA_INFO_LIST;
class CMediaSdp
{
public:
    CMediaSdp();

    virtual ~CMediaSdp();

    void setUrl(const std::string &strUrl);

    std::string getUrl() const;

    void setRange(const std::string &strRange);

    std::string getRange() const;

    void setSessionName(const std::string &strSessionName);

    std::string getSessionName() const;

    void setConnAddr(const std::string &strConnAddr);

    std::string getConnAddr() const;

    void getVideoInfo(MEDIA_INFO_LIST &infoList);

    void addVideoInfo(const SDP_MEDIA_INFO &info);

    void getAudioInfo(MEDIA_INFO_LIST &infoList);

    void addAudioInfo(const SDP_MEDIA_INFO &info);

    void setTransDirect(TRANS_DIRECTION enTransDirect);

    TRANS_DIRECTION getTransDirect();

    void setSsrc(std::string& strSsrc);

    void setSsrcByGB28181Dev(PLAY_TYPE enPlayType,uint32_t ulIndex);

    void setGB28181Format(std::string& strFormat);

    void setOwner(std::string& strOwner);

    int32_t decodeSdp(const std::string   &strSdp);

    int32_t encodeSdp(std::string   &strSdp ,int32_t isplayback,std::string timeRange);

    void copy(CMediaSdp& rtspSdp);

    void makeRtpmap(std::string& strRtpmap,uint8_t ucPT,uint32_t ulClockFre);

protected:
    enum _enSDP_STATUS
    {
        SDP_GLOBAL_INFO,
        SDP_VIDEO_INFO,
        SDP_AUDIO_INFO
    };
    bool getNextLine(const std::string &strSdpMsg,
                     uint32_t &nNextStart,
                     std::string  &strBuff) const;



    int32_t parseConnDesc(char *pszBuff);

    int32_t parseTimeDesc(char *pszBuff);

    int32_t parseMediaDesc(char *pszBuff,SDP_MEDIA_INFO*& pMediaInfo);

    int32_t parseAttributes(char *pszBuff,SDP_MEDIA_INFO* pMediaInfo);

    void encodeMediaDesc(std::string &strSdp, uint32_t unStatus);
protected:
    uint32_t        m_unSession;
    std::string     m_strOwner;
    std::string     m_strSessionName;
    std::string     m_strUrl;
    std::string     m_strConnAddr;
    std::string     m_range;
    std::string     m_strSsrc;
    bool            m_bSetFormat;
    std::string     m_str28181Format;
    TRANS_DIRECTION m_enTransDirect;
    MEDIA_INFO_LIST m_VideoInfoList;
    MEDIA_INFO_LIST m_AudioInfoList;

    uint32_t        m_enParseStatus;
};


#endif /* RTSPSDP_H_ */
