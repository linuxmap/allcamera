#ifndef _SVS_Mb_Buffer_h
#define _SVS_Mb_Buffer_h


class CAC_Mb_Buffer
{
public:
    CAC_Mb_Buffer();
    ~CAC_Mb_Buffer();
    static CAC_Mb_Buffer *instance()
    {
       static CAC_Mb_Buffer mbBuffer;
       return &mbBuffer;
    }

/*
* FUNCTION: int32_t init()
* DESCRIPTION: 初始化缓冲区
* INPUT:
*    int32_t nBufSize --- 缓冲区每个元素大小
*    int32_t nNum     --- 预先分配数量
* OUTPUT:
*    成功返回0，失败返回-1.
*/
   int32_t init(int32_t nBufSize, int32_t nNum);

   ACE_Message_Block *alloc_mb();
   ACE_Message_Block *alloc_mb(size_t mbSize);

   int32_t free_mb(ACE_Message_Block *mb);
    int32_t reset(int32_t iCount);
protected:
   int32_t sys_error_exit() const;
protected:
   size_t mb_size_;                //每个mb大小
    int32_t free_mb_count_;             //空闲mb个数
   ACE_Message_Block *free_mb_list_;  //空闲mb列表
   int16_t max_free_mb_;             //最大空闲数
   ACE_Thread_Mutex mutex_;
};



#endif //_SVS_Mb_Buffer_h




