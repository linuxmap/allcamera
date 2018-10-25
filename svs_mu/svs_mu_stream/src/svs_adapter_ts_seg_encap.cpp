#include "svs_adapter_ts_seg_encap.h"


CEs2TSEncap::CEs2TSEncap()
{
    m_pTsSegHandle = NULL;
    m_pUserData    = NULL;
    m_ulSegDuration  = 0;
    m_ulSendDuration = HLS_SEND_DURATION_MIN;
    m_ulLastCheck    = 0;
    m_bOpen        = false;
    m_ulSegStatus  = TS_SEG_TAG_START;
}
CEs2TSEncap::~CEs2TSEncap()
{
    m_MpegTsEnc.release();
}
int32_t CEs2TSEncap::Init(CTsSegEncapHandle* pTsSegHandle,void* pUserData)
{
    m_pTsSegHandle = pTsSegHandle;
    m_pUserData    = pUserData;
    m_MpegTsEnc.init(this,NULL);
    m_MpegTsEnc.open();
    m_bOpen = true;
    m_ulSegStatus  = TS_SEG_TAG_START;
    return 0;
}

void CEs2TSEncap::handleTsStream(const char* pTSData, uint32_t unDataSize,
                                uint32_t ulDuration, void* pUserData)
{
    if(NULL != m_pTsSegHandle)
    {
        m_pTsSegHandle->handleTsSegPacket(pTSData,unDataSize,ulDuration,m_ulSegStatus,m_pUserData);
        m_ulSegDuration += ulDuration;
        if(TS_SEG_TAG_START ==m_ulSegStatus)
        {
            m_ulSegStatus = TS_SEG_TAG_MIDDLE;
        }
    }
}

void CEs2TSEncap::handleVideoEsStream(const char* pEsData, uint32_t unDataSize,
                                               uint32_t unTimestamp,bool bkeyFrame)
{

    /*if(!m_bOpen)
    {
        m_MpegTsEnc.open();
        m_bOpen = true;
        m_ulSegStatus  = TS_SEG_TAG_START;
    }*/
    //if(bkeyFrame)
    //{
        checkSegInfo();
    //}
    m_MpegTsEnc.writeVideoEsStream(pEsData,unDataSize, unTimestamp,bkeyFrame);
}

void CEs2TSEncap::handleAudioEsStream(const char* pEsData, uint32_t unDataSize,
                                               uint32_t unTimestamp)
{
    m_MpegTsEnc.writeAudioEsStream(pEsData,unDataSize,unTimestamp);
}
void CEs2TSEncap:: checkSegInfo()
{
    uint32_t ulDuration = CalcSendDuration();
    if(ulDuration <= (m_ulSegDuration/TIME_TICK_SECOND))
    {
        m_ulSegStatus  = TS_SEG_TAG_END;
        //m_MpegTsEnc.close();
        m_ulSegDuration = 0;
        //m_MpegTsEnc.open();
        m_bOpen = true;
        m_ulSegStatus  = TS_SEG_TAG_START;
    }
}


uint32_t CEs2TSEncap::CalcSendDuration()
{
    if(m_ulSendDuration < HLS_SEND_DURATION_MAX)
    {
        time_t ulCurTick = time(NULL);
        if((0 < m_ulLastCheck)&&((ulCurTick - m_ulLastCheck) >= HLS_SEND_DURATION_INTER))
        {
            m_ulSendDuration++;
        }
        m_ulLastCheck = ulCurTick;
       }
    return m_ulSendDuration;
}


