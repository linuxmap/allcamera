#ifndef __SVS_Recv_RT_Media_Buffer_h_
#define __SVS_Recv_RT_Media_Buffer_h_
#include <list>
using namespace std;
#include "ace/Message_Block.h"
#include "ace/Recursive_Thread_Mutex.h"
#include "ace/Guard_T.h"
#include "svs_rt_record_common.h"

class CSVS_Recv_RT_Media_Buffer;

// 媒体块类
class SVS_Media_Block : public ACE_Message_Block
{
public:
    SVS_Media_Block(CSVS_Recv_RT_Media_Buffer *pOwner, size_t nSize);

    SVS_Media_Block(CSVS_Recv_RT_Media_Buffer *pOwner, size_t nSize, bool bTmpfsMemory );

    virtual ~SVS_Media_Block();

    // 重写数据块的释放接口，直接调用m_pOwner释放
    virtual ACE_Message_Block *release();

    void SetLastMemoryBufferFlag( bool bFlag )
    {
        m_bLastMemoryBufferFlag = bFlag;
    }
private:
    SVS_Media_Block();
    SVS_Media_Block(size_t nSize);

    CSVS_Recv_RT_Media_Buffer *m_pOwner;

    bool m_bTmpfsMemory;    // tmpfs内存标志
    bool m_bLastMemoryBufferFlag;   // 最后1个内存缓冲块
};

//接收媒体缓冲类
class CSVS_Recv_RT_Media_Buffer
{
public:
    CSVS_Recv_RT_Media_Buffer();
    ~CSVS_Recv_RT_Media_Buffer();
public:
    //功能:初化缓冲区
    //BufferLen:缓冲区长度,单位byte
    //BufferNum:缓冲区个数
    //返回值:成功返回0,失败返回-1
    int32_t init( uint32_t BufferLen, uint32_t BufferNum,
              uint32_t tmpfsBufferLen, uint32_t tmpfsBufferNum, uint32_t lensNum );

    //功能:清空所有申请的缓冲区
    //返回值:正确返回0,失败返回-1
    int32_t close();

    //功能:获取缓冲区
    //返回值:失败返回NULL,成功返回申请到的缓冲区
    ACE_Message_Block* alloc_buffer( bool bLensFirstAllocFlag );

    ACE_Message_Block* alloc_tmpfs_buffer();

    //功能:缓放指定的缓冲区
    //返回值:成功返回0,失败返回-1
    int32_t free_buffer( ACE_Message_Block* buffer, bool bShareMemory, bool bLastMemoryBuffer );


    //功能:取空闲缓冲区个数
    uint32_t free_buffer_size()
    {
        ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, 0 );
        uint32_t freeSize = free_buffer_list_.size();
        freeSize += tmpfs_free_buffer_list_.size();
        return freeSize;
    }


    //功能:取已使用缓冲区个数
    uint32_t used_buffer_size()
    {
        ACE_GUARD_RETURN( ACE_Recursive_Thread_Mutex, pObj, buffer_mutex_, 0 );
        uint32_t usedSize = used_buffer_list_.size();
        usedSize += tmpfs_used_buffer_list_.size();
        return usedSize;
    }

    //取每个缓冲区的长度
    uint32_t buffer_len() const
    {
        return buffer_len_;
    }

    // 获取缓存区申请失败次数,缓存区调试信息
    uint64_t alloc_buffer_failure_times() const
    {
        return alloc_buffer_failure_times_;
    }

    // 获取缓存区申请总次数,缓存区调试信息
    uint64_t alloc_buffer_times() const
    {
        return alloc_buffer_times_;
    }
private:
    int32_t init_tempfs_buffer(uint32_t tmpfsBufferNum, uint32_t tmpfsBufferLen);
protected:
    bool init_flag_;            //已初始化标志

    uint32_t buffer_len_;  //缓冲区长度

    uint64_t alloc_buffer_times_;   //分配缓冲区次数

    ACE_Recursive_Thread_Mutex buffer_mutex_;       //操作缓冲区列表锁

    std::list< ACE_Message_Block* > free_buffer_list_;   //空闲缓冲区列表

    std::list< ACE_Message_Block* > used_buffer_list_;   //使用缓冲区列表

    std::list< ACE_Message_Block* > tmpfs_free_buffer_list_;   // tmpfs空闲缓冲区列表
    std::list< ACE_Message_Block* > tmpfs_used_buffer_list_;   // tmpfs使用缓冲区列表

    uint32_t serve_lens_num_;  // 服务的镜头总数
    uint32_t curr_serve_lens_num_; // 当前服务的镜头数
    uint32_t free_memory_buffer_num_;  // 空闲的内存缓冲个数
    uint32_t free_tmpfs_buffer_num_;   // 空闲的tmpfs缓冲个数
    bool used_tmpfs_buffer_flag_;    // 是否使用tmpfs标志

    //分配缓冲区失败次数
    uint64_t alloc_buffer_failure_times_;

    SVS_Media_Buffer_Stat media_buffer_stat_;
};

#endif // SVS_Recv_RT_Media_Buffer.h



