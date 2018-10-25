/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, ��Ϊ�������޹�˾

 ******************************************************************************
  �� �� ��   : SVS_IO_Scheduler.h
  �� �� ��   : ����
  �������   : 2009��8��21��
  ����޸�   :
  ��������   : NRU��IO������(������),ʵ�ֶ��ļ�IO�ĵ���,��֤�Դ��̵Ķ�д�ܹ�ƽ��,
               ���ȹ��������ڶ����ͳ��ģ��
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2009��8��21��
    �޸�����   : �����ļ�

******************************************************************************/
#ifndef __SVS_IO_SCHEDULER_H__
#define  __SVS_IO_SCHEDULER_H__

#include "svs_rt_record_common.h"
class SVS_Media_Output_Stat;

// �������ģ���IO����
typedef enum tagIO_Operation
{
    IO_READ      = 0x01,            // ������
    IO_WRITE     = 0x02,            // д����
    IO_FFLUSH    = 0x03,           // ǿ�ư����ˢ�µ�����
    IO_CLOSE     = 0x10             // �رղ��������������ˢ�µ�����
} IO_Operation;

// �����첽IO�����Ļص��ӿ�,����ֱ�Ϊ(�����������Ĳ���)
typedef int32_t (*IO_CallBack_Func)(int32_t writeResult, int32_t errCode,void *pParam);

class SVS_IO_Scheduler : public ACE_Task<ACE_MT_SYNCH>
{

public:
    // ����һ��IO����ݽṹ
    typedef struct tagIO_Data
    {
        int32_t                m_nFD;         // ��Ҫ����IO�����ľ��
        IO_Operation       m_Oper;        // IO����
        ACE_Message_Block *m_pData;       // ��ݿռ�
        IO_CallBack_Func   m_Func;        // �ص�����ӿڣ���ΪNULL
        void*              m_pParam;      // �ص���������ΪNULL
    }IO_Data;

public:  // ͨ�ýӿ�
    virtual ~SVS_IO_Scheduler();

    static SVS_IO_Scheduler & instance()
    {
        static SVS_IO_Scheduler svs_io_scheduler;
        return svs_io_scheduler;
    };

    // ��ʼ��
    int32_t open(void*);

    // IO�����߳�
    int32_t svc(void);

    // �رյ�����
    int32_t close(u_long);

    // ����IO
    int32_t add_IO(int32_t fd,
                   int32_t raidIndex,
                   IO_Operation oper,
                   ACE_Message_Block *pData,
                   IO_CallBack_Func pFunc,
                   void *pParam)const;

public: // ����ӿ�

    // ��ȡ������Ϣ
    void statMediaQueueInfo(char * strBuffer, uint32_t nBuffSize);
private:
    // ý���������߳�
    int32_t IO_process_thread(int32_t threadIndex)const;

    // ��ȡ�߳�
    int32_t read_media_data(const tagIO_Data* /*pTagIoData*/)const;

    // дý�����
    int32_t write_media_data(const tagIO_Data* pTagIoData,
            SVS_Media_Output_Stat* pCmediaOutputStat,
            ACE_Message_Block *pTmpfsDataMb)const;

    void output_record_file(const tagIO_Data * pTagIoData,
                            ACE_Message_Block * pTmpfsDataMb,
                            uint32_t& writeDataLen,
                            ssize_t & iSendRet)const;

    // �ر��ļ�
    int32_t close_file(const tagIO_Data * pTagIoData)const;

private:
    SVS_IO_Scheduler();

    //����:����̺߳�
    UTAPI int32_t add_index();

    // ������ת��
    int32_t convert_err_no(const int32_t sysErrNum)const;

    ACE_Message_Block* alloc_mb(uint32_t mbSize)const;

private:

    // ���������
    ACE_Message_Queue <ACE_SYNCH> **m_pOutputQueue;

    ACE_Recursive_Thread_Mutex m_mutex;

    // ���б�־��������false, ��̽�����.
    // �߳�svc()Ӧ�ý���
    bool m_bRunning;

    // �̺߳�
    int32_t m_threadIndex;

    uint32_t m_threadTotal;

    int32_t m_debugWriteFile;
};
#endif /* __SVS_IO_SCHEDULER_H__ */



