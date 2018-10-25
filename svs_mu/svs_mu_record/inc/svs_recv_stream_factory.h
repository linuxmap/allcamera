#ifndef _SVS_Recv_Stream_Factory_h
#define _SVS_Recv_Stream_Factory_h
#include <vector>
#include "svs_recv_stream.h"
#include "svs_rt_record_common.h"
using namespace std;

/**
 * @class CAC_Recv_Stream_Factory
 * @brief 媒体流接收对象工厂类，负责媒体流接收对象的创建和释放
 */
class CAC_Recv_Stream_Factory
{
public:
    CAC_Recv_Stream_Factory();

    ~CAC_Recv_Stream_Factory();

    static CAC_Recv_Stream_Factory &instance()
    {
        static CAC_Recv_Stream_Factory factory;
        return factory;
    }

    // 初始化流媒体接收类工厂, 最多支持<nMaxLens>个镜头录像.
    int32_t init(uint32_t ulMaxLens, uint32_t ulMaxRcvThread);

    // 增加镜头ID参数
    CAC_Recv_Stream * create();

    int32_t destroy(CAC_Recv_Stream *pStream);

    // 取最空闲的线程号
    uint32_t most_free_thread_no();
private:
    // 接收流对象大小
    const uint32_t object_size_;

    // 最大镜头数
    uint32_t max_lens_;

    // 最大接收线程数
    uint32_t max_recv_thread_;

    char *p_object_buffer_;

    ACE_Recursive_Thread_Mutex mutex_;

    ACE_DLList <char> free_list_;

    // 接收线程池中每个线程所挂镜头个数
    vector<uint32_t> device_num_pre_thread_vector_;

    // 每个镜头所挂的线程号
    vector<uint32_t> device_using_thread_vector_;
};
#endif //_SVS_Recv_Stream_Factory_h



