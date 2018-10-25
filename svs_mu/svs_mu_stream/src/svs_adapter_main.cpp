
#include <sys/resource.h>

#include "svs_ace_header.h"
#include "svs_log_msg.h"
#include "svs_adapter_svs_def.h"
#include "svs_adapter_def.h"
#include "vms/vms.h"
#include "svs_config.h"
#include "svs_print_version.h"
#include "svs_utility.h"
#include "svs_logging_strategy.h"
#include "svs_log_msg.h"
#include "svs_daemon.h"



#ifdef UNITTEST
extern "C" int32_t utmain(int32_t argc, char* argv[]);
#endif

// �ɸ�����ʵ�ֵĺ���
extern "C" const char *getServiceName();
extern "C" void startService();
extern "C" void stopService();


// �����Ƿ�������ǰ̨����
bool g_bRunAsBackground = true;
bool g_bServerStarted   = false;


/*****************************************************************************
 �� �� ��  : main
 ��������  : ����main����
 �������  : int32_t argc
             char* argv[]
 �������  : ��
 �� �� ֵ  : int32_t
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2010��12��8��
    ��    ��   :
    �޸�����   : �����ɺ���

*****************************************************************************/
int32_t main(int32_t argc, char* argv[])
{
#ifdef UNITTEST
    return utmain(argc, argv);
#endif



    //��ӡ������Ϣ
    if (argc > 1)
    {
        //printSvrInfo(argc, argv);
        print_server_info(argc, argv, "svs_mu_adapter");
    }

    //��ȡ�����ļ��еĵ�������������
    int32_t runType;
    int32_t logEnable;
    int32_t logLM;
    if (0 != getDebugConfig(SVS_MDU_CONFIG_FILE, runType, logEnable, logLM))
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Readed debug config info in the configuration file failed,"
            "quit the service. configfile[%s].",
            SVS_MDU_CONFIG_FILE));

        return -1;
    }


    //������־�ȼ�
    SVS_LOG_LM(logLM);

    //�򿪣��ر���־����
    if (logEnable)
    {
        SVS_LOG_ENABLE();
    }
    else
    {
        SVS_LOG_DISABLE();
    }

    /*��ȡ���̹���ģʽ*/
    g_bServerStarted = true;
    svs_run_service(startService,
                    runType,
                    stopService,
                    SVS_MDU_CONFIG_FILE,
                    SERVER_TYPE_MU_STREAM);
    return 0;
}


