#ifndef __CMDUSESSIONFACTORY_H__
#define __CMDUSESSIONFACTORY_H__

#include <map>
#include <list>
#include "svs_adapter_session.h"
#include "svs_adapter_port_manager.h"
#include "svs_adapter_def.h"

typedef std::list<CMduSession*>     CMduSessionList;
typedef CMduSessionList::iterator   CMduSessionIter;

typedef std::map<uint64_svs,uint64_svs>  LIVE_PUSTREAM_MAP ;
typedef std::map<uint64_svs,uint64_svs>::iterator  LIVE_PUSTREAM_ITER ;
typedef std::map<std::string,LIVE_PUSTREAM_MAP>  LIVE_LENS_PUSTREAM_MAP ;
typedef std::map<std::string,LIVE_PUSTREAM_MAP>::iterator  LIVE_LENS_PUSTREAM_ITER ;


class CSessionStatusTimer : public ACE_Event_Handler
{
public:
    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);
};


class CMduSessionFactory
{
public:
    /** Default destructor */
    virtual ~CMduSessionFactory();

    static CMduSessionFactory *instance()
    {
        if (NULL == g_sessionFactory)
        {
            try
            {
                g_sessionFactory = new CMduSessionFactory;
            }
            catch(...)
            {
            }
        }

        return g_sessionFactory;
    }

    UTAPI int32_t init();

    UTAPI void close();

    UTAPI CMduSession* createSourceSession(std::string& strContend,
                                    PEER_TYPE unPeerType,
                                    SESSION_TYPE unSessionType,
                                    bool bLocalFlag = false);

    UTAPI CMduSession* createSession(PEER_TYPE unPeerType,
                                    SESSION_TYPE unSessionType,
                                    bool bLocalFlag = false);

    UTAPI void releaseSession(CMduSession* &pMduSession);

    UTAPI void releaseSession(uint64_svs streamID);

    UTAPI CMduSession* findSession(uint64_svs streamID);

    UTAPI CMduSession* findSession(std::string& strContent);

    UTAPI CMduSession* findSessionNotAddRef(uint64_svs streamID) ;

    UTAPI void getAllSession(CMduSessionList& sessionList);

    UTAPI void getSessionCount(uint32_t& inputNum,
                                    uint32_t& outputNum,
                                    uint32_t& bidirectionNum,
                                    uint32_t& totalNum);

    UTAPI void checkSessionStatus();

private:
    typedef std::map<uint64_svs, CMduSession*>                STREAM_SESSION_MAP;
    typedef std::map<uint64_svs, CMduSession*>::iterator      STREAM_SESSION_ITER;

    typedef std::multimap<std::string, uint64_svs>            CONTENT_SESSION_MAP;
    typedef std::multimap<std::string, uint64_svs>::iterator  CONTENT_SESSION_ITER;

private:
    CMduSessionFactory();
    CMduSessionFactory(const CMduSessionFactory&);
    CMduSessionFactory& operator=(const CMduSessionFactory&);

    UTAPI CMduSession* createConcreteSession(PEER_TYPE unPeerType,SESSION_TYPE unSessionType,bool bLocalFlag = false) const;

    UTAPI void startStatusCheckTimer();

    UTAPI void stopStatusCheckTimer();

private:
    static CMduSessionFactory*      g_sessionFactory;

    STREAM_SESSION_MAP              m_SessionMap;
    CONTENT_SESSION_MAP             m_ContentMap;
    uint64_svs                      m_SessionIndex;
    ACE_Recursive_Thread_Mutex      m_SessionMapMutex;

    CSessionStatusTimer*            m_pStatusTimer;

#ifdef UNITTEST
    friend class MockCMduSessionFactory;
#endif
};


#endif // __CMDUSESSIONFACTORY_H__
