#ifndef __CLOCKFREEMEDIABUFFER_H__
#define __CLOCKFREEMEDIABUFFER_H__

#include "svs_adapter_media_data_queue.h"

class CMediaBlockBuffer
{
public:
    CMediaBlockBuffer();

    virtual ~CMediaBlockBuffer();

    static CMediaBlockBuffer& instance()
    {
        if (NULL == g_MediaBuffer)
        {
            try
            {
                g_MediaBuffer = new CMediaBlockBuffer;
            }
            catch(...)
            {
                static CMediaBlockBuffer buffer;
                g_MediaBuffer = &buffer;
            }
        }

        return (*g_MediaBuffer);
    }

    int32_t init(uint32_t unBlockSize, uint32_t unBlockCount);

    void close();

    ACE_Message_Block* allocMediaBlock();

    void freeMediaBlock(ACE_Message_Block *&pMb);

    uint32_t getMaxBlockNum() const;

    uint32_t getRemainBlockNum() const;

    uint32_t getBlockSize()const;

    uint32_t getFailureTimes()const;
private:
    ACE_Message_Block* createMediaBlock(uint32_t unBlockSize)const;

private:
    static CMediaBlockBuffer* g_MediaBuffer;

    CMediaDataQueue*          m_pMediaQueue;

    uint32_t                  m_unSize;
    uint32_t                  m_unCount;
    uint32_t                  m_FailureTimes;
};


#endif // __CLOCKFREEMEDIABUFFER_H__
