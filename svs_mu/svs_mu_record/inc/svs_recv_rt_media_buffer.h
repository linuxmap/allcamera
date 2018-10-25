#ifndef __SVS_Recv_RT_Media_Buffer_h_
#define __SVS_Recv_RT_Media_Buffer_h_
#include <list>
using namespace std;
#include "ace/Message_Block.h"
#include "ace/Recursive_Thread_Mutex.h"
#include "ace/Guard_T.h"
#include "svs_rt_record_common.h"

class CSVS_Recv_RT_Media_Buffer;

// ý�����
class SVS_Media_Block : public ACE_Message_Block
{
public:
    SVS_Media_Block(CSVS_Recv_RT_Media_Buffer *pOwner, size_t nSize);

    SVS_Media_Block(CSVS_Recv_RT_Media_Buffer *pOwner, size_t nSize, bool bTmpfsMemory );

    virtual ~SVS_Media_Block();

    // ��д���ݿ���ͷŽӿڣ�ֱ�ӵ���m_pOwner�ͷ�
    virtual ACE_Message_Block *release();

    void SetLastMemoryBufferFlag( bool bFlag )
    {
        m_bLastMemoryBufferFlag = bFlag;
    }
private:
    SVS_Media_Block();
    SVS_Media_Block(size_t nSize);

    CSVS_Recv_RT_Media_Buffer *m_pOwner;

    bool m_bTmpfsMemory;    // tmpfs�ڴ��־
    bool m_bLastMemoryBufferFlag;   // ���1���ڴ滺���
};

//����ý�建����
class CSVS_Recv_RT_Media_Buffer
{
public:
    CSVS_Recv_RT_Media_Buffer();
    ~CSVS_Recv_RT_Media_Buffer();
public:
    //����:����������
    //BufferLen:����������,��λbyte
    //BufferNum:����������
    //����ֵ:�ɹ�����0,ʧ�ܷ���-1
    int32_t init( uint32_t BufferLen, uint32_t BufferNum,
              uint32_t tmpfsBufferLen, uint32_t tmpfsBufferNum, uint32_t lensNum );

    //����:�����������Ļ�����
    //����ֵ:��ȷ����0,ʧ�ܷ���-1
    int32_t close();

    //����:��ȡ������
    //����ֵ:ʧ�ܷ���NULL,�ɹ��������뵽�Ļ�����
    ACE_Message_Block* alloc_buffer( bool bLensFirstAllocFlag );

    ACE_Message_Block* alloc_tmpfs_buffer();

    //����:����ָ���Ļ�����
    //����ֵ:�ɹ�����0,ʧ�ܷ���-1
    int32_t free_buffer( ACE_Message_Block* buffer, bool bShareMemory, bool bLastMemoryBuffer );


    //����:ȡ���л���������
    uint32_t free_buffer_size()
    {
        ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, 0 );
        uint32_t freeSize = free_buffer_list_.size();
        freeSize += tmpfs_free_buffer_list_.size();
        return freeSize;
    }


    //����:ȡ��ʹ�û���������
    uint32_t used_buffer_size()
    {
        ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, 0 );
        uint32_t usedSize = used_buffer_list_.size();
        usedSize += tmpfs_used_buffer_list_.size();
        return usedSize;
    }

    //ȡÿ���������ĳ���
    uint32_t buffer_len() const
    {
        return buffer_len_;
    }

    // ��ȡ����������ʧ�ܴ���,������������Ϣ
    uint64_t alloc_buffer_failure_times() const
    {
        return alloc_buffer_failure_times_;
    }

    // ��ȡ�����������ܴ���,������������Ϣ
    uint64_t alloc_buffer_times() const
    {
        return alloc_buffer_times_;
    }
private:
    int32_t init_tempfs_buffer(uint32_t tmpfsBufferNum, uint32_t tmpfsBufferLen);
protected:
    bool init_flag_;            //�ѳ�ʼ����־

    uint32_t buffer_len_;  //����������

    uint64_t alloc_buffer_times_;   //���仺��������

    ACE_Recursive_Thread_Mutex buffer_mutex_;       //�����������б���

    std::list< ACE_Message_Block* > free_buffer_list_;   //���л������б�

    std::list< ACE_Message_Block* > used_buffer_list_;   //ʹ�û������б�

    std::list< ACE_Message_Block* > tmpfs_free_buffer_list_;   // tmpfs���л������б�
    std::list< ACE_Message_Block* > tmpfs_used_buffer_list_;   // tmpfsʹ�û������б�

    uint32_t serve_lens_num_;  // ����ľ�ͷ����
    uint32_t curr_serve_lens_num_; // ��ǰ����ľ�ͷ��
    uint32_t free_memory_buffer_num_;  // ���е��ڴ滺�����
    uint32_t free_tmpfs_buffer_num_;   // ���е�tmpfs�������
    bool used_tmpfs_buffer_flag_;    // �Ƿ�ʹ��tmpfs��־

    //���仺����ʧ�ܴ���
    uint64_t alloc_buffer_failure_times_;

    SVS_Media_Buffer_Stat media_buffer_stat_;
};

#endif // SVS_Recv_RT_Media_Buffer.h



