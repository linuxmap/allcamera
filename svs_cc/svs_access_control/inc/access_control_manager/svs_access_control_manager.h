#ifndef _SVS_ACCESS_CONTROL_MANAGER_H_
#define _SVS_ACCESS_CONTROL_MANAGER_H_

#include "vms/vms.h"
#include "svs_ace_header.h"
#include <map>
#include <vector>
#include "svs_acm_async.h"

class IAccessControlManager : public SVS_ACM::IAsync, public ACE_Task<ACE_MT_SYNCH>
{
public:
    static IAccessControlManager& instance()
    {
        static IAccessControlManager obj;
        return obj;
    }

    int32_t initialize();
    int32_t release();

    int32_t start();
    int32_t stop();

    virtual int32_t svc();

    //interface for all
    //异步请求：需设置回调
    //异步通知：不需设置回调
    virtual int32_t asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack = NULL, void* pUserData = NULL);

    //interface for control stack
    int32_t getDeviceInfo(std::string& strDevID, SVS_ACM::DEVICE_INFO& stDeviceInfo);
    int32_t getLensInfo(std::string& strLensID, SVS_ACM::LENS_INFO& stLensInfo);
    int32_t getRtspUrl(std::string& strLensID, std::string& strRtspUrl);

public:
    static void device_report_timer_callback(void *pArg, int32_t eventId, int32_t timerId, int32_t nDummy);

    void report_device_info();
public:
    // for debug
    void show_deviceInfo(const std::string& strDevID,std::string& Info);
    void show_LensInfo(const std::string& strLensID,std::string& Info);

private:
    IAccessControlManager();
    virtual ~IAccessControlManager();

    int32_t notifyDeviceInfo(SVS_ACM::REQUEST_NOTIFY_DEV_INFO& stNodifyInfo);
    int32_t notifyResponse(const SVS_ACM::RESPONSE& rResponse);
    int32_t handleToDevReq(SVS_ACM::REQUEST_TO_DEV& rToDev, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData);

private:
    typedef std::map<std::string, SVS_ACM::DEVICE_INFO> MAP_DEV_INFO;
    typedef std::map<std::string, SVS_ACM::LENS_INFO*> MAP_LENS_INFO;

    MAP_DEV_INFO    m_mapDeviceInfo;
    MAP_LENS_INFO   m_mapLensInfo;
    ACE_Recursive_Thread_Mutex m_mutex;

    bool    m_bRunning;
    ACE_Message_Queue<ACE_MT_SYNCH> m_queue;

    int32_t m_nTimerID;
};

#endif
