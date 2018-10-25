#include "vms/vms.h"
#include "svs_config.h"
#include "svs_timer.h"
#include "svs_log_msg.h"
#include "svs_stat_manager.h"
#include "svs_record_disk.h"
#include "svs_recv_stream.h"
#include "svs_recv_stream_factory.h"
#include "svs_record_index.h"
#include "svs_real_record_lens.h"
#include "svs_record_inform.h"
#include "svs_real_record_task.h"
#include "svs_real_record_server.h"
#include "svs_mb_buffer.h"
#include "svs_bakup_front_record_lens.h"
#include "svs_io_scheduler.h"
#include "svs_disk_manager.h"
#include "svs_rt_record_common.h"
#include "svs_recv_stream.h"
#include "svs_index_list.h"
#include "svs_file_egress.h"
#include "svs_media_processor.h"
#include "svs_daemon_thread.h"

CFluxStat g_FluxStat;   // ����ͳ��
//=========================CAC_RT_Record_Server BEGIN======================
CAC_RT_Record_Server::CAC_RT_Record_Server()
{
    b_running_              = true;
    delete_record_strategy_ = DELETE_EXPIRE_RECORD;
    max_lens_               = 0;
    max_lens_per_raid_      = 0;
    server_type_            = STREAM_TYPE_TIMINGRECORD;

    str_play_server_id_[0] = '\0';
    str_node_id_[0] = '\0';
    str_cms_index_[0] = '\0';

    // д�����¼���ļ�
    debug_write_file_       = 1;

    // д���������Ż�*/
    // ���ջ������С
    recv_media_buffer_size_     = 0;
    write_media_data_stripe_width_ = 0;
    // ���ջ��������
    recv_media_buffer_num_      = 0;
    tmpfs_recv_media_buffer_num_ = 0;
    // ����ý�����߳���
    recv_media_thread_num_      = 0;

    // д¼���ļ��߳�ģʽ��Ϊ�����̱߳���*/
    sync_work_raid_group_num_ = SYNC_WORK_RAID_GROUP_DEFAULT_NUM;
    output_thread_num_per_raid_ = WRITE_MEDIA_DATA_DEFAULT_THREAD_NUM;

    transaction_no_             = 1;
    storage_type_               = STORAGE_TYPE_MASTER;

    event_record_lost_timeout_ = SVS_EVENT_RECORD_LOST_DEFAULT_TIMEOUT;

    config_file_last_moded_time_ = 0;

    (void)ACE_OS::memset(record_indexfile_path_, 0 ,sizeof (record_indexfile_path_));
    (void)ACE_OS::memset(record_storage_path_, 0 ,sizeof (record_storage_path_));
    (void)ACE_OS::memset(old_index_path_, 0 ,sizeof (old_index_path_));
    index_path_update_time_ = 0;
    disk_lower_limit_ = 0;
    disk_alarm_resume_limit_ = 0;
    disk_alarm_limit_ = 0;

    check_thread_status_interval_ = DEFAULT_INTERVAL;
    restart_server_flag_          = 0;
    dump_server_flag_             = 0;

    use_direct_io_flag_           = 1;
    write_media_flowctrl_flag_ = 1;
    check_disk_interval_ = CHECK_DISK_INTERVAL_DEFAULT;

    load_report_interval_ = 0;
    load_report_interval_timer_ = -1;
    test_bandwidth_total_ = 0;
    (void)ACE_OS::memset(str_conf_buffer_, 0, sizeof(str_conf_buffer_));
    (void)ACE_OS::memset(tmpfs_path_, 0, sizeof(tmpfs_path_));
    m_ulRecordTimeTick = RT_RECORD_TIMETICK_PU;

}

CAC_RT_Record_Server::~CAC_RT_Record_Server()
{
    try
    {
        SVS_LOG(( SVS_LM_DEBUG,
            "[OBJ_DESTROY] ~CAC_RT_Record_Server, this:0x%08x.",
            this));
    }catch(...)
    {
    }
}

/*****************************************************************************
 �� �� ��  : open_service
 ��������  : ��RtRecordSvr����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true,ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Server::open_service()
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_INFO, "The service of RtRecordSvr start."));

    // �����ô�ӡ
    SVS_LOG((SVS_LM_INFO,
        "RtRecordSvr's basal configuration:ServerId[%s] Service IP[%s] "
        "Max lens number[%u] Deleting record stratety[%u] "
        "Lan address[%s:%u]",
        this->str_id(),
        this->str_local_ip(),
        this->max_lens(),
        this->delete_record_strategy(),
        this->recordctrl_ip(),
        this->recordctrl_port() ));

    //��ʼ��һЩ��ʱ������Ϣ����
    int32_t iRet = signal_task_.init_task();
    if( 0 != iRet)
    {
        SVS_LOG((SVS_LM_ERROR,
            "Open signal task failed, as init task error. "
            "return code[%d].",
            iRet));
        return false;
    }

    //��ʼ����������
    int32_t nRet = SVS_Disk_Manager::instance().initialize(
                                    (int32_t)check_disk_interval_,
                                    disk_lower_limit_,
                                    disk_alarm_limit_,
                                    disk_alarm_resume_limit_);
    if ( 0 != nRet )
    {
        SVS_LOG((SVS_LM_ERROR, "Initialize disk manager failed."));
        return false;
    }

    nRet = SVS_Disk_Manager::instance().refreshDiskList(record_storage_path_);
    if ( 0 != nRet )
    {
        SVS_LOG((SVS_LM_ERROR, "Refresh disk list failed."));
        return false;
    }

    char strTime[SVS_STR_TIME_MAX_LEN] = {0,};
    (void)SVS_SS_UTILITIES::time2str(strTime, sizeof(strTime), index_path_update_time_);
    SVS_LOG((SVS_LM_INFO,
        "Record index file information:"
        "new index file path[%s], old index file path[%s], "
        "index file path update time[%u:%s]",
        record_indexfile_path_,
        old_index_path_,
        index_path_update_time_,
        strTime));

    // ��ʼ��ý�崦����
    if (SVS_RESULT_OK != SVS_Media_Processor::instance().open((void *)0))
    {
        SVS_LOG((SVS_LM_ERROR, "Initialize media processor failed."));
        return false;
    }

    // ��ʼ��ǰ���豸�����࣬��ʼ���豸¼�����
    if (!CAC_Record_Inform_Manager::instance()->init(max_lens_))
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Initialize the record informing manager failed."));

        return false;
    }

    // ��ʼ��¼���豸������, ����Realtime_Record_Device����
    if (!CAC_RT_Record_Device_Manager::instance()->init(max_lens_))
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Initialize the camera manager failed."));

        return false;
    }

    // ��ʼ��socket������
    if (0 != CAC_Recv_Stream_Factory::instance().init(max_lens_, recv_media_thread_num_))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Initialize receiving media stream factory failed."));
        return false;
    }

    // ��ʼ��ǰ��¼���豸������
    if (!CAC_Bakup_Record_Dev_Manager::instance()->init(max_lens_))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Initialize the backuping camera manager failed."));
        return false;
    }

    // ��ʼ���߳���
    if (0 != signal_task_.open(NULL))
    {
        SVS_LOG((SVS_LM_CRITICAL, "Initialize the signal task failed."));
        return false;
    }

    return true;
}

/*****************************************************************************
 �� �� ��  : init_configure
 ��������  : ��ʼ�������ļ�
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true,ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Server::init_configure(const char * const)
{
    SVS_TRACE();
    const char *strServerName = "SVS_RECORD";

    char strBuf[READ_CONFIG_STRING_BUF_LEN];
    CAC_Config sys_config(CONF_FILE);

    char *endPtr = NULL;
    bool initResult = CSVS_Record_Server::init_configure(CONF_FILE);
    if ( !initResult )
    {
        return false;
    }

    int32_t checkIdResult = CheckDeviceID(  str_rt_server_id_);
    if ( CHECK_OK != checkIdResult )
    {
        SVS_LOG((SVS_LM_ERROR, "RtRecordSvr servr id is invalid."));

        return false;
    }

    debug_write_file_ = 1;
    if (0 == sys_config.get(strServerName, "DebugWriteFile", strBuf))
    {
        debug_write_file_ = atoi(strBuf);
    }

    test_bandwidth_total_ = 0;
    if(0 == sys_config.get(strServerName, "TestTotalBandwidth", strBuf))
    {
        test_bandwidth_total_ = (uint32_t)atol(strBuf);
    }

    if(!init_system_configure())
    {
        return false;
    }

    if (!init_record_event_configure())
    {
        return false;
    }

    storage_type_ = STORAGE_TYPE_MASTER;

    uint16_t debugPort = SVS_DEFALT_REMOTE_DEBUG_PORT;
    if (0 == sys_config.get( "SVS_DEBUG", "DebugPort", strBuf))
    {
        debugPort = (uint16_t)ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }
    signal_task_.remote_debug_port( debugPort );


    int32_t lensNum = 0;
    ConfAttribute szAttrbt;
    szAttrbt.lMaxValue = RECORD_MAX_LENS_NUM;
    szAttrbt.lMinValue = 1;
    szAttrbt.lDefaultValue = DEFAULT_MAX_LENS_NUM;
    szAttrbt.enDefaultValueType = get_conf_type(USE_DEFAULT_VALUE_GET_FAIL, USE_NOTHING_OVER_RANGE);
    if (!get_conf_integer(strServerName, "MaxLens",
                            szAttrbt, lensNum))
    {
        return false;
    }

    max_lens_ = (uint32_t)lensNum;

    // ��ʼ��ý�建�������������
    if (!init_media_buffer_configure())
    {
        return false;
    }

    // ��ʼ��¼��ɾ�����������
    if (!init_media_thread_configure())
    {
        return false;
    }

    // ��ʼ��¼��ɾ�����������
    if (!init_delete_record_configure())
    {
        return false;
    }

    m_ulRecordTimeTick = RT_RECORD_TIMETICK_LOCAL;
    if (0 == sys_config.get( "SVS_RECORD", "RecordTimeTick", strBuf))
    {
        m_ulRecordTimeTick = (uint32_t)ACE_OS::strtoul( strBuf, &endPtr, 10 );
        if(RT_RECORD_TIMETICK_PU < m_ulRecordTimeTick)
        {
            m_ulRecordTimeTick = RT_RECORD_TIMETICK_PU;
        }
    }

    SVS_LOG((SVS_LM_DEBUG,"Set Record Timetick flag:[%d]",m_ulRecordTimeTick));

    // ��ʼ��¼��洢��Ϣ���뱣֤¼��ɾ��ģʽ�Ѷ�
    if (!init_record_storagy_configure())
    {
        return false;
    }

    g_FluxStat.SetStatPara( recv_media_thread_num_,
                              output_thread_num_per_raid_ * sync_work_raid_group_num_,
                              recv_media_buffer_size_ );
    return true;
}

/*****************************************************************************
 �� �� ��  : refresh_configure
 ��������  : ˢ�¶�̬����������ж��ļ��޸�ʱ���Ƿ��б仯������������¶�ȡ
             ��̬����������
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Server::refresh_configure()
{
    struct stat fileStat;
    int32_t iRet = stat(CONF_FILE, &fileStat);
    // ����ȡ�����ļ���Ϣʧ�ܣ�ֱ�ӷ���
    if (0 != iRet)
    {
        SVS_LOG((SVS_LM_WARNING,
            "Refresh configuration failed.Stat the configuration file failed."
            "configuration file name[%s].",
            CONF_FILE));
        return SVS_RESULT_OK;
    }

    // ��������ļ�����޸�ʱ��δ�仯����˵������û�и���
    if ( config_file_last_moded_time_ == fileStat.st_mtime)
    {
        return SVS_RESULT_OK;
    }

    SVS_LOG((SVS_LM_WARNING,
        "Configuration file is refreshed.last time[%u], this time[%u]",
        config_file_last_moded_time_,
        fileStat.st_mtime));

    // ���������ļ�����޸�ʱ��
    config_file_last_moded_time_ = fileStat.st_mtime;
    // 1.�¼�¼�������Чʱ�����ö�̬����
    const char *strServerName = "SVS_RECORD";
    char strBuf[READ_CONFIG_STRING_BUF_LEN];
    uint32_t tempTimeOut;
    char *endPtr = NULL;
    CAC_Config sys_config(CONF_FILE);

    // ����ȡ������ʧ�ܣ��򲻸���
    if (0 != sys_config.get(strServerName, "StopRecordEventLostTimeout", strBuf))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Refresh the item [StopRecordEventLostTimeout] from configuration file failed."
            "Read the item failed."));
    }
    else
    {
        tempTimeOut = ACE_OS::strtoul( strBuf, &endPtr, 10 );

        if ( tempTimeOut < SVS_EVENT_RECORD_LOST_MIN_TIMEOUT )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Refresh the item [StopRecordEventLostTimeout] from configuration file failed,"
                "The item value[%d] is too small.",
                tempTimeOut));
        }
        else if ( tempTimeOut > SVS_EVENT_RECORD_LOST_MAX_TIMEOUT )
        {
            SVS_LOG((SVS_LM_ERROR,
                "Refresh the item [StopRecordEventLostTimeout] from configuration file failed,"
                "The item valude[%d] is too big.",
                tempTimeOut));
        }
        else
        {
            //������ת��������
            tempTimeOut = TIME_UNIT_MINUTE * tempTimeOut;
            (void)CAC_RT_Record_Device_Manager::instance()->refresh_event_record_outtime((int32_t)(tempTimeOut - event_record_lost_timeout_));
            SVS_LOG((SVS_LM_WARNING,
                "Refresh [StopRecordEventLostTimeout] from configuration file succeeded."
                "[%d]s ==> [%d]s .",
                event_record_lost_timeout_,
                tempTimeOut));

            // �����¼�¼����Чʱ������
            event_record_lost_timeout_ = tempTimeOut;
        }
    }

    uint32_t loadReportInterval = load_report_interval_;

    // ��ʱˢ��ϵͳ����
    if (!init_system_configure())
    {
        return SVS_RESULT_FAILURE;
    }

    // ˢ�¼���̼߳������
    CSVS_Daemon_Thread *pDaemonThread = CSVS_Daemon_Thread::instance();
    if (NULL != pDaemonThread)
    {
        (void)pDaemonThread->reConfig(check_thread_status_interval_,
                                        restart_server_flag_,
                                        dump_server_flag_);
    }

    // ����ϱ������б仯���򴴽��¶�ʱ��
    if(load_report_interval_ != loadReportInterval)
    {
        (void)set_load_report_interval_timer();
    }

    // ˢ�´�������
    // ��ȡ¼���ļ��洢·��
    memset(str_conf_buffer_, 0 , sizeof(str_conf_buffer_));
    if (0 != sys_config.get(strServerName, "RecordFilePath", str_conf_buffer_,true ,sizeof(str_conf_buffer_)))
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "Read the item [RecordFilePath] from configuration file failed."
            "configuration file name [%s]",
            CONF_FILE));

        return SVS_RESULT_OK;
    }

    // ����ԭ�������ò�һ����ˢ�´����б�,�����汾�ε�����ֵ
    if (0 != strcmp(str_conf_buffer_, record_storage_path_))
    {
        SVS_LOG((SVS_LM_WARNING,
            "The configuration of disk list has been changed,so begin to refresh disk list."
            "old conf[%s], new conf[%s].",
            record_storage_path_, str_conf_buffer_));

        (void)snprintf(record_storage_path_, sizeof(record_storage_path_), str_conf_buffer_);
        (void)SVS_Disk_Manager::instance().refreshDiskList(record_storage_path_);
    }

    return SVS_RESULT_OK;
}

/*****************************************************************************
 �� �� ��  : end_reactor_event_loop
 ��������  : ��̽���֪ͨ��Ӧ�߳̽���
 �������  : NA
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CAC_RT_Record_Server::end_reactor_event_loop()
{
    SVS_TRACE();
    // ��ֹͣ��־
    b_running_ = false;

    ACE_Message_Block *mb = NULL;
    try
    {
        mb = new ACE_Message_Block(0, ACE_Message_Block::MB_STOP);
    }
    catch(...)
    {
        SVS_LOG( (SVS_LM_CRITICAL,
            "End the threads failed.Create message block failed ." ) );
        return ;
    }

    ACE_Time_Value tv(ACE_OS::gettimeofday().sec() + SVS_ENQUEUE_TIMEOUT, 0);
    if ((NULL != p_connector_)
        && (-1 == p_connector_->msg_queue()->enqueue_tail(mb, &tv)))
    {
        SVS_LOG((SVS_LM_ERROR,
            "End the reactor for connecting with StoreSvr failed."
            "The queue of connection enqueue end-message failed."));

        SVS_DELETE( mb );
    }


    signal_task_.stop();

   // ��Ϣ����У���Ϣ�����ʱ���ͷ�
   return;//lint !e429
}//lint !e429

/*****************************************************************************
 �� �� ��  : stop_record
 ��������  : ָֹͣ��·�ɵ�¼��
 �������  : const ACE_Message_Block *mb:Ҫֹͣ��·������
 �������  : NA
 �� �� ֵ  : �ɹ�,���� 0 ���򷵻�-1
 �޸���ʷ  :
*****************************************************************************/
int32_t CAC_RT_Record_Server::stop_record( const ACE_Message_Block *mb ) const
{
    if ( NULL == mb )
    {
        SVS_LOG( (SVS_LM_ERROR,
            "Process stopping route message failed "
            "The parameter message block is NULL.") );
        return SVS_RESULT_FAILURE;
    }

    PSVS_STORESERVER_SCHEDULE_OVER_REQ_MSG pMsg =
        (PSVS_STORESERVER_SCHEDULE_OVER_REQ_MSG)(void*)mb->rd_ptr();
    uint32_t RouterID = pMsg->StreamId;
    SVS_LOG( (SVS_LM_INFO,
        "Process stopping route message succeeded."
        "Start to stop recording and the route id is [%u].",
        RouterID ) );

    // ֹͣʵʱ¼���·��
    int32_t result = CAC_RT_Record_Device_Manager::instance()->shutdown_router( RouterID );

    // ��Ϊ�ֱ治��·����Ϣ��ʵʱ¼����ǰ�˱���·�ɣ�������ͣһ��
    result &= CAC_Bakup_Record_Dev_Manager::instance()->stop_device(RouterID);

    return result;
}

/*****************************************************************************
 �� �� ��  : print_msg_del_task
 ��������  : ��ӡɾ��������Ϣ
 �������  : const SVS_DEL_RECORD *del_task:ɾ������
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CAC_RT_Record_Server::print_msg_del_task(const SVS_DEL_RECORD *del_task) const
{
    if(NULL == del_task)
    {
        return;
    }

    char strDelTime[TIME_STRING_LEN]={0};
    (void)SVS_SS_UTILITIES::time2str(strDelTime, sizeof(strDelTime), del_task->DeleteTime);
    SVS_LOG((SVS_LM_DEBUG,
        "[DEL]Deleting task info: DelTaskType=%d, DeleteTime=%u[%s], CameraId=%s",
        del_task->MsgType,
        del_task->DeleteTime,
        strDelTime,
        del_task->LensId));
}

/*****************************************************************************
 �� �� ��  : print_msg_del_record
 ��������  : ��ӡɾ��¼����Ϣ
 �������  : const SVS_DEL_RECORD *del_record:ɾ����Ϣ
 �������  : NA
 �� �� ֵ  : NA
 �޸���ʷ  :
*****************************************************************************/
void CAC_RT_Record_Server::print_msg_del_record(const SVS_DEL_RECORD_FILE *del_record) const
{
    if(NULL == del_record)
    {
        return;
    }

    char strStartTime[TIME_STRING_LEN]={0};
    char strEndTime[TIME_STRING_LEN]={0};
    (void)SVS_SS_UTILITIES::time2str(strStartTime, sizeof(strStartTime), del_record->StartTime);
    (void)SVS_SS_UTILITIES::time2str(strEndTime, sizeof(strEndTime), del_record->EndTime);
    SVS_LOG((SVS_LM_DEBUG,
        "[DEL]deleting record info:delType=%d, CameraId=%s, DelStartTime=%u[%s], DelEndTime=%u[%s], DelMode=%d",
        del_record->MsgType,
        del_record->LensId,
        del_record->StartTime, strStartTime,
        del_record->EndTime, strEndTime,
        del_record->DeleteMode));

}

/*****************************************************************************
 �� �� ��  : init_system_configure
 ��������  : ��ʼ��ϵͳ��ص�����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Server::init_system_configure()
{
    const char *strServerName = "SVS_RECORD";
    char strBuf[READ_CONFIG_STRING_BUF_LEN];
    char *endPtr = NULL;
    CAC_Config sys_config(CONF_FILE);
    // ��ȡʹ��DirectIO�ı�־
    use_direct_io_flag_  = 1;
    if (0 == sys_config.get(strServerName, "UseDirectIO", strBuf))
    {
        use_direct_io_flag_ = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }

    // ��ȡ����̵߳��̼߳����
    check_thread_status_interval_ = DEFAULT_INTERVAL;
    if (0 == sys_config.get(strServerName, "CheckThreadInterval", strBuf))
    {
        check_thread_status_interval_ = ACE_OS::strtoul( strBuf, &endPtr, 10 );

        if (( check_thread_status_interval_ < MIN_INTERVAL)
        || ( check_thread_status_interval_ > MAX_INTERVAL))
        {
            SVS_LOG((SVS_LM_WARNING,
                "config item[CheckThreadInterval][%u] invalid, use default value[%u].",
                check_thread_status_interval_, DEFAULT_INTERVAL));
            check_thread_status_interval_ = DEFAULT_INTERVAL;
        }
    }

    // ��ȡ�Ƿ�������������־
    restart_server_flag_ = 0;
    if (0 == sys_config.get(strServerName, "RestartServerFlag", strBuf))
    {
        restart_server_flag_ = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }

    // ��ȡ�Ƿ�ѷ�����CoreDump��־
    dump_server_flag_ = 0;
    if (0 == sys_config.get(strServerName, "DumpServerFlag", strBuf))
    {
        dump_server_flag_ = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }

    // ��ȡͳ��ģ����־����
    uint32_t nStatLogLevel = SVS_Stat_Manager::STAT_LOG_LEVEL_INFO;
    if (0 == sys_config.get(strServerName, "StatLogLevel", strBuf))
    {
        nStatLogLevel = ACE_OS::strtoul( strBuf, &endPtr, 10 );
        if(nStatLogLevel > SVS_Stat_Manager::STAT_LOG_LEVEL_CLOSEED)
        {
            SVS_LOG((SVS_LM_WARNING,
                "Stat log level invalid.invalid levle[%u],use closed level[%u].",
                nStatLogLevel,
                 SVS_Stat_Manager::STAT_LOG_LEVEL_CLOSEED));

            nStatLogLevel = SVS_Stat_Manager::STAT_LOG_LEVEL_CLOSEED;
        }
    }

    SVS_Stat_Manager::instance().refreshStatLogLeve(nStatLogLevel);

    // ��ȡͳ���ϱ�����
    int32_t reportInterval = 0;
    ConfAttribute szAttrbt;
    szAttrbt.lMaxValue = SVS_LOAD_REPORT_MAX_INTERVAL;
    szAttrbt.lMinValue = SVS_LOAD_REPORT_MIN_INTERVAL;
    szAttrbt.lDefaultValue = SVS_LOAD_REPORT_DEFAULT_INTERVAL;
    szAttrbt.enDefaultValueType = get_conf_type(USE_DEFAULT_VALUE_GET_FAIL, USE_LIKE_VALUE_OVER_RANGE);
    if (!get_conf_integer(strServerName, "LoadReportInterval",
                            szAttrbt, reportInterval))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Load report interval invalid.invalid levle[%u],use default value[%u].",
            reportInterval,
            SVS_LOAD_REPORT_DEFAULT_INTERVAL));

        reportInterval = SVS_LOAD_REPORT_DEFAULT_INTERVAL;
    }

    load_report_interval_ = (uint32_t)reportInterval;

    // ��ȡ���ؿ���
    if (0 == sys_config.get(strServerName, "WriteMediaFlowControlSwitch", strBuf))
    {
        write_media_flowctrl_flag_ = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }
    return true;
}

/*****************************************************************************
 �� �� ��  : init_record_storagy_configure
 ��������  : ��ʼ��¼��洢���������
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Server::init_record_storagy_configure()
{
    // ��ʼ������������
    if(!init_record_index_configure())
    {
        return false;
    }

    const char *strServerName = "SVS_RECORD";
    char strBuf[READ_CONFIG_STRING_BUF_LEN];
    char *endPtr = NULL;
    CAC_Config sys_config(CONF_FILE);

    // ��ȡ¼���ļ��洢·��
    if (0 != sys_config.get(strServerName, "RecordFilePath", record_storage_path_,true, sizeof(record_storage_path_)))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Read the item [RecordFilePath] from configuration file failed."
            "configuration file name [%s]",
            CONF_FILE));
        return false;
    }

    // ����ʣ��ռ�:ֹͣ¼������
    disk_lower_limit_ = DEFAULT_DISK_LOWER_LIMIT;  //��λ����
    if (0 == sys_config.get(strServerName, "DiskLowerLimit", strBuf))
    {
        disk_lower_limit_ = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }

    disk_alarm_resume_limit_ = DEFAULT_DISK_LOWER_LIMIT * 4;  //��λ����
    disk_alarm_limit_ = DEFAULT_DISK_LOWER_LIMIT * 2;  //��λ����
    // ���������Ƿ�ʽ���Ŷ�ȡ���ã����򣬱����ڸ澯���������̣����´���
    // ����ʣ��ռ�:������ǵĲ����£�����ɾ������¼�������
    if (0 == sys_config.get(strServerName, "DiskAlarmLimit", strBuf))
    {
        disk_alarm_limit_ = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }

    if (disk_lower_limit_ >= disk_alarm_limit_)
    {
        SVS_LOG((SVS_LM_ERROR,
            "The value of the item [DiskAlarmLimit] is too small."
            "It must be [DiskAlarmLimit] > [DiskLowerLimit],"
            "but actually [DiskAlarmLimit]=%u < [DiskLowerLimit]=%u.",
            disk_alarm_limit_,
            disk_lower_limit_));
        return false;
    }

    // ����ʣ��ռ�:������ǵĲ����£�ֹͣɾ������¼�������
    if (0 == sys_config.get(strServerName, "DiskAlarmResumeLimit", strBuf))
    {
        disk_alarm_resume_limit_ = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }

    if (disk_alarm_limit_ >= disk_alarm_resume_limit_)
    {
        SVS_LOG((SVS_LM_ERROR,
            "The value of the item [DiskAlarmResumeLimit] is too small."
            "It must be [DiskAlarmResumeLimit] > [DiskAlarmLimit],"
            "but actually [DiskAlarmResumeLimit]=%u <= [DiskAlarmLimit]=%u.",
            disk_alarm_resume_limit_,
            disk_alarm_limit_));
        return false;
    }

    int32_t getConfRet = 0;
    ConfAttribute szAttrbt;

    // ���̼������
    szAttrbt.lMaxValue = CHECK_DISK_INTERVAL_MAX;
    szAttrbt.lMinValue = CHECK_DISK_INTERVAL_DEFAULT;
    szAttrbt.lDefaultValue = CHECK_DISK_INTERVAL_DEFAULT;
    szAttrbt.enDefaultValueType = get_conf_type(USE_DEFAULT_VALUE_GET_FAIL, USE_DEFAULT_VALUE_OVER_RANGE);
    if (!get_conf_integer(strServerName, "CheckDiskInterval",
                            szAttrbt, getConfRet))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Check disk interval invalid.invalid value[%u],use default value[%u].",
            getConfRet,
            CHECK_DISK_INTERVAL_DEFAULT));

        getConfRet = CHECK_DISK_INTERVAL_DEFAULT;
    }

    check_disk_interval_ = (uint32_t)getConfRet;

    return true;
}

/*****************************************************************************
 �� �� ��  : init_record_index_configure
 ��������  : ��ʼ������·��������������·����ҪǨ�ƣ���Ҫ����Ǩ��ǰ������·��
             ��Ǩ�ƺ������·����������Ҫ����Ǩ��ʱ�䡣ɾ���ʱ�����Ҫɾ���¼��
             ��Ǩ��֮ǰ��¼�������ļ��ͻ���old_index_path_��Ѱ�������ļ������
             Ҫ�ҵ������ļ�����֮���ʱ�䣬����Ҫ���µ�·�����������ļ��������
             �������������Բ���������ʱ��;ɵ������ļ�·����
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Server::init_record_index_configure()
{
    const char *strServerName = "SVS_RECORD";
    char strBuf[READ_CONFIG_STRING_BUF_LEN];
    CAC_Config sys_config(CONF_FILE);

    // ��ȡ¼�������ļ����Ŀ¼��ʱ��
    index_path_update_time_ = 0;
    if (0 == sys_config.get(strServerName, "RecordIndexfilePathUpdateTime", strBuf))
    {
        index_path_update_time_ = SVS_SS_UTILITIES::str2time(strBuf);
    }

    // ��ȡ¼�������ļ����Ŀ¼
    memset(strBuf, 0, sizeof(strBuf));
    if (0 != sys_config.get(strServerName, "RecordIndexfilePath", strBuf))
    {
         SVS_LOG((SVS_LM_ERROR,
            "Read the item [RecordIndexfilePath] from configuration file failed."
            "configuration file name[%s].",
            CONF_FILE));
         return false;
    }

    char*pos = strBuf;
    for(;(!(',' == *pos || ' ' == *pos || '\0' == *pos)); ++pos)
    {
        // nothing
    }

    memset(record_indexfile_path_, 0, sizeof(record_indexfile_path_));
    strncpy(record_indexfile_path_, strBuf, (uint32_t)(pos - strBuf));

    // ȥ������·�������� '/'
    uint32_t indexpathLen = strlen(record_indexfile_path_);
    if((1 != indexpathLen) && ('/' == record_indexfile_path_[indexpathLen - 1]))
    {
        record_indexfile_path_[indexpathLen - 1] = '\0';
    }

    // �ɵ�����·��
    ++pos;
    char* startPos = pos;
    for(;(!(',' == *pos || ' ' == *pos || '\0' == *pos)); ++pos)
    {
        // nothing
    }

    memset(old_index_path_, 0, sizeof(old_index_path_));
    strncpy(old_index_path_, startPos, (uint32_t)(pos - startPos));

    return true;
}

/*****************************************************************************
 �� �� ��  : init_media_buffer_configure
 ��������  : ��ʼ��ý�建�������������
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Server::init_media_buffer_configure()
{
    const char *strServerName = "SVS_RECORD";
    char strBuf[READ_CONFIG_STRING_BUF_LEN];
    char *endPtr = NULL;
    CAC_Config sys_config(CONF_FILE);

    //��ȡ����ý�建�����С
    if (0 != sys_config.get(strServerName, "RecvMediaBufferSize", strBuf))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Read the item [RecvMediaBufferSize] from configuration file failed."
            "configuration file name[%s]",
            CONF_FILE));
        return false;
    }

    uint32_t bufferSize = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    recv_media_buffer_size_ = bufferSize;
    recv_media_buffer_size_ *= 1024;
    if (0 != recv_media_buffer_size_ % SVS_RECORD_FILE_ALIGN_LEN)
    {
        SVS_LOG((SVS_LM_ERROR,
            "the config item [RecvMediaBufferSize] = [%d] invalid,"
            "it must align with 4KB, configuration file name[%s]",
            recv_media_buffer_size_,
            CONF_FILE));
        return false;
    }

    // �Ϸ��Լ��
    if ( SVS_RECV_RT_MEDIA_DATA_BUFFER_MIN_LEN > recv_media_buffer_size_  )
    {
        SVS_LOG((SVS_LM_ERROR,
            "The value of the item [RecvMediaBufferSize] is too small."
            "It must be bigger than [%u]KB, but actually [RecvMediaBufferSize]KB = %u",
            SVS_RECV_RT_MEDIA_DATA_BUFFER_MIN_LEN/1024,
            bufferSize ));
        return false;
    }

    if ( SVS_RECV_RT_MEDIA_DATA_BUFFER_MAX_LEN < recv_media_buffer_size_  )
    {
        SVS_LOG((SVS_LM_ERROR,
            "The value of the item [RecvMediaBufferSize] is too big."
            "It must be smaller than [%u]KB, but actually [RecvMediaBufferSize]KB = %u",
            SVS_RECV_RT_MEDIA_DATA_BUFFER_MAX_LEN/1024,
            bufferSize ));
        return false;
    }

    write_media_data_stripe_width_ = recv_media_buffer_size_;

    // ��Ϊ��Ҫ����4K����
    recv_media_buffer_size_ += (SVS_RECORD_FILE_ALIGN_LEN + sizeof(SVS_Recv_Media_Header));

    //��ȡ����ý�建�������
    if (0 != sys_config.get(strServerName, "TmpfsRecvMediaBufferNum", strBuf))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Read the item [TmpfsRecvMediaBufferNum] from configuration file failed."
            "configuration file name [%s]",
            CONF_FILE));
        return false;
    }

    tmpfs_recv_media_buffer_num_ = ACE_OS::strtoul( strBuf, &endPtr, 10 );

    //��ȡ����ý�建������ʱ�ļ�·��
    memset( tmpfs_path_, 0, sizeof( tmpfs_path_ ) );
    if (0 != sys_config.get(strServerName, "TmpfsPath", tmpfs_path_))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Read the item [TmpfsPath] from configuration file failed."
            "configuration file name [%s]",
            CONF_FILE));

        // �����ʱ���������Ϊ0���ü����ʱ�ļ�ϵͳ·��
        if(tmpfs_recv_media_buffer_num_ > 0)
        {
            return false;
        }
    }

    //��ȡ����ý�建�������
    if (0 != sys_config.get(strServerName, "RecvMediaBufferNum", strBuf))
    {
        SVS_LOG((SVS_LM_ERROR,
            "Read the item [RecvMediaBufferNum] from configuration file failed."
            "configuration file name [%s]",
            CONF_FILE));
        return false;
    }

    recv_media_buffer_num_ = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    if( SVS_RECV_RT_MEDIA_DATA_BUFFER_MIN_NUM > recv_media_buffer_num_ )
    {
        SVS_LOG((SVS_LM_ERROR,
            "The value of the item [RecvMediaBufferNum] is too small."
            "It must be bigger than [%u], but actually [RecvMediaBufferNum] = %u",
            SVS_RECV_RT_MEDIA_DATA_BUFFER_MIN_NUM,
            recv_media_buffer_num_ ));
        return false;
    }

    uint32_t maxLens = (uint32_t)(max_lens_*SVS_MAX_LENS_NUM_2);
    if ( (recv_media_buffer_num_ + tmpfs_recv_media_buffer_num_) < maxLens )
    {
        SVS_LOG((SVS_LM_ERROR,
            "The value of the item [RecvMediaBufferNum] is too small."
            "It must be bigger than [%u] which is 2 * %u(maxLens),"
            " but actually [RecvMediaBufferNum] = %u",
            maxLens,
            max_lens_,
            recv_media_buffer_num_ ));
        return false;
    }

    if ( recv_media_buffer_num_ < max_lens_ )
    {
        SVS_LOG((SVS_LM_ERROR,
            "The value of the item [RecvMediaBufferNum] is too small."
            "It must be bigger than [%u] which is %u(maxLens),"
            " but actually [RecvMediaBufferNum] = %u",
            recv_media_buffer_num_,
            max_lens_,
            recv_media_buffer_num_ ));
        return false;
    }


    if ( SVS_RECV_RT_MEDIA_DATA_POOL_MAX_LEN < recv_media_buffer_num_*recv_media_buffer_size_ )
    {
         SVS_LOG((SVS_LM_ERROR,
            "The value of the item [RecvMediaBufferNum] is too big."
            "It must be smaller than [%u], but actually [RecvMediaBufferNum] = %u.",
            SVS_RECV_RT_MEDIA_DATA_POOL_MAX_LEN/recv_media_buffer_size_,
            recv_media_buffer_num_ ));
        return false;
    }

    return true;
}

/*****************************************************************************
 �� �� ��  : init_media_thread_configure
 ��������  : ��ʼ��¼��ɾ�����������
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Server::init_media_thread_configure()
{
    const char *strServerName = "SVS_RECORD";
    int32_t threadNum = 0;
    ConfAttribute szAttrbt;

    // ��ȡ����¼���߳��������
    szAttrbt.lMaxValue = RECV_MEDIA_DATA_MAX_THREAD_NUM;
    szAttrbt.lMinValue = 1;
    szAttrbt.lDefaultValue = RECV_MEDIA_DATA_DEFAULT_THREAD_NUM;
    // ����ȡʧ�ܣ���Ĭ��ֵ����ȡ�ɹ����ǳ�����Χ��ʧ�ܣ����Ը��߲���Ա��ȷ�Լ�������
    szAttrbt.enDefaultValueType = get_conf_type(USE_DEFAULT_VALUE_GET_FAIL, USE_NOTHING_OVER_RANGE);
    if (!get_conf_integer(strServerName, "InputThreadNum",
                            szAttrbt, threadNum))
    {
        return false;
    }
    recv_media_thread_num_ = (uint32_t)threadNum;

    // ��ȡÿ��raid���¼������߳�������
    szAttrbt.lMaxValue = WRITE_MEDIA_DATA_MAX_THREAD_NUM;
    szAttrbt.lMinValue = WRITE_MEDIA_DATA_MIN_THREAD_NUM;
    szAttrbt.lDefaultValue = WRITE_MEDIA_DATA_DEFAULT_THREAD_NUM;
    // ����ȡʧ�ܣ���Ĭ��ֵ����ȡ�ɹ����ǳ�����Χ��ʧ�ܣ����Ը��߲���Ա��ȷ�Լ�������
    szAttrbt.enDefaultValueType = get_conf_type(USE_DEFAULT_VALUE_GET_FAIL, USE_NOTHING_OVER_RANGE);
    if (!get_conf_integer(strServerName, "OutputThreadPerRaidGroup",
                            szAttrbt, threadNum))
    {
        return false;
    }
    output_thread_num_per_raid_ = (uint32_t)threadNum;

    // ��ȡ��֧�ֵĲ���raid����
    szAttrbt.lMaxValue = SVS_RAID_MAX_NUM;
    szAttrbt.lMinValue = SYNC_WORK_RAID_GROUP_MIN_NUM;
    szAttrbt.lDefaultValue = SYNC_WORK_RAID_GROUP_DEFAULT_NUM;
    // ����ȡʧ�ܣ���Ĭ��ֵ����ȡ�ɹ����ǳ�����Χ��ʧ�ܣ����Ը��߲���Ա��ȷ�Լ�������
    szAttrbt.enDefaultValueType = get_conf_type(USE_DEFAULT_VALUE_GET_FAIL, USE_NOTHING_OVER_RANGE);
    if (!get_conf_integer(strServerName, "SyncWorkRaidGroupNum",
                            szAttrbt, threadNum))
    {
        return false;
    }
    sync_work_raid_group_num_ = (uint32_t)threadNum;

    // ��ȡÿ��raid����֧�ֵ����ͷ��
    int32_t lensNum = 0;
    szAttrbt.lMaxValue = (int32_t)max_lens_;
    szAttrbt.lMinValue = 1;
    szAttrbt.lDefaultValue = DEFAULT_MAX_LENS_NUM;
    // ����ȡʧ�ܣ���Ĭ��ֵ����ȡ�ɹ����ǳ�����Χ��ʧ�ܣ����Ը��߲���Ա��ȷ�Լ�������
    szAttrbt.enDefaultValueType = get_conf_type(USE_NOTHING_GET_FAIL, USE_NOTHING_OVER_RANGE);
    if (!get_conf_integer(strServerName, "MaxLensPerRaid",
                            szAttrbt, lensNum))
    {
        return false;
    }

    max_lens_per_raid_ = (uint32_t)lensNum;

    if (max_lens_ > (sync_work_raid_group_num_ * max_lens_per_raid_))
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "The configuration is invalid.The value of [MaxLens] must is not larger than"
            " the value of [MaxLensPerRaid]*[SyncWorkRaidGroupNum]."
            "real value[MaxLens=%u],[MaxLensPerRaid=%u],[SyncWorkRaidGroupNum=%u].",
            max_lens_, max_lens_per_raid_, sync_work_raid_group_num_));

        return false;
    }

    return true;
}

/*****************************************************************************
 �� �� ��  : init_delete_record_configure
 ��������  : ��ʼ��¼��ɾ�����������
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Server::init_delete_record_configure()
{
    const char *strServerName = "SVS_RECORD";
    int32_t getConfRet = 0;
    ConfAttribute szAttrbt;

    // ��ȡ¼��ɾ��ģʽ
    szAttrbt.lMaxValue = OVERWRITE_RECORD;
    szAttrbt.lMinValue = DELETE_EXPIRE_RECORD;
    szAttrbt.lDefaultValue = DELETE_EXPIRE_RECORD;
    // ����ȡʧ�ܣ���Ĭ��ֵ����ȡ�ɹ����ǳ�����Χ��ʧ�ܣ����Ը��߲���Ա��ȷ�Լ�������
    szAttrbt.enDefaultValueType = get_conf_type(USE_DEFAULT_VALUE_GET_FAIL, USE_NOTHING_OVER_RANGE);
    if (!get_conf_integer(strServerName, "DeleteRecord",
                            szAttrbt, getConfRet))
    {
        return false;
    }
    delete_record_strategy_ = (uint8_t)getConfRet;

    return true;
}

/*****************************************************************************
 �� �� ��  : init_record_event_configure
 ��������  : ��ʼ���¼�¼�����������
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Server::init_record_event_configure()
{
    const char *strServerName = "SVS_RECORD";
    int32_t getConfRet = 0;
    ConfAttribute szAttrbt;

    // ��ȡ¼��ɾ��ģʽ
    szAttrbt.lMaxValue = SVS_EVENT_RECORD_LOST_MAX_TIMEOUT;
    szAttrbt.lMinValue = SVS_EVENT_RECORD_LOST_MIN_TIMEOUT;
    szAttrbt.lDefaultValue = SVS_EVENT_RECORD_LOST_DEFAULT_TIMEOUT;
    // ����ȡʧ�ܣ���Ĭ��ֵ����ȡ�ɹ����ǳ�����Χ��ʧ�ܣ����Ը��߲���Ա��ȷ�Լ�������
    szAttrbt.enDefaultValueType = get_conf_type(USE_DEFAULT_VALUE_GET_FAIL, USE_LIKE_VALUE_OVER_RANGE);
    if (!get_conf_integer(strServerName, "StopRecordEventLostTimeout",
                            szAttrbt, getConfRet))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Stop record event lost timeout invalid and use default value.",
            "real value[%d],default value[%d]",
            getConfRet, SVS_EVENT_RECORD_LOST_DEFAULT_TIMEOUT));

        getConfRet = SVS_EVENT_RECORD_LOST_DEFAULT_TIMEOUT;
    }
    event_record_lost_timeout_ = (uint32_t)getConfRet;

    //������ת��������
    event_record_lost_timeout_ *= 60;

    return true;
}

/*****************************************************************************
 �� �� ��  : get_conf_integer
 ��������  : ��ȡ�����������ݴ������������ƣ���ȡ�������ֵ����ȡ���óɹ�
             �󣬸�ݴ���
 �������  : NA
 �������  : NA
 �� �� ֵ  : �ɹ�����true ʧ�ܷ���false
 �޸���ʷ  :
*****************************************************************************/
bool CAC_RT_Record_Server::get_conf_integer(const char* pUpperConfName,
                                                const char* pSubConfName,
                                                const ConfAttribute &szConfAttribute,
                                                int32_t& nOutPutValue)const
{
    if((NULL == pUpperConfName) || (NULL == pSubConfName))
    {
        SVS_LOG((SVS_LM_CRITICAL,
            "get conf integer failed.conf name is NULL."
            "pUpperConfName[0x%08x], pSubConfName[%0x%08x]",
            pUpperConfName, pSubConfName));
        return false;
    }

    char strBuf[READ_CONFIG_STRING_BUF_LEN];
    char *endPtr = NULL;
    CAC_Config sys_config(CONF_FILE);
    int32_t tempDefaultType = 0;
    if (0 != sys_config.get(pUpperConfName, pSubConfName, strBuf))
    {
        // ��ȡ��ȡ����ʧ��ʱ������
        tempDefaultType = szConfAttribute.enDefaultValueType & 0xff00;
        if (USE_NOTHING_GET_FAIL == tempDefaultType)
        {
            SVS_LOG((SVS_LM_ERROR,
                "Get conf failed.configuration name[%s][%s],default value type[0x%04x].",
                pUpperConfName,pSubConfName,szConfAttribute.enDefaultValueType));

            return false;
        }
        else if(USE_DEFAULT_VALUE_GET_FAIL == tempDefaultType)
        {
            SVS_LOG((SVS_LM_WARNING, "Get conf failed,so use default value."
                "configuration name[%s][%s],default value[%d],default value type[0x%04x].",
                pUpperConfName,pSubConfName,
                szConfAttribute.lDefaultValue,szConfAttribute.enDefaultValueType));

            nOutPutValue = szConfAttribute.lDefaultValue;

            return true;
        }

        // ʹ��Ĭ��ֵ���ͷǷ�
        SVS_LOG((SVS_LM_ERROR,
            "Get conf failed and use defalt value type is invalid."
            "configuration name[%s][%s],default value[0x%04x].",
            pUpperConfName,pSubConfName,szConfAttribute.enDefaultValueType));

        return false;
    }

    // ��ȡ���������ļ��

    // �Ƿ��з�����ַ�
    if (0 != digit_check(strBuf))
    {
        SVS_LOG((SVS_LM_ERROR,
            "The value of the item [%s][%s] is invalid."
            "[%s] includes no-numerical character.",
            pUpperConfName,pSubConfName,strBuf));
        return false;
    }

    // ��ȡ��ȡ���ķ�Χ�Ƿ���ʱ�򣬴�������
    tempDefaultType = szConfAttribute.enDefaultValueType & 0x00ff;
    nOutPutValue = ACE_OS::strtol( strBuf, &endPtr, 10 );

    if ( nOutPutValue < szConfAttribute.lMinValue)
    {
        if(USE_NOTHING_OVER_RANGE == tempDefaultType)
        {
            SVS_LOG((SVS_LM_ERROR,
                "The value of configuration is too small."
                "configuration name[%s][%s], realVaule[%d], minVale[%d],"
                "default value type[0x%04x].",
                pUpperConfName,pSubConfName,nOutPutValue, szConfAttribute.lMinValue,
                szConfAttribute.enDefaultValueType));

            return false;
        }
        else if(USE_LIKE_VALUE_OVER_RANGE == tempDefaultType)
        {
            SVS_LOG((SVS_LM_WARNING,
                "The value of configuration is too small, use minimal value."
                "configuration name[%s][%s], realVaule[%d], minVale[%d],"
                "default value type[0x%04x].",
                pUpperConfName,pSubConfName,nOutPutValue, szConfAttribute.lMinValue,
                szConfAttribute.enDefaultValueType));

            nOutPutValue = szConfAttribute.lMinValue;

            return true;
        }
        else if(USE_DEFAULT_VALUE_OVER_RANGE == tempDefaultType)
        {
            SVS_LOG((SVS_LM_WARNING,
                "The value of configuration is too small, use default value."
                "configuration name[%s][%s], realVaule[%d], minvalue[%d],"
                "defalut[%d],default value type[0x%04x].",
                pUpperConfName,pSubConfName,nOutPutValue,szConfAttribute.lMinValue,
                szConfAttribute.lDefaultValue, szConfAttribute.enDefaultValueType));

            nOutPutValue = szConfAttribute.lDefaultValue;

            return true;
        }

        // ʹ��Ĭ��ֵ���ͷǷ�
        SVS_LOG((SVS_LM_ERROR,
            "The value of configuration is too small and use defalt value type is invalid."
            "configuration name[%s][%s], realVaule[%d], minvalue[%d],"
            ",default value type[0x%04x].",
            pUpperConfName,pSubConfName,nOutPutValue,szConfAttribute.lMinValue,
            szConfAttribute.enDefaultValueType));

        return false;
    }
    else if ( nOutPutValue > szConfAttribute.lMaxValue)
    {
        if(USE_NOTHING_OVER_RANGE == tempDefaultType)
        {
            SVS_LOG((SVS_LM_ERROR,
                "The value of configuration is too large."
                "configuration name[%s][%s], realVaule[%d], maxVale[%d],"
                "default value type[0x%04x].",
                pUpperConfName,pSubConfName,nOutPutValue, szConfAttribute.lMaxValue,
                szConfAttribute.enDefaultValueType));

            return false;
        }
        else if(USE_LIKE_VALUE_OVER_RANGE == tempDefaultType)
        {
            SVS_LOG((SVS_LM_WARNING,
                "The value of configuration is too large, use maximal value."
                "configuration name[%s][%s], realVaule[%d], maxVale[%d],"
                "default value type[0x%04x].",
                pUpperConfName,pSubConfName,nOutPutValue, szConfAttribute.lMaxValue,
                szConfAttribute.enDefaultValueType));

            nOutPutValue = szConfAttribute.lMaxValue;

            return true;
        }
        else if(USE_DEFAULT_VALUE_OVER_RANGE == tempDefaultType)
        {
            SVS_LOG((SVS_LM_WARNING,
                "The value of configuration is too small, use default value."
                "configuration name[%s][%s], realVaule[%d], maxvalue[%d],"
                "defalut[%d],default value type[0x%04x].",
                pUpperConfName,pSubConfName,nOutPutValue,szConfAttribute.lMaxValue,
                szConfAttribute.lDefaultValue, szConfAttribute.enDefaultValueType));

            nOutPutValue = szConfAttribute.lDefaultValue;

            return true;
        }

        // ʹ��Ĭ��ֵ���ͷǷ�
        SVS_LOG((SVS_LM_ERROR,
            "The value of configuration is too large and use defalt value type is invalid."
            "configuration name[%s][%s], realVaule[%d], maxvalue[%d],"
            ",default value type[0x%04x].",
            pUpperConfName,pSubConfName,nOutPutValue,szConfAttribute.lMaxValue,
            szConfAttribute.enDefaultValueType));

        return false;
    }

    // �������ֵ�Ϸ�
    return true;
}

int32_t CAC_RT_Record_Server::get_conf_type(int32_t failValueType, int32_t defaultValueType)const
{
    return (failValueType | defaultValueType);
}

// ���̹��?���ϱ��澯֪ͨ
int32_t CAC_RT_Record_Server::notify_exception(std::vector<string> &lens_vec, int32_t raid_id, int32_t error_code)
{
    size_t lens_num = lens_vec.size();
    ACE_Message_Block *pMsg = g_p_msg_buffer->alloc_mb(CSVS_Mb_Buffer_Manager::MB_TYPE_CALL_BACK, sizeof(SVS_EXCEPTION_ALARM));
    if (NULL == pMsg)
    {
        SVS_LOG((SVS_LM_WARNING, "notify exception raid_id[%d] code[%d], alloc message fail.",
                   raid_id, error_code));
        return SVS_RESULT_FAILURE;
    }
    SVS_EXCEPTION_ALARM *pAlarmMsg = (SVS_EXCEPTION_ALARM *)(void*)pMsg->wr_ptr();
    pAlarmMsg->ExceptEvent          = RT_MONITOR_EVENT_DISK_ALARM;
    pAlarmMsg->ExceptCode           = error_code;
    pAlarmMsg->RaidID               = raid_id;
    pAlarmMsg->DevIDVec             = NULL;
    pAlarmMsg->DevIDVec             = SVS_NEW(pAlarmMsg->DevIDVec);
    if (NULL == pAlarmMsg->DevIDVec)
    {
        SVS_LOG((SVS_LM_WARNING, "notify exception raid_id[%d] code[%d], alloc lens vector fail.",
                   raid_id, error_code));
        g_p_msg_buffer->free_mb(pMsg);
        return SVS_RESULT_FAILURE;
    }

    for (size_t i = 0; i < lens_num; i++)
    {
        pAlarmMsg->DevIDVec->push_back(lens_vec[i]);
    }
    pMsg->wr_ptr(sizeof(SVS_EXCEPTION_ALARM));

    ACE_Time_Value tv(ACE_OS::gettimeofday());
    int32_t nRet = signal_task_.enqueue_monitor_event(pMsg, &tv);
    if (-1 == nRet)
    {
        SVS_LOG((SVS_LM_WARNING, "notify exception raid_id[%d] code[%d], enqueue fail.",
                   raid_id, error_code));
        SVS_DELETE(pAlarmMsg->DevIDVec);
        g_p_msg_buffer->free_mb(pMsg);
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_INFO, "notify exception raid_id[%d] code[%d] success.",
               raid_id, error_code));
    return SVS_RESULT_OK;
}

// ��Ӹ����ϱ�����
void CAC_RT_Record_Server::add_load_report_task()
{
    uint32_t msgLen = sizeof(SVS_LOAD_REPORT_TASK);
    ACE_Message_Block* mb = g_p_msg_buffer->alloc_mb( CSVS_Mb_Buffer_Manager::MB_TYPE_EVENT_MSG,
                                                       msgLen );

    if ( NULL == mb )
    {
        SVS_LOG((SVS_LM_ERROR,
            "Create load report task failed.Fail to allocate mb. type[%d], size[%d].",
            CSVS_Mb_Buffer_Manager::MB_TYPE_EVENT_MSG,
            msgLen ));

        return ;
    }

    // ������Ϣ,ֻ��Ҫ������Ϣ���ͼ��ɣ�����Ϣ���ͱ������д���
    SVS_LOAD_REPORT_TASK* pMsg = (SVS_LOAD_REPORT_TASK*)(void*)mb->wr_ptr();
    pMsg->LoadReportType = 0;

    // ��Ϣ�����
    // ��������Ϣ�����ͷ
    ACE_Time_Value tv(ACE_OS::gettimeofday());
    tv.sec(tv.sec() + 10);

    if(SVS_RESULT_FAILURE == signal_task_.enqueue_load_report( mb,&tv))
    {
        SVS_LOG((SVS_LM_ERROR, "Load report task message enqueue failed."));
        g_p_msg_buffer->free_mb(mb);
        return ;
    }

    SVS_LOG((SVS_LM_INFO, "Add load report task successfully."));

    return ;
}

int32_t CAC_RT_Record_Server::set_load_report_interval_timer()
{
    if(-1 < load_report_interval_timer_)
    {
        (void)CAC_Timer_Manager::instance()->free_timer(load_report_interval_timer_);
    }

    // ˢ�������ļ�����
    ACE_Time_Value delayTime( (int32_t)load_report_interval_);
    ACE_Time_Value intervalTime( (int32_t)load_report_interval_ );
    load_report_interval_timer_ = CAC_Timer_Manager::instance()->alloc_timer(
            this,
            CAC_RT_Record_Server::timer_callback,
            REFRESH_LOAD_REPORT_INTERVAL_EVENT_TYPE,
            delayTime,
            intervalTime );

    if( SVS_RESULT_FAILURE == load_report_interval_timer_ )
    {
        SVS_LOG( (SVS_LM_CRITICAL, "Set load report interval timer failed.") );
        return SVS_RESULT_FAILURE;
    }

    SVS_LOG((SVS_LM_DEBUG,
        "Set load report interval timer successfully.interval[%u] timer[%d]",
        load_report_interval_,
        load_report_interval_timer_));

    return SVS_RESULT_OK;
}

void CAC_RT_Record_Server::get_register_info(SVS_RECORDSVR_INTERNAL_REGISTER_REQ_MSG & szRegMsg)
{
    SVS_TRACE();

    SetNLSDeviceId((uint8_t *)szRegMsg.ServerID, sizeof(szRegMsg.ServerID),str_rt_server_id_);

    szRegMsg.ServerType = server_type_;

    szRegMsg.Dummy[0] = is_default();

    (void)memcpy(szRegMsg.ServerName, str_name_, sizeof(szRegMsg.ServerName));

    (void)memcpy(szRegMsg.IPAddr, str_local_ip_, sizeof(szRegMsg.IPAddr));

    (void)memcpy(szRegMsg.MacAddr, str_mac_addr_, sizeof(szRegMsg.MacAddr));

    (void)memcpy(szRegMsg.Gateway, str_gateway_, sizeof(szRegMsg.Gateway));

    (void)memcpy(szRegMsg.NetMask, str_netmask_, sizeof(szRegMsg.NetMask));

    // ��ӷ�����ע��ʱ��POP������
    (void)memcpy(szRegMsg.szNodeID, str_node_id_, sizeof(szRegMsg.szNodeID));

    (void)memcpy(szRegMsg.szCMSIndex, str_cms_index_, sizeof(szRegMsg.szCMSIndex));

    // �ڼ��������ϵĻطŷ�����ʱ,ָ��Ϊ�ϻطŷ�����ID,����Ϊ�»طŷ�����ID���߲�ָ��
    SetNLSDeviceId((uint8_t *)szRegMsg.szPartnerId, sizeof(szRegMsg.szPartnerId),str_play_server_id_);

    return ;
}
//============================CAC_RT_Record_Server END==================

