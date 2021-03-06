#ifndef __EHOME_MEDIA_HANDLE_H__
#define __EHOME_MEDIA_HANDLE_H__

#include <svs_adapter_network_handle.h>
#include "HCEHomeStream.h"
#include "HCEHomePublic.h"
#include <map>
#include "svs_adapter_mpeg_def.h"

#define EHOME_FRAME_MAX_SIZE (1024*1024)

class CEhomeStreamHandle
{
public:
    CEhomeStreamHandle();
    virtual ~CEhomeStreamHandle();
    void    setStreamID(uint64_t ullSessionID);
    void    setSessionID(int32_t lSessionID);
    int32_t getSessionID();
    int32_t startLinkHandle(int32_t lLinkHandle);
    void    stopLinkHandle();
    void    handle_preview_data(LONG  iPreviewHandle,NET_EHOME_PREVIEW_CB_MSG *pPreviewCBMsg);
private:
    static void preview_data_cb(LONG  iPreviewHandle,NET_EHOME_PREVIEW_CB_MSG *pPreviewCBMsg,void *pUserData);
    void    send_ehome_stream(char* pdata,uint32_t ulDataLen);
    void    send_h264_frame(char* pData,uint32_t ulLens);
    void    send_h264_key_nalu(char* pData,uint32_t ulLens);
    void    send_h264_nalu(char* pData,uint32_t ulLens,uint32_t ulTimeStamp);
    void    send_h264_single_nalu(char* pData,uint32_t ulLens,uint32_t ulTimeStamp);
    void    send_h264_multi_nalu(char* pData,uint32_t ulLens,uint32_t ulTimeStamp,uint16_t usPacketCount);
    void    send_h265_frame(char* pData,uint32_t ulLens);
    void    send_audio_frame(char* pData,uint32_t ulLens);
    void    send_cache_vidoe_frame();
    void    send_vidoe_frame(char* pData,uint32_t ulLens);
private:
    void ProgramStreamPackHeader(char*& pData,uint32_t& ulLens);
    void ProgramSystemPackHeader(char*& pData,uint32_t& ulLens);
    void ProgramStreamMap(char*& pData,uint32_t& ulLens);
    void ProgramPrivateHeader(char*& pData,uint32_t& ulLens);
    void ProgramEStramHead(char*& pData,uint32_t& ulLens);
private:
    void getNextRtpSeq(uint16_t& usCurSeq,uint16_t count);
    uint32_t getNextVideoTimeStamp();
    uint32_t getNextAudioTimeStamp();
private:
    uint64_t    m_ullStreamId;
    int32_t     m_lSessionId;
    int32_t     m_lLinkHandle;
    uint8_t     m_VideoPayload;
    uint8_t     m_AudioPayload;
    uint16_t    m_usVideoRtpSeq;
    uint16_t    m_usAudioRtpSeq;
    uint32_t    m_ulVideoSsrc;
    uint32_t    m_ulAudioSsrc;
    uint32_t    m_ulVideoTimeStamp;
    uint32_t    m_ulAudioTimeStamp;
    ACE_Time_Value m_ulLastVideoTime;
    ACE_Time_Value m_ulLastAudioTime;
    uint8_t     m_szFrameBuf[EHOME_FRAME_MAX_SIZE];
    uint32_t    m_ulFrameLen;
};

class CEhomeHandle : public CHandle
{
public:
    /** Default constructor */
    CEhomeHandle();

    /** Default destructor */
    virtual ~CEhomeHandle();

    int32_t openHandle(const ACE_INET_Addr &localAddr);

    void closeHandle();

    int32_t startHandle(uint64_t ullStreamID, const ACE_INET_Addr &remoteAddr);

    int32_t stopHandle(uint64_t ullStreamID);

    int32_t sendMessage(uint64_t ullStreamID,
                    const char *pDataBuffer,
                    uint32_t unDataSize,
                    const ACE_INET_Addr &peerAddr);

    // 检查连接状态，如果连接状态异常，返回false
    bool checkHandleStatus(uint64_t ullStreamID, uint32_t unTimeout);

    uint16_t getStreamCount() const { return m_mapStreamHandle.size();};

    void setEhomeSessionID(uint64_t ullStreamID,int32_t ulSessionID);

    BOOL handle_input_link(LONG lLinkHandle, NET_EHOME_NEWLINK_CB_MSG *pNewLinkCBMsg);

protected:
    static BOOL handle_link(LONG lLinkHandle, NET_EHOME_NEWLINK_CB_MSG *pNewLinkCBMsg, void *pUserData);

protected:
    ACE_SOCK_Dgram       m_UdpSocket;
    uint32_t             m_ulLastInvalidAlarmTime;   // 上一次无效数据告警时间
    uint32_t             m_ulLastRecvTime;
    ACE_INET_Addr        m_RemoteAddr;

    uint32_t             m_unStreamCount;      /// 本连接上流的个数
    uint64_t             m_ullStreamID;        /// 只保存第一个流ID，多条流时流ID从RTP扩展头中取
    ACE_Thread_Mutex     m_ListMutex;

    uint32_t             m_enPlayType;

    int32_t              m_lListenHandle;

    typedef std::map<uint64_t,CEhomeStreamHandle*> EHOMESTREAMMAP;
    EHOMESTREAMMAP       m_mapStreamHandle;
};

#endif // __EHOME_MEDIA_HANDLE_H__