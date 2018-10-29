#ifndef _SVS_ACM_REQUEST_H_
#define _SVS_ACM_REQUEST_H_

#include "vms/vms.h"
#include "svs_acm_struct.h"
#include "svs_ac_common.h"
#include <list>

namespace SVS_ACM
{

enum REQ_TYPE
{
    REQ_TYPE_NOTIFY_DEV_INFO,
    REQ_TYPE_NOTIFY_ALL_DEV,
    REQ_TYPE_NOTIFY_DEV_ALARM,
    REQ_TYPE_PLAY_URL_REQ,
    REQ_TYPE_HTTP_REPORT,
    REQ_TYPE_SEND_INVITE2DEV,
    REQ_TYPE_SEND_ACK2DEV,
    REQ_TYPE_SEND_KEYFRAME2DEV,
    REQ_TYPE_SEND_BYE2DEV,
    REQ_TYPE_DEV_CTRL,
    REQ_TYPE_MAX
};

class REQUEST
{
public:
    int32_t nRequestID;

public:
    REQUEST()
    {
        eRequestType = REQ_TYPE_MAX;
    }

    REQ_TYPE getType() const
    {
        return eRequestType;
    }

protected:
    REQ_TYPE   eRequestType;
};

//发往设备的请求
class REQUEST_TO_DEV : public REQUEST
{
public:
    char            szLensID[SVS_DEVICEID_LEN + 1];
    char            szDeviceID[SVS_DEVICEID_LEN + 1];   //由access_control_manager设置
    SVS_DEV_TYPE    eDevType;

    REQUEST_TO_DEV()
    {
        memset(szLensID, 0, sizeof(szLensID));
        memset(szDeviceID, 0, sizeof(szDeviceID));
    }
};

class REQUEST_NOTIFY_DEV_INFO : public REQUEST
{
public:
    DEVICE_INFO stDeviceInfo;

public:
    REQUEST_NOTIFY_DEV_INFO()
    {
        eRequestType = REQ_TYPE_NOTIFY_DEV_INFO;
    }
};

class REQUEST_NOTIFY_DEV_ALARM : public REQUEST
{
public:
    DEV_ALARM_INFO stDevAlarmInfo;

public:
    REQUEST_NOTIFY_DEV_ALARM()
    {
        eRequestType = REQ_TYPE_NOTIFY_DEV_ALARM;
    }
};

class REQUEST_NOTIFY_ALL_DEV : public REQUEST
{
public:
    std::list<SVS_ACM::DEVICE_INFO> ListDeviceInfo;

public:
    REQUEST_NOTIFY_ALL_DEV()
    {
        eRequestType = REQ_TYPE_NOTIFY_ALL_DEV;
    }
};


//直接使用通用响应：RESP_TYPE_BASE
class REQUEST_DEV_CTRL : public REQUEST_TO_DEV
{
public:
    DEV_CTRL_TYPE   eCtrlType;
    uint8_t         nCtrlParam1;
    uint8_t         nCtrlParam2;
    uint16_t        nCtrlParam3;
    uint32_t        nPriority;

public:
    REQUEST_DEV_CTRL()
    {
        eRequestType    = REQ_TYPE_DEV_CTRL;
        eCtrlType       = DEV_CTRL_TYPE_MAX;
        nCtrlParam1     = 0;
        nCtrlParam2     = 0;
        nCtrlParam3     = 0;
        nPriority       = 0;
    }
};

class REQUEST_PLAY_URL_INFO : public REQUEST
{
public:
    PLAY_TYPE        ePlayType;
    PLAY_URL_TYPE    ePlayUrlType;
    DEV_STREAM_TYPE  enStreamType;
    SVS_DEV_TYPE     eDevType;
    char             szLensID[SVS_DEVICEID_LEN + 1];

public:
    REQUEST_PLAY_URL_INFO()
    {
        eRequestType = REQ_TYPE_PLAY_URL_REQ;
        ePlayType    = PLAY_TYPE_MAX;
        ePlayUrlType = PLAY_URL_TYPE_MAX;
        enStreamType = DEV_STREAM_TYPE_MAIN;
        eDevType = SVS_DEV_TYPE_MAX;
        memset(szLensID,0,SVS_DEVICEID_LEN + 1);
    }
};


/*
class REQUEST_HTTP_REPORT : public REQUEST
{
public:
    HTTP_REPORT_TYPE      enHttpReportType;

    char                  szLensID[SVS_DEVICEID_LEN + 1];
    SETUP_TYPE            eCallType;
    PLAY_URL_TYPE         eUrlType;
    PLAY_TYPE             ePlayType;
    uint32_t              UrlLen;
    uint8_t               szUrl[RTSP_URL_LEN];
    uint32_t              SdpLen;
    uint8_t               szSdp[SDP_MSG_LENS];

public:
    REQUEST_HTTP_REPORT()
    {
        eRequestType     = REQ_TYPE_HTTP_REPORT;
        enHttpReportType = HTTP_REPORT_TYPE_MAX;

        eCallType        = SETUP_TYPE_MAX;
        ePlayType        = PLAY_TYPE_MAX;
        eUrlType         = PLAY_URL_TYPE_MAX;
        UrlLen           = 0;
        memset(szUrl,0,RTSP_URL_LEN);
        SdpLen           = 0;
        memset(szSdp,0,SDP_MSG_LENS);

        memset(szLensID,0,SVS_DEVICEID_LEN + 1);
    }
};
*/

class REQUEST_HTTP_REPORT : public REQUEST
{
public:
    char                                                          szServerID[SERVER_ID_LEN + 1];
    uint32_t                                                     BusinessTotalNum;
    std::list<SVS_SBUSINESS_REPORT_INFO>   BusinessInfoList;

public:
    REQUEST_HTTP_REPORT()
    {
        eRequestType     = REQ_TYPE_HTTP_REPORT;

        ACE_OS::memset(szServerID, 0, sizeof(szServerID));

        BusinessTotalNum = 0;
    }
};


class REQUEST_SEND_INVITE2DEV : public REQUEST_TO_DEV
{
public:
    char            szMuSvrID[SERVER_ID_LEN + 1];   // mu server id
    SETUP_TYPE      eCallType;
    PLAY_URL_TYPE   eUrlType;
    PLAY_TYPE       ePlayType;
    DEV_STREAM_TYPE eStreamtype;//0- 主码流，1- 子码流, 2- 第三码流
    uint32_t        UrlLen;
    uint8_t         szUrl[RTSP_URL_LEN];
    uint32_t        SdpLen;
    uint8_t         szSdp[SDP_MSG_LENS];
    uint32_t        MediaLinkMode;//FOR VMS and EHOME 连接模式：0- TCP方式，1- UDP方式，2- HRUDP方式（可靠传输）
    uint8_t         szMediaIP[SVS_IP_LEN];//FOR VMS and EHOME
    uint16_t        usMediaPort;//FOR VMS and EHOME


public:
    REQUEST_SEND_INVITE2DEV()
    {
        eRequestType = REQ_TYPE_SEND_INVITE2DEV;
        eCallType = SETUP_TYPE_MAX;
        ePlayType       = PLAY_TYPE_MAX;
        eUrlType         = PLAY_URL_TYPE_MAX;
        eStreamtype      = DEV_STREAM_TYPE_MAIN;
        UrlLen = 0;
        memset(szUrl,0,RTSP_URL_LEN);
        SdpLen = 0;
        memset(szSdp,0,SDP_MSG_LENS);
        memset(szMuSvrID, 0, sizeof(szMuSvrID));
        MediaLinkMode = 0;
        memset(szMediaIP,0,SVS_IP_LEN);
        usMediaPort = 0;
    }
};

class REQUEST_SEND_ACK2DEV : public REQUEST_TO_DEV
{
public:
    uint32_t    lSessionID; // FOR Ehome Dev
public:
    REQUEST_SEND_ACK2DEV()
    {
        eRequestType = REQ_TYPE_SEND_ACK2DEV;
        lSessionID = 0;
    }
};


class REQUEST_SEND_KEYFRAME2DEV : public REQUEST_TO_DEV
{
public:
    // uint32_t                    uiServerIndex;
    // SVS_MSG_HEADER    stRequestHeader;  // 响应中带着


public:
    REQUEST_SEND_KEYFRAME2DEV()
    {
        eRequestType = REQ_TYPE_SEND_KEYFRAME2DEV;
        // uiServerIndex = 0;
        // memset(&stRequestHeader, 0, sizeof(stRequestHeader));
    }
};


class REQUEST_SEND_BYE2DEV : public REQUEST_TO_DEV
{
public:
    uint32_t    lSessionID; // FOR Ehome Dev
public:
    REQUEST_SEND_BYE2DEV()
    {
        eRequestType = REQ_TYPE_SEND_BYE2DEV;
        lSessionID = 0;
    }
};




}   //namespace SVS_ACM

#endif
