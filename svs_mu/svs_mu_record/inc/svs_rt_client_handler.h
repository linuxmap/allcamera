#ifndef _SVS_RT_Client_Handler_h
#define _SVS_RT_Client_Handler_h
#include "svs_rt_record_common.h"
#include "svs_stat_manager.h"
class CSVS_Client_Handler  :public ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_MT_SYNCH>
{
public:
    /* 状态机*/
    // 连接StoreSvr的状态
    enum RT_CONN_STORE_STATUS
    {
        RT_CONN_STORE_INIT = 0,
        RT_CONN_STORE_DISCONNECTED,
        RT_CONN_STORE_CONNECTING,
        RT_CONN_STORE_LOGGING,
        RT_CONN_STORE_READY,
        RT_CONN_STORE_EXIT
    };

    // 连接器线程
    enum RT_HANDLE_CLIENT_THREAD
    {
        RT_HANDLE_CLIENT_SEND_MSG_THREAD = 0,
        RT_HANDLE_CLIENT_CHECK_CONNECT_THREAD,
        RT_HANDLE_CLIENT_MAX_THREAD
    };

    // 数据解析结果
    enum HANDLE_RECV_DATA_RET
    {
        HANDLE_RECV_DATA_ERROR = -1,
        HANDLE_RECV_DATA_SUCCESS = 0,
        HANDLE_RECV_DATA_CONTINUE = 1,
        HANDLE_RECV_DATA_BREAK = 2,
        HANDLE_RECV_DATA_RET_MAX
    };


    enum
    {
        SVS_SERVER_TYPE_STORESVR    = 0,
        SVS_SERVER_TYPE_CONSOLE     = 1,
    };
    typedef ACE_Connector<CSVS_Client_Handler, ACE_SOCK_CONNECTOR> MyConnector;
public:
    CSVS_Client_Handler();
    CSVS_Client_Handler( ACE_Reactor *pReactor,
                        ACE_Message_Queue<ACE_SYNCH> *pMsgQueue,
                        uint32_t nServerType = SVS_SERVER_TYPE_STORESVR);

    virtual ~CSVS_Client_Handler();

    // 初始化
    virtual int32_t open(void *pArg);

    // 接收信令数据
    virtual int32_t handle_input(ACE_HANDLE handle);

    // 定时器回调
    virtual int32_t handle_timeout(const ACE_Time_Value & /*currentTime*/, const void * /*act = 0*/);

    // 当关闭本连接后，反应器回调该函数
    virtual int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask mask);

    // 当在open方法中激活(activate)了服务后，便启动线程。
    virtual int32_t svc(void);

   // 发送消息
   virtual int32_t send_msg(ACE_Message_Block * mb, ACE_Time_Value * timeout = 0);

    // 连接状态是否为正常连接状态
    virtual bool is_ready_status()const
    {
        return (RT_CONN_STORE_READY == conn_status_);
    }

    virtual int32_t conn_status()const
    {
        return conn_status_;
    }

    // 释放消息块
    virtual ACE_Message_Block* alloc_mb(uint32_t mbMsgLen, uint8_t mbType)const;

    // 申请消息块
    virtual void free_mb(ACE_Message_Block* mb)const;
protected:
    // 消息信令数据解析
    virtual int32_t parse_data();

    // 初始化成员变量
    virtual int32_t construct();

    // 设置socket
    virtual void set_socket_option()const;

    // 发送监控事件
    virtual int32_t send_monitor_event(int32_t monitorEventType);

    // 发送消息线程
    virtual int32_t send_message_thread();

    // 与StoreSvr的连接状态检测线程

    virtual int32_t check_connection_status_thread();

    // 设置连接状态
    virtual int32_t set_conn_status(int32_t newStatus);

    // 生成具体消息
    virtual ACE_Message_Block* build_msg( const SVS_HEADER* pMsgHeader, uint8_t mbType ) const;

    // 消息列表加入队列等待处理
    virtual int32_t equeue_msg(ACE_Message_Block * mbBatch, ACE_Time_Value tv);

    // 检查消息数据头和长度合法性
    virtual int32_t check_data(const uint32_t procDataPos,
                        uint32_t & nextDataPos,
                        SVS_HEADER *& pMsgHeader);

    // 更新数据缓存区信息
    virtual int32_t update_buffer_cursor(uint32_t procDataPos, uint32_t nextDataPos);

    // 发送注册消息
    virtual int32_t send_register_msg();

    // 处理注册相应消息
    virtual int32_t handle_register_response_msg(const SVS_HEADER * pMsgHeader);

    // 重新连接服务器：仅发送重连事件到事件队列中.
    virtual int32_t reconnect(void);

    // 重新向StoreSvr服务器注册
    virtual int32_t register2server();

    // 断开连接
    virtual int32_t disconnection(void);

    //功能:生成线程号
    virtual int32_t add_index()
    {
        int32_t oldIndex;
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, SVS_RESULT_FAILURE);
        oldIndex = thr_index_++;
        return oldIndex;
    }

protected:
    ACE_Recursive_Thread_Mutex mutex_;

    // 运行标志，如果等于false, 进程将结束.线程svc()应该结束
    bool brunning_;

    // 线程号
    int32_t thr_index_;
    // 信令消息队列
    ACE_Message_Queue<ACE_SYNCH> *p_signal_queue_;

    // 重连失败次数
    int16_t connect_fail_count_;

    // 心跳超时次数
    volatile uint32_t heartbeat_timeout_times_;

    // 注册时间，用于注册超时判断
    volatile time_t new_loggin_time_;

    // 应用层心跳消息传输号
    volatile uint16_t heartbeat_transaction_no_;

    // 接收数据长度
    uint32_t  recv_data_len_;

    // 接收数据缓冲区
    char recv_data_buffer_[RECV_DATA_BUFF_MAX_LEN];

    // 发送消息缓冲区
    ACE_Message_Block *send_msg_buffer_;

    // 与StoreSvr的连接状态
    volatile int32_t conn_status_;

    // 录像状态
    volatile int32_t record_status_;

    // 统计信息
    SVS_Signal_Message_Stat signal_msg_stat_;

    ACE_INET_Addr local_addr_;
    ACE_INET_Addr remote_addr_;

    // 服务器类型
    uint32_t server_type_;
};
#endif // _SVS_Client_Handler_h


