#ifndef _SVS_Recv_Stream_Factory_h
#define _SVS_Recv_Stream_Factory_h
#include <vector>
#include "svs_recv_stream.h"
#include "svs_rt_record_common.h"
using namespace std;

/**
 * @class CAC_Recv_Stream_Factory
 * @brief ý�������ն��󹤳��࣬����ý�������ն���Ĵ������ͷ�
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

    // ��ʼ����ý������๤��, ���֧��<nMaxLens>����ͷ¼��.
    int32_t init(uint32_t ulMaxLens, uint32_t ulMaxRcvThread);

    // ���Ӿ�ͷID����
    CAC_Recv_Stream * create();

    int32_t destroy(CAC_Recv_Stream *pStream);

    // ȡ����е��̺߳�
    uint32_t most_free_thread_no();
private:
    // �����������С
    const uint32_t object_size_;

    // ���ͷ��
    uint32_t max_lens_;

    // �������߳���
    uint32_t max_recv_thread_;

    char *p_object_buffer_;

    ACE_Recursive_Thread_Mutex mutex_;

    ACE_DLList <char> free_list_;

    // �����̳߳���ÿ���߳����Ҿ�ͷ����
    vector<uint32_t> device_num_pre_thread_vector_;

    // ÿ����ͷ���ҵ��̺߳�
    vector<uint32_t> device_using_thread_vector_;
};
#endif //_SVS_Recv_Stream_Factory_h



