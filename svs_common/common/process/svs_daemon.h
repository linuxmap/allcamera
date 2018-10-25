#ifndef _SVS_Daemon_h
#define _SVS_Daemon_h

#define SVS_SUCCESS    0
#define SVS_FAIL       -1

typedef enum
{
    enForeGround          = 0, //ǰ̨����
    enBackGround          = 1 //��̨����
}RUNNING_MOD;



/*iRunningMod : 1  �ػ�����ģʽ��0 ǰ̨ģʽ*/
void svs_run_service(    void (*pWorkFunc)(),
                         int32_t iRunningMod,
                         void (*pExitFunc)(),
                         const char* service_conf_path,
            int32_t service_id );
//�˳�
void send_sigquit_to_deamon();

#endif // _SVS_Daemon_h


