#include "svs_print_version.h"
#include "svs_log_msg.h"
#include "svs_debug_accept_handler.h"
#include "svs_access_control.h"
#include "svs_server_manager.h"
#include "svs_business_manager.h"
#include "svs_access_control_manager.h"

using namespace std;

extern time_t g_program_start_time;
int32_t findSubString(std::string& strSource, std::string strSub);
int32_t CSVS_Debug_Accept_Handler::m_client_number = 0;

CSVS_Debug_Accept_Handler::CSVS_Debug_Accept_Handler()
{
    SVS_LOG((SVS_LM_DEBUG, "[OBJ_CREAT] CSVS_Debug_Accept_Handler create. address=0x%08x.",
        this));
    (void)constructor();
}

CSVS_Debug_Accept_Handler::CSVS_Debug_Accept_Handler(ACE_Thread_Manager* /*tm*/)
{
    SVS_LOG((SVS_LM_DEBUG, "[OBJ_CREAT] CSVS_Debug_Accept_Handler create. address=0x%08x.",
        this));
    (void)constructor();
}

CSVS_Debug_Accept_Handler::~CSVS_Debug_Accept_Handler()
{
    try
    {
        SVS_LOG((
            SVS_LM_DEBUG,
            "[OBJ_DESTORY] ~CSVS_Debug_Accept_Handler destroy. "
            "address=0x%08x.",
            this));
    }
    catch (...)
    {
        printf("~CSVS_Debug_Accept_Handler catch an exception.\n");
    }
}

//被构造函数调用
int32_t CSVS_Debug_Accept_Handler::constructor()
{
    (void)ACE_OS::memset(str_peer_addr_, 0, sizeof str_peer_addr_);
    m_index = 0;
    m_timerId = 0;
    m_nWaitHeartBeatCount = 0;
    return 0;
}

int32_t CSVS_Debug_Accept_Handler::open(void *)
{
    SVS_TRACE();

    //注册反应器
    (void)reactor()->register_handler(this, ACE_Event_Handler::READ_MASK);

    //记下连接端的IP及端口号
    ACE_INET_Addr remoteaddr;
    (void)peer().get_remote_addr (remoteaddr);

    (void)ACE_OS::snprintf(str_peer_addr_, sizeof str_peer_addr_, "%s:%d",
        remoteaddr.get_host_addr(), remoteaddr.get_port_number());

    ++m_client_number;
    SVS_LOG((SVS_LM_INFO, "Debug from %s. ClientNumber=%d.", str_peer_addr_, m_client_number));

    if (m_client_number > MAX_DEBUG_CLIENT_NUMBER)
    {
        (void)ACE_OS::snprintf(
            send_buf_, sizeof send_buf_,
            "\r\nSorry for Closing, DebugClientNumber=%d, MaxClientNumber=%d\r\n\r\n",
            m_client_number, MAX_DEBUG_CLIENT_NUMBER);

        (void)sendData();
        return -1;
    }

    //发送欢迎信息
    (void)sendVersion();

    ACE_Time_Value delay(DEBUG_HEARTBEAT_SEND_INTERVAL);
    m_timerId = reactor()->schedule_timer(this, 0, delay, delay);
    if (SVS_ERROR_FAIL == m_timerId)
    {
        SVS_LOG((SVS_LM_ERROR, "Fail to allocate heart_beat timer. timer=%d, accept from %s.",
            m_timerId, str_peer_addr_));
        return SVS_ERROR_FAIL;
    }

    return 0;
}

int32_t CSVS_Debug_Accept_Handler::svc()
{
    return 0;
}

int32_t CSVS_Debug_Accept_Handler::handle_input(ACE_HANDLE /*handle*/)
{
    SVS_TRACE();

    ++m_nWaitHeartBeatCount;
    ACE_Time_Value tv(WAIT_TIME_5SECONDS);
    int32_t recvResult = peer().recv(recv_buf_ + m_index, SVS_LOG_BUF_MAX - 1, &tv);

    if (recvResult <= 0)
    {
        SVS_LOG((SVS_LM_ERROR, "Debug from %s, receive error. receive=%d.",
            str_peer_addr_, recvResult));
        return SVS_ERROR_FAIL;
    }

    recv_buf_[recvResult + m_index] = '\0';
    SVS_LOG((SVS_LM_DEBUG, "Debug from %s. length=%d, index=%d, message=%s.",
        str_peer_addr_, recvResult, m_index, recv_buf_));

    for (uint32_t i = 0; i < sizeof LINUX_CTRL_C; ++i)   //Linux Ctrol + C
    {
        if (LINUX_CTRL_C[i] == static_cast<uint8_t>(recv_buf_[i]))
        {
            if (sizeof LINUX_CTRL_C - 1 == i)
            {
                return SVS_ERROR_FAIL;
            }
        }
        else
        {
            break;
        }
    }

    if (1 == recvResult)     //Windows
    {
        if (WINDOWS_CTRL_C == static_cast<uint8_t>(recv_buf_[m_index]))
        {
            return SVS_ERROR_FAIL;
        }

        if (++m_index > static_cast<int32_t>(sizeof recv_buf_) - 2)
        {
            recv_buf_[sizeof recv_buf_ - 1] = 0;
            SVS_LOG((SVS_LM_ERROR, "Debug from %s. length=%d too int32_t, message=%s.",
                str_peer_addr_, m_index, recv_buf_));
            m_index = 0;
        }

        return 0;
    }
    else if (2 == recvResult && '\r' == recv_buf_[m_index] && '\n' == recv_buf_[m_index + 1])
    {
        recv_buf_[m_index + 2] = 0;
        m_index = 0;
    }
    /*else if ('A' > toupper(recv_buf_[m_index]) || 'Z' < toupper(recv_buf_[m_index]))
    {
        return 0;
    }*/

    return processData1(recv_buf_);
}

int32_t CSVS_Debug_Accept_Handler::handle_close(
    ACE_HANDLE /*handle*/,
    ACE_Reactor_Mask /*mask*/)
{
    SVS_TRACE();

    (void)reactor()->cancel_timer(this, 1);

    //清除所有消息
    (void)reactor()->remove_handler(
        this,
        ACE_Event_Handler::ALL_EVENTS_MASK | ACE_Event_Handler::DONT_CALL);

    (void)ACE_OS::shutdown(peer().get_handle(), SHUT_RDWR);
    (void)peer().close();

    SVS_LOG((SVS_LM_INFO, "Debug close. remote address=%s, ClientNumber=%d.",
        str_peer_addr_, m_client_number));

    --m_client_number;

    delete this;
    return 0;
}
int32_t CSVS_Debug_Accept_Handler::handle_timeout(const ACE_Time_Value&, const void*)
{
    SVS_TRACE();

    if (m_nWaitHeartBeatCount > 0)
    {
        SVS_LOG((SVS_LM_DEBUG, "Debug from %s, WaitHeartBeatCount is %d.",
            str_peer_addr_, m_nWaitHeartBeatCount));
        m_nWaitHeartBeatCount = 0;
        return 0;
    }

    string strData("Sorry for Closing, more than 20 minutes no input\r\n");
    (void)strData.insert(strData.length(), "\r\nsvs_access_control-->");
    (void)sendData(strData.c_str());

    return SVS_ERROR_FAIL;
}


//发送数据包装
int32_t CSVS_Debug_Accept_Handler::sendData()
{
    SVS_TRACE();

    //防止内存访问越界
    send_buf_[ sizeof send_buf_ - 1 ] = '\0';

    ACE_Time_Value tv(WAIT_TIME_5SECONDS);
    ssize_t len = (ssize_t)ACE_OS::strlen( send_buf_ );
    if (len != peer().send_n(send_buf_, static_cast<uint32_t>(len), &tv))
    {
        SVS_LOG((SVS_LM_ERROR, "Debug error. remote address=%s, length=%d, message=%s.",
            str_peer_addr_, len, send_buf_));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_DEBUG, "Debug to %s. length=%d, message=%s.",
        str_peer_addr_, len, send_buf_));
    return 0;
}

int32_t CSVS_Debug_Accept_Handler::sendData(const char *pszData)
{
    SVS_TRACE();

    if (NULL == pszData)
    {
        SVS_LOG((SVS_LM_ERROR, "Invalid parameter. pszData is null."));
        return SVS_ERROR_FAIL;
    }

    ACE_Time_Value tv(WAIT_TIME_5SECONDS);
    ssize_t len = (ssize_t)ACE_OS::strlen( pszData );
    if (len != peer().send_n(pszData, static_cast<uint32_t>(len), &tv))
    {
        SVS_LOG((SVS_LM_ERROR, "Debug error to %s. length=%d, message=%s.",
            str_peer_addr_, len, pszData));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_DEBUG, "Debug to %s. length=%d, message=%s.",
        str_peer_addr_, len, pszData));
    return 0;
}

int32_t CSVS_Debug_Accept_Handler::processData1(std::string strCmd)
{
    SVS_TRACE();

    //去掉换行
    std::string::size_type pos;
    while ((pos = strCmd.find("\r\n")) != std::string::npos)
    {
        (void)strCmd.replace( pos, 2, "" );
    }

    while ((pos = strCmd.find("\n")) != std::string::npos)
    {
        (void)strCmd.replace(pos, 1, "");
    }

    std::string strID = "";
    int32_t ret = 0;
    //命令为空
    if (strCmd.empty())
    {
        return sendSpaceCmd();
    }

    if( std::string::npos != strCmd.find( "[A" ) )
    {
        strCmd = last_exec_cmd_;
    }
    else
    {
        last_exec_cmd_ = strCmd;
    }

    if (0 == findSubString(strCmd, "show device"))
    {
        pos= strlen("showdevice");
        if (pos < strCmd.length())
        {
            strID = strCmd.substr(pos, strCmd.length() - pos);
        }

        ret = sendDeviceInfo(strID);
    }
    else if (0 == findSubString(strCmd, "show lens"))
    {
        pos= strlen("showlens");
        if (pos < strCmd.length())
        {
            strID = strCmd.substr(pos, strCmd.length() - pos);
        }

        ret = sendLensInfo(strID);
    }
    else if (0 == findSubString(strCmd, "show stream"))
    {
        pos= strlen("showstream");
        if (pos < strCmd.length())
        {
            strID = strCmd.substr(pos, strCmd.length() - pos);
        }

        ret = sendStreamSvrInfo(strID);
    }
    else if (0 == findSubString(strCmd, "show session"))
    {
        pos= strlen("showsession");
        if (pos < strCmd.length())
        {
            strID = strCmd.substr(pos, strCmd.length() - pos);
        }
        ret = sendSessionInfo(strID);
    }
    else
    {
        ret = processData2(strCmd);
    }

    return ret;
}

int32_t CSVS_Debug_Accept_Handler::processData2(std::string &strCmd)
{
    SVS_TRACE();

    int32_t ret = 0;
    if (0 == findSubString(strCmd, "help"))
    {
        ret = sendHelpInfo();
    }
    else if (0 == findSubString(strCmd, "show config"))
    {
        ret = sendBasicInfo();
    }
    else if (0 == findSubString(strCmd, "quit"))
    {
        ret = SVS_ERROR_FAIL;
    }
    else if (0 == findSubString(strCmd, "show version"))
    {
        ret = sendVersion();
    }
    else
    {
        ret = sendBadCmd();
    }

    return ret;
}


//发送帮助信息
//发送服务器基本配置
int32_t CSVS_Debug_Accept_Handler::sendHelpInfo()
{
    SVS_TRACE();

    (void)ACE_OS::snprintf(
        send_buf_, sizeof send_buf_,
        "\r\nhelp"
        "\r\nquit\r\n"
        "\r\nshow config\t\tshow config"
        "\r\nshow version\t\tshow version\r\n"
        "\r\nshow device\t\tshow all device"
        "\r\nshow device ID\t\tshow device match ID\r\n"
        "\r\nshow lens\t\tshow all lens"
        "\r\nshow lens ID\t\tshow lens match ID\r\n"
        "\r\nshow stream\t\tshow all stream"
        "\r\nshow stream ID\t\tshow stream match ID\r\n"
        "\r\nshow session\t\tshow all stream session"
        "\r\nshow session ID\t\tshow stream session match lens ID\r\n"
        "\r\n\r\nsvs_access_control-->" );

    return sendData();
}



//发送服务器基本配置
int32_t CSVS_Debug_Accept_Handler::sendBasicInfo()
{
    SVS_TRACE();

    (void)ACE_OS::snprintf(
        send_buf_, sizeof send_buf_,
        "\r\nLogLevel define : DEBUG=%d, INFO=%d, WARNING=%d, ERROR=%d, CRITICAL=%d\r\n"
        "LogEnable define : 1=true, 0=false\r\n"
        "\r\nID\t\t= %s"
        "\r\nSipIP\t\t= %s"
        "\r\nSipprot\t\t= %d"
        "\r\nMuIP\t\t= %s"
        "\r\nMuprot\t\t= %d"
        "\r\nHttpIP\t\t= %s"
        "\r\nHttpport\t= %d"
        "\r\nLogLevel\t= %d"
        "\r\nLogEnable\t= %d"
        "\r\n\r\nsvs_access_control-->",
        SVS_LM_DEBUG, SVS_LM_INFO, SVS_LM_WARNING, SVS_LM_ERROR, SVS_LM_CRITICAL,
        CAccessControlSvr::instance().getSelfID(),
        CAccessControlSvr::instance().getSvrIp(),
        CAccessControlSvr::instance().get_device_gb28181_stack_listen_port(),
        CAccessControlSvr::instance().getMuListenIP(),
        CAccessControlSvr::instance().getMuListenPort(),
        CAccessControlSvr::instance().getHttpStackListenAddr(),
        CAccessControlSvr::instance().getHttpStackListenPort(),
        SVS_Log_Msg::get_log_priority(),
        SVS_Log_Msg::get_log_flag());

   return sendData();
}

//发送错误消息
int32_t CSVS_Debug_Accept_Handler::sendBadCmd()
{
    SVS_TRACE();

    (void)ACE_OS::snprintf(
        send_buf_, sizeof send_buf_,
        "\r\n\tCommand not found, try `help' for more information"
        "\r\n\r\nsvs_access_control-->");

    return sendData();
}

//发送换行
//发送空命令消息
int32_t CSVS_Debug_Accept_Handler::sendSpaceCmd()
{
    SVS_TRACE();

    (void)ACE_OS::snprintf(send_buf_, sizeof send_buf_, "%s", "svs_access_control-->");
    return sendData();
}


int32_t CSVS_Debug_Accept_Handler::sendVersion()
{
    SVS_TRACE();

    time_t now_time;
    (void)time(&now_time);

    char now_time_buffer[SVS_IP_LEN] = {0};
    (void)ACE_OS::ctime_r(&now_time, now_time_buffer, sizeof now_time_buffer);

    char run_time_buffer[SVS_IP_LEN] = {0};;
    (void)ACE_OS::ctime_r(&g_program_start_time, run_time_buffer, sizeof run_time_buffer);

    (void)ACE_OS::snprintf(
        send_buf_, sizeof send_buf_,
        "\r\n\t==============================================================="
        "\r\n\r\n\t\t Welcome Client=%s, Number=%d"
        "\r\n\r\n\t Version=%s, Build=%s, %s"
        "\r\n\r\n\t\t ServerRun=%d=%s"
        "\r\n\t\t ServerNow=%d=%s"
        "\r\n\t==============================================================="
        "\r\n\r\nsvs_access_control-->",
        str_peer_addr_, m_client_number,
        SVR_VERSION_INFO, __DATE__, __TIME__,
        g_program_start_time, run_time_buffer,
        now_time, now_time_buffer);

    return sendData();
}

int32_t CSVS_Debug_Accept_Handler::findSubString(std::string& strSource, std::string strSub)
{
    SVS_TRACE();

    if ((strSource.empty())||(strSub.empty()))
    {
        SVS_LOG((SVS_LM_ERROR, "Invalid parameter. SouceStr=%s, SubStr=%s.",
            strSource.c_str(), strSub.c_str()));
        return SVS_ERROR_FAIL;
    }

    //去除所有空格换行
    string::size_type pos;
    while ((pos = strSource.find( "\r\n" )) != std::string::npos)
    {
        (void)strSource.replace( pos, 2, "" );
    }

    while ((pos = strSource.find( "\n" )) != std::string::npos)
    {
        (void)strSource.replace( pos, 1, "" );
    }

    while ((pos = strSource.find( " " )) != std::string::npos)
    {
        (void)strSource.replace( pos, 1, "" );
    }

    while ((pos = strSub.find( "\r\n" )) != std::string::npos)
    {
        (void)strSub.replace( pos, 2, "" );
    }

    while ((pos = strSub.find( "\n" )) != std::string::npos)
    {
        (void)strSub.replace( pos, 1, "" );
    }

    while ((pos = strSub.find( " " )) != std::string::npos)
    {
        (void)strSub.replace( pos, 1, "" );
    }

    if ((pos = strSource.find(strSub.c_str()) ) != std::string::npos)
    {
        //找到子串
        return (int32_t)pos;
    }

    return SVS_ERROR_FAIL;
}

int32_t CSVS_Debug_Accept_Handler::sendDeviceInfo(const std::string &strID)
{
    SVS_TRACE();

    string strData;
    IAccessControlManager::instance().show_deviceInfo(strID,strData);

    (void)strData.insert(strData.length(), "\r\nsvs_access_control-->");
    return sendData(strData.c_str());
}
int32_t CSVS_Debug_Accept_Handler::sendLensInfo(const std::string &strID)
{
    SVS_TRACE();

    string strData;
    IAccessControlManager::instance().show_LensInfo(strID,strData);

    (void)strData.insert(strData.length(), "\r\nsvs_access_control-->");
    return sendData(strData.c_str());
}
int32_t CSVS_Debug_Accept_Handler::sendStreamSvrInfo(const std::string &strID)
{
    SVS_TRACE();

    string strData;
    CServerManager::instance().show_server_info(strID,strData);

    (void)strData.insert(strData.length(), "\r\nsvs_access_control-->");
    return sendData(strData.c_str());
}
int32_t CSVS_Debug_Accept_Handler::sendSessionInfo(const std::string &strID)
{
    SVS_TRACE();

    string strData;
    CBusinessManager::instance().show_business_info(strID,strData);

    (void)strData.insert(strData.length(), "\r\nsvs_access_control-->");
    return sendData(strData.c_str());
}








