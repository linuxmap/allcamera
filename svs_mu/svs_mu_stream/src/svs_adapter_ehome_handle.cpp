#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_time.h"
#include <vms/vms.h>
#include "svs_adapter_ehome_handle.h"
#include "svs_adapter_net_conn_manager.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_session.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_exchange.h"
#include "svs_adapter_session_factory.h"
#include "svs_utility.h"
#include "svs_adapter_sdp.h"

static ACE_OS_Log_Msg_Attributes g_objACELogMsgAttr;

struct ehome_ps_start_code
{
    unsigned char start_code[3];
    unsigned char stream_id[1];
};


CEhomeStreamHandle::CEhomeStreamHandle()
{
    m_ullStreamId  = 0;
    m_lSessionId   = -1;
    m_lLinkHandle  = -1;
    m_VideoPayload = PT_TYPE_H264;
    m_AudioPayload = PT_TYPE_PCMA;
    m_usVideoRtpSeq= 0;
    m_usAudioRtpSeq= 0;
    m_ulVideoSsrc  = 0;
    m_ulAudioSsrc  = 0;
    m_ulVideoTimeStamp = 0;
    m_ulAudioTimeStamp = 0;
    m_ulLastVideoTime  = ACE_OS::gettimeofday();
    m_ulLastAudioTime  = ACE_OS::gettimeofday();
    m_ulFrameLen   = 0;
}
CEhomeStreamHandle::~CEhomeStreamHandle()
{
}
void    CEhomeStreamHandle::setStreamID(uint64_t ullSessionID)
{
    m_ullStreamId = ullSessionID;
}

void    CEhomeStreamHandle::setSessionID(int32_t lSessionID)
{
    m_lSessionId = lSessionID;
}

int32_t CEhomeStreamHandle::getSessionID()
{
    return m_lSessionId;
}
int32_t CEhomeStreamHandle::startLinkHandle(int32_t lLinkHandle)
{
    m_lLinkHandle = lLinkHandle;
    m_ulVideoSsrc  = 1000 +m_lLinkHandle;
    m_ulAudioSsrc  = 2000 +m_lLinkHandle;
    NET_EHOME_PREVIEW_DATA_CB_PARAM  StruCBParam;
    memset(&StruCBParam,0,sizeof(NET_EHOME_PREVIEW_DATA_CB_PARAM));

    StruCBParam.fnPreviewDataCB = preview_data_cb;
    StruCBParam.pUserData = this;
    if(!NET_ESTREAM_SetPreviewDataCB(m_lLinkHandle,&StruCBParam))
    {
        return RET_FAIL;
    }
    return RET_OK;
}
void CEhomeStreamHandle::stopLinkHandle()
{
    if(-1 < m_lLinkHandle)
    {
        (void)NET_ESTREAM_StopPreview(m_lLinkHandle);
    }
    return ;
}
void CEhomeStreamHandle::handle_preview_data(LONG  iPreviewHandle,
                                    NET_EHOME_PREVIEW_CB_MSG *pPreviewCBMsg)
{
#ifdef __PRINT_MEDIA__
    if (!ACE_Log_Msg::exists())
    {
        ACE_Log_Msg::inherit_hook(NULL, g_objACELogMsgAttr);
        SVS_LOG((SVS_LM_INFO, "ACE_Log_Msg::inherit_hook in ehome call back."));
    }
#endif

    if(NET_EHOME_STREAMDATA != pPreviewCBMsg->byDataType)
    {
        return;
    }

    send_ehome_stream((char*)pPreviewCBMsg->pRecvdata,(uint32_t)pPreviewCBMsg->dwDataLen);
    return;
}
void CEhomeStreamHandle::send_ehome_stream(char* pdata,uint32_t ulDataLen)
{
    char* pPsData = pdata;
    uint32_t ulPsDataLen = ulDataLen;
    ehome_ps_start_code* phead = (ehome_ps_start_code*)pdata;
    if ((phead->start_code[0] == 0x00)
        ||(phead->start_code[1] == 0x00)
        ||(phead->start_code[2] == 0x01))
    {
        /* parse the ps head */
        ProgramStreamPackHeader(pPsData,ulPsDataLen);
        ProgramSystemPackHeader(pPsData,ulPsDataLen);
        ProgramStreamMap(pPsData,ulPsDataLen);
        ProgramPrivateHeader(pPsData,ulPsDataLen);
        ProgramEStramHead(pPsData,ulPsDataLen);
    }
    else
    {
        /* not the start packet so cache it */
        if(EHOME_FRAME_MAX_SIZE < (m_ulFrameLen+ulDataLen)) {
            /* cache is not enough,so drop the frame */
            m_ulFrameLen = 0;
            return;
        }


        memcpy(&m_szFrameBuf[m_ulFrameLen],pdata,ulDataLen);
        m_ulFrameLen += ulDataLen;
    }

}
void CEhomeStreamHandle::send_h264_frame(char* pData,uint32_t ulLens)
{

    if((0x00 == pData[0])&&(0x00 == pData[1])&&(0x00 == pData[2])&&(0x01 == pData[3]))
    {
        /* new video frame comming,so send the last frame */
        if(0 < m_ulFrameLen){
            send_cache_vidoe_frame();
        }
    }
    /* need more data for this frame,so cache it*/
    memcpy(&m_szFrameBuf[m_ulFrameLen],pData,ulLens);
    m_ulFrameLen += ulLens;
    return;
}
void CEhomeStreamHandle::send_h264_key_nalu(char* pData,uint32_t ulLens)
{
    uint32_t ulSendSize = 0;
    uint32_t ulNalSize = 0;
    char* pStart = (char*)pData;
    char* pEnd  = pStart;
    uint32_t ulStartcodeLen = 0;
    H264_NALU_HEADER* nalu_hdr = NULL;

    uint32_t ulTimeStamp = getNextVideoTimeStamp();

    for(ulSendSize = 0;ulSendSize < ulLens;)
    {
        if((0x00 != pEnd[0])||(0x00 != pEnd[1])||(0x00 != pEnd[2])||(0x01 != pEnd[3]))
        {
            ulSendSize++;
            pEnd++;
            continue;
        }

        /* find the next start code */
        ulNalSize = pEnd - pStart;
        send_h264_nalu(pStart,ulNalSize,ulTimeStamp);
        pStart = (pEnd + H264_START_CODE);
        ulSendSize += (ulNalSize + H264_START_CODE);
        pEnd  = pStart;
    }
    /* find the next start code */
    ulNalSize = (pData + ulLens) - pStart;
    if(0 < ulNalSize) {
        send_h264_nalu(pStart,ulNalSize,ulTimeStamp);
    }


}

void CEhomeStreamHandle::send_h264_nalu(char* pData,uint32_t ulLens,uint32_t ulTimeStamp)
{
    uint16_t usPacketCount = ulLens/RTP_PLAYLOAD_LEN;
    if(0 != ulLens%RTP_PLAYLOAD_LEN)
    {
        usPacketCount++;
    }
    H264_NALU_HEADER* pNalu = (H264_NALU_HEADER*)(void*)pData;

    if(1 == usPacketCount)
    {
        send_h264_single_nalu(pData, ulLens, ulTimeStamp);
        return;
    }
    else
    {
        send_h264_multi_nalu(pData, ulLens, ulTimeStamp,usPacketCount);
    }
}
void CEhomeStreamHandle::send_h264_single_nalu(char* pData,uint32_t ulLens,uint32_t ulTimeStamp)
{
    CRtpPacket rtpPacket;
    size_t rtpHeadPos = sizeof(RTP_FIXED_HEADER);
    ACE_Message_Block *pMsg = NULL;
    int32_t nRet = RET_OK;


    pMsg = CMediaBlockBuffer::instance().allocMediaBlock();
    if (NULL == pMsg)
    {
        SVS_LOG((SVS_LM_WARNING,"ehome alloc media block fail."));
        return;
    }
    STREAM_TRANSMIT_PACKET *pPacket = (STREAM_TRANSMIT_PACKET *)(void*)pMsg->base();
    pMsg->wr_ptr(sizeof(STREAM_TRANSMIT_PACKET) - 1);
    pPacket->enPacketType        = STREAM_PACKET_TYPE_MEDIA_DATA;
    pPacket->PuStreamId          = m_ullStreamId;

    (void)rtpPacket.GeneratePacket(pMsg->wr_ptr(),rtpHeadPos);
    rtpPacket.SetTimeStamp(ulTimeStamp);
    rtpPacket.SetSSRC(m_ulVideoSsrc);
    getNextRtpSeq(m_usVideoRtpSeq, 1);
    rtpPacket.SetSeqNum(m_usVideoRtpSeq);
    rtpPacket.SetPayloadType(m_VideoPayload);
    rtpPacket.SetMarker(true);
    pMsg->wr_ptr((size_t)rtpHeadPos);
    pMsg->copy(pData,ulLens);

    nRet =  CStreamMediaExchange::instance()->addData(pMsg);
    if(RET_OK != nRet)
    {
        CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
        return;
    }
    return;
}
void CEhomeStreamHandle::send_h264_multi_nalu(char* pData,uint32_t ulLens,uint32_t ulTimeStamp,uint16_t usPacketCount)
{
    CRtpPacket rtpPacket;
    size_t rtpHeadPos = sizeof(RTP_FIXED_HEADER);
    ACE_Message_Block *pMsg = NULL;
    int32_t nRet = RET_OK;
    H264_NALU_HEADER    nalu_hdr;
    FU_INDICATOR  *fu_ind   = NULL;
    FU_HEADER     *fu_hdr   = NULL;

    bool bStart = true;

    H264_NALU_HEADER* pNalu = (H264_NALU_HEADER*)(void*)pData;

    /* save the nalu head */
    nalu_hdr.TYPE = pNalu->TYPE;
    nalu_hdr.NRI  = pNalu->NRI;
    nalu_hdr.F    = pNalu->F;

    pData++;
    ulLens--;

    do
    {
        pMsg = CMediaBlockBuffer::instance().allocMediaBlock();
        if (NULL == pMsg)
        {
            SVS_LOG((SVS_LM_WARNING,"ehome alloc media block fail."));
            return;
        }

        STREAM_TRANSMIT_PACKET *pPacket = (STREAM_TRANSMIT_PACKET *)(void*)pMsg->base();
        pMsg->wr_ptr(sizeof(STREAM_TRANSMIT_PACKET) - 1);
        pPacket->enPacketType        = STREAM_PACKET_TYPE_MEDIA_DATA;
        pPacket->PuStreamId          = m_ullStreamId;

        (void)rtpPacket.GeneratePacket(pMsg->wr_ptr(),rtpHeadPos);
        rtpPacket.SetTimeStamp(ulTimeStamp);
        rtpPacket.SetSSRC(m_ulVideoSsrc);
        getNextRtpSeq(m_usVideoRtpSeq, usPacketCount);
        rtpPacket.SetSeqNum(m_usVideoRtpSeq);
        rtpPacket.SetPayloadType(m_VideoPayload);
        pMsg->wr_ptr((size_t)rtpHeadPos);

        if(ulLens > RTP_PLAYLOAD_LEN) {
            fu_ind = (FU_INDICATOR*)pMsg->wr_ptr();
            fu_hdr = (FU_HEADER*)(pMsg->wr_ptr()+1);
            pMsg->wr_ptr(2);

            rtpPacket.SetMarker(false);

            fu_ind->F=nalu_hdr.F;
            fu_ind->NRI=nalu_hdr.NRI;
            fu_ind->TYPE=RTP_FUA;
            fu_hdr->S=0;
            fu_hdr->E=0;
            fu_hdr->R=0;

            if(bStart)
            {
                fu_hdr->S=1;
                bStart = false;
            }

            fu_hdr->TYPE=nalu_hdr.TYPE;

            pMsg->copy(pData,RTP_PLAYLOAD_LEN);

            nRet =  CStreamMediaExchange::instance()->addData(pMsg);
            if(RET_OK != nRet)
            {
                CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
                return;
            }
            pData  += RTP_PLAYLOAD_LEN;
            ulLens -= RTP_PLAYLOAD_LEN;
        }
        else {
            fu_ind = (FU_INDICATOR*)pMsg->wr_ptr();
            fu_hdr = (FU_HEADER*)(pMsg->wr_ptr()+1);
            pMsg->wr_ptr(2);

            rtpPacket.SetMarker(true);

            fu_ind->F=nalu_hdr.F;
            fu_ind->NRI=nalu_hdr.NRI;
            fu_ind->TYPE=RTP_FUA;
            fu_hdr->S=0;
            fu_hdr->E=1;
            fu_hdr->R=0;

            fu_hdr->TYPE=nalu_hdr.TYPE;

            pMsg->copy(pData,ulLens);

            nRet =  CStreamMediaExchange::instance()->addData(pMsg);
            if(RET_OK != nRet)
            {
                CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
                return;
            }
            pData  += ulLens;
            ulLens = 0;
        }
    }while(0 < ulLens);
}


void CEhomeStreamHandle::send_h265_frame(char* pData,uint32_t ulLens)
{
}
void CEhomeStreamHandle::send_audio_frame(char* pData,uint32_t ulLens)
{
    ACE_Message_Block *pMsg = NULL;
    size_t rtpHeadPos = sizeof(RTP_FIXED_HEADER);
    int32_t nRet = RET_OK;
    if(ulLens  > RTP_PLAYLOAD_LEN)
    {
        SVS_LOG((SVS_LM_WARNING,"the audio len:[%d] is big than RTP PLAYLOAD.",ulLens));
        return;
    }

    pMsg = CMediaBlockBuffer::instance().allocMediaBlock();
    if (NULL == pMsg)
    {
        SVS_LOG((SVS_LM_WARNING,"ehome alloc media block fail."));
        return ;
    }

    STREAM_TRANSMIT_PACKET *pPacket = (STREAM_TRANSMIT_PACKET *)(void*)pMsg->base();
    pMsg->wr_ptr(sizeof(STREAM_TRANSMIT_PACKET) - 1);
    pPacket->enPacketType        = STREAM_PACKET_TYPE_MEDIA_DATA;
    pPacket->PuStreamId          = m_ullStreamId;



    CRtpPacket rtpPacket;
    (void)rtpPacket.GeneratePacket(pMsg->wr_ptr(),ulLens);
    uint32_t ulTimeStamp = getNextAudioTimeStamp();
    rtpPacket.SetTimeStamp(ulTimeStamp);
    rtpPacket.SetSSRC(m_ulAudioSsrc);
    rtpPacket.SetMarker(true);
    getNextRtpSeq(m_usAudioRtpSeq, 1);
    rtpPacket.SetSeqNum(m_usAudioRtpSeq);
    rtpPacket.SetPayloadType(m_AudioPayload);

    pMsg->wr_ptr((size_t)rtpHeadPos);
    pMsg->copy(pData,ulLens);



    nRet =  CStreamMediaExchange::instance()->addData(pMsg);
    if(RET_OK != nRet)
    {
        CMediaBlockBuffer::instance().freeMediaBlock(pMsg);
        return;
    }
    return;
}
void CEhomeStreamHandle::send_cache_vidoe_frame()
{
    if(PT_TYPE_H264 == m_VideoPayload)
    {
        /* skip the start code */
        char* pData = (char*)&m_szFrameBuf[0];
        uint32_t ulLens = m_ulFrameLen;
        pData  += H264_START_CODE;
        ulLens -= H264_START_CODE;

        H264_NALU_HEADER* pNalu = (H264_NALU_HEADER*)pData;

        if((H264_NALU_TYPE_SEI == pNalu->TYPE)
            ||(H264_NALU_TYPE_SPS == pNalu->TYPE)
            ||(H264_NALU_TYPE_PPS == pNalu->TYPE))
        {
            send_h264_key_nalu(pData,ulLens);
        }
        else
        {
            uint32_t ulTimeStamp = getNextVideoTimeStamp();
            send_h264_nalu(pData,ulLens,ulTimeStamp);
        }
    }
    else if(PT_TYPE_H265 == m_VideoPayload)
    {
        send_h265_frame((char*)&m_szFrameBuf[0],m_ulFrameLen);
    }
    else
    {
    }
    m_ulFrameLen = 0;
    return;
}
void CEhomeStreamHandle::send_vidoe_frame(char* pData,uint32_t ulLens)
{
    if(PT_TYPE_H264 == m_VideoPayload)
    {
        send_h264_frame(pData,ulLens);
    }
    else if(PT_TYPE_H265 == m_VideoPayload)
    {
        send_h265_frame(pData,ulLens);
    }
    else
    {
    }
}



void CEhomeStreamHandle::preview_data_cb(LONG  iPreviewHandle,
                     NET_EHOME_PREVIEW_CB_MSG *pPreviewCBMsg,void *pUserData)
{
    CEhomeStreamHandle* pHandle = (CEhomeStreamHandle*)pUserData;
    pHandle->handle_preview_data(iPreviewHandle, pPreviewCBMsg);
}


void CEhomeStreamHandle::ProgramStreamPackHeader(char*& pData,uint32_t& ulLens)
{
    //00 00 01 ba
    program_stream_pack_header *PsHead = (program_stream_pack_header *)pData;
    if ((PsHead->PackStart.start_code[0] != 0x00)
        ||(PsHead->PackStart.start_code[1] != 0x00)
        ||(PsHead->PackStart.start_code[2] != 0x01)
        || (PsHead->PackStart.stream_id[0] != 0xBA))
    {
        return;
    }

    uint32_t ulPsHeadLen = sizeof(program_stream_pack_header) + PsHead->pack_stuffing_length;

    if(ulPsHeadLen >= ulLens)
    {
        /* error */
        return;
    }
    if(0 < ulPsHeadLen)
    {
        ulLens -= ulPsHeadLen;
        pData += ulPsHeadLen;
    }
    return;
}
void CEhomeStreamHandle::ProgramSystemPackHeader(char*& pData,uint32_t& ulLens)
{
    //00 00 01 bb
    program_system_pack_header *SysHead = (program_system_pack_header *)pData;
    if ((SysHead->PackStart.start_code[0] != 0x00)
        || (SysHead->PackStart.start_code[1] != 0x00)
        || (SysHead->PackStart.start_code[2] != 0x01)
        || (SysHead->PackStart.stream_id[0] != 0xBB))
    {
        return;
    }
    littel_endian_size psm_length;// from PackLength;
    psm_length.byte[0] = SysHead->PackLength.byte[1];
    psm_length.byte[1] = SysHead->PackLength.byte[0];

    uint32_t ulSysHeadLen = sizeof(pack_start_code) + sizeof(littel_endian_size) + psm_length.length;

    if(ulSysHeadLen >= ulLens)
    {
        /* error */
        return;
    }

    if(0 < ulSysHeadLen)
    {
        pData  += ulSysHeadLen;
        ulLens -= ulSysHeadLen;
    }

    return;
}
void CEhomeStreamHandle::ProgramStreamMap(char*& pData,uint32_t& ulLens)
{

    program_stream_map* PSMPack = (program_stream_map*)pData;

    if (ulLens < sizeof(program_stream_map))
    {
        return;
    }
    if ((PSMPack->PackStart.start_code[0] != 0x00)
        || (PSMPack->PackStart.start_code[1] != 0x00)
        || (PSMPack->PackStart.start_code[2] != 0x01)
        || (PSMPack->PackStart.stream_id[0] != 0xBC))
    {
        return;
    }

    littel_endian_size psm_length;
    psm_length.byte[0] = PSMPack->PackLength.byte[1];
    psm_length.byte[1] = PSMPack->PackLength.byte[0];

    uint32_t ulPsMapLen = sizeof(pack_start_code) + sizeof(littel_endian_size) + psm_length.length;

    if(ulPsMapLen >= ulLens)
    {
        /* error */
        return;
    }



    if(PT_TYPE_MAX != m_VideoPayload)
    {
        /* have parser the pa map head */
        if(0 < ulPsMapLen)
        {
            ulLens -= ulPsMapLen;
            pData  += ulPsMapLen;
        }
        return;
    }

    littel_endian_size psm_info_Length;
    psm_info_Length.byte[0] = PSMPack->program_stream_info_length[1];
    psm_info_Length.byte[1] = PSMPack->program_stream_info_length[0];

    uint32_t ulPsInfolength = psm_info_Length.length;

    pData += sizeof(program_stream_map) + ulPsInfolength + sizeof(program_elementary_stream_map_info);

    int32_t ulPsMapInfoLength
        = ulPsMapLen - sizeof(program_stream_map)
          - ulPsInfolength - sizeof(program_elementary_stream_map_info)
          - sizeof(uint32_t); /*unsigned char CRC_32[4];*/

    while (ulPsMapInfoLength >= 4) {

        program_elementary_stream_info* eInfo
                  = (program_elementary_stream_info*)(void*)pData;

        /*video*/
        if(VIDEO_STREAM_TYPE_MPEG4 == eInfo->stream_type)
        {
            m_VideoPayload = PT_TYPE_MPEG4;
        }
        else if(VIDEO_STREAM_TYPE_H264 == eInfo->stream_type)
        {
            m_VideoPayload = PT_TYPE_H264;
        }
        else if(VIDEO_STREAM_TYPE_H265 == eInfo->stream_type)
        {
            m_VideoPayload = PT_TYPE_H265;
        }
        /*audio*/
        else if(AUDIO_STREAM_TYPE_G711 == eInfo->stream_type)
        {
            m_AudioPayload = PT_TYPE_PCMA;
        }

        littel_endian_size es_info_Length;
        es_info_Length.byte[0] = eInfo->elementary_stream_info_length[1];
        es_info_Length.byte[1] = eInfo->elementary_stream_info_length[0];
        uint32_t esInfoLen = es_info_Length.length;
#ifdef __PRINT_MEDIA__
        SVS_LOG((SVS_LM_DEBUG,"*******system head stream type[0x%02x],esInfoLen:[%d]**************",eInfo->stream_type,esInfoLen));
#endif

        pData  += sizeof(program_elementary_stream_info) + esInfoLen;
        ulLens -= sizeof(program_elementary_stream_info) + esInfoLen;;

        ulPsMapInfoLength -= sizeof(program_elementary_stream_info) + esInfoLen;
    }

    if(0 < ulPsMapLen)
    {
        ulLens -= ulPsMapLen;
        pData  += ulPsMapLen;
    }

    return;
}
void CEhomeStreamHandle::ProgramPrivateHeader(char*& pData,uint32_t& ulLens)
{

    program_private_head* PriData = (program_private_head*)pData;


    if (ulLens < sizeof(program_private_head))
    {
        return;
    }
    if ((PriData->PackStart.start_code[0] != 0x00)
        || (PriData->PackStart.start_code[1] != 0x00)
        || (PriData->PackStart.start_code[2] != 0x01)
        || ((PriData->PackStart.stream_id[0] != 0xBD)
        && (PriData->PackStart.stream_id[0] != 0xBE)
        && (PriData->PackStart.stream_id[0] != 0xBF)))
    {
        return;
    }

    littel_endian_size psm_length;
    psm_length.byte[0] = PriData->PackLength.byte[1];
    psm_length.byte[1] = PriData->PackLength.byte[0];


    uint32_t ulpsPrivateLen = sizeof(program_private_head) + psm_length.length;

    if(ulpsPrivateLen >= ulLens)
    {
        /* error */
        return;
    }
    if(0 < ulpsPrivateLen)
    {
        ulLens -= ulpsPrivateLen;
        pData  += ulpsPrivateLen;
    }

}
void CEhomeStreamHandle::ProgramEStramHead(char*& pData,uint32_t& ulLens)
{
    program_stream_e* PSEPack = (program_stream_e*)pData;
    if ((PSEPack->PackStart.start_code[0] != 0x00)
        || (PSEPack->PackStart.start_code[1] != 0x00)
        || (PSEPack->PackStart.start_code[2] != 0x01))
    {
        return ;
    }


    if (ulLens < sizeof(program_stream_e))
    {
        return ;
    }

    littel_endian_size pse_length;
    pse_length.byte[0] = PSEPack->PackLength.byte[1];
    pse_length.byte[1] = PSEPack->PackLength.byte[0];

    uint32_t ulesLength    = pse_length.length - 3 - PSEPack->stuffing_length;//PTS DTS stuffing_length
//#ifdef __PRINT_MEDIA__
//    SVS_LOG((SVS_LM_ERROR, "CEhomeStreamHandle::ProgramEStramHead, EsLen:[%d],pseLen:[%d] stuffLen:[%d]",
//                                         ulesLength,pse_length.length,PSEPack->stuffing_length ));
//#endif
    //uint8_t streamId  = PSEPack->PackStart.stream_id[0];
    uint32_t ulPesHeadLen = sizeof(program_stream_e) + PSEPack->stuffing_length;

    if(ulPesHeadLen >= ulLens)
    {
//#ifdef __PRINT_MEDIA__
//    SVS_LOG((SVS_LM_ERROR, "CEhomeStreamHandle::ProgramEStramHead, discard pes head len:[%d],packet len:[%d].",
//                                         ulPesHeadLen,ulLens));
//#endif
        /* error */
        return;
    }

    if(0 < ulPesHeadLen) {
        ulLens -= ulPesHeadLen;
        pData += ulPesHeadLen;
    }


    if (PSEPack->PackStart.stream_id[0] == 0xC0)
    {
        send_audio_frame(pData,ulLens);
    }
    else if (PSEPack->PackStart.stream_id[0] == 0xE0)
    {
        send_vidoe_frame(pData,ulLens);
    }
    else
    {
        pData += ulLens;
        ulLens = 0;
    }

    return;
}
void CEhomeStreamHandle::getNextRtpSeq(uint16_t& usCurSeq,uint16_t count)
{
    if((0xFFFF - usCurSeq) < count)
    {
        usCurSeq = 0;
    }
    else
    {
        usCurSeq++;
    }
}

uint32_t CEhomeStreamHandle::getNextVideoTimeStamp()
{
    ACE_Time_Value cur = ACE_OS::gettimeofday();
    if((PT_TYPE_H264 == m_VideoPayload)||(PT_TYPE_H265 == m_VideoPayload))
    {
        uint32_t ulPass = (cur.usec() - m_ulLastVideoTime.usec())/1000;
        //m_ulVideoTimeStamp += (ulPass*H264_HZ)/1000;
        //TODO:
        m_ulVideoTimeStamp += 3600;

    }
    m_ulLastVideoTime = cur;
    return m_ulVideoTimeStamp;
}
uint32_t CEhomeStreamHandle::getNextAudioTimeStamp()
{
    ACE_Time_Value cur = ACE_OS::gettimeofday();
    if((PT_TYPE_PCMU == m_usAudioRtpSeq)||(PT_TYPE_PCMA == m_usAudioRtpSeq))
    {
        uint32_t ulPass = (cur.usec() - m_ulLastAudioTime.usec())/1000;
        //m_ulAudioTimeStamp += (ulPass*PCM_HZ)/1000;
        m_ulAudioTimeStamp += 160;//TODO:

    }
    m_ulLastAudioTime = cur;
    return m_ulAudioTimeStamp;
}



CEhomeHandle::CEhomeHandle()
{
    m_ulLastInvalidAlarmTime = 0;
    m_ulLastRecvTime         = 0;
    m_unStreamCount          = 0;
    m_ullStreamID            = 0;

    m_enPlayType          = 0;

    m_lListenHandle          = -1;
}

CEhomeHandle::~CEhomeHandle()
{
    try
    {
        this->closeHandle();
    }
    catch(...)
    {}
    m_ulLastInvalidAlarmTime = 0;
    m_unStreamCount          = 0;
    m_ullStreamID            = 0;
}

int32_t CEhomeHandle::openHandle(const ACE_INET_Addr &localAddr)
{
    ACE_Log_Msg::init_hook(g_objACELogMsgAttr);

    if (NETWORK_HANDLE_STATUS_INIT != getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,"open ehome handle address[%s:%d] fail, status[%d] abnormal.",
                        localAddr.get_host_addr(),
                        localAddr.get_port_number(),
                        getStatus()));
        return RET_ERR_STATUS_ABNORMAL;
    }

    setLocalAddr(localAddr);

    NET_EHOME_LISTEN_PREVIEW_CFG elisten;
    memset(&elisten,0,sizeof(NET_EHOME_LISTEN_PREVIEW_CFG));

    strncpy(elisten.struIPAdress.szIP,localAddr.get_host_addr(),128);
    elisten.struIPAdress.wPort = localAddr.get_port_number();
    elisten.fnNewLinkCB = handle_link;
    elisten.pUser       = this;
    elisten.byLinkMode  = CStreamConfig::instance()->getEhomeTransType();

    LONG nRet = NET_ESTREAM_StartListenPreview(&elisten);
    if(-1 == nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"ehome handle ip[%s] port[%d] open fail.",
            localAddr.get_host_addr(),
            localAddr.get_port_number()));
        return RET_FAIL;
    }
    m_lListenHandle = nRet;
    setStatus( NETWORK_HANDLE_STATUS_OPEN);
    SVS_LOG((SVS_LM_DEBUG,"ehome handle ip[%s] port[%d] open success.",
            localAddr.get_host_addr(),
            localAddr.get_port_number()));

    return RET_OK;
}

void CEhomeHandle::closeHandle()
{
    if(0 > m_lListenHandle)
    {
        return;
    }
    (void)NET_ESTREAM_StopListenPreview(m_lListenHandle);
    m_lListenHandle = -1;

    setStatus( NETWORK_HANDLE_STATUS_INIT);
    SVS_LOG((SVS_LM_DEBUG,"close ehome handle[%s:%d] success.",
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number()));
    return;
}

int32_t CEhomeHandle::startHandle(uint64_t ullStreamID, const ACE_INET_Addr &remoteAddr)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ListMutex);
    if ((NETWORK_HANDLE_STATUS_OPEN != getStatus())
      &&(NETWORK_HANDLE_STATUS_START != getStatus()))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Start ehome handle fail, status [%d] is incorrect.",
            getStatus()));
        return RET_ERR_STATUS_ABNORMAL;
    }

    m_RemoteAddr = remoteAddr;

    CEhomeStreamHandle* pStreamHandle = NULL;

    pStreamHandle = SVS_NEW(pStreamHandle);
    if(NULL == pStreamHandle)
    {
        return RET_ERR_SYS_NEW;
    }
    pStreamHandle->setStreamID(ullStreamID);

    m_mapStreamHandle.insert(EHOMESTREAMMAP::value_type(ullStreamID,pStreamHandle));

    m_unStreamCount++;

    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        m_ulLastRecvTime = SVS_GetSecondTime();
        setStatus( NETWORK_HANDLE_STATUS_START);
    }

    SVS_LOG((SVS_LM_INFO,"start stream[%Q] ehome handle[%s:%d], all stream count[%d].",
            ullStreamID,
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number(),
            m_unStreamCount));

    return RET_OK;
}

int32_t CEhomeHandle::stopHandle(uint64_t ullStreamID)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ListMutex);
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        return RET_ERR_STATUS_ABNORMAL;
    }

    m_unStreamCount--;

    if (0 == m_unStreamCount)
    {
        m_ullStreamID = 0;
        setStatus( NETWORK_HANDLE_STATUS_OPEN);
    }
    CEhomeStreamHandle* pStreamHandle = NULL;

    EHOMESTREAMMAP::iterator iter = m_mapStreamHandle.find(ullStreamID);
    if(iter != m_mapStreamHandle.end())
    {
        pStreamHandle = iter->second;
        pStreamHandle->stopLinkHandle();
        m_mapStreamHandle.erase(iter);
        SVS_DELETE(pStreamHandle);
        pStreamHandle = NULL;
    }

    SVS_LOG((SVS_LM_INFO,"stop stream[%Q] ehome handle[%s:%d], all stream count[%d].",
            ullStreamID,
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number(),
            m_unStreamCount));
    return RET_OK;
}

int32_t CEhomeHandle::sendMessage(uint64_t ullStreamID,
                            const char *pDataBuffer,
                            uint32_t unDataSize,
                            const ACE_INET_Addr &peerAddr)
{

    return RET_OK;
}

bool CEhomeHandle::checkHandleStatus(uint64_t /*ullStreamID*/, uint32_t unTimeout)
{
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        SVS_LOG((SVS_LM_INFO,"check udp port[%d] status fail, current status[%d].",
                getLocalAddr().get_port_number(), getStatus()));
        return false;
    }

    uint32_t ulCostTime = SVS_GetSecondTime() - m_ulLastRecvTime;
    if (unTimeout <= ulCostTime)
    {
        SVS_LOG((SVS_LM_INFO,"check udp port[%d] status fail, not recv any data in [%u]s.",
                        getLocalAddr().get_port_number(), ulCostTime));
        return false;
    }

    SVS_LOG((SVS_LM_DEBUG,"check udp port[%d] status success.",
                    getLocalAddr().get_port_number()));
    return true;
}

void CEhomeHandle::setEhomeSessionID(uint64_t ullStreamID,int32_t ulSessionID)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ListMutex);
    CEhomeStreamHandle* pStreamHandle = NULL;

    EHOMESTREAMMAP::iterator iter = m_mapStreamHandle.find(ullStreamID);
    if(iter != m_mapStreamHandle.end())
    {
        pStreamHandle = iter->second;
        pStreamHandle->setSessionID(ulSessionID);
    }
}
BOOL CEhomeHandle::handle_input_link(LONG lLinkHandle, NET_EHOME_NEWLINK_CB_MSG *pNewLinkCBMsg)
{
    if (!ACE_Log_Msg::exists())
    {
        ACE_Log_Msg::inherit_hook(NULL, g_objACELogMsgAttr);
        SVS_LOG((SVS_LM_INFO, "ACE_Log_Msg::inherit_hook in ehome call back."));
    }
    /*1. find the stream handle by session id */
    ACE_Guard<ACE_Thread_Mutex> locker(m_ListMutex);
    CEhomeStreamHandle* pStreamHandle = NULL;

    EHOMESTREAMMAP::iterator iter = m_mapStreamHandle.begin();
    for(;iter != m_mapStreamHandle.end();++iter)
    {
        pStreamHandle = iter->second;
        if(pNewLinkCBMsg->iSessionID == pStreamHandle->getSessionID())
        {
            break;
        }
        pStreamHandle = NULL;
    }

    if(NULL == pStreamHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "handle ehome new input handle fail, ehome session id [%d] is incorrect.",
            pNewLinkCBMsg->iSessionID));
        return FALSE;
    }

    if(RET_OK != pStreamHandle->startLinkHandle(lLinkHandle))
    {
        SVS_LOG((SVS_LM_WARNING,
            "start ehome handle fail, ehome session id [%d].",
            pNewLinkCBMsg->iSessionID));
        return FALSE;
    }

    return TRUE;

}


BOOL CEhomeHandle::handle_link(LONG lLinkHandle, NET_EHOME_NEWLINK_CB_MSG *pNewLinkCBMsg, void *pUserData)
{
    CEhomeHandle* pEhomeHandle = (CEhomeHandle*)pUserData;
    return pEhomeHandle->handle_input_link(lLinkHandle, pNewLinkCBMsg);
}

