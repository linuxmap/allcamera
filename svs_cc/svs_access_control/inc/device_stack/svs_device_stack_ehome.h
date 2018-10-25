#ifndef _SVS_DEVICE_STACK_EHOME_H_
#define _SVS_DEVICE_STACK_EHOME_H_

#include "svs_device_stack.h"
#include "HCEHomePublic.h"
#include "HCEHomeCMS.h"
#include "svs_ace_header.h"
#include <map>

struct EHOME_DEV_INFO
{
    LONG                   lUserID;
    NET_EHOME_DEV_REG_INFO regInfo;
    NET_EHOME_DEVICE_INFO  devInfo;
};
typedef std::map<std::string, EHOME_DEV_INFO*> MAP_EHOME_DEV;

class CDeviceStackEhome : public IDeviceStack
{
public:
    CDeviceStackEhome();
    virtual ~CDeviceStackEhome();

    virtual int32_t initialize();
    virtual int32_t release();

    virtual int32_t start();
    virtual int32_t stop();

    virtual int32_t timeout();

    virtual int32_t asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack = NULL, void* pUserData = NULL);

public:
    void handle_ehome_event(LONG lUserID, DWORD dwDataType, void *pOutBuffer,
                            DWORD dwOutLen,void *pInBuffer, DWORD dwInLen);
    void notifyDeviceOnline(EHOME_DEV_INFO* pDevInfo);
    void notifyDeviceOffline(EHOME_DEV_INFO* pDevInfo);

private:
    static BOOL ehomeDeviceEventCallback(LONG lUserID, DWORD dwDataType, void *pOutBuffer, DWORD dwOutLen,
                                                 void *pInBuffer, DWORD dwInLen, void *pUser);
    int32_t ehomeMediaRequest(SVS_ACM::REQUEST_SEND_INVITE2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData);
    int32_t ehomeMediaPlayRequest(SVS_ACM::REQUEST_SEND_ACK2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData);
    int32_t ehomeMediaStopRequest(SVS_ACM::REQUEST_SEND_BYE2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData);
private:
    void generateLensIDbycChannel(uint8_t* pszDevID,LONG lChannel,uint8_t* pszLenID);
    LONG getChannelIDbyLensID(uint8_t* pszLenID);
    std::string getDevIDbyUserID(LONG lUserID);
    LONG getUserIDbyDevID(uint8_t* pszDevID);
private:
    LONG                                 m_ulEhomeHandle;

    ACE_Recursive_Thread_Mutex           m_mutex;
    MAP_EHOME_DEV                        m_EhomeDevMap;
};

#endif