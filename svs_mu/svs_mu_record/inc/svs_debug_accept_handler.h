#ifndef _SVS_Debug_Accept_Handler_h
#define _SVS_Debug_Accept_Handler_h
#include "ace/OS.h"
#include "ace/INET_Addr.h"
#include "svs_log_msg.h"
#include "ace/SOCK_Stream.h"
#include "ace/Svc_Handler.h"
//���շ��ͻ�������С
#define SVS_LOG_RECV_BUF_MAX (1024)
#define SVS_LOG_SEND_BUF_MAX (512*1024)

//IP��ַ����
#define SVS_ADDR_LEN_MAX 64

//���ͽ�������ʱ��(��)
#define SVS_LOG_RS_TIME 5

class CSVS_Debug_Accept_Handler:public ACE_Svc_Handler<ACE_SOCK_STREAM,ACE_MT_SYNCH>
{
protected:
   enum SVS_DEBUG_CMD_EN
    {
        SVS_DEBUG_CMD_UNFOUND = 99,
        SVS_DEBUG_CMD_MAX
    };
public:
    CSVS_Debug_Accept_Handler();
    CSVS_Debug_Accept_Handler(ACE_Thread_Manager* tm);
    virtual ~CSVS_Debug_Accept_Handler();
public:
    int32_t svc(void);

    int32_t open(void *);

    int32_t handle_input(ACE_HANDLE handle);

    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask mask);
protected:
    //�����캯������
    int32_t constructor();

    //��������
    int32_t sendData();

    //���Ͱ�����Ϣ
    int32_t sendHelpInfo();

    //���ͷ�������������
    int32_t sendBasicInfo();

    //���ʹ���������Ϣ
    int32_t sendBadCmd();

    //�������������Ϣ
    int32_t sendDebugInfo();

    //���ͻ���
    int32_t sendSpaceCmd();

    //�����豸��Ϣ
    int32_t sendDeviceDebugInfo( const char *pDeviceID );

    // ����handle��Ϣ
    int32_t sendAllHanldeInfo();

    // ���ͱ��ݾ�ͷ��Ϣ
    int32_t sendBakupDeviceDebugInfo(const char * pDeviceID);

    //���������豸��Ϣ
    int32_t sendAllDeviceDebugInfo();

    //����: ��ʾ��������������Ϣ
    int32_t sendDefineInfo();

    //�������о�ͷ��Ϣ
    int32_t sendAllLensDebugInfo();

    //���ͻ�������Ϣ
    int32_t sendBufferDebugInfo();

    // ���Ͷ�����Ϣ
    int32_t sendQueueInfo();

    // ����������Ϣ
    int32_t sendConfInfo();

    //�������о�ͷ��ϸ��Ϣ
    int32_t sendAllDeviceDebugDetailInfo();

    // ��������ǰ��¼�񱸷ݾ�ͷ��Ϣ
    int32_t sendAllBakupDeviceDebugDetialInfo();

    // ��ʾ���ݿ����ӳ���Ϣ
    int32_t sendDbConnctionDebugInfo();

    // ��ʾraid����Ϣ
    int32_t sendRaidListInfo();

    // ��ʾд�߳��ϵľ�ͷ������Ϣ
    int32_t sendWriteThreadInfo();

    int32_t sendCameraListOnRaid();

    int32_t sendResendIndexInfo();

    // ��չhelp����
    int32_t sendExhelpInfo();

    int32_t sendMsgBufferDebugInfo();

    int32_t sendMsgDebugInfo();

    // ��ʾͳ�Ƶ����о�ͷ��Ϣ
    int32_t sendStatCameraTail();

    // ����ϵͳ��Ϣ
    void sendVersion();

    // ɾ�����з�
    void delChangelineCharacter(string& strCmd)const;

    // ����"ctrl+c"����
    bool parseAbortHandle()const;

    // ����������telnet��������
    int32_t parseDebugCmd(const string& strCmd);

    // ϵͳ������Ϣ
    int32_t parseSystemCmd(const string& strCmd);

    // һ��¼������
    int32_t parseRecordCmd(const string& strCmd);

    // ��չ����
    int32_t parseExtendCmd(const string& strCmd);

    int32_t parseStatCmd(const string & strCmd);

protected:
    char str_peer_addr_[SVS_ADDR_LEN_MAX];  //���Ӷ�IP���˿�

    char recv_buf_[SVS_LOG_RECV_BUF_MAX];      //�������ݻ�����

    char send_buf_[SVS_LOG_SEND_BUF_MAX];      //�������ݻ�����

    string last_exec_cmd_;
};
#endif



