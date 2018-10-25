/*
 * MpegPsDec.cpp
 *
 *  Created on: 2012-12-18
 */
#include "svs_log_msg.h"
#include "svs_adapter_mpeg_ps_dec.h"

#define MAX_BUFFER_LEN (1024*1024)

CMpegPsDec::CMpegPsDec()
{
    m_pEsStreamHandle                = NULL;
    m_pUserData                      = NULL;
    m_enCurType                      = ES_TYPE_MAX;
    m_pCurMediaData                  = NULL;
    m_ulCurMediaLen                  = 0;

}

CMpegPsDec::~CMpegPsDec()
{
    // TODO Auto-generated destructor stub
    m_pEsStreamHandle                = NULL;
    m_pUserData                      = NULL;
    m_pCurMediaData                  = NULL;
}


int32_t CMpegPsDec::init(CMpegPsDecHandle* pHandle, void* pUserData)
{
    if(NULL == pHandle)
    {
        return -1;
    }
    m_pEsStreamHandle = pHandle;
    m_pUserData       = pUserData;
    return 0;
}


int32_t CMpegPsDec::writePsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp)
{
    if(NULL == pInData)
    {
        return -1;
    }

    if(NULL == m_pEsStreamHandle)
    {
        return -1;
    }

    m_pCurMediaData = (char*)pInData;
    m_ulCurMediaLen = unDataSize;

    //bool bControl = true;
    bool bFound   = false;
    uint32_t unStartCode = (uint32_t)PS_STARTCODE_INVAL;

    char pBuffer[MAX_BUFFER_LEN] ={0};
    uint32_t  nBufferLen = 0;
    //uint8_t  ucStreamID  = STREAM_TYPE_VIDEO_MPEG1;

    const char* pLastBytePos = pInData + unDataSize;

    while(m_pCurMediaData < pLastBytePos )
    {
        if(0 == m_ulCurMediaLen)
        {
            return -1;
        }
        unStartCode = getStartCode();


        switch(unStartCode)
        {
            case PACK_START_CODE:
            {
                if(0 != StripPSHeader())
                {
                    return -1;
                }
                break;
            }
            case SYSTEM_HEADER_START_CODE:
            {
                if(0 != StripSysHeader())
                {
                    return -1;
                }
                break;
            }
            case PADDING_STREAM:
            {
                break;
            }
            case PRIVATE_STREAM_2:
            {
                break;
            }
            case PROGRAM_STREAM_MAP:
            {
                if(0 != StripPsmPacket())
                {
                    return -1;
                }
                break;
            }
            case H264_PES_STREAM_CODE:
            case G711_PES_STREAM_CODE:
            {
                if(0!= StripPESHeader())
                {
                    return -1;
                }
                if( nBufferLen < MAX_BUFFER_LEN)
                {
                       memcpy(pBuffer+nBufferLen,m_pCurMediaData,m_ulCurMediaLen);
                    nBufferLen += m_ulCurMediaLen;
                    m_pCurMediaData += m_ulCurMediaLen;
                }
                bFound = true;
                break;
            }
            default:
            {
                m_pCurMediaData += 1;
                break;
            }
        }
    }

    if(!bFound)
    {
        return -1;
    }


    if( H264_PES_STREAM_CODE == unStartCode )
    {
        m_pEsStreamHandle->handleVideoEsStream(pBuffer,nBufferLen,unTimestamp,VIDEO_STREAM_TYPE_H264,m_pUserData);
    }
    else if( G711_PES_STREAM_CODE == unStartCode )
    {
        m_pEsStreamHandle->handleAudioEsStream(pBuffer,nBufferLen,unTimestamp,AUDIO_STREAM_TYPE_G711,m_pUserData);

    }
    else
    {
        return -1;
    }
    return 0;
}

void CMpegPsDec::release()
{
    m_pEsStreamHandle = NULL;
    m_pUserData       = NULL;
    return;
}


int32_t CMpegPsDec::StripPSHeader()
{
    if(NULL == m_pCurMediaData)
    {
        return -1;
    }

    if(PS_HEAD_LEN > m_ulCurMediaLen)
    {
        return -1;
    }

    uint32_t unLen = (uint32_t)(uint8_t)(m_pCurMediaData[13]&0x07) + PS_HEAD_LEN;
    if(unLen > m_ulCurMediaLen)
    {
        return -1;
    }

    m_pCurMediaData +=  unLen;
    m_ulCurMediaLen -= unLen;
    return 0;
}

int32_t CMpegPsDec::StripSysHeader()
{
    if(NULL == m_pCurMediaData)
    {
        return -1;
    }

    if(SYS_HEAD_LEN_MIN > m_ulCurMediaLen)
    {
        return -1;
    }

    uint16_t usHeadSize =  ntohs(*(uint16_t*)&m_pCurMediaData[4])
                           +sizeof(uint16_t)+PS_STARTCODE_LEN;//lint !e826
    if(usHeadSize > m_ulCurMediaLen)
    {
        return -1;
    }

    m_pCurMediaData +=  usHeadSize;
    m_ulCurMediaLen -= usHeadSize;
    return 0;
}

int32_t CMpegPsDec::StripPsmPacket()
{
    if(NULL == m_pCurMediaData)
    {
        return -1;
    }

    if(PSM_HEAD_LEN_MIN > m_ulCurMediaLen)
    {
        return -1;
    }
    /*lint-e826*/
    uint16_t usHeadSize =  ntohs(*(uint16_t*)&m_pCurMediaData[4])
                           +sizeof(uint16_t)+PS_STARTCODE_LEN;
    if(usHeadSize > m_ulCurMediaLen)
    {
        return -1;
    }

    m_pCurMediaData +=  usHeadSize;
    m_ulCurMediaLen -= usHeadSize;
    return 0;
}

int32_t CMpegPsDec::StripPESHeader()
{
    if(NULL == m_pCurMediaData)
    {
        return -1;
    }

    if(PSM_HEAD_LEN_MIN > m_ulCurMediaLen)
    {
        return -1;
    }

    uint8_t usHeadSize =  9;

    uint16_t usPackLen =  ntohs(*(uint16_t*)&m_pCurMediaData[4]);//lint !e826

    uint8_t usHeadLen = (uint8_t)m_pCurMediaData[8];

    m_pCurMediaData +=  usHeadSize + usHeadLen;

    m_ulCurMediaLen  = usPackLen - (3 + usHeadLen) ;

    return 0;
}

uint32_t CMpegPsDec::getStartCode()const
{
    uint32_t unStartCode = 0;
    if(NULL == m_pCurMediaData)
    {
        return (uint32_t)PS_STARTCODE_INVAL;
    }
    if(PS_STARTCODE_LEN > m_ulCurMediaLen)
    {
        return (uint32_t)PS_STARTCODE_INVAL;
    }
    uint32_t unVal = (uint32_t)m_pCurMediaData[0];
    unStartCode = ((unStartCode << 8) | unVal) & 0xFFFFFF;
    unVal = (uint32_t)m_pCurMediaData[1];
    unStartCode = ((unStartCode << 8) | unVal) & 0xFFFFFF;
    unVal = (uint32_t)m_pCurMediaData[2];
    unStartCode = ((unStartCode << 8) | unVal) & 0xFFFFFF;
    if(PS_STARTCODE != unStartCode)
    {
        return (uint32_t)PS_STARTCODE_INVAL;
    }
    unVal = (uint32_t)(uint8_t)m_pCurMediaData[3];
    unStartCode = ((unStartCode << 8) | unVal) & 0xFFFFFFFF;

    return unStartCode;
}
