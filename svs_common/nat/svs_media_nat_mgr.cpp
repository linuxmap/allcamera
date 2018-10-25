// CMduNatMgr.cpp: implementation of the CMduNatMgr class.
//
//////////////////////////////////////////////////////////////////////


#include "svs_media_nat_mgr.h"
#include "svs_log_msg.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMduNatMgr::CMduNatMgr()
{
    m_bNatAG = false;
    m_pTimeReactor = NULL;
}

CMduNatMgr::~CMduNatMgr()
{

}
int32_t CMduNatMgr::Open(string strConfigfile,ACE_Reactor *pTimeReactor)
{
    if(NULL == pTimeReactor)
    {
        SVS_LOG((SVS_LM_WARNING,"open Mdu Nat Mgr,the timeReactor is NULL"));
        return -1;
    }
    m_strConfigFile = strConfigfile;
    m_pTimeReactor = pTimeReactor;

    int32_t lResult = ReadNatConfig();
    if (-1 == lResult)
    {
        SVS_LOG((SVS_LM_WARNING, "Read nat Config failed."));
        return lResult;
    }
    else
    {
        SVS_LOG((SVS_LM_DEBUG, "Read nat Config success."));
    }
    ACE_Time_Value  tv(NATIP_REFRESH_CONFIG_TIME);
    lResult = m_pTimeReactor->schedule_timer(this, 0, tv,ACE_Time_Value::zero);

    if (-1 == lResult)
    {
        SVS_LOG((SVS_LM_WARNING, "nat Config schedule timer failed."));
        return -1;
    }
    else
    {
        SVS_LOG((SVS_LM_DEBUG, "nat Config schedule timer success."));
    }
    return 0;
}

void CMduNatMgr::CheckNatIP(uint32_t ulSrcIP,uint32_t& ulNatIP)
{
    ulNatIP = ulSrcIP;
    if(false == m_bNatAG)
    {
        return;
    }
    ACE_GUARD_REACTION(ACE_Recursive_Thread_Mutex, pObj, m_mutex, return);
    MDUNATMAP::iterator iter = m_MduNatIPMap.find(ulSrcIP);
    if(iter == m_MduNatIPMap.end())
    {
        SVS_LOG((SVS_LM_DEBUG,"not find ip:[%u]'s nat ip",ulSrcIP));
        return;
    }
    ulNatIP = (uint32_t)iter->second;
    SVS_LOG((SVS_LM_DEBUG,"nat the ip:[%u] to :[%u]",ulSrcIP,ulNatIP));
    return;
}
int32_t CMduNatMgr::ReadNatConfig()
{
    SVS_LOG((SVS_LM_DEBUG,"Read the nat config Start!"));
    char   szReadBuffer[1024] = {0};

    CAC_Config  config(m_strConfigFile.c_str());

    if(0 != config.get("MduMediaNatConfig","MduIPNatConfig",szReadBuffer))
    {
        SVS_LOG((SVS_LM_WARNING,"Read Mdu nat config failed!"));
        return -1;
    }
    int32_t nConfig = atoi(szReadBuffer);
    if(1 != nConfig)
    {
        m_bNatAG = false;
        return 0;
    }
    else
    {
        m_bNatAG = true;
    }

    //需要操作map,加锁
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, -1);
    m_MduNatIPMap.clear();
    int32_t lResult = 0;
    char szNatIP[128] = {0};

    for(int32_t i = 1 ; i <= NATIP_COUNT_MAX;i++ )
    {
        memset(&szNatIP,0,128);
        memset(&szReadBuffer,0,1024);
        (void) snprintf(szNatIP,sizeof(szNatIP),"MduNatIP%d",i);
        lResult = config.get("MduMediaNatIPList",szNatIP,szReadBuffer);
        if(0 != lResult)
        {
            break;
        }
        string strSrc;
        string strDest;
        string strNat = szReadBuffer;
        string::size_type first_index = 0;
        string::size_type semicolon_index = strNat.find('|', first_index);
        if( string::npos == semicolon_index)
        {
            continue;
        }
        strSrc = strNat.substr(first_index, semicolon_index);
        strDest = strNat.substr(semicolon_index + 1, strNat.length());

        struct in_addr SrcInetAddr;
        memset((void*)&SrcInetAddr, 0x0, sizeof(struct in_addr));

        //检查 源目的地址合法性
        if (0 == inet_aton(strSrc.c_str(), &SrcInetAddr))
        {
            SVS_LOG((SVS_LM_ERROR,
                    "Fail to convert ip addr pointer by string to digital. pszIp[%s].",
                                                                      strSrc.c_str()));
            continue;
        }
        struct in_addr DestInetAddr;
        memset((void*)&DestInetAddr, 0x0, sizeof(struct in_addr));
        if (0 == inet_aton(strDest.c_str(), &DestInetAddr))
        {
            SVS_LOG((SVS_LM_ERROR,
                    "Fail to convert ip addr pointer by string to digital. pszIp[%s].",
                                                                   strDest.c_str()));

            continue;
        }
        if(m_MduNatIPMap.end() != m_MduNatIPMap.find(SrcInetAddr.s_addr))
        {
            SVS_LOG((SVS_LM_ERROR,"the Repeat Config the nat info pszIp[%s].",
                                                              strDest.c_str()));
            continue;
        }
        m_MduNatIPMap.insert(MDUNATMAP::value_type(SrcInetAddr.s_addr, DestInetAddr.s_addr));
        SVS_LOG((SVS_LM_DEBUG,"Refresh the nat config ,SourceIP:[%s],NatIP:[%s]",
                                                            strSrc.c_str(),
                                                            strDest.c_str()));
    }
    return 0;
}
int32_t CMduNatMgr::handle_timeout(const ACE_Time_Value &tv, const void *arg)
{
    if(NULL != arg)
    {
        SVS_LOG((SVS_LM_CRITICAL,"the nat manager is error!"));
        return 0;
    }
    ReadNatConfig();
    return 0;
}
