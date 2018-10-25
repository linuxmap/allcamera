#ifndef _SVS_ACM_ASYNC_H_
#define _SVS_ACM_ASYNC_H_

#include "svs_log_msg.h"
#include "vms/vms.h"
#include "svs_acm_request.h"
#include "svs_acm_response.h"

namespace SVS_ACM
{

typedef void (*RESPONSE_CALLBACK)(RESPONSE& rResponse, void* pUserData);

class IAsync
{
public:
    IAsync(){};
    virtual ~IAsync(){};

    //�첽���������ûص�
    //�첽֪ͨ���������ûص�
    virtual int32_t asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack = NULL, void* pUserData = NULL)
    {
        SVS_TRACE();
        return SVS_ERROR_OK;
    }
};

}
#endif
