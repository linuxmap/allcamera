#ifndef _SVS_Record_Server_h
#define _SVS_Record_Server_h
//ע����Ӧ��Ϣ����������
#define REG_RESP_MSG_BUFF_LEN   512
#include "svs_rt_client_handler.h"

/**
 * @class CSVS_Record_Server
 * @brief ʵʱ¼���������
 */
class CSVS_Record_Server
{
protected:
    CSVS_Record_Server();

public:
    virtual ~CSVS_Record_Server();

    virtual int32_t init( const char *strConfigFile );

    // ��ȡRtRecordSvr ID
    const char *str_id() const
    {
        return str_rt_server_id_;
    }


    //��ȡlocal IP
    const char *str_local_ip() const
    {
        return str_local_ip_;
    }

    //����¼���������IP, �����ֽ���
    uint32_t get_local_ip(int32_t) const
    {
        return local_ip_;
    }

    //����¼����Ʒ�����IP
    const char *recordctrl_ip() const
    {
        return str_recordctrl_ip_;
    }

    //���������ֽ����¼����Ʒ������˿ں�
    uint16_t recordctrl_port() const
    {
        return recordctrl_port_;
    }

    //����¼����Ʒ�����IP
    const char *lan_ip() const
    {
        return str_lan_ip_;
    }

    //���������ֽ����¼����Ʒ������˿ں�
    /*���������Ӷ˿���ӱ��ö˿�
    1. ԭ�˿���66**���ֱ���61**,62**,63**,64**,65**
    2. ��ÿ���˿�����3�Σ�ÿ��3�볬ʱ������ʧ�ܺ󣬲�����һ�˿ڽ������ӣ������ѭ��ֱ�����ӳɹ�
    3. ��������Ҫ��������ǽ�Ĳ��������޸ģ�NRU��VTDU��
    */
    uint16_t lan_port()
    {
        uint16_t realUsePort = 0;
        // 6���˿ڣ�ÿ�γ���3��
        uint16_t mult = lan_port_cyc_use_flag_ / 3;
        realUsePort = lan_port_ - (uint16_t)(mult * 100);

        // ʹ�ô�������1��Ϊ��һ����ѭ��׼��
        lan_port_cyc_use_flag_ = (lan_port_cyc_use_flag_ + 1) % (3*6);

        return realUsePort;
    }

    // ��ʼ�����ӷ�Ӧ��
    int32_t connect_reactor(    ACE_Reactor *pReactor,
                            ACE_Message_Queue <ACE_SYNCH> *pMsgQueue);

    //������Ϣ�����Ͷ��С�
    int32_t send_msg(ACE_Message_Block *mb);
protected:
    virtual bool init_configure(const char * const strConfigFile);

    bool initRecordServerConfig(const char* const strConfigFile);

    /**
     * ��������Ϣ: MAC, ���أ�����ȡ�
     * �ɹ�����0��ʧ�ܷ���-1��
     */
    int32_t read_network_info();

protected:
    char str_name_[RECORDSVR_NAME_LEN];

    uint32_t local_ip_;

    char str_local_ip_[SVS_IP_LEN];

    char str_rt_server_id_[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    char str_play_server_id_[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    //����������ҵ��ڵ�ID
    char str_node_id_[SVS_NODEID_LEN + 1];

    char str_cms_index_[SVS_CMSINDEX_LEN + 1];

    uint8_t server_type_;

    uint8_t default_server_;

    ACE_Reactor *p_connect_reactor_;  //���ӷ�Ӧ��

    CSVS_Client_Handler *p_connector_;
    char str_mac_addr_[SVS_IP_LEN];
    char str_gateway_[SVS_IP_LEN];
    char str_netmask_[SVS_IP_LEN];

    char str_lan_ip_[SVS_IP_LEN];
    //����StoreSvr�������˿�
    uint16_t lan_port_;
    uint8_t lan_port_cyc_use_flag_;

    char str_recordctrl_ip_[SVS_IP_LEN]; //¼�������ϵͳIP

//    char str_db_ip_[SVS_IP_LEN];
//    uint16_t db_port_;
    //¼�������ϵͳ��������˿ں�, �����ֽ���
    uint16_t recordctrl_port_;

    // ��StoreSvrע����Ӧ������
    char register_resp_msg_buff_[REG_RESP_MSG_BUFF_LEN];
};
#endif // SVS_Record_Server.h


