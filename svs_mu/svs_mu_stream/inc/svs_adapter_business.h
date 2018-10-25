/*
 * StreamBusiness.h
 *
 *  Created on: 2016-1-21
 *      Author:
 */

#ifndef STREAMBUSINESS_H_
#define STREAMBUSINESS_H_

#include "svs_adapter_session.h"
#include "svs_adapter_inner_msg.h"
#include "vms/vms.h"

class CStreamBusiness
{
    friend class CStreamBusinessManager;
public:
    CStreamBusiness();
    virtual ~CStreamBusiness();

    int32_t init(uint64_svs recvSessionId, uint64_svs sendSessionId, PLAY_TYPE enPlayType);

    int32_t start();

    int32_t stop();

    void handleInnerMsg(const STREAM_INNER_MSG &innerMsg, uint32_t unMsgSize);

    uint64_svs getRecvStreamID() const;

    uint64_svs getSendStreamID() const;

    CStreamSession* getSession(uint64_svs ullStreamID) const;

    void Dump(ACE_HANDLE handle);

    void DumpSdp(uint64_svs ullStreamId, ACE_HANDLE handle);

    const char* getContentID() const;

    void statFluxInfo();

    uint32_t getInputRate();

    uint32_t getOutputRate()const;

    uint64_t getFluxszie()const;

    uint32_t getStarttime()const;

    uint32_t getEndtime();

    PLAY_TYPE getPlayType()const;

private:
    int32_t addReference();

    int32_t decReference();

    int32_t createMediaProcessor();

    int32_t registMediaProcessor();

    void destroyMediaProcessor();

    void createDirectProcessor();

private:
    int32_t             m_nRefCount;
    char                m_szContentID[CONTENT_ID_LEN + 1];
    CStreamSession*        m_pRecvSession;
    CStreamSession*        m_pSendSession;
    PLAY_TYPE           m_enPlayType;

    CStreamMediaProcessor* m_pRecvProcessor;
    CStreamMediaProcessor* m_pSendProcessor;
    uint32_t            m_OutputRate;// kbps
    uint64_t            m_ullFluxSize;//KB
    uint32_t            m_unStartTime;
    uint32_t            m_unEndTime;

    ACE_Recursive_Thread_Mutex    m_Mutex;
};

#endif /* STREAMBUSINESS_H_ */
