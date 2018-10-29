#ifndef CBUSINESSMANAGER_H_
#define CBUSINESSMANAGER_H_

#include <list>
#include <ctime>
#include "svs_ace_header.h"
#include "svs_ac_common.h"
#include <set>
#include <map>
#include <string>
using namespace std;

#define BUSINESS_TIME_OUT    30

class CBusiness
{
public:
    CBusiness();
    virtual ~CBusiness();

    virtual int32_t on_timer(time_t tCurrentTime, int32_t eventId = 0, int32_t timerId = 0) = 0;
    virtual bool isTimeOut(time_t tCurrentTime) = 0;

    virtual void server_offline(){};

    void setDevID(std::string strDevID);
    std::string getDevID();

    void setMuSvrID(std::string& strMuSvrID);
    std::string getMuSvrID();

    void setReqHandle(uint32_t uiReqHandle);
    uint32_t getReqHandle();

    void setReqTranNum(uint16_t uiReqTranNum);
    uint16_t getReqTranNum();

    void setReqLocalIndex(uint32_t uiReqLocalIndex);
    uint32_t getReqLocalIndex();

    void  setRtspUrl(std::string strRtspUrl) {m_strRtspUrl = strRtspUrl;};
    std::string getRtspUrl(){return m_strRtspUrl;};

    void setEhomeSessionID(int32_t lSessionID) {m_ulSessionID = lSessionID;};
    int32_t getEhomeSessionID() {return m_ulSessionID;};

protected:
    ACE_Recursive_Thread_Mutex            mutex_;
    uint32_t                 m_uiReqHandle;
    uint16_t                 m_usReqTranNum;
    uint32_t                 m_uiLocalIndex;

    int64_t                  m_llTimerID;
    std::time_t              m_tLastEventTime;

    uint32_t                 m_uiWaitTime;
    std::string              m_strDevID;
    std::string              m_strMuSvrID;
    std::string              m_strRtspUrl;
    uint16_t                 m_usTransno;
    int32_t                  m_ulSessionID;// for ehome

};


class CRealTimeBusiness : public CBusiness
{
public:
    CRealTimeBusiness();
    virtual ~CRealTimeBusiness();

    virtual void setDevStatus(uint32_t uiDevStatus);
    virtual uint32_t getDevStatus();
    virtual bool isTimeOut(time_t tCurrentTime);
    int32_t on_timer(time_t tCurrentTime, int32_t eventId = 0, int32_t timerId = 0);
    int32_t mu_session_setup_realtime(ACE_Message_Block *mb, uint16_t usTransno);
    int32_t mu_session_play_realtime(ACE_Message_Block *mb);
    int32_t mu_media_keyframe_req(ACE_Message_Block *mb);
    int32_t mu_session_teardown_req(ACE_Message_Block *mb);
    int32_t http_report_response(ACE_Message_Block *mb);
    int32_t notify_dev_start_stream_response(ACE_Message_Block *mb);
    int32_t notify_dev_stop_stream_response(ACE_Message_Block *mb);
    virtual void server_offline();
private:
    uint32_t            m_uiDevStreamStatus;
    ACE_Message_Block*  m_MsgCached;
};





class CBusinessManager
{
public:
    static CBusinessManager& instance()
    {
        static CBusinessManager  szCBusinessManager;
        return szCBusinessManager;
    }

    virtual ~CBusinessManager();

    int32_t initialize();
    int32_t start();
    int32_t stop();
    int32_t release();
    void on_timer(int32_t eventId, int32_t timerId);
public:
    int32_t get_url_req(
                const string& strParaDevID, PLAY_TYPE enParaPlayType, PLAY_URL_TYPE enPlayUrlType,
                DEV_STREAM_TYPE enParaStreamType, SVS_DEV_TYPE enParaDevType, string& strOutParaUrl);
    int32_t mu_session_setup_req(ACE_Message_Block *mb);
    int32_t mu_session_play_req(ACE_Message_Block *mb);
    int32_t mu_media_keyframe_req(ACE_Message_Block *mb);
    int32_t mu_media_keyframe_resp(ACE_Message_Block *mb);
    int32_t mu_session_teardown_req(ACE_Message_Block *mb);
    int32_t http_report_response(ACE_Message_Block *mb);
    int32_t notify_dev_start_stream_response(ACE_Message_Block *mb);
    int32_t notify_dev_stop_stream_response(ACE_Message_Block *mb);


    void send_mdu_session_setup_resp(int32_t iRespCode,CBusiness* pBusiness);
    void send_mdu_session_setup_resp(int32_t iRespCode, ACE_Message_Block* mb, ACE_Message_Block* mbInviteResp = NULL);
    void send_mdu_session_play_resp(int32_t iRespCode, ACE_Message_Block* mb);
    void send_mu_session_teardown_resp(int32_t iRespCode,ACE_Message_Block* mb);
    void mu_server_offline(const std::string&  strServerID, const std::list<std::string>&   BindDevList);
public:
    void show_business_info(const std::string& strLensID,std::string& Info);
protected:
    CBusinessManager();
private:
    uint16_t getNextReqTransno();
    int32_t mu_session_setup_realtime(ACE_Message_Block *mb);
    static void business_timer_callback(void *pArg, int32_t eventId, int32_t timerId, int32_t nDummy);

private:
    ACE_Recursive_Thread_Mutex               m_mutexBusMgr;

    typedef std::map<std::string, CBusiness*> DEV2BUSINESS_MAP;
    DEV2BUSINESS_MAP                         m_mapDev2RealtimeBusiness;
    int32_t                                  m_iTimerID;

    uint16_t                                 m_next_transno;
};

#endif //CBUSINESSMANAGER_H_

