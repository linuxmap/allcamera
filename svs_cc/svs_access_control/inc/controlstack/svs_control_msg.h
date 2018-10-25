#ifndef __SVS_CONTROL_MESSAGE_H__
#define __SVS_CONTROL_MESSAGE_H__
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "svs_control_msg_define.h"
#include "svs_acm_async.h"
extern "C"{
#include "event2/http.h"
#include "event.h"
#include "event2/buffer.h"
#include "event2/keyvalq_struct.h"
#include "evhttp.h"
#include "event2/dns.h"
#include "event2/thread.h"
#include "svs_json.h"
}

#define   HTTP_TMP_BUF_SIZE   256

#define   MSG_RESULT_SUCCESS "0"

const std::string HTTP_URL_PREFIX = "/scc/v1";
//const std::string HTTP_URL_PREFIX = "/scc";
typedef enum CONTROL_MGS_HTTP_STATUS
{
    CONTROL_MGS_HTTP_STATUS_INIT      = 0,
    CONTROL_MGS_HTTP_STATUS_WAIT_REQ  = 1,
    CONTROL_MGS_HTTP_STATUS_WAIT_RESP = 2,
    CONTROL_MGS_HTTP_STATUS_RELEASE   = 3,
}CONTROL_MGS_STATUS;

class CControlMessage
{
public:
    CControlMessage();
    virtual ~CControlMessage();

    int32_t ParserMsg(struct evhttp_request *req,std::string &strMsg);

    int32_t ParserMsg(std::string &strMsg);

    int32_t CreateMsg(SVS_ACM::REQUEST& rRequest);

    int32_t CreateMsg(SVS_ACM::RESPONSE& rResponse);

    struct evhttp_request * GetHttpReq(){return m_req;};

    void SetResponseCallBack(SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData);

    void SetHttpBase(struct event_base *pBase,struct evdns_base *pDnsbase );

    CONTROL_MGS_STATUS getStatus(){return m_enStatus;};

    bool checkTimeout();

    virtual int32_t DealMsg() = 0;

    virtual int32_t SendMsg() = 0;
public:
    void handle_remote_read(struct evhttp_request* remote_rsp);
    void handle_readchunk(struct evhttp_request* remote_rsp);
    void handle_remote_connection_close(struct evhttp_connection* connection);
private:
    static void remote_read_cb(struct evhttp_request* remote_rsp, void* arg);
    static void readchunk_cb(struct evhttp_request* remote_rsp, void* arg);
    static void remote_connection_close_cb(struct evhttp_connection* connection, void* arg);
protected:
    int32_t send_http_request(std::string& strUri,std::string& strMsg);
    int32_t send_http_response(std::string& strMsg);
protected:
    virtual int32_t ParserMsgBody(cJSON* body) = 0;

    virtual int32_t CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body) = 0;

    virtual int32_t CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body) = 0;
protected:
    std::string                m_strMsg;
    int32_t                    m_nRequestID;
    struct evhttp_request     *m_req;
    struct event_base         *m_pBase;
    struct evhttp_connection  *m_pConn;
    struct evdns_base         *m_pDnsbase;
    int32_t                    m_lReusltCode;
    std::string                m_strResultDesc;
    SVS_ACM::RESPONSE_CALLBACK m_cb;
    void*                      m_UserData;
    CONTROL_MGS_STATUS         m_enStatus;
    time_t                     m_ReqTime;
};

#endif /* __SVS_CONTROL_MESSAGE_H__ */