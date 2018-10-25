#include "svs_adapter_rtp_packer.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "svs_log_msg.h"

//#define __PRINT_MEDIA__


CPackager::CPackager()
{
    m_pCallBackHandle = NULL;
    m_VideoRTPSeq = 0;
    m_AudioRTPSeq = 0;
    m_ulVideoTimeStamp =   0;
    m_ulAudioTimeStamp = 0;
    memset(&m_pSendVideoBuf ,0,RTP_PACK_BUF_SIZE);
    memset(&m_pSendAudioBuf ,0,RTP_PACK_BUF_SIZE);
    m_ulVideoPt   = PT_H264;
    m_ulAudioPt   = PCMU;
    m_ulVideoClockFreq = H264_HZ;
    m_ulAudioClockFreq = PCM_HZ;
}

CPackager::~CPackager()
{
}

int32_t CPackager::Init (CRtpHandle* pCallBackHandle,uint32_t ulVideoPt,uint32_t ulAudioPt,uint32_t ulVideoClockFreq,uint32_t ulAudioClockFreq)
{
    m_pCallBackHandle = pCallBackHandle;
    m_ulVideoPt   = ulVideoPt;
    m_ulAudioPt   = ulAudioPt;
    m_ulVideoClockFreq = ulVideoClockFreq;
    m_ulAudioClockFreq = ulAudioClockFreq;
    return 0;
}

int32_t CPackager::Release()
{
    return 0;
}

CH264Packager::CH264Packager()
{
    m_VideoRTPSeq = 0;
    m_AudioRTPSeq = 0;
    m_ulVideoTimeStamp =   0;
    m_ulAudioTimeStamp = 0;
    memset(&m_pSendVideoBuf ,0,RTP_PACK_BUF_SIZE);
    memset(&m_pSendAudioBuf ,0,RTP_PACK_BUF_SIZE);
    m_usCacheAudiFrameCount = 0;
}

CH264Packager::~CH264Packager()
{
}
//#define __CUT_PPS_SPS_FRAME__
int32_t CH264Packager::SendRawVideoData(const char* pBuf,uint32_t ulBufLen ,uint32_t ulTimeStamp)
{
    uint32_t ulSendSize = 0;
    uint32_t ulNalSize = 0;
    char* pNalBuf = (char*)pBuf;
    uint32_t ulStartcodeLen = 0;
    int32_t ulRet = 0;
#ifdef __CUT_PPS_SPS_FRAME__
    H264_NALU_HEADER* nalu_hdr = NULL;

    for(ulSendSize = 0;ulSendSize < ulBufLen;)
    {
        if( !FindStartCode(pNalBuf, ulBufLen-ulSendSize, ulStartcodeLen))
        {
            SVS_LOG((SVS_LM_INFO,"CH264Packager::SendRawVideoData not find the start code.Buff:[%d] sendSize:[%d]",ulBufLen,ulSendSize));
            break;
        }

        //strip the start code
        pNalBuf += ulStartcodeLen;
        ulSendSize += ulStartcodeLen;
        nalu_hdr = (H264_NALU_HEADER*)pNalBuf;
        switch(nalu_hdr->TYPE)
        {
            case H264_NALU_TYPE_SEI:
            case H264_NALU_TYPE_SPS:
            case H264_NALU_TYPE_PPS:
            {
                if(!FindNaluEnd(pNalBuf,ulBufLen-ulSendSize,ulNalSize))
                {
                    return -1;
                }
                ulSendSize += ulNalSize;
                break;
            }
            default:
            {
                ulNalSize = ulBufLen-ulSendSize;
                break;
            }
        }
        if(0 < ulNalSize)
        {
            ulRet = SendNaluVideoData(pNalBuf,ulNalSize,ulTimeStamp);
            pNalBuf += ulNalSize;
            ulSendSize +=  ulNalSize;
            if(0 != ulRet)
            {
                SVS_LOG((SVS_LM_ERROR,"CH264Packager::SendRawVideoData send Nal fail."));
                break;
            }
        }
    }
#else
    if( !FindStartCode(pBuf, ulBufLen, ulStartcodeLen))
    {
        SVS_LOG((SVS_LM_INFO,"CH264Packager::SendRawVideoData not find the start code.Buff:[%d] sendSize:[%d]",ulBufLen,ulSendSize));
        return 0;
    }
    pNalBuf = pNalBuf + ulStartcodeLen;
    ulNalSize = ulBufLen - ulStartcodeLen;
    ulRet = SendNaluVideoData(pNalBuf,ulNalSize,ulTimeStamp);
    if(0 != ulRet)
    {
        SVS_LOG((SVS_LM_WARNING,"CH264Packager::SendRawVideoData send Nal fail."));
        return 0;
    }

#endif
    return 0;
}

int32_t CH264Packager::SendNaluVideoData(const char* pBuf,uint32_t ulBufLen ,uint32_t ulTimeStamp)
{

    const char* pNalu = pBuf;//Startcode

    uint32_t ulNaluLen = ulBufLen - 1;//NAL head

    RTP_FIXED_HEADER        *rtp_hdr = (RTP_FIXED_HEADER*)&m_pSendVideoBuf;
    H264_NALU_HEADER        *nalu_hdr = (H264_NALU_HEADER*)pNalu;
    FU_INDICATOR            *fu_ind;
    FU_HEADER                *fu_hdr;
#ifdef __PRINT_MEDIA__
    SVS_LOG((SVS_LM_DEBUG,"the h264 nal info,TYPE:[%d] NRI:[%d] F:[%d]",nalu_hdr->TYPE,nalu_hdr->NRI,nalu_hdr->F));
#endif
    rtp_hdr->payload     = m_ulVideoPt;
    rtp_hdr->version     = 2;
    rtp_hdr->marker      = 0;
    rtp_hdr->extension   = 0;
    rtp_hdr->ssrc        = htonl(RTP_SSRC);

    m_ulVideoTimeStamp = ulTimeStamp*(m_ulVideoClockFreq/1000);

    uint32_t ulSize = RTP_HDR_LEN;

    if(ulNaluLen <= RTP_PLAYLOAD_LEN)
    {
        rtp_hdr->marker = 1;
        rtp_hdr->seq_no = htons(m_VideoRTPSeq++);
        rtp_hdr->timestamp = htonl(m_ulVideoTimeStamp);

        memcpy(&m_pSendVideoBuf[ RTP_HDR_LEN], pNalu, ulNaluLen + 1);
        ulSize += ulNaluLen + 1;

#ifdef __PRINT_MEDIA__
        SVS_LOG((SVS_LM_DEBUG,"Send Rtp Packet,version:[%d] seq:[%d] timestamp:[%d] marker:[%d] Len:[%d]",
                                  rtp_hdr->version,m_VideoRTPSeq,m_ulVideoTimeStamp,rtp_hdr->marker,ulSize));
#endif

        m_pCallBackHandle->handleRtpPacket(m_pSendVideoBuf,ulSize);

    }
    else
    {
        uint32_t k=0,lLastLen=0;
        k=ulNaluLen/RTP_PLAYLOAD_LEN;
        lLastLen = ulNaluLen%RTP_PLAYLOAD_LEN;

        if(lLastLen ==0)
        {
            lLastLen = RTP_PLAYLOAD_LEN;
            k--;
        }

        uint32_t t=0;
        rtp_hdr->timestamp = htonl(m_ulVideoTimeStamp);

        while(t<=k)
        {
            rtp_hdr->seq_no = htons(m_VideoRTPSeq++);
            if(t==0)
            {
                rtp_hdr->marker=0;
                fu_ind =(FU_INDICATOR*)&m_pSendVideoBuf[RTP_HDR_LEN];
                fu_ind->F=nalu_hdr->F;
                fu_ind->NRI=nalu_hdr->NRI;
                fu_ind->TYPE=RTP_FUA;

                fu_hdr =(FU_HEADER*)&m_pSendVideoBuf[RTP_HDR_LEN+1];
                fu_hdr->E=0;
                fu_hdr->R=0;
                fu_hdr->S=1;
                fu_hdr->TYPE=nalu_hdr->TYPE;

                memcpy(&m_pSendVideoBuf[RTP_HDR_LEN + 2], pNalu + 1, RTP_PLAYLOAD_LEN);

                ulSize =  RTP_PLAYLOAD_LEN+RTP_HDR_LEN +2;
#ifdef __PRINT_MEDIA__
                SVS_LOG((SVS_LM_DEBUG,"Send Rtp Packet,version:[%d] seq:[%d] timestamp:[%d] marker:[%d] Len:[%d]",
                                  rtp_hdr->version,m_VideoRTPSeq,m_ulVideoTimeStamp,rtp_hdr->marker,ulSize));
#endif
                m_pCallBackHandle->handleRtpPacket(m_pSendVideoBuf,ulSize);
                t++;
            }
            else if(k==t)
            {
                rtp_hdr->marker=1;
                fu_ind =(FU_INDICATOR*)&m_pSendVideoBuf[RTP_HDR_LEN];
                fu_ind->F=nalu_hdr->F;
                fu_ind->NRI=nalu_hdr->NRI;
                fu_ind->TYPE=RTP_FUA;

                fu_hdr =(FU_HEADER*)&m_pSendVideoBuf[RTP_HDR_LEN+1];
                fu_hdr->E=1;
                fu_hdr->R=0;
                fu_hdr->S=0;
                fu_hdr->TYPE=nalu_hdr->TYPE;

                memcpy(&m_pSendVideoBuf[RTP_HDR_LEN+2],pNalu+t*RTP_PLAYLOAD_LEN+1,lLastLen);
                t++;

                ulSize =lLastLen+RTP_HDR_LEN+2;
#ifdef __PRINT_MEDIA__
                SVS_LOG((SVS_LM_DEBUG,"Send Rtp Packet,version:[%d] seq:[%d] timestamp:[%d] marker:[%d] Len:[%d]",
                                  rtp_hdr->version,m_VideoRTPSeq,m_ulVideoTimeStamp,rtp_hdr->marker,ulSize));
#endif

                m_pCallBackHandle->handleRtpPacket(m_pSendVideoBuf,ulSize);
            }
            else if(t<k && 0!=t)
            {
                rtp_hdr->marker=0;
                fu_ind = (FU_INDICATOR*)&m_pSendVideoBuf[RTP_HDR_LEN];
                fu_ind->F = nalu_hdr->F;
                fu_ind->NRI = nalu_hdr->NRI;
                fu_ind->TYPE = RTP_FUA;

                fu_hdr =(FU_HEADER*)&m_pSendVideoBuf[RTP_HDR_LEN+1];
                fu_hdr->E = 0;
                fu_hdr->R = 0;
                fu_hdr->S = 0;
                fu_hdr->TYPE = nalu_hdr->TYPE;

                memcpy(&m_pSendVideoBuf[RTP_HDR_LEN+2], pNalu+t*RTP_PLAYLOAD_LEN+1, RTP_PLAYLOAD_LEN);
                ulSize = RTP_PLAYLOAD_LEN + RTP_HDR_LEN + 2;
#ifdef __PRINT_MEDIA__
                SVS_LOG((SVS_LM_DEBUG,"Send Rtp Packet,version:[%d] seq:[%d] timestamp:[%d] marker:[%d] Len:[%d]",
                                    rtp_hdr->version,m_VideoRTPSeq,m_ulVideoTimeStamp,rtp_hdr->marker,ulSize));
#endif

                m_pCallBackHandle->handleRtpPacket(m_pSendVideoBuf,ulSize);
                t++;
            }
        }
    }

    return 0;
}

int32_t CH264Packager::SendRawAudioData( const char* pBuf,uint32_t ulBufLen ,uint32_t ulTimeStamp)
{
    RTP_FIXED_HEADER *rtp_hdr = (RTP_FIXED_HEADER*)&m_pSendAudioBuf;
    rtp_hdr->payload = m_ulAudioPt;

    //switch (CMediaProcessor::instance().m_ulAudioFormat)
    //{
    //case PUMW_AUDIO_ENCODE_TYPE_PCMU:
    //    rtp_hdr->payload = PCMU;
    //    break;
    //case PUMW_AUDIO_ENCODE_TYPE_PCMA:
    //    rtp_hdr->payload = PCMA;
    //    break;
    //default:
    //    rtp_hdr->payload = PCMA;
    //    break;
    //}

    m_ulAudioTimeStamp = ulTimeStamp*(m_ulAudioClockFreq/1000);

    rtp_hdr->version     = 2;
    rtp_hdr->marker      = 0;
    rtp_hdr->extension = 0;
    rtp_hdr->ssrc        = htonl(RTP_SSRC);
    rtp_hdr->timestamp=htonl(m_ulAudioTimeStamp);

    uint32_t ulSize = 0;

    if(PT_ACC == m_ulAudioPt)
    {
        uint16_t off = 0;
        uint16_t payload = RTP_PLAYLOAD_LEN - AU_HEADER_SIZE;
        char *packet = &m_pSendAudioBuf[RTP_HDR_LEN];

        packet[0] = 0x00;
        packet[1] = 0x10;
        packet[2] = (ulBufLen & 0x1fe0) >> 5;
        packet[3] = (ulBufLen & 0x1f) << 3;
        //SetWBE(&packet[0],0);
        if (ulBufLen > payload)
        {
            ulSize = RTP_PLAYLOAD_LEN + RTP_HDR_LEN;
            while (ulBufLen > payload)
            {
                //SetWBE(&packet[2],off);
                memcpy(packet + AU_HEADER_SIZE, pBuf + off, payload);
                rtp_hdr->seq_no = htons(m_AudioRTPSeq++);
                rtp_hdr->marker = 0;
                m_pCallBackHandle->handleRtpPacket(m_pSendAudioBuf,ulSize);
                ulBufLen -= payload;
                off += payload;
            }
        }

        //SetWBE(&packet[2],off);
        memcpy(packet + AU_HEADER_SIZE, pBuf + off, ulBufLen);
        rtp_hdr->marker      = 1;
        rtp_hdr->seq_no = htons(m_AudioRTPSeq++);
        ulSize = ulBufLen+AU_HEADER_SIZE+RTP_HDR_LEN;
        m_pCallBackHandle->handleRtpPacket(m_pSendAudioBuf,ulSize);

    }
    else
    {
        rtp_hdr->seq_no = htons(m_AudioRTPSeq++);
        rtp_hdr->marker = 1;
        memcpy(&m_pSendAudioBuf[RTP_HDR_LEN],pBuf,ulBufLen);
        ulSize = ulBufLen + RTP_HDR_LEN;
        m_pCallBackHandle->handleRtpPacket(m_pSendAudioBuf,ulSize);
    }

    return 0;
}

bool CH264Packager::FindStartCode( const char* pBuf,  uint32_t ulBufLen, uint32_t& ulStartcodeLen)
{
    if(ulBufLen < 4)
    {
        return false;
    }

    if(pBuf[0]==0 && pBuf[1]==0 && pBuf[2] ==0 && pBuf[3] ==1)
    {
        ulStartcodeLen = 4;
        return true;
    }
    if(pBuf[0]==0 && pBuf[1]==0 && pBuf[2] ==1)
    {
        ulStartcodeLen = 3;
        return true;
    }

    return false;

}
bool CH264Packager::FindNaluEnd( const char* pBuf,  uint32_t ulBufLen, uint32_t& ulNalLen)
{
    if(ulBufLen < 4)
    {
        return false;
    }

    ulNalLen = 0;

    uint32_t i = 0;
    char* pPtr = NULL;
    char* pStartCode = NULL;
    uint32_t ulSearchLen = ulBufLen -3;
    while(i < ulSearchLen)
    {
        pPtr = (char*)&pBuf[i];
#ifdef __PRINT_MEDIA__
        SVS_LOG((SVS_LM_DEBUG,"CH264Packager::FindNaluEnd  StartCode:[%d:%d:%d].",pPtr[0],pPtr[1],pPtr[2]));
#endif
        if(pPtr[2] > 1)
        {
           i += 3;
        }
        else if (pPtr[1])
        {
           i += 2;
        }
        else if (pPtr[0])
        {
           i++;
        }
        else
        {
            if(( 0 == pPtr[0]&& 0 == pPtr[1] && 1 == pPtr[2])
                ||(0 == pPtr[0]&& 0 == pPtr[1] && 0 == pPtr[2]&& 1 == pPtr[3]))
            {
                pStartCode = pPtr;
                break;
            }
            i++;
        }
    }

    if(NULL == pStartCode)
    {
        ulNalLen = ulBufLen;
    }
    else
    {
        ulNalLen= pStartCode - pBuf;
    }
    return true;

}


