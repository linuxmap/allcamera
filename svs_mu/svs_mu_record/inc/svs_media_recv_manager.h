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

    // ��ʼ����Ӧ���̺߳ͽ��ջ�����
    int32_t open(void*);

    // ��Ӧ���߳�
    int32_t svc(void);

    // �رշ�Ӧ���߳�
    int32_t close(u_long);

    ACE_Reactor * getRecvReactor(uint32_t ulIndex);

    CSVS_Recv_RT_Media_Buffer *getMediaBuffer(uint32_t ulIndex);

private:
    SVS_Media_Recv_Manager();

    // ɾ����һ��ramdisk�����ļ�
    int32_t clear_tmpfs_recv_buffers()const;

    int32_t check_tmpfs_recv_buff_conf();

    // ˽�к������������ݽ��ջ�����
    int32_t create_recv_buffers();

    // ˽�к������������ݽ��շ�Ӧ��
    int32_t create_recv_reactors();

    // ˽�к�������ȡ�߳�����
    int32_t getThreadIndex()
    {
        ACE_Guard<ACE_Mutex> locker(m_ThreadIndexMutex);

        return m_nThreadIndex++;
    };

    // �����̸߳���
    uint32_t m_ulRecvThreadNum;

    // �߳��˳���־
    volatile bool  m_bThreadControlFlag;

    // ���շ�Ӧ��
    vector<ACE_Reactor *> m_pRecvReactorVector;

    // ���ջ�����
    vector<CSVS_Recv_RT_Media_Buffer*> m_pRecvMediaBuffer;

    // �߳�������
    volatile int32_t             m_nThreadIndex;

    // �߳������ŵ���
    ACE_Mutex                m_ThreadIndexMutex;
};

#endif
