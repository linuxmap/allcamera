#ifndef _SVS_ACM_RESPONSE_H_
#define _SVS_ACM_RESPONSE_H_

#include "vms/vms.h"

namespace SVS_ACM
{

enum RESP_TYPE
{
    RESP_TYPE_BASE                = 1, //通用响应，直接用SVS_ACM_RESPONSE
    RESP_TYPE_NOTIFY_DEV_INFO     = 2,
    RESP_TYPE_PLAY_URL            = 3,
    RESP_TYPE_HTTP_REPORT         = 4,
    RESP_TYPE_SEND_INVITE2DEV     = 5,
    RESP_TYPE_SEND_KEYFRAME2DEV     = 6,
    RESP_TYPE_SEND_BYE2DEV        = 7,
    RESP_TYPE_MAX
};

class RESPONSE
{
public:
    int32_t nRequestID;
    int32_t nResponseCode;

public:
    RESPONSE()
    {
        eResponseType = RESP_TYPE_BASE;
        nResponseCode = SVS_ERROR_OK;
    }

    RESP_TYPE getType() const
    {
        return eResponseType;
    }

protected:
    RESP_TYPE   eResponseType;
};

class RESPONSE_NOTIFY_DEV_INFO : public RESPONSE
{
public:
    RESPONSE_NOTIFY_DEV_INFO()
    {
        eResponseType = RESP_TYPE_NOTIFY_DEV_INFO;
    }
};

class RESPONSE_PLAY_URL_INFO : public RESPONSE
{
public:
    char     szUrl[MEDIA_URL_LEN + 1];
public:
    RESPONSE_PLAY_URL_INFO()
    {
        eResponseType = RESP_TYPE_NOTIFY_DEV_INFO;
        memset(szUrl,0,MEDIA_URL_LEN + 1);
    }
};

class RESPONSE_HTTP_REPORT : public RESPONSE
{
public:
    char szServerID[SERVER_ID_LEN + 1];

public:
    RESPONSE_HTTP_REPORT()
    {
        eResponseType = RESP_TYPE_HTTP_REPORT;
        memset(szServerID, 0 , sizeof(szServerID));
    }
};

class RESPONSE_SEND_INVITE2DEV : public RESPONSE
{
public:
    char        szLensID[SVS_DEVICEID_LEN + 1];
    uint32_t    SdpLen;
    uint8_t     szSdp[SDP_MSG_LENS + 1];
    uint32_t    lSessionID; // FOR Ehome Dev
public:
    RESPONSE_SEND_INVITE2DEV()
    {
        eResponseType = RESP_TYPE_SEND_INVITE2DEV;
        memset(szLensID,0,SVS_DEVICEID_LEN + 1);
        SdpLen           = 0;
        memset(szSdp,0,SDP_MSG_LENS + 1);
        lSessionID = 0;
    }
};





class RESPONSE_SEND_BYE2DEV : public RESPONSE
{
public:
    char            szLensID[SVS_DEVICEID_LEN + 1];
public:
    RESPONSE_SEND_BYE2DEV()
    {
        eResponseType = RESP_TYPE_SEND_BYE2DEV;
        memset(szLensID,0,SVS_DEVICEID_LEN + 1);
    }
};

}   //namespace SVS_ACM

#endif
