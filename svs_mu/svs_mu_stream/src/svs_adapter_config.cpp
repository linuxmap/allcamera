// MduConfig.cpp: implementation of the CMduConfig class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "svs_adapter_svs_retcode.h"

#include "svs_adapter_config.h"
#include "svs_adapter_service_task.h"
#include "svs_daemon_thread.h"
#include "svs_adapter_svs_def.h"
#include <sys/time.h>

CMduConfig::CMduConfig()
{
    (void)ACE_OS::memset(m_szMduID, 0x0, sizeof(m_szMduID));
    (void)ACE_OS::memset(m_szMduName, 0x0, sizeof(m_szMduName));
    m_unAccountPeriod   = MDU_ACCOUNT_INTERVAL_DEFAULT;
    m_unReportPeriod    = MDU_REPORT_INTERVAL_DEFAULT;
    m_unServiceNetType  = MDU_SERVICE_NET_TYPE_DEFAULT;
    m_unServiceCapacity = MDU_SERVICE_CAPACITY_DEFAULT;

    m_unServiceIp = 0;
    m_unServicePortListNum = 0;
    memset(m_usServicePortList, 0x0, sizeof(m_usServicePortList));

    m_usDebugPort = 0;
    m_unSccIp     = 0;
    m_usSccPort   = 0;
    m_unSccHeartbeat = DEFAULT_HEARTBEAT_INTERVAL;

    m_unInternalMediaIp = 0;
    m_ExternalMediaIpList.clear();

    m_unInternalReportIp = 0;
    m_ExternalReportIpList.clear();

    m_usRecordMediaPort   = 0;
    m_usRtspServerPort    = 0;
    m_usRtmpServerPort    = 0;
    m_usHlsServerPort     = 0;
    m_usTcpMediaPort      = 0;
    memset(&m_stUdpMediaPort, 0x0, sizeof(m_stUdpMediaPort));
    m_usEnableEhome       = 0;
    m_usEhomeTransType    = 0;
    memset(&m_stEhomeMediaPort, 0x0, sizeof(m_stEhomeMediaPort));



    m_unCheckThreadInterval = DEFAULT_INTERVAL;
    m_unRestartServerFlag   = 0;
    m_unDumpServerFlag      = 0;

    m_LastModifyTime  = 0;
    memset(m_FileName, 0x0, MAX_CONFIG_FILE_NAME_LEN);
    m_strDecodeTag = "";
    m_ulUrlEffectiveWhile = 0;

    memset(m_strRegisterPasswd, 0x0, sizeof(m_strRegisterPasswd));

    m_DistributeLimitPerChannel = MDU_SERVICE_CAPACITY_DEFAULT;

}

CMduConfig::~CMduConfig()
{

}

const char* CMduConfig::getDecodeTag() const
{
    return m_strDecodeTag.c_str();
}


int32_t CMduConfig::init(const char* pCfgFile)
{
    if (NULL == pCfgFile)
    {
        SVS_LOG((SVS_LM_CRITICAL,"Read mdu config fail. config file is null."));
        return RET_FAIL;
    }
    CSVS_Config sysConfig(pCfgFile);

    strncpy(m_FileName, pCfgFile, MAX_CONFIG_FILE_NAME_LEN - 1);

    struct stat fileInfo;
    if (0 == stat(m_FileName, &fileInfo))
    {
        m_LastModifyTime = fileInfo.st_mtime;
    }

    if (RET_OK != readMduConfigure(sysConfig))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu configure fail."));
        return RET_FAIL;
    }

    if (RET_OK != readMediaConfigure(sysConfig))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get media configure fail."));
        return RET_FAIL;
    }

    if (RET_OK != readSccConfigure())
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get scc configure fail."));
        return RET_FAIL;
    }

    if (RET_OK != readDebugConfigure(sysConfig))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get debug configure fail."));
        return RET_FAIL;
    }


    return RET_OK;
}

int32_t CMduConfig::set(const char* sectionName, const char* itemName, const char* keyValue)const
{
    CSVS_Config sysConfig(m_FileName);
    int32_t iRet;
    iRet = sysConfig.set(sectionName, itemName, keyValue);
    return iRet;
}

int32_t CMduConfig::handle_timeout(const ACE_Time_Value &tv, const void *arg)
{
    struct stat fileInfo;
    if (0 == stat(m_FileName, &fileInfo))
    {
        if (m_LastModifyTime == fileInfo.st_mtime)
        {
            return 0;
        }
        m_LastModifyTime = fileInfo.st_mtime;
    }


    (void)CSVS_Daemon_Thread::instance()->reConfig(m_unCheckThreadInterval,
                                                   m_unRestartServerFlag,
                                                   m_unDumpServerFlag);
    SVS_LOG((SVS_LM_INFO,"mdu config reload success."));
    return 0;
}

// ���������ļ�ˢ�¶�ʱ�����������ڷ���һ��ʼ�ͳ�ʼ����
// ��ʱ�����̻߳�û�����������Զ�ʱ�������ڷ���������󵥶�����
int32_t CMduConfig::startReloadTimer()
{
    ACE_Reactor *pReactor = CMduServiceTask::instance()->getTimerReactor();
    if (NULL == pReactor)
    {
        return RET_FAIL;
    }

    ACE_Time_Value tv(MDU_CONFIG_RELOAD_INTERVAL, 0);
    int32_t TimerId = pReactor->schedule_timer(this, this, tv, tv);
    if (-1 == TimerId)
    {
        SVS_LOG((SVS_LM_WARNING,"mdu config start reload timer fail."));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"mdu config start reload timer[%d] success.", TimerId));
    return RET_OK;
}

// ֹͣ�����ļ�ˢ�¶�ʱ��
void CMduConfig::stopReloadTimer()
{
    ACE_Reactor *pReactor = CMduServiceTask::instance()->getTimerReactor();
    if (NULL == pReactor)
    {
        return;
    }

    (void) pReactor->cancel_timer(this);
    SVS_LOG((SVS_LM_INFO,"mdu config stop reload timer success."));

    return;
}

/*****************************************************************************
 �� �� ��  : readMduConfigure
 ��������  : ��ȡMDU������
 �������  : CSVS_Config &sysConfig
 �������  : ��
 �� �� ֵ  : ȫ����ȡ�ɹ�,���� RET_OK
             ������һ���������ȡʧ���򷵻� RET_FAIL
 �޸���ʷ      :
 1. ��    ��   :
    ��    ��   :
    �޸�����   :
*****************************************************************************/
int32_t CMduConfig::readMduConfigure(CSVS_Config &sysConfig)
{
    if (RET_OK != readItemServiceId(sysConfig))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu id fail."));
        return RET_FAIL;
    }

    if (RET_OK != readItemServiceName(sysConfig))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu name fail."));
        return RET_FAIL;
    }


    if (RET_OK != readItemServiceNetType(sysConfig))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu service net type fail."));
        return RET_FAIL;
    }

    if (RET_OK != readIntValue("SVS_STREAM",
                               "ServiceCapacity",
                               m_unServiceCapacity,
                               MDU_SERVICE_CAPACITY_MAX,
                               1,
                               MDU_SERVICE_CAPACITY_DEFAULT))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu service capacity fail."));
        return RET_FAIL;
    }

    if (RET_OK != readIpValue("SVS_STREAM", "ServiceIp", m_unServiceIp))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu service ip fail."));
        return RET_FAIL;
    }

    if (RET_OK != readItemServicePortList(sysConfig))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu service port list fail."));
        return RET_FAIL;
    }

    if (RET_OK != readIntValue("SVS_STREAM",
                               "AccountPeriod",
                               m_unAccountPeriod,
                               MDU_ACCOUNT_INTERVAL_MAX,
                               MDU_ACCOUNT_INTERVAL_MIN,
                               MDU_ACCOUNT_INTERVAL_DEFAULT))
    {
        SVS_LOG((SVS_LM_INFO,"Get mdu account period fail."));
        return RET_FAIL;
    }

    if (RET_OK != readIntValue("SVS_STREAM",
                               "ReportPeriod",
                               m_unReportPeriod,
                               MDU_REPORT_INTERVAL_MAX,
                               MDU_REPORT_INTERVAL_MIN,
                               MDU_REPORT_INTERVAL_DEFAULT))
    {
        SVS_LOG((SVS_LM_INFO,"Get mdu report period fail."));
        return RET_FAIL;
    }

    // decode tag
    const char *szSectionMdu = "SVS_STREAM";
    const char *szKeyServicePortList = "DecodeTag";
    char buf[MDU_MAX_BUF_LEN] = {0};
    if (RET_OK != sysConfig.get(szSectionMdu, szKeyServicePortList, buf))
    {
        SVS_LOG((SVS_LM_WARNING,"Haven't found [%s:%s] in the config file.",
                szSectionMdu,
                szKeyServicePortList));
    }

    m_strDecodeTag = buf;
    SVS_LOG((SVS_LM_INFO,"success to load DecodeTag[%s].", m_strDecodeTag.c_str()));

    if (0 != sysConfig.get("SVS_SCHEDULE", "RegisterPasswd", m_strRegisterPasswd, sizeof(m_strRegisterPasswd) - 1))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Fail to init mdu server, get config item [SVS_SCHEDULE][RegisterPasswd] failed, "
            "will use empty register passwd"));

        return RET_FAIL;
    }
    m_strRegisterPasswd[sizeof(m_strRegisterPasswd) - 1] = '\0';


    SVS_LOG((SVS_LM_INFO,"success to load RegisterPasswd[%s].", m_strRegisterPasswd));

    if (RET_FAIL == readIntValue("SVS_STREAM",
                               "UrlEffectiveWhile",
                               m_ulUrlEffectiveWhile,
                               MDU_RTSP_ANTIHOTLINKING_TIME_LEN_MAX,
                               0,
                               MDU_RTSP_ANTIHOTLINKING_TIME_LEN_DEFAULT))
    {
        SVS_LOG((SVS_LM_INFO,"Get RtspUrlEffectiveWhile fail."));
        return RET_FAIL;
    }


    SVS_LOG((SVS_LM_INFO,"success to load RtspUrlEffectiveWhile[%u].", m_ulUrlEffectiveWhile));


    SVS_LOG((SVS_LM_INFO,"success to load MDU configure."));
    return RET_OK;
}

/*****************************************************************************
 �� �� ��  : readMediauConfigure
 ��������  : ��ȡMedia������
 �������  : CSVS_Config &sysConfig
 �������  : ��
 �� �� ֵ  : RET_OK - �ɹ��� Others - ʧ��
 �޸���ʷ      :
 1. ��    ��   :
    ��    ��   :
    �޸�����   :
*****************************************************************************/
int32_t CMduConfig::readMediaConfigure(CSVS_Config &sysConfig)
{
    const char* szSection = "MEDIA";

    if (RET_OK != readIpValue(szSection, "InternalMediaIp", m_unInternalMediaIp))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu internal media ip config fail."));
        return RET_FAIL;
    }

    (void)readIpList(szSection, "ExternalMediaIp", m_ExternalMediaIpList);

    (void)readIpValue(szSection, "InternalReportIp", m_unInternalReportIp);

    (void)readIpList(szSection, "ExternalReportIp", m_ExternalReportIpList);

    if (RET_OK != readItemMediaPortConfig(sysConfig))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get stream media port config  fail."));
        return RET_FAIL;
    }
    if (RET_OK != readItemEhomeMediaPortConfig(sysConfig))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get stream ehome media port config  fail."));
        return RET_FAIL;
    }

    (void)readIntValue(szSection, "DistributeLimitPerChannel",
                       m_DistributeLimitPerChannel,
                       MDU_SERVICE_CAPACITY_MAX, 1, MDU_SERVICE_CAPACITY_DEFAULT);

    return RET_OK;
}

void CMduConfig::checkMediaIp(const std::string &strIp, MDU_IP_LIST& ipList)const
{
    uint32_t unIp = ACE_OS::inet_addr(strIp.c_str());
    unIp = ACE_NTOHL(unIp);
    if (0 == unIp)
    {
        SVS_LOG((SVS_LM_WARNING,"parse Ip[%s] fail.", strIp.c_str()));
        return;
    }

    for (MDU_IP_LIST::iterator iter = ipList.begin(); iter != ipList.end(); iter++)
    {
        if (unIp == *iter)
        {
            return;
        }
    }

    ipList.push_back(unIp);
    SVS_LOG((SVS_LM_INFO,"add Ip [%s] to ip list.", strIp.c_str()));
}
/*****************************************************************************
 �� �� ��  : readSccConfigure
 ��������  : ��ȡ�͵�����ص�������
 �������  : CSVS_Config &sysConfig
 �������  : ��
 �� �� ֵ  : ȫ����ȡ�ɹ�,���� RET_OK
             ������һ���������ȡʧ���򷵻� RET_FAIL
 �޸���ʷ      :
 1. ��    ��   :
    ��    ��   :
    �޸�����   :
*****************************************************************************/
int32_t CMduConfig::readSccConfigure()
{
    if (RET_OK != readIpValue("SVS_SCHEDULE", "Ip", m_unSccIp))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get Scc ip fail."));
        return RET_FAIL;
    }

    uint32_t Port = 0;
    if (RET_OK != readIntValue("SVS_SCHEDULE",
                               "Port",
                               Port,
                               0xFFFF,
                               1,
                               DEFAULT_SCC_CONNECT_PORT))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get Scc port fail."));
        return RET_FAIL;
    }
    m_usSccPort = (uint16_t)Port;

    uint32_t unSccHeartbeat = 0;
    if (RET_OK != readIntValue("SVS_SCHEDULE", "HeartbeatInterval",
                                unSccHeartbeat, MAX_HEARTBEAT_INTERVAL,
                                MIN_HEARTBEAT_INTERVAL,
                                DEFAULT_HEARTBEAT_INTERVAL))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get Scc heartbeat interval fail."));
        return RET_FAIL;
    }
    m_unSccHeartbeat = unSccHeartbeat;

    SVS_LOG((SVS_LM_INFO,"success to load SCC configure."));
    return RET_OK;
}


int32_t CMduConfig::readDebugConfigure(CSVS_Config &sysConfig)
{
    uint32_t Port;
    if (RET_OK != readIntValue("SVS_STREAM",
                               "DebugPort",
                               Port,
                               0xFFFF,
                               1,
                               DEFAULT_MDU_DEBUG_PORT))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get debug port fail."));
        return RET_FAIL;
    }
    m_usDebugPort = (uint16_t)Port;
    SVS_LOG((SVS_LM_INFO,"success to load debug configure."));
    return RET_OK;
}

int32_t CMduConfig::readItemServiceId(CSVS_Config &sysConfig)
{

    const char *szSectionMdu= "SVS_STREAM";

    const char *szKeyID = "ID";
    if (RET_OK != sysConfig.get(szSectionMdu, szKeyID, m_szMduID))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Haven't found [%s:%s] in the config file.",
            szSectionMdu, szKeyID));
        return RET_FAIL;
    }

    if(RET_OK != checkDeviceID(m_szMduID))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Invalid mdu id string, id[%s].", m_szMduID));

        return RET_FAIL;
    }
    SVS_LOG((SVS_LM_INFO,"suceess to read [%s:%s], value [%s].",
        szSectionMdu,
        szKeyID,
        m_szMduID));

    return RET_OK;
}

/*****************************************************************************
 �� �� ��  : readItemServiceName
 ��������  : ��ȡMDU������
 �������  : CSVS_Config &sysConfig
 �������  : ��
 �� �� ֵ  : �ɹ���ȡ,���� RET_OK ���򷵻� RET_FAIL
 �޸���ʷ      :
 1. ��    ��   :
    ��    ��   :
    �޸�����   :
*****************************************************************************/
int32_t CMduConfig::readItemServiceName(CSVS_Config &sysConfig)
{
    const char *szSectionMdu = "SVS_STREAM";

    // ��ȡMDU�����
    const char *szKeyName = "Name";
    if (RET_OK != sysConfig.get(szSectionMdu, szKeyName, m_szMduName))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Haven't found [%s:%s] in the config file.",
            szSectionMdu,
            szKeyName));
        return RET_FAIL;
    }
    // ����MDU����ַ���������һ���ֽ�Ϊ0,��ֹ��Ƴ���
    m_szMduName[sizeof(m_szMduName) - 1] = 0;

    SVS_LOG((SVS_LM_INFO,"suceess to read [%s:%s], value [%s].",
            szSectionMdu,
            szKeyName,
            m_szMduName));

    return RET_OK;
}


/*****************************************************************************
 �� �� ��  : readItemServiceNetType
 ��������  :
 �������  : CSVS_Config &sysConfig
 �������  : ��
 �� �� ֵ  : �ɹ���ȡ,���� RET_OK ���򷵻� RET_FAIL
 �޸���ʷ      :
 1. ��    ��   :
    ��    ��   :
    �޸�����   :
*****************************************************************************/
int32_t CMduConfig::readItemServiceNetType(CSVS_Config &sysConfig)
{
    const char *szSectionMdu = "SVS_STREAM";

    char buf[MDU_MAX_BUF_LEN] = {0,};

    const char *szKeyServiceNetType = "ServiceNetType";
    if (RET_OK != sysConfig.get(szSectionMdu, szKeyServiceNetType, buf))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Haven't found [%s:%s] in the config file.",
            szSectionMdu,
            szKeyServiceNetType));
        return RET_FAIL;
    }
    m_unServiceNetType= (uint16_t)ACE_OS::atoi(buf);

    if(m_unServiceNetType == 0)
    {
        m_unServiceNetType = MDU_SERVICE_NET_TYPE_DEFAULT;
        SVS_LOG((SVS_LM_CRITICAL,"Invalid service net type, set the default value [%d].",
            MDU_SERVICE_NET_TYPE_DEFAULT));

        return RET_OK;
    }

    SVS_LOG((SVS_LM_INFO,"suceess to read [%s:%s], value [%d].",
            szSectionMdu,
            szKeyServiceNetType,
            m_unServiceNetType));

    return RET_OK;
}

/*****************************************************************************
 �� �� ��  : readItemServicePortList
 ��������  :
 �������  : CSVS_Config &sysConfig
 �������  : ��
 �� �� ֵ  : �ɹ���ȡ,���� RET_OK ���򷵻� RET_FAIL
 �޸���ʷ      :
 1. ��    ��   :
    ��    ��   :
    �޸�����   :
*****************************************************************************/
int32_t CMduConfig::readItemServicePortList(CSVS_Config &sysConfig)
{
    const char *szSectionMdu = "SVS_STREAM";

    char buf[MDU_MAX_BUF_LEN] = {0,};

    const char *szKeyServicePortList = "ServicePortList";
    if (RET_OK != sysConfig.get(szSectionMdu, szKeyServicePortList, buf))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Haven't found [%s:%s] in the config file.",
            szSectionMdu,
            szKeyServicePortList));
        return RET_FAIL;
    }

    std::string strItemPortList= buf;
    std::string strSub;
    std::string::size_type idx;

    for (int32_t i = 0; i < MAX_SERVICE_PORT_NUM; i++)
    {
        idx = strItemPortList.find("|");
        if (std::string::npos != idx)
        {
            strSub = strItemPortList.substr(0,idx);
            strItemPortList = strItemPortList.substr(idx+1);
            m_usServicePortList[i] =  (uint16_t)atoi(strSub.c_str());
            if(m_usServicePortList[i] == 0)
            {
                SVS_LOG((SVS_LM_CRITICAL,"failed to read [%s:%s], Invalid value [%s].",
                    szSectionMdu,
                    szKeyServicePortList,
                    strSub.c_str()));
                return RET_FAIL;
            }
            m_unServicePortListNum++;
            SVS_LOG((SVS_LM_INFO,"suceess to read [%s:%s:%d], value [%s].",
                szSectionMdu,
                szKeyServicePortList,
                m_unServicePortListNum,
                strSub.c_str()));
        }
        else
        {
            m_usServicePortList[i] = (uint16_t)atoi(strItemPortList.c_str());
            if(m_usServicePortList[i] == 0)
            {
                SVS_LOG((SVS_LM_CRITICAL,"failed to read [%s:%s], Invalid value [%s].",
                    szSectionMdu,
                    szKeyServicePortList,
                    strItemPortList.c_str()));
                return RET_FAIL;
            }
            m_unServicePortListNum++;
            SVS_LOG((SVS_LM_INFO,"suceess to read [%s:%s:%d], value [%s].",
                szSectionMdu,
                szKeyServicePortList,
                m_unServicePortListNum,
                strItemPortList.c_str()));
            break;
        }
    }
    if(m_unServicePortListNum == 0)
    {
            SVS_LOG((SVS_LM_CRITICAL,"failed to read [%s:%s], haven't found validata port.",
                szSectionMdu,
                szKeyServicePortList));
            return RET_FAIL;
    }

    return RET_OK;


}

int32_t CMduConfig::readItemMediaPortConfig(CSVS_Config &sysConfig)
{
    const char *szSectionMdu  = "MEDIA";
    const char *szKeyTcpPort = "TcpMediaPort";
    const char *szKeyUdpPortList = "UdpMediaPort";

    char buf[MDU_MAX_BUF_LEN] ={ 0, };

    uint32_t unPort;
    if (RET_OK != readIntValue(szSectionMdu,
                               szKeyTcpPort,
                               unPort,
                               0xFFFF,
                               1,
                               DEFAULT_TCP_RECV_MEDIA_PORT))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu tcp media port config fail."));
        return RET_FAIL;
    }
    m_usTcpMediaPort = (uint16_t)unPort;

    memset(buf, 0x0, MDU_MAX_BUF_LEN);
    if (RET_OK != sysConfig.get(szSectionMdu, szKeyUdpPortList, buf))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Haven't found [%s:%s] in the config file.",
                szSectionMdu,
                szKeyUdpPortList));
        return RET_FAIL;
    }

    if (RET_OK != parsePortList(buf, m_stUdpMediaPort))
    {
        SVS_LOG((SVS_LM_CRITICAL,"parse [%s:%s] fail, config value[%s].",
                                szSectionMdu,
                                szKeyUdpPortList,
                                buf));
        return RET_FAIL;
    }


    if (RET_OK != readIntValue(szSectionMdu,
                               "RtspServerPort",
                               unPort,
                               0xFFFF,
                               1,
                               DEFAULT_RTSP_SERVER_PORT))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu rtsp server port config  fail."));
        return RET_FAIL;
    }
    m_usRtspServerPort = (uint16_t) unPort;

    if (RET_OK != readIntValue(szSectionMdu,
                               "HlsServerPort",
                               unPort,
                               0xFFFF,
                               0,
                               DEFAULT_HLS_SERVER_PORT))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu hls server port config  fail."));
        return RET_FAIL;
    }
    m_usHlsServerPort = (uint16_t) unPort;

    if (RET_OK != readIntValue(szSectionMdu,
                               "RtmpServerPort",
                               unPort,
                               0xFFFF,
                               0,
                               DEFAULT_HLS_SERVER_PORT))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu rtmp server port config  fail."));
        return RET_FAIL;
    }
    m_usRtmpServerPort = (uint16_t) unPort;


    if (RET_OK != readIntValue(szSectionMdu,
                               "RecordMediaPort",
                               unPort,
                               0xFFFF,
                               1,
                               DEFAULT_RECORD_MEDIA_PORT))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Get mdu record media port config  fail."));
        return RET_FAIL;
    }
    m_usRecordMediaPort = (uint16_t) unPort;

    return RET_OK;
}

int32_t CMduConfig::readItemEhomeMediaPortConfig(CSVS_Config &sysConfig)
{
    const char *szSectionMdu  = "MEDIA";
    const char *szEnableEhome = "EhomePortSwitch";
    const char *szEhomeTrans  = "EhomeTransType";
    const char *szEhomePortList = "EhomeMediaPort";

    char buf[MDU_MAX_BUF_LEN] ={ 0, };

    uint32_t unEnable;
    if (RET_OK != readIntValue(szSectionMdu,
                               szEnableEhome,
                               unEnable,
                               1,
                               0,
                               0))
    {
        SVS_LOG((SVS_LM_WARNING,"Get mdu ehome enable media flag config fail."));
        m_usEnableEhome = 0;
        return RET_OK;
    }
    m_usEnableEhome = (uint16_t)unEnable;

    uint32_t unEhomeTransType;

    if (RET_OK != readIntValue(szSectionMdu,
                               szEhomeTrans,
                               unEhomeTransType,
                               1,
                               0,
                               0))
    {
        SVS_LOG((SVS_LM_WARNING,"Get mdu ehome transtype media flag config fail."));
        m_usEhomeTransType = 0;
        return RET_OK;
    }
    m_usEhomeTransType = unEhomeTransType;

    memset(buf, 0x0, MDU_MAX_BUF_LEN);
    if (RET_OK != sysConfig.get(szSectionMdu, szEhomePortList, buf))
    {
        SVS_LOG((SVS_LM_WARNING,"Haven't found [%s:%s] in the config file.",
                szSectionMdu,
                szEhomePortList));
        m_usEnableEhome = 0;
        return RET_OK;
    }

    if (RET_OK != parsePortList(buf, m_stEhomeMediaPort))
    {
        SVS_LOG((SVS_LM_CRITICAL,"parse [%s:%s] fail, config value[%s].",
                                szSectionMdu,
                                szEhomePortList,
                                buf));
        return RET_FAIL;
    }

    return RET_OK;
}


int32_t CMduConfig::readIpValue
(
    const char*   section,
    const char*   key,
    uint32_t& value
)const
{
    if (NULL == section)
    {
        return RET_FAIL;
    }

    if (NULL == key)
    {
        return RET_FAIL;
    }

    CSVS_Config sysConfig(m_FileName);
    char szIp[MDU_IP_ADDR_LEN] = {0};
    if (RET_OK != sysConfig.get(section, key, szIp))
    {
        SVS_LOG((SVS_LM_CRITICAL,"Haven't found [%s:%s] in the config file.",
            section, key));
        return RET_FAIL;
    }

    value = ACE_OS::inet_addr(szIp);
    value = ACE_NTOHL(value);
    if(value == 0)
    {
        SVS_LOG((SVS_LM_CRITICAL,"Invalidate string [%s:%s] in config file.",
            section, key));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"suceess to read [%s:%s], value [%s].",
            section, key, szIp));

    return RET_OK;

}

int32_t CMduConfig::readIpList
(
    const char*   section,
    const char*   key,
    MDU_IP_LIST&  ipList
)const
{
    if (NULL == section)
    {
        return RET_FAIL;
    }

    if (NULL == key)
    {
        return RET_FAIL;
    }

    CSVS_Config sysConfig(m_FileName);
    char  szMediaIp[MDU_MAX_BUF_LEN] = { 0 };
    if (RET_OK == sysConfig.get(section, key, szMediaIp))
    {
        std::string strIpConfig = szMediaIp;
        std::string strIp;
        std::string::size_type idx;
        for (int32_t i = 0; i < MAX_MEDIA_IP_NUM; i++)
        {
            idx = strIpConfig.find("|");
            if (std::string::npos != idx)
            {
                strIp = strIpConfig.substr(0, idx);
                strIpConfig = strIpConfig.substr(idx + 1);

                checkMediaIp(strIp, ipList);
            }
            else
            {
                checkMediaIp(strIpConfig, ipList);
                break;
            }
        }

        if (ipList.empty())
        {
            SVS_LOG((SVS_LM_CRITICAL,"Get mdu [%s:%s] config fail, no invalid ip.", section, key));
            return RET_FAIL;
        }

        SVS_LOG((SVS_LM_INFO,"suceess to read [%s:%s], ip count[%d].", section, key, ipList.size()));
    }
    else
    {
        SVS_LOG((SVS_LM_WARNING,"no config [%s:%s].", section, key));
    }

    return RET_OK;
}

int32_t CMduConfig::readIntValue
(
    const char*   section,
    const char*   key,
    uint32_t& value,
    uint32_t  maxValue,
    uint32_t  minValue,
    uint32_t  defaultValue
)const
{
    if (NULL == section)
    {
        return RET_FAIL;
    }

    if (NULL == key)
    {
        return RET_FAIL;
    }

    CSVS_Config sysConfig(m_FileName);

    value = defaultValue;

    char buf[MDU_MAX_BUF_LEN] = {0};
    if (RET_OK != sysConfig.get(section, key, buf))
    {
        SVS_LOG((SVS_LM_WARNING,"Haven't found [%s:%s] in the config file set default value [%d].",
            section, key, defaultValue));
        return RET_ERR_CONFIG_ITEM_NOT_FOUND;
    }

    value = strtoul(buf, NULL, 0);
    if (value < minValue)
    {
        value = minValue;
    }
    else if (value > maxValue)
    {
        value = maxValue;
    }

    SVS_LOG((SVS_LM_INFO,"suceess to read [%s:%s], value [%d].",
        section, key, value));

    return RET_OK;

}


int32_t CMduConfig::checkPortRange(uint16_t usPortRangeNum, const PortRange *pstPortRange) const
{
    int32_t i = 0;
    int32_t j = 0;

    if (0 == usPortRangeNum)
    {
        SVS_LOG((SVS_LM_ERROR,"Port range num is 0."));
        return RET_FAIL;
    }

    SVS_LOG((SVS_LM_INFO,"Port range [%d].", usPortRangeNum));
    for (i = 0; i < usPortRangeNum; i++)
    {
        SVS_LOG((SVS_LM_INFO,"Check port range [%d] [%d-%d].",
            i,
            pstPortRange[i].usStartPort,
            pstPortRange[i].usEndPort));

        if (pstPortRange[i].usEndPort < pstPortRange[i].usStartPort ||
            pstPortRange[i].usEndPort ==0 || pstPortRange[i].usStartPort ==0)
        {
            SVS_LOG((SVS_LM_ERROR,
                "CMduConfig::checkPortRange fail, port range [%d-%d] is abnormal.",
                pstPortRange[i].usStartPort,
                pstPortRange[i].usEndPort));
            return RET_FAIL;
        }
    }

    for (i = 0; i < usPortRangeNum - 1; i++)
    {
        for (j = i + 1; j < usPortRangeNum; j++)
        {
            if (pstPortRange[i].usStartPort == pstPortRange[j].usStartPort)
            {
                SVS_LOG((SVS_LM_ERROR,
                    "Port range is abnormal, port range[%d-%d] and [%d-%d] overlap.",
                    pstPortRange[i].usStartPort,
                    pstPortRange[i].usEndPort,
                    pstPortRange[j].usStartPort,
                    pstPortRange[j].usEndPort));
                return RET_FAIL;
            }
            else if (pstPortRange[i].usStartPort < pstPortRange[j].usStartPort)
            {
                if (pstPortRange[i].usEndPort >= pstPortRange[j].usStartPort)
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Port range is abnormal, port range[%d-%d] and [%d-%d] overlap.",
                        pstPortRange[i].usStartPort,
                        pstPortRange[i].usEndPort,
                        pstPortRange[j].usStartPort,
                        pstPortRange[j].usEndPort));
                    return RET_FAIL;
                }
            }
            else
            {
                if (pstPortRange[i].usStartPort <= pstPortRange[j].usEndPort)
                {
                    SVS_LOG((SVS_LM_ERROR,
                        "Port range is abnormal, port range[%d-%d] and [%d-%d] overlap.",
                        pstPortRange[i].usStartPort,
                        pstPortRange[i].usEndPort,
                        pstPortRange[j].usStartPort,
                        pstPortRange[j].usEndPort));
                    return RET_FAIL;
                }
            }
        }
    }

    return RET_OK;
}

int32_t CMduConfig::checkDeviceID(const char *pszDevID) const
{
    if (NULL == pszDevID)
    {
        return RET_FAIL;
    }

    int32_t iDeviceLen = (int32_t)strlen(pszDevID);

    if (iDeviceLen <= 0 || iDeviceLen > DEVICE_ID_LEN)
    {
        return RET_FAIL;
    }

    for (int32_t i = 0; i < iDeviceLen; i++)
    {
        if((pszDevID[i] >= '0' && pszDevID[i] <= '9')
            || (pszDevID[i] >= 'a' && pszDevID[i] <= 'z')
            || (pszDevID[i] >= 'A' && pszDevID[i] <= 'Z'))
        {
            continue;
        }
        else
        {
            return RET_FAIL;
        }
    }

    return RET_OK;
}

int32_t CMduConfig::parsePortList(const char* pPortbuf, MediaPortConfig &config) const
{
    std::string strItemPortRange = pPortbuf;
    std::string strSub;
    std::string::size_type idx;

    for (int32_t i = 0; i < MAX_MEDIA_PORT_RANGE_NUM; i++)
    {
        idx = strItemPortRange.find("|");
        if (std::string::npos != idx)
        {
            strSub = strItemPortRange.substr(0, idx);
            strItemPortRange = strItemPortRange.substr(idx + 1);
            config.stPortRange[i].usStartPort = (uint16_t) atoi(strSub.c_str());
            idx = strSub.find(",");
            if (std::string::npos != idx)
            {
                strSub = strSub.substr(idx + 1);
                config.stPortRange[i].usEndPort = (uint16_t) atoi(strSub.c_str());
                config.usPortRangeNum++;
            }
            else
            {
                config.stPortRange[i].usEndPort
                        = config.stPortRange[i].usStartPort;
                config.usPortRangeNum++;
            }
        }
        else
        {
            config.stPortRange[i].usStartPort = (uint16_t) atoi( strItemPortRange.c_str());

            idx = strItemPortRange.find(",");
            if (std::string::npos != idx)
            {
                strSub = strItemPortRange.substr(idx + 1);
                config.stPortRange[i].usEndPort = (uint16_t) atoi(strSub.c_str());
                config.usPortRangeNum++;
            }
            else
            {
                config.stPortRange[i].usEndPort
                        = config.stPortRange[i].usStartPort;
                config.usPortRangeNum++;
            }
            break;
        }
    }

    if (RET_OK != checkPortRange(config.usPortRangeNum,
                                 config.stPortRange))
    {
        return RET_FAIL;
    }

    return RET_OK;
}

const char* CMduConfig::getServiceId() const
{
    return m_szMduID;
}


const char* CMduConfig::getServiceName() const
{
    return m_szMduName;
}


uint32_t CMduConfig::getServiceNetType() const
{
    return m_unServiceNetType;
}


uint32_t CMduConfig::getServiceCapacity() const
{
    return m_unServiceCapacity;
}


uint32_t CMduConfig::getServiceIp() const
{
    return m_unServiceIp;
}

void CMduConfig::getServicePortList(uint16_t *&pusServicePortList,
                                    uint32_t &unArrayItemNum)
{
    pusServicePortList = m_usServicePortList;
    unArrayItemNum     = m_unServicePortListNum;
}

uint32_t CMduConfig::getInternalMediaIp() const
{
    return m_unInternalMediaIp;
}

void CMduConfig::getExternalMediaIpList(MDU_IP_LIST &ipList) const
{
    ipList.clear();
    ipList =  m_ExternalMediaIpList;
    return;
}

uint32_t CMduConfig::getInternalReportIp() const
{
    return m_unInternalReportIp;
}

void CMduConfig::getExternalReportIpList(MDU_IP_LIST &ipList) const
{
    ipList.clear();
    ipList =  m_ExternalReportIpList;
    return;
}


void CMduConfig::getUdpMediaPortConfig(MediaPortConfig* &pUdpConfig)
{
    pUdpConfig = &m_stUdpMediaPort;
}
uint16_t CMduConfig::getEnableEhome() const
{
    return m_usEnableEhome;
}
uint16_t CMduConfig::getEhomeTransType() const
{
    return m_usEhomeTransType;
}

void CMduConfig::getEhomeMediaPortConfig(MediaPortConfig* &pEhomeConfig)
{
    pEhomeConfig = &m_stEhomeMediaPort;
}


uint16_t CMduConfig::getTcpMediaPort()const
{
    return m_usTcpMediaPort;
}

uint16_t CMduConfig::getRecordMediaPort() const
{
    return m_usRecordMediaPort;
}

uint16_t CMduConfig::getRtspServerPort() const
{
    return m_usRtspServerPort;
}

uint16_t CMduConfig::getRtmpServerPort() const
{
    return m_usRtmpServerPort;
}

uint16_t CMduConfig::getHLSServerPort() const
{
    return m_usHlsServerPort;
}


uint32_t CMduConfig::getDistributeLimitPerChannel()const
{
    return m_DistributeLimitPerChannel;
}


uint32_t CMduConfig::getAccountPeriod() const
{
    return m_unAccountPeriod;
}


uint32_t CMduConfig::getReportPeriod() const
{
    return m_unReportPeriod;
}


uint32_t CMduConfig::getSccIp() const
{
    return m_unSccIp;
}


uint16_t CMduConfig::getSccPort() const
{
    return m_usSccPort;
}

uint32_t  CMduConfig::getSccHeartbeatInterval() const
{
    return m_unSccHeartbeat;
}

uint16_t  CMduConfig::getDebugPort() const
{
    return m_usDebugPort;
}

uint32_t CMduConfig::getDebugCheckThreadInterval() const
{
    return m_unCheckThreadInterval;
}

uint32_t CMduConfig::getDebugRestartServerFlag() const
{
    return m_unRestartServerFlag;
}

uint32_t CMduConfig::getDebugDumpServerFlag() const
{
    return m_unDumpServerFlag;
}

uint32_t CMduConfig::getUrlEffectiveWhile()const
{
    return m_ulUrlEffectiveWhile;
}

const char* CMduConfig::GetRegisterPasswd()const
{
    return m_strRegisterPasswd;
}



