#ifndef __SIGNAL_BUFFER_MANAGER_H__
#define __SIGNAL_BUFFER_MANAGER_H__
#include "svs_adapter_svs_def.h"
#include "svs_ace_header.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_types.h"


#ifndef UTAPI
#define UTAPI
#endif

//一次性分配消息个数
#define SVS_ALLOC_MSG_BUFF_COUNT    200

//一次性释放消息个数
#define SVS_FREE_MSG_BUFF_COUNT     200

//缓冲区信息
typedef struct _SVS_Mb_Buffer_Info
{
    uint8_t type;     //缓冲区消息类型
    uint8_t length;   //缓冲区消息长度
}SVS_Mb_Buffer_Info;
// 消息缓冲区类
class CSignalBuffer
{
public:
    CSignalBuffer();
    virtual ~CSignalBuffer();

    //初始化缓冲区
    int32_t init(   uint32_t mbCount,
                uint32_t mbSize,
                uint8_t mbType,
                uint8_t mbLen );

    //申请缓冲区
    UTAPI ACE_Message_Block *alloc_mb();

    //申请指定个数的缓冲区
    UTAPI ACE_Message_Block *alloc_mb( uint32_t allocCount );

    //释放缓冲区
    int32_t free_mb(ACE_Message_Block *mb);

    //清空所有缓冲区数据
    int32_t clear();
    UTAPI uint32_t max_free_mb_count()const
    {
        return max_free_mb_count_;
    }

    UTAPI uint32_t free_mb_count()const
    {
        return free_mb_count_;
    }
protected:
    //复位数据
    void reset();
protected:
    uint32_t mb_size_;            //每个mb大小

    uint32_t free_mb_count_;      //空闲mb个数

    uint32_t max_free_mb_count_;  //最大空闲数

    ACE_Message_Block* free_mb_list_;  //空闲mb列表

    ACE_Thread_Mutex mutex_; //互斥锁

    bool inited_flag_;  //已初始化标志

    uint8_t mb_type_; //消息类型

    uint8_t mb_len_;  //消息长度
};
// 消息缓冲区管理类
class CSignalBufferManager
{
public:
    enum _MB_TYPE_
    {
        MB_TYPE_NORMAL_EVENT = 0,    //其它信令消息类型
        MB_TYPE_MAX_VALUE
    };

    enum _MB_LEN_
    {
        MB_LEN_128_BYTE = 0,    //消息长度，128字节
        MB_LEN_256_BYTE,        //消息长度，256字节
        MB_LEN_1K_BYTE,         //消息长度，1K字节
        MB_LEN_4K_BYTE,         //消息长度，4K字节
        MB_LEN_64K_BYTE,        //消息长度，64K字节
        MB_LEN_MAX_VALUE
     };

    enum _MB_LEN_VALUE_
    {
        SVS_128_BYTE = 128,
        SVS_256_BYTE = 256,
        SVS_1K_BYTE = 1024,
        SVS_4K_BYTE = (4*1024),
        SVS_64K_BYTE = (64*1024)
    };

    static CSignalBufferManager* instance()
    {
        static CSignalBufferManager* pManager = NULL;
        if ( NULL == pManager )
        {
            pManager = new CSignalBufferManager;
        }
        return pManager;
    }

    virtual ~CSignalBufferManager();

    //从消息大小转换成消息长度类型
    UTAPI uint8_t size_to_len_normal(uint32_t mbSize)const
    {
        uint8_t mbLen = MB_LEN_MAX_VALUE;
        if(mbSize <= SVS_128_BYTE)
        {
            mbLen = MB_LEN_128_BYTE;
        }
        else if ( mbSize <= SVS_256_BYTE )
        {
            mbLen = MB_LEN_256_BYTE;
        }
        else if( mbSize <= SVS_1K_BYTE )
        {
            mbLen = MB_LEN_1K_BYTE;
        }
        else if( mbSize <= SVS_4K_BYTE )
        {
            mbLen = MB_LEN_4K_BYTE;
        }
        else if(mbSize <= SVS_64K_BYTE)
        {
            mbLen = MB_LEN_64K_BYTE;
        }
        return mbLen;
    }

    UTAPI uint8_t mb_size_to_len_type( uint8_t mbType, uint32_t mbSize )const
    {
        uint8_t mbLen = MB_LEN_MAX_VALUE;
        switch ( mbType )
        {
        case MB_TYPE_NORMAL_EVENT:  //其它信令消息
            mbLen = size_to_len_normal(mbSize);
            break;
        default:
            break;
        }
        return mbLen;
    }

    //初始化
    UTAPI int32_t init();

    //申请缓冲区
    UTAPI ACE_Message_Block *alloc_mb(    uint8_t mbType,
                                    uint32_t mbSize,
                                    uint32_t mbCount = 1) const;

    //释放缓冲区
    UTAPI int32_t free_mb(ACE_Message_Block *mb) const;

    //清空所有缓冲区
    UTAPI int32_t clear();

    UTAPI int32_t debug_get_buffer_info(  char *strBufferInfo,
                                const uint32_t szBufferInfo )const
    {
        (void)ACE_OS::snprintf( strBufferInfo,
            szBufferInfo,
            "\t   MessageBlockType         Size        TotalNum    FreeNum\n\n" );
        size_t writeLen = ACE_OS::strlen( strBufferInfo );
        const char* msgTypeDesc[] =
        {
            "Normal event        ",
            "\0"
        };
        const char* msgSizeDesc[] =
        {
            "128 byte",
            "256 byte",
            "1K  byte",
            "4K  byte",
            "64K byte",
            "\0"
         };
        for ( uint8_t i = 0; i < MB_TYPE_MAX_VALUE; i++ )
        {
            for ( uint8_t j = 0; j < MB_LEN_MAX_VALUE; j++ )
            {
                if ( NULL == mb_buff_[i][j] )
                {
                    continue;
                }
                writeLen = ACE_OS::strlen( strBufferInfo );
                //缓冲区已用完，退出循环
                if( 0 == szBufferInfo-writeLen )
                {
                    break;
                }
                (void)ACE_OS::snprintf(  &strBufferInfo[writeLen],
                                         szBufferInfo-writeLen,
                                         "\t%s(%d)   %s(%d)  %8d   %8d \n",
                                         msgTypeDesc[i],
                                         i,
                                         msgSizeDesc[j],
                                         j,
                                         mb_buff_[i][j]->max_free_mb_count(),
                                         mb_buff_[i][j]->free_mb_count() );
            }
        }
        return 0;
    }
protected:
    // 单例模式的类，构造析构函数需要隐藏
    CSignalBufferManager();

    // 获取"正常事件"类型的buffer
    CSignalBuffer *get_mb_buffer_normal_event(uint32_t mbSize) const
    {
        if( mbSize <= SVS_128_BYTE )
        {
            return mb_buff_[MB_TYPE_NORMAL_EVENT][MB_LEN_128_BYTE];
        }
        else if ( mbSize <= SVS_256_BYTE )
        {
            return mb_buff_[MB_TYPE_NORMAL_EVENT][MB_LEN_256_BYTE];
        }
        else if( mbSize <= SVS_1K_BYTE )
        {
            return mb_buff_[MB_TYPE_NORMAL_EVENT][MB_LEN_1K_BYTE];
        }
        else if( mbSize <= SVS_64K_BYTE )
        {
            return mb_buff_[MB_TYPE_NORMAL_EVENT][MB_LEN_64K_BYTE];
        }
        return NULL;
    }

    //根据消息类型和消息大小，获取缓冲区指针
    CSignalBuffer *get_mb_buffer(uint8_t mbType, uint32_t mbLen) const;
protected:

    // 消息缓冲区
    CSignalBuffer *mb_buff_[MB_TYPE_MAX_VALUE][MB_LEN_MAX_VALUE];

    // 已初始化标志
    bool inited_flag_;
};


// 线程消息缓冲区管理类
class CSignalBufferTss
{
public:
    CSignalBufferTss();
    virtual ~CSignalBufferTss();

    //初始化
    void init();

    //申请消息缓冲区
    UTAPI ACE_Message_Block *alloc_mb(uint8_t mbType, uint32_t mbSize );

    //释放消息缓冲区
    UTAPI void free_mb(ACE_Message_Block *mb);

    //清空数据
    UTAPI void clear();

    //申请后未释放的消息个数
    UTAPI int32_t alloced_msg_buffer_count(uint8_t type, uint8_t len)const
    {
        return alloced_msg_buffer_count_[type][len];
    }
protected:
    //复位数据
    void reset();
protected:
    enum
    {
        THREAD_MSG_MAX_TYPE = CSignalBufferManager::MB_TYPE_MAX_VALUE,
        THREAD_MSG_MAX_LEN = CSignalBufferManager::MB_LEN_MAX_VALUE
    };

    //空闲消息缓冲区
    ACE_Message_Block *free_msg_buffer_[THREAD_MSG_MAX_TYPE][THREAD_MSG_MAX_LEN];

    //使用的消息缓冲区
    ACE_Message_Block *used_msg_buffer_;

    //使用的消息缓冲区中消息块个数
    uint32_t used_msg_buffer_count_;

    //申请后未释放的消息个数
    int32_t alloced_msg_buffer_count_[THREAD_MSG_MAX_TYPE][THREAD_MSG_MAX_LEN];
};

#endif /*__SIGNAL_BUFFER_MANAGER_H__*/


