#ifndef _SVS_Onlyone_Process_h
#define _SVS_Onlyone_Process_h

const int32_t SEM_PRMS = 0644;//信号量操作权限，0644即主用户(属主)可读写、组成员及其它成员可读不可写

class CAC_Onlyone_Process
{
protected:
    CAC_Onlyone_Process();
    ~CAC_Onlyone_Process();
    CAC_Onlyone_Process(const CAC_Onlyone_Process& obj);
    CAC_Onlyone_Process& operator=(const CAC_Onlyone_Process& obj);
public:
    static bool onlyone(const char *strFileName,int32_t key =0);

    /**
    * 检查进程是否需要重新启动.
    * 如果需要重新启动，那么返回true，否则返回false.
    */
    static bool need_restart(const char *strFileName, int32_t key=0);
protected:
    int32_t init(const char *strFileName, int32_t key);
    bool exists();  //检查信号量是否已存在
    bool mark();    //设置信号量
    bool unmark();  //清除信号量
private:
    key_t key_;
    int32_t sem_id_;
};

#endif //_SVS_Onlyone_Process_h

