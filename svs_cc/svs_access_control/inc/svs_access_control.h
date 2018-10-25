#ifndef _SVS_Scheduler_h
#define _SVS_Scheduler_h

#include <list>
#include <vector>
#include "svs_config.h"
#include "svs_ac_common.h"
#include "svs_access_control_task.h"
#include "svs_ace_header.h"

void freeMessageBlock(ACE_Message_Block* &mb);
ACE_Message_Block* allockMessageBlock(
    size_t length,
    ACE_Message_Block::ACE_Message_Type type = ACE_Message_Block::MB_DATA);

class CAccessControlSvr
{
public:

    static CAccessControlSvr& instance()
    {
        static CAccessControlSvr objAccessCtrlSvr;
        return objAccessCtrlSvr;
    }

    virtual ~CAccessControlSvr();

    bool init();

    int32_t relase();

    bool init_configure();


    // read dynamic config iterms
    int32_t read_dynamic_config_iterms();


    const char * getSelfName()
    {
        return m_szSvrName;
    }

    char * getSelfID()
    {
        return m_szSvrID;
    }

    const char * getSvrIp()
    {
        return m_szSvrIP;
    }

    uint32_t getSvrIP(int32_t)
    {
        return m_uiSvrIP;
    }

    const char * getHttpStackListenAddr()
    {
        if (0 != strlen(m_szHttpStackListenIP))
        {
            return m_szHttpStackListenIP;
        }

        return m_szSvrIP;
    }
    uint32_t getHttpStackListenIP()
    {
        if (0 != strlen(m_szHttpStackListenIP))
        {
            return m_uiHttpStackListenIP;
        }

        return m_uiSvrIP;
    }

    uint32_t getHttpStackListenPort()
    {
        return m_uiHttpStackListenPort;
    }

    const char * getHttpStackFWAddr()
    {
        if (0 != strlen(m_szHttpStackFwIP))
        {
            return m_szHttpStackFwIP;
        }

        return getHttpStackListenAddr();
    }

    uint32_t getHttpStackFwIP()
    {
        if (0 != strlen(m_szHttpStackFwIP))
        {
            return getHttpStackListenIP();
        }

        return m_uiHttpStackFwIP;
    }

    uint32_t getHttpStackFwPort()
    {
        return m_uiHttpStackFwPort;
    }

    const char * getControlServerAddr()
    {
        return m_szControlServerIP;
    }
    uint32_t getControlServerIP()
    {
        return m_uiControlServerIP;
    }

    uint32_t getControlServerPort()
    {
        return m_uiControlServerPort;
    }

    const char * getMuListenIP()
    {
        if (0 != strlen(m_szMuListenIP))
        {
            return m_szMuListenIP;
        }

        return m_szSvrIP;
    }

    uint32_t getMuListenIP(int32_t)
    {
        if (0 != strlen(m_szMuListenIP))
        {
            return m_uiMuListenIP;
        }

        return m_uiSvrIP;
    }

    uint32_t getMuListenPort()
    {
        return m_uiMuListenPort;
    }

    uint32_t getDebugListenPort()
    {
        return m_uiDebugPort;
    }


    uint32_t get_check_thread_status_interval() const
    {
        return m_uiCheckThreadStatusInterval;
    }

    uint32_t get_restart_server_flag() const
    {
        return m_uiRestartServerFlag;
    }

    uint32_t get_dump_server_flag() const
    {
        return m_uiDumpServerFlag;
    }

public: //for device access
    bool init_configure_for_device_access(CAC_Config& rConfig);

    uint32_t get_query_catalog_interval() const
    {
        return m_nQueryCatalogInterval;
    }

    uint32_t get_Dev_Report_Interval() const
    {
        return m_nDevReportInterval;
    }

    int32_t get_device_gb28181_stack_listen_port() const
    {
        return m_nGB28181ListenPort;
    }

    int32_t get_device_ehome_stack_listen_port() const
    {
        return m_nEhomeListenPort;
    }

    uint32_t get_device_keep_alive_time_out() const
    {
        return m_nDeviceKeepAliveTimeout;
    }

protected:
    CAccessControlSvr();

protected:
    char       m_szSvrName[SVS_SERVER_NAME_LEN + 1];
    char       m_szSvrID[SVS_SERVER_ID_MAX_LEN+1];

    // IP for svs_access server
    char       m_szSvrIP[SVS_IP_LEN+1];
    ACE_UINT32 m_uiSvrIP;

    // IP and port for http stack
    char       m_szHttpStackListenIP[SVS_IP_LEN+1];
    ACE_UINT32 m_uiHttpStackListenIP;
    uint32_t   m_uiHttpStackListenPort;

    // IP and port for http stack firewall
    char       m_szHttpStackFwIP[SVS_IP_LEN+1];
    ACE_UINT32 m_uiHttpStackFwIP;
    uint32_t   m_uiHttpStackFwPort;

    char       m_szControlServerIP[SVS_IP_LEN+1];
    ACE_UINT32 m_uiControlServerIP;
    uint32_t   m_uiControlServerPort;

    // IP and port for mu
    char       m_szMuListenIP[SVS_IP_LEN+1];
    ACE_UINT32 m_uiMuListenIP;
    uint32_t   m_uiMuListenPort;

    // debug port
    uint32_t   m_uiDebugPort;


    uint32_t   m_uiCheckThreadStatusInterval;
    uint32_t   m_uiRestartServerFlag;
    uint32_t   m_uiDumpServerFlag;

    //for device access
    uint32_t    m_nDevReportInterval;
    uint32_t    m_nDeviceKeepAliveTimeout;
    //for GB28181 device access
    uint32_t    m_nQueryCatalogInterval;
    int32_t     m_nGB28181ListenPort;
    // for ehome device access
    int32_t     m_nEhomeListenPort;
};



#endif //_SVS_Scheduler_h

