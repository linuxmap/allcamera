#ifndef __CMDUDEBUGGER_H__
#define __CMDUDEBUGGER_H__

#include <svs_ace_header.h>
#include <map>
#include <string>
#include <vms/vms.h>





class CMduDebugger:public ACE_Svc_Handler<ACE_SOCK_STREAM,ACE_MT_SYNCH>
{
#define MDU_MAX_DEBUG_NUM              10
#define MDU_DEBUG_CMD_LEN              128
#define MDU_DEBUG_INFO_LEN             2048

#define SVS_ADDR_LEN_MAX  32
#define SVS_LOG_RECV_TIMEOUT 5
#define SVS_LOG_SEND_TIMEOUT 5
#define SVS_ROUTER_MAX    5120

#define SVS_CMD_PROMPT                  "\n\nsvs_mu_stream-->"
#define SVS_CMD_SER_NAME                "svs_mu_stream"

#define SVS_CMD_SPACE                   ""

#define FIRST_LEVEL_CMD_HELP            "help"
#define FIRST_LEVEL_CMD_SHOW            "show"
#define FIRST_LEVEL_CMD_QUIT            "quit"
#define FIRST_LEVEL_CMD_EXIT            "exit"


#define SECOND_LEVEL_CMD_DEBUG          "debug"
#define SECOND_LEVEL_CMD_SESSION        "session"
#define SECOND_LEVEL_CMD_BUSINESS       "business"
#define SECOND_LEVEL_CMD_PORT           "port"
#define SECOND_LEVEL_CMD_DEVICE         "device"
#define SECOND_LEVEL_CMD_STREAM         "stream"
#define SECOND_LEVEL_CMD_BUFFER         "buffer"
#define SECOND_LEVEL_CMD_SDP            "sdp"



#define THIRD_LEVEL_CMD_COUNT           "count"


#define THIRD_LEVEL_CMD_PORT_PAIR       "pair"


#define DEBUG_HEARTBEAT_SEND_INTERVAL   (20 * 60)

protected:

#define SE 0xF0
#define SB 0xFA
#define IAC 0xFF
#define WILL 0xFB
#define DO 0xFD
#define WONT 0xFC
#define DONT 0xFE


#define Techo 0x01
#define TGA 0x03
#define TType 0x18
#define TWindow 0x1F
#define TSpeed 0x20
#define TFlower 0x21
#define TLine 0x22
#define TEnviron 0x24

    enum SVS_DEBUG_Login
    {
        Login_SendVersion,
        Login_WaitInputName,
        Login_WaitInputPassword,
        Login_HasLogin,
        Login_MAX
    };
public:
    CMduDebugger();
    virtual ~CMduDebugger();

    int32_t svc(void);
    int32_t open(void *);
    int32_t handle_input(ACE_HANDLE handle);
    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask mask);

    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);


protected:

    void constructor();

    int32_t sendData();

    int32_t sendBadCmd();

    int32_t sendHelpInfo(std::string& strCmd);

    int32_t exit(std::string& strCmd);

    int32_t handleShowCmd(std::string& strCmd);

    int32_t sendBasicInfo(std::string& strCmd);

    int32_t sendDebugInfo(std::string& strCmd);

    int32_t sendSpaceCmd(std::string& strCmd);

    int32_t sendBufferInfo(std::string& strCmd);

    int32_t handleShowSessionCmd(std::string& strCmd);

    int32_t handleShowSdpCmd(std::string& strCmd);

    int32_t handleShowBusinessCmd(std::string& strCmd);

    int32_t sendSessionCnt(std::string& strCmd);

    int32_t handleFirstLevelCmd(std::string &strCmd);

    void sendVersion();

    void sendNewLine();
    void sendPrompt();

    void sendLogFailMsg();

    void sendSessionInfo(uint64_svs ullStreamId);

    void sendSessionsdp(uint64_svs ullStreamId);

    int32_t sendAllSessionInfo(std::string &strCmd);

    int32_t sendBusinessCnt(std::string& strCmd);

    void sendBusinessInfo(uint64_svs ullStreamId);

    int32_t sendAllBusinessInfo(std::string &strCmd);

    void parseCmd(std::string& strCmd, std::string& strSubCmd) const;

    int32_t sendNegotiate();
    int32_t sendIAC(uint8_t cmd,uint8_t option);

    typedef int32_t (CMduDebugger::*pMsgProcessor)(std::string& strCmd);
    typedef std::map<std::string, pMsgProcessor> MessageMap;
    typedef MessageMap::iterator                 MessageIter;

protected:
    char m_szInetAddr[SVS_ADDR_LEN_MAX];
    char m_szRecvBuf[MDU_DEBUG_CMD_LEN + 1];
    char m_szSendBuf[MDU_DEBUG_INFO_LEN + 1];
    char m_szLastRecvBuf[MDU_DEBUG_CMD_LEN + 1];

    static int32_t m_iClientNum;
    int32_t        m_iHeartbeatNum;
    int32_t        m_iTimerId;

    MessageMap m_firstLevelMessage;

    MessageMap m_showMessage;

    MessageMap m_sessionMessage;

    MessageMap m_businessMessage;
};

typedef ACE_Acceptor <CMduDebugger, ACE_SOCK_ACCEPTOR> CMduDebugAcceptor;

#endif

