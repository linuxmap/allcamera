#ifndef __STATIC_PREASSIGN_BUFFER_H__
#define __STATIC_PREASSIGN_BUFFER_H__

#include <svs_ace_header.h>
#define DEFAULT_BUFFER_SIZE  (64 * 1024)

class Static_PreAssign_Buffer
{
public:
    Static_PreAssign_Buffer()
    {
        m_bufBlock = new char[DEFAULT_BUFFER_SIZE];
        memset(this->m_bufBlock, 0x0, DEFAULT_BUFFER_SIZE);
        m_nBufferLength = DEFAULT_BUFFER_SIZE;
        m_pHead = m_bufBlock;
        m_pTail = m_bufBlock;
        m_iLength = 0;
    }

    Static_PreAssign_Buffer(size_t bufferLength)
    {
        m_bufBlock = new char[bufferLength];
        memset(this->m_bufBlock, 0x0, bufferLength);
        m_nBufferLength = bufferLength;
        m_pHead = m_bufBlock;
        m_pTail = m_bufBlock;
        m_iLength = 0;
    }



    Static_PreAssign_Buffer(const Static_PreAssign_Buffer& tmp)
    {
        copyTmpBuff(tmp);
    }

    Static_PreAssign_Buffer& operator=(const Static_PreAssign_Buffer& tmp)
    {
        if (this == &tmp)
        {
            return *this;
        }
        if (this != &tmp)
        {
            if (NULL != m_bufBlock)
            {
                delete []m_bufBlock;
                m_bufBlock = NULL;
            }

            copyTmpBuff(tmp);

        }
        return *this;
    }


    virtual ~Static_PreAssign_Buffer()
    {
        if (NULL != m_bufBlock)
        {
            delete[] m_bufBlock;
        }

        m_bufBlock = NULL;
        m_pHead = NULL;
        m_pTail = NULL;
        m_iLength = 0;
    }

    void copyTmpBuff(const Static_PreAssign_Buffer& tmp)
    {
        m_bufBlock = NULL;
        if (NULL != tmp.m_bufBlock)
        {
            m_bufBlock = new char[tmp.m_nBufferLength];
            memset(this->m_bufBlock, 0, tmp.m_nBufferLength);
            (void)memcpy(m_bufBlock, tmp.m_bufBlock, tmp.m_nBufferLength);
        }

        if (NULL != m_bufBlock)
        {
            m_nBufferLength = tmp.m_nBufferLength;
            m_pHead = m_bufBlock + (tmp.m_pHead - tmp.m_bufBlock);
            m_pTail = m_bufBlock + (tmp.m_pTail - tmp.m_bufBlock);
            m_iLength = tmp.m_iLength;
        }
        else
        {
            m_nBufferLength = 0;
            m_pHead = NULL;
            m_pTail = NULL;
            m_iLength = 0;
        }

    }

    void wr_ptr(size_t n)
    {
        if (n > m_nBufferLength)
        {
            return;
        }

        if(NULL != m_bufBlock)
        {
            m_pTail = (char *) m_bufBlock + n;
            m_iLength = n;
        }
    }

    char * wr_ptr()
    {
        return m_pTail;
    }

    void rd_ptr(size_t n)
    {
        if (n > m_nBufferLength)
        {
            return;
        }

        if(NULL != m_bufBlock)
        {
            m_pHead = (char *) m_bufBlock + n;
        }
    }

    char * rd_ptr()
    {
        return m_pHead;
    }

    size_t size() const
    {
        return m_nBufferLength;
    }

    size_t length() const
    {
        return m_iLength;
    }

    int32_t copy(const char *buf, size_t n)
    {
        if (n > m_nBufferLength)
        {
            return -1;
        }

        if(NULL != m_bufBlock)
        {
            memmove(m_bufBlock, buf, n);
            m_pHead = m_bufBlock;
            m_pTail = (char *) m_bufBlock + n;
            m_iLength = n;
        }

        return (int32_t) n;
    }

    void reset()
    {
        m_pHead = m_bufBlock;
        m_pTail = m_bufBlock;
        m_iLength = 0;
    }

private:
    char    *m_bufBlock;
    size_t  m_nBufferLength;
    char    *m_pHead;
    char    *m_pTail;
    size_t  m_iLength;
};

#endif /* __STATIC_PREASSIGN_BUFFER_H__ */

