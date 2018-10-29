#include "svs_device_stack_ehome.h"
#include "svs_log_msg.h"
#include "svs_logging_strategy.h"
#include "svs_ac_common.h"
#include "ace/OS_Log_Msg_Attributes.h"
#include "svs_gb28181_manscdp.h"
#include "svs_access_control_manager.h"
#include "svs_acm_request.h"
#include "svs_access_control.h"
#include "svs_utility.h"
#include "svs_ac_common.h"


static ACE_OS_Log_Msg_Attributes g_objACELogMsgAttr;

enum enEHOME_LOG_LEVEL
{
    EHOME_LOG_LEVEL_CLOSE    = 0,/*0-表示关闭日志 */
    EHOME_LOG_LEVEL_ERROR    = 1,/*1-表示只输出ERROR错误日志*/
    EHOME_LOG_LEVEL_ERRDEBUG = 2,/*2-输出ERROR错误信息和DEBUG调试信息*/
    EHOME_LOG_LEVEL_ALL      = 3,/*3-输出ERROR错误信息、DEBUG调试信息和INFO普通信息等所有信息*/
};


CDeviceStackEhome::CDeviceStackEhome()
    : m_ulEhomeHandle(-1)
{
    SVS_TRACE();
}

CDeviceStackEhome::~CDeviceStackEhome()
{
    SVS_TRACE();
}
int32_t CDeviceStackEhome::initialize()
{
    SVS_TRACE();

     ACE_Log_Msg::init_hook(g_objACELogMsgAttr);

    if (!NET_ECMS_Init())
    {
        SVS_LOG((SVS_LM_ERROR, "Initialize ehome stack failed."));
        return SVS_ERROR_FAIL;
    }
    /* set ehome log info */
    int32_t lLoglevel = SVS_Log_Msg::get_log_priority();
    DWORD   iehomeLogLevel = EHOME_LOG_LEVEL_CLOSE;

    if((SVS_LM_TRACE == lLoglevel)||(SVS_LM_DEBUG == lLoglevel))
    {
        iehomeLogLevel = EHOME_LOG_LEVEL_ALL;
    }
    else if((SVS_LM_DEBUG == lLoglevel)||(SVS_LM_INFO == lLoglevel))
    {
        iehomeLogLevel = EHOME_LOG_LEVEL_ERRDEBUG;
    }
    else if((SVS_LM_ERROR == lLoglevel)||(SVS_LM_WARNING == lLoglevel)||(SVS_LM_CRITICAL == lLoglevel))
    {
        iehomeLogLevel = EHOME_LOG_LEVEL_ERROR;
    }
    (void)NET_ECMS_SetLogToFile(iehomeLogLevel,(char*)LOG_DIR,TRUE);
    return SVS_ERROR_OK;
}

int32_t CDeviceStackEhome::release()
{
    SVS_TRACE();
    (void)NET_ECMS_Fini();
    return SVS_ERROR_OK;
}

int32_t CDeviceStackEhome::start()
{
    SVS_TRACE();

    NET_EHOME_CMS_LISTEN_PARAM listen_param;
    memset(&listen_param,0,sizeof(NET_EHOME_CMS_LISTEN_PARAM));

    int32_t nPort = CAccessControlSvr::instance().get_device_ehome_stack_listen_port();
    const char* pszIP = CAccessControlSvr::instance().getSvrIp();

    memcpy(listen_param.struAddress.szIP, pszIP, strlen(pszIP));
    listen_param.struAddress.wPort = (WORD)nPort;
    listen_param.fnCB = ehomeDeviceEventCallback;
    listen_param.pUserData = this;


    m_ulEhomeHandle = NET_ECMS_StartListen(&listen_param);
    if(m_ulEhomeHandle <= -1)
    {
        SVS_LOG((SVS_LM_ERROR, "NET_ECMS_StartListen failed, error code: %d", NET_ECMS_GetLastError()));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_DEBUG, "NET_ECMS_StartListen success."));

    return SVS_ERROR_OK;
}

int32_t CDeviceStackEhome::stop()
{
    SVS_TRACE();
    if(-1 < m_ulEhomeHandle) {
        NET_ECMS_StopListen(m_ulEhomeHandle);
        m_ulEhomeHandle = -1;
    }
    return SVS_ERROR_OK;
}

int32_t CDeviceStackEhome::timeout()
{
    ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, m_mutex, SVS_ERROR_FAIL);
    MAP_EHOME_DEV::iterator iter = m_EhomeDevMap.begin();
    EHOME_DEV_INFO         *pDev = NULL;

    for(;iter != m_EhomeDevMap.end();++iter)
    {
        pDev = iter->second;
        notifyDeviceOnline(pDev);
    }
    return SVS_ERROR_OK;
}


void CDeviceStackEhome::handle_ehome_event(LONG lUserID, DWORD dwDataType, void *pOutBuffer,
                                           DWORD dwOutLen,void *pInBuffer, DWORD dwInLen)
{

    SVS_LOG((SVS_LM_DEBUG, "handle ehome event,user id:[%d] type:[%d].",lUserID,dwDataType));
    EHOME_DEV_INFO         *pDev = NULL;
    MAP_EHOME_DEV::iterator iter;
    char* pbuf                   = NULL;
    std::string strDevID;


    ACE_GUARD(ACE_Recursive_Thread_Mutex, guard, m_mutex);

    if (ENUM_DEV_ON == dwDataType)
    {
       NET_EHOME_DEV_REG_INFO *pDevInfo = (NET_EHOME_DEV_REG_INFO *)pOutBuffer;
       if (pDevInfo == NULL)
       {
           return;
       }
       SVS_LOG((SVS_LM_DEBUG, "ehome device Online, lUserID: %d, Device ID: %s\n", lUserID, pDevInfo->byDeviceID));

       strDevID = (char*)&pDevInfo->byDeviceID[0];

       iter = m_EhomeDevMap.find(strDevID);

       if(iter == m_EhomeDevMap.end())
       {

           pbuf = SVS_NEW(pbuf,sizeof(EHOME_DEV_INFO));
           if(NULL == pbuf)
           {
               SVS_LOG((SVS_LM_CRITICAL, "ehome device Online, lUserID: %d, Device ID: %s,allcato the buffer fail.",
                                         lUserID, pDevInfo->byDeviceID));
               return;
           }
           pDev = (EHOME_DEV_INFO *)pbuf;
           pDev->lUserID = lUserID;
           pDev->bSyncInfo = false;
           memcpy(&pDev->regInfo,pOutBuffer,sizeof(NET_EHOME_DEV_REG_INFO));

           m_EhomeDevMap.insert(MAP_EHOME_DEV::value_type(strDevID,pDev));
       }
       else {
           pDev = iter->second;
           pDev->lUserID = lUserID;
           memcpy(&pDev->regInfo,pOutBuffer,sizeof(NET_EHOME_DEV_REG_INFO));
       }
       //输入参数
       NET_EHOME_SERVER_INFO *pServerInfo = (NET_EHOME_SERVER_INFO *)pInBuffer;
       pServerInfo->dwTimeOutCount = 6; //心跳超时次数
       pServerInfo->dwKeepAliveSec = CAccessControlSvr::instance().get_device_keep_alive_time_out(); //心跳间隔

       /* notify the device online by the time */
       //notifyDeviceOnline(pDev);
    }
    else if (ENUM_DEV_OFF == dwDataType)
    {
        SVS_LOG((SVS_LM_DEBUG, "ehome device Offline, lUserID: %d\n", lUserID));

        iter = m_EhomeDevMap.begin();

        for(;iter != m_EhomeDevMap.end();++iter)
        {
           pDev = iter->second;
           if(lUserID == pDev->lUserID)
           {
               m_EhomeDevMap.erase(iter);

               notifyDeviceOffline(pDev);

               pbuf = (char*)pDev;
               SVS_DELETE(pbuf,SVS_DELETE_MULTI);
               pbuf = NULL;
               break;
           }
        }

        NET_ECMS_ForceLogout(lUserID);

    }
}


BOOL CDeviceStackEhome::ehomeDeviceEventCallback(LONG lUserID, DWORD dwDataType, void *pOutBuffer, DWORD dwOutLen,
                                                 void *pInBuffer, DWORD dwInLen, void *pUser)
{
    CDeviceStackEhome* pEhomeStack = (CDeviceStackEhome*)pUser;
    pEhomeStack->handle_ehome_event(lUserID, dwDataType, pOutBuffer, dwOutLen,pInBuffer, dwInLen);
    return TRUE;
}

void CDeviceStackEhome::notifyDeviceOnline(EHOME_DEV_INFO* pDevInfo)
{
    SVS_ACM::REQUEST_NOTIFY_DEV_INFO stRequest;
    SVS_ACM::DEVICE_INFO& stDeviceInfo = stRequest.stDeviceInfo;
    stDeviceInfo.eDeviceType = SVS_DEV_TYPE_EHOME;
    stDeviceInfo.eDeviceStatus = SVS_DEV_STATUS_ONLINE;

    if(!pDevInfo->bSyncInfo)
    {
        if(!queryDevInfo(pDevInfo))
        {
            return;
        }
    }

    strncpy(&stDeviceInfo.szDeviceID[0], (char*)&pDevInfo->regInfo.byDeviceID[0], SVS_DEVICEID_LEN);

    char szPort[SVS_PORT_LEN + 1] = {0};

    snprintf(szPort,SVS_PORT_LEN,"%d",pDevInfo->regInfo.struDevAdd.wPort);

    strncpy(stRequest.stDeviceInfo.szHost, pDevInfo->regInfo.struDevAdd.szIP, sizeof(stRequest.stDeviceInfo.szHost) - 1);
    strncpy(stRequest.stDeviceInfo.szPort, szPort, sizeof(stRequest.stDeviceInfo.szPort) - 1);

    SVS_LOG((SVS_LM_DEBUG, "notifyDeviceOnline, dwChannelNumber: %d, dwChannelAmount:%d, dwStartChannel:%d ,dwDevType: %d.",
                                     pDevInfo->devInfo.dwChannelNumber, pDevInfo->devInfo.dwChannelAmount,
                                     pDevInfo->devInfo.dwStartChannel,pDevInfo->devInfo.dwDevType));
    SVS_LOG((SVS_LM_DEBUG, "notifyDeviceOnline, sServerName: %s, dwChannelNum:%d, dwDevType: %d.",
                                         pDevInfo->devCfg.sServerName, pDevInfo->devCfg.dwChannelNum,pDevInfo->devCfg.dwServerType));

    //for(DWORD i = 0; i < pDevInfo->devInfo.dwChannelAmount;i++)
    for(DWORD i = 0; i < pDevInfo->devCfg.dwChannelNum;i++)
    {
        SVS_ACM::LENS_INFO stLensInfo;
        stLensInfo.eLensType = SVS_DEV_TYPE_EHOME;
        stLensInfo.eLensStatus = SVS_DEV_STATUS_ONLINE;
        generateLensIDbycChannel((uint8_t*)&stDeviceInfo.szDeviceID[0],i,(uint8_t*)&stLensInfo.szLensID[0]);
        strncpy(stLensInfo.szDeviceID, stDeviceInfo.szDeviceID, SVS_DEVICEID_LEN);
        snprintf(&stLensInfo.szLensName[0],DEVICE_NAME_LEN,"ehome-dev-%02d",i);
        strncpy(stLensInfo.szManufacturer, "hik-ehome",DEV_FAC_LEN);
        stDeviceInfo.vecLensInfo.push_back(stLensInfo);
    }

    int32_t nResult = IAccessControlManager::instance().asyncRequest(stRequest);
    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "ehome Notfiy device '%s' online failed.", stRequest.stDeviceInfo.szDeviceID));
        return ;
    }

    SVS_LOG((SVS_LM_INFO, "Notfiy device '%s' online success.", stRequest.stDeviceInfo.szDeviceID));
    return ;
}
void CDeviceStackEhome::notifyDeviceOffline(EHOME_DEV_INFO* pDevInfo)
{
    SVS_TRACE();

    SVS_ACM::REQUEST_NOTIFY_DEV_INFO stRequest;
    SVS_ACM::DEVICE_INFO& stDeviceInfo = stRequest.stDeviceInfo;
    stDeviceInfo.eDeviceType = SVS_DEV_TYPE_EHOME;
    stDeviceInfo.eDeviceStatus = SVS_DEV_STATUS_OFFLINE;
    strncpy(stDeviceInfo.szDeviceID, (char*)&pDevInfo->regInfo.byDeviceID[0], SVS_DEVICEID_LEN);

    int32_t nResult = IAccessControlManager::instance().asyncRequest(stRequest);
    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Notfiy device '%s' offline failed.", stDeviceInfo.szDeviceID));
        return;
    }

    SVS_LOG((SVS_LM_INFO, "Notfiy device '%s' offline success.", stDeviceInfo.szDeviceID));
    return ;
}

bool CDeviceStackEhome::queryDevInfo(EHOME_DEV_INFO* pDevInfo)
{
   //获取设备通道参数
   NET_EHOME_CONFIG        config;
   memset(&config,0,sizeof(NET_EHOME_CONFIG));

   config.pOutBuf = (void*)&pDevInfo->devInfo;
   config.dwOutSize = sizeof(NET_EHOME_DEVICE_INFO);

   BOOL bRet = NET_ECMS_GetDevConfig(pDevInfo->lUserID,NET_EHOME_GET_DEVICE_INFO,&config,sizeof(NET_EHOME_CONFIG));
   if(bRet)
   {

       SVS_LOG((SVS_LM_DEBUG, "ehome device Info, dwChannelNumber: %d, dwChannelAmount:%d, dwStartChannel:%d ,dwDevType: %d.",
                                     pDevInfo->devInfo.dwChannelNumber, pDevInfo->devInfo.dwChannelAmount,
                                     pDevInfo->devInfo.dwStartChannel,pDevInfo->devInfo.dwDevType));
   }
   else
   {
       SVS_LOG((SVS_LM_WARNING, "ehome device Info, lUserID: %d, Device ID: %s,get device info fail,last error:[%d].",
                                     pDevInfo->lUserID, pDevInfo->regInfo.byDeviceID,NET_ECMS_GetLastError()));
       return false;
   }

   //获取设备配置参数
   memset(&config,0,sizeof(NET_EHOME_CONFIG));

   config.pOutBuf = (void*)&pDevInfo->devCfg;
   config.dwOutSize = sizeof(NET_EHOME_DEVICE_CFG);

   bRet = NET_ECMS_GetDevConfig(pDevInfo->lUserID,NET_EHOME_GET_DEVICE_CFG,&config,sizeof(NET_EHOME_CONFIG));
   if(bRet)
   {
       SVS_LOG((SVS_LM_DEBUG, "ehome device cfg, sServerName: %s, dwChannelNum:%d,"
                              "dwDevType: %d.",
                              pDevInfo->devCfg.sServerName, pDevInfo->devCfg.dwChannelNum,
                              pDevInfo->devCfg.dwServerType));

   }
   else
   {
       SVS_LOG((SVS_LM_WARNING, "ehome device cfg, lUserID: %d, Device ID: %s,get device cfg fail,last error:[%d].",
                                     pDevInfo->lUserID, pDevInfo->regInfo.byDeviceID,NET_ECMS_GetLastError()));
       return false;
   }
   pDevInfo->bSyncInfo = true;
   return true;
}


/*********************** ehome ID ********************
*
* MASTER ID 20 :18 Byte number + 00 :20181023163800000100
* LENS ID: 18 byte prefix maste ID+ 2 byte channel ID :20181023163800000101
*/
void CDeviceStackEhome::generateLensIDbycChannel(uint8_t* pszDevID,LONG lChannel,uint8_t* pszLenID)
{
    LONG lChannelIndex = lChannel + 1;
    strncpy((char*)pszLenID, (char*)pszDevID, SVS_DEVICEID_LEN - 2);
    snprintf((char*)&pszLenID[SVS_DEVICEID_LEN - 2],3,"%02d",lChannelIndex);
}
LONG CDeviceStackEhome::getChannelIDbyLensID(uint8_t* pszDevID,uint8_t* pszLenID)
{
    uint8_t ChannelIndex[SVS_DEVICEID_LEN] = {0};
    memcpy(ChannelIndex,&pszLenID[SVS_DEVICEID_LEN - 2],2);

    LONG lChannelID = atoi((char*)&ChannelIndex[0]);
    if(0 < lChannelID) {
        lChannelID--;
    }

    MAP_EHOME_DEV::iterator iter = m_EhomeDevMap.find((char*)pszDevID);
    EHOME_DEV_INFO         *pDev = NULL;
    if(iter == m_EhomeDevMap.end())
    {
        return lChannelID;
    }
    pDev = iter->second;

    lChannelID += pDev->devInfo.dwStartChannel;


    return lChannelID;
}

std::string CDeviceStackEhome::getDevIDbyUserID(LONG lUserID)
{
    std::string strDevID = "";
    MAP_EHOME_DEV::iterator iter = m_EhomeDevMap.begin();
    EHOME_DEV_INFO         *pDev = NULL;

    for(;iter != m_EhomeDevMap.end();++iter)
    {
        pDev = iter->second;
        if(lUserID == pDev->lUserID)
        {
            strDevID = iter->first;
            break;
        }
    }
    return strDevID;
}

LONG CDeviceStackEhome::getUserIDbyDevID(uint8_t* pszDevID)
{
    MAP_EHOME_DEV::iterator iter = m_EhomeDevMap.find((char*)pszDevID);
    EHOME_DEV_INFO         *pDev = NULL;
    if(iter == m_EhomeDevMap.end())
    {
        return SVS_ERROR_FAIL;
    }
    pDev = iter->second;
    return pDev->lUserID;
}

int32_t CDeviceStackEhome::asyncRequest(SVS_ACM::REQUEST& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    SVS_TRACE();

    int32_t nResult = SVS_ERROR_OK;

    switch (rRequest.getType())
    {
        case SVS_ACM::REQ_TYPE_SEND_INVITE2DEV:
        {
            nResult = ehomeMediaRequest((SVS_ACM::REQUEST_SEND_INVITE2DEV&)rRequest,pCallBack,pUserData);
            break;
        }
        case SVS_ACM::REQ_TYPE_SEND_BYE2DEV:
        {
            nResult = ehomeMediaStopRequest((SVS_ACM::REQUEST_SEND_BYE2DEV&)rRequest,pCallBack,pUserData);
            break;
        }
        case SVS_ACM::REQ_TYPE_SEND_ACK2DEV:
        {
            nResult = ehomeMediaPlayRequest((SVS_ACM::REQUEST_SEND_ACK2DEV&)rRequest,pCallBack,pUserData);;
            break;
        }
        case SVS_ACM::REQ_TYPE_SEND_KEYFRAME2DEV:
        {
            break;
        }
        case SVS_ACM::REQ_TYPE_DEV_CTRL:
        {
            break;
        }
        default:
        SVS_LOG((SVS_LM_ERROR, "Async request type %d is invalid.", rRequest.getType()));
        return SVS_ERROR_FAIL;
        //break;
    }
    return nResult;
}

int32_t CDeviceStackEhome::ehomeMediaRequest(SVS_ACM::REQUEST_SEND_INVITE2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    NET_EHOME_PREVIEWINFO_IN_V11 previewInfo;
    NET_EHOME_PREVIEWINFO_OUT   previewOut;
    SVS_ACM::RESPONSE_SEND_INVITE2DEV rResponse;

    memset(&previewInfo,0,sizeof(NET_EHOME_PREVIEWINFO_IN_V11));
    memset(&previewOut,0,sizeof(NET_EHOME_PREVIEWINFO_OUT));

    if(SVS_DEV_TYPE_EHOME != rRequest.eDevType)
    {
         SVS_LOG((SVS_LM_ERROR, "ehome Media Request,but the device type:[%d] is not ehome device.", rRequest.eDevType));
         return SVS_ERROR_FAIL;
    }

    LONG  lUserID = getUserIDbyDevID((uint8_t*)&rRequest.szDeviceID[0]);

    previewInfo.iChannel = getChannelIDbyLensID((uint8_t*)&rRequest.szDeviceID[0],(uint8_t*)&rRequest.szLensID[0]);
    previewInfo.dwStreamType = rRequest.eStreamtype;
    previewInfo.dwLinkMode = rRequest.MediaLinkMode;
    strncpy(&previewInfo.struStreamSever.szIP[0],(char*)&rRequest.szMediaIP[0],SVS_IP_LEN);
    previewInfo.struStreamSever.wPort = rRequest.usMediaPort;
    previewInfo.byDelayPreview = 0;

    BOOL ret = NET_ECMS_StartGetRealStreamV11(lUserID,&previewInfo,&previewOut);
    if(!ret) {
        SVS_LOG((SVS_LM_ERROR, "ehome Media Request,start get real stream fail,userid:[%d]"
                               "channel:[%d],streamType:[%d],linkmode:[%d],"
                               "server:[%s:%d],"
                               "last error:[%d].",
                               lUserID,
                               previewInfo.iChannel,previewInfo.dwStreamType,previewInfo.dwLinkMode,
                               previewInfo.struStreamSever.szIP,previewInfo.struStreamSever.wPort,
                               NET_ECMS_GetLastError()));
        return SVS_ERROR_FAIL;
    }
    SVS_LOG((SVS_LM_DEBUG, "ehome Media Request,start get real stream success,SessionID:[%d].", previewOut.lSessionID));
    rResponse.lSessionID = previewOut.lSessionID;
    rResponse.nRequestID = rRequest.nRequestID;
    memcpy(rResponse.szSdp, rRequest.szSdp, rRequest.SdpLen);
    rResponse.SdpLen = rRequest.SdpLen;
    strncpy(rResponse.szLensID, rRequest.szLensID, sizeof(rRequest.szLensID) - 1);
    SVS_LOG((SVS_LM_DEBUG, "ehome Media Request,start get real stream callback,lenid:[%s],sdp:[%s].", rResponse.szLensID,rResponse.szSdp));
    pCallBack(rResponse, pUserData);
    SVS_LOG((SVS_LM_DEBUG, "ehome Media Request,start get real stream end."));
    return SVS_ERROR_OK;
}

int32_t CDeviceStackEhome::ehomeMediaPlayRequest(SVS_ACM::REQUEST_SEND_ACK2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    NET_EHOME_PUSHSTREAM_IN     PushInfo;
    NET_EHOME_PUSHSTREAM_OUT    PushOut;

    memset(&PushInfo,0,sizeof(NET_EHOME_PUSHSTREAM_IN));
    memset(&PushOut,0,sizeof(NET_EHOME_PUSHSTREAM_OUT));
    LONG  lUserID = getUserIDbyDevID((uint8_t*)&rRequest.szDeviceID[0]);
    PushInfo.lSessionID = rRequest.lSessionID;

    BOOL ret = NET_ECMS_StartPushRealStream(lUserID,&PushInfo,&PushOut);
    if(!ret) {
        SVS_LOG((SVS_LM_ERROR, "ehome Media push,start push real stream fail,last error:[%d].", NET_ECMS_GetLastError()));
        return SVS_ERROR_FAIL;
    }
    return SVS_ERROR_OK;
}
int32_t CDeviceStackEhome::ehomeMediaStopRequest(SVS_ACM::REQUEST_SEND_BYE2DEV& rRequest, SVS_ACM::RESPONSE_CALLBACK pCallBack, void* pUserData)
{
    LONG  lUserID = getUserIDbyDevID((uint8_t*)&rRequest.szDeviceID[0]);

    BOOL ret = NET_ECMS_StopGetRealStream(lUserID,rRequest.lSessionID);
    if(!ret) {
        SVS_LOG((SVS_LM_ERROR, "ehome Media push,stop real stream fail,last error:[%d].", NET_ECMS_GetLastError()));
        return SVS_ERROR_FAIL;
    }
    return SVS_ERROR_OK;
}



