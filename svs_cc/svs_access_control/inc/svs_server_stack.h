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


    /* �첽֪ͨ�ӿ� */
    virtual int32_t asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack = NULL, void* pUserData = NULL);

    /* �첽�ص��ӿ� */
    static void asyncResponse(SVS_ACM::RESPONSE& rResponse, void* pUserData);

    // void handle_asyncResponse(SVS_ACM::RESPONSE& rResponse,CControlMessage* pMessage);

    int32_t getRtspUrl(std::string& strLensID, std::string& strRtspUrl);

private:
    IServerStack(){};
    virtual ~IServerStack(){};
};

#endif
