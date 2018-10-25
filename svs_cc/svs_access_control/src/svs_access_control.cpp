#include <string>
#include "stdafx.h"
#include "svs_log_msg.h"
#include "svs_global_msg_queue.h"
#include "svs_access_control.h"
#include "svs_daemon_thread.h"

using namespace std;


void freeMessageBlock(ACE_Message_Block* &mb)
{
    SVS_TRACE();

    if (NULL == mb)
    {
        SVS_LOG((SVS_LM_ERROR, "Invalid parameter. mb is NULL."));
        return;
    }

    (void)mb->release();
    mb = NULL;
}

ACE_Message_Block* allockMessageBlock(size_t length, ACE_Message_Block::ACE_Message_Type type)
{
    SVS_TRACE();

    ACE_Message_Block *mb = NULL;
    try
    {
        mb = new ACE_Message_Block(length, type);
        //if (NULL == mb)
        //{
        //    SVS_LOG((SVS_LM_ERROR, "Allocate AceMessageBlock error. length=%d, type=%d.",
        //        length, type));
        //    return NULL;
        //}

        if (ACE_Message_Block::MB_STOP == type)
        {
            mb->rd_ptr(mb->base());
            mb->wr_ptr(mb->base());
        }
        else if (NULL == mb->data_block() || NULL == mb->base())
        {
            SVS_LOG((
                SVS_LM_ERROR,
                "Allocate AceMessageBlock error. "
                "length=%d, type=%d, data=0x%08x, base=0x%08x.",
                length, type, mb->data_block(), mb->base()));
            freeMessageBlock(mb);
        }
        else
        {
            mb->rd_ptr(mb->base());
            mb->wr_ptr(mb->base());
            (void)ACE_OS::memset(mb->base(), 0, length);
        }
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Allocate AceMessageBlock error. length=%d, type=%d",
            length, type));
        freeMessageBlock(mb);
    }

    if (NULL != mb)
    {
        mb->cont(NULL);
    }

    return mb;
}


CAccessControlSvr::CAccessControlSvr()
{
    SVS_LOG((SVS_LM_DEBUG, "[OBJ_CREAT] CAccessControlSvr create. address=0x%08x.", this));

    (void)ACE_OS::memset(m_szSvrName, 0, sizeof(m_szSvrName));
    (void)ACE_OS::memset(m_szSvrID, 0, sizeof(m_szSvrID));
    (void)ACE_OS::memset(m_szSvrIP, 0, sizeof(m_szSvrIP));
    m_uiSvrIP = 0;

    (void)ACE_OS::memset(m_szHttpStackListenIP, 0, sizeof(m_szHttpStackListenIP));
    m_uiHttpStackListenIP = 0;
    m_uiHttpStackListenPort = 0;

    (void)ACE_OS::memset(m_szHttpStackFwIP, 0, sizeof(m_szHttpStackFwIP));
    m_uiHttpStackFwIP = 0;
    m_uiHttpStackFwPort = 0;

    (void)ACE_OS::memset(m_szControlServerIP, 0, sizeof(m_szControlServerIP));
    m_uiControlServerIP = 0;
    m_uiControlServerPort = 0;

    (void)ACE_OS::memset(m_szMuListenIP, 0, sizeof(m_szMuListenIP));
    m_uiMuListenIP = 0;
    m_uiMuListenPort = 0;

    m_uiDebugPort = 0;


    m_uiCheckThreadStatusInterval = DEFAULT_INTERVAL;
    m_uiRestartServerFlag = 0;
    m_uiDumpServerFlag = 0;

    //for device stack
    m_nQueryCatalogInterval     = DEVICE_STACK_QUERY_CATALOG_INTERVAL;
    m_nDevReportInterval        = DEVICE_REPORT_INTERVAL;
    m_nGB28181ListenPort        = SVS_DEFALT_GB28181_STACK_LISTEN_PORT;
    m_nEhomeListenPort          = SVS_DEFALT_EHOME_STACK_LISTEN_PORT;
    m_nDeviceKeepAliveTimeout   = DEVICE_KEEP_ALIVE_TIMEOUT;
}

CAccessControlSvr::~CAccessControlSvr()
{
    try
    {
        SVS_LOG((SVS_LM_DEBUG, "[OBJ_DESTORY] ~CAccessControlSvr destroy. address=0x%08x.", this));
    }
    catch (...)
    {
        printf("~CAccessControlSvr catch an exception.\n");
    }
}


bool CAccessControlSvr::init()
{
    SVS_TRACE();


    return true;
}

int32_t CAccessControlSvr::relase()
{
    SVS_TRACE();



    return 0;
}


bool CAccessControlSvr::init_configure()
{
    SVS_TRACE();

    CAC_Config sys_config(CONF_FILE);


    char str[READ_CONFIG_STRING_BUF_LEN] = {0};

    // read server name
    if (0 != sys_config.get("SVS_ACCESS_CONTROL", "Name", m_szSvrName))
    {
        memcpy(m_szSvrName,"AccessControl",sizeof("AccessControl"));
    }
    SVS_LOG((SVS_LM_INFO, "AccessControl name is %s.", m_szSvrName));

    // read server id, must be ok.
    if (sys_config.get("SVS_ACCESS_CONTROL", "ID", m_szSvrID) != 0)
    {
        SVS_LOG((SVS_LM_ERROR, "No AccessControl ID in %s.", CONF_FILE));
        return false;
    }
    SVS_LOG((SVS_LM_INFO, "Get AccessControl ID in %s. ID=%s.", CONF_FILE, m_szSvrID));

    if ( CHECK_ERROR == CheckDeviceID(m_szSvrID) )
    {
        SVS_LOG((SVS_LM_ERROR, "Schedule ID error. ID=%s.", m_szSvrID));
        return false;
    }

    // read server ip, must be ok.
    if (sys_config.get("SVS_ACCESS_CONTROL", "Ip", m_szSvrIP) != 0)
    {
        SVS_LOG((SVS_LM_ERROR, "No AccessControl Ip in %s.", CONF_FILE));
        return false;
    }
    SVS_LOG((SVS_LM_INFO, "Get AccessControl Ip in %s. ID=%s.", CONF_FILE, m_szSvrIP));

    m_uiSvrIP = ACE_OS::inet_addr(m_szSvrIP);
    if (INADDR_NONE == m_uiSvrIP)
    {
        SVS_LOG((SVS_LM_ERROR, "The AccessControl Ip error in %s. IP=%s.",
            CONF_FILE, m_szSvrIP));
        return false;
    }

    m_uiSvrIP = ACE_NTOHL(m_uiSvrIP);
    SVS_LOG((SVS_LM_INFO, "Get AccessControl Ip in %s. IP=%s[0x%08x].",
        CONF_FILE, m_szSvrIP, m_uiSvrIP));



    // read http stack ip , not must be ok
    if (sys_config.get("SVS_HTTP_STACK", "Ip", m_szHttpStackListenIP) == 0)
    {
        m_uiHttpStackListenIP = ACE_OS::inet_addr(m_szHttpStackListenIP);
        if (INADDR_NONE == m_uiHttpStackListenIP)
        {
            SVS_LOG((SVS_LM_WARNING, "The http stack listen IP error in %s. IP=%s.",
                             CONF_FILE, m_szHttpStackListenIP));
            (void)ACE_OS::memset(m_szHttpStackListenIP, 0, sizeof(m_szHttpStackListenIP));
            m_uiHttpStackListenIP = 0;
        }
        else
        {
            m_uiHttpStackListenIP = ACE_NTOHL(m_uiHttpStackListenIP);
            SVS_LOG((SVS_LM_INFO, "Get http stack listen IP in %s. IP=%s[0x%08x].",
                             CONF_FILE, m_szHttpStackListenIP, m_uiHttpStackListenIP));
        }

        m_uiHttpStackFwIP = m_uiHttpStackListenIP;
        memcpy(m_szHttpStackFwIP,m_szHttpStackListenIP,SVS_IP_LEN);

    }
    else
    {
        SVS_LOG((SVS_LM_WARNING, "No http stack listen IP in %s.", CONF_FILE));
        (void)ACE_OS::memset(m_szHttpStackListenIP, 0, sizeof(m_szHttpStackListenIP));
    }

    // read http stack port, must be ok
    m_uiHttpStackListenPort = SVS_DEFALT_HTTP_STACK_LISTEN_PORT;
    if (0 == sys_config.get("SVS_HTTP_STACK", "Port", str))
    {
        m_uiHttpStackListenPort = static_cast<uint16_t>(atoi(str));
        m_uiHttpStackFwPort = m_uiHttpStackListenPort;
    }
    SVS_LOG((SVS_LM_INFO, "Get http stack listen port in %s. port=%d.", CONF_FILE, m_uiHttpStackListenPort));

    // read http stack ip  of firewall
    if (sys_config.get("SVS_HTTP_STACK", "FWIp", m_szHttpStackFwIP) == 0)
    {
        m_uiHttpStackFwIP = ACE_OS::inet_addr(m_szHttpStackFwIP);
        if (INADDR_NONE == m_uiHttpStackFwIP)
        {
            SVS_LOG((SVS_LM_WARNING, "The http stack firewall IP error in %s. IP=%s.",
                             CONF_FILE, m_szHttpStackFwIP));
            (void)ACE_OS::memset(m_szHttpStackFwIP, 0, sizeof(m_szHttpStackFwIP));
            m_uiHttpStackFwIP = 0;
        }
        else
        {
            m_uiHttpStackFwIP = ACE_NTOHL(m_uiHttpStackFwIP);
            SVS_LOG((SVS_LM_INFO, "Get http stack firewall IP in %s. IP=%s[0x%08x].",
                             CONF_FILE, m_szHttpStackListenIP, m_uiHttpStackListenIP));
        }
    }

    // read http stack firewall port
    if (0 == sys_config.get("SVS_HTTP_STACK", "FWPort", str))
    {
        m_uiHttpStackFwPort = static_cast<uint16_t>(atoi(str));
    }
    SVS_LOG((SVS_LM_INFO, "Get http stack firewall port in %s. port=%d.", CONF_FILE, m_uiHttpStackFwPort));





    if (sys_config.get("SVS_HTTP_STACK", "ServerIp", m_szControlServerIP) != 0)
    {
        SVS_LOG((SVS_LM_ERROR, "No Server call  IP in %s.", CONF_FILE));
    }

    m_uiControlServerIP = ACE_OS::inet_addr(m_szControlServerIP);
    if (INADDR_NONE == m_uiControlServerIP)
    {
        SVS_LOG((SVS_LM_ERROR, "The control server IP error in %s. IP=%s.",
            CONF_FILE, m_szControlServerIP));
    }

    m_uiControlServerIP = ACE_NTOHL(m_uiControlServerIP);
    SVS_LOG((SVS_LM_INFO, "Get control server IP in %s. IP=%s[0x%08x].",
        CONF_FILE, m_szControlServerIP, m_uiControlServerIP));

    m_uiControlServerPort = SVS_DEFALT_CONTROL_SERVER_PORT;
    if (0 == sys_config.get("SVS_HTTP_STACK", "ServerPort", str))
    {
        m_uiControlServerPort = static_cast<uint16_t>(atoi(str));
    }
    SVS_LOG((SVS_LM_INFO, "Get http control port in %s. port=%d.", CONF_FILE, m_uiControlServerPort));

    // read mu ip , not must be ok
    if (sys_config.get("SVS_MU_ACCESS", "Ip", m_szMuListenIP) == 0)
    {
        m_uiMuListenIP = ACE_OS::inet_addr(m_szMuListenIP);
        if (INADDR_NONE == m_uiHttpStackListenIP)
        {
            SVS_LOG((SVS_LM_WARNING, "The mu listen IP error in %s. IP=%s.", CONF_FILE, m_szMuListenIP));

            (void)ACE_OS::memset(m_szMuListenIP, 0, sizeof(m_szMuListenIP));
            m_uiHttpStackListenIP = 0;
        }
        else
        {
            m_uiMuListenIP = ACE_NTOHL(m_uiMuListenIP);
            SVS_LOG((SVS_LM_INFO, "Get mu listen IP in %s. IP=%s[0x%08x].",
                             CONF_FILE, m_szMuListenIP, m_uiHttpStackListenIP));
        }

    }
    else
    {
        SVS_LOG((SVS_LM_WARNING, "No mu listen IP in %s.", CONF_FILE));
        (void)ACE_OS::memset(m_szMuListenIP, 0, sizeof(m_szMuListenIP));
    }

    // read mu listen port
    m_uiMuListenPort = SVS_DEFALT_MU_LISTEN_PORT;
    if (0 == sys_config.get("SVS_MU_ACCESS", "Port", str))
    {
        m_uiMuListenPort = static_cast<uint16_t>(atoi(str));
    }
    SVS_LOG((SVS_LM_INFO, "Get mu listen port in %s. port=%d.", CONF_FILE, m_uiMuListenPort));


    // read debug port
    m_uiDebugPort = SVS_DEFALT_REMOTE_DEBUG_PORT;
    if (0 == sys_config.get( "SVS_DEBUG", "DebugPort", str))
    {
        m_uiDebugPort = atoi(str);
    }
    SVS_LOG((SVS_LM_INFO, "DebugPort in %s is %d.", CONF_FILE, m_uiDebugPort));

    return init_configure_for_device_access(sys_config);
}

bool CAccessControlSvr::init_configure_for_device_access(CAC_Config& rConfig)
{
    char str[READ_CONFIG_STRING_BUF_LEN] = {0};

    if (0 == rConfig.get( "SVS_DEVICE", "DevReportInterval", str))
    {
        m_nDevReportInterval = atoi(str);
    }
    SVS_LOG((SVS_LM_INFO, "DevReportInterval in %s is %d.", CONF_FILE, m_nDevReportInterval));

    if (0 == rConfig.get( "SVS_DEVICE", "KeepAliveTimeout", str))
    {
        m_nDeviceKeepAliveTimeout = atoi(str);
    }
    SVS_LOG((SVS_LM_INFO, "KeepAliveTimeout of SVS_DEVICE_ACCESS in %s is %d.", CONF_FILE, m_nDeviceKeepAliveTimeout));

    /*GB28181 configure */

    if (0 == rConfig.get( "SVS_DEVICE_GB28181", "QueryCatalogInterval", str))
    {
        m_nQueryCatalogInterval = atoi(str);
    }
    SVS_LOG((SVS_LM_INFO, "QueryCatalogInterval in %s is %d.", CONF_FILE, m_nQueryCatalogInterval));

    if (0 == rConfig.get( "SVS_DEVICE_GB28181", "Port", str))
    {
        m_nGB28181ListenPort = atoi(str);
    }
    SVS_LOG((SVS_LM_INFO, "gb28181 Port of SVS_DEVICE_ACCESS in %s is %d.", CONF_FILE, m_nGB28181ListenPort));



    /*Ehome configure */

    if (0 == rConfig.get( "SVS_DEVICE_EHOME", "Port", str))
    {
        m_nEhomeListenPort = atoi(str);
    }
    SVS_LOG((SVS_LM_INFO, "ehome listen port in %s is %d.", CONF_FILE, m_nEhomeListenPort));

    return true;
}

int32_t CAccessControlSvr::read_dynamic_config_iterms()
{
    const char *strServerName = "SVS_ACCESS_CONTROL";
    char strBuf[READ_CONFIG_STRING_BUF_LEN] = {0};
    char *endPtr = NULL;
    CAC_Config sys_config(CONF_FILE);

    m_uiCheckThreadStatusInterval = DEFAULT_INTERVAL;
    m_uiCheckThreadStatusInterval = DEFAULT_INTERVAL;
    if (0 == sys_config.get(strServerName, "CheckThreadInterval", strBuf))
    {
        m_uiCheckThreadStatusInterval = ACE_OS::strtoul( strBuf, &endPtr, 10 );

        if (( m_uiCheckThreadStatusInterval < MIN_INTERVAL)
        || ( m_uiCheckThreadStatusInterval > MAX_INTERVAL))
        {
            SVS_LOG(( SVS_LM_WARNING,
                        "config item[CheckThreadInterval][%u] invalid, "
                        "use default value[%u].",
                           m_uiCheckThreadStatusInterval, DEFAULT_INTERVAL));
            m_uiCheckThreadStatusInterval = DEFAULT_INTERVAL;
        }
    }

    m_uiRestartServerFlag = 0;
    m_uiRestartServerFlag = 0;
    if (0 == sys_config.get(strServerName, "RestartServerFlag", strBuf))
    {
        m_uiRestartServerFlag = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }

    m_uiDumpServerFlag = 0;
    m_uiDumpServerFlag = 0;
    if (0 == sys_config.get(strServerName, "DumpServerFlag", strBuf))
    {
        m_uiDumpServerFlag = ACE_OS::strtoul( strBuf, &endPtr, 10 );
    }

    return 0;
}


