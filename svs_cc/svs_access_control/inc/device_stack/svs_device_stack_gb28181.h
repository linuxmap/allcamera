#ifndef _SVS_DEVICE_STACK_GB28181_H_
#define _SVS_DEVICE_STACK_GB28181_H_

#include "svs_device_stack.h"
#include "eXosip2/eXosip.h"
#include "svs_ace_header.h"
#include <map>
#define ALLCAM_AGENT_NAME "Allcam Dev Server"
class CDeviceStackGB28181 : public IDeviceStack, public ACE_Task<ACE_MT_SYNCH>
{
public:
    CDeviceStackGB28181();
    virtual ~CDeviceStackGB28181();

    virtual int32_t initialize();
    virtual int32_t release();

    virtual int32_t start();
    virtual int32_t stop();

    virtual int32_t timeout();

    virtual int32_t svc();

    virtual int32_t asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack = NULL, void* pUserData = NULL);

    int32_t notifyDeviceOnline(SVS_ACM::REQUEST_NOTIFY_DEV_INFO& rRequest);
    int32_t notifyDeviceKeepAlive(const char* pszDeviceID);

private:
    int32_t initExosip();
    int32_t activeThread();
    void    receiveThread();
    void    dealSipEvent(eXosip_event_t *pEvent);
    int32_t checkUserName(eXosip_event_t *pEvent);
    int32_t notifyDeviceOffline(const char* pszDeviceID);

    void    sendQueryCatalogReq();
    void    checkExpires();

    int32_t handleRegisterReq(eXosip_event_t& rEvent);
    int32_t handleDeviceRegister(eXosip_event_t& rEvent, bool& bIsNewRegister, std::string& strTo,
                                 std::string& strBody, int32_t nExpires);
    int32_t handleDeviceUnRegister(eXosip_event_t& rEvent);
    int32_t handleMessageReq(eXosip_event_t& rEvent);
    int32_t handleMessageAnswered(eXosip_event_t& rEvent);
    int32_t handleMessageAnsweredError(eXosip_event_t& rEvent);
    int32_t handleCallAnswered(eXosip_event_t& rEvent);
    int32_t handleCallAnsweredError(eXosip_event_t& rEvent);
    int32_t handleCallMessageAnswered(eXosip_event_t& rEvent);
    int32_t response(eXosip_event_t& rEvent, int32_t nErrorCode);

    int32_t convertDeviceID2SipToHeader(const char* pszDeviceID, std::string& strTo);
    int32_t convertLensID2SipToHeader(const char* pszDeviceID,const char* pszLensID, std::string& strTo);
    int32_t sendMessageReq(std::string& strTo, std::string& strBody, int32_t& nTransactionID);
    int32_t sendSubscribeReq(std::string& strTo, std::string& strBody);
    int32_t sendInviteReq(std::string& strTo, std::string& strBody, std::string& strSubject, int32_t& nCallID);

    int32_t sendInviteReq(SVS_ACM::REQUEST_SEND_INVITE2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData);
    int32_t sendByeReq(SVS_ACM::REQUEST_SEND_BYE2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData);
    int32_t notifyAck(SVS_ACM::REQUEST_SEND_ACK2DEV& rAck);
    int32_t sendKeyFrameReq(SVS_ACM::REQUEST_SEND_KEYFRAME2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData);
    int32_t sendDevCtrlReq(SVS_ACM::REQUEST_DEV_CTRL& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData);

private:
    int32_t handleXmlMsgReq(const char* pszXML);

private:
    class CExOsipLockGuard
    {
    public:
        CExOsipLockGuard(struct eXosip_t *pEXosipCtx) : m_pEXosipCtx(pEXosipCtx)
        {
            eXosip_lock(m_pEXosipCtx);
        }

        ~CExOsipLockGuard()
        {
            eXosip_unlock(m_pEXosipCtx);
        }

    private:
        struct eXosip_t *m_pEXosipCtx;
    };

    struct USER_INFO
    {
        char szUserName[SVS_DEVICEID_LEN + 1];
        char szHost[SVS_IP_LEN + 1];
        char szPort[SVS_PORT_LEN + 1];
        int32_t nExpires;
        time_t tLastRegisterTime;
        time_t tLastKeepAliveTime;
        std::string strTo;

        USER_INFO()
            : nExpires(0)
            , tLastRegisterTime(0)
            , tLastKeepAliveTime(0)
        {
            memset(szUserName, 0, sizeof(szUserName));
            memset(szHost, 0, sizeof(szHost));
            memset(szPort, 0, sizeof(szPort));
            strTo           = "sip:";
        }
    };

    struct CALLBACK_INFO
    {
        int32_t nRequestID;
        SVS_ACM::RESPONSE_CALLBACK pCallBack;
        void* pUserData;

        CALLBACK_INFO()
        {
            nRequestID  = 0;
            pCallBack   = NULL;
            pUserData   = NULL;
        }
    };

    struct INVITE_INFO : CALLBACK_INFO
    {
        int32_t nCallID;
        int32_t nDialogID;
        char szLensID[SVS_DEVICEID_LEN + 1];

        INVITE_INFO()
        {
            nCallID     = 0;
            nDialogID   = 0;
            memset(szLensID, 0, sizeof(szLensID));
        }
    };

    struct TRANSACTION_INFO : CALLBACK_INFO
    {
        int32_t nTransactionID;

        TRANSACTION_INFO()
        {
            nTransactionID  = 0;
        }
    };

    typedef std::map<std::string, USER_INFO> MAP_USER;
    typedef std::map<int32_t, INVITE_INFO> MAP_CALL_ID_2_INVITE_INFO;
    typedef std::map<std::string, INVITE_INFO*> MAP_LENS_ID_2_INVITE_INFO;
    typedef std::map<int32_t, TRANSACTION_INFO> MAP_TID_2_INFO;

    struct eXosip_t                     *m_pEXosipCtx;
    std::string                          m_strFrom;
    bool                                 m_bRunning;
    time_t                               m_tSendQueryCatalogReqTime;

    ACE_Recursive_Thread_Mutex           m_mutex;
    MAP_USER m_mapUser;

    ACE_Recursive_Thread_Mutex           m_mutexMapInviteInfo;
    MAP_CALL_ID_2_INVITE_INFO            m_mapCallID2InviteInfo;
    MAP_LENS_ID_2_INVITE_INFO            m_mapLensID2InviteInfo;

    ACE_Recursive_Thread_Mutex           m_mutexMapTID2Info;
    MAP_TID_2_INFO                       m_mapTID2Info;

    ACE_Atomic_Op<ACE_Thread_Mutex, int> m_nThreadIndex;
};

#endif