#include "vms/vms.h"
#include "svs_netinfo.h"
#include "svs_config.h"
#include "svs_log_msg.h"
#include "svs_stat_manager.h"
#include "svs_record_server.h"
#include "svs_mb_buffer.h"
#include "svs_real_record_task.h"

//=======================CSVS_Record_Server BEGIN==========================

CSVS_Record_Server::CSVS_Record_Server()
{
    str_name_[0] = '\0';
    local_ip_ = 0;
    str_local_ip_[0] = '\0';
    server_type_ = 0;
    default_server_ = 0;

     // 录像控制子系统信令监听端口号
    recordctrl_port_ = 0;

    //录像控制子系统IP
    (void)ACE_OS::memset(str_recordctrl_ip_,0,sizeof(str_recordctrl_ip_));

    // lan地址
    lan_port_ = 0;
    (void)ACE_OS::memset(str_lan_ip_,0,sizeof(str_lan_ip_));
    (void)ACE_OS::memset(str_rt_server_id_,0,sizeof(str_rt_server_id_));
    (void)ACE_OS::memset( str_mac_addr_, 0, sizeof(str_mac_addr_) );
    (void)ACE_OS::memset( str_gateway_, 0, sizeof(str_gateway_) );
    (void)ACE_OS::memset( str_netmask_, 0, sizeof(str_netmask_) );

    p_connect_reactor_ = NULL;
    p_connector_ = NULL;

    (void)ACE_OS::memset( register_resp_msg_buff_, 0, sizeof(register_resp_msg_buff_) );

    lan_port_cyc_use_flag_ = 0;
}

CSVS_Record_Server::~CSVS_Record_Server()
{
    try
    {
        p_connect_reactor_ = NULL;

        delete p_connector_;
        p_connector_       = NULL;
    }
    catch(...)
    {
    }
}

/*****************************************************************************
 函 数 名  : init
 功能描述  : 初始化
 输入参数  : const char *strConfigFile:配置文件全路径
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CSVS_Record_Server::init(const char *strConfigFile )
{
    SVS_TRACE();
    if (!init_configure(strConfigFile))
    {
        SVS_LOG((SVS_LM_ERROR, "Occur error while reading configuration file."));
        return -1;
    }

    return 0;
}
bool CSVS_Record_Server::initRecordServerConfig(const char* const strConfigFile)
{
    SVS_TRACE();

    char str[512];
    CAC_Config sys_config((char *)strConfigFile);

    if (sys_config.get("RECORD_SERVER", "Name", str) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [RECORD_SERVER] [Name] from configuration file."
            "configuration file name[%s].",
            strConfigFile));
        return false;
    }

    uint32_t realNameLen = strlen(str);
    if(RECORDSVR_NAME_LEN < realNameLen)
    {
        SVS_LOG((SVS_LM_ERROR,
            "The item [RECORD_SERVER] [Name]value is too int32_t. max len[%d], "
            "real len[%u],value[%s], configuration file name[%s].",
            RECORDSVR_NAME_LEN, realNameLen,str,
            strConfigFile));
        return false;
    }

    memset(str_name_, 0, sizeof(str_name_));
    memcpy(str_name_, str, realNameLen);

    if (sys_config.get("RECORD_SERVER", "Ip", str_local_ip_) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [RECORD_SERVER][Ip] from configuration file."
            "configuration file name[%s].",
            strConfigFile));
        return false;
    }

    if (sys_config.get("RECORD_SERVER", "ID", str_rt_server_id_) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [RECORD_SERVER][ID] from configuration file."
            "configuration file name[%s].",
            strConfigFile));
        return false;
    }

    if (sys_config.get("RECORD_SERVER", "NodeID", str_node_id_) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [RECORD_SERVER][NodeID] from configuration file."
            "configuration file name[%s].",
            strConfigFile));

        return false;
    }

    if (sys_config.get("RECORD_SERVER", "CMSIndex", str_cms_index_) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [RECORD_SERVER][CMSIndex] from configuration file."
            "configuration file name[%s].",
            strConfigFile));

        return false;
    }

    if (sys_config.get("RECORD_SERVER", "StreamID", str_play_server_id_) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [RECORD_SERVER][StreamID] from configuration file."
            "configuration file name[%s].",
            strConfigFile));
    }

    if (sys_config.get("RECORD_SERVER", "DefaultServer", str) != 0)
    {
        default_server_ = 0;
    }
    else
    {
        default_server_ = (uint8_t)atoi(str);
    }

    if (sys_config.get("SVS_STORE", "Ip", str_recordctrl_ip_) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [SVS_STORE][Ip] from configuration file."
            "configuration file name[%s].",
            strConfigFile));
        return false;
    }

    return true;
}
/*****************************************************************************
 函 数 名  : init_configure
 功能描述  : 初始化配置
 输入参数  : const char * const strConfigFile:配置文件全路径
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
bool CSVS_Record_Server::init_configure(const char * const strConfigFile)
{
    SVS_TRACE();

    char str[512];
    CAC_Config sys_config((char *)strConfigFile);

    if (!initRecordServerConfig(strConfigFile))
    {
        SVS_LOG((SVS_LM_ERROR, "Init record server config fail."));
        return false;
    }

    if (0 != read_network_info())
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read network information from configuration file."
            "configuration file name[%s].",
            strConfigFile));
        return false;
    }

    local_ip_ = inet_addr(str_local_ip_);



    //录像控制子系统端口
    if (sys_config.get("SVS_STORE", "Port", str) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [SVS_STORE][Port] from configuration file."
            "configuration file name[%s].",
            strConfigFile));
        return false;
    }

    recordctrl_port_ = (uint16_t)atoi(str);

    //局域网IP,连接StoreSvr服务器的IP
    if (sys_config.get("RECORD_SERVER", "LanIp", str_lan_ip_) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [RECORD_SERVER][LanIp] from configuration file."
            "configuration file name[%s].",
            strConfigFile));
        return false;
    }

    //连接StoreSvr服务器的本地端口号
    if (sys_config.get("RECORD_SERVER", "ConnectStoreSvrLocalPort", str) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [RECORD_SERVER][ConnectStoreSvrLocalPort]"
            " from configuration file."
            "configuration file name[%s].",
            strConfigFile));
        return false;
    }
    lan_port_ = (uint16_t)atoi(str);
    lan_port_cyc_use_flag_ = 0;

    return true;
}

/*****************************************************************************
 函 数 名  : connect_reactor
 功能描述  : 初始化连接反应器
 输入参数  : ACE_Reactor * pReactor:连接反应器
             ACE_Message_Queue <ACE_SYNCH> *pMsgQueue:消息队列
             ACE_Message_Queue <ACE_SYNCH> *pEventQueue:时间队列
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CSVS_Record_Server::connect_reactor( ACE_Reactor * pReactor,
                                ACE_Message_Queue <ACE_SYNCH> *pMsgQueue)
{
    SVS_TRACE();

    // 参数判断
    if ( NULL == pReactor )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create the handle for connection with StoreSvr failed. "
            "The parameter is invalid. pReactor is null."));
        return -1;
    }

    // 创建handle
    p_connect_reactor_ = pReactor;
    try
    {
        p_connector_ = new CSVS_Client_Handler(pReactor, pMsgQueue);
    }
    catch( ...)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create the handle for connection with StoreSvr failed."
            "Allocate memory failed."));
        return -1;
    }

    // 连接服务
    if (0 != p_connector_->open(0))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Register the handle for connecting with StoreSvr failed."));
        return -1;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 函 数 名  : read_network_info
 功能描述  : 获取网络配置信息
 输入参数  : NA
 输出参数  : NA
 返 回 值  : 成功返回0,失败返回-1
 修改历史  :
*****************************************************************************/
int32_t CSVS_Record_Server::read_network_info()
{
    SVS_TRACE();
    char macaddr[24] = {0};
    char netmask[24] = {0};
    char gateway[24] = {0};

    CNetInfo netinfo;

    int32_t ret = netinfo.getIPNetInfo(str_local_ip_, macaddr, netmask);
    if (0 != ret)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to get MAC address from system."));
        netmask[0] = '\0';
        macaddr[0] = '\0';
    }

    ret = netinfo.getDstIPGateway(str_local_ip_, gateway);
    if (0 != ret)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to get gateway's ip from system."));
        gateway[0] = '\0';
    }

    if (strlen(macaddr) >= sizeof(str_mac_addr_)
        || (strlen(netmask) >= sizeof(str_netmask_))
        || (strlen(gateway) >= sizeof(str_gateway_)))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Get network information from system failed."
            "Buffer size is to small."));
        return -1;
    }

    (void)ACE_OS::memcpy(str_mac_addr_, macaddr, strlen(macaddr) + 1);
    (void)ACE_OS::memcpy(str_netmask_, netmask, strlen(netmask) + 1);
    (void)ACE_OS::memcpy(str_gateway_, gateway, strlen(gateway) + 1);

    return 0;
}

/*****************************************************************************
 函 数 名  : send_msg
 功能描述  : 向StoreSvr发送消息
 输入参数  : ACE_Message_Block *mb:要发送的消息内容
 输出参数  : NA
 返 回 值  : 成功返回true,失败返回false
 修改历史  :
*****************************************************************************/
int32_t CSVS_Record_Server::send_msg(ACE_Message_Block *mb)
{
    SVS_TRACE();

    if ( NULL == p_connector_ )
    {
        SVS_LOG((SVS_LM_ERROR,
            "[SEND]Send message to StoreSvr failed, p_connector_ is null."));

        return -1;
    }

    ACE_Time_Value tvDelay(0, 100 * 1000);
    ACE_Time_Value tvTimeout = ACE_OS::gettimeofday() + tvDelay;

    if (-1 == p_connector_->send_msg(mb, &tvTimeout))
    {
        SVS_LOG((SVS_LM_ERROR, "[SEND] message enqueue failed."));
        // 因为发送线程无法判断接收线程是否重新连接， 有可能将刚连接好的连接关闭.
        return -1;
    }

    return 0;
}


