/**
* @file    SvsClientHandle.h
* @brief   SvsЭ��ͻ��˴���������
*
* Copyright (c) 2010 AllCam Tech.Co.,Ltd
*
* @date      2010-12-10
*/

#ifndef __SVSCLIENTHANDLE_H__
#define __SVSCLIENTHANDLE_H__

#include <list>
#include "svs_ace_header.h"
#include <vms/vms.h>
#include "svs_vms_stack.h"

#define STATUS_CHECK_INTERVAL (100 * 1000)

#define MAX_RETRY_INTERVAL    15

#define SEND_ALARM_INTERVAL (MAX_RETRY_INTERVAL * 1000 * 1000/ STATUS_CHECK_INTERVAL)

#define DEFAULT_RESP_MSG_TIMEOUT 60

typedef std::list<uint16_t>              LOCAL_PORT_LIST;
typedef std::list<uint16_t>::iterator    LOCAL_PORT_LIST_ITER;

typedef enum _HANDLE_STATUS
{
    HANDLE_STATUS_DISCONNECT  = 0x0,
    HANDLE_STATUS_CONNECTING  = 0X1,
    HANDLE_STATUS_CONNECTED   = 0x2,
    HANDLE_STATUS_REGISTING   = 0x3,
    HANDLE_STATUS_REGISTED    = 0x4
} HANDLE_STATUS;

class CSvsClientHandle : public ISvsMsgHandler, public ACE_Task<ACE_MT_SYNCH>
{
public:
    CSvsClientHandle();

    virtual ~CSvsClientHandle();

    UTAPI int32_t init(const char *szServiceID,
             uint32_t unServerType,
             const char *szServerName,
             uint32_t unServiceNetType,
             uint32_t unHearbeatInterval,
              const char *szRegisterPasswd);

    UTAPI void setServerAddr(uint32_t unServerIp, uint16_t usServerPort);

    UTAPI void getServerAddr(uint32_t &unServerIp, uint16_t &usServerPort) const;

    UTAPI void setLocalAddr(uint32_t unLocalIp, const LOCAL_PORT_LIST &localPortList);

    UTAPI void getLocalAddr(uint32_t &unLocalIp, uint16_t &usLocalPort) const;

    UTAPI const char* getServerID() const;

    UTAPI const char* getZoneID() const;

    UTAPI int32_t open(void *);

    UTAPI int32_t restartHandle();

    UTAPI void close();

    UTAPI int32_t svc();

    UTAPI HANDLE_STATUS getStatus() const;

    UTAPI bool reportTaskOK()const;

    UTAPI void handleMessage(const char* pszMsg,
                       const uint32_t unMsgLength,
                       const uint32_t unHandleIndex,
                       Result &stResult);

    UTAPI void handleConnEvent(const ConnEvent* pstEvent);

    UTAPI int32_t sendMessage(char *pDatabuffer,
                    uint32_t unLength,
                    uint32_t unTimeout = DEFAULT_RESP_MSG_TIMEOUT);

    virtual void registerNotify();

    virtual void connectNotify(const ConnEvent* pstEvent);

    virtual void handleRecvMessage(ACE_Message_Block *pMsg) = 0;

    virtual void handleFailedMessage(ACE_Message_Block *pMsg) = 0;

    virtual void handleTaskReport() = 0;

    void setLastRunTime(uint32_t ulRunTime);

    virtual void registServer();

    virtual void handleRegistResp(const char *pszMsg, uint32_t unMsgLen);


private:
    void startConnect();

    void checkConnect();

    void checkRegiste();

    int32_t createMsgBlock(uint32_t unMsgLen, ACE_Message_Block *&pMsg) const;


protected:
    bool           m_bExitFlag;
    bool           m_bFirstConnect;

    HANDLE_STATUS  m_enStatus;
    uint32_t       m_ulStatusTime;
    bool           m_bHandleFailMsg;

    ILink*         m_pClientLink;
    uint32_t       m_unLinkIndex;

    char           m_szServerID[SERVER_ID_LEN + 1];
    uint32_t       m_unServerType;
    uint32_t       m_unResetFlags;
    char           m_szName[SERVER_NAME_LEN + 1];
    uint32_t       m_unServiceNetType;

    char           m_szZoneID[SERVER_ID_LEN + 1];

    ACE_INET_Addr   m_LocalAddr;
    LOCAL_PORT_LIST m_LocalPortList;
    ACE_INET_Addr   m_ServerAddr;

    char           m_szLocalIp[SERVER_ID_LEN];
    char           m_szServerIp[SERVER_ID_LEN];


    uint32_t       m_ulServerLastRunTime;

    bool           m_bReportTaskFlag;

    char           m_szRegisterPasswd[AUTH_PASSWD_LEN + 1];
    uint32_t       m_nRegisterPasswdLen;
};

#endif /* __SVSCLIENTHANDLE_H__ */

