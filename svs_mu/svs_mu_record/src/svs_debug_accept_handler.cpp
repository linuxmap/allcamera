#include "vms/vms.h"
#include "svs_config.h"
#include "svs_timer.h"
#include "svs_print_version.h"
#include "ace/OS.h"
#include "ace/INET_Addr.h"
#include "svs_log_msg.h"
#include "ace/SOCK_Stream.h"
#include "ace/Svc_Handler.h"
#include <string>
#include "svs_stat_manager.h"
#include "svs_record_index.h"
#include "svs_real_record_lens.h"
#include "svs_real_record_task.h"
#include "svs_record_inform.h"
#include "svs_real_record_server.h"
#include "svs_debug_accept_handler.h"
#include "svs_rt_record_common.h"
#include "svs_bakup_front_record_lens.h"
#include "svs_stat_manager.h"
#include "svs_record_disk.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"
#include "svs_index_resend_mgr.h"
using namespace std;
//extern const char *g_strVersion;
extern time_t g_ulStartTime;
CSVS_Debug_Accept_Handler::CSVS_Debug_Accept_Handler()
{
    (void)constructor();
}

CSVS_Debug_Accept_Handler::CSVS_Debug_Accept_Handler(ACE_Thread_Manager* /*tm*/)
{
    (void)constructor();
}

CSVS_Debug_Accept_Handler::~CSVS_Debug_Accept_Handler()
{
}

/*****************************************************************************
�� �� ��  : constructor
��������  : ��ʼ���ڲ���Ա����
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::constructor()
{
    SVS_TRACE();
    (void)ACE_OS::memset( str_peer_addr_, 0, sizeof(str_peer_addr_));
    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : open
��������  : �򿪵���
�������  : void *
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::open(void *)
{
    SVS_TRACE();

    // ע�ᷴӦ��
    (void)reactor()->register_handler(this, ACE_Event_Handler::READ_MASK);

    // �������Ӷ˵�IP���˿ں�
    ACE_INET_Addr remoteaddr;
    (void)peer().get_remote_addr (remoteaddr);
    ACE_OS::last_error(0);
    (void)ACE_OS::snprintf( str_peer_addr_,
                      sizeof(str_peer_addr_),
                      "%s:%d",
                      remoteaddr.get_host_addr(),
                      remoteaddr.get_port_number());

    SVS_LOG((SVS_LM_INFO, "[DEBUG]Recv debug msg and start debug. recv addr[%s], this[0x%08x].",
               str_peer_addr_,
               this));

//    // ���ͻ�ӭ��Ϣ
//    (void)ACE_OS::snprintf( send_buf_, sizeof(send_buf_),
//                "\n\n\tWelcome to use [Realtime Record Server] Debuger V1.0 \n\nsvs_mru_record-->");
//
//    (void)sendData();
    sendVersion();

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : svc
��������  :
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::svc()
{
    SVS_TRACE();

    ACE_hthread_t currThreadID = (ACE_hthread_t)ACE_OS::thr_self();
    SVS_LOG((SVS_LM_WARNING, "[DEBUG]Current thread ID[%u,%d]", currThreadID, currThreadID ));

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : handle_close
��������  : ����رյ���
�������  : ACE_HANDLE
            ACE_Reactor_Mask
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::handle_close(ACE_HANDLE /*handle*/, ACE_Reactor_Mask /*mask*/)
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_WARNING, "[DEBUG]Disconnect debuger. peer addr[%s], this[0x%08x].",
        str_peer_addr_,
        this));

    // ���������Ϣ
    (void)reactor()->remove_handler(this, ACE_Event_Handler::ALL_EVENTS_MASK | ACE_Event_Handler::DONT_CALL);

    (void)ACE_OS::shutdown(peer().get_handle(), SHUT_RDWR);
    (void)peer().close();

    SVS_LOG((SVS_LM_INFO, "[DEBUG]Success to disconnect debuger. peer addr[%s], this[0x%08x].",
        str_peer_addr_,
        this));

    delete this;

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : sendData
��������  : ������ݰ�װ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendData()
{
    SVS_TRACE();

    // ��ֹ�ڴ����Խ��
    send_buf_[ sizeof(send_buf_) - 1 ] = '\0';
    ACE_Time_Value tv(SVS_LOG_RS_TIME);
    size_t len = ACE_OS::strlen( send_buf_ );
    if (len != (size_t)peer().send_n( send_buf_, len, &tv ))
    {
        SVS_LOG((SVS_LM_ERROR, "[DEBUG]Fail to send data. peer addr[%s], data[%s], this[0x%08x].",
            str_peer_addr_,
            send_buf_,
            this));
        return SVS_RESULT_FAILURE;
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
�� �� ��  : handle_input
��������  : �������
�������  : ACE_HANDLE
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::handle_input(ACE_HANDLE /*handle*/)
{
    SVS_TRACE();

    ACE_Time_Value tv(SVS_LOG_RS_TIME);
    int32_t recvResult = peer().recv( recv_buf_, SVS_LOG_RECV_BUF_MAX - 1, &tv );

    if (0 >= recvResult)
    {
        SVS_LOG((SVS_LM_ERROR,
            "[DEBUG]Fail to recv data. peer addr[%s], this[0x%08x], return code[%d].",
            str_peer_addr_,
            this,
            recvResult ));
        return SVS_RESULT_FAILURE;
    }

    // ���Ӷ�ctrl+c�Ĵ���,ֱ�ӹر�����
    if (parseAbortHandle())
    {
        return SVS_RESULT_FAILURE;
    }

    recv_buf_[recvResult] = '\0';
    SVS_LOG((SVS_LM_DEBUG, "[DEBUG]Recv message. peer addr[%s], msg[%s].",
        str_peer_addr_,
        recv_buf_));

    string strCmd = recv_buf_;

    // ȥ������
    delChangelineCharacter(strCmd);

    // ����Ϊ��
    if (strCmd.empty())
    {
        return sendSpaceCmd();
    }

    // �ظ���һ������
    if( string::npos != strCmd.find( "[A" ) )
    {
        strCmd = last_exec_cmd_;
    }
    else
    {
        last_exec_cmd_ = strCmd;
    }

    // ���˳�����
    if ((strCmd == "q") || (strCmd == "quit"))
    {
        return SVS_RESULT_FAILURE;
    }
    else if ((strCmd == "x") || (strCmd == "exit") )
    {
        return SVS_RESULT_FAILURE;
    }

    // �������ĵ�������
    return parseDebugCmd(strCmd);
}

/*****************************************************************************
�� �� ��  : sendHelpInfo
��������  : ���ͷ�����������
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendHelpInfo()
{
    SVS_TRACE();
    (void)ACE_OS::snprintf( send_buf_,
                      sizeof(send_buf_),
                      "\nHelp Info: \n\t"
                      "help                     Show Help Info\n\t"
                      "show                     Show Server Basic Configuration\n\t"
                      "show debug               Show Debug Info\n\t"
                      "show all device          Show all Camera Info(Short Command:a)\n\t"
                      "show define              Show Basic Param Definition\n\t"
                      "show all lens            Show all Camera Basic Info(Short Command:l)\n\t"
                      "show buffer              Show Buffer Info(Short Command:b)\n\t"
                      "show raid                Show Raid Info(Short Command:r)\n\t"
                      "info all device          Show all Camera Detail Info(Short Command:d)\n\t"
                      "quit                     Quit this Help System(Short Command:q)\n\t"
                      "exit                     Quit this Help System(Short Command:x)\n\t"
                      "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendExhelpInfo
��������  : ��չhelp����
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendExhelpInfo()
{
    SVS_TRACE();
    (void)ACE_OS::snprintf( send_buf_,
                      sizeof(send_buf_),
                      "\nExtend help info: \n\t"
                      "exhelp           Show Extend Help Info\n\t"
                      "k                Show all Backup Device Info\n\t"
                      "show bdevice     Show Backup Device Info eg.[show bdevice 75500001010100000002]\n\t"
                      "show device      Show Camera Info, eg.[show device 75500001010100000002]\n\t"
                      "s                Show Message Buffder Using Info\n\t"
                      "m                Show Message Count Info\n\t"
                      "show db pool     Show Database Connection Pool Infomation\n\t"
                      "show all handle  Show using handle Infomation\n\t"
                      "list             Show camera list on raid\n\t"
                      "queue info       Show key queue information\n\t"
                      "conf             Show main configuration list\n\t"
                      "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendBasicInfo
��������  : ���ͷ�����������
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendBasicInfo()
{
    SVS_TRACE();

    CAC_RT_Record_Server* pServer = CAC_RT_Record_Server::instance();
    (void)ACE_OS::snprintf( send_buf_,
                      sizeof(send_buf_),
                      "\nRealtime Record Server Basic Configuration: \n\t"
                      "ID                          [%s]\n\t"
                      "IP                          [%s]\n\t"
                      "Max Camera Num              [%d]\n\t"
                      "Del-record Strategy Type    [%d]\n\t"
                      "StoreSvrIP                  [%s:%d]\n\t"
                      "Current Status              [%d]\n\t"
                      "ServerType                  [%d]\n\t"
                      "ServerVersion               [%s]\n\t"
                      "\nsvs_mru_record-->",
                      pServer->str_id(),
                      pServer->str_local_ip(),
                      pServer->max_lens(),
                      pServer->delete_record_strategy(),
                      pServer->recordctrl_ip(),
                      pServer->recordctrl_port(),
                      pServer->state(),
                      pServer->server_type(),
                      SVR_VERSION_INFO);
    return sendData();
}

/*****************************************************************************
�� �� ��  : sendBadCmd
��������  : ���ʹ�����Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendBadCmd()
{
    SVS_TRACE();
    (void)ACE_OS::snprintf( send_buf_,
                      sizeof(send_buf_),
                      "\nUnsupported command, please enter [help] to get help info\n\t"
                      "\nsvs_mru_record-->" );
    return sendData();
}

/*****************************************************************************
�� �� ��  : sendSpaceCmd
��������  : ���Ϳ�������Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendSpaceCmd()
{
    SVS_TRACE();

    (void)ACE_OS::snprintf( send_buf_,
                      sizeof(send_buf_),
                      "svs_mu_record-->" );
    return sendData();
}

/*****************************************************************************
�� �� ��  : sendDebugInfo
��������  : ���͵��Ի�����
�������  : void *
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendDebugInfo()
{
    SVS_TRACE();

    (void)ACE_OS::snprintf( send_buf_,
                      sizeof(send_buf_),
                      "\nDebug Basic Configuration: \n\t"
                      "Log Output Level   [%d]\n\t"
                      "Log Output Switch  [%d]\n\t"
                      "\nsvs_mru_record-->",
                      SVS_Log_Msg::get_log_priority(),
                      SVS_Log_Msg::get_log_flag());

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendDeviceDebugInfo
��������  : ���;�ͷ����Ϣ
�������  : const char *pDeviceID:��ͷID
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendDeviceDebugInfo( const char *pDeviceID )
{
    SVS_TRACE();
    send_buf_[0] = '\0';
    (void)CAC_RT_Record_Device_Manager::instance()->debug_get_device_info(
        pDeviceID,
        send_buf_,
        sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendAllHanldeInfo
��������  : ��������handle��Ϣ
�������  : NA
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendAllHanldeInfo()
{
    ACE_HANDLE debugHandle = peer().get_handle();
    (void)SVS_Stat_Manager::instance().statAllHandleInfo((int32_t)debugHandle);

    (void)ACE_OS::snprintf( send_buf_, sizeof(send_buf_), "\nsvs_mru_record-->" );

    return sendData();
}
/*****************************************************************************
�� �� ��  : sendBakupDeviceDebugInfo
��������  : ���ͱ��ݾ�ͷ����Ϣ
�������  : const char *pDeviceID:��ͷID
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendBakupDeviceDebugInfo( const char *pDeviceID )
{
    SVS_TRACE();
    send_buf_[0] = '\0';
    (void)CAC_Bakup_Record_Dev_Manager::instance()->debug_get_device_info(
        pDeviceID,
        send_buf_,
        sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendAllDeviceDebugInfo
��������  : �������о�ͷ����Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendAllDeviceDebugInfo( )
{
    SVS_TRACE();

    send_buf_[0] = '\0';
    (void)CAC_RT_Record_Device_Manager::instance()->debug_get_all_device_info(
        send_buf_,
        sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendDefineInfo
��������  : ��ʾ���������Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendDefineInfo()
{
    SVS_TRACE();

    (void)ACE_OS::snprintf(   send_buf_,
                        sizeof(send_buf_),
                        "\nBasic Param Definition: \n\t\n\t"
                        "Record Type Definition\n\t"
                        "[%d]    Forbid Record\n\t"
                        "[%d]    Always Record\n\t"
                        "[%d]    Event  Record\n\t"
                        "[%d]    Manual Record\n\t\n\t"
                        "Camera Status Definition\n\t"
                        "[%d]    Init\n\t"
                        "[%d]    Waiting for strategy taking effect\n\t"
                        "[%d]    Disk full,waiting\n\t"
                        "[%d]    Request ScheduleSvr for route\n\t"
                        "[%d]    Connect VTDU, Informing recv port\n\t"
                        "[%d]    Recieving stream\n\t"
                        "[%d]    Waiting for over\n\t"
                        "[%d]   Stop\n\t"
                        "\nsvs_mru_record-->",
                        CAC_Realtime_Record_Device::RECORD_TYPE_PROBID,
                        CAC_Realtime_Record_Device::RECORD_TYPE_ALWAYS,
                        CAC_Realtime_Record_Device::RECORD_TYPE_EVENT,
                        CAC_Realtime_Record_Device::RECORD_TYPE_MANUAL,
                        CAC_Realtime_Record_Device::RECORD_STATE_INIT,
                        CAC_Realtime_Record_Device::RECORD_STATE_SLEEP,
                        CAC_Realtime_Record_Device::RECORD_STATE_DISK_FULL,
                        CAC_Realtime_Record_Device::RECORD_STATE_REQUEST_ROUTER,
                        CAC_Realtime_Record_Device::RECORD_STATE_CONNECT_VTDU,
                        CAC_Realtime_Record_Device::RECORD_STATE_RECEIVING_FILE,
                        CAC_Realtime_Record_Device::RECORD_STATE_WAITING_OVER,
                        SVS_FRONT_DEVICE_STATE_STOP );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendAllLensDebugInfo
��������  : ������ͷ��Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendAllLensDebugInfo()
{
    SVS_TRACE();

    send_buf_[0] = '\0';
    (void)CAC_Record_Inform_Manager::instance()->debug_get_all_lens_info(
        send_buf_,
        sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendBufferDebugInfo
��������  : ���ͻ�������Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendBufferDebugInfo()
{
    ACE_HANDLE debugHandle = peer().get_handle();
    (void)SVS_Stat_Manager::instance().statMediaBufferStat((int32_t)debugHandle);

    (void)ACE_OS::snprintf( send_buf_, sizeof(send_buf_), "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendQueueInfo
��������  : ������Ҫ�Ķ�����Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendQueueInfo()
{
    ACE_HANDLE debugHandle = peer().get_handle();
    (void)SVS_Stat_Manager::instance().statQueueInfo((int32_t)debugHandle);

    (void)ACE_OS::snprintf( send_buf_, sizeof(send_buf_), "\nsvs_mru_record-->" );

    return sendData();
}

int32_t CSVS_Debug_Accept_Handler::sendConfInfo()
{
    ACE_HANDLE debugHandle = peer().get_handle();
    (void)SVS_Stat_Manager::instance().statConfInfo((int32_t)debugHandle);

    (void)ACE_OS::snprintf(send_buf_, sizeof(send_buf_), "\nsvs_mru_record-->");

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendAllDeviceDebugDetailInfo
��������  : �������о�ͷ��ϸ��Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendAllDeviceDebugDetailInfo()
{
    SVS_TRACE();

    send_buf_[0] = '\0';
    (void)CAC_RT_Record_Device_Manager::instance()->debug_get_all_device_detail_info(
        send_buf_,
        sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendAllBakupDeviceDebugDetialInfo
��������  : �����������ڱ���ǰ��¼��ͷ��Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendAllBakupDeviceDebugDetialInfo()
{
    SVS_TRACE();
    send_buf_[0] = '\0';
    (void)CAC_Bakup_Record_Dev_Manager::instance()->debug_all_bakup_device_info(
        send_buf_,
        sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendMsgBufferDebugInfo
��������  : ��ӡ��Ϣ������ʹ�����
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendMsgBufferDebugInfo()
{
    SVS_TRACE();
    send_buf_[0] = '\0';
    (void)CSVS_Mb_Buffer_Manager::instance()->debug_get_buffer_info(
        send_buf_,
        sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendMsgDebugInfo
��������  : ��ӡ��Ϣ�������
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendMsgDebugInfo()
{
    SVS_TRACE();
    send_buf_[0] = '\0';
    (void)CAC_RT_Record_Server::instance()->debug_get_msg_info(
        send_buf_,
        sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}


int32_t CSVS_Debug_Accept_Handler::sendDbConnctionDebugInfo()
{
    SVS_TRACE();
    send_buf_[0] = '\0';

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendRaidListInfo
��������  : ��ӡraid����Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendRaidListInfo()
{
    send_buf_[0] = '\0';
    (void)SVS_Disk_Manager::instance().debugGetRaidListInfo(send_buf_, sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

/*****************************************************************************
�� �� ��  : sendWriteThreadInfo
��������  : ��ӡд¼���߳��ϵľ�ͷ������Ϣ
�������  : ��
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::sendWriteThreadInfo()
{
    send_buf_[0] = '\0';
    (void)SVS_Disk_Manager::instance().debugGetWriteThreadInfo(send_buf_, sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

int32_t CSVS_Debug_Accept_Handler::sendCameraListOnRaid()
{
    send_buf_[0] = '\0';
    (void)SVS_Disk_Manager::instance().statCamreaListOnRaid(send_buf_, sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

int32_t CSVS_Debug_Accept_Handler::sendStatCameraTail()
{
//    send_buf_[0] = '\0';
//    (void)SVS_Stat_Manager::instance().statAllCameras(send_buf_, sizeof(send_buf_));

    size_t len = ACE_OS::strlen( send_buf_ );
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->" );

    return sendData();
}

int32_t CSVS_Debug_Accept_Handler::sendResendIndexInfo()
{
    send_buf_[0] = '\0';
    CIndexResendMgr::instance()->DebugGetResendIndexInfo(send_buf_, sizeof(send_buf_));
    size_t len = ACE_OS::strlen( send_buf_);
    (void)ACE_OS::snprintf( &send_buf_[len], sizeof(send_buf_) - len, "\nsvs_mru_record-->");

    return sendData();
}

void CSVS_Debug_Accept_Handler::sendVersion()
{
    SVS_TRACE();

    time_t now_time;
    (void)time(&now_time);

    char now_time_buffer[128] = {0};
    (void)ACE_OS::ctime_r(&now_time, now_time_buffer, sizeof now_time_buffer);

    char run_time_buffer[128] = {0};;
    (void)ACE_OS::ctime_r(&g_ulStartTime, run_time_buffer, sizeof run_time_buffer);


    (void)ACE_OS::snprintf(
        send_buf_, sizeof send_buf_,
        "\r\n\t==============================================================="
        "\r\n\r\n\t Welcome to use [Realtime Record Server] Debuger V1.0 from %s."
        "\r\n\r\n\t version [%s] build time [%s, %s]"
        "\r\n\r\n\t\t ServerRun: %s"
        "\r\n\t\t ServerNow: %s"
        "\r\n\t==============================================================="
        "\r\nsvs_mru_record-->",
        str_peer_addr_,
        SVR_VERSION_INFO, __DATE__, __TIME__,
        run_time_buffer,
        now_time_buffer);

    (void)sendData();

    return;
}
/*****************************************************************************
�� �� ��  : delChangelineCharacter
��������  : �������ȥ������
�������  : string& strCmd:����ؼ���
�������  : ��
�� �� ֵ  : ��
�޸���ʷ  :
*****************************************************************************/
void CSVS_Debug_Accept_Handler::delChangelineCharacter(string &strCmd)const
{
    SVS_TRACE();

    string::size_type pos;
    while ((pos = strCmd.find( "\r\n" )) != string::npos)
    {
        (void)strCmd.replace( pos, 2, "" );
    }

    while ((pos = strCmd.find( "\n" )) != string::npos)
    {
        (void)strCmd.replace( pos, 1, "" );
    }
    return;
}

/*****************************************************************************
�� �� ��  : parseAbortHandle
��������  : �ж��Ƿ�Ϊ"ctrl+c"
�������  : ��
�������  : ��
�� �� ֵ  : ��:true ��:false
�޸���ʷ  :
*****************************************************************************/
bool CSVS_Debug_Accept_Handler::parseAbortHandle()const
{
    SVS_TRACE();

    // �����"ctrl+c"
    if (0XFF == static_cast<uint8_t>(recv_buf_[0])
        && 0XF4 == static_cast<uint8_t>(recv_buf_[1])
        && 0XFF == static_cast<uint8_t>(recv_buf_[2])
        && 0XFD == static_cast<uint8_t>(recv_buf_[3])
        && 0X06 == static_cast<uint8_t>(recv_buf_[4]))
    {
        return true;
    }

    return false;
}

/*****************************************************************************
�� �� ��  : parseDebugCmd
��������  : ���������������
�������  : const string& strCmd:����ؼ���
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::parseDebugCmd(const string& strCmd)
{
    SVS_TRACE();

    int32_t ret = SVS_RESULT_FAILURE;
    // ϵͳ����
    ret = parseSystemCmd(strCmd);

    // ��ͨ��¼������
    if(SVS_DEBUG_CMD_UNFOUND == ret)
    {
        ret = parseRecordCmd(strCmd);
    }

    // ��չ����
    if(SVS_DEBUG_CMD_UNFOUND == ret)
    {
        ret = parseExtendCmd(strCmd);
    }

    // ͳ������
    if(SVS_DEBUG_CMD_UNFOUND == ret)
    {
        ret = parseStatCmd(strCmd);
    }

    // ��֧�ֵ�����
    if(SVS_DEBUG_CMD_UNFOUND == ret)
    {
        return sendBadCmd();
    }

    // ��ʾ�ҵ�������,������ʵ���
    return ret;
}

/*****************************************************************************
 �� �� ��  : parseRecordCmd
 ��������  : һ��¼������
 �������  : const string& strCmd:����ؼ���
 �������  : ��
 �� �� ֵ  : �ɹ�:0 ʧ��:-1
 �޸���ʷ  :
 *****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::parseRecordCmd(const string& strCmd)
{
    SVS_TRACE();

    // ��ʼ��Ϊû���ҵ�������
    int32_t ret = SVS_DEBUG_CMD_UNFOUND;
    if (strCmd.find( "show device " ) == 0)
    {
        string strDeviceID = strCmd.substr( 12 );
        if (strDeviceID.empty())
        {
            ret =  sendBadCmd();
        }

        ret = sendDeviceDebugInfo( strDeviceID.c_str());
    }
    else if ( (strCmd == "a") || (strCmd.find( "show all device" ) == 0) )
    {
        ret = sendAllDeviceDebugInfo();
    }
    else if ( (strCmd == "l") || (strCmd.find( "show all lens" )) == 0 )
    {
        ret = sendAllLensDebugInfo();
    }
    else if ( (strCmd == "b") || (strCmd.find( "show buffer" ) ) == 0 )
    {
        ret = sendBufferDebugInfo();
    }
    else if ( (strCmd == "d") || (strCmd.find( "info all device" ) ) == 0 )
    {
        ret = sendAllDeviceDebugDetailInfo();
    }
    else if ( (strCmd == "r") || (strCmd.find( "show raid" ) ) == 0 )
    {
        ret = sendRaidListInfo();
    }
    else if ( (strCmd == "wt") || (strCmd.find( "write thread info" ) ) == 0 )
    {
        ret = sendWriteThreadInfo();
    }

    return ret;
}

/*****************************************************************************
�� �� ��  : parseExtendCmd
��������  : ��չ����
�������  : const string& strCmd:����ؼ���
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::parseExtendCmd(const string& strCmd)
{
    SVS_TRACE();

    // ��ʼ��Ϊû���ҵ�������
    int32_t ret = SVS_DEBUG_CMD_UNFOUND;
    if (strCmd.find( "show bdevice " ) == 0)
    {
        string strDeviceID = strCmd.substr( 13 );
        if (strDeviceID.empty())
        {
            ret = sendBadCmd();
        }
        ret = sendBakupDeviceDebugInfo( strDeviceID.c_str());
    }
    else if (strCmd == "k")
    {
        ret = sendAllBakupDeviceDebugDetialInfo();
    }
    else if (strCmd == "s")
    {
        ret = sendMsgBufferDebugInfo();
    }
    else if (strCmd == "m")
    {
        ret = sendMsgDebugInfo();
    }
    else if(strCmd.find( "show db pool" ) == 0)
    {
        ret = sendDbConnctionDebugInfo();
    }
    else if(strCmd.find( "show all handle" ) == 0)
    {
        ret = sendAllHanldeInfo();
    }
    else if(strCmd.find( "list") == 0)
    {
        ret = sendCameraListOnRaid();
    }
    else if(strCmd.find("queue info") == 0)
    {
        ret = sendQueueInfo();
    }
    else if(strCmd.find("conf") == 0)
    {
        ret = sendConfInfo();
    }
    return ret;
}

/*****************************************************************************
�� �� ��  : parseSystemCmd
��������  : ϵͳ������Ϣ
�������  : const string& strCmd:����ؼ���
�������  : ��
�� �� ֵ  : �ɹ�:0 ʧ��:-1
�޸���ʷ  :
*****************************************************************************/
int32_t CSVS_Debug_Accept_Handler::parseSystemCmd(const string& strCmd)
{
    SVS_TRACE();

    // ��ʼ��Ϊû���ҵ�������
    int32_t ret = SVS_DEBUG_CMD_UNFOUND;
    // ϵͳ����
    if (strCmd == "help")
    {
        ret = sendHelpInfo();
    }
    else if ( strCmd == "exhelp" )
    {
        ret = sendExhelpInfo();
    }
    else if (strCmd == "show")
    {
        ret = sendBasicInfo();
    }
    else if (strCmd == "show debug")
    {
        ret = sendDebugInfo();
    }
    else if ( strCmd == "show define" )
    {
        ret = sendDefineInfo();
    }

    return ret;
}

int32_t CSVS_Debug_Accept_Handler::parseStatCmd(const string& strCmd)
{
    // ��ʼ��Ϊû���ҵ�������
    int32_t ret = SVS_DEBUG_CMD_UNFOUND;
    // ϵͳ����
    if (strCmd == "stat camera tail")
    {
        ret = sendStatCameraTail();
    }
    else if (strCmd == "o")
    {
        ret = sendResendIndexInfo();
    }

    return ret;
}

