/******************************************************************************

                  版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文 件 名   : SVS_Console_Server.h
  版 本 号   : 初稿
  生成日期   : 2010年3月10日
  最近修改   :
  功能描述   : 与控制台的连接管理，消息接收和发送。
  函数列表   :
  修改历史   :
  1.日    期   : 2010年3月10日
    修改内容   : 创建文件

******************************************************************************/
#ifndef __SVS_CONSOLE_SERVER_H__
#define __SVS_CONSOLE_SERVER_H__
#include "svs_rt_client_handler.h"

// 模块类型，向控制台注册用
#define SVS_MRU_RECORD_MODULE_TYPE 6060420

ACE_Message_Block* allocate_mb(uint32_t ulMbSize);

// 连接类。用于连接Console，继承CSVS_Client_Handler,只重写与基类不用的几个函数
class SVS_Console_Client_Handler:public CSVS_Client_Handler
{
public:
    SVS_Console_Client_Handler( ACE_Reactor *pReactor,
                        ACE_Message_Queue<ACE_SYNCH> *pMsgQueue);
    virtual ~SVS_Console_Client_Handler();

public:
    // 重连
    virtual int32_t reconnect();

    // 断开连接
    virtual int32_t disconnection();

    // 发送注册消息
    virtual int32_t send_register_msg();

    // 处理注册相应消息
    virtual int32_t handle_register_response_msg(const SVS_HEADER * pMsgHeader);

    // 释放消息块，区别于基类，从系统内存申请
    virtual ACE_Message_Block* alloc_mb(uint32_t mbMsgLen, uint8_t mbType)const;

    // 申请消息块，区别于基类，把内存释放给系统
    virtual void free_mb(ACE_Message_Block* mb)const;


};

// 对接控制台业务控制类。
class SVS_Console_Server :public ACE_Task <ACE_MT_SYNCH>
{
public:
    enum SVS_CONSOLE_THREAD_INDEX
    {
        // 连接线程
        SVS_CONSOLE_CONNECT_THREAD = 0,

        // 定时器回调任务处理线程
        SVS_TIMER_CALLBACK_THREAD,

        // 控制台消息处理线程
        SVS_HANDLE_MESSAGE_THREAD,

        SVS_CONSOLE_MAX_THREAD
    };

public: // 通用接口
    virtual ~SVS_Console_Server();

    // 单例
    static SVS_Console_Server *instance()
    {
        static SVS_Console_Server szServerManager;
        return &szServerManager;
    }

    // 初始化
    int32_t open(void*);

    // 线程
    int32_t svc(void);

    // 关闭调度器
    int32_t close(u_long);

public: // 配置相关接口
    int32_t initConfig(const char* strConfigPath);

    // 控制台IP
    const char* strConsoleIp()const
    {
        return m_strConsoleIp;
    }

    // 控制台监听端口
    const uint16_t consolePort()const
    {
        return m_uConsolePort;
    }

    // 连接控制台的本地IP
    const char* strLanIp()const
    {
        return m_strLanIp;
    }

    // 连接控制台的本地端口
    const uint16_t lanPort()const
    {
        return m_uLanPort;
    }

    // 向控制台的注册内容
    const SVS_CONSOLE_REG_REQ_MSG* regInfo()const
    {
        return &m_szRegMsg;
    }

public: // 录像质量管理接口
    // 心跳消息回调
    int32_t sendMsg(ACE_Message_Block *mb);

    // 定时器任务如队列
    int32_t addTimerCallbackMsg(ACE_Message_Block * msg);

private:
    // 单例模式的类，构造函数隐藏
    SVS_Console_Server();

    // 启动连接Console线程
    int32_t startConnect();

    // 定时器任务处理线程
    int32_t timerCallbackThread();

    // 消息处理线程
    int32_t handleMessageThread();

    // 消息处理函数
    int32_t handleMessage(const ACE_Message_Block * mb);

    // 线程索引递增控制函数
    int32_t addIndex()
    {
        int32_t oldIndex;
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_RESULT_FAILURE);
        oldIndex = m_nThreadIndex++;
        return oldIndex;
    }

private:
    // 服务运行状态
    volatile bool m_bRunning;

    // 类内部锁，启动线程时生成线程ID用
    ACE_Recursive_Thread_Mutex m_mutex;

    // 线程序号
    int32_t m_nThreadIndex;

    // 连接反应器，用于连接Control
    ACE_Reactor *m_pConnectReactor;

    // 连接epoll反应器，用于连接Control
    ACE_Dev_Poll_Reactor* m_pConnectEpollReactor;

    // 是否连接Console的开关，1:off; 0:on
    uint32_t m_ulConnectConsoleSwitch;

    // 与Console连接的连接器
    SVS_Console_Client_Handler *m_pClientHandler;

    // Console的IP和端口
    char m_strConsoleIp[SVS_IP_LEN];
    uint16_t m_uConsolePort;

    // 连接Console的本地IP和端口
    char m_strLanIp[SVS_IP_LEN];
    uint16_t m_uLanPort;

    // 注册消息
    SVS_CONSOLE_REG_REQ_MSG m_szRegMsg;

    // 定时器任务队列
    ACE_Message_Queue <ACE_SYNCH> m_timerCallbackQueue;
};
#endif // _SVS_CONTROL_SERVER_H_


