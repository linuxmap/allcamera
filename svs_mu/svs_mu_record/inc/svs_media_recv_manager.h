#ifndef __SVS_MEDIA_RECV_MANAGER_H__
#define  __SVS_MEDIA_RECV_MANAGER_H__
#include <vector>
using namespace std;

class SVS_Media_Recv_Manager : public ACE_Task<ACE_MT_SYNCH>
{
public:
    virtual ~SVS_Media_Recv_Manager();

    static SVS_Media_Recv_Manager & instance()
    {
        static SVS_Media_Recv_Manager svs_media_recv_manager;
        return svs_media_recv_manager;
    };

    // 初始化反应器线程和接收缓冲区
    int32_t open(void*);

    // 反应器线程
    int32_t svc(void);

    // 关闭反应器线程
    int32_t close(u_long);

    ACE_Reactor * getRecvReactor(uint32_t ulIndex);

    CSVS_Recv_RT_Media_Buffer *getMediaBuffer(uint32_t ulIndex);

private:
    SVS_Media_Recv_Manager();

    // 删除上一次ramdisk缓冲文件
    int32_t clear_tmpfs_recv_buffers()const;

    int32_t check_tmpfs_recv_buff_conf();

    // 私有函数，创建数据接收缓冲区
    int32_t create_recv_buffers();

    // 私有函数，创建数据接收反应器
    int32_t create_recv_reactors();

    // 私有函数，获取线程索引
    int32_t getThreadIndex()
    {
        ACE_Guard<ACE_Mutex> locker(m_ThreadIndexMutex);

        return m_nThreadIndex++;
    };

    // 接收线程个数
    uint32_t m_ulRecvThreadNum;

    // 线程退出标志
    volatile bool  m_bThreadControlFlag;

    // 接收反应器
    vector<ACE_Reactor *> m_pRecvReactorVector;

    // 接收缓冲器
    vector<CSVS_Recv_RT_Media_Buffer*> m_pRecvMediaBuffer;

    // 线程索引号
    volatile int32_t             m_nThreadIndex;

    // 线程索引号的锁
    ACE_Mutex                m_ThreadIndexMutex;
};

#endif
