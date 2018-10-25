/*
 * CSVSMediaLinkFactory.cpp
 *
 *  Created on: Jan 9, 2015
 *      Author: timehunter
 */

#include "svs_media_hot_link.h"
#include "md5.h"
#include "svs_utility.h"

CSVSMediaLink::CSVSMediaLink()
{
   m_strUrl       = "";
   m_enType       = PLAY_URL_TYPE_DEFAULT;
   m_enDevType    = SVS_DEV_TYPE_GB28181;
   m_enPlayType   = PLAY_TYPE_LIVE;
   m_strServerIP  = str_arg_no;
   m_ulPort       = 554;
   m_strContentID = str_arg_no;
   m_ulStreamtype = 0;
   m_strStartTime = str_arg_no;
   m_strEndTime   = str_arg_no;
}
CSVSMediaLink::~CSVSMediaLink()
{
}


CSVSMediaLinkFactory::CSVSMediaLinkFactory()
{
    m_strMask = key_mask;
}

CSVSMediaLinkFactory::~CSVSMediaLinkFactory()
{
}

int32_t CSVSMediaLinkFactory::init(std::string & strMask)
{
    m_strMask = strMask;
    return SVS_MEDIA_LINK_RESULT_SUCCESS;
}
int32_t CSVSMediaLinkFactory::makeMediaUrl(CSVSMediaLink * linkInfo,std::string& strUrl)
{
    if(NULL == linkInfo) {
        return SVS_MEDIA_LINK_RESULT_FAIL;
    }
    char szTmpStr[MEDIA_STR_MAX_LEN] = {0};
    std::string strMediaUrl= "rtsp://";
    std::string strMediaUri= "";
    std::string strHotUri  = "";
    PLAY_URL_TYPE type = linkInfo->UrlType();
    PLAY_TYPE playType = linkInfo->PlayType();


    /*protocol */
    if(PLAY_URL_TYPE_RTSP == type) {
        strMediaUrl= "rtsp://";
    }
    else if(PLAY_URL_TYPE_HLS == type) {
        strMediaUrl= "http://";
    }
    else if(PLAY_URL_TYPE_RTMP == type) {
        strMediaUrl= "rtmp://";
    }
    /* IP */
    strMediaUrl += linkInfo->ServerIP();

    /*port*/
    snprintf(szTmpStr,MEDIA_STR_MAX_LEN,":%d",linkInfo->Port());
    strMediaUrl += szTmpStr;

    /* play type */
    strMediaUri = str_split_dir;
    if(PLAY_TYPE_LIVE == playType) {
        strMediaUri += str_live_tag;
    }
    else if(PLAY_TYPE_PLAT_RECORD == playType) {
        strMediaUri += str_plat_tag;
    }
    else if(PLAY_TYPE_FRONT_RECORD == playType) {
        strMediaUri += str_front_tag;
    }

    /* contentid */
    strMediaUri += str_split_dir;
    strMediaUri += linkInfo->ContentID();

    /* args */
    strMediaUri += str_arg_begin;
    strMediaUri += str_streamtype;
    snprintf(szTmpStr,MEDIA_STR_MAX_LEN,"%d",linkInfo->StreamType());
    strMediaUri += szTmpStr;

    strMediaUri += str_splitchar;
    strMediaUri += str_devtype;
    snprintf(szTmpStr,MEDIA_STR_MAX_LEN,"%d",linkInfo->DevType());
    strMediaUri += szTmpStr;

    strMediaUri += str_splitchar;
    strMediaUri += str_starttime;
    strMediaUri += linkInfo->StartTime();

    strMediaUri += str_splitchar;
    strMediaUri += str_endtime;
    strMediaUri += linkInfo->EndTime();

    /* timeout */
    std::string strTimestamp = "";
    std::string strTimeout = "";
    std::string strEncrypt = "";

    time_t lSysTime = time(NULL);

    char strLocalTime[TIME_LEN + 1] = {0, };
    (void)SVS_SS_UTILITIES::time2str(strLocalTime, sizeof(strLocalTime), lSysTime);
    strTimestamp = strLocalTime;

    /* timestamp */
    strMediaUri += str_splitchar;
    strMediaUri += time_stamp;
    strMediaUri += strTimestamp;

    /* timeout */
    strMediaUri += str_splitchar;
    strMediaUri += time_out;
    strMediaUri += str_time;

    makeHotUri(strMediaUri,strHotUri);
    strUrl = strMediaUrl+ strHotUri;
    linkInfo->Url(strUrl);
    return SVS_MEDIA_LINK_RESULT_SUCCESS;
}
int32_t CSVSMediaLinkFactory::parseMediaUrl(const std::string& strUrl,CSVSMediaLink * linkInfo)
{
    if(NULL == linkInfo) {
        return SVS_MEDIA_LINK_RESULT_FAIL;
    }

    std::string strTmp = strUrl;
    linkInfo->Url(strTmp);

    std::string::size_type Idx = 0;
    Idx = strTmp.find("://");
    if(std::string::npos == Idx)
    {
        return SVS_MEDIA_LINK_RESULT_FAIL;
    }
    Idx += 3;
    std::string strProtcol = strTmp.substr(0, Idx);

    if(url_prexfix_rtsp == strProtcol) {
        linkInfo->UrlType(PLAY_URL_TYPE_RTSP);
    }
    else if(url_prexfix_hls == strProtcol) {
        linkInfo->UrlType(PLAY_URL_TYPE_HLS);
    }
    else if(url_prexfix_rtmp == strProtcol) {
        linkInfo->UrlType(PLAY_URL_TYPE_RTMP);
    }
    else {
        return SVS_MEDIA_LINK_RESULT_FAIL;
    }

    strTmp = strTmp.substr(Idx);
    Idx = strTmp.find("/");
    if(std::string::npos == Idx)
    {
        return SVS_MEDIA_LINK_RESULT_FAIL;
    }
    std::string strHotUri = strTmp.substr(Idx);


    /* ip and port */
    Idx = strTmp.find(":");
    std::string strIp = "";
    if(std::string::npos != Idx)
    {
        strIp = strTmp.substr(0, Idx);
        linkInfo->ServerIP(strIp);
        Idx += 1;
        strTmp = strTmp.substr(Idx);
        Idx = strTmp.find("/");
        if(std::string::npos == Idx)
        {
            return SVS_MEDIA_LINK_RESULT_FAIL;
        }
        std::string strPort = strTmp.substr(0, Idx);
        linkInfo->Port(atoi(strPort.c_str()));
        Idx += 1;
        strTmp = strTmp.substr(Idx);
    }
    else {
        Idx = strTmp.find("/");
        if(std::string::npos == Idx)
        {
            return SVS_MEDIA_LINK_RESULT_FAIL;
        }
        strIp = strTmp.substr(0, Idx);
        linkInfo->ServerIP(strIp);
        Idx += 1;
        strTmp = strTmp.substr(Idx);
    }

    /* play type */
    Idx = strTmp.find("/");
    if(std::string::npos == Idx)
    {
        return SVS_MEDIA_LINK_RESULT_FAIL;
    }

    std::string strPlayType = strTmp.substr(0, Idx);
    Idx += 1;
    strTmp = strTmp.substr(Idx);

    if(str_live_tag == strPlayType) {
        linkInfo->PlayType(PLAY_TYPE_LIVE);
    }
    else if(str_plat_tag == strPlayType) {
        linkInfo->PlayType(PLAY_TYPE_PLAT_RECORD);
    }
    else if(str_front_tag == strPlayType) {
        linkInfo->PlayType(PLAY_TYPE_FRONT_RECORD);
    }
    else {
        return SVS_MEDIA_LINK_RESULT_FAIL;
    }

    /* content */
    Idx = strTmp.find("?");
    if(std::string::npos == Idx)
    {
        /* there is no args ,so not set the arg info */
        std::string strContentID = strTmp;
        return SVS_MEDIA_LINK_RESULT_SUCCESS;
    }

    std::string strContentID = strTmp.substr(0, Idx);
    linkInfo->ContentID(strContentID);
    Idx += 1;
    strTmp = strTmp.substr(Idx);

    Idx = strTmp.find(str_arg_eq);
    while(std::string::npos != Idx)
    {
        std::string strarg = strTmp.substr(0, Idx);
        std::string strvalue = "";
        Idx += 1;
        strTmp = strTmp.substr(Idx);
        Idx = strTmp.find(str_splitchar);
        if(std::string::npos == Idx) {
            strvalue = strTmp;
            strTmp ="";//end of the arg parser
        }
        else {
            strvalue = strTmp.substr(0,Idx);
            Idx += 1;
            strTmp = strTmp.substr(Idx);
        }
        /* set the args */
        if(str_streamtype == strarg) {
            linkInfo->StreamType(atoi(strvalue.c_str()));
        }
        else if(str_devtype == strarg) {
            uint32_t ulDevType = atoi(strvalue.c_str());
            if(SVS_DEV_TYPE_VMS == ulDevType) {
                linkInfo->DevType(SVS_DEV_TYPE_VMS);
            }
            else if(SVS_DEV_TYPE_GB28181 == ulDevType) {
                linkInfo->DevType(SVS_DEV_TYPE_GB28181);
            }
        }
        else if(str_starttime == strarg) {
            linkInfo->StartTime(strvalue);
        }
        else if(str_endtime == strarg) {
            linkInfo->EndTime(strvalue);
        }

        Idx = strTmp.find(str_arg_eq);
    }

    std::string strbaseUri = "";
    return checkHoturi(strHotUri,strbaseUri,linkInfo);
}



void CSVSMediaLinkFactory::makeHotUri(const std::string& strBasicUrl, std::string& strHotUrl)const
{
    std::string strTimestamp = "";
    std::string strTimeout = "";
    std::string strEncrypt = "";

    std::string strTmpUrl = strBasicUrl + str_splitchar + key + m_strMask;
    std::string strEncryptResult = "";
    (void)encrpyt(strTmpUrl, strEncryptResult);

    strHotUrl = strBasicUrl + str_splitchar + str_encrypt + strEncryptResult;

    return ;
}
int32_t CSVSMediaLinkFactory::checkHoturi(const std::string& strHotUri, std::string& strBasicUri,CSVSMediaLink * linkInfo)const
{
    std::string strTimestamp = "";
    std::string strTimeout = "";
    std::string strEncrypt = "";
    int32_t nRet = splitUrl(strHotUri, strBasicUri,linkInfo, strTimestamp,strTimeout, strEncrypt);
    if(nRet != 0)
    {
        return SVS_MEDIA_LINK_RESULT_AUTH_FAIL;
    }

    time_t lUrlTime = SVS_SS_UTILITIES::str2time(strTimestamp.c_str());
    time_t lSysTime = time(NULL);

    uint32_t ulEffectiveWhile = atoi(strTimeout.c_str());

    if(ulEffectiveWhile < (uint32_t)abs(lSysTime - lUrlTime))
    {
        return SVS_MEDIA_LINK_RESULT_AUTH_FAIL;
    }
    std::string strSrc = strBasicUri + str_splitchar + key + m_strMask;
    std::string strEncryptResult = "";
    (void)encrpyt(strSrc, strEncryptResult);
    if (strEncrypt != strEncryptResult)
    {
        return SVS_MEDIA_LINK_RESULT_AUTH_FAIL;
    }
    return SVS_MEDIA_LINK_RESULT_SUCCESS;
}

int32_t CSVSMediaLinkFactory::splitUrl(const std::string &strUrl,std::string &strBasicUrl,CSVSMediaLink * linkInfo,
                                      std::string &strTimestamp,std::string &strTimeout,std::string &strEncrypt)const
{

    std::string::size_type nTimeIdx = 0;
    std::string::size_type nTimeoutIdx = 0;
    std::string::size_type nEncryptIdx = 0;
    nTimeIdx = strUrl.find(time_stamp);
    nTimeoutIdx = strUrl.find(time_out);
    nEncryptIdx = strUrl.find(str_encrypt);
    if((std::string::npos == nTimeIdx) || (std::string::npos == nEncryptIdx)
        ||(std::string::npos == nTimeoutIdx))
    {
        return SVS_MEDIA_LINK_RESULT_FAIL;
    }

    strBasicUrl = strUrl.substr(0, nEncryptIdx-1);
    std::string::size_type nStartIndex = nTimeIdx + time_stamp.length();
    std::string::size_type nLength = nTimeoutIdx - nStartIndex;
    strTimestamp = std::string(strUrl, nStartIndex, nLength);

    nStartIndex = nTimeoutIdx + time_out.length();
    nLength = nEncryptIdx - nStartIndex;
    strTimeout = std::string(strUrl, nStartIndex, nLength);

    nStartIndex = nEncryptIdx + str_encrypt.length();
    nLength = strUrl.length() - nStartIndex;
    strEncrypt = std::string(strUrl, nStartIndex, nLength);

    return SVS_MEDIA_LINK_RESULT_SUCCESS;
}
int32_t CSVSMediaLinkFactory::encrpyt(const std::string& srcSrc, std::string &strResult)const
{
    uint8_t md5Result[MD5_CODE_LEN] = {0,};
    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);
    MD5_Update(&md5Ctx, (uint8_t*)srcSrc.c_str(), srcSrc.length());
    MD5_Final(md5Result, &md5Ctx);

    strResult.clear();
    char buff[3] = {0,};
    for(uint32_t i = 0; i < MD5_CODE_LEN; i++)
    {
        sprintf(buff, "%02x",md5Result[i]);
        strResult.append(buff);
    }

    return 0;
}
