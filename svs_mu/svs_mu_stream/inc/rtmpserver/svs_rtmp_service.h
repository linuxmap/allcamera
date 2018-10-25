/*
 * StreamRtspService.h
 *
 *  Created on: 2016-5-12
 *      Author:
 */

#ifndef STREAMRTMPSERVICE_H_
#define STREAMRTMPSERVICE_H_

#include <map>
#include "svs_ace_header.h"
#include "svs_rtmp_push_session.h"
#include "svs_vms_message.h"



class CRtmpSessionCheckTimer : public ACE_Event_Handler
{
public:
    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);
};


typedef std::map<uint32_t, CStreamRtmpPushSession*>      RTMP_SESSION_MAP;
typedef RTMP_SESSION_MAP::iterator                    RTMP_SESSION_MAP_ITER;

class CStreamRtmpService : public ACE_Event_Handler
{
public:
    virtual ~CStreamRtmpService();

    static CStreamRtmpService& instance()
    {
        static CStreamRtmpService streamRtmpService;
        return streamRtmpService;
    }

    int32_t open();

    void close() const;

    int32_t handleSvsMessage(CStreamSvsMessage &message);

    int32_t handle_input(ACE_HANDLE handle);

    int32_t handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask);

    void checkSession();

    void removeSession(CStreamRtmpPushSession* pSession);

private:
    CStreamRtmpService();

    uint32_t getLocalSessionIndex();

private:
    uint32_t               m_unLocalSessionIndex;
    ACE_INET_Addr          m_RtmpAddr;
    ACE_SOCK_Acceptor      m_RtmpAcceptor;

    RTMP_SESSION_MAP       m_RtmpSessionMap;
    ACE_Thread_Mutex       m_MapMutex;

    CRtmpSessionCheckTimer m_SessionCheckTimer;
    int32_t                m_ulCheckTimerId;
};

#endif /* STREAMRTMPSERVICE_H_ */
