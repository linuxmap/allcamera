#include "svs_log_msg.h"
#include "svs_logging_strategy.h"
#include "svs_utility.h"
#include "svs_access_control.h"
#include "svs_control_stack.h"
#include "svs_ac_common.h"
#include "svs_control_msg_dev_register.h"
#include "svs_control_msg_live_play.h"
#include "svs_daemon_thread.h"
#include "svs_control_msg_dev_ctrl.h"


CAccessControlStack::CAccessControlStack()
{
    m_httpBase   = NULL;
    m_httpServer = NULL;
    m_pDnsbase = NULL;
}

CAccessControlStack::~CAccessControlStack()
{
}
int32_t CAccessControlStack::open(void *args)
{
    int32_t nRet = SVS_ERROR_OK;

    SVS_LOG((SVS_LM_INFO,"CAccessControlStack::open begin."));

    /* create the reactor */
    event_init();
    evthread_use_pthreads();
    m_httpBase = event_base_new();
    if (NULL == m_httpBase)
    {
        SVS_LOG((SVS_LM_CRITICAL,"CAccessControlStack::open,create the event base fail."));
        return SVS_ERROR_FAIL;
    }
    m_httpServer = evhttp_new(m_httpBase);
    if (NULL == m_httpServer)
    {
        SVS_LOG((SVS_LM_CRITICAL,"CAccessControlStack::open,create the http base fail."));
        return SVS_ERROR_FAIL;
    }

    m_pDnsbase = evdns_base_new(m_httpBase, 0);
    if (NULL == m_pDnsbase)
    {
        return SVS_ERROR_FAIL;
    }
    const char* pszAddr = CAccessControlSvr::instance().getHttpStackListenAddr();
    uint16_t usPort     = CAccessControlSvr::instance().getHttpStackListenPort();

    int ret = evhttp_bind_socket(m_httpServer, pszAddr, usPort);
    if (0 != ret)
    {
        SVS_LOG((SVS_LM_CRITICAL,"CAccessControlStack::open,bind the http socket fail."));
        return SVS_ERROR_FAIL;
    }

    evhttp_set_timeout(m_httpServer, HTTP_OPTION_TIMEOUT);
    evhttp_set_gencb(m_httpServer, http_callback, this);


    // Activate threads.
    size_t *stack_size = NULL;
    try
    {
        stack_size = new size_t[AC_HTTP_THREAD_MAX];
    }
    catch (...)
    {
        SVS_LOG((SVS_LM_ERROR, "Create stack size for thread error."));

        delete []stack_size;
        stack_size = NULL;

        return SVS_ERROR_FAIL;
    }

    for ( int32_t i = 0; i < AC_HTTP_THREAD_MAX; i++)
    {
        stack_size[i] = SVS_THREAD_STATCK_MAX;
    }
    m_bRunning = true;
    nRet = activate(
        THR_NEW_LWP ,
        AC_HTTP_THREAD_MAX,
        0,
        ACE_DEFAULT_THREAD_PRIORITY,
        SVS_ERROR_FAIL,
        0,
        0,
        0,
        stack_size,
        0);

    delete []stack_size;
    stack_size = NULL;

    if(SVS_ERROR_OK != nRet) {
        SVS_LOG((SVS_LM_ERROR, "open the http service fail,start thread fail."));
        return nRet;
    }



    SVS_LOG((SVS_LM_INFO,"open http server start success."));
    return SVS_ERROR_OK;

}
int32_t CAccessControlStack::close(u_long flags)
{
    m_bRunning = false;
    SVS_LOG((SVS_LM_INFO,"success to close hls server."));
    return SVS_ERROR_OK;
}
uint32_t CAccessControlStack::getThreadIndex()
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ThreadIndexMutex);
    return m_unThreadIndex++;
}
int32_t CAccessControlStack::svc()
{
    uint32_t unThreadIndex = getThreadIndex();

    switch(unThreadIndex)
    {
        case AC_HTTP_THREAD_REACTOR:
        {
            ReactorThread();
            break;
        }
        case AC_HTTP_THREAD_CHECKMSG:
        {
            MsgCheckThread();
            break;
        }
        default:
        {
            break;
        }
    }
    return SVS_ERROR_OK;
}
void CAccessControlStack::ReactorThread()
{
    (void)signal(SIGPIPE, SIG_IGN);

    SVS_LOG((SVS_LM_INFO,"CAccessControlStack reactor thread start."));

    if (NULL == m_httpBase)
    {
        SVS_LOG((SVS_LM_WARNING,"CAccessControlStack reactor thread exit, resource not created."));
        return ;
    }

    while (m_bRunning)
    {
        event_base_dispatch(m_httpBase);
    }

    SVS_LOG((SVS_LM_INFO,"CAccessControlStack reactor thread  exit."));
}

void CAccessControlStack::MsgCheckThread()
{
    SVS_LOG((SVS_LM_WARNING, "The message check thread start to running..."));

    CThread_Stat_Reporter reporter("ControlCheckMessageThread");

    while ( m_bRunning )
    {
        CheckMessage();

        reporter.ReportStat();

        (void)sleep(10);

    }
    SVS_LOG((SVS_LM_WARNING, "The message process thread exit"));
}
void CAccessControlStack::CheckMessage()
{
    return;
}

int32_t CAccessControlStack::asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_TRACE();
    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::asyncRequest begin"));
    CControlMessage* pMessage = NULL;
    SVS_ACM::REQ_TYPE enType = rRequest.getType();
    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::asyncRequest type %d .", enType));
    switch (enType)
    {
    case SVS_ACM::REQ_TYPE_NOTIFY_DEV_INFO:
        {
            CControlDevRegMsg* pOnline = NULL;
            pMessage = SVS_NEW(pOnline);
            break;
        }
    case SVS_ACM::REQ_TYPE_NOTIFY_ALL_DEV:
        {
            CControlDevReportMsg* pDevReport = NULL;
            pMessage = SVS_NEW(pDevReport);
            break;
        }
    case SVS_ACM::REQ_TYPE_MAX:
    default:
        SVS_LOG((SVS_LM_ERROR, "Async request type %d is invalid.", rRequest.getType()));
        break;
    }

    if(NULL == pMessage)
    {
        SVS_LOG((SVS_LM_WARNING, "CAccessControlStack::asyncRequest, create message object fail."));
        return SVS_ERROR_FAIL;
    }
    pMessage->SetResponseCallBack(pCallBack,pUserData);
    pMessage->SetHttpBase(m_httpBase,m_pDnsbase);

    int32_t nRet = pMessage->CreateMsg(rRequest);
    if(SVS_ERROR_OK != nRet)
    {
        SVS_DELETE(pMessage);
        SVS_LOG((SVS_LM_WARNING, "CAccessControlStack::asyncRequest, create message fail."));
        return SVS_ERROR_FAIL;
    }
    nRet = pMessage->SendMsg();
    if(SVS_ERROR_OK != nRet)
    {
        SVS_DELETE(pMessage);
        SVS_LOG((SVS_LM_WARNING, "CAccessControlStack::asyncRequest, send message fail."));
        return SVS_ERROR_FAIL;
    }


    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_MapMutex);
    m_MsgWaitList.push_back(pMessage);
    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::asyncRequest end"));
    return SVS_ERROR_OK;
}

void CAccessControlStack::asyncResponse(SVS_ACM::RESPONSE& rResponse, void* pUserData)
{
    CControlMessage* pMessage = (CControlMessage* )pUserData;
    CAccessControlStack::instance().handle_asyncResponse(rResponse,pMessage);
}


void CAccessControlStack::handle_asyncResponse(SVS_ACM::RESPONSE& rResponse,CControlMessage* pMessage)
{
    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::handle_asyncResponse begin"));
    if(NULL == pMessage)
    {
        return;
    }
    bool bFind = false;
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_MapMutex);
    WAITMSGLISTITER iter = m_MsgWaitList.begin();
    for(;iter != m_MsgWaitList.end();++iter)
    {
        if(pMessage == (*iter))
        {
            bFind = true;
            m_MsgWaitList.erase(iter);
            break;
        }
    }

    if(!bFind)
    {
        SVS_LOG((SVS_LM_WARNING, "CAccessControlStack::handle_asyncResponse,not find message."));
        return;
    }


    int32_t nRet = pMessage->CreateMsg(rResponse);

    if(SVS_ERROR_OK != nRet)
    {
        SVS_DELETE(pMessage);
        SVS_LOG((SVS_LM_WARNING, "CAccessControlStack::handle_asyncResponse,create message fail."));
        return ;
    }
    nRet = pMessage->SendMsg();
    if(SVS_ERROR_OK != nRet)
    {
        SVS_DELETE(pMessage);
        SVS_LOG((SVS_LM_WARNING, "CAccessControlStack::handle_asyncResponse,send message fail."));
        return ;
    }
    SVS_DELETE(pMessage);
    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::handle_asyncResponse end"));
    return;
}

int32_t CAccessControlStack::handle_http_reponse(struct evhttp_request* req,std::string& strMsg)
{
    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::handle_http_reponse begin"));
    CControlMessage* pMessage = NULL;

    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::handle_http_reponse,the response msg:\n[%s].",strMsg.c_str()));

    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_MapMutex);
    WAITMSGLISTITER iter = m_MsgWaitList.begin();
    bool bFind = false;
    for(;iter != m_MsgWaitList.end();++iter)
    {
        pMessage = *iter;
        if(req == pMessage->GetHttpReq())
        {
            bFind = true;
            m_MsgWaitList.erase(iter);
            break;
        }
    }

    if(!bFind)
    {
        SVS_LOG((SVS_LM_WARNING, "CAccessControlStack::handle_http_reponse,not find the response msg."));
        return SVS_ERROR_FAIL;
    }


    int32_t nRet = pMessage->ParserMsg(strMsg);
    if(SVS_ERROR_OK != nRet)
    {
        SVS_DELETE(pMessage);
        SVS_LOG((SVS_LM_WARNING, "CAccessControlStack::handle_http_reponse,farser msg fail."));
        return SVS_ERROR_FAIL;
    }

    nRet = pMessage->DealMsg();
    SVS_DELETE(pMessage);
    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::handle_http_reponse end"));
    return nRet;
}

void  CAccessControlStack::http_callback(struct evhttp_request *req, void *arg)
{
    CAccessControlStack* pManage = (CAccessControlStack*)arg;
    pManage->handle_http_req(req);
}
void  CAccessControlStack::handle_http_req(struct evhttp_request *req)
{
    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::handle_http_req begin"));

    if (NULL == req)
    {
        return;
    }

    CControlMessage* pMessage = NULL;

    string uri_str = req->uri;
    string::size_type pos = uri_str.find_last_of(ACCESS_CONTROL_URI_LIVE_PLAY);
	//string::size_type dev_ctrl_pos = uri_str.find_last_of(ACCESS_CONTROL_URI_DEV_CTRL);


    if(ACCESS_CONTROL_URI_LIVE_PLAY == uri_str){//if(pos != string::npos) {
        CControlLivePlay* pReg = NULL;
        pMessage = SVS_NEW(pReg);
    }
    else if( ACCESS_CONTROL_URI_DEV_CTRL == uri_str)//else if(dev_ctrl_pos != string::npos)
    {
        CControlDevCtrl* pReg = NULL;
        pMessage = SVS_NEW(pReg);    
    }
    else {
         SVS_LOG((SVS_LM_WARNING, "CAccessControlStack::handle_http_reponse,not find uri:[%s] deal msg.",uri_str.c_str()));
         evhttp_send_error(req, 404, "service was not found!");
         return;
    }
    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::handle_http_req request path[%s].",
                                                                       uri_str.c_str()));

    evbuffer *pbuffer = req->input_buffer;
    string post_str;
    int n = 0;
    char  szBuf[HTTP_REQUEST_MAX + 1] = { 0 };
    while ((n = evbuffer_remove(pbuffer, &szBuf, HTTP_REQUEST_MAX - 1)) > 0)
    {
        szBuf[n] = '\0';
        post_str.append(szBuf, n);
    }

    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::handle_http_req, msg[%s]",
                                                                       post_str.c_str()));
    pMessage->SetHttpBase(m_httpBase,m_pDnsbase);

    int32_t nRet = pMessage->ParserMsg(req,post_str);
    if(SVS_ERROR_OK != nRet)
    {
        SVS_DELETE(pMessage);
        return;
    }
    ACE_Guard<ACE_Recursive_Thread_Mutex> locker(m_MapMutex);
    m_MsgWaitList.push_back(pMessage);
    pMessage->DealMsg();
    SVS_LOG((SVS_LM_DEBUG, "CAccessControlStack::handle_http_req end"));
}


int32_t CAccessControlStack::send_device_info(SVS_ACM::REQUEST_NOTIFY_DEV_INFO& rNotify, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{

    /*
    uint32_t index = 0;
    int32_t nRet = pOnline->CreateMsg(pMsg,false);
    if(SVS_ERROR_OK != nRet)
    {
        SVS_DELETE(pOnline);
        return SVS_ERROR_FAIL;
    }
    nRet = pOnline->SendMsg(index);
    if(SVS_ERROR_OK == nRet)
    {
        SVS_DELETE(pOnline);
        return SVS_ERROR_FAIL;
    }*/
    return 0;
}






