#include "svs_log_msg.h"
#include "svs_adapter_mpeg_ps_enc.h"
#include "svs_adapter_rtp_packet.h"

#define  MIN_NALU_SIZE      5
/*************CRC�㷨������PSM��CRC����***************/
static uint32_t uVal = 0;
static uint32_t upoly = 0;
static bool bCRCInit = false;
void CRCInit()
{
    uVal = 0xffffffff;
    upoly = 0x02608edb;  //0x04c11db7;
    bCRCInit = true;
}
void CRCClose()
{
    uVal = 0;
    upoly = 0;
    bCRCInit = false;
}
void CRCInputBit(uint8_t byin)
{
    uint32_t utemp = 0;
    //param is uint8_t value, it can't over 2

    utemp = (((uVal & 0x80000000) >> 31) ^ byin);

    if( utemp == 1)
    {
        uVal ^= upoly;
    }

    uVal <<= 1;
    uVal |= utemp;
}
uint32_t CRCGet(const uint8_t* byValue, int32_t iLength)
{
    uint8_t i = 0, j = 0;

    if(!bCRCInit)
    {
        return 0xffffffff;
    }
    for(i = 0; i < iLength; i++)
    {
        for(j = 0; j < 8; j++)
        {
            CRCInputBit(((byValue[i] << j) & 0x80) >> 7);
        }
    }

    return uVal;
}
/****************************/

CMpegPsEnc::CMpegPsEnc()
{
    m_pPsStreamHandle           = NULL;
    m_pUserData                 = NULL;

    m_pPsBuffer                 = NULL;
    m_unBufferSize              = 0;
    m_pWritePos                 = NULL;

    m_ullScr                    = INVALID_TIMESTAMP;
    m_ullVideoPts               = INVALID_TIMESTAMP;
    m_ullAudioPts               = INVALID_TIMESTAMP;
    m_unMuxRate                 = 0;

    m_unVideoTimestamp          = 0;
    m_unAudioTimestamp          = 0;
    m_unEsStreamSize            = 0;
    m_LastCalcTime              = 0;
    m_isFirstPES =  true;
}


CMpegPsEnc::~CMpegPsEnc()
{
    try
    {
        release();
    }
    catch (...)
    {
    }

    m_pPsBuffer = NULL;
    m_pPsStreamHandle = NULL;
    m_pUserData = NULL;
    m_pWritePos = NULL;
}


// ��װ����ʼ��
int32_t CMpegPsEnc::init(CMpegPsEncHandle* pHandle, void* pUserData)
{
    if (NULL == pHandle)
    {
        return -1;
    }

    m_pPsStreamHandle   = pHandle;
    m_pUserData         = pUserData;

    // ������
    try
    {
        m_pPsBuffer     = new uint8_t[DEFAULT_PS_CACHE];
        m_pWritePos     = m_pPsBuffer;
        m_unBufferSize  = DEFAULT_PS_CACHE;
    }
    catch (...)
    {

    }

    // SCR��PTS��DTS
    m_ullScr        = 0;
    m_ullVideoPts   = m_ullScr;
    m_ullAudioPts   = m_ullScr;
    m_unMuxRate     = (1024 * 1024) / 50;        // ��ʼĬ������Ϊ1M

    SVS_LOG((SVS_LM_INFO,"success to init mpeg ps encapsulation, scr=%Q.", m_ullScr));
    return 0;
}

void CMpegPsEnc::release()
{
    // �������������PS����û�д����꣬���ȴ���
    if (m_pWritePos != m_pPsBuffer)
    {
        if (NULL != m_pPsStreamHandle)
        {
            m_pPsStreamHandle->handlePsStream((char*)m_pPsBuffer,
                                              getWritePos(),
                                              m_unVideoTimestamp,
                                              m_pUserData,true);

            // ����дָ��
            m_pWritePos = m_pPsBuffer;
        }
    }

    if (NULL != m_pPsBuffer)
    {
        delete[] m_pPsBuffer;
        m_pPsBuffer = NULL;
        m_pWritePos = NULL;
    }

    SVS_LOG((SVS_LM_INFO,"success to release mpeg ps encapsulation."));
}

uint32_t CMpegPsEnc::getWritePos()const
{
    if (NULL == m_pWritePos)
    {
        return 0;
    }

    if (NULL == m_pPsBuffer)
    {
        return 0;
    }

    uint32_t unPos = (uint32_t)(m_pWritePos - m_pPsBuffer);
    return unPos;
}

// д��һ����Ƶ������
int32_t CMpegPsEnc::writeVideoEsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp)
{
    if (unDataSize < MIN_NALU_SIZE)
    {
        return -1;
    }

    // ����ES��ʱ�������PTS
    if (0 == m_unVideoTimestamp)
    {
        m_unVideoTimestamp = unTimestamp;
        m_ullVideoPts      = unTimestamp;
    }
    else
    {
        uint64_t ullTick = 0;
        if (unTimestamp >= m_unVideoTimestamp)
        {
            ullTick = (uint64_t)(unTimestamp - m_unVideoTimestamp);
        }
        else
        {
            // ʱ�����ת
            ullTick = (uint64_t)((0xffffffffL - m_unVideoTimestamp) + unTimestamp); //lint !e776
        }

        m_ullVideoPts += ullTick;
        m_unVideoTimestamp = unTimestamp;
    }

    // SCR����PTS��ǰ�ƶ�, Ϊ��ֹ���Ӳ�������ͬһʱ�����SCRȡ����PTS����Сֵ
    m_ullScr = m_ullVideoPts > m_ullAudioPts ? m_ullVideoPts : m_ullAudioPts;

    char* pDataCurrent = (char*)pInData;
    uint32_t unLenPerRtpPacket = MAX_ENCAP_RTP_SIZE - sizeof(RTP_FIXED_HEADER);
    uint32_t unLeftDataSize = unDataSize;
    calcEsBitrate(unDataSize);

    m_isFirstPES =  true;
    uint32_t  uSeqNum = 1;
    //��ȥ13�ǿ��ǰ���dts��pts����
    while(0xffff-13 < unLeftDataSize)
    {
         uint32_t unSendDataNum = (0xffff - 13)/unLenPerRtpPacket;
         uint32_t unSendDataSize   = unSendDataNum*unLenPerRtpPacket;
         uSeqNum += unSendDataNum;

        if (0 != writeESPacket(pDataCurrent, unSendDataSize, ES_TYPE_VIDEO))
        {
            SVS_LOG((SVS_LM_WARNING,"fail to write video es frame, size[%u], timestamp[%u].",
                    unDataSize, unTimestamp));
            return -1;
        }
        pDataCurrent += unSendDataSize;
        unLeftDataSize -= unSendDataSize;

        if (NULL != m_pPsStreamHandle)
        {
            m_pPsStreamHandle->handlePsStream((char*)m_pPsBuffer,
                                           getWritePos(),
                                           m_unVideoTimestamp,
                                           m_pUserData,
                                           false);

        // ����дָ��
            m_pWritePos = m_pPsBuffer;
        }
    }

    if (0 != writeESPacket(pDataCurrent, unLeftDataSize, ES_TYPE_VIDEO))
    {
        SVS_LOG((SVS_LM_WARNING,"fail to write video es frame, size[%u], timestamp[%u].",
                    unDataSize, unTimestamp));
        return -1;
    }

    if (NULL != m_pPsStreamHandle)
    {
        m_pPsStreamHandle->handlePsStream((char*)m_pPsBuffer,
                                           getWritePos(),
                                           m_unVideoTimestamp,
                                           m_pUserData,
                                           true);

        // ����дָ��
        m_pWritePos = m_pPsBuffer;
    }


    SVS_LOG((SVS_LM_INFO,"success to write video es frame, size[%u], timestamp[%u].",
            unDataSize, unTimestamp));
    return 0;
}

// д��һ����Ƶ������
int32_t CMpegPsEnc::writeAudioEsStream(const char* pInData, uint32_t unDataSize, uint32_t unTimestamp)
{
    // ����ES��ʱ�������PTS
    if (0 == m_unAudioTimestamp)
    {
        m_unAudioTimestamp = unTimestamp;
        m_ullAudioPts      = unTimestamp;
    }
    else
    {
        uint64_t ullTick = 0;
        if (unTimestamp >= m_unAudioTimestamp)
        {
            ullTick = (uint64_t)(unTimestamp - m_unAudioTimestamp);
        }
        else
        {
            // ʱ�����ת
            ullTick = (uint64_t)((0xffffffffL - m_unAudioTimestamp) + unTimestamp);//lint !e776
        }

        m_ullAudioPts += ullTick;
        m_unAudioTimestamp = unTimestamp;
    }

     calcEsBitrate(unDataSize);

    // SCR����PTS��ǰ�ƶ�, Ϊ��ֹ���Ӳ�������ͬһʱ�����SCRȡ����PTS����Сֵ
    m_ullScr = m_ullVideoPts > m_ullAudioPts ? m_ullVideoPts : m_ullAudioPts;

    m_isFirstPES =  true;

    if (0 != writeESPacket(pInData, unDataSize, ES_TYPE_AUDIO))
    {
        SVS_LOG((SVS_LM_WARNING,"fail to write audio es frame, size[%u], timestamp[%u].",
            unDataSize, unTimestamp));
        return -1;
    }

    if (NULL != m_pPsStreamHandle)
    {
        m_pPsStreamHandle->handlePsStream((char*)m_pPsBuffer,
                                           getWritePos(),
                                           m_unAudioTimestamp,
                                           m_pUserData,
                                           true);

        // ����дָ��
        m_pWritePos = m_pPsBuffer;
    }

    SVS_LOG((SVS_LM_INFO,"success to write audio es frame, size[%u], timestamp[%u].",
        unDataSize, unTimestamp));
    return 0;
}

/********************�ڲ��ӿ�ʵ��********************/
// д��һ��ES��
int32_t CMpegPsEnc::writeESPacket(const char* pInData, uint32_t unDataSize, ES_TYPE_E enEsType)
{
    if ((NULL == pInData) || (0 == unDataSize))
    {
        return -1;
    }

    if (m_unBufferSize - MAX_PS_HEADER_LEN < unDataSize)
    {
        // ����Ŀǰ��ʵ��������һ��H264֡һ��PS����Ӧ�ò������������
        SVS_LOG((SVS_LM_INFO,"fail to write es packet, frame size[%u] larger than cache size[%u].",
            unDataSize, m_unBufferSize));
        return -1;
    }

    // ��������
    //calcEsBitrate(unDataSize);

    // ���ʣ�໺���������ˣ���ص�����
    if (m_unBufferSize - getWritePos() < unDataSize + MAX_PS_HEADER_LEN)
    {
        // ����Ŀǰ��ʵ��������һ��H264֡һ��PS����Ӧ�ò���Ҫ�����������ȫ����Ƶ��
        if (NULL != m_pPsStreamHandle)
        {
            m_pPsStreamHandle->handlePsStream((char*)m_pPsBuffer,
                                              getWritePos(),
                                              m_unAudioTimestamp,
                                              m_pUserData,
                                              true);

            // ����дָ��
            m_pWritePos = m_pPsBuffer;
        }
    }

    if (m_pWritePos == m_pPsBuffer && m_isFirstPES == true)
    {
        // �������������ݣ�����Ҫд��PSͷ
        if (0 != writePSHeader())
        {
            SVS_LOG((SVS_LM_WARNING,"fail to write es packet, wirte ps header failed. frame size[%u].",
                unDataSize));
            return -1;
        }

        // д��ϵͳͷ
        if (0 != writeSysHeader())
        {
            SVS_LOG((SVS_LM_WARNING,"fail to write es packet, wirte system header failed. frame size[%u].",
                unDataSize));
            return -1;
        }

        // д��PSM
        if (0 != writePsmPacket())
        {
            SVS_LOG((SVS_LM_WARNING,"fail to write es packet, wirte PSM failed. frame size[%u].",
                unDataSize));
            return -1;
        }

        m_isFirstPES = false;
    }

    // ����ES���ͣ��ֱ�ȡstream_id��pts��dts
    uint8_t ucStreamId  = VIDEO_STREAM_ID;
    uint64_t ullPts = m_ullVideoPts;
    uint64_t ullDts = m_ullVideoPts;
    if (ES_TYPE_AUDIO == enEsType)
    {
        ucStreamId  = AUDIO_STREAM_ID;
        ullPts      = m_ullAudioPts;
        ullDts      = INVALID_TIMESTAMP;
    }


        // ǰ���Ѿ����˻�������С���ƣ�������Է���дPES����
    if (0 != writePESHeader(unDataSize, ucStreamId, ullPts, ullDts))
    {
        SVS_LOG((SVS_LM_WARNING,"fail to write es packet, wirte pes header failed. frame size[%u].",
            unDataSize));
        return -1;
    }


    // д��ES����
    memcpy(m_pWritePos, pInData, unDataSize);
    m_pWritePos += unDataSize;

    return 0;
}

// д��PS��ͷ
/*lint -e701*/
int32_t CMpegPsEnc::writePSHeader()
{
    if ((NULL == m_pWritePos)
        || (getWritePos() >= m_unBufferSize) )
    {
        return -1;
    }

    // pack_start_code = 0x00 00 01 BA
    *m_pWritePos++ = 0x00;
    *m_pWritePos++ = 0x00;
    *m_pWritePos++ = 0x01;
    *m_pWritePos++ = 0xBA;

    // 33λ��SCR_base
    // '01'(2b) + scr_base[32..30](3b) + maker(1b) + scr_base[29..28](2b)
    *m_pWritePos++ = 0x40 | (uint8_t)(GET_BITS(m_ullScr, 30, 32) << 3)
                          | 0x04 | (uint8_t)GET_BITS(m_ullScr, 28, 29);

    // scr_base[27..20] (8b)
    *m_pWritePos++ = (uint8_t)GET_BITS(m_ullScr, 20, 27);

    // scr_base[19..15] (5b) + maker(1b) + scr_base[14..13](2b)
    *m_pWritePos++ = ((uint8_t)GET_BITS(m_ullScr, 15, 19) << 3 ) | 0x04
                    | (uint8_t)GET_BITS(m_ullScr, 13, 14);

    // scr_base[12..5] (8b)
    *m_pWritePos++ = (uint8_t)GET_BITS(m_ullScr, 5, 12);

    // SCR_EXT����ȡ0, ֱ�����õ���������
    // scr_base[4..0] (5b) + maker(1b) + scr_ext[9..7] (2b)
    *m_pWritePos++ = (uint8_t)(((uint8_t)GET_BITS(m_ullScr, 0, 6) << 3) | 0x04);

    // src_ext[6..0] (7b) + maker(1b)
    *m_pWritePos++ = 0x01;

    // 22λ��program_mux_rate, ������ȡ0
    // mux_rate[21..14]
    *m_pWritePos++ = (uint8_t)GET_BITS(m_unMuxRate, 14, 21);

    // mux_rate[13..6]
    *m_pWritePos++ = (uint8_t)GET_BITS(m_unMuxRate, 6, 13);

    // mux_rate[5..0] + maker + maker
    *m_pWritePos++ = (uint8_t)(((uint8_t)GET_BITS(m_unMuxRate, 0, 6) << 2) | 0x03);

    // �����ֽ�(5b) + pack_stuffing_length(3b)
    *m_pWritePos++ = 0xf8;

    return 0;
}

// ��PS��ͷ��д��ϵͳͷ
int32_t CMpegPsEnc::writeSysHeader()
{
    if ( (NULL == m_pWritePos)
        || (getWritePos() >= m_unBufferSize) )
    {
        return -1;
    }

    // system_header_start_code = 0x000001BB
    *m_pWritePos++ = 0x00;
    *m_pWritePos++ = 0x00;
    *m_pWritePos++ = 0x01;
    *m_pWritePos++ = 0xBB;

    // header_length, ���ֶκ����ϵͳͷ����

    *m_pWritePos++ = 0x0;
    *m_pWritePos++ = 0x06;      // System header�̶���С����6�ֽ�

    // maker(1b) + rate_bond(22b) + maker(1b)
    // rate_bond = mux_rate
    *m_pWritePos++ = 0x80 | (uint8_t)GET_BITS(m_unMuxRate, 15, 21);
    *m_pWritePos++ = (uint8_t)GET_BITS(m_unMuxRate, 7, 14);
    *m_pWritePos++ = ( (uint8_t)GET_BITS(m_unMuxRate, 0, 6) << 1 ) | 0x1;

    // audio_bond(6b) + fixed_flag(1b) + CSPS_flag(1b)
    // Ĭ�ϴ�����Ƶ��
    *m_pWritePos++ = 0x1 << 2;

    // system_audio_lock_flag(1b) + system_video_lock_flag(1b) + maker(1b) + video_bond(5b)
    // Ĭ�ϴ�����Ƶ��
    *m_pWritePos++ = 0xe1;

    // packet_rate_restriction_flag(1b) + reserved(7b)
    *m_pWritePos++ = 0x7f;

#if 0
    // ������Ƶ��ID��Ϣ
    // TODO ��ǰ����Ƶ����ID��д��
    // stream_id(8b) + '11'(2b) + P_STD_buffer_bond_scale(1b) + P_STD_buffer_size_bond(13b)
    // ��Ƶ��P_STD_buffer_bond_scale����Ϊ'1'����P_STD_buffer_size_bond�ĵ�λΪ1024byte
    *m_pWritePos++ = VIDEO_STREAM_ID;

    uint16_t usSizeBond = (uint16_t)(m_unBufferSize / 1024);
    *m_pWritePos++ = 0xe0 | (uint8_t)GET_BITS(usSizeBond, 8, 13);
    *m_pWritePos++ = (uint8_t)GET_BITS(usSizeBond, 0, 7);

    // ������Ƶ��ID��Ϣ
    // TODO ��ǰ����Ƶ����ID��д��
    // stream_id(8b) + '11'(2b) + P_STD_buffer_bond_scale(1b) + P_STD_buffer_size_bond(13b)
    // ��Ƶ��P_STD_buffer_bond_scale����Ϊ'0'����P_STD_buffer_size_bond�ĵ�λΪ128byte
    *m_pWritePos++ = AUDIO_STREAM_ID;

    usSizeBond = (uint16_t)(m_unBufferSize / 128);
    *m_pWritePos++ = 0xc0 | (uint8_t)GET_BITS(usSizeBond, 8, 13);
    *m_pWritePos++ = (uint8_t)GET_BITS(usSizeBond, 0, 7);
#endif

    return 0;
}

int32_t CMpegPsEnc::writePsmPacket()
{
    if ( (NULL == m_pWritePos)
       ||(getWritePos() >= m_unBufferSize) )
    {
        return -1;
    }

    // ��¼��ǰλ�ã�����CRC����
    uint8_t *pStartPos = m_pWritePos;

    // PSM_start_code = 0x000001BC
    *m_pWritePos++ = 0x00;
    *m_pWritePos++ = 0x00;
    *m_pWritePos++ = 0x01;
    *m_pWritePos++ = 0xBC;

    // psm_length(16b)
    // ���ֶκ�����PSM�ĳ��ȣ���Ϊ��Ƶ��Ĭ�϶��У����Բ�ȡ�̶�����(18�ֽ�)
    *m_pWritePos++ = 0x0;
    *m_pWritePos++ = 0x12;

    // current_next_indicator(1b) + reserved(2b) + PSM_version(5b)
    // ����װ���У�PSM�������޸ģ�����PSM_versionд��
    static uint8_t   cVersion = 0;
    *m_pWritePos++ = 0xe0 | cVersion;
    cVersion = (cVersion + 1) % 0x1f;

    // reserved(7b) + maker(1b)
    *m_pWritePos++ = 0xff;

    // program_stream_info_length(16b)
    // ����Ҫ������Ϣ������ȡ0
    *m_pWritePos++ = 0x0;
    *m_pWritePos++ = 0x0;

    // elementary_stream_map_length(16b)
    // Ĭ������Ƶ�����У����Գ��ȿ���д��(8���ֽ�)
    *m_pWritePos++ = 0x0;
    *m_pWritePos++ = 0x8;

    // ��Ƶ��Ϣ
    // stream_type(8b) + elementary_stream_id(8b) + elementary_stream_info_length(16b)
    // �����ͺ���ID��д����û�ж��������Ϣ������Lengthȡ0
    *m_pWritePos++ = VIDEO_STREAM_TYPE_H264;
    *m_pWritePos++ = VIDEO_STREAM_ID;
    *m_pWritePos++ = 0x0;
    *m_pWritePos++ = 0x0;

    // ��Ƶ��Ϣ
    // stream_type(8b) + elementary_stream_id(8b) + elementary_stream_info_length(16b)
    // �����ͺ���ID��д����û�ж��������Ϣ������Lengthȡ0
    *m_pWritePos++ = AUDIO_STREAM_TYPE_G711;
    *m_pWritePos++ = AUDIO_STREAM_ID;
    *m_pWritePos++ = 0x0;
    *m_pWritePos++ = 0x0;

    // CRC_32(32b)
    CRCInit();
    uint32_t unCrc = CRCGet(pStartPos, (int32_t)(m_pWritePos - pStartPos));
    uint8_t* pCrc = (uint8_t*)&unCrc;

    *m_pWritePos++ = pCrc[0];
    *m_pWritePos++ = pCrc[1];
    *m_pWritePos++ = pCrc[2];
    *m_pWritePos++ = pCrc[3];

    return 0;
}

// д��һ��PES��ͷ
int32_t CMpegPsEnc::writePESHeader(uint32_t unDataSize, uint8_t ucStreamId,
                               uint64_t ullPts, uint64_t ullDts)
{
    if ( (NULL == m_pWritePos)
        || (getWritePos() >= m_unBufferSize) )
    {
        return -1;
    }

    // �����������PTS��������DTS�����
    if ((INVALID_TIMESTAMP == ullPts)
        && (INVALID_TIMESTAMP != ullDts))
    {
        return -1;
    }

    // start code = 0x00 00 01
    *m_pWritePos++ = 0x00;
    *m_pWritePos++ = 0x00;
    *m_pWritePos++ = 0x01;

    // stream id(8b)
    *m_pWritePos++ = ucStreamId;

    // �ȸ���PTS��DTS��ES���ݳ��ȣ���������PES������PES��չ�ֶγ��Ⱥ�PTS_DTS_flag��־
    uint16_t usLen = 3;               //PES ��ͷ�̶���������Ϊ3�ֽ�
    uint8_t  extLen = 0;
    uint8_t  tsFlag = 0;              // PTS/DTS��־
    if (INVALID_TIMESTAMP != ullPts)
    {
        usLen  += 5;                        // PTS��DTS��ռ��5Byte
        extLen += 5;
        tsFlag |= 0x02;
    }
    if (INVALID_TIMESTAMP != ullDts)
    {
        usLen  += 5;
        extLen += 5;
        tsFlag |= 0x01;
    }
    usLen += (uint16_t)unDataSize;    // ����PESͷ��־��ȡ0��������Ҫ����ES������

    // PES_packet_length(16b)
    *m_pWritePos++ = (uint8_t)GET_BITS(usLen, 8, 15);
    *m_pWritePos++ = (uint8_t)GET_BITS(usLen, 0, 7);

    // '10'(2b) + PES_scrambing_control(2b) + PES_priority(1b)
    // + data_alignment_indicator(1b) + copyright(1b) + original_or_copy(1b)
    // ��������ȫ��ȡ0
    *m_pWritePos++ = 0x80;

    // PTS_DTS_flag(2b) + ESCR_flag(1b) + ES_rate_flag(1b) + DSM_trick_mode_flag(1b)
    // + additional_copy_flag(1b) + PES_CRC_flag(1b) + PES_extension_flag(1b)
    // ��PTS_DTS_flag�⣬������־ȫȡ0
    *m_pWritePos++ = (uint8_t)(tsFlag << 6);

    // PES_header_data_length(8b)
    *m_pWritePos++ = extLen;

    // д��PTS
    if (INVALID_TIMESTAMP != ullPts)
    {
        // '0011'(4b) + PTS[32..30] (3b) + maker(1b)
        *m_pWritePos++ = (uint8_t)((tsFlag << 4) | ((uint8_t)GET_BITS(ullPts, 30, 32) << 1) | 0x1);

        // PTS[29..15](15b) + maker(1b)
        *m_pWritePos++ = (uint8_t)GET_BITS(ullPts, 22, 29);
        *m_pWritePos++ = ((uint8_t)GET_BITS(ullPts, 15, 21) << 1) | 0x1;

        // PTS[14..0](15b) + maker(1b)
        *m_pWritePos++ = (uint8_t)GET_BITS(ullPts, 7, 14);
        *m_pWritePos++ = ((uint8_t)GET_BITS(ullPts, 0, 6) << 1) | 0x1;
    }
    else
    {
        // ��PTS��û�У�DTSҲ�����У�ֱ�ӷ���
        return 0;
    }

    // д��DTS
    if (INVALID_TIMESTAMP != ullDts)
    {
        // '0001'(4b) + DTS[32..30] (3b) + maker(1b)
        *m_pWritePos++ = (0x1 << 4) | ((uint8_t)GET_BITS(ullDts, 30, 32) << 1) | 0x1;

        // DTS[29..15](15b) + maker(1b)
        *m_pWritePos++ = (uint8_t)GET_BITS(ullDts, 22, 29);
        *m_pWritePos++ = ((uint8_t)GET_BITS(ullDts, 15, 21) << 1) | 0x1;

        // DTS[14..0](15b) + maker(1b)
        *m_pWritePos++ = (uint8_t)GET_BITS(ullDts, 7, 14);
        *m_pWritePos++ = ((uint8_t)GET_BITS(ullDts, 0, 6) << 1) | 0x1;
    }

    return 0;
}
/*lint +e701*/

// ����ES������
void CMpegPsEnc::calcEsBitrate(uint32_t unDataSize)
{
    m_unEsStreamSize += unDataSize;

    if (0 == m_LastCalcTime)
    {
        m_LastCalcTime = SVS_GetSecondTime();
        return;
    }

    uint32_t curTick = SVS_GetSecondTime();
    if (curTick > m_LastCalcTime)
    {
        //m_unMuxRate = m_unEsStreamSize / (curTick - m_LastCalcTime) * 8 / 50;
        m_unMuxRate = m_unEsStreamSize / (curTick - m_LastCalcTime);
        m_unMuxRate /= (8 * 50);

        m_LastCalcTime = curTick;
        SVS_LOG((SVS_LM_INFO,"mpeg ps encapsulation calc mutex rate: %u", m_unMuxRate));
    }

    return;
}
