#ifndef __CSTREAMMEDIAEXCHANGE_H__
#define __CSTREAMMEDIAEXCHANGE_H__

#include <list>
#include <map>
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_processor.h"

typedef enum
{
    STREAM_PACKET_TYPE_MEDIA_DATA   = 0,
    STREAM_PACKET_TYPE_ADD_SESSION  = 1,
    STREAM_PACKET_TYPE_DEL_SESSION  = 2,
    STREAM_PACKET_TYPE_SESSION_EOS  = 3,
    STREAM_PACKET_TYPE_SESSION_BOS  = 4,

    STREAM_PACKET_TYPE_MAX
}STREAM_PACKET_TYPE;


#pragma pack(push, 1)

typedef struct
{
    CStreamMediaProcessor* pMediaProcessor;
    uint32_t            unRecvTranType;
    uint32_t            unSendTranType;
}STREAM_PROCESSOR_INFO;

typedef struct
{
    STREAM_PACKET_TYPE     enPacketType;
    uint64_svs          PuStreamId;
    uint8_t             Reserved[4];
    char                cData[1];
}STREAM_TRANSMIT_PACKET;
#pragma pack(pop)

class CStreamMediaExchange : public ACE_Task<ACE_MT_SYNCH>
{
private:
    CStreamMediaExchange();
    CStreamMediaExchange(const CStreamMediaExchange&);
    CStreamMediaExchange& operator=(const CStreamMediaExchange&);
public:
    ~CStreamMediaExchange();

    static CStreamMediaExchange* instance()
    {
        if (NULL == g_pMediaExchange)
        {
            try
            {
                g_pMediaExchange = new CStreamMediaExchange;
            }
            catch(...)
            {
                return NULL;
            }
        }

        return g_pMediaExchange;
    }

    int32_t Init(uint32_t ulThreadNum);

    void    Destroy();

    int32_t svc();

    int32_t addData(ACE_Message_Block* pDataBlock)const;

    int32_t addMediaProcessor(uint64_svs PuStreamId,CStreamMediaProcessor* pstProcessor,
                              uint32_t   unRecvTranType,uint32_t unSendTranType)const;

    int32_t delMediaProcessor(uint64_svs PuStreamId,CStreamMediaProcessor* pstProcessor,
                              uint32_t   unRecvTranType )const;
private:
    int32_t GenProcessThreadIdx(uint64_svs PuStreamId,uint32_t& ulThreadIdx)const;

    int32_t EnqueuePacket(uint32_t ulThreadIdx,ACE_Message_Block* pMb)const;

    uint32_t getThreadIndex();

    void MediaExchangeThread(uint32_t ulThreadIdx);

    void ProcessMediaData(uint32_t ulThreadIdx, ACE_Message_Block* pMb)const;
    void ProcessAddSession(uint32_t ulThreadIdx, const ACE_Message_Block* pMb)const;
    void ProcessDelSession(uint32_t ulThreadIdx, const ACE_Message_Block* pMb)const;

private:
    static CStreamMediaExchange*       g_pMediaExchange;
    typedef std::map<uint64_svs, CStreamMediaProcessorSet*>      MediaExchangeMap;
    typedef MediaExchangeMap::iterator                          MediaExchangeIter;

    MediaExchangeMap**              m_pMediaExchangeMap;

    uint32_t                    m_ulThreadIdx;
    CMediaDataQueue**               m_pDataExchangeQueue;   /// ���ת������

    ACE_Thread_Mutex                m_ExchangeMutex;        /// ������ȡ�߳�ID

    uint32_t                    m_ulThreadNum;          // �߳���

    bool                            m_bRunning;
};



#endif



