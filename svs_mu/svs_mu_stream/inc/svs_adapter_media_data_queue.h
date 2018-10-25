#ifndef __CMEDIADATAQUEUE_H__
#define __CMEDIADATAQUEUE_H__

#include "svs_ace_header.h"
#include "svs_adapter_atomic.h"


class CMediaDataQueue
{
public:
    CMediaDataQueue();

    virtual ~CMediaDataQueue();

    int32_t init(uint32_t unQueueSize);

    void close();

    uint32_t message_count() const;

    bool empty() const;

    bool full() const;

    int32_t enqueue_tail(ACE_Message_Block* mb, const ACE_Time_Value *timeout = NULL);

    int32_t dequeue_head(ACE_Message_Block*& mb, const ACE_Time_Value *timeout = NULL);
private:
    volatile uint32_t      m_WriteIndex;
    volatile uint32_t      m_WriteTag;
    volatile uint32_t      m_ReadIndex;
    volatile uint32_t      m_ReadTag;

    volatile int32_t       m_ActiveFlag; 
    uint32_t               m_unMaxQueueSize;
    uint32_t               m_unArraySize;

    volatile uint32_t      m_QueueSize; 
    ACE_Message_Block**    m_pDataArray;
};

#endif // __CMEDIADATAQUEUE_H__
