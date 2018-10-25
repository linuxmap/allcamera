/*
 * CServerManager.h
 *
 *  Created on: Sep 16, 2014
 *      Author: timehunter
 */

#ifndef __CSERVERMANAGER_H__
#define __CSERVERMANAGER_H__
#include "svs_ace_header.h"
#include <vms/vms.h>
#include "svs_vms_stack.h"
#include "svs_vms_stack_comm.h"
#include <map>
#include "svs_logging_strategy.h"
#include "svs_log_msg.h"
#include <list>
using namespace std;

#define SERVER_THREAD_MAX     4
#define SEND_TIMEOUT          100


class CSVSLoger: public ISvsLogger
{
public:
    CSVSLoger(){};
    virtual ~CSVSLoger(){};

    static CSVSLoger *instance()
    {
        static CSVSLoger svsLoger;
        return &svsLoger;
    }

    void writeLog(uint8_t ucType, uint8_t ucLevel, const char *pszLog,
                             uint32_t unLogLength, char* pFile, int32_t lLine);
};


class CServer
{
public:
    friend class CServerManager;
    friend class CBusinessManager;
    friend class CRealTimeBusiness;
public:
    CServer();
    virtual ~CServer();
public:
    char*       getServerID();
    uint32_t    getServerIndex();
    uint32_t    getServerType();
    char*       getServerName();
    void        setServerIP(const char* pszServerIP);
    char*       getServerIP();

    std::string  getMediaServerIP();
    uint16_t     getMediaRtspPort();
    uint16_t     getMediaHlsPort();
    uint16_t     getMediaRtmpPort();

    void           setLastRunTime(uint32_t uiLastRunTime);
    uint32_t      getLastRunTime();

    void           setNetType(uint32_t uiNetType);
    uint32_t      getNetType();

    void           setResetFlags(uint32_t uiResetFlags);
    uint32_t      getResetFlags();
    virtual void offline(){};
protected:
    void  setServerID(const char* pszServerID);
    void  setServerName(const char* pszSvrName);

protected:
    void  setServerIndex(uint32_t ulIndex);
    int32_t  addReference();
    int32_t decReference();
    int32_t reference();

protected:
    void     setMediaSvrIP(const char* pszServerIP);
    void     SetMediaRtspPort(uint16_t usPort);
    void     SetMediaHlsPort(uint16_t usPort);
    void     SetMediaRtmpPort(uint16_t usPort);

protected:
    ACE_Thread_Mutex              m_mutex;
    uint32_t                               m_uiServerIndex;
    uint32_t                               m_uiRefCount;

    uint32_t                               m_uiServerType;
    uint32_t                               m_uiResetFlags;

    char                                    m_szServerName[SERVER_NAME_LEN+1];
    char                                    m_szServerID[SERVER_ID_LEN+1];
    char                                    m_szServerIP[SVS_IP_LEN+1];

    std::string                            m_strMeidaSvrIP;
    uint16_t                               m_usRtspServerPort;
    uint16_t                               m_usHLSServerPort;
    uint16_t                               m_usRtmpServerPort;

    uint32_t                               m_uiServiceNetType;
    uint32_t                               m_uiLastRunTime;
  };



class CStreamServer:public CServer
{
public:
    friend class CServerManager;
    friend class CBusinessManager;

public:
    CStreamServer();
    virtual ~CStreamServer();

    uint32_t getAllSessionCount();
    uint32_t findRTSessionbyDevID(const char* pszDevID);
    int32_t  allocStreamforRealtime(const char* pszDevID);
    int32_t  freeStreamforRealtime(const char* pszDevID);
    int32_t  allocStreamforRecord(const char* pszDevID);
    int32_t  freeStreamforRecorde(const char* pszDevID);
    void     updateLoadCount(uint32_t ulCount);

    void     display_streamsvr_state();
    int32_t handle_business_report_req(const char* pszMsg);
    int32_t handle_load_info_report_req(const char* pszMsg);

    uint32_t load_type();
    int32_t get_health_value();

    virtual void offline();
    void     outputSessionInfo(string &strDest);

private:
    bool    is_load_light();
    bool    is_load_heavy();
private:

    uint32_t                                                        m_uiTotalCapacity;
    uint32_t                                                        m_uiPortUsage;

    typedef struct _tagDevSession
    {
        char        m_szDevID[CONTENT_ID_LEN+1];
        uint32_t    m_uiSessionCount;
    }DevSession;

    typedef std::map<std::string,DevSession*>   RTDevMAP;
    RTDevMAP                                                     m_DevBindMap;
    uint32_t                                                         m_uibindSessionCount;

    uint32_t                                                         m_uiBusinessTotalNum;
    std::list<SVS_SBUSINESS_REPORT_INFO>      m_BusinessInfoList;

    ACE_Recursive_Thread_Mutex                         m_mutex;


    // load info
    uint32_t                                                         m_uiTransmitNumber;
    uint32_t                                                         m_uiTotalMemorySize;
    uint32_t                                                         m_uiUsedMemorySize;
    uint32_t                                                         m_uiUsedCpuLoad;
    uint32_t                                                         m_uiIowaitCpuLoad;
    uint32_t                                                         m_uiCacheTotalNum;
    uint32_t                                                         m_uiCacheUsedNum;
    uint32_t                                                         m_uiNetworkCardNum;
    std::list<NETWORK_CARD_INFO>                    m_listNetworkCardInfo;

};

class CRecordServer:public CServer
{
public:
    friend class CServerManager;
    friend class CBusinessManager;

public:
    CRecordServer();
    virtual ~CRecordServer();

    uint32_t getAllSessionCount();
    uint32_t findRTSessionbyContent(const char* pszContentID);
    int32_t  allocStreamforRealtime(const char* strContendId);
    int32_t  freeStreamforRealtime(const char* strContendId);
    int32_t  allocStreamforRecord(const char* strContendId);
    int32_t  freeStreamforRecorde(const char* strContendId);
    void     updateLoadCount(uint32_t ulCount);

    void     display_streamsvr_state();
    void     outputSessionInfo(string &strDest);

private:


    uint32_t        m_uiTotalCapacity;
    uint32_t        m_uiPortUsage;

    typedef struct _tagContentSession
    {
        char        m_szContentID[CONTENT_ID_LEN+1];
        uint32_t    m_uiSessionCount;
    }ContentSession;

    typedef std::map<std::string,ContentSession*> RTCOUNTMAP;
    RTCOUNTMAP      m_ContentBindMap;
    uint32_t        m_uibindSessionCount;

    ACE_Recursive_Thread_Mutex m_mutex;
};



using namespace std;

class CServerManager: public ISvsMsgHandler, public ACE_Task<ACE_MT_SYNCH>
{
public:
    static CServerManager& instance()
    {
        static CServerManager objServerManager;
        return objServerManager;
    }
    virtual ~CServerManager();

    // called by external component.
    int32_t initialize();

    int32_t start();

    int32_t stop();

    int32_t release();

    void freeMessageBlock(ACE_Message_Block* &mb);
    ACE_Message_Block* allockMessageBlock(size_t length, ACE_Message_Block::ACE_Message_Type type = ACE_Message_Block::MB_DATA);
    CServer* findServer(const char* pszServerID);
    CServer* findServer(const uint32_t ulServerIndex);
    void     ReleaseServer(CServer* pServer);
    int32_t sendMessage(const uint32_t unHandleIndex,char *pDatabuffer,uint32_t unLength);

protected:
    int32_t open();
    void    close();
    int32_t svc();

public:
    // put the message into the queue.
    void sndMsgToSvrMgr(ACE_Message_Block *mb);

    void handleMessage(const char* pszMsg,const uint32_t unMsgLength,
                       const uint32_t unHandleIndex,Result &stResult);
    void handleConnEvent(const ConnEvent* pstEvent);
public:
    void show_server_info(const std::string& strSvrID,std::string& Info);
public:
    CServer* allocMuServer(PLAY_TYPE enParaPlayType,const char* pszDevID);

private:
    CServer* allocStreamSvrforRealtime(const char* pszDevID);
    CServer* allocExistStreamSvrforRealtime(const char* pszDevID);
    CServer* allocBestStreamSvrforRealtime(const char* pszDevID);
    CServer* allocRecordSvrforRecord(const char* strContendId);


protected:
    CServerManager();
private:
    uint32_t getthreadindex()
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex_, 0);
        uint32_t ulIndex = m_ulThreadIndex;
        m_ulThreadIndex++;
        return ulIndex;
    };
    int32_t openqueues();
    int32_t openlistener();
    void processMsgThread(uint32_t ulThreadIndex);
    int32_t processMsg(ACE_Message_Block *mb);
private:
    void handleServerReg(uint32_t ulHandleIndex,const char* pszMsg);
    void handleServerBusinessReport(uint32_t ulHandleIndex,const char* pszMsg);
    void handleServerLoadInfoReq(uint32_t ulHandleIndex,const char* pszMsg);

private:
    int32_t AddServer(uint32_t ulHandleIndex,SVS_MSG_SERVER_REGIST_REQ* pReqMsg ,CServer*& pServer);
    void    RemoveServer(CServer* pServer);
private:
    std::map<uint32_t,CServer*>           m_ServerIndexMap;
    std::map<std::string,CServer*>        m_ServerIDMap;
    ACE_Recursive_Thread_Mutex            m_mutex_;
    uint32_t                              m_ulThreadIndex;
    ACE_Message_Queue <ACE_SYNCH>         m_QueueArray[SERVER_THREAD_MAX];
    bool                                  m_bRunning;
    ILink*                                m_pServerLink;


    std::string                         m_strACSvrID;

};

#endif /* CSERVERMANAGER_H_ */
