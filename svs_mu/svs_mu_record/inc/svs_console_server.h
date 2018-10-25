/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : SVS_Console_Server.h
  �� �� ��   : ����
  ��������   : 2010��3��10��
  ����޸�   :
  ��������   : �����̨�����ӹ�����Ϣ���պͷ��͡�
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2010��3��10��
    �޸�����   : �����ļ�

******************************************************************************/
#ifndef __SVS_CONSOLE_SERVER_H__
#define __SVS_CONSOLE_SERVER_H__
#include "svs_rt_client_handler.h"

// ģ�����ͣ������̨ע����
#define SVS_MRU_RECORD_MODULE_TYPE 6060420

ACE_Message_Block* allocate_mb(uint32_t ulMbSize);

// �����ࡣ��������Console���̳�CSVS_Client_Handler,ֻ��д����಻�õļ�������
class SVS_Console_Client_Handler:public CSVS_Client_Handler
{
public:
    SVS_Console_Client_Handler( ACE_Reactor *pReactor,
                        ACE_Message_Queue<ACE_SYNCH> *pMsgQueue);
    virtual ~SVS_Console_Client_Handler();

public:
    // ����
    virtual int32_t reconnect();

    // �Ͽ�����
    virtual int32_t disconnection();

    // ����ע����Ϣ
    virtual int32_t send_register_msg();

    // ����ע����Ӧ��Ϣ
    virtual int32_t handle_register_response_msg(const SVS_HEADER * pMsgHeader);

    // �ͷ���Ϣ�飬�����ڻ��࣬��ϵͳ�ڴ�����
    virtual ACE_Message_Block* alloc_mb(uint32_t mbMsgLen, uint8_t mbType)const;

    // ������Ϣ�飬�����ڻ��࣬���ڴ��ͷŸ�ϵͳ
    virtual void free_mb(ACE_Message_Block* mb)const;


};

// �Խӿ���̨ҵ������ࡣ
class SVS_Console_Server :public ACE_Task <ACE_MT_SYNCH>
{
public:
    enum SVS_CONSOLE_THREAD_INDEX
    {
        // �����߳�
        SVS_CONSOLE_CONNECT_THREAD = 0,

        // ��ʱ���ص��������߳�
        SVS_TIMER_CALLBACK_THREAD,

        // ����̨��Ϣ�����߳�
        SVS_HANDLE_MESSAGE_THREAD,

        SVS_CONSOLE_MAX_THREAD
    };

public: // ͨ�ýӿ�
    virtual ~SVS_Console_Server();

    // ����
    static SVS_Console_Server *instance()
    {
        static SVS_Console_Server szServerManager;
        return &szServerManager;
    }

    // ��ʼ��
    int32_t open(void*);

    // �߳�
    int32_t svc(void);

    // �رյ�����
    int32_t close(u_long);

public: // ������ؽӿ�
    int32_t initConfig(const char* strConfigPath);

    // ����̨IP
    const char* strConsoleIp()const
    {
        return m_strConsoleIp;
    }

    // ����̨�����˿�
    const uint16_t consolePort()const
    {
        return m_uConsolePort;
    }

    // ���ӿ���̨�ı���IP
    const char* strLanIp()const
    {
        return m_strLanIp;
    }

    // ���ӿ���̨�ı��ض˿�
    const uint16_t lanPort()const
    {
        return m_uLanPort;
    }

    // �����̨��ע������
    const SVS_CONSOLE_REG_REQ_MSG* regInfo()const
    {
        return &m_szRegMsg;
    }

public: // ¼����������ӿ�
    // ������Ϣ�ص�
    int32_t sendMsg(ACE_Message_Block *mb);

    // ��ʱ�����������
    int32_t addTimerCallbackMsg(ACE_Message_Block * msg);

private:
    // ����ģʽ���࣬���캯������
    SVS_Console_Server();

    // ��������Console�߳�
    int32_t startConnect();

    // ��ʱ���������߳�
    int32_t timerCallbackThread();

    // ��Ϣ�����߳�
    int32_t handleMessageThread();

    // ��Ϣ������
    int32_t handleMessage(const ACE_Message_Block * mb);

    // �߳������������ƺ���
    int32_t addIndex()
    {
        int32_t oldIndex;
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_RESULT_FAILURE);
        oldIndex = m_nThreadIndex++;
        return oldIndex;
    }

private:
    // ��������״̬
    volatile bool m_bRunning;

    // ���ڲ����������߳�ʱ�����߳�ID��
    ACE_Recursive_Thread_Mutex m_mutex;

    // �߳����
    int32_t m_nThreadIndex;

    // ���ӷ�Ӧ������������Control
    ACE_Reactor *m_pConnectReactor;

    // ����epoll��Ӧ������������Control
    ACE_Dev_Poll_Reactor* m_pConnectEpollReactor;

    // �Ƿ�����Console�Ŀ��أ�1:off; 0:on
    uint32_t m_ulConnectConsoleSwitch;

    // ��Console���ӵ�������
    SVS_Console_Client_Handler *m_pClientHandler;

    // Console��IP�Ͷ˿�
    char m_strConsoleIp[SVS_IP_LEN];
    uint16_t m_uConsolePort;

    // ����Console�ı���IP�Ͷ˿�
    char m_strLanIp[SVS_IP_LEN];
    uint16_t m_uLanPort;

    // ע����Ϣ
    SVS_CONSOLE_REG_REQ_MSG m_szRegMsg;

    // ��ʱ���������
    ACE_Message_Queue <ACE_SYNCH> m_timerCallbackQueue;
};
#endif // _SVS_CONTROL_SERVER_H_


