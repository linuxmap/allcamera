#ifndef _SVS_Daemon_h
#define _SVS_Daemon_h

#define SVS_SUCCESS    0
#define SVS_FAIL       -1

typedef enum
{
    enForeGround          = 0, //前台运行
    enBackGround          = 1 //后台运行
}RUNNING_MOD;



/*iRunningMod : 1  守护进程模式，0 前台模式*/
void svs_run_service(    void (*pWorkFunc)(),
                         int32_t iRunningMod,
                         void (*pExitFunc)(),
                         const char* service_conf_path,
            int32_t service_id );
//退出
void send_sigquit_to_deamon();

#endif // _SVS_Daemon_h


