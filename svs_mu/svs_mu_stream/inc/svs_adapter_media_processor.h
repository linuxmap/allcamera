#ifndef __CSTREAMMEDIAPROCESSOR_H__
#define __CSTREAMMEDIAPROCESSOR_H__



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

#include <list>
#include <map>
#include "svs_data_stat.h"
#include "svs_adapter_rtp_frame_organizer.h"
#include "svs_es_rtp_encap.h"
#include "svs_adapter_ps_rtp_encap.h"
#include "svs_adapter_rtp_packer.h"
#include "svs_adapter_ts_seg_encap.h"
#include "svs_adapter_rtp_packet.h"
#include "svs_adapter_mpeg_def.h"

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

class CStreamMediaProcessor
{
private:
    CStreamMediaProcessor(const CStreamMediaProcessor&);
    CStreamMediaProcessor& operator=(const CStreamMediaProcessor&);
public:
    CStreamMediaProcessor();
    virtual ~CStreamMediaProcessor();

    virtual int32_t Init(uint64_t ullSendSessionID);

    virtual void Send (ACE_Message_Block* pMb)=0;
    virtual void Dump(ACE_HANDLE handle) = 0;

    CStreamSession* getSession();

    uint32_t getInputRate()const;

    void statFlux(uint32_t flux);

    uint32_t getFlux(uint32_t& ulStartTime, uint32_t& ulEndTime);
protected:
    uint64_t            m_SendSessionId;
    CStreamSession*        m_pSendSession;
    CDataStat           m_dataStat;
};


class CPacketList
{
public:
    CPacketList();
    virtual ~CPacketList();

    int32_t  Send(CStreamMediaProcessor* pProcessor);
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


class CDirectProcessor : public CStreamMediaProcessor
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

typedef std::list<CStreamMediaProcessor*>  ProcessorList;
typedef ProcessorList::iterator         ProcessorIter;

class CStreamMediaProcessorSet
{
public:
    CStreamMediaProcessorSet();

    virtual ~CStreamMediaProcessorSet();

    virtual int32_t AddMediaProcessor(CStreamMediaProcessor* pProcessor);

    virtual int32_t DelMediaProcessor(CStreamMediaProcessor*& pProcessor);

    virtual int32_t Send(ACE_Message_Block* pMb);

    virtual int32_t SetEncodeFormat(uint32_t unEncodeFormat);
protected:
    int32_t GetSendSessionType( CStreamMediaProcessor* pProcessor,
                                   uint32_t& unSessionType) const;
private:
    int32_t SendEsRtpPacket(ACE_Message_Block* pMb);
private:
    void Destroy();
protected:
    ProcessorList                           m_EsOverRtpProcessList;
};



class CStreamPsMediaProcessorSet: public CStreamMediaProcessorSet,IRtpFrameHandler
{
public:
    CStreamPsMediaProcessorSet();
    virtual ~CStreamPsMediaProcessorSet();

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


