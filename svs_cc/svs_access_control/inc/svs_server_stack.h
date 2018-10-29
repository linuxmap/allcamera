#ifndef _SVS_SERVCER_STACK_H_
#define _SVS_SERVCER_STACK_H_

#include "vms/vms.h"
#include "svs_acm_async.h"
#include "svs_log_msg.h"

class IServerStack : public SVS_ACM::IAsync
{
public:
    static IServerStack& instance()
    {
        static IServerStack objServerStack;
        return objServerStack;
    }

    int32_t initialize()
    {
        SVS_TRACE();
        return SVS_ERROR_OK;
    }

    int32_t release()
    {
        SVS_TRACE();
        return SVS_ERROR_OK;
    }

    int32_t start()
    {
        SVS_TRACE();
        return SVS_ERROR_OK;
    }

    int32_t stop()
    {
        SVS_TRACE();
        return SVS_ERROR_OK;
    }


    /* 异步通知接口 */
    virtual int32_t asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack = NULL, void* pUserData = NULL);

    /* 异步回调接口 */
    static void asyncResponse(SVS_ACM::RESPONSE& rResponse, void* pUserData);

private:
    IServerStack(){};
    virtual ~IServerStack(){};
};

#endif
