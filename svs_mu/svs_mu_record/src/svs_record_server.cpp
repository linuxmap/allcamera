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

     // ¼�������ϵͳ��������˿ں�
    recordctrl_port_ = 0;

    //¼�������ϵͳIP
    (void)ACE_OS::memset(str_recordctrl_ip_,0,sizeof(str_recordctrl_ip_));

    // lan��ַ
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
 �� �� ��  : init
 ��������  : ��ʼ��
 �������  : const char *strConfigFile:�����ļ�ȫ·��
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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
 �� �� ��  : init_configure
 ��������  : ��ʼ������
 �������  : const char * const strConfigFile:�����ļ�ȫ·��
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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



    //¼�������ϵͳ�˿�
    if (sys_config.get("SVS_STORE", "Port", str) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [SVS_STORE][Port] from configuration file."
            "configuration file name[%s].",
            strConfigFile));
        return false;
    }

    recordctrl_port_ = (uint16_t)atoi(str);

    //������IP,����StoreSvr��������IP
    if (sys_config.get("RECORD_SERVER", "LanIp", str_lan_ip_) != 0)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Failed to read the item [RECORD_SERVER][LanIp] from configuration file."
            "configuration file name[%s].",
            strConfigFile));
        return false;
    }

    //����StoreSvr�������ı��ض˿ں�
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
 �� �� ��  : connect_reactor
 ��������  : ��ʼ�����ӷ�Ӧ��
 �������  : ACE_Reactor * pReactor:���ӷ�Ӧ��
             ACE_Message_Queue <ACE_SYNCH> *pMsgQueue:��Ϣ����
             ACE_Message_Queue <ACE_SYNCH> *pEventQueue:ʱ�����
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Record_Server::connect_reactor( ACE_Reactor * pReactor,
                                ACE_Message_Queue <ACE_SYNCH> *pMsgQueue)
{
    SVS_TRACE();

    // �����ж�
    if ( NULL == pReactor )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create the handle for connection with StoreSvr failed. "
            "The parameter is invalid. pReactor is null."));
        return -1;
    }

    // ����handle
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

    // ���ӷ���
    if (0 != p_connector_->open(0))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Register the handle for connecting with StoreSvr failed."));
        return -1;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : read_network_info
 ��������  : ��ȡ����������Ϣ
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����0,ʧ�ܷ���-1
 �޸���ʷ  :
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
 �� �� ��  : send_msg
 ��������  : ��StoreSvr������Ϣ
 �������  : ACE_Message_Block *mb:Ҫ���͵���Ϣ����
 �������  : NA
 �� �� ֵ  : �ɹ�����true,ʧ�ܷ���false
 �޸���ʷ  :
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
        // ��Ϊ�����߳��޷��жϽ����߳��Ƿ��������ӣ� �п��ܽ������Ӻõ����ӹر�.
        return -1;
    }

    return 0;
}


