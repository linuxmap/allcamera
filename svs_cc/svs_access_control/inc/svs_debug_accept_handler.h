#ifndef _SVS_Debug_Accept_Handler_h
#define _SVS_Debug_Accept_Handler_h

#include <string>
#include "svs_ace_header.h"
#include "svs_ac_common.h"

#define SVS_LOG_BUF_MAX 20480
#define MAX_DEBUG_CLIENT_NUMBER  10
#define DEBUG_HEARTBEAT_SEND_INTERVAL  (20 * 60)
#define WAIT_TIME_1SECONDS            1
#define WAIT_TIME_5SECONDS            5
#define WAIT_TIME_10SECONDS           10
#define WAIT_TIME_30SECONDS           30
#define WAIT_TIME_100SECONDS          100
#define WAIT_TIME_130SECONDS          130
#define RECONNECT_TIMEOUT             10

const uint8_t LINUX_CTRL_C[] = {0xFF, 0xF4, 0xFF, 0xFD, 0x06};
const uint8_t WINDOWS_CTRL_C = 0X03;






class CSVS_Debug_Accept_Handler : public ACE_Svc_Handler<ACE_SOCK_STREAM,ACE_MT_SYNCH>
{
public:
    CSVS_Debug_Accept_Handler();
    CSVS_Debug_Accept_Handler(ACE_Thread_Manager* tm);
    virtual ~CSVS_Debug_Accept_Handler();

    int32_t svc(void);
    int32_t open(void*);
    int32_t processData1(std::string strCmd);
    int32_t processData2(std::string &strCmd);
    int32_t handle_input(ACE_HANDLE handle);
    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask mask);
    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);

protected:
    int32_t sendQueueInfo();
    int32_t sendVersion();
    int32_t constructor();

    int32_t sendData();
    int32_t sendData(const char *pszData);

    int32_t sendHelpInfo();
    int32_t sendBasicInfo();

    int32_t sendBadCmd();
    int32_t sendSpaceCmd();

    int32_t sendDeviceInfo(const std::string &strID);
    int32_t sendLensInfo(const std::string &strID);
    int32_t sendStreamSvrInfo(const std::string &strID);
    int32_t sendSessionInfo(const std::string &strID);
private:
    int32_t findSubString(std::string& strSource, std::string strSub);
protected:
    char str_peer_addr_[SVS_IP_LEN];         //连接端IP及端口
    char recv_buf_[SVS_LOG_BUF_MAX];      //接收数据缓冲区
    char send_buf_[SVS_LOG_BUF_MAX];      //发送数据缓冲区

private:
    static int32_t m_client_number;
    int32_t m_index;
    int32_t m_timerId;
    u_int m_nWaitHeartBeatCount;
    string last_exec_cmd_;  // 上次执行的命令
};
#endif

