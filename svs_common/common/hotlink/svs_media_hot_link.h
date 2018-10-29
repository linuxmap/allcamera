/*
 *
 *  Created on: Jan 9, 2015
 *      Author: timehunter
 */

#ifndef CSVSMediaLinkFactory_H_
#define CSVSMediaLinkFactory_H_
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vms/vms.h>
const std::string key_mask       = "svs_url_mask";
const std::string key            = "key=";
const std::string time_stamp     = "timestamp=";
const std::string time_out       = "timeout=";
const std::string str_time       = "30";
const std::string str_encrypt    = "encrypt=";
const std::string str_streamtype = "streamtype=";
const std::string str_starttime  = "starttime=";
const std::string str_endtime    = "endtime=";
const std::string str_devtype    = "devtype=";
const std::string str_splitchar  = "&";
const std::string str_arg_begin  = "?";
const std::string str_split_dir  = "/";
const std::string str_arg_no     = "no";
const std::string str_arg_eq     = "=";


const std::string url_prexfix_rtsp       = "rtsp://";
const std::string url_prexfix_hls        = "http://";
const std::string url_prexfix_rtmp       = "rtmp://";


const std::string str_live_tag   = "live";
const std::string str_plat_tag   = "plat_vod";
const std::string str_front_tag  = "front_vod";


const uint32_t MD5_CODE_LEN      =  16;

const uint32_t TIME_LEN          = 15;

const uint32_t MEDIA_STR_MAX_LEN = 512;


typedef enum tagSVS_MEDIA_LINK_RESULT
{
    SVS_MEDIA_LINK_RESULT_FAIL      = -1,
    SVS_MEDIA_LINK_RESULT_SUCCESS   = 0,
    SVS_MEDIA_LINK_RESULT_AUTH_FAIL = 1
}SVS_MEDIA_LINK_RESULT;


class CSVSMediaLink
{
public:
    CSVSMediaLink();
    virtual ~CSVSMediaLink();
    std::string   Url() { return m_strUrl;};
    void Url(const  std::string& strUrl) { m_strUrl = strUrl;};
    PLAY_URL_TYPE UrlType() {return m_enType;};
    void UrlType(PLAY_URL_TYPE type){ m_enType = type;};
    PLAY_TYPE PlayType(){return m_enPlayType;};
    void PlayType(PLAY_TYPE type){m_enPlayType = type;};
    SVS_DEV_TYPE DevType(){return m_enDevType;};
    void DevType(SVS_DEV_TYPE type){m_enDevType = type;};
    std::string ServerIP() {return m_strServerIP;};
    void ServerIP(const std::string& strServerIP) { m_strServerIP = strServerIP; };
    uint32_t Port() { return m_ulPort;};
    void Port(uint32_t ulPort) { m_ulPort = ulPort;};
    std::string ContentID() {return m_strContentID;};
    void ContentID(const std::string& strContentID) { m_strContentID = strContentID;};
    DEV_STREAM_TYPE StreamType(){ return m_enStreamtype;} ;
    void StreamType(DEV_STREAM_TYPE enStreamtype) { m_enStreamtype = enStreamtype;};
    std::string StartTime() { return m_strStartTime;};
    void StartTime(const std::string& strStartTime) { m_strStartTime = strStartTime;};
    std::string EndTime() { return m_strEndTime;};
    void EndTime(const std::string& strEndTime) { m_strEndTime = strEndTime;};
private:
    std::string     m_strUrl;
    PLAY_URL_TYPE   m_enType;
    PLAY_TYPE       m_enPlayType;
    std::string     m_strServerIP;
    uint32_t        m_ulPort;
    std::string     m_strContentID;
    DEV_STREAM_TYPE m_enStreamtype;
    std::string     m_strStartTime;
    std::string     m_strEndTime;
    SVS_DEV_TYPE    m_enDevType;
};

class CSVSMediaLinkFactory
{
public:
    static CSVSMediaLinkFactory& instance()
    {
       static CSVSMediaLinkFactory objSVShotLink;
       return objSVShotLink;
    }
    virtual ~CSVSMediaLinkFactory();
    int32_t init(std::string & strMask);
    int32_t makeMediaUrl(CSVSMediaLink * linkInfo,std::string& strUrl);
    int32_t parseMediaUrl(const std::string& strUrl,CSVSMediaLink * linkInfo);
protected:
    CSVSMediaLinkFactory();
private:
    void    makeHotUri(const std::string& strBasicUri, std::string& strHotUri)const;
    int32_t checkHoturi(const std::string& strHotUri, std::string& strBasicUri,CSVSMediaLink * linkInfo)const;
    int32_t splitUrl(const std::string &strUrl,std::string &strBasicUrl,CSVSMediaLink * linkInfo,
                     std::string &strTimestamp,std::string &strTimeout,std::string &strEncrypt)const;
    int32_t encrpyt(const std::string &srcSrc, std::string &strResult)const;
private:
    std::string m_strMask;
};

#endif /* CSVSMediaLinkFactory_H_ */
