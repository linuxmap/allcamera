#include "svs_control_msg.h"
#include "vms/vms.h"
#include "svs_utility.h"
#include "svs_log_msg.h"
#include "svs_control_stack.h"
#include "svs_access_control.h"

CControlMessage::CControlMessage()
{
    m_lReusltCode   = SVS_ERROR_OK;
    m_strResultDesc = "";
    m_req           = NULL;
    m_pBase         = NULL;
    m_pConn         = NULL;
    m_pDnsbase      = NULL;
    m_enStatus      = CONTROL_MGS_HTTP_STATUS_INIT;
    m_ReqTime       = time(NULL);
}
CControlMessage::~CControlMessage()
{
    if(m_req)
    {
        evhttp_request_free(m_req);
        m_req = NULL;
    }

}

int32_t CControlMessage::CreateMsg(SVS_ACM::REQUEST& rRequest)
{
    m_nRequestID = rRequest.nRequestID;

    cJSON* root = cJSON_CreateObject();
    if(NULL == root)
    {
        SVS_LOG((SVS_LM_WARNING, "CControlMessage::CreateMsg ,create the json fail."));
        return SVS_ERROR_FAIL;
    }

    if(SVS_ERROR_OK != CreateMsgBody(rRequest,root))
    {
        SVS_LOG((SVS_LM_WARNING, "CControlMessage::CreateMsg ,create the body fail."));
        return SVS_ERROR_FAIL;
    }
    char* pszMsg = cJSON_Print(root);
    if(NULL == pszMsg)
    {
        SVS_LOG((SVS_LM_WARNING, "CControlMessage::CreateMsg ,print the json fail."));
        return SVS_ERROR_FAIL;
    }
    m_strMsg = pszMsg;
    free(pszMsg);
    cJSON_Delete(root);
    m_enStatus      = CONTROL_MGS_HTTP_STATUS_WAIT_REQ;
    SVS_LOG((SVS_LM_DEBUG, "CControlMessage::CreateMsg ,msg:[%s].",m_strMsg.c_str()));
    return SVS_ERROR_OK;
}
int32_t CControlMessage::CreateMsg(SVS_ACM::RESPONSE& rResponse)
{

    cJSON* root = cJSON_CreateObject();
    if(NULL == root)
    {
        SVS_LOG((SVS_LM_WARNING, "CControlMessage::CreateMsg ,create the json fail."));
        return SVS_ERROR_FAIL;
    }

    cJSON* resultCode = cJSON_CreateNumber(rResponse.nResponseCode);

    if(NULL == resultCode)
    {
        SVS_LOG((SVS_LM_WARNING, "CControlMessage::CreateMsg ,create the json resultCode fail."));
        cJSON_Delete(root);
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(root,"resultCode",resultCode);

    cJSON* resultDesc = NULL;
    if(SVS_ERROR_OK != rResponse.nResponseCode)
    {
        resultDesc = cJSON_CreateString("fail");
    }
    else
    {
        resultDesc = cJSON_CreateString("success");
    }

    if(NULL == resultDesc)
    {
        SVS_LOG((SVS_LM_WARNING, "CControlMessage::CreateMsg ,create the json resultDesc fail."));
        cJSON_Delete(root);
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(root,"resultDesc",resultDesc);

    if(SVS_ERROR_OK != CreateMsgBody(rResponse,root))
    {
        SVS_LOG((SVS_LM_WARNING, "CControlMessage::CreateMsg ,create the body fail."));
        cJSON_Delete(root);
        return SVS_ERROR_FAIL;
    }
    char* pszMsg = cJSON_Print(root);
    if(NULL == pszMsg)
    {
        SVS_LOG((SVS_LM_WARNING, "CControlMessage::CreateMsg ,print the json fail."));
        cJSON_Delete(root);
        return SVS_ERROR_FAIL;
    }
    m_strMsg = pszMsg;
    free(pszMsg);
    cJSON_Delete(root);
    m_enStatus      = CONTROL_MGS_HTTP_STATUS_WAIT_RESP;
    return SVS_ERROR_OK;
}
int32_t CControlMessage::ParserMsg(struct evhttp_request *req,std::string &strMsg)
{
    SVS_LOG((SVS_LM_INFO, "CControlMessage::ParserMsg,handle message:\n %s.",strMsg.c_str()));
    m_req = req;
    return ParserMsg(strMsg);
}


int32_t CControlMessage::ParserMsg(std::string &strMsg)
{
    SVS_LOG((SVS_LM_INFO, "CControlMessage::ParserMsg,handle message:\n %s.",strMsg.c_str()));

    cJSON*  jsonMsg = cJSON_Parse(strMsg.c_str());
    if (NULL == jsonMsg)
    {
        SVS_LOG((SVS_LM_WARNING, "CControlMessage::ParserMsg MsgHead Parse json failed,msg:\n %s.",strMsg.c_str()));
        return SVS_ERROR_FAIL;
    }
    cJSON* jresultCode    = cJSON_GetObjectItem(jsonMsg,"resultCode");
    cJSON* jresultDesc    = cJSON_GetObjectItem(jsonMsg,"resultDesc");

    if(NULL != jresultCode)
    {
        m_lReusltCode = jresultCode->valueint;
    }
    if(NULL != jresultDesc &&  NULL != jresultDesc->valuestring)
    {
        m_strResultDesc = jresultDesc->valuestring;
    }
    int32_t nRet = ParserMsgBody(jsonMsg);
    cJSON_Delete(jsonMsg);

    return nRet;
}
bool CControlMessage::checkTimeout()
{
    if(CONTROL_MGS_HTTP_STATUS_WAIT_RESP != m_enStatus) {
        return false;
    }
    return true;
}

void CControlMessage::SetResponseCallBack(SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    m_cb       = pCallBack;
    m_UserData = pUserData;
}
void CControlMessage::SetHttpBase(struct event_base *pBase,struct evdns_base *pDnsbase )
{
    m_pBase         = pBase;
    m_pDnsbase      = pDnsbase;
}


void CControlMessage::handle_remote_read(struct evhttp_request* remote_rsp)
{
    if (NULL == remote_rsp){
        //event_base_loopexit(m_pBase, NULL);
        //event_base_loopbreak(m_pBase);
        return;
    }
    m_strMsg = "";

    size_t len = evbuffer_get_length(remote_rsp->input_buffer);
    const char * str = (const char*)evbuffer_pullup(remote_rsp->input_buffer, len);
    if ((0 == len) || (NULL == str)) {
        SVS_LOG((SVS_LM_WARNING,"CControlMessage::handle_remote_read fail."));
        return;
    }
    m_strMsg.append(str, 0, len);
    SVS_LOG((SVS_LM_INFO,"CControlMessage::handle_remote_read,msg:[%s] .",m_strMsg.c_str()));
    CAccessControlStack::instance().handle_http_reponse(remote_rsp, m_strMsg);
}

void CControlMessage::handle_readchunk(struct evhttp_request* remote_rsp)
{
    if (NULL == remote_rsp){
        return;
    }
    m_strMsg = "";

    char buf[HTTP_REQUEST_MAX];
    struct evbuffer* evbuf = evhttp_request_get_input_buffer(remote_rsp);
    int len = 0;
    while ((len = evbuffer_remove(evbuf, buf, HTTP_REQUEST_MAX)) > 0)
    {
        m_strMsg.append(buf, len);
    }

    SVS_LOG((SVS_LM_INFO,"CControlMessage::handle_readchunk,msg:[%s] .",m_strMsg.c_str()));
    CAccessControlStack::instance().handle_http_reponse(remote_rsp, m_strMsg);
}

void CControlMessage::handle_remote_connection_close(struct evhttp_connection* connection)
{
    SVS_LOG((SVS_LM_INFO,"CControlMessage::handle_remote_connection_close."));
    //event_base_loopexit(m_pBase, NULL);
}

void CControlMessage::remote_read_cb(struct evhttp_request* remote_rsp, void* arg)
{
    CControlMessage* client = (CControlMessage*)arg;
    client->handle_remote_read(remote_rsp);
    return;
}

void CControlMessage::readchunk_cb(struct evhttp_request* remote_rsp, void* arg)
{
    CControlMessage* client = (CControlMessage*)arg;
    client->handle_readchunk(remote_rsp);
    return;
}

void CControlMessage::remote_connection_close_cb(struct evhttp_connection* connection, void* arg)
{
    CControlMessage* client = (CControlMessage*)arg;
    client->handle_remote_connection_close(connection);
    return;
}


int32_t CControlMessage::send_http_request(std::string& strUri,std::string& strMsg)
{
    SVS_LOG((SVS_LM_INFO,"CControlMessage::send_http_request begin."));


    uint32_t port = CAccessControlSvr::instance().getControlServerPort();

    const char *host = CAccessControlSvr::instance().getControlServerAddr();

    char lenbuf[HTTP_TMP_BUF_SIZE] = { 0 };

    do {

        m_req = evhttp_request_new(remote_read_cb, this);
        evhttp_add_header(m_req->output_headers, "Content-Type", "application/json");
        snprintf(lenbuf, HTTP_TMP_BUF_SIZE, "%lu", strMsg.length());
        evhttp_add_header(m_req->output_headers, "Content-length", lenbuf); //content length
        snprintf(lenbuf, HTTP_TMP_BUF_SIZE, "%s:%d", host,port);
        evhttp_add_header(m_req->output_headers, "Host", lenbuf);
        evhttp_add_header(m_req->output_headers, "Connection", "close");
        evhttp_request_set_chunked_cb(m_req, readchunk_cb);


        m_pConn = evhttp_connection_base_new(m_pBase,m_pDnsbase, host, port);
        if (!m_pConn)
        {
            break;
        }
        evhttp_connection_set_closecb(m_pConn, remote_connection_close_cb, this);


        m_req->output_buffer = evbuffer_new();
        if (NULL == m_req->output_buffer)
        {
            break;
        }


        evbuffer_add(m_req->output_buffer,strMsg.c_str(),strMsg.length());
        m_req->flags = EVHTTP_USER_OWNED;

        if(SVS_ERROR_OK != evhttp_make_request(m_pConn, m_req, EVHTTP_REQ_POST, strUri.c_str()))
        {
            SVS_LOG((SVS_LM_WARNING,"CControlMessage::send_http_request,make the http request fail."));
            break;
        }
        evhttp_connection_set_timeout(m_req->evcon, 600);


        SVS_LOG((SVS_LM_DEBUG,"CControlMessage::send_http_request end."));

        m_enStatus      = CONTROL_MGS_HTTP_STATUS_WAIT_RESP;
        m_ReqTime       = time(NULL);

        return SVS_ERROR_OK;
    }while(true);

    //evhttp_request_free(m_req);
    //m_req = NULL;

	
    evhttp_connection_free(m_pConn);
    m_pConn = NULL;
    SVS_LOG((SVS_LM_WARNING,"CControlMessage::send_http_request break."));
    return SVS_ERROR_FAIL;
}

int32_t CControlMessage::send_http_response(std::string& strMsg)
{
    SVS_LOG((SVS_LM_DEBUG,"CControlMessage::send_http_response,begin, message:[%s]",
                                                  m_strMsg.c_str()));
    struct evbuffer* evbuf = evbuffer_new();
    if (NULL == evbuf)
    {
        return SVS_ERROR_FAIL;
    }
    evbuffer_add_printf(evbuf, "%s", strMsg.c_str());

    evhttp_add_header(m_req->output_headers, "Content-Type", "application/json");

    evhttp_send_reply(m_req, HTTP_OK, "OK", evbuf);
    evbuffer_free(evbuf);
    evhttp_send_reply_end(m_req);
    m_req= NULL;
    m_enStatus      = CONTROL_MGS_HTTP_STATUS_RELEASE;
    SVS_LOG((SVS_LM_DEBUG,"CControlMessage::send_http_response,end."));
    return SVS_ERROR_OK;
}


