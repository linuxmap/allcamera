#include "svs_control_msg_dev_register.h"
#include "vms/vms.h"
#include "svs_log_msg.h"
#include "svs_utility.h"
#include "svs_control_msg_define.h"
#include "svs_access_control.h"

CControlDevRegMsg::CControlDevRegMsg()
{
}
CControlDevRegMsg::~CControlDevRegMsg()
{
}

int32_t CControlDevRegMsg::DealMsg()
{

    SVS_ACM::RESPONSE_NOTIFY_DEV_INFO response;
    response.nRequestID = m_nRequestID;

    response.nResponseCode = SVS_ERROR_OK;

    if(NULL != m_cb)
    {
        m_cb(response,m_UserData);
    }
    m_enStatus = CONTROL_MGS_HTTP_STATUS_RELEASE;
    return SVS_ERROR_OK;
}
int32_t CControlDevRegMsg::SendMsg()
{
    std::string uri = HTTP_URL_PREFIX + ACCESS_CONTROL_URI_DEV_REG;
    SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::SendMsg ,msg:[%s].",m_strMsg.c_str()));
    return CControlMessage::send_http_request(uri,m_strMsg);
}

int32_t CControlDevRegMsg::ParserMsgBody(cJSON* body)
{
    //NO BODY
    return SVS_ERROR_OK;
}

int32_t CControlDevRegMsg::CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body)
{
    SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,begin."));

    cJSON* devInfo = cJSON_CreateObject();
    if(NULL == devInfo)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create devInfo object fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(body,"devInfo",devInfo);
    SVS_ACM::REQUEST_NOTIFY_DEV_INFO& rNotify = (SVS_ACM::REQUEST_NOTIFY_DEV_INFO&)rRequest;
    const SVS_ACM::DEVICE_INFO& stDeviceInfo = rNotify.stDeviceInfo;

    cJSON* protocol = cJSON_CreateNumber((double)stDeviceInfo.eDeviceType);
    if(NULL == protocol)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create devInfo object fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(devInfo,"protocol",protocol);
    cJSON* status = cJSON_CreateNumber((double)stDeviceInfo.eDeviceStatus);
    if(NULL == status)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create devInfo object fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(devInfo,"status",status);
    cJSON* facName = cJSON_CreateString(szDevType[stDeviceInfo.eDeviceType]);
    if(NULL == facName)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create facName object fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(devInfo,"facName",facName);
    cJSON* ip = cJSON_CreateString(stDeviceInfo.szHost);
    if(NULL == ip)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create ip object fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(devInfo,"ip",ip);

    cJSON* port = cJSON_CreateNumber((double)atoi(stDeviceInfo.szPort));
    if(NULL == port)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create port object fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(devInfo,"port",port);

    cJSON* devId = cJSON_CreateString(stDeviceInfo.szDeviceID);
    if(NULL == devId)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create devId object fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(devInfo,"devId",devId);

    const char* pAccessIP    = CAccessControlSvr::instance().getHttpStackFWAddr();
    uint32_t    ulAccessPort = CAccessControlSvr::instance().getHttpStackFwPort();

    cJSON* accessIp = cJSON_CreateString(pAccessIP);
    if(NULL == accessIp)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create accessIp object fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(devInfo,"accessIp",accessIp);

    cJSON* accessPort = cJSON_CreateNumber((double)ulAccessPort);
    if(NULL == accessPort)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create accessPort object fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(devInfo,"accessPort",accessPort);

    cJSON* LensInfo;
    cJSON* CameraID;
    cJSON* CameraStatus;
    cJSON* CameraFac;
    cJSON* LensList = cJSON_CreateArray();
    if(NULL == LensList)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create LensList object fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(devInfo,"cameraList",LensList);

    for (size_t i = 0; i < stDeviceInfo.vecLensInfo.size(); i++)
    {
        const SVS_ACM::LENS_INFO& rLensInfo = stDeviceInfo.vecLensInfo[i];
        LensInfo = cJSON_CreateObject();
        if(NULL == LensInfo)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create LensInfo object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToArray(LensList,LensInfo);

        CameraID = cJSON_CreateString(rLensInfo.szLensID);
        if(NULL == CameraID)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create CameraID object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToObject(LensInfo,"cameraId",CameraID);

        CameraStatus = cJSON_CreateNumber((double)rLensInfo.eLensStatus);
        if(NULL == CameraStatus)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create CameraStatus object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToObject(LensInfo,"status",CameraStatus);

        CameraFac = cJSON_CreateString(rLensInfo.szManufacturer);
        if(NULL == CameraFac)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,create CameraFac object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToObject(LensInfo,"facName",CameraFac);


    }
    SVS_LOG((SVS_LM_DEBUG, "CControlDevRegMsg::CreateMsgBody,end."));
    return SVS_ERROR_OK;
}

int32_t CControlDevRegMsg::CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body)
{
    return SVS_ERROR_OK;
}


CControlDevReportMsg::CControlDevReportMsg()
{
}
CControlDevReportMsg::~CControlDevReportMsg()
{
}

int32_t CControlDevReportMsg::DealMsg()
{

    SVS_ACM::RESPONSE_NOTIFY_DEV_INFO response;
    response.nRequestID = m_nRequestID;

    response.nResponseCode = SVS_ERROR_OK;

    if(NULL != m_cb)
    {
        m_cb(response,m_UserData);
    }
    m_enStatus = CONTROL_MGS_HTTP_STATUS_RELEASE;
    return SVS_ERROR_OK;
}
int32_t CControlDevReportMsg::SendMsg()
{
    std::string uri = HTTP_URL_PREFIX + ACCESS_CONTROL_URI_DEV_REPORT;
    SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::SendMsg ,msg:[%s].",m_strMsg.c_str()));
    return CControlMessage::send_http_request(uri,m_strMsg);
}

int32_t CControlDevReportMsg::ParserMsgBody(cJSON* body)
{
    //NO BODY
    return SVS_ERROR_OK;
}

int32_t CControlDevReportMsg::CreateMsgBody(SVS_ACM::REQUEST& rRequest,cJSON* body)
{
    SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,begin."));

    cJSON* devInfoList = cJSON_CreateArray();
    if(NULL == devInfoList)
    {
        SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create devInfo list fail."));
        return SVS_ERROR_FAIL;
    }
    cJSON_AddItemToObject(body,"devInfoList",devInfoList);

    SVS_ACM::REQUEST_NOTIFY_ALL_DEV& rNotify = (SVS_ACM::REQUEST_NOTIFY_ALL_DEV&)rRequest;
    std::list<SVS_ACM::DEVICE_INFO>::iterator iter = rNotify.ListDeviceInfo.begin();
    for(;iter != rNotify.ListDeviceInfo.end();++iter)
    {

        const SVS_ACM::DEVICE_INFO& stDeviceInfo = *iter;

        cJSON* devInfo = cJSON_CreateObject();
        if(NULL == devInfo)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create devInfo list fail."));
            return SVS_ERROR_FAIL;
        }

        cJSON_AddItemToArray(devInfoList,devInfo);

        cJSON* protocol = cJSON_CreateNumber((double)stDeviceInfo.eDeviceType);
        if(NULL == protocol)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create devInfo object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToObject(devInfo,"protocol",protocol);
        cJSON* status = cJSON_CreateNumber((double)stDeviceInfo.eDeviceStatus);
        if(NULL == status)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create devInfo object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToObject(devInfo,"status",status);
        cJSON* facName = cJSON_CreateString("GB28181 Dev");
        if(NULL == facName)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create facName object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToObject(devInfo,"facName",facName);
        cJSON* ip = cJSON_CreateString(stDeviceInfo.szHost);
        if(NULL == ip)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create ip object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToObject(devInfo,"ip",ip);

        cJSON* port = cJSON_CreateNumber((double)atoi(stDeviceInfo.szPort));
        if(NULL == port)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create port object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToObject(devInfo,"port",port);
        cJSON* devId = cJSON_CreateString(stDeviceInfo.szDeviceID);
        if(NULL == devId)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create devId object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToObject(devInfo,"devId",devId);

        cJSON* LensInfo;
        cJSON* CameraID;
        cJSON* CameraStatus;
        cJSON* CameraFac;
        cJSON* LensList = cJSON_CreateArray();
        if(NULL == LensList)
        {
            SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create LensList object fail."));
            return SVS_ERROR_FAIL;
        }
        cJSON_AddItemToObject(devInfo,"cameraList",LensList);

        for (size_t i = 0; i < stDeviceInfo.vecLensInfo.size(); i++)
        {
            const SVS_ACM::LENS_INFO& rLensInfo = stDeviceInfo.vecLensInfo[i];
            LensInfo = cJSON_CreateObject();
            if(NULL == LensInfo)
            {
                SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create LensInfo object fail."));
                return SVS_ERROR_FAIL;
            }
            cJSON_AddItemToArray(LensList,LensInfo);

            CameraID = cJSON_CreateString(rLensInfo.szLensID);
            if(NULL == CameraID)
            {
                SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create CameraID object fail."));
                return SVS_ERROR_FAIL;
            }
            cJSON_AddItemToObject(LensInfo,"cameraId",CameraID);

            CameraStatus = cJSON_CreateNumber((double)rLensInfo.eLensStatus);
            if(NULL == CameraStatus)
            {
                SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create CameraStatus object fail."));
                return SVS_ERROR_FAIL;
            }
            cJSON_AddItemToObject(LensInfo,"status",CameraStatus);

            CameraFac = cJSON_CreateString(rLensInfo.szManufacturer);
            if(NULL == CameraFac)
            {
                SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,create CameraFac object fail."));
                return SVS_ERROR_FAIL;
            }
            cJSON_AddItemToObject(LensInfo,"facName",CameraFac);


        }
    }
    SVS_LOG((SVS_LM_DEBUG, "CControlDevReportMsg::CreateMsgBody,end."));
    return SVS_ERROR_OK;
}

int32_t CControlDevReportMsg::CreateMsgBody(SVS_ACM::RESPONSE& rResponse,cJSON* body)
{
    return SVS_ERROR_OK;
}
