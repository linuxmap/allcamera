#ifndef __SIGNAL_BUFFER_MANAGER_H__
#define __SIGNAL_BUFFER_MANAGER_H__
#include "svs_adapter_svs_def.h"
#include "svs_ace_header.h"
#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_types.h"


#ifndef UTAPI
#define UTAPI
#endif

//һ���Է�����Ϣ����
#define SVS_ALLOC_MSG_BUFF_COUNT    200

//һ�����ͷ���Ϣ����
#define SVS_FREE_MSG_BUFF_COUNT     200

//��������Ϣ
typedef struct _SVS_Mb_Buffer_Info
{
    uint8_t type;     //��������Ϣ����
    uint8_t length;   //��������Ϣ����
}SVS_Mb_Buffer_Info;
// ��Ϣ��������
class CSignalBuffer
{
public:
    CSignalBuffer();
    virtual ~CSignalBuffer();

    //��ʼ��������
    int32_t init(   uint32_t mbCount,
                uint32_t mbSize,
                uint8_t mbType,
                uint8_t mbLen );

    //���뻺����
    UTAPI ACE_Message_Block *alloc_mb();

    //����ָ�������Ļ�����
    UTAPI ACE_Message_Block *alloc_mb( uint32_t allocCount );

    //�ͷŻ�����
    int32_t free_mb(ACE_Message_Block *mb);

    //������л���������
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
    //��λ����
    void reset();
protected:
    uint32_t mb_size_;            //ÿ��mb��С

    uint32_t free_mb_count_;      //����mb����

    uint32_t max_free_mb_count_;  //��������

    ACE_Message_Block* free_mb_list_;  //����mb�б�

    ACE_Thread_Mutex mutex_; //������

    bool inited_flag_;  //�ѳ�ʼ����־

    uint8_t mb_type_; //��Ϣ����

    uint8_t mb_len_;  //��Ϣ����
};
// ��Ϣ������������
class CSignalBufferManager
{
public:
    enum _MB_TYPE_
    {
        MB_TYPE_NORMAL_EVENT = 0,    //����������Ϣ����
        MB_TYPE_MAX_VALUE
    };

    enum _MB_LEN_
    {
        MB_LEN_128_BYTE = 0,    //��Ϣ���ȣ�128�ֽ�
        MB_LEN_256_BYTE,        //��Ϣ���ȣ�256�ֽ�
        MB_LEN_1K_BYTE,         //��Ϣ���ȣ�1K�ֽ�
        MB_LEN_4K_BYTE,         //��Ϣ���ȣ�4K�ֽ�
        MB_LEN_64K_BYTE,        //��Ϣ���ȣ�64K�ֽ�
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

    //����Ϣ��Сת������Ϣ��������
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
        case MB_TYPE_NORMAL_EVENT:  //����������Ϣ
            mbLen = size_to_len_normal(mbSize);
            break;
        default:
            break;
        }
        return mbLen;
    }

    //��ʼ��
    UTAPI int32_t init();

    //���뻺����
    UTAPI ACE_Message_Block *alloc_mb(    uint8_t mbType,
                                    uint32_t mbSize,
                                    uint32_t mbCount = 1) const;

    //�ͷŻ�����
    UTAPI int32_t free_mb(ACE_Message_Block *mb) const;

    //������л�����
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
                //�����������꣬�˳�ѭ��
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
    // ����ģʽ���࣬��������������Ҫ����
    CSignalBufferManager();

    // ��ȡ"�����¼�"���͵�buffer
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

    //������Ϣ���ͺ���Ϣ��С����ȡ������ָ��
    CSignalBuffer *get_mb_buffer(uint8_t mbType, uint32_t mbLen) const;
protected:

    // ��Ϣ������
    CSignalBuffer *mb_buff_[MB_TYPE_MAX_VALUE][MB_LEN_MAX_VALUE];

    // �ѳ�ʼ����־
    bool inited_flag_;
};


// �߳���Ϣ������������
class CSignalBufferTss
{
public:
    CSignalBufferTss();
    virtual ~CSignalBufferTss();

    //��ʼ��
    void init();

    //������Ϣ������
    UTAPI ACE_Message_Block *alloc_mb(uint8_t mbType, uint32_t mbSize );

    //�ͷ���Ϣ������
    UTAPI void free_mb(ACE_Message_Block *mb);

    //�������
    UTAPI void clear();

    //�����δ�ͷŵ���Ϣ����
    UTAPI int32_t alloced_msg_buffer_count(uint8_t type, uint8_t len)const
    {
        return alloced_msg_buffer_count_[type][len];
    }
protected:
    //��λ����
    void reset();
protected:
    enum
    {
        THREAD_MSG_MAX_TYPE = CSignalBufferManager::MB_TYPE_MAX_VALUE,
        THREAD_MSG_MAX_LEN = CSignalBufferManager::MB_LEN_MAX_VALUE
    };

    //������Ϣ������
    ACE_Message_Block *free_msg_buffer_[THREAD_MSG_MAX_TYPE][THREAD_MSG_MAX_LEN];

    //ʹ�õ���Ϣ������
    ACE_Message_Block *used_msg_buffer_;

    //ʹ�õ���Ϣ����������Ϣ�����
    uint32_t used_msg_buffer_count_;

    //�����δ�ͷŵ���Ϣ����
    int32_t alloced_msg_buffer_count_[THREAD_MSG_MAX_TYPE][THREAD_MSG_MAX_LEN];
};

#endif /*__SIGNAL_BUFFER_MANAGER_H__*/


