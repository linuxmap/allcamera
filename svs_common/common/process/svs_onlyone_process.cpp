#include "ace/OS.h"
#include "ace/OS_NS_Thread.h"
#include "svs_onlyone_process.h"
#include "svs_log_msg.h"

#ifdef WIN32
#include "Winbase.h"
#include "Windows.h"
#endif

CAC_Onlyone_Process::CAC_Onlyone_Process()
{
    this->key_ = 0;
    sem_id_ = -1;
}

CAC_Onlyone_Process::~CAC_Onlyone_Process()
{
}


bool CAC_Onlyone_Process::onlyone(const char *strFileName,int32_t key)
{
#ifdef WIN32
    SECURITY_ATTRIBUTES eventAtrributes;
    eventAtrributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    eventAtrributes.bInheritHandle = FALSE;
    eventAtrributes.lpSecurityDescriptor = NULL;

    HANDLE handle = ::CreateEvent(&eventAtrributes, TRUE, FALSE, strFileName);
    if(NULL == handle || (ERROR_ALREADY_EXISTS == ::GetLastError()))
    {
        SVS_LOG((SVS_LM_ERROR, "A instance is running"));
        return false;
    }

    return true;
#else
    CAC_Onlyone_Process onlyoneProcess;
    if(0 != onlyoneProcess.init(strFileName,key))
    return false;

    if(onlyoneProcess.exists())
    {
        SVS_LOG((SVS_LM_ERROR, "A instance is running, semaphore ID[%d].", onlyoneProcess.sem_id_));
        return false;
    }

    if (!onlyoneProcess.mark())
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to create semaphore to avoid re-run, semaphore ID[%d].", onlyoneProcess.sem_id_));
        return false;
    }
#endif

    return true;
}

int32_t CAC_Onlyone_Process::init(const char *strFileName,int32_t key)
{
#ifndef WIN32
    const char *fileName = "/dev";
    if(-1 == (key_ = ftok(fileName, key)))
    {
        SVS_LOG((SVS_LM_ERROR,"Fail to create semaphore key by configfile[%s]", strFileName));
        ACE_OS::exit(1);
    }
#endif
    return 0;
}

bool CAC_Onlyone_Process::exists()
{
    sem_id_ = ACE_OS::semget(key_, 0, 0);
    if (sem_id_ == -1)
    {
        return false;
    }

    semun semctl_arg;
    semctl_arg.array = NULL;

    return ACE_OS::semctl(sem_id_, 0, GETVAL, semctl_arg) > 0;
}

bool CAC_Onlyone_Process::mark()
{
    sem_id_ = ACE_OS::semget(key_, 0, 0);
    if (sem_id_ == -1)
    {
        sem_id_ = ACE_OS::semget(key_, 1, IPC_CREAT | IPC_EXCL | SEM_PRMS);
    }

    struct sembuf buf[2];

    buf[0].sem_num = 0;
    buf[0].sem_op = 0;
    buf[0].sem_flg = IPC_NOWAIT;
    buf[1].sem_num = 0;
    buf[1].sem_op = 1;
    buf[1].sem_flg = SEM_UNDO;//进程退出时自动回滚

    return ACE_OS::semop(sem_id_, &buf[0], 2) == 0;
}

bool CAC_Onlyone_Process::unmark()
{
    sem_id_ = ACE_OS::semget(key_, 0, 0);
    if (sem_id_ == -1)
    {
        return true;
    }

    semun semctl_arg;
    semctl_arg.val = 0;
    return ACE_OS::semctl(sem_id_, 0, IPC_RMID, semctl_arg) == 0;
}

bool CAC_Onlyone_Process::need_restart(const char *strFileName, int32_t key)
{
    CAC_Onlyone_Process onlyoneProcess;
    if(0 != onlyoneProcess.init(strFileName,key))
    {
        return false;
    }

    if(onlyoneProcess.exists())
    {
        SVS_LOG((SVS_LM_ERROR,"A instance is running, semaphore ID[%d].", onlyoneProcess.sem_id_));
        return false;
    }

    return true;
}


