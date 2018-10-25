#ifndef _SVS_Debug_Accept_Handler_h
#define _SVS_Debug_Accept_Handler_h
#include "ace/OS.h"
#include "ace/INET_Addr.h"
#include "svs_log_msg.h"
#include "ace/SOCK_Stream.h"
#include "ace/Svc_Handler.h"
//接收发送缓冲区大小
#define SVS_LOG_RECV_BUF_MAX (1024)
#define SVS_LOG_SEND_BUF_MAX (512*1024)

//IP地址长度
#define SVS_ADDR_LEN_MAX 64

//发送接收数据时间(秒)
#define SVS_LOG_RS_TIME 5

class CSVS_Debug_Accept_Handler:public ACE_Svc_Handler<ACE_SOCK_STREAM,ACE_MT_SYNCH>
{
protected:
   enum SVS_DEBUG_CMD_EN
    {
        SVS_DEBUG_CMD_UNFOUND = 99,
        SVS_DEBUG_CMD_MAX
    };
public:
    CSVS_Debug_Accept_Handler();
    CSVS_Debug_Accept_Handler(ACE_Thread_Manager* tm);
    virtual ~CSVS_Debug_Accept_Handler();
public:
    int32_t svc(void);

    int32_t open(void *);

    int32_t handle_input(ACE_HANDLE handle);

    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask mask);
protected:
    //被构造函数调用
    int32_t constructor();

    //发送数据
    int32_t sendData();

    //发送帮助信息
    int32_t sendHelpInfo();

    //发送服务器基本配置
    int32_t sendBasicInfo();

    //发送错误命令消息
    int32_t sendBadCmd();

    //输出调试配置信息
    int32_t sendDebugInfo();

    //发送换行
    int32_t sendSpaceCmd();

    //发送设备信息
    int32_t sendDeviceDebugInfo( const char *pDeviceID );

    // 发送handle信息
    int32_t sendAllHanldeInfo();

    // 发送备份镜头信息
    int32_t sendBakupDeviceDebugInfo(const char * pDeviceID);

    //发送所有设备信息
    int32_t sendAllDeviceDebugInfo();

    //功能: 显示基本参数定义信息
    int32_t sendDefineInfo();

    //发送所有镜头信息
    int32_t sendAllLensDebugInfo();

    //发送缓冲区信息
    int32_t sendBufferDebugInfo();

    // 发送队列信息
    int32_t sendQueueInfo();

    // 发送配置信息
    int32_t sendConfInfo();

    //发送所有镜头详细信息
    int32_t sendAllDeviceDebugDetailInfo();

    // 发送所有前端录像备份镜头信息
    int32_t sendAllBakupDeviceDebugDetialInfo();

    // 显示数据库连接池信息
    int32_t sendDbConnctionDebugInfo();

    // 显示raid组信息
    int32_t sendRaidListInfo();

    // 显示写线程上的镜头负载信息
    int32_t sendWriteThreadInfo();

    int32_t sendCameraListOnRaid();

    int32_t sendResendIndexInfo();

    // 扩展help命令
    int32_t sendExhelpInfo();

    int32_t sendMsgBufferDebugInfo();

    int32_t sendMsgDebugInfo();

    // 显示统计的所有镜头信息
    int32_t sendStatCameraTail();

    // 发送系统信息
    void sendVersion();

    // 删除换行符
    void delChangelineCharacter(string& strCmd)const;

    // 解析"ctrl+c"命令
    bool parseAbortHandle()const;

    // 解析正常的telnet调试命令
    int32_t parseDebugCmd(const string& strCmd);

    // 系统调试信息
    int32_t parseSystemCmd(const string& strCmd);

    // 一般录像命令
    int32_t parseRecordCmd(const string& strCmd);

    // 扩展命令
    int32_t parseExtendCmd(const string& strCmd);

    int32_t parseStatCmd(const string & strCmd);

protected:
    char str_peer_addr_[SVS_ADDR_LEN_MAX];  //连接端IP及端口

    char recv_buf_[SVS_LOG_RECV_BUF_MAX];      //接收数据缓冲区

    char send_buf_[SVS_LOG_SEND_BUF_MAX];      //发送数据缓冲区

    string last_exec_cmd_;
};
#endif



