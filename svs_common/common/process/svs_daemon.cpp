#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>

#ifndef uint32_t
typedef u_int32_t  uint32_t;

#endif

#ifndef uint64_t
typedef u_int64_t  uint64_t;

#endif

#ifndef uint16_t
typedef u_int16_t uint16_t;
#endif


#include "svs_onlyone_process.h"
#include "svs_daemon.h"
using namespace std;



int32_t g_iCfgDaemonlize = 1;

int32_t g_iReStartTimes  = 0;
uint32_t g_ulReStartTime   = 0;



#define RLIMIT (1024 * 1024)

int32_t setResourceLimit()
{
    struct rlimit limit;
    limit.rlim_cur = RLIMIT;
    limit.rlim_max = RLIMIT;

    (void)setrlimit(RLIMIT_NOFILE, &limit);

    limit.rlim_cur = RLIM_INFINITY;
    limit.rlim_max = RLIM_INFINITY;

    (void)setrlimit(RLIMIT_AS, &limit);
    (void)setrlimit(RLIMIT_CORE, &limit);
    (void)setrlimit(RLIMIT_CPU, &limit);
    (void)setrlimit(RLIMIT_DATA, &limit);
    (void)setrlimit(RLIMIT_FSIZE, &limit);
    (void)setrlimit(RLIMIT_LOCKS, &limit);
    (void)setrlimit(RLIMIT_MEMLOCK, &limit);
    (void)setrlimit(RLIMIT_RSS, &limit);
    (void)setrlimit(RLIMIT_STACK, &limit);
    (void)setrlimit(RLIMIT_NPROC, &limit);

    return SVS_SUCCESS;
}

static pid_t child_pid; //�������̵Ľ���ID

//����Ϊ��
uint32_t SVS_GetAbsTicks ( void )
{
    uint32_t ticks = 0 ;

    struct timeval now;

    (void)gettimeofday(&now, NULL);

    ticks = (uint32_t )now.tv_sec;

    return( ticks );
}



void (*workfunc)();

void (*exitfunc)();

void init_daemon( void (*pWorkFunc)(), void (*pExitFunc)())
{
    workfunc = pWorkFunc;
    exitfunc = pExitFunc;
}

#define WAIT_RELAUNCH 1
#define WAIT_PARENT_KILL 1
#define WAIT_DAEMON 1
#define WAIT_KILL_ALL 10

#define PROCESS_MASK 022

void  rrs_run()
{
    (void)sleep(10);
}

void sigquit_handle(int32_t signum)
{
    syslog(LOG_USER|LOG_WARNING,
        "daemon(pid=%d) recive signal SIGQUIT.\n", getpid());

    pid_t pid;

    signum = signum;

    //���ٽ���SIGCHLD�źţ���ֹ�ظ�����
    (void)signal(SIGCHLD, SIG_IGN);

    (void)system("echo \" fail to start service, please check log files!\" | wall");

    //����������ȫ��ɱ��
    (void)kill(0 - child_pid, SIGTERM);
    (void)usleep(WAIT_KILL_ALL);
    (void)kill(0 - child_pid, SIGKILL);

    //�ͷ��ӽ���ռ�õ�ϵͳ��Դ
    do
    {
        pid = waitpid(-1, NULL, WNOHANG | __WALL);
    }
    while (pid > 0);

    syslog(LOG_USER|LOG_WARNING,
        "daemon(pid=%d) exit.\n", getpid());

    //daemon�Լ�Ҳ�˳�
    exit(1);
}


void sigchld_handle(int32_t signum)
{
    g_iReStartTimes++ ;

    syslog(LOG_USER|LOG_WARNING,
        "daemon(pid=%d) recive signal SIGCHLD "
        "g_iReStartTimes = %d g_ulReStartTime = %lu.\n",
         getpid(), g_iReStartTimes, g_ulReStartTime);

    uint32_t ulTempReStartTime = 0;

    /*10��֮�����������Σ����ػ������˳�*/
    if(g_iReStartTimes > 3)
    {
        ulTempReStartTime = SVS_GetAbsTicks( );

        syslog(LOG_USER|LOG_WARNING,
            "daemon(pid=%d)  g_ulReStartTime = %lu ulTempReStartTime = %lu.\n",
             getpid(), g_ulReStartTime, ulTempReStartTime);

        //��������ʼʱ������10��֮�ڳ���������daemon�����˳�
        if( (ulTempReStartTime - g_ulReStartTime) < 10 )
        {
            syslog(LOG_USER|LOG_WARNING,
                "daemon(pid=%d) exit. "
                "because daemon restarted too many times in time(%lu).\n",
                getpid(), (ulTempReStartTime - g_ulReStartTime));

            //daemon�˳�
            exit(1);
        }

        //�������κ����¼�������ʱ��
        g_iReStartTimes = 0;
        g_ulReStartTime = SVS_GetAbsTicks();

    }

    pid_t pid;

    signum = signum;

    //����������ȫ��ɱ��
    (void)kill(0 - child_pid, SIGTERM);
    (void)usleep(WAIT_KILL_ALL);
    (void)kill(0 - child_pid, SIGKILL);

    //�ͷ��ӽ���ռ�õ�ϵͳ��Դ
    do
    {
        pid = waitpid(-1, NULL, WNOHANG | __WALL);
    }
    while (pid > 0);

    //�Ȼ�ԭSIGCHLD������������fork�������ǽ�ʬ
    (void)signal(SIGCHLD, SIG_DFL);

    //daemon�����������������̣���ʱdaemon���̽���ع�������
    pid = fork();

    switch (pid)
    {
        case -1:
        {
            //forkʧ��
            syslog(LOG_USER|LOG_ERR,
               "Unable to fork worker process, exit.\n");
            exit(1);
            break;
        }
        case 0:
        {
            //�������̣���������

            (void)umask(PROCESS_MASK);

            (void)setpgid(0, getpid());

            (void)signal(SIGCHLD, SIG_DFL);
            (void)signal(SIGQUIT, SIG_IGN);
            (void)signal(SIGPIPE, SIG_IGN);

            workfunc();
            exit(0);
        }
        default:
        {
            //�ػ�����
            child_pid = pid;

            syslog(LOG_USER|LOG_WARNING,
                   "respawed new worker pid is  %d , daemon pid is %d\n",
                    child_pid, getpid());

            //����ע��SIGCHLD������
            (void)signal(SIGCHLD, sigchld_handle);

            //����һ���ٴ�������ٶȣ���ֹ��������
            (void)sleep(WAIT_RELAUNCH);
            break;
        }
    }
}

void send_sigquit_to_deamon()
{
    int32_t enback = 1;
    if (enback == g_iCfgDaemonlize)
    {
        //�������̷�һ��SIGQUIT����ʾ��������һ���˳�
        (void)kill(getppid(), SIGQUIT);

        (void)sleep(WAIT_PARENT_KILL);
    }
    exit(1);
}

int32_t create_daemon( const char* service_conf_path, int32_t service_id )
{
    int32_t fdnull;
    pid_t pid;

    //fork��deamon����
    pid = fork();

    switch (pid)
    {
        case -1:
        {
            //forkʧ��
            printf("Unable to fork()!\n");
            exit(1);
            break;
        }
        case 0:
        {
            //�ӽ��̣�deemon���̣�����������
            break;
        }
        default:
        {
            //daemon���̵ĸ����̣��˳�
            exit(0);
        }
    }

    if(!CAC_Onlyone_Process::onlyone( service_conf_path, service_id))  //��ֻ֤��һ��ʵ������
    {
        printf( "\nA instance is running[%s].\n\n", service_conf_path );
        exit(0);
    }
    (void)setsid();

    fdnull = open("/dev/null", O_RDWR);
    if (fdnull > 0)
    {
        (void)dup2(fdnull, STDIN_FILENO);
        (void)dup2(fdnull, STDOUT_FILENO);
        (void)dup2(fdnull, STDERR_FILENO);
    }

    (void)signal(SIGCHLD, sigchld_handle);
    (void)signal(SIGQUIT, sigquit_handle);

    //deamon�����������������̣���ʱdeamon���̽���ع�������
    pid = fork();

    switch (pid)
    {
        case -1:
        {
            //forkʧ��
            printf("Unable to fork()!\n");
            exit(1);
            break;
        }
        case 0:
        {
            //�ӽ��̣�deemon���̣�����������
            break;
        }
        default:
        {
            //�ػ�����
            child_pid = pid;
            syslog(LOG_USER|LOG_WARNING,
                "create daemon pid=%d, worker pid=%d.\n", getpid(), child_pid);

            //�����ź� ctrl+c
            (void)signal(SIGINT, SIG_IGN);
            (void)signal(SIGPIPE, SIG_IGN);

            g_ulReStartTime = SVS_GetAbsTicks();

            syslog(LOG_USER|LOG_WARNING,
                "g_ulReStartTime =%lu.\n", g_ulReStartTime);

            //(void)signal(SIGCHLD, sigchld_handle);
            //(void)signal(SIGQUIT, sigquit_handle);

            for(; ;)
            {
                //��ѭ��������cpuʹ����
                (void)sleep(WAIT_DAEMON);
            }
        }
    }

    (void)signal(SIGCHLD, SIG_DFL);
    (void)signal(SIGQUIT, SIG_DFL);

    (void)sleep(WAIT_DAEMON);

    (void)umask(PROCESS_MASK);

    (void)setpgid(0, getpid());

    (void)signal(SIGCHLD, SIG_DFL);

    (void)signal(SIGQUIT, SIG_DFL);

    //ִ�й��������е�������
    workfunc();

    return SVS_SUCCESS;
}

typedef void sigfunc(int32_t, siginfo_t *, void *);


void sigprocess(int32_t iSignal, const siginfo_t *pSiginfo, void *procContext)
{
    printf("Receove FPE signal. signal = %d, SIGFPE = %d\n", iSignal, SIGFPE);

    procContext = procContext;

    if (pSiginfo != NULL)
    {
        printf("pSiginfo->si_signo = %d\n", pSiginfo->si_signo);

        printf("pSiginfo->si_errno = %d\n", pSiginfo->si_errno);
    }
    else
    {
        printf("pSiginfo is NULL\n");
    }


    exitfunc();

    return;
}

int32_t sigRegister(int32_t iSigNo, sigfunc *pFunc)
{
    struct sigaction newAct;

    newAct.sa_sigaction = pFunc;
    (void)sigemptyset(&newAct.sa_mask);

    newAct.sa_flags = 0;
    newAct.sa_flags |= SA_SIGINFO;

    if (sigaction(iSigNo, &newAct, NULL) < 0)
    {
        return SVS_FAIL;
    }

    return SVS_SUCCESS;
}

void svs_run_service(void (*pWorkFunc)(),
                          int32_t iRunningMod, void (*pExitFunc)(),
                           const char* service_conf_path, int32_t service_id)
{
    printf("server is starting\n");

    if (SVS_FAIL == setResourceLimit())
    {
        printf("set resource limit failed, abort\n");
        return ;
    }

    g_iCfgDaemonlize = iRunningMod;

     //��ʼ��daemon�� ע��ص�����
    init_daemon(pWorkFunc, pExitFunc);

    if (enBackGround == g_iCfgDaemonlize)
    {

        //��������
        (void)create_daemon( service_conf_path, service_id );
    }
    else
    {
         if(!CAC_Onlyone_Process::onlyone( service_conf_path, service_id))  //��ֻ֤��һ��ʵ������
         {
             printf( "\nA instance is running[%s].\n\n", service_conf_path );
             exit(0);
         }
        if (SVS_SUCCESS != sigRegister(SIGINT, (sigfunc *)sigprocess))
        {
            printf("SigRegister SIGINT fail, abort\n");
            return ;
        }
        workfunc();
    }

    return ;

}



