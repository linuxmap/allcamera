#ifndef __CMDUMEDIAPROCESSOR_H__
#define __CMDUMEDIAPROCESSOR_H__



#define FRAME_CACHE_NUM   5
#define FRAME_ALLOC_NUM   (FRAME_CACHE_NUM + 10)

#define SAVE_PACKET_PRINT_NUM  100000

#define PS_FRAME_SIZE_MAX      (1024*1024)

#define SVS_FRAME_SIZE_MAX     (1024*1024)

#define ES_PACKET_SIZE_MAX     (10000)


#define MAX_SVS_FRAME_COUNT    10
#define DEFAULT_FRAME_COUNT    0
#define MAX_SVS_FRAME_CACHED   8


#define SVS_MAX_PACK_COUNT    500

#define SVS_FRAME_ID_REVERSED 0x00000FFF

#include  <list>
#include  <map>
#include  "svs_data_stat.h"
#include "svs_adapter_rtp_frame_organizer.h"
#include "svs_es_rtp_encap.h"
#include "svs_adapter_ps_rtp_encap.h"
#include "svs_adapter_rtp_packer.h"
#include "svs_adapter_ts_seg_encap.h"
#include "svs_adapter_rtp_packet.h"




typedef struct
{
    uint8_t TypeSpecific;
    uint8_t FragmentOffset[3];
    uint8_t Type;
    uint8_t Q;
    uint8_t Width;
    uint8_t Height;

    uint32_t getFragmentOffset()
    {
        uint32_t unOffset;
        unOffset = (FragmentOffset[0]<<16) + (FragmentOffset[1]<<8) + FragmentOffset[2];
        return unOffset;
    }

    void setFragmentOffset(uint32_t unOffset)
    {
        FragmentOffset[2] = unOffset&0xFF;
        FragmentOffset[1] = (unOffset>>8)&0xFF;
        FragmentOffset[0] = (unOffset>>16)&0xFF;
    }

    bool hasRestartMarker()
    {
        if ((Type >= 64) && (Type <= 127))
        {
            return true;
        }

        return false;
    }

    bool hasQuantizationTable()
    {
        if (Q >= 128)
        {
            return true;
        }

        return false;
    }
}JPEG_HEADER;

typedef struct
{
    uint16_t RestartInterval;
    uint16_t F:1;
    uint16_t L:1;
    uint16_t RestartCount:14;
}RESTART_MARKER_HEADER;

typedef struct
{
    uint8_t MBZ;
    uint8_t Precision;
    uint16_t Length;
}QUANTIZATION_TABLE_HEADER;

typedef std::list<ACE_Message_Block*> MEDIA_PACK_LIST;
typedef MEDIA_PACK_LIST::iterator MEDIA_PACK_LIST_ITER;

class CMduMediaProcessor
{
private:
    CMduMediaProcessor(const CMduMediaProcessor&);
    CMduMediaProcessor& operator=(const CMduMediaProcessor&);
public:
    CMduMediaProcessor();
    virtual ~CMduMediaProcessor();

    virtual int32_t Init(uint64_t ullSendSessionID);

    virtual void Send (ACE_Message_Block* pMb)=0;
    virtual void Dump(ACE_HANDLE handle) = 0;

    CMduSession* getSession();

    uint32_t getInputRate()const;

    void statFlux(uint32_t flux);

    uint32_t getFlux(uint32_t& ulStartTime, uint32_t& ulEndTime);
protected:
    uint64_t            m_SendSessionId;
    CMduSession*        m_pSendSession;
    CDataStat           m_dataStat;
};


class CPacketList
{
public:
    CPacketList();
    virtual ~CPacketList();

    int32_t  Send(CMduMediaProcessor* pProcessor);
    int32_t  Save(const ACE_Message_Block *pMb);
private:
    void Clear();
    void ClearFrame(uint32_t num);

    enum
    {
        MAX_BUF_PACKET_NUM = 100
    };

    MEDIA_PACK_LIST     m_PacketList;
    uint64_t            m_SessionId;
};


class CDirectProcessor : public CMduMediaProcessor
{
public:
    CDirectProcessor();
    virtual ~CDirectProcessor();

    virtual void Send (ACE_Message_Block* pMb);
    virtual void Dump(ACE_HANDLE handle);
private:
    void  Save(const ACE_Message_Block *pMb);

    CPacketList     m_PacketBuf;
    uint32_t        m_SavePacketNum;
    uint32_t        m_DropPacketNum;
};

typedef std::list<CMduMediaProcessor*>  ProcessorList;
typedef ProcessorList::iterator         ProcessorIter;

class CMduMediaProcessorSet
{
public:
    CMduMediaProcessorSet();

    virtual ~CMduMediaProcessorSet();

    virtual int32_t AddMediaProcessor(CMduMediaProcessor* pProcessor);

    virtual int32_t DelMediaProcessor(CMduMediaProcessor*& pProcessor);

    virtual int32_t Send(ACE_Message_Block* pMb);

    virtual int32_t SetEncodeFormat(uint32_t unEncodeFormat);
protected:
    int32_t GetSendSessionType( CMduMediaProcessor* pProcessor,
                                   uint32_t& unSessionType) const;
private:
    int32_t SendEsRtpPacket(ACE_Message_Block* pMb);
private:
    void Destroy();
protected:
    ProcessorList                           m_EsOverRtpProcessList;
};

#define H264_START_CODE 4
#define H265_START_CODE 4


#define PS_HEAD_BUF_SIZE_MAX   32
#pragma pack(1)
union littel_endian_size
{
    unsigned short int  length;
    unsigned char       byte[2];
};
struct pack_start_code
{
    unsigned char start_code[3];
    unsigned char stream_id[1];
};
struct program_stream_pack_header
{
    pack_start_code PackStart;// 4

    uint8_t         system_clock_reference_base21:2;
    uint8_t         marker_bit:1;
    uint8_t         system_clock_reference_base1:3;
    uint8_t         fix_bit:2;    //'01'

    uint8_t         system_clock_reference_base22;

    uint8_t         system_clock_reference_base31:2;
    uint8_t         marker_bit1:1;
    uint8_t         system_clock_reference_base23:5;

    uint8_t         system_clock_reference_base32;

    uint8_t         system_clock_reference_extension1:2;
    uint8_t         marker_bit2:1;
    uint8_t         system_clock_reference_base33:5; //system_clock_reference_base 33bit

    uint8_t         marker_bit3:1;
    uint8_t         system_clock_reference_extension2:7; //system_clock_reference_extension 9bit

    uint8_t         program_mux_rate1;

    uint8_t         program_mux_rate2;

    uint8_t         marker_bit5:1;
    uint8_t         marker_bit4:1;
    uint8_t         program_mux_rate3:6;

    uint8_t         pack_stuffing_length:3;
    uint8_t         reserved : 5;
};
struct program_system_pack_header
{
    pack_start_code     PackStart;// 4
    littel_endian_size  PackLength;//2

    uint32_t            marker_bit1:1;//1
    uint32_t            rate_bound:22;
    uint32_t            marker_bit2:1;
    uint32_t            audio_bound:6;
    uint32_t            fixed_flag:1;
    uint32_t            CSPS_flag:1;

    uint16_t system_audio_lock_flag:1;
    uint16_t system_video_lock_flag:1;
    uint16_t            marker_bit3:1;
    uint16_t            video_bound:5;
    uint16_t packet_rate_restriction_flag:1;
    uint16_t            reserved_bits:7;

    uint8_t             streamid;
    uint8_t             reserved[5];
};//18

struct program_elementary_stream_info
{
    uint8_t             stream_type;
    uint8_t             elementary_stream_id;
    uint8_t             elementary_stream_info_length[2];
};
struct program_elementary_stream_map_info
{
    uint8_t             elementary_stream_map_length[2];
};


struct program_stream_map
{
    pack_start_code    PackStart;
    littel_endian_size PackLength;//we mast do exchange

    uint8_t            program_stream_map_version:5;
    uint8_t            reserved1:2;
    uint8_t            current_next_indicator:1;

    uint8_t            marker_bit:1;
    uint8_t            reserved2:7;

    uint8_t            program_stream_info_length[2];

};
struct program_private_head
{
    pack_start_code PackStart;
    littel_endian_size PackLength;//we mast do exchange
};
struct program_stream_e
{
    pack_start_code     PackStart;
    littel_endian_size  PackLength;//we mast do exchange
    char                PackInfo1[2];
    unsigned char       stuffing_length;
};


#define RTP_PLAYLOAD_LEN   1400
#define RTP_PACK_BUF_SIZE   1500

#define AU_HEADER_SIZE 4
#define AAC_EXTRA      7
#define AU_HALF_SIZE   2

struct es_frame_info
{
    uint8_t            payload;
    uint32_t           length;
    uint8_t            keyframe;
    uint8_t            streamId;
    uint32_t           timestamp;
    uint32_t           ssrc;
};

#pragma pack()


class CMduPsMediaProcessorSet: public CMduMediaProcessorSet,IRtpFrameHandler
{
public:
    CMduPsMediaProcessorSet();
    virtual ~CMduPsMediaProcessorSet();

    virtual int32_t Send(ACE_Message_Block* pMb);

    virtual void handleRtpFrame(RTP_FRAME_LIST &rtpFrameList);
private:
    bool checkProgramStreamHead(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void dealProgramStreamRtpFrame(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    int32_t SkipAllRtpHead(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList,RTP_FRAME_LIST &PatserFrameList);
    void ProgramStreamPackHeader(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void ProgramSystemPackHeader(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void ProgramStreamMap(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void ProgramPrivateHeader(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void ProgramEStramHead(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void ProgramKnowFrame(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void sendEsAudioFrame(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void sendEsVideoFrame(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void sendH264EsSigleNalu(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void sendH264EsFuNale(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void sendH265EsSigleNalu(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void sendH265EsFuNale(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
    void skipunKnowFrame(es_frame_info& FrameInfo,RTP_FRAME_LIST &rtpFrameList);
private:
    enum MPEGParseState {
      PARSING_PACK_HEADER,
      PARSING_SYSTEM_HEADER,
      PARSING_SYSMAP_HEADER,
      PARSING_PRIVATE_HEADER,
      PARSING_PES_PACKET,
      PARSING_FOUND_DATA
    };
    CRtpFrameOrganizer    m_RtpFrameOrganizer;
    uint16_t              m_usRtpSeq;
    MPEGParseState        m_enStatus;
    uint8_t               m_VideoPayload;
    uint8_t               m_AudioPayload;
};

#endif


