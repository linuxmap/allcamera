// CMduNatMgr.h: interface for the CMduNatMgr class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CMDUNATMGR_H__15F2433A_2E4E_47BB_94F5_DF9E999E6F70__INCLUDED_)
#define AFX_CMDUNATMGR_H__15F2433A_2E4E_47BB_94F5_DF9E999E6F70__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "svs_ace_header.h"
#include "svs_log_msg.h"
#include "svs_config.h"
#include <map>
#include <string>
using namespace std;

typedef std::map<uint32_t,uint32_t> MDUNATMAP;
typedef std::map<uint32_t,uint32_t>::iterator MDUNATMAPITER;

#define NATIP_COUNT_MAX      2048

#define NATIP_REFRESH_CONFIG_TIME  600

class CMduNatMgr : public ACE_Event_Handler
{
public:
    static CMduNatMgr& GetInstance()
    {
        static CMduNatMgr mduNatMgr;
        return mduNatMgr;
    }
    virtual ~CMduNatMgr();
public:
    int32_t Open(string strConfigfile,ACE_Reactor *pTimeReactor);
    void CheckNatIP(uint32_t ulSrcIP,uint32_t& ulNatIP);
    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);
protected:
    CMduNatMgr();
private:
    int32_t ReadNatConfig();
private:
    MDUNATMAP                            m_MduNatIPMap;               //nat ip 保存map
    volatile bool                        m_bNatAG;                     //判断是否是NAT AG
    ACE_Recursive_Thread_Mutex           m_mutex;                      //锁
    string                               m_strConfigFile;             //配置文件路径
    ACE_Reactor *                        m_pTimeReactor;              //定时器錜eactor
};

#endif // !defined(AFX_CMDUNATMGR_H__15F2433A_2E4E_47BB_94F5_DF9E999E6F70__INCLUDED_)
