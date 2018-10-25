#ifndef _SVS_Onlyone_Process_h
#define _SVS_Onlyone_Process_h

const int32_t SEM_PRMS = 0644;//�ź�������Ȩ�ޣ�0644�����û�(����)�ɶ�д�����Ա��������Ա�ɶ�����д

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
    * �������Ƿ���Ҫ��������.
    * �����Ҫ������������ô����true�����򷵻�false.
    */
    static bool need_restart(const char *strFileName, int32_t key=0);
protected:
    int32_t init(const char *strFileName, int32_t key);
    bool exists();  //����ź����Ƿ��Ѵ���
    bool mark();    //�����ź���
    bool unmark();  //����ź���
private:
    key_t key_;
    int32_t sem_id_;
};

#endif //_SVS_Onlyone_Process_h

