#ifndef _SVS_Record_Server_h
#define _SVS_Record_Server_h
//注册响应消息缓冲区长度
#define REG_RESP_MSG_BUFF_LEN   512
#include "svs_rt_client_handler.h"

/**
 * @class CSVS_Record_Server
 * @brief 实时录像服务器类
 */
class CSVS_Record_Server
{
protected:
    CSVS_Record_Server();

public:
    virtual ~CSVS_Record_Server();

    virtual int32_t init( const char *strConfigFile );

    // 获取RtRecordSvr ID
    const char *str_id() const
    {
        return str_rt_server_id_;
    }


    //获取local IP
    const char *str_local_ip() const
    {
        return str_local_ip_;
    }

    //返回录像服务器的IP, 网络字节序
    uint32_t get_local_ip(int32_t) const
    {
        return local_ip_;
    }

    //返回录像控制服务器IP
    const char *recordctrl_ip() const
    {
        return str_recordctrl_ip_;
    }

    //返回主机字节序的录像控制服务器端口号
    uint16_t recordctrl_port() const
    {
        return recordctrl_port_;
    }

    //返回录像控制服务器IP
    const char *lan_ip() const
    {
        return str_lan_ip_;
    }

    //返回主机字节序的录像控制服务器端口号
    /*将本地连接端口添加备用端口
    1. 原端口是66**，分别备用61**,62**,63**,64**,65**
    2. 对每个端口重试3次，每次3秒超时；重试失败后，采用下一端口进行连接；如此轮循，直到连接成功
    3. 对所有需要经过防火墙的部件进行修改（NRU、VTDU）
    */
    uint16_t lan_port()
    {
        uint16_t realUsePort = 0;
        // 6个端口，每次尝试3次
        uint16_t mult = lan_port_cyc_use_flag_ / 3;
        realUsePort = lan_port_ - (uint16_t)(mult * 100);

        // 使用次数增加1，为下一次轮循做准备
        lan_port_cyc_use_flag_ = (lan_port_cyc_use_flag_ + 1) % (3*6);

        return realUsePort;
    }

    // 初始化连接反应器
    int32_t connect_reactor(    ACE_Reactor *pReactor,
                            ACE_Message_Queue <ACE_SYNCH> *pMsgQueue);

    //发送消息到发送队列。
    int32_t send_msg(ACE_Message_Block *mb);
protected:
    virtual bool init_configure(const char * const strConfigFile);

    bool initRecordServerConfig(const char* const strConfigFile);

    /**
     * 读网络信息: MAC, 网关，掩码等。
     * 成功返回0，失败返回-1。
     */
    int32_t read_network_info();

protected:
    char str_name_[RECORDSVR_NAME_LEN];

    uint32_t local_ip_;

    char str_local_ip_[SVS_IP_LEN];

    char str_rt_server_id_[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    char str_play_server_id_[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    //服务器所属业务节点ID
    char str_node_id_[SVS_NODEID_LEN + 1];

    char str_cms_index_[SVS_CMSINDEX_LEN + 1];

    uint8_t server_type_;

    uint8_t default_server_;

    ACE_Reactor *p_connect_reactor_;  //连接反应器

    CSVS_Client_Handler *p_connector_;
    char str_mac_addr_[SVS_IP_LEN];
    char str_gateway_[SVS_IP_LEN];
    char str_netmask_[SVS_IP_LEN];

    char str_lan_ip_[SVS_IP_LEN];
    //连接StoreSvr服务器端口
    uint16_t lan_port_;
    uint8_t lan_port_cyc_use_flag_;

    char str_recordctrl_ip_[SVS_IP_LEN]; //录像控制子系统IP

//    char str_db_ip_[SVS_IP_LEN];
//    uint16_t db_port_;
    //录像控制子系统信令监听端口号, 主机字节序
    uint16_t recordctrl_port_;

    // 向StoreSvr注册响应缓存区
    char register_resp_msg_buff_[REG_RESP_MSG_BUFF_LEN];
};
#endif // SVS_Record_Server.h


