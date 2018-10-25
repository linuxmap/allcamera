/*
 * svs_adapter_business_manager.h
 *
 *  Created on: 2016-1-21
 *      Author:
 */

#ifndef MDUBUSINESSMANAGER_H_
#define MDUBUSINESSMANAGER_H_

#include <map>
#include "svs_ace_header.h"
#include "svs_adapter_business.h"

class CSessionFluxTimer:public ACE_Event_Handler
{
public:
    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);
};


typedef std::multimap<uint64_svs, CMduBusiness*>           STREAM_BUSINESS_MAP;
typedef std::multimap<uint64_svs, CMduBusiness*>::iterator STREAM_BUSINESS_MAP_ITER;

typedef std::list<CMduBusiness*>                           BUSINESS_LIST;
typedef std::list<CMduBusiness*>::iterator                 BUSINESS_LIST_ITER;



class CMduBusinessManager
{
public:
    virtual ~CMduBusinessManager();

    static CMduBusinessManager* instance();

    int32_t initManager();

    void closeManager();

    CMduBusiness* createBusiness(uint64_svs recvSessionId,
                                 uint64_svs sendSessionId,
                                 PLAY_TYPE enPlayType);

    CMduBusiness* findBusiness(uint64_svs streamID);

    void findBusiness(uint64_svs streamID, BUSINESS_LIST &list);

    uint32_t getAttachedBusinessCount(uint64_svs streamID);

    void getAllBusiness(BUSINESS_LIST& businessList);

    void releaseBusiness(CMduBusiness* &pBusiness);

    void releaseBusiness(BUSINESS_LIST &list);

    uint32_t getBusinessCount();

    uint32_t getBusinessCount(uint64_svs ullStreamId);

    void statFlux();

    uint32_t getInputRate()const;

    uint32_t getOutputRate()const;

private:
    CMduBusinessManager();

    void destroyBusiness(CMduBusiness *&pBusiness);

    void startFluxTimer();

    void stopFluxTimer();

    bool isBusinessInList(const CMduBusiness *pBusiness, BUSINESS_LIST& businessList)const;
private:
    static CMduBusinessManager*  m_pMduBusinessManager;

    STREAM_BUSINESS_MAP          m_BusinessMap;
    ACE_Thread_Mutex             m_BusinessMapMutex;

    uint32_t                     m_unMaxBusinessNum;
    CSessionFluxTimer*           m_pFluxTimer;
    uint32_t                     m_InputRate;            // Mbps
    uint32_t                     m_OutputRate;           // Mbps
};

#endif /* MDUBUSINESSMANAGER_H_ */
