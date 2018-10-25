
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

// 由各服务实现的函数
extern "C" const char *getServiceName();
extern "C" void startService();
extern "C" void stopService();


// 程序是否运行在前台参数
bool g_bRunAsBackground = true;
bool g_bServerStarted   = false;


/*****************************************************************************
 函 数 名  : main
 功能描述  : 服务main函数
 输入参数  : int32_t argc
             char* argv[]
 输出参数  : 无
 返 回 值  : int32_t
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2010年12月8日
    作    者   :
    修改内容   : 新生成函数

*****************************************************************************/
int32_t main(int32_t argc, char* argv[])
{
#ifdef UNITTEST
    return utmain(argc, argv);
#endif



    //打印服务信息
    if (argc > 1)
    {
        //printSvrInfo(argc, argv);
        print_server_info(argc, argv, "svs_mu_adapter");
    }

    //读取配置文件中的调试配置项内容
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


    //设置日志等级
    SVS_LOG_LM(logLM);

    //打开／关闭日志开关
    if (logEnable)
    {
        SVS_LOG_ENABLE();
    }
    else
    {
        SVS_LOG_DISABLE();
    }

    /*读取进程工作模式*/
    g_bServerStarted = true;
    svs_run_service(startService,
                    runType,
                    stopService,
                    SVS_MDU_CONFIG_FILE,
                    SERVER_TYPE_MU_STREAM);
    return 0;
}


