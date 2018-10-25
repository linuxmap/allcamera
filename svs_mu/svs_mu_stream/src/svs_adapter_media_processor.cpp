#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_rtp_packet.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_exchange.h"
#include "svs_adapter_common.h"
#include "svs_adapter_sdp.h"


#define __PRINT_MEDIA__


CPacketList::CPacketList()
{
    m_SessionId = 0;
}
CPacketList::~CPacketList()
{
    try
    {
        Clear();
    }
    catch(...)
    {
    }
}

int32_t CPacketList::Send(CStreamMediaProcessor* pProcessor)
{
    if (m_PacketList.empty())
    {
        return RET_OK;
    }

    if (NULL == pProcessor)
    {
        return RET_FAIL;
    }

    CStreamSession *pSession = pProcessor->getSession();
    if (NULL == pSession)
    {
        return RET_FAIL;
    }
    m_SessionId = pSession->getStreamId();

    ACE_Message_Block *pMb = NULL;
    while (!m_PacketList.empty())
    {
        pMb = m_PacketList.front();
        if (RET_OK != pSession->sendMediaData(&pMb, 1))
        {
            break;
        }

        pProcessor->statFlux(pMb->length());

        m_PacketList.pop_front();
        CMediaBlockBuffer::instance().freeMediaBlock(pMb);
    }

    return RET_FAIL;
};

void CPacketList::Clear()
{
    ACE_Message_Block *pMb = NULL;
    while (!m_PacketList.empty())
    {
        pMb = m_PacketList.front();
        CMediaBlockBuffer::instance().freeMediaBlock(pMb);
        m_PacketList.pop_front();
    }
}

int32_t CPacketList::Save(const ACE_Message_Block *pMb)
{
    if (NULL == pMb)
    {
        return RET_FAIL;
    }

    if (m_PacketList.size() >= MAX_BUF_PACKET_NUM)
    {
        ClearFrame(2);
    }

    ACE_Message_Block *pNewMb = pMb->duplicate();
    if (NULL == pNewMb)
    {
        return RET_FAIL;
    }

    m_PacketList.push_back(pNewMb);
    return RET_OK;
}

void CPacketList::ClearFrame(uint32_t num)
{
    ACE_Message_Block *pMb = NULL;
    CRtpPacket rtpPacket;

    uint32_t i = 0;

    while (!m_PacketList.empty())
    {
        pMb = m_PacketList.front();
        m_PacketList.pop_front();

        if (RET_OK == rtpPacket.ParsePacket(pMb->rd_ptr(), pMb->length()))
        {
            if (rtpPacket.GetMarker())
            {
                i++;
                if (i >= num)
                {
                    CMediaBlockBuffer::instance().freeMediaBlock(pMb);
                    break;
                }
            }
        }

        CMediaBlockBuffer::instance().freeMediaBlock(pMb);
    }

    SVS_LOG((SVS_LM_INFO,"session [%Q] clear [%d] frame success. now packet num[%d]",
        m_SessionId, num, m_PacketList.size()));

}

CStreamMediaProcessor::CStreamMediaProcessor()
{

    m_pSendSession  = NULL;
    m_SendSessionId = 0;
}

CStreamMediaProcessor::~CStreamMediaProcessor()
{
    try
    {
        if (NULL != m_pSendSession)
        {
            CStreamSessionFactory::instance()->releaseSession(m_pSendSession);
            m_pSendSession = NULL;
        }
    }
    catch(...)
    {
    }
}

int32_t CStreamMediaProcessor::Init(uint64_t ullSendSessionID)
{
    if (NULL != m_pSendSession)
    {
        return RET_FAIL;
    }

    m_pSendSession = CStreamSessionFactory::instance()->findSession(ullSendSessionID);
    if (NULL == m_pSendSession)
    {
        SVS_LOG((SVS_LM_ERROR,"Init media processor fail, can't find send session by id[%Q].",
                ullSendSessionID));
        return RET_ERR_PARAM;
    }

    m_SendSessionId = ullSendSessionID;

    SVS_LOG((SVS_LM_INFO,"CStreamMediaProcessor::Init success. send session[%p] stream id[%Q].",
            m_pSendSession,
            m_SendSessionId));
    return RET_OK;
}

CStreamSession* CStreamMediaProcessor::getSession()
{
    return m_pSendSession;
}

uint32_t CStreamMediaProcessor::getInputRate()const
{
    return m_dataStat.getRate();
}

void CStreamMediaProcessor::statFlux(uint32_t flux)
{
    m_dataStat.addFlux(flux);
}

uint32_t CStreamMediaProcessor::getFlux(uint32_t& ulStartTime, uint32_t& ulEndTime)
{
    return m_dataStat.getFlux(ulStartTime, ulEndTime);
}

CDirectProcessor::CDirectProcessor()
{
    m_SavePacketNum = 0;
    m_DropPacketNum = 0;
}

CDirectProcessor::~CDirectProcessor()
{
}

void CDirectProcessor::Send(ACE_Message_Block * pMb)
{
    if (NULL == m_pSendSession)
    {
        return;
    }

    if (NULL == pMb)
    {
        SVS_LOG((SVS_LM_ERROR,"CDirectProcessor::Send fail, mb is null."));
        return;
    }

    m_dataStat.stat(pMb->length());

    if (RET_OK != m_PacketBuf.Send(this))
    {
        Save(pMb);
        return;
    }

    if (RET_OK == m_pSendSession->sendMediaData(&pMb, 1))
    {
        statFlux(pMb->length());
    }
    else
    {
        Save(pMb);
    }

    return;
}

void  CDirectProcessor::Save(const ACE_Message_Block *pMb)
{
    if (RET_OK == m_PacketBuf.Save(pMb))
    {
        m_SavePacketNum++;
    }
    else
    {
        m_DropPacketNum++;
    }

    if (1 == m_SavePacketNum % SAVE_PACKET_PRINT_NUM)
    {
        SVS_LOG((SVS_LM_WARNING,"save packet num[%u] drop packet num[%u] stream id[%Q].",
            m_SavePacketNum, m_DropPacketNum, m_SendSessionId));
    }

    return;
}

void CDirectProcessor::Dump(ACE_HANDLE handle)
{
    if (NULL == m_pSendSession)
    {
        return;
    }

    m_dataStat.stat(0);

    char buf[KILO + 1] = {0};

    snprintf(buf,
             KILO,
             "\tSend session[%lld] ContentID[%s] service type[%s] rate[%d]kbps"
             "local addr[%s:%d] peer addr[%s:%d] trans protocol[%s] \n",
             m_pSendSession->getStreamId(),
             m_pSendSession->getContentID(),
             CSessionInfo::instance().getPlayType(m_pSendSession->getPlayType()),
             m_dataStat.getRate(),
             m_pSendSession->getVideoAddr().get_host_addr(),
             m_pSendSession->getVideoAddr().get_port_number(),
             m_pSendSession->getPeerAddr().get_host_addr(),
             m_pSendSession->getPeerAddr().get_port_number(),
             CSessionInfo::instance().getTransProtocol(m_pSendSession->getTransProtocol()));

    (void)ACE::send(handle, buf, strlen(buf));
    return;
}


CStreamMediaProcessorSet::CStreamMediaProcessorSet()
{
}

CStreamMediaProcessorSet::~CStreamMediaProcessorSet()
{
    try
    {
        Destroy();
    }
    catch(...)
    {
        ;
    }
}

//
int32_t CStreamMediaProcessorSet::AddMediaProcessor(CStreamMediaProcessor* pstProcessor)
{
    if (NULL == pstProcessor)
    {
        SVS_LOG((SVS_LM_ERROR,
            "CStreamMediaProcessorSet::AddMediaProcessor fail, processor is null."));
        return RET_FAIL;
    }

    ProcessorIter iter;
    for (iter = m_EsOverRtpProcessList.begin(); iter != m_EsOverRtpProcessList.end(); iter++)
    {
        if (*iter == pstProcessor)
        {
            SVS_LOG((SVS_LM_ERROR,
                "CStreamMediaProcessorSet::AddMediaProcessor fail, processor [%p] already in convert list.",
                pstProcessor));
            return RET_FAIL;
        }
    }

    m_EsOverRtpProcessList.push_back(pstProcessor);
    SVS_LOG((SVS_LM_INFO,"CStreamMediaProcessorSet::AddMediaProcessor success."
        " processor[%p] processor set[%p]"
        " now processor in list num[%d].",
        pstProcessor,
        this,
        m_EsOverRtpProcessList.size()));

    return RET_OK;
}

//
int32_t CStreamMediaProcessorSet::DelMediaProcessor(CStreamMediaProcessor*& pProcessor)
{
    if (NULL == pProcessor)
    {
        SVS_LOG((SVS_LM_DEBUG,"CStreamMediaProcessorSet::DelMediaProcessor fail, processor is null."));
        return RET_ERR_PARAM;
    }

    ProcessorIter iter;

    for (iter = m_EsOverRtpProcessList.begin(); iter != m_EsOverRtpProcessList.end(); iter++)
    {
        if (*iter == pProcessor)
        {
            SVS_LOG((SVS_LM_INFO,
                "CStreamMediaProcessorSet::DelMediaProcessor success, processor [%p] find in es list."
                " now processor in list num[%d].",
                pProcessor,
                m_EsOverRtpProcessList.size()));
            m_EsOverRtpProcessList.erase(iter);

            delete pProcessor;
            pProcessor = NULL;
            break;
        }
    }



    if (m_EsOverRtpProcessList.empty())
    {
        SVS_LOG((SVS_LM_INFO,"CStreamMediaProcessorSet::DelMediaProcessor list is empty."));
        return RET_ERR_LIST_EMPTY;
    }

    return RET_OK;
}

int32_t CStreamMediaProcessorSet::GetSendSessionType( CStreamMediaProcessor* pProcessor,
                                                     uint32_t& unSessionType)const
{
    if(NULL == pProcessor)
    {
        return RET_FAIL;
    }
    CStreamSession*  pSendSession = pProcessor->getSession();
    if(NULL == pSendSession)
    {
        return RET_FAIL;
    }

    unSessionType = pSendSession->getMediaTransType();

    return RET_OK;
}

int32_t CStreamMediaProcessorSet::Send(ACE_Message_Block* pMb)
{
    ProcessorIter iter;
    CStreamMediaProcessor* pProcessor = NULL;

    if (NULL == pMb)
    {
        SVS_LOG((SVS_LM_ERROR,"CStreamMediaProcessorSet::Send fail, mb is null."));
        return RET_FAIL;
    }

    if((0 == m_EsOverRtpProcessList.size()))
    {
        return RET_OK;
    }

    for (iter = m_EsOverRtpProcessList.begin(); iter != m_EsOverRtpProcessList.end(); iter++)
    {
        pProcessor = *iter;
        if (NULL != pProcessor)
        {
            pProcessor->Send(pMb);
        }
    }

    return RET_OK;
}


int32_t CStreamMediaProcessorSet::SetEncodeFormat(uint32_t unEncodeFormat)
{
    return RET_OK;
}
void CStreamMediaProcessorSet::Destroy()
{
    ProcessorIter iter;
    CStreamMediaProcessor* pProcessor = NULL;

    for (iter = m_EsOverRtpProcessList.begin(); iter != m_EsOverRtpProcessList.end(); ++iter)
    {
        pProcessor = *iter;

        delete pProcessor;
        pProcessor = NULL;
    }

    m_EsOverRtpProcessList.clear();

}

CStreamPsMediaProcessorSet::CStreamPsMediaProcessorSet()
{
    m_RtpFrameOrganizer.init(this,MAX_RTP_FRAME_CACHE_NUM);
    m_usRtpSeq = 0;
    m_VideoPayload = PT_TYPE_H264;
    m_AudioPayload = PT_TYPE_PCMU;
}

CStreamPsMediaProcessorSet::~CStreamPsMediaProcessorSet()
{
}


int32_t CStreamPsMediaProcessorSet::Send(ACE_Message_Block* pMb)
{
    if (NULL == pMb)
    {
        SVS_LOG((SVS_LM_ERROR,"CStreamMediaProcessorSet::Send fail, mb is null."));
        return RET_FAIL;
    }

    return m_RtpFrameOrganizer.insertRtpPacket(pMb);
}

void CStreamPsMediaProcessorSet::handleRtpFrame(RTP_FRAME_LIST &rtpFrameList)
{
    es_frame_info FrameInfo;
    memset(&FrameInfo,0,sizeof(es_frame_info));


    bool bPsStream = checkProgramStreamHead(FrameInfo,rtpFrameList);
    if(bPsStream)
    {
        dealProgramStreamRtpFrame(FrameInfo,rtpFrameList);
        return;
    }
    /* not the ps stream,and send direct */
    CRtpPacket rtpPacket;

    /* skip all packet rtp head */
    RTP_FRAME_LIST::iterator iter = rtpFrameList.begin();
    ACE_Message_Block* pMb = NULL;
    for(;iter != rtpFrameList.end();++iter)
    {
        pMb = *iter;
        if (NULL == pMb)
        {
            return;
        }
        if (RET_OK != rtpPacket.ParsePacket(pMb->rd_ptr(), pMb->length()))
        {
            SVS_LOG((SVS_LM_ERROR, "fail to send ps rtp packet, parse rtp packet fail."));
            return;
        }
        rtpPacket.SetPayloadType(FrameInfo.payload);
        (void)CStreamMediaProcessorSet::Send(pMb);
    }


}
bool CStreamPsMediaProcessorSet::checkProgramStreamHead(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    ACE_Message_Block* pMb = rtpFrameList.front();

    CRtpPacket rtpPacket;
    if (RET_OK != rtpPacket.ParsePacket(pMb->rd_ptr(), pMb->length()))
    {
        SVS_LOG((SVS_LM_ERROR, "fail to send ps rtp packet, parse rtp packet fail."));
        return RET_FAIL;
    }
    char* buffer  = (char*)(void*)pMb->rd_ptr() + rtpPacket.GetHeadLen();
    pack_start_code *PackStart = (pack_start_code *)(void*)buffer;
    if ((PackStart->start_code[0] == 0x00)
        ||(PackStart->start_code[1] == 0x00)
        ||(PackStart->start_code[2] == 0x01))
    {
        return true;
    }

    /* not the ps stream ,so check the h264¡¢h265 and audio */
    if((0x00 == buffer[0])&&(0x00 == buffer[1])&&(0x00 == buffer[2])&&(0x01 == buffer[3]))
    {
        /* H264 or H265 start code */
        /* try H264 */
        H264_NALU_HEADER* pNalu = (H264_NALU_HEADER*)(void*)&buffer[4];
        if((!pNalu->F)&&(H264_NAL_UNDEFINED < pNalu->TYPE || H264_NAL_END > pNalu->TYPE)) {
            FrameInfo.payload = PT_TYPE_H264;
        }

        /* try H265 */
        H265_NALU_HEADER* pH265Nalu = (H265_NALU_HEADER*)(void*)&buffer[4];
        if((!pH265Nalu->F)&&(HEVC_NAL_END > pH265Nalu->TYPE)) {
            FrameInfo.payload = PT_TYPE_H265;
        }
    }
    else
    {
        /* audio */
        FrameInfo.payload = PT_TYPE_PCMU;
    }
#ifdef __PRINT_MEDIA__
        SVS_LOG((SVS_LM_ERROR, "CStreamPsMediaProcessorSet::checkProgramStreamHead ,this is no ps ,payload:[%d].",FrameInfo.payload ));
#endif
    return false;
}
void CStreamPsMediaProcessorSet::dealProgramStreamRtpFrame(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    m_enStatus  = PARSING_PACK_HEADER;

    bool bSkipHead = false;
    ACE_Message_Block*pMb = NULL;
    RTP_FRAME_LIST FrameParserList;

    do
    {
        /* 1.skip all packet rtp head */
        if(!bSkipHead)
        {
            if(RET_OK != SkipAllRtpHead(FrameInfo,rtpFrameList,FrameParserList))
            {
                break;
            }
            bSkipHead = true;
        }

        if(0 == FrameParserList.size())
        {
            break;
        }

        /* 2.Parser the ps stream */
        switch(m_enStatus) {
            case PARSING_PACK_HEADER:
            {
                ProgramStreamPackHeader(FrameInfo, FrameParserList); //0x00 00 01 BA
                break;
            }
            case PARSING_SYSTEM_HEADER:
            {
                ProgramSystemPackHeader(FrameInfo, FrameParserList);//0x00 00 01 BB
                break;
            }
            case PARSING_SYSMAP_HEADER:
            {
                ProgramStreamMap(FrameInfo, FrameParserList); //0x00 00 01 BC
                break;
            }
            case PARSING_PRIVATE_HEADER:
            {
                ProgramPrivateHeader(FrameInfo, FrameParserList); //0x00 00 01 BD/BE/BF
                break;
            }
            case PARSING_PES_PACKET:
            {
                ProgramEStramHead(FrameInfo, FrameParserList);
                break;
            }
            case PARSING_FOUND_DATA:
            {
                ProgramKnowFrame(FrameInfo, FrameParserList);
                break;
            }
            default:
            {
                break;
            }
        }

    }while(true);


    while(0  < FrameParserList.size())
    {
         pMb = FrameParserList.front();
         FrameParserList.pop_front();
         CMediaBlockBuffer::instance().freeMediaBlock(pMb);
    }
    return;
}



int32_t CStreamPsMediaProcessorSet::SkipAllRtpHead(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList,RTP_FRAME_LIST &PatserFrameList)
{
    ACE_Message_Block* pMb = rtpFrameList.front();
    ACE_Message_Block* pNewMb = NULL;
    CRtpPacket rtpPacket;
    if (RET_OK != rtpPacket.ParsePacket(pMb->rd_ptr(), pMb->length()))
    {
        SVS_LOG((SVS_LM_ERROR, "fail to send ps rtp packet, parse rtp packet fail."));
        return RET_FAIL;
    }
    FrameInfo.timestamp = rtpPacket.GetTimeStamp();
    FrameInfo.ssrc      = rtpPacket.GetSSRC();
    FrameInfo.payload   = rtpPacket.GetPayloadType();

    uint32_t headLen = 0;

    /* skip all packet rtp head */
    RTP_FRAME_LIST::iterator iter = rtpFrameList.begin();
    for(;iter != rtpFrameList.end();++iter)
    {
        pMb = *iter;
        /* copy mb to th parser list */
        pNewMb = pMb->duplicate();
        if (NULL == pNewMb)
        {
            SVS_LOG((SVS_LM_ERROR,"fail to insert rtp packet, duplicate mb fail."));
            return RET_FAIL;
        }
        PatserFrameList.push_back(pNewMb);
        if (RET_OK != rtpPacket.ParsePacket(pNewMb->rd_ptr(), pNewMb->length()))
        {
            SVS_LOG((SVS_LM_ERROR, "fail to send ps rtp packet, parse rtp packet fail."));
            return RET_FAIL;
        }
        headLen = rtpPacket.GetHeadLen();
        /* skip the rtp head */
        pNewMb->rd_ptr(headLen);
        FrameInfo.length += pNewMb->length();
    }

    return RET_OK;
}

void CStreamPsMediaProcessorSet::ProgramStreamPackHeader(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    ACE_Message_Block* pMb = rtpFrameList.front();

    char* buffer  = (char*)(void*)pMb->rd_ptr();
    uint32_t   length  = pMb->length();

    //00 00 01 ba
    program_stream_pack_header *PsHead = (program_stream_pack_header *)buffer;
    if ((PsHead->PackStart.start_code[0] != 0x00)
        ||(PsHead->PackStart.start_code[1] != 0x00)
        ||(PsHead->PackStart.start_code[2] != 0x01)
        || (PsHead->PackStart.stream_id[0] != 0xBA))
    {
        m_enStatus  = PARSING_SYSTEM_HEADER;
        return;
    }
    //unsigned char pack_stuffing_length = PsHead->stuffinglen & '\x07';
    //uint32_t ulPsHeadLen = sizeof(program_stream_pack_header) + pack_stuffing_length;

    uint32_t ulPsHeadLen = sizeof(program_stream_pack_header) + PsHead->pack_stuffing_length;

    if(ulPsHeadLen >= length)
    {
        /* free the fisrt mb */
        rtpFrameList.pop_front();
        CMediaBlockBuffer::instance().freeMediaBlock(pMb);
        if(0 == rtpFrameList.size())
        {
            return;
        }
        ulPsHeadLen -= length;
        pMb = rtpFrameList.front();
    }
    if(0 < ulPsHeadLen)
    {
        pMb->rd_ptr(ulPsHeadLen);
    }
    m_enStatus  = PARSING_SYSTEM_HEADER;
    return;
}
void CStreamPsMediaProcessorSet::ProgramSystemPackHeader(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    //00 00 01 bb
    ACE_Message_Block* pMb = rtpFrameList.front();

    char* buffer  = (char*)(void*)pMb->rd_ptr();
    uint32_t   length  = pMb->length();

    program_system_pack_header *SysHead = (program_system_pack_header *)buffer;
    if ((SysHead->PackStart.start_code[0] != 0x00)
        || (SysHead->PackStart.start_code[1] != 0x00)
        || (SysHead->PackStart.start_code[2] != 0x01)
        || (SysHead->PackStart.stream_id[0] != 0xBB))
    {
        m_enStatus  = PARSING_SYSMAP_HEADER;
        return;
    }
    littel_endian_size psm_length;// from PackLength;
    psm_length.byte[0] = SysHead->PackLength.byte[1];
    psm_length.byte[1] = SysHead->PackLength.byte[0];

    uint32_t ulSysHeadLen = sizeof(pack_start_code) + sizeof(littel_endian_size) + psm_length.length;

    if(ulSysHeadLen >= length)
    {
        /* free the fisrt mb */
        rtpFrameList.pop_front();
        CMediaBlockBuffer::instance().freeMediaBlock(pMb);
        if(0 == rtpFrameList.size())
        {
            return;
        }
        ulSysHeadLen -= length;
        pMb = rtpFrameList.front();
    }

    if(0 < ulSysHeadLen)
    {
        pMb->rd_ptr(ulSysHeadLen);
    }

    m_enStatus  = PARSING_SYSMAP_HEADER;

    return;
}
void CStreamPsMediaProcessorSet::ProgramStreamMap(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    ACE_Message_Block* pMb = rtpFrameList.front();

    char* buffer  = (char*)(void*)pMb->rd_ptr();
    uint32_t   length  = pMb->length();

    program_stream_map* PSMPack = (program_stream_map*)buffer;

    if (length < sizeof(program_stream_map))
    {
        m_enStatus  = PARSING_PRIVATE_HEADER;
        return;
    }
    if ((PSMPack->PackStart.start_code[0] != 0x00)
        || (PSMPack->PackStart.start_code[1] != 0x00)
        || (PSMPack->PackStart.start_code[2] != 0x01)
        || (PSMPack->PackStart.stream_id[0] != 0xBC))
    {
        m_enStatus  = PARSING_PRIVATE_HEADER;
        return;
    }

    littel_endian_size psm_length;
    psm_length.byte[0] = PSMPack->PackLength.byte[1];
    psm_length.byte[1] = PSMPack->PackLength.byte[0];

    uint32_t ulPsMapLen = sizeof(pack_start_code) + sizeof(littel_endian_size) + psm_length.length;

    if(ulPsMapLen >= length)
    {
        /* free the fisrt mb */
        rtpFrameList.pop_front();
        CMediaBlockBuffer::instance().freeMediaBlock(pMb);
        if(0 == rtpFrameList.size())
        {
            return;
        }
        ulPsMapLen -= length;
        pMb = rtpFrameList.front();
    }



    if(PT_TYPE_MAX != m_VideoPayload)
    {
        /* have parser the pa map head */
        if(0 < ulPsMapLen)
        {
            pMb->rd_ptr(ulPsMapLen);
        }
        m_enStatus  = PARSING_PRIVATE_HEADER;
        return;
    }

    littel_endian_size psm_info_Length;
    psm_info_Length.byte[0] = PSMPack->program_stream_info_length[1];
    psm_info_Length.byte[1] = PSMPack->program_stream_info_length[0];

    uint32_t ulPsInfolength = psm_info_Length.length;

    buffer += sizeof(program_stream_map) + ulPsInfolength + sizeof(program_elementary_stream_map_info);

    int32_t ulPsMapInfoLength
        = ulPsMapLen - sizeof(program_stream_map)
          - ulPsInfolength - sizeof(program_elementary_stream_map_info)
          - sizeof(uint32_t); /*unsigned char CRC_32[4];*/

    while (ulPsMapInfoLength >= 4) {

        program_elementary_stream_info* eInfo
                  = (program_elementary_stream_info*)(void*)buffer;

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

        buffer += sizeof(program_elementary_stream_info) + esInfoLen;

        ulPsMapInfoLength -= sizeof(program_elementary_stream_info) + esInfoLen;
    }

    if(0 < ulPsMapLen)
    {
        pMb->rd_ptr(ulPsMapLen);
    }

    m_enStatus  = PARSING_PRIVATE_HEADER;

    return;
}
void CStreamPsMediaProcessorSet::ProgramPrivateHeader(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    ACE_Message_Block* pMb = rtpFrameList.front();

    char* buffer  = (char*)(void*)pMb->rd_ptr();
    uint32_t   length  = pMb->length();

    program_private_head* PriData = (program_private_head*)buffer;


    if (length < sizeof(program_private_head))
    {
        m_enStatus  = PARSING_PES_PACKET;
        return;
    }
    if ((PriData->PackStart.start_code[0] != 0x00)
        || (PriData->PackStart.start_code[1] != 0x00)
        || (PriData->PackStart.start_code[2] != 0x01)
        || ((PriData->PackStart.stream_id[0] != 0xBD)
        && (PriData->PackStart.stream_id[0] != 0xBE)
        && (PriData->PackStart.stream_id[0] != 0xBF)))
    {
        m_enStatus  = PARSING_PES_PACKET;
        return;
    }

    littel_endian_size psm_length;
    psm_length.byte[0] = PriData->PackLength.byte[1];
    psm_length.byte[1] = PriData->PackLength.byte[0];


    uint32_t ulpsPrivateLen = sizeof(program_private_head) + psm_length.length;

    if(ulpsPrivateLen >= length)
    {
        /* free the fisrt mb */
        rtpFrameList.pop_front();
        CMediaBlockBuffer::instance().freeMediaBlock(pMb);
        if(0 == rtpFrameList.size())
        {
            return;
        }
        ulpsPrivateLen -= length;
        pMb = rtpFrameList.front();
    }
    if(0 < ulpsPrivateLen)
    {
        pMb->rd_ptr(ulpsPrivateLen);
    }

    m_enStatus  = PARSING_PES_PACKET;
}
void CStreamPsMediaProcessorSet::ProgramEStramHead(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    ACE_Message_Block* pMb = rtpFrameList.front();

    char* buffer  = (char*)(void*)pMb->rd_ptr();
    uint32_t   length  = pMb->length();

    program_stream_e* PSEPack = (program_stream_e*)buffer;
    if ((PSEPack->PackStart.start_code[0] != 0x00)
        || (PSEPack->PackStart.start_code[1] != 0x00)
        || (PSEPack->PackStart.start_code[2] != 0x01))
    {
        m_enStatus  = PARSING_FOUND_DATA;
        return ;
    }


    if (length < sizeof(program_stream_e))
    {
        m_enStatus  = PARSING_FOUND_DATA;
        return ;
    }


    littel_endian_size pse_length;
    pse_length.byte[0] = PSEPack->PackLength.byte[1];
    pse_length.byte[1] = PSEPack->PackLength.byte[0];

    FrameInfo.length    = pse_length.length - 3 - PSEPack->stuffing_length;//PTS DTS stuffing_length
#ifdef __PRINT_MEDIA__
    SVS_LOG((SVS_LM_ERROR, "CStreamPsMediaProcessorSet::ProgramEStramHead, EsLen:[%d],pseLen:[%d] stuffLen:[%d]",
                                         FrameInfo.length,pse_length.length,PSEPack->stuffing_length ));
#endif
    FrameInfo.streamId  = PSEPack->PackStart.stream_id[0];
    uint32_t ulPesHeadLen = sizeof(program_stream_e) + PSEPack->stuffing_length;

    if(ulPesHeadLen >= length)
    {
#ifdef __PRINT_MEDIA__
    SVS_LOG((SVS_LM_ERROR, "CStreamPsMediaProcessorSet::ProgramEStramHead, discard pes head len:[%d],packet len:[%d],payload:[%d].",
                                         ulPesHeadLen,length,FrameInfo.payload));
#endif
        /* free the fisrt mb */
        rtpFrameList.pop_front();
        CMediaBlockBuffer::instance().freeMediaBlock(pMb);
        if(0 == rtpFrameList.size())
        {
            return;
        }
        ulPesHeadLen -= length;
        pMb = rtpFrameList.front();
    }

    if(0 < ulPesHeadLen) {
        pMb->rd_ptr(ulPesHeadLen);
    }
    m_enStatus  = PARSING_PES_PACKET;


    if (PSEPack->PackStart.stream_id[0] == 0xC0)
    {
        FrameInfo.payload = m_AudioPayload;
        sendEsAudioFrame(FrameInfo,rtpFrameList);

    }
    else if (PSEPack->PackStart.stream_id[0] == 0xE0)
    {
        FrameInfo.payload = m_VideoPayload;
        sendEsVideoFrame(FrameInfo,rtpFrameList);

    }
    else
    {
        skipunKnowFrame(FrameInfo,rtpFrameList);
    }

    return;
}
void CStreamPsMediaProcessorSet::ProgramKnowFrame(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    ACE_Message_Block* pMb = rtpFrameList.front();

    char* buffer  = (char*)(void*)pMb->rd_ptr();

    if((0x00 == buffer[0])&&(0x00 == buffer[1])&&(0x00 == buffer[2])&&(0x01 == buffer[3]))
    {
        /* H264 or H265 start code */
        /* try H264 */
        H264_NALU_HEADER* pNalu = (H264_NALU_HEADER*)(void*)&buffer[4];
#ifdef __PRINT_MEDIA__
        SVS_LOG((SVS_LM_ERROR, "CStreamPsMediaProcessorSet::ProgramKnowFrame,H264,F:[%d],Type:[%d]",
                                                     pNalu->F,pNalu->TYPE));
#endif
        if((!pNalu->F)&&(H264_NAL_UNDEFINED < pNalu->TYPE || H264_NAL_END > pNalu->TYPE)) {
            FrameInfo.payload = PT_TYPE_H264;


            sendEsVideoFrame(FrameInfo,rtpFrameList);
            return;
        }

        /* try H265 */
        H265_NALU_HEADER* pH265Nalu = (H265_NALU_HEADER*)(void*)&buffer[4];
        if((!pH265Nalu->F)&&(HEVC_NAL_END > pH265Nalu->TYPE)) {
            FrameInfo.payload = PT_TYPE_H265;
            sendEsVideoFrame(FrameInfo,rtpFrameList);
            return;
        }
    }
    else
    {
        /* audio */
        FrameInfo.payload = PT_TYPE_PCMU;
        sendEsAudioFrame(FrameInfo,rtpFrameList);
    }

    return;
}

void CStreamPsMediaProcessorSet::sendEsAudioFrame(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    if(PT_TYPE_MAX == m_AudioPayload)
    {
        ProgramKnowFrame(FrameInfo,rtpFrameList);
        return;
    }

    uint32_t ulEsLen = FrameInfo.length;
    ACE_Message_Block* pMb     = NULL;
    ACE_Message_Block* pSendMb = NULL;
    CRtpPacket rtpPacket;
    size_t rtpHeadPos = sizeof(RTP_FIXED_HEADER);

    do
    {
        if(0 == ulEsLen){
            break;
        }

        pMb = rtpFrameList.front();

        if(ulEsLen >= pMb->length()) {

            ulEsLen -= pMb->length();
            rtpFrameList.pop_front();

            char* rd_ptr = pMb->rd_ptr() - rtpHeadPos;
            pMb->rd_ptr(rd_ptr);

            (void)rtpPacket.GeneratePacket(pMb->rd_ptr(),rtpHeadPos);
            rtpPacket.SetTimeStamp(FrameInfo.timestamp);
            rtpPacket.SetSSRC(FrameInfo.ssrc);
            rtpPacket.SetSeqNum(m_usRtpSeq++);
            if(0 == ulEsLen)
            {
                rtpPacket.SetMarker(true);
            }

            if (FrameInfo.streamId== 0xC0)
            {
                rtpPacket.SetPayloadType(m_AudioPayload);
            }
            else if (FrameInfo.streamId== 0xE0)
            {
                rtpPacket.SetPayloadType(m_VideoPayload);
            }

            (void)CStreamMediaProcessorSet::Send(pMb);
            CMediaBlockBuffer::instance().freeMediaBlock(pMb);
        }
        else {
            pSendMb = CMediaBlockBuffer::instance().allocMediaBlock();
            if(NULL != pSendMb)
            {
                (void)rtpPacket.GeneratePacket(pSendMb->rd_ptr(),rtpHeadPos);
                rtpPacket.SetTimeStamp(FrameInfo.timestamp);
                rtpPacket.SetSSRC(FrameInfo.ssrc);
                rtpPacket.SetSeqNum(m_usRtpSeq++);
                rtpPacket.SetMarker(true);
                if (FrameInfo.streamId== 0xC0)
                {
                    rtpPacket.SetPayloadType(m_AudioPayload);
                }
                else if (FrameInfo.streamId== 0xE0)
                {
                    rtpPacket.SetPayloadType(m_VideoPayload);
                }
                pSendMb->wr_ptr(sizeof(RTP_FIXED_HEADER));
                pSendMb->copy(pMb->rd_ptr(),ulEsLen);

                (void)CStreamMediaProcessorSet::Send(pSendMb);
                CMediaBlockBuffer::instance().freeMediaBlock(pSendMb);

            }
            pMb->rd_ptr(ulEsLen);
            ulEsLen = 0;

        }
    }while(0 < rtpFrameList.size());

    FrameInfo.timestamp++;
    return;
}
void CStreamPsMediaProcessorSet::sendEsVideoFrame(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    ACE_Message_Block* pMb = rtpFrameList.front();

    if(PT_TYPE_MAX == m_VideoPayload)
    {
#ifdef __PRINT_MEDIA__
        SVS_LOG((SVS_LM_ERROR, "not wait the video ps with system head."));
#endif
        return;
    }

    if(PT_TYPE_H264 == FrameInfo.payload) {
        /* send h264 frame */
        if(FrameInfo.length <= pMb->length()) {
            sendH264EsSigleNalu(FrameInfo,rtpFrameList);
        }
        else
        {
            sendH264EsFuNale(FrameInfo,rtpFrameList);
        }
    }
    else if(PT_TYPE_H265 == FrameInfo.payload) {
        /* send h265 frame */
        if(FrameInfo.length <= pMb->length()) {
            sendH265EsSigleNalu(FrameInfo,rtpFrameList);
        }
        else
        {
            sendH265EsFuNale(FrameInfo,rtpFrameList);
        }
    }
    else {
        ProgramKnowFrame(FrameInfo,rtpFrameList);
    }
    return;
}
void CStreamPsMediaProcessorSet::sendH264EsSigleNalu(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    uint32_t ulEsLen = FrameInfo.length;
    ACE_Message_Block* pMb     = NULL;
    ACE_Message_Block* pSendMb = NULL;
    CRtpPacket rtpPacket;
    size_t rtpHeadPos = sizeof(RTP_FIXED_HEADER);

    pMb = rtpFrameList.front();

    if(ulEsLen == pMb->length()) {

        rtpFrameList.pop_front();

        char* rd_ptr = pMb->rd_ptr();

        if((0x00 == rd_ptr[0])&&(0x00 == rd_ptr[1])&&(0x00 == rd_ptr[2])&&(0x01 == rd_ptr[3]))
        {
            /* skip the start code */
            pMb->rd_ptr(H264_START_CODE);
        }
        rd_ptr = pMb->rd_ptr() - rtpHeadPos;
        pMb->rd_ptr(rd_ptr);

        (void)rtpPacket.GeneratePacket(pMb->rd_ptr(),rtpHeadPos);
        rtpPacket.SetTimeStamp(FrameInfo.timestamp);
        rtpPacket.SetSSRC(FrameInfo.ssrc);
        rtpPacket.SetSeqNum(m_usRtpSeq++);
        rtpPacket.SetPayloadType(m_VideoPayload);
        rtpPacket.SetMarker(true);

        (void)CStreamMediaProcessorSet::Send(pMb);
        CMediaBlockBuffer::instance().freeMediaBlock(pMb);

    }
    else if(ulEsLen < pMb->length()) {
        pSendMb = CMediaBlockBuffer::instance().allocMediaBlock();
        if(NULL != pSendMb)
        {
            (void)rtpPacket.GeneratePacket(pSendMb->rd_ptr(),rtpHeadPos);
            rtpPacket.SetTimeStamp(FrameInfo.timestamp);
            rtpPacket.SetSSRC(FrameInfo.ssrc);
            rtpPacket.SetSeqNum(m_usRtpSeq++);
            rtpPacket.SetMarker(true);
            rtpPacket.SetPayloadType(m_VideoPayload);
            char* rd_ptr = pMb->rd_ptr();
            if(FrameInfo.streamId== 0xE0)
            {
                if((0x00 == rd_ptr[0])&&(0x00 == rd_ptr[1])&&(0x00 == rd_ptr[2])&&(0x01 == rd_ptr[3]))
                {
                    /* skip the start code */
                    pMb->rd_ptr(H264_START_CODE);
                    ulEsLen -= H264_START_CODE;
                }
            }
            pSendMb->wr_ptr(sizeof(RTP_FIXED_HEADER));
            pSendMb->copy(pMb->rd_ptr(),ulEsLen);

            (void)CStreamMediaProcessorSet::Send(pSendMb);

            CMediaBlockBuffer::instance().freeMediaBlock(pSendMb);

        }
        pMb->rd_ptr(ulEsLen);
    }

    FrameInfo.timestamp++;
    return;
}
void CStreamPsMediaProcessorSet::sendH264EsFuNale(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    uint32_t ulEsLen = FrameInfo.length;
    ACE_Message_Block* pMb     = NULL;
    ACE_Message_Block* pSendMb = NULL;
    CRtpPacket rtpPacket;
    size_t rtpHeadPos = sizeof(RTP_FIXED_HEADER);
    H264_NALU_HEADER    nalu_hdr;
    FU_INDICATOR  *fu_ind   = NULL;
    FU_HEADER     *fu_hdr   = NULL;

    bool bStart = true;

    do
    {
        if(0 == ulEsLen){
            break;
        }

        pMb = rtpFrameList.front();

        if(ulEsLen >= pMb->length()) {

            ulEsLen -= pMb->length();
            rtpFrameList.pop_front();

            char* rd_ptr = pMb->rd_ptr();

            if((0x00 == rd_ptr[0])&&(0x00 == rd_ptr[1])&&(0x00 == rd_ptr[2])&&(0x01 == rd_ptr[3]))
            {
                /* skip the start code */
                pMb->rd_ptr(H264_START_CODE);
                /* save the nalu head */
                H264_NALU_HEADER* pNalu = (H264_NALU_HEADER*)(void*)(pMb->rd_ptr());
                nalu_hdr.TYPE = pNalu->TYPE;
                nalu_hdr.NRI  = pNalu->NRI;
                nalu_hdr.F    = pNalu->F;
                pMb->rd_ptr(1);

            }
            pMb->rd_ptr(-2);
            fu_ind = (FU_INDICATOR*)pMb->rd_ptr();
            fu_hdr = (FU_HEADER*)(pMb->rd_ptr()+1);

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

            rd_ptr = pMb->rd_ptr() - rtpHeadPos;
            pMb->rd_ptr(rd_ptr);

            (void)rtpPacket.GeneratePacket(pMb->rd_ptr(),rtpHeadPos);
            rtpPacket.SetTimeStamp(FrameInfo.timestamp);
            rtpPacket.SetSSRC(FrameInfo.ssrc);
            rtpPacket.SetSeqNum(m_usRtpSeq++);
            if(0 == ulEsLen)
            {
                fu_hdr->E=1;
                rtpPacket.SetMarker(true);
            }
            rtpPacket.SetPayloadType(m_VideoPayload);

            (void)CStreamMediaProcessorSet::Send(pMb);
            CMediaBlockBuffer::instance().freeMediaBlock(pMb);
        }
        else {
            /* last fu packet */
            pSendMb = CMediaBlockBuffer::instance().allocMediaBlock();
            if(NULL != pSendMb)
            {
                (void)rtpPacket.GeneratePacket(pSendMb->rd_ptr(),rtpHeadPos);
                rtpPacket.SetTimeStamp(FrameInfo.timestamp);
                rtpPacket.SetSSRC(FrameInfo.ssrc);
                rtpPacket.SetSeqNum(m_usRtpSeq++);
                rtpPacket.SetMarker(true);
                rtpPacket.SetPayloadType(m_VideoPayload);

                char* rd_ptr = pMb->rd_ptr();
                if((0x00 == rd_ptr[0])&&(0x00 == rd_ptr[1])&&(0x00 == rd_ptr[2])&&(0x01 == rd_ptr[3]))
                {
                    /* skip the start code*/
                    pMb->rd_ptr(H264_START_CODE);
                    /* save the nalu head */
                    H264_NALU_HEADER* pNalu = (H264_NALU_HEADER*)(void*)(pMb->rd_ptr());
                    nalu_hdr.TYPE = pNalu->TYPE;
                    nalu_hdr.NRI  = pNalu->NRI;
                    nalu_hdr.F    = pNalu->F;
                    pMb->rd_ptr(1);
                    ulEsLen -= 1;
                }

                pSendMb->wr_ptr(sizeof(RTP_FIXED_HEADER));
                fu_ind = (FU_INDICATOR*)pSendMb->wr_ptr();
                fu_hdr = (FU_HEADER*)(pSendMb->wr_ptr()+1);
                pSendMb->wr_ptr(2);

                fu_ind->F=nalu_hdr.F;
                fu_ind->NRI=nalu_hdr.NRI;
                fu_ind->TYPE=RTP_FUA;

                fu_hdr->S=0;
                fu_hdr->E=1;
                fu_hdr->R=0;
                fu_hdr->TYPE=nalu_hdr.TYPE;

                pSendMb->copy(pMb->rd_ptr(),ulEsLen);

                (void)CStreamMediaProcessorSet::Send(pSendMb);
                CMediaBlockBuffer::instance().freeMediaBlock(pSendMb);
            }
            pMb->rd_ptr(ulEsLen);
            ulEsLen = 0;

        }
    }while(0 < rtpFrameList.size());

    FrameInfo.timestamp++;
}
void CStreamPsMediaProcessorSet::sendH265EsSigleNalu(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    uint32_t ulEsLen = FrameInfo.length;
    ACE_Message_Block* pMb     = NULL;
    ACE_Message_Block* pSendMb = NULL;
    CRtpPacket rtpPacket;
    size_t rtpHeadPos = sizeof(RTP_FIXED_HEADER);
    size_t nalSize = sizeof(H265_NALU_HEADER);

    pMb = rtpFrameList.front();

    if(ulEsLen == pMb->length()) {

        rtpFrameList.pop_front();

        char* rd_ptr = pMb->rd_ptr();

        if((0x00 == rd_ptr[0])&&(0x00 == rd_ptr[1])&&(0x00 == rd_ptr[2])&&(0x01 == rd_ptr[3]))
        {
            /* skip the start code */
            pMb->rd_ptr(H265_START_CODE);
        }
        rd_ptr = pMb->rd_ptr() - rtpHeadPos - nalSize;
        pMb->rd_ptr(rd_ptr);

        /* set nalu info */
        rd_ptr = pMb->rd_ptr() + rtpHeadPos;
        *rd_ptr++ = 48 << 1;
        *rd_ptr   = 1;

        (void)rtpPacket.GeneratePacket(pMb->rd_ptr(),rtpHeadPos);
        rtpPacket.SetTimeStamp(FrameInfo.timestamp);
        rtpPacket.SetSSRC(FrameInfo.ssrc);
        rtpPacket.SetSeqNum(m_usRtpSeq++);
        rtpPacket.SetPayloadType(m_VideoPayload);
        rtpPacket.SetMarker(true);

        (void)CStreamMediaProcessorSet::Send(pMb);
        CMediaBlockBuffer::instance().freeMediaBlock(pMb);

    }
    else if(ulEsLen < pMb->length()) {
        pSendMb = CMediaBlockBuffer::instance().allocMediaBlock();
        if(NULL != pSendMb)
        {
            (void)rtpPacket.GeneratePacket(pSendMb->rd_ptr(),rtpHeadPos);
            rtpPacket.SetTimeStamp(FrameInfo.timestamp);
            rtpPacket.SetSSRC(FrameInfo.ssrc);
            rtpPacket.SetSeqNum(m_usRtpSeq++);
            rtpPacket.SetMarker(true);
            rtpPacket.SetPayloadType(m_VideoPayload);
            char* rd_ptr = pMb->rd_ptr();
            if(FrameInfo.streamId== 0xE0)
            {
                if((0x00 == rd_ptr[0])&&(0x00 == rd_ptr[1])&&(0x00 == rd_ptr[2])&&(0x01 == rd_ptr[3]))
                {
                    /* skip the start code */
                    pMb->rd_ptr(H265_START_CODE);
                    ulEsLen -= H265_START_CODE;
                }
            }
            pSendMb->wr_ptr(sizeof(RTP_FIXED_HEADER));
            /* set nalu info */
            char* wd_ptr = pMb->wr_ptr();
            *wd_ptr++ = 48 << 1;
            *wd_ptr   = 1;
            pSendMb->wr_ptr(nalSize);

            pSendMb->copy(pMb->rd_ptr(),ulEsLen);

            (void)CStreamMediaProcessorSet::Send(pSendMb);

            CMediaBlockBuffer::instance().freeMediaBlock(pSendMb);

        }
        pMb->rd_ptr(ulEsLen);
    }

    FrameInfo.timestamp++;
    return;
}
void CStreamPsMediaProcessorSet::sendH265EsFuNale(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    uint32_t ulEsLen = FrameInfo.length;
    ACE_Message_Block* pMb     = NULL;
    ACE_Message_Block* pSendMb = NULL;
    CRtpPacket rtpPacket;
    size_t rtpHeadPos = sizeof(RTP_FIXED_HEADER);
    size_t nalSize = sizeof(H265_NALU_HEADER);

    uint8_t    nal_type = 0;

    bool bStart = true;

    do
    {
        if(0 == ulEsLen){
            break;
        }

        pMb = rtpFrameList.front();

        if(ulEsLen >= pMb->length()) {

            ulEsLen -= pMb->length();
            rtpFrameList.pop_front();

            char* rd_ptr = pMb->rd_ptr();

            if((0x00 == rd_ptr[0])&&(0x00 == rd_ptr[1])&&(0x00 == rd_ptr[2])&&(0x01 == rd_ptr[3]))
            {
                /* skip the start code */
                pMb->rd_ptr(H265_START_CODE);
                /* save the nalu head */
                uint8_t value = *(uint8_t*)(void*)pMb->rd_ptr();
                nal_type = (value >> 1) & 0x3F;
                pMb->rd_ptr(nalSize);

            }
            pMb->rd_ptr(-3);
            /*
             * create the HEVC payload header and transmit the buffer as fragmentation units (FU)
             *
             *    0                   1
             *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
             *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *   |F|   Type    |  LayerId  | TID |
             *   +-------------+-----------------+
             *
             *      F       = 0
             *      Type    = 49 (fragmentation unit (FU))
             *      LayerId = 0
             *      TID     = 1
             */
            rd_ptr = pMb->rd_ptr();
            rd_ptr[0] = 49 << 1;
            rd_ptr[1] = 1;

            /*
             *     create the FU header
             *
             *     0 1 2 3 4 5 6 7
             *    +-+-+-+-+-+-+-+-+
             *    |S|E|  FuType   |
             *    +---------------+
             *
             *       S       = variable
             *       E       = variable
             *       FuType  = NAL unit type
             */
            rd_ptr[2]  = nal_type;
            if(bStart) {
                /* set the S bit: mark as start fragment */
                rd_ptr[2] |= 1 << 7;
                bStart = false;
            }


            rd_ptr = pMb->rd_ptr() - rtpHeadPos;
            pMb->rd_ptr(rd_ptr);

            (void)rtpPacket.GeneratePacket(pMb->rd_ptr(),rtpHeadPos);
            rtpPacket.SetTimeStamp(FrameInfo.timestamp);
            rtpPacket.SetSSRC(FrameInfo.ssrc);
            rtpPacket.SetSeqNum(m_usRtpSeq++);
            if(0 == ulEsLen)
            {
                /* set the S bit: mark as end fragment */
                rd_ptr[2] |= 1 << 6;
                rtpPacket.SetMarker(true);
            }
            rtpPacket.SetPayloadType(m_VideoPayload);

            (void)CStreamMediaProcessorSet::Send(pMb);
            CMediaBlockBuffer::instance().freeMediaBlock(pMb);
        }
        else {
            /* last fu packet */
            pSendMb = CMediaBlockBuffer::instance().allocMediaBlock();
            if(NULL != pSendMb)
            {
                (void)rtpPacket.GeneratePacket(pSendMb->rd_ptr(),rtpHeadPos);
                rtpPacket.SetTimeStamp(FrameInfo.timestamp);
                rtpPacket.SetSSRC(FrameInfo.ssrc);
                rtpPacket.SetSeqNum(m_usRtpSeq++);
                rtpPacket.SetMarker(true);
                rtpPacket.SetPayloadType(m_VideoPayload);

                char* rd_ptr = pMb->rd_ptr();
                if((0x00 == rd_ptr[0])&&(0x00 == rd_ptr[1])&&(0x00 == rd_ptr[2])&&(0x01 == rd_ptr[3]))
                {
                     /* skip the start code */
                    pMb->rd_ptr(H265_START_CODE);
                    /* save the nalu head */
                    uint8_t value = *(uint8_t*)(void*)pMb->rd_ptr();
                    nal_type = (value >> 1) & 0x3F;
                    pMb->rd_ptr(nalSize);
                    ulEsLen -= nalSize;
                }

                pSendMb->wr_ptr(sizeof(RTP_FIXED_HEADER));
                rd_ptr = pSendMb->wr_ptr();
                pSendMb->wr_ptr(3);

                /*
                 * create the HEVC payload header and transmit the buffer as fragmentation units (FU)
                 *
                 *    0                   1
                 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
                 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 *   |F|   Type    |  LayerId  | TID |
                 *   +-------------+-----------------+
                 *
                 *      F       = 0
                 *      Type    = 49 (fragmentation unit (FU))
                 *      LayerId = 0
                 *      TID     = 1
                 */
                rd_ptr = pMb->rd_ptr();
                rd_ptr[0] = 49 << 1;
                rd_ptr[1] = 1;

                /*
                 *     create the FU header
                 *
                 *     0 1 2 3 4 5 6 7
                 *    +-+-+-+-+-+-+-+-+
                 *    |S|E|  FuType   |
                 *    +---------------+
                 *
                 *       S       = variable
                 *       E       = variable
                 *       FuType  = NAL unit type
                 */
                rd_ptr[2]  = nal_type;
                /* set the S bit: mark as end fragment */
                rd_ptr[2] |= 1 << 6;

                pSendMb->copy(pMb->rd_ptr(),ulEsLen);

                (void)CStreamMediaProcessorSet::Send(pSendMb);
                CMediaBlockBuffer::instance().freeMediaBlock(pSendMb);
            }
            pMb->rd_ptr(ulEsLen);
            ulEsLen = 0;

        }
    }while(0 < rtpFrameList.size());

    FrameInfo.timestamp++;
}



void CStreamPsMediaProcessorSet::skipunKnowFrame(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList)
{
    uint32_t ulEsLen = FrameInfo.length;
    ACE_Message_Block* pMb = NULL;
#ifdef __PRINT_MEDIA__
    SVS_LOG((SVS_LM_ERROR, "CStreamPsMediaProcessorSet::skipunKnowFrame, total EsLen:[%d],streamID:[0x%02x]",
                                   FrameInfo.length,FrameInfo.streamId));
#endif

    do
    {
        if(0 == ulEsLen){
            break;
        }

        pMb = rtpFrameList.front();

        if(ulEsLen >= pMb->length()) {
            ulEsLen -= pMb->length();
            rtpFrameList.pop_front();
            CMediaBlockBuffer::instance().freeMediaBlock(pMb);
        }
        else {
            pMb->rd_ptr(ulEsLen);
            ulEsLen = 0;
        }
    }while(0 < rtpFrameList.size());
}



