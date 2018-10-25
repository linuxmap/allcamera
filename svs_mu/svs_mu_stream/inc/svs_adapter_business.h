/*
 * MduBusiness.h
 *
 *  Created on: 2016-1-21
 *      Author:
 */

#ifndef MDUBUSINESS_H_
#define MDUBUSINESS_H_

#include "svs_adapter_session.h"
#include "svs_adapter_inner_msg.h"
#include "vms/vms.h"

class CMduBusiness
{
    friend class CMduBusinessManager;
public:
    CMduBusiness();
    virtual ~CMduBusiness();

    int32_t init(uint64_svs recvSessionId, uint64_svs sendSessionId, PLAY_TYPE enPlayType);

    int32_t start();

    int32_t stop();

    void handleInnerMsg(const MDU_INNER_MSG &innerMsg, uint32_t unMsgSize);

    uint64_svs getRecvStreamID() const;

    uint64_svs getSendStreamID() const;

    CMduSession* getSession(uint64_svs ullStreamID) const;

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
    CMduSession*        m_pRecvSession;
    CMduSession*        m_pSendSession;
    PLAY_TYPE           m_enPlayType;

    CMduMediaProcessor* m_pRecvProcessor;
    CMduMediaProcessor* m_pSendProcessor;
    uint32_t            m_OutputRate;// kbps
    uint64_t            m_ullFluxSize;//KB
    uint32_t            m_unStartTime;
    uint32_t            m_unEndTime;

    ACE_Recursive_Thread_Mutex    m_Mutex;
};

#endif /* MDUBUSINESS_H_ */
