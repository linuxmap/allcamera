#ifndef __ACCESS_CONTROL_STACK_H__
#define __ACCESS_CONTROL_STACK_H__
#include <svs_ace_header.h>
#include "vms/vms.h"
#include <map>
#include <list>
#include "svs_control_msg_define.h"
#include "svs_control_msg.h"
#include "svs_acm_async.h"
#include "svs_acm_request.h"
#include "svs_acm_response.h"
extern "C"{
#include "event2/http.h"
#include "event.h"
#include "event2/buffer.h"
#include "event2/keyvalq_struct.h"
#include "evhttp.h"
#include "event2/dns.h"
#include "event2/thread.h"

}

#define HTTP_OPTION_TIMEOUT            864000
#define HTTP_REQUEST_MAX               4096


typedef std::list<CControlMessage*>         WAITMSGLIST;
typedef WAITMSGLIST::iterator               WAITMSGLISTITER;

class CHttpMessage;

class CAccessControlStack: public SVS_ACM::IAsync, public ACE_Task<ACE_MT_SYNCH>
{
public:
    friend class CHttpConnect;
public:
    static CAccessControlStack& instance()
    {
        static CAccessControlStack objAccessControlStack;
        return objAccessControlStack;
    }
    virtual ~CAccessControlStack();
    int32_t open(void *args = 0);

    int32_t close(u_long flags = 0);

    int32_t svc(void);

    /* 异步通知接口 */
    virtual int32_t asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack = NULL, void* pUserData = NULL);

    /* 异步回调接口 */
    static void asyncResponse(SVS_ACM::RESPONSE& rResponse, void* pUserData);

    void handle_asyncResponse(SVS_ACM::RESPONSE& rResponse,CControlMessage* pMessage);

    int32_t handle_http_reponse(struct evhttp_request* req,std::string& strMsg);
public:
    static void  http_callback(struct evhttp_request *req, void *arg);
protected:

    void  handle_http_req(struct evhttp_request *req);
protected:
    CAccessControlStack();
private:
    int32_t send_device_info(SVS_ACM::REQUEST_NOTIFY_DEV_INFO& rNotify, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData);
private:
    uint32_t getThreadIndex();

    void ReactorThread();

    void MsgCheckThread();

    void CheckMessage();
private:
    bool                            m_bRunning;
    struct event_base              *m_httpBase;
    struct evdns_base              *m_pDnsbase;
    struct evhttp                  *m_httpServer;

    ACE_Recursive_Thread_Mutex      m_MapMutex;
    WAITMSGLIST                     m_MsgWaitList;

    uint32_t                        m_unThreadIndex;
    ACE_Thread_Mutex                m_ThreadIndexMutex;
};
#endif
